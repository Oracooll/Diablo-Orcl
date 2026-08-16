/**
 * @file oracool_audit_test.cpp
 *
 * Regression tests pinning the fixes from the 2026-08-15 self-audit (vault reports "Self-Audit -
 * Four Findings" through "Audit Round Ten"). Every test here encodes a bug that actually shipped:
 * the test names say what BROKE, so a future red run reads as "that bug is back" rather than as an
 * abstract invariant failing.
 *
 * The fixes that cannot be pinned headlessly - the HUD chrome click routing, the shift-cast paths,
 * the Zeal burst timing (all input/tick machinery), and the stash/tab torn-file loaders (need a
 * crafted archive) - are listed in the round-eleven dev report instead, as play-test items.
 */

#include <gtest/gtest.h>

#include <algorithm>
#include <cstring>
#include <string>
#include <vector>

#include "DiabloUI/ui_flags.hpp"
#include "engine/random.hpp"
#include "engine/render/text_render.hpp"
#include "engine/render/clx_render.hpp"
#include "engine/surface.hpp"
#include "cursor.h"
#include "inv.h"
#include "items.h"
#include "monstdat.h"
#include "monster.h"
#include "multi.h"
#include "oracool/class_tree.h"
#include "oracool/aura_field.h"
#include "oracool/charms.h"
#include "oracool/class_skills.h"
#include "oracool/crafting.h"
#include "oracool/gradual_healing.h"
#include "oracool/gems.h"
#include "oracool/item_set_stats.h"
#include "oracool/item_sets.h"
#include "oracool/hero_chunks.h"
#include "oracool/lesser_uniques.h"
#include "oracool/monster_difficulty.h"
#include "oracool/monster_scale.h"
#include "oracool/paladin_melee.h"
#include "oracool/paladin_skills.h"
#include "oracool/rng_streams.h"
#include "oracool/runewords.h"
#include "oracool/skill_points.h"
#include "oracool/sprite_scale.h"
#include "oracool/stat_sheet.h"
#include "oracool/ornate_border.h"
#include "oracool/telemetry.h"
#include "oracool/xp_counter.h"
#include "panels/spell_book.hpp"
#include "player.h"
#include "playerdat.hpp"
#include "qol/stash.h"
#include "spells.h"
#include "stores.h"
#include "utils/surface_to_clx.hpp"

using namespace devilution;

namespace {

/** @brief A monster with an identity-mapped unique TRN, the shape TintLesserUnique operates on. */
void GiveIdentityTrn(Monster &monster)
{
	monster.uniqueMonsterTRN = std::make_unique<uint8_t[]>(256);
	for (int i = 0; i < 256; i++)
		monster.uniqueMonsterTRN[i] = static_cast<uint8_t>(i);
}

} // namespace

// Bug (v1.6.4): the name was derived from aiSeed, which MonsterSeeds() rewrites from the game-loop
// counter on every tick - "his name was constantly changing." The name must come from
// lesserNameSeed alone and hold still whatever aiSeed does.
TEST(OracoolAudit, LesserUniqueNameIgnoresAiSeed)
{
	Monster monster {};
	monster.lesserAffix = LesserUniqueAffix::Warded;
	monster.lesserNameSeed = 1234;

	monster.aiSeed = 1;
	const std::string first = oracool::GetLesserUniqueName(monster);
	monster.aiSeed = 987654321;
	const std::string second = oracool::GetLesserUniqueName(monster);

	EXPECT_EQ(first, second) << "the name moved when aiSeed did - it is being derived from per-tick state again";
	EXPECT_FALSE(first.empty());
	EXPECT_NE(first.find(' '), std::string::npos) << "expected \"Given the Epithet\" shape";
}

TEST(OracoolAudit, LesserUniqueNameIsDeterministicAndSeedDependent)
{
	Monster monster {};
	monster.lesserAffix = LesserUniqueAffix::Warded;

	monster.lesserNameSeed = 7;
	const std::string a1 = oracool::GetLesserUniqueName(monster);
	const std::string a2 = oracool::GetLesserUniqueName(monster);
	EXPECT_EQ(a1, a2);

	monster.lesserNameSeed = 8;
	const std::string b = oracool::GetLesserUniqueName(monster);
	EXPECT_NE(a1, b) << "adjacent seeds should differ in the given name";
}

// Bug (v1.6.5): the kill log and death screen printed monster.name() - the borrowed champion -
// while the health bar printed the generated name. GetMonsterDisplayName is the one authority; it
// must compose exactly affix word + space + generated name for a lesser unique.
TEST(OracoolAudit, DisplayNameIsAffixPlusGeneratedName)
{
	Monster monster {};
	monster.lesserAffix = LesserUniqueAffix::Thunderous;
	monster.lesserNameSeed = 42;

	const std::string display = oracool::GetMonsterDisplayName(monster);
	const std::string expected = std::string(_(oracool::GetLesserUniqueAffixName(LesserUniqueAffix::Thunderous)))
	    + " " + oracool::GetLesserUniqueName(monster);
	EXPECT_EQ(display, expected);
}

// Phase 3.4, the monster-facing aura pass. Conviction is its flagship, and the property that makes
// it a design rather than a delete button: an immunity is a statement about what a monster IS, so a
// strong aura should erode it to a resistance, never all the way to nothing.
TEST(OracoolAudit, ConvictionErodesImmunitiesButNeverDeletesThem)
{
	constexpr int Shallow = 1;
	constexpr int Deep = 5;

	// Nothing invested changes nothing - the path every monster in the game takes every frame.
	EXPECT_EQ(oracool::ConvictionAdjusted(IMMUNE_FIRE | RESIST_MAGIC, 0), IMMUNE_FIRE | RESIST_MAGIC);

	// Shallow: plain resistances go, immunities are untouched.
	const uint16_t shallow = oracool::ConvictionAdjusted(IMMUNE_FIRE | RESIST_MAGIC, Shallow);
	EXPECT_EQ(shallow & RESIST_MAGIC, 0) << "a plain resistance survived Conviction";
	EXPECT_NE(shallow & IMMUNE_FIRE, 0) << "a shallow Conviction broke an immunity";

	// Deep: the immunity steps DOWN to a resistance rather than vanishing.
	const uint16_t deep = oracool::ConvictionAdjusted(IMMUNE_FIRE, Deep);
	EXPECT_EQ(deep & IMMUNE_FIRE, 0) << "a deep Conviction left the immunity standing";
	EXPECT_NE(deep & RESIST_FIRE, 0) << "the broken immunity vanished instead of stepping down";

	// The whole point, over every combination: no depth of Conviction ever leaves a monster with
	// NOTHING where it started with an immunity.
	for (const uint16_t immunity : { IMMUNE_MAGIC, IMMUNE_FIRE, IMMUNE_LIGHTNING }) {
		for (int points = 1; points <= 20; points++) {
			const uint16_t out = oracool::ConvictionAdjusted(immunity, points);
			EXPECT_NE(out, 0) << "Conviction at " << points
			                  << " points erased an immunity entirely instead of eroding it";
		}
	}
}

// Phase 3.4's other half: a champion lends to its PACK. Two properties matter.
//
// The first is that the lending never runs away with itself - PackAdjustedDamage writes into
// uint8_t fields, and the Torment difficulty block already had to learn that a wrap makes a
// STRONGER pack unpredictably weaker.
//
// The second is that a champion must not buff itself. A boss that did would just be a boss with
// bigger numbers, which its own affix already provides.
TEST(OracoolAudit, PackAuraClampsAndOnlySomeAffixesLend)
{
	// Exactly two of the six affixes reach past their own champion. The other four are personal,
	// and keeping them so is what stops the six blurring into one another.
	EXPECT_FALSE(oracool::PackAuraFrom(LesserUniqueAffix::Relentless, 1).isNothing());
	EXPECT_FALSE(oracool::PackAuraFrom(LesserUniqueAffix::Fortified, 1).isNothing());
	for (const LesserUniqueAffix personal : { LesserUniqueAffix::None, LesserUniqueAffix::Warded,
	         LesserUniqueAffix::Vampiric, LesserUniqueAffix::Thunderous, LesserUniqueAffix::Colossal }) {
		EXPECT_TRUE(oracool::PackAuraFrom(personal, 1).isNothing())
		    << "a personal affix started lending to the pack";
	}

	// The pack has an edge, and it is somewhere the player can pull a monster past.
	int lastLending = 0;
	for (int distance = 0; distance <= 40; distance++) {
		if (!oracool::PackAuraFrom(LesserUniqueAffix::Relentless, distance).isNothing())
			lastLending = distance;
	}
	EXPECT_GT(lastLending, 0) << "the pack aura reached nothing at all";
	EXPECT_LT(lastLending, 15) << "the pack reached far enough that pulling a monster out is hopeless";
	EXPECT_TRUE(oracool::PackAuraFrom(LesserUniqueAffix::Relentless, lastLending + 1).isNothing());

	// The clamp. These land in uint8_t fields, and the Torment block already learned that a wrap
	// makes a STRONGER pack unpredictably weaker.
	EXPECT_EQ(oracool::RaiseDamageByPercent(10, 0), 10);
	EXPECT_GT(oracool::RaiseDamageByPercent(10, 40), 10);
	for (int base = 0; base <= 255; base++) {
		for (const int percent : { 0, 40, 100, 500 }) {
			const uint8_t out = oracool::RaiseDamageByPercent(static_cast<uint8_t>(base), percent);
			EXPECT_GE(out, base) << "raising " << base << " by " << percent << "% made it smaller";
		}
	}
}

// The radius is what makes an aura a thing you POSITION yourself with. Zero when nothing is
// invested is the part that matters most: it is how "no aura lit" is expressed everywhere.
TEST(OracoolAudit, AuraRadiusStartsAtNothingAndIsBounded)
{
	EXPECT_EQ(oracool::AuraRadiusForPoints(0), 0);
	EXPECT_EQ(oracool::AuraRadiusForPoints(-3), 0);

	int previous = 0;
	for (int points = 1; points <= 20; points++) {
		const int radius = oracool::AuraRadiusForPoints(points);
		EXPECT_GE(radius, previous) << "the field shrank as points went in";
		EXPECT_LE(radius, 8) << "the field grew past the screen, where positioning stops mattering";
		previous = radius;
	}
	EXPECT_GT(oracool::AuraRadiusForPoints(20), oracool::AuraRadiusForPoints(1))
	    << "investment bought no reach at all";
}

// Phase 3.3. UniqueMonsterData has ONE resistance column where MonsterData has two, so a champion
// used to keep its Normal-difficulty set forever while the rank and file it leads switched to the
// hard set on Hell - champions got relatively SOFTER as the difficulty rose.
//
// The invariant, and the reason the fix is a union rather than a second authored column: a champion
// is never less resistant than an ordinary monster of its own type, on any difficulty, and it never
// loses a bit it was hand-authored with.
TEST(OracoolAudit, AChampionIsNeverSofterThanItsOwnRankAndFile)
{
	// Every combination of the seven meaningful bits in BOTH columns, rather than a walk over
	// MonstersData - which is not exported to the test binary, and which in any case only contains
	// the pairings the game happens to ship. This covers pairings it does not.
	constexpr uint8_t Bits[] = { RESIST_MAGIC, RESIST_FIRE, RESIST_LIGHTNING,
		IMMUNE_MAGIC, IMMUNE_FIRE, IMMUNE_LIGHTNING, IMMUNE_ACID };
	constexpr int Combos = 1 << 7;

	for (int normalMask = 0; normalMask < Combos; normalMask++) {
		for (int hellMask = 0; hellMask < Combos; hellMask++) {
			MonsterData data {};
			for (int b = 0; b < 7; b++) {
				if ((normalMask & (1 << b)) != 0)
					data.resistance |= Bits[b];
				if ((hellMask & (1 << b)) != 0)
					data.resistanceHell |= Bits[b];
			}

			for (const _difficulty difficulty : { DIFF_NORMAL, DIFF_NIGHTMARE, DIFF_HELL, DIFF_TORMENT }) {
				const uint16_t ordinary = oracool::MonsterResistancesFor(data, difficulty);
				// A champion whose own sheet grants nothing is the worst case, and exactly the one
				// that used to come out weaker than the monsters standing beside it.
				const uint16_t champion = oracool::ChampionResistancesFor(0, data, difficulty);
				ASSERT_EQ(champion & ordinary, ordinary)
				    << "champion lost a resistance the rank and file keep: normal=" << normalMask
				    << " hell=" << hellMask << " difficulty=" << static_cast<int>(difficulty);

				// And an authored bit survives whatever the type's ladder says.
				const uint16_t authored = oracool::ChampionResistancesFor(IMMUNE_LIGHTNING, data, difficulty);
				ASSERT_NE(authored & IMMUNE_LIGHTNING, 0) << "champion lost its own authored immunity";
			}
		}
	}
}

// Nightmare used to be Normal with fatter monsters - the second resistance column only arrived at
// Hell. It now gets Hell's set with the IMMUNITIES demoted to plain resistances, so the walls Hell
// will put up start pushing back a difficulty early without anything becoming unkillable yet.
TEST(OracoolAudit, NightmareDemotesHellImmunitiesToResistances)
{
	MonsterData data {};
	data.resistance = 0;
	data.resistanceHell = IMMUNE_FIRE | RESIST_MAGIC;

	EXPECT_EQ(oracool::MonsterResistancesFor(data, DIFF_NORMAL), 0);

	const uint16_t nightmare = oracool::MonsterResistancesFor(data, DIFF_NIGHTMARE);
	EXPECT_EQ(nightmare & IMMUNE_FIRE, 0) << "an immunity arrived a whole difficulty early";
	EXPECT_NE(nightmare & RESIST_FIRE, 0) << "the demoted immunity bought nothing";
	EXPECT_NE(nightmare & RESIST_MAGIC, 0) << "a plain Hell resistance was dropped instead of demoted";

	EXPECT_EQ(oracool::MonsterResistancesFor(data, DIFF_HELL), data.resistanceHell);
	EXPECT_EQ(oracool::MonsterResistancesFor(data, DIFF_TORMENT), data.resistanceHell);

	// Nightmare must never REMOVE something Normal already had: resistanceHell is authored as a
	// replacement set, not a superset, so the ladder unions rather than overwrites.
	MonsterData keeps {};
	keeps.resistance = RESIST_LIGHTNING;
	keeps.resistanceHell = IMMUNE_FIRE;
	EXPECT_NE(oracool::MonsterResistancesFor(keeps, DIFF_NIGHTMARE) & RESIST_LIGHTNING, 0)
	    << "a monster lost a Normal resistance by the difficulty going up";
}

// Phase 3.2. Two properties matter more than the size itself.
//
// The first is that an ORDINARY monster must not touch the scale path at all: GetScaledAnim runs
// from Monster::changeAnimationData, which fires every time any monster turns to face the player,
// so anything but an immediate nullptr there is a per-frame cost paid by every monster in the game.
//
// The second is that every affix must have a name. GetLesserUniqueAffixName is a switch, and the
// display name is affix + " " + generated name - so a new enum value that falls through returns ""
// and the champion silently loses its title.
TEST(OracoolAudit, ColossalIsNamedAndOnlySizedMonstersReachTheScaler)
{
	Monster monster {};

	monster.lesserAffix = LesserUniqueAffix::None;
	EXPECT_EQ(oracool::GetMonsterSize(monster), oracool::MonsterSize::Normal);
	EXPECT_EQ(oracool::GetScaledAnim(monster, MonsterGraphic::Stand), nullptr)
	    << "an ordinary monster reached the scale cache";

	monster.lesserAffix = LesserUniqueAffix::Colossal;
	EXPECT_EQ(oracool::GetMonsterSize(monster), oracool::MonsterSize::Colossal);
	EXPECT_GT(oracool::MonsterSizePercent(oracool::MonsterSize::Colossal), 100u);
	EXPECT_EQ(oracool::MonsterSizePercent(oracool::MonsterSize::Normal), 100u);

	for (int i = 1; i <= static_cast<int>(LesserUniqueAffix::LAST); i++) {
		const auto affix = static_cast<LesserUniqueAffix>(i);
		EXPECT_STRNE(oracool::GetLesserUniqueAffixName(affix), "")
		    << "affix " << i << " has no name - a champion would be called \" Malgrith the Unclean\"";
	}
	EXPECT_STREQ(oracool::GetLesserUniqueAffixName(LesserUniqueAffix::None), "");
}

// A Colossal champion's size is a promise about the fight, so the stat half must land too - and it
// must leave the monster at FULL health, because ApplyLesserUniqueAffix runs after hit points are
// already set and raising only the maximum would spawn every one of them visibly wounded.
TEST(OracoolAudit, ColossalRaisesLifeAndSpawnsAtFull)
{
	Monster monster {};
	monster.lesserAffix = LesserUniqueAffix::Colossal;
	monster.maxHitPoints = 1000;
	monster.hitPoints = 1000;

	oracool::ApplyLesserUniqueAffix(monster);

	EXPECT_GT(monster.maxHitPoints, 1000);
	EXPECT_EQ(monster.hitPoints, monster.maxHitPoints) << "a Colossal champion spawned already hurt";
}

// Bug (v1.6.5, regression introduced by v1.6.4): TintLesserUnique gained a caller that runs for
// EVERY unique on the level, and without this guard a scripted unique (lesserNameSeed 0 decodes to
// shift -2) was repainted two shades darker on every load. No affix means no tint, ever.
TEST(OracoolAudit, TintLeavesScriptedUniquesAlone)
{
	Monster monster {};
	monster.lesserAffix = LesserUniqueAffix::None; // a scripted unique, e.g. Gharbad
	monster.lesserNameSeed = 0;
	GiveIdentityTrn(monster);

	oracool::TintLesserUnique(monster);

	for (int i = 0; i < 256; i++)
		ASSERT_EQ(monster.uniqueMonsterTRN[i], static_cast<uint8_t>(i)) << "TRN entry " << i << " was repainted on a monster with no affix";
}

// The tint's own contract, layout-independent: only the global half (128+) moves, every moved
// entry stays inside its own 16-shade ramp within two shades, and some seed actually tints.
TEST(OracoolAudit, TintStaysInsideTheRamp)
{
	bool anySeedTints = false;
	for (uint16_t seed = 0; seed < 6500; seed += 271) {
		Monster monster {};
		monster.lesserAffix = LesserUniqueAffix::Warded;
		monster.lesserNameSeed = seed;
		GiveIdentityTrn(monster);

		oracool::TintLesserUnique(monster);

		for (int i = 0; i < 128; i++)
			ASSERT_EQ(monster.uniqueMonsterTRN[i], static_cast<uint8_t>(i)) << "level-specific palette half moved (seed " << seed << ", entry " << i << ")";
		for (int i = 128; i < 256; i++) {
			const uint8_t mapped = monster.uniqueMonsterTRN[i];
			if (mapped == i)
				continue;
			anySeedTints = true;
			ASSERT_EQ(mapped & 0xF0, i & 0xF0) << "tint crossed a ramp boundary (seed " << seed << ", entry " << i << ")";
			ASSERT_LE(std::abs((mapped & 0x0F) - (i & 0x0F)), 2) << "tint moved more than two shades";
		}
	}
	EXPECT_TRUE(anySeedTints) << "no seed produced any tint at all - the recolour is dead";
}

TEST(OracoolAudit, NameSeedRollStaysInBiasCorrectedBand)
{
	SetRndSeed(12345);
	for (int i = 0; i < 1000; i++) {
		const uint16_t seed = oracool::RollLesserUniqueNameSeed();
		ASSERT_LT(seed, 0x7FFF) << "the roll left GenerateRnd's bias-corrected band - see RollLesserUniqueNameSeed";
	}
}

// Bug (v1.6.5): IsQuestUnique tested mtalkmsg alone, and four quest bosses do not talk - the
// Skeleton King, the Butcher, the Hork Demon and Na-Krul were all legal lesser-unique candidates.
// A borrowed Butcher spawned in the real Butcher's room; a borrowed Na-Krul rewrote
// UberDiabloMonsterIndex. The MonsterAvailability::Never test catches all four.
TEST(OracoolAudit, SilentQuestBossesAreNotLesserUniqueCandidates)
{
	EXPECT_EQ(oracool::IsQuestUniqueForTest("Skeleton King"), 1);
	EXPECT_EQ(oracool::IsQuestUniqueForTest("The Butcher"), 1);
	EXPECT_EQ(oracool::IsQuestUniqueForTest("Hork Demon"), 1);
	EXPECT_EQ(oracool::IsQuestUniqueForTest("Na-Krul"), 1);
	// The talkers were always excluded - keep that half pinned too.
	EXPECT_EQ(oracool::IsQuestUniqueForTest("Gharbad the Weak"), 1);
	// And an ordinary champion must remain borrowable, or the whole system starves.
	EXPECT_EQ(oracool::IsQuestUniqueForTest("Bonehead Keenaxe"), 0);
}

// Bug (v1.6.3): HasShieldEquipped read INVLOC_HAND_RIGHT alone. Shields are ILOC_ONEHAND and
// equip to whichever hand slot they are dropped on; a shield in the LEFT hand silently removed
// Shield Bash and Blessed Shield from the Abilities window.
TEST(OracoolAudit, ShieldCountsInEitherHand)
{
	Players.resize(1);
	devilution::Player &player = Players[0];
	player.InvBody[INVLOC_HAND_LEFT].clear();
	player.InvBody[INVLOC_HAND_RIGHT].clear();

	EXPECT_FALSE(oracool::HasShieldEquipped(player));

	player.InvBody[INVLOC_HAND_RIGHT]._itype = ItemType::Shield;
	EXPECT_TRUE(oracool::HasShieldEquipped(player));

	player.InvBody[INVLOC_HAND_RIGHT].clear();
	player.InvBody[INVLOC_HAND_LEFT]._itype = ItemType::Shield;
	EXPECT_TRUE(oracool::HasShieldEquipped(player)) << "a shield in the left hand is not a shield - the v1.6.3 bug is back";
}

// Bug (v1.6.3): _pAblSpells was rebuilt only at creation, level-up and load, so equipping a shield
// mid-level left the mask stale - the Abilities window drew the shield skills unlocked but refused
// to ready them. The mask itself must reflect equipment; CalcPlrInv rebuilds it on every change.
TEST(OracoolAudit, InnateMaskFollowsTheShield)
{
	Players.resize(1);
	devilution::Player &player = Players[0];
	player._pClass = HeroClass::Warrior; // the class the Paladin plays as - see ClassHasPaladinSkills
	player._pLevel = 25;                 // past every skill's level gate
	player.InvBody[INVLOC_HAND_LEFT].clear();
	player.InvBody[INVLOC_HAND_RIGHT].clear();

	const uint64_t shieldBash = GetSpellBitmask(SpellID::ShieldBash);
	const uint64_t blessedShield = GetSpellBitmask(SpellID::BlessedShield);
	const uint64_t zeal = GetSpellBitmask(SpellID::Zeal);

	uint64_t mask = oracool::InnateSpellsBitmask(player);
	EXPECT_EQ(mask & shieldBash, 0u) << "Shield Bash granted without a shield";
	EXPECT_EQ(mask & blessedShield, 0u) << "Blessed Shield granted without a shield";
	EXPECT_NE(mask & zeal, 0u) << "Zeal should not be shield-gated";

	player.InvBody[INVLOC_HAND_LEFT]._itype = ItemType::Shield;
	mask = oracool::InnateSpellsBitmask(player);
	EXPECT_NE(mask & shieldBash, 0u);
	EXPECT_NE(mask & blessedShield, 0u);

	player._pClass = HeroClass::Rogue;
	mask = oracool::InnateSpellsBitmask(player);
	EXPECT_EQ(mask & shieldBash, 0u) << "Paladin skills leaked to another class";
}

// The Zeal strike ladder, as re-specified by Phase 2.1 (supersedes the 2026-08-15 character-level
// ladder): the level-6 gate buys the 2-strike burst, INVESTED points buy the rest - one strike per
// two points, capped at five. Character level beyond the gate no longer adds strikes.
TEST(OracoolAudit, ZealStrikeLadder)
{
	Players.resize(1);
	devilution::Player &player = Players[0];
	std::memset(player._pSkillInvestment, 0, sizeof(player._pSkillInvestment));
	const auto zeal = static_cast<size_t>(
	    oracool::GetPaladinSkillData(oracool::PaladinSkill::Zeal).spellId);

	player._pLevel = 5;
	EXPECT_EQ(oracool::ZealStrikeCount(player), 0) << "below the gate";
	player._pLevel = 50;
	EXPECT_EQ(oracool::ZealStrikeCount(player), 2) << "level alone must not add strikes";

	const struct {
		int invested;
		int strikes;
	} ladder[] = { { 0, 2 }, { 1, 2 }, { 2, 3 }, { 4, 4 }, { 6, 5 }, { 20, 5 } };
	for (const auto &step : ladder) {
		player._pSkillInvestment[zeal] = static_cast<uint8_t>(step.invested);
		EXPECT_EQ(oracool::ZealStrikeCount(player), step.strikes)
		    << "with " << step.invested << " points invested";
	}
	player._pSkillInvestment[zeal] = 0;
}

// Bug (v1.6.11): TotalPlayerGold summed _pGold + Stash.gold as plain int, while the stash's own
// deposit guard allows Stash.gold to reach INT_MAX. The overflow wrapped negative, converted to a
// huge uint32_t, and PlayerCanAfford approved everything - after which TakePlrsMoney drove the
// stash negative and kept the wraparound alive.
TEST(OracoolAudit, TotalPlayerGoldSaturatesInsteadOfWrapping)
{
	Players.resize(1);
	MyPlayer = &Players[0];
	const int savedStashGold = Stash.gold;

	MyPlayer->_pGold = 200000;
	Stash.gold = std::numeric_limits<int>::max() - 1000;
	EXPECT_EQ(TotalPlayerGold(), 2147682647u) << "the rich-player sum wrapped";

	// A negative pool (the state the old wraparound produced) must read as empty, not as riches.
	Stash.gold = -500000;
	EXPECT_EQ(TotalPlayerGold(), 200000u);

	Stash.gold = savedStashGold;
}

// Bug (v1.6.8): gradual healing's pending drip was file-scope and never reset - a potion drunk in
// the last seconds before quitting healed the NEXT character loaded. ResetGradualHealing must
// leave nothing behind.
TEST(OracoolAudit, GradualHealDoesNotOutliveItsHero)
{
	Players.resize(1);
	devilution::Player &player = Players[0];
	player._pHitPoints = 100 << 6;
	player._pHPBase = 100 << 6;
	player._pMaxHP = 200 << 6;
	player._pMaxHPBase = 200 << 6;

	oracool::ResetGradualHealing(); // clear anything a previous test queued

	// The bug: queue a heal (the potion), "switch character" (reset), and tick the new session.
	oracool::QueueGradualHeal(50 << 6);
	oracool::ResetGradualHealing();
	for (int i = 0; i < 120; i++)
		oracool::ProcessGradualHealing(player);
	EXPECT_EQ(player._pHitPoints, 100 << 6) << "a previous hero's potion healed this one";

	// And the control: without the reset the drip must actually heal, or the mechanic is dead.
	oracool::QueueGradualHeal(50 << 6);
	for (int i = 0; i < 120; i++)
		oracool::ProcessGradualHealing(player);
	EXPECT_GT(player._pHitPoints, 100 << 6);
	oracool::ResetGradualHealing();
}

// Not a bug fix but a claim worth pinning (user, 2026-08-15, citing Belzebub's 65k stacks): this
// fork's single-player gold cap is 100,000,000 per stack, applied by CalcPlrItemVals whenever the
// local player is in a single-player game. The compact ItemPack.wValue uint16 is NOT the ceiling -
// single-player's authoritative item data is int32 and overwrites it on load (see items.h's
// GoldStackSaveLimit note). If this test goes red, someone re-tied the cap to the 16-bit field.
TEST(OracoolAudit, SinglePlayerGoldStackCapIsOneHundredMillion)
{
	Players.resize(1);
	MyPlayer = &Players[0];
	devilution::Player &player = Players[0];
	gbIsMultiplayer = false;
	for (auto &item : player.InvBody)
		item.clear();
	for (int i = 0; i < InventoryGridCells; i++)
		player.InvGrid[i] = 0;
	player._pNumInv = 0;
	for (auto &beltItem : player.SpdList)
		beltItem.clear();

	CalcPlrInv(player, /*loadgfx=*/false);

	EXPECT_EQ(MaxGold, GoldStackSaveLimit);
	EXPECT_EQ(GoldStackSaveLimit, 100'000'000);
}

// Bug (v1.6.11): SmithBuyPItem's sparse-array scan had the skip count as its only loop condition.
// A stale selected row (the premium list shrinks on every buy; ConfirmEnter restores the old
// selection) walked isEmpty() past the array, then CLEARED whatever slot the overrun landed on,
// and charged for it. A stale row must now cost nothing, place nothing and clear nothing.
TEST(OracoolAudit, PremiumBuyStaleRowChargesNothing)
{
	Players.resize(1);
	MyPlayer = &Players[0];
	MyPlayer->_pGold = 0;
	const int savedStashGold = Stash.gold;
	Stash.gold = 500000;

	for (devilution::Item &premium : premiumitems)
		premium.clear();
	InitializeItem(premiumitems[3], IDI_HEAL);
	premiumitems[3]._iIvalue = 5000;
	InitializeItem(premiumitems[7], IDI_HEAL);
	premiumitems[7]._iIvalue = 5000;
	numpremium = 2;

	devilution::Item purchased = premiumitems[3];
	// Visible row 5 with only two live items: the stale-row shape ConfirmEnter's restore produces
	// after the list shrank under the selection.
	SimulateSmithPremiumBuyForTest(5, purchased);

	EXPECT_EQ(Stash.gold, 500000) << "a stale premium row charged gold";
	EXPECT_EQ(numpremium, 2) << "a stale premium row consumed stock";
	EXPECT_FALSE(premiumitems[3].isEmpty()) << "a stale premium row cleared a live slot";
	EXPECT_FALSE(premiumitems[7].isEmpty()) << "a stale premium row cleared a live slot";

	for (devilution::Item &premium : premiumitems)
		premium.clear();
	numpremium = 0;
	Stash.gold = savedStashGold;
}

// Megaplan Phase 0.3 (oracool/rng_streams.h): the cosmetic stream exists so that visual effects
// can never again shift a deterministic gameplay stream - the Thunderous/SpawnLoot class of bug.
// These two tests ARE that guarantee: if either fails, some change has re-entangled the streams.
TEST(OracoolRngStreams, CosmeticStreamDoesNotPerturbMainStream)
{
	devilution::SetRndSeed(844660068);
	int32_t clean[10];
	for (int32_t &value : clean)
		value = devilution::GenerateRnd(1000);

	devilution::SetRndSeed(844660068);
	oracool::SeedCosmeticRngForTest(12345);
	int32_t interleaved[10];
	for (int32_t &value : interleaved) {
		// A burst of cosmetic rolls between every gameplay roll - far denser than any real effect.
		for (int i = 0; i < 7; i++)
			(void)oracool::CosmeticRnd(360);
		(void)oracool::CosmeticFlipCoin();
		value = devilution::GenerateRnd(1000);
	}

	for (int i = 0; i < 10; i++)
		EXPECT_EQ(clean[i], interleaved[i]) << "cosmetic rolls shifted the vanilla stream at " << i;
}

TEST(OracoolRngStreams, MainSeedGuardRestoresEngineState)
{
	devilution::SetRndSeed(555);
	const uint32_t before = devilution::GetLCGEngineState();
	{
		oracool::MainSeedGuard guard;
		// A legacy-style burst that reaches the vanilla LCG directly, as AddMissile's random
		// animation frames do.
		for (int i = 0; i < 36; i++)
			(void)devilution::GenerateRnd(8);
	}
	EXPECT_EQ(devilution::GetLCGEngineState(), before)
	    << "MainSeedGuard failed to rewind the vanilla engine";

	// And the guard must not have frozen the generator: rolls after the scope still advance it.
	(void)devilution::GenerateRnd(8);
	EXPECT_NE(devilution::GetLCGEngineState(), before);
}

// Megaplan Phase 0.4: pins CalcPlrItemVals' aggregation semantics BEFORE the bonus-provider
// refactor, so the extraction is provably behaviour-neutral. Every accumulation rule the loop
// applies is represented: unconditional base stats, the identified-only bonus gate, the
// percentage-of-own-AC computation with its Sign fallback, the vit/mag->HP/mana multipliers,
// resistance clamping, and the light radius delta.
TEST(OracoolStatSheet, CalcPlrItemValsAggregationPinned)
{
	Players.resize(1);
	MyPlayer = &Players[0];
	devilution::Player &player = Players[0];
	player = {};
	player._pClass = HeroClass::Warrior;
	player._pLevel = 10;
	player._pBaseStr = 30;
	player._pBaseMag = 10;
	player._pBaseDex = 20;
	player._pBaseVit = 25;
	player._pMaxHPBase = 70 << 6;
	player._pHPBase = 70 << 6;
	player._pMaxManaBase = 10 << 6;
	player._pManaBase = 10 << 6;
	player._pLightRad = 12; // matches the expected 10 + 2 below, so no light engine call fires
	player._pRSpell = SpellID::Invalid;
	player._pRSplType = SpellType::Invalid;

	// An identified magic sword: base damage counts, bonuses count.
	devilution::Item &sword = player.InvBody[INVLOC_HAND_LEFT];
	sword = {};
	sword._itype = ItemType::Sword;
	sword._iClass = ICLASS_WEAPON;
	sword._iStatFlag = true;
	sword._iMagical = ITEM_QUALITY_MAGIC;
	sword._iIdentified = true;
	sword._iMinDam = 3;
	sword._iMaxDam = 9;
	sword._iPLDam = 60;
	sword._iPLToHit = 15;
	sword._iPLStr = 5;
	sword._iPLFR = 25;
	sword._iPLLight = 2;

	// Identified magic armor with a percentage AC bonus and vit/mana feeds.
	devilution::Item &armor = player.InvBody[INVLOC_CHEST];
	armor = {};
	armor._itype = ItemType::MediumArmor;
	armor._iClass = ICLASS_ARMOR;
	armor._iStatFlag = true;
	armor._iMagical = ITEM_QUALITY_MAGIC;
	armor._iIdentified = true;
	armor._iAC = 20;
	armor._iPLAC = 50; // 50% of 20 = +10 bonus AC
	armor._iPLVit = 4;
	armor._iPLMag = 6;
	armor._iPLHP = 3 << 6;
	armor._iPLMana = 2 << 6;
	armor._iPLLR = 130; // clamps to MaxResistance

	// An UNidentified magic ring: its base AC contributes, its bonuses must NOT.
	devilution::Item &ring = player.InvBody[INVLOC_RING_LEFT];
	ring = {};
	ring._itype = ItemType::Ring;
	ring._iClass = ICLASS_MISC;
	ring._iStatFlag = true;
	ring._iMagical = ITEM_QUALITY_MAGIC;
	ring._iIdentified = false;
	ring._iPLDam = 100;
	ring._iPLStr = 50;
	ring._iPLMR = 75;

	CalcPlrItemVals(player, false);

	EXPECT_EQ(player._pIMinDam, 3);
	EXPECT_EQ(player._pIMaxDam, 9);
	EXPECT_EQ(player._pIAC, 20);
	EXPECT_EQ(player._pIBonusDam, 60) << "unidentified bonuses leaked into damage";
	EXPECT_EQ(player._pIBonusToHit, 15);
	EXPECT_EQ(player._pIBonusAC, 10) << "percentage-of-own-AC computation changed";
	EXPECT_EQ(player._pStrength, 35) << "unidentified strength leaked";
	EXPECT_EQ(player._pMagic, 16);
	EXPECT_EQ(player._pVitality, 29);
	EXPECT_EQ(player._pFireResist, 25);
	EXPECT_EQ(player._pLghtResist, MaxResistance) << "resistance clamp changed";
	EXPECT_EQ(player._pMagResist, 0) << "unidentified resistance leaked";
	EXPECT_EQ(player._pLightRad, 12);

	// HP/mana: item points plus the class multiplier on item vit/mag, exactly as the engine
	// computes them - read the multipliers from PlayersData so the test cannot drift from it.
	const auto &classData = PlayersData[static_cast<size_t>(HeroClass::Warrior)];
	const int expectedIhp = (3 << 6) + (((4 * classData.itmLife) >> 6) << 6);
	const int expectedImana = (2 << 6) + (((6 * classData.itmMana) >> 6) << 6);
	EXPECT_EQ(player._pMaxHP, expectedIhp + player._pMaxHPBase);
	EXPECT_EQ(player._pMaxMana, expectedImana + player._pMaxManaBase);
}

// Phase 0.4: the Rage stat swings moved out of CalcPlrItemVals into the "rage" bonus provider -
// the first non-item source. Same numbers, new home; this pins the parity in both directions.
TEST(OracoolStatSheet, RageProviderMatchesVanillaSwings)
{
	Players.resize(1);
	MyPlayer = &Players[0];
	devilution::Player &player = Players[0];
	player = {};
	player._pClass = HeroClass::Barbarian;
	player._pLevel = 10;
	player._pBaseStr = 30;
	player._pBaseDex = 20;
	player._pBaseVit = 25;
	player._pLightRad = 10;
	player._pRSpell = SpellID::Invalid;
	player._pRSplType = SpellType::Invalid;

	player._pSpellFlags = SpellFlag::RageActive;
	CalcPlrItemVals(player, false);
	EXPECT_EQ(player._pStrength, 30 + 2 * 10);
	EXPECT_EQ(player._pDexterity, 20 + 10 + 10 / 2);
	EXPECT_EQ(player._pVitality, 25 + 2 * 10);

	player._pSpellFlags = SpellFlag::RageCooldown;
	CalcPlrItemVals(player, false);
	EXPECT_EQ(player._pStrength, 30 - 2 * 10);
	EXPECT_EQ(player._pDexterity, 20 - 15);
	EXPECT_EQ(player._pVitality, 25 - 2 * 10);

	player._pSpellFlags = SpellFlag::None;
	CalcPlrItemVals(player, false);
	EXPECT_EQ(player._pStrength, 30);
}

// Megaplan Phase 0.1: the hero file's chunk tail (oracool/hero_chunks.h). These three tests are
// the format's contract: state round-trips, unknown chunks are skipped not fatal, and a torn tail
// is rejected WHOLE rather than half-applied.
TEST(OracoolHeroChunks, SkillPointsAndWaypointsRoundTrip)
{
	Players.resize(1);
	devilution::Player &source = Players[0];
	source = {};
	source._pUnspentSkillPoints = 7;
	source._pSkillInvestment[3] = 5;
	source._pSkillInvestment[MAX_SPELLS - 1] = 2;
	source._pWaypointUnlocked[0][1] = true;
	source._pWaypointUnlocked[0][24] = true;
	source._pWaypointUnlocked[2][40] = true; // past the fixed u32 masks' reach - chunk-only ground
	source._pWaypointUnlocked[3][63] = true; // the last storable slot

	const std::vector<uint8_t> tail = oracool::BuildHeroChunkTail(source);
	ASSERT_GT(tail.size(), 4u);

	devilution::Player target {};
	target._pWaypointUnlocked[1][10] = true; // stale state that a full apply must overwrite
	oracool::ApplyHeroChunks(target, tail.data(), tail.size());

	EXPECT_EQ(target._pUnspentSkillPoints, 7);
	EXPECT_EQ(target._pSkillInvestment[3], 5);
	EXPECT_EQ(target._pSkillInvestment[MAX_SPELLS - 1], 2);
	EXPECT_TRUE(target._pWaypointUnlocked[0][1]);
	EXPECT_TRUE(target._pWaypointUnlocked[0][24]);
	EXPECT_TRUE(target._pWaypointUnlocked[2][40]) << "slot 40 lives only in the 64-bit chunk";
	EXPECT_TRUE(target._pWaypointUnlocked[3][63]);
	EXPECT_FALSE(target._pWaypointUnlocked[1][10]) << "stale unlock survived a full chunk apply";
}

TEST(OracoolHeroChunks, UnknownChunkIsSkippedNotFatal)
{
	Players.resize(1);
	devilution::Player &source = Players[0];
	source = {};
	source._pUnspentSkillPoints = 3;
	std::vector<uint8_t> tail = oracool::BuildHeroChunkTail(source);

	// Splice a chunk from "a newer build" (tag 999, 4-byte payload) between magic and the real
	// chunks. A correct reader walks over it and still applies everything after it.
	const std::vector<uint8_t> unknown = { 0xE7, 0x03, 0x04, 0x00, 0x00, 0x00, 0xDE, 0xAD, 0xBE, 0xEF };
	tail.insert(tail.begin() + 4, unknown.begin(), unknown.end());

	devilution::Player target {};
	oracool::ApplyHeroChunks(target, tail.data(), tail.size());
	EXPECT_EQ(target._pUnspentSkillPoints, 3) << "a skippable unknown chunk stopped the walk";
}

TEST(OracoolHeroChunks, TruncatedTailIsRejectedWhole)
{
	Players.resize(1);
	devilution::Player &source = Players[0];
	source = {};
	source._pUnspentSkillPoints = 9;
	source._pWaypointUnlocked[0][5] = true;
	const std::vector<uint8_t> tail = oracool::BuildHeroChunkTail(source);

	devilution::Player target {};
	// Cut mid-payload: nothing may apply, not even the chunks before the tear.
	oracool::ApplyHeroChunks(target, tail.data(), tail.size() - 3);
	EXPECT_EQ(target._pUnspentSkillPoints, 0) << "a torn tail half-applied";
	EXPECT_FALSE(target._pWaypointUnlocked[0][5]);

	// And the empty/absent tail is the legacy no-op, never an error.
	oracool::ApplyHeroChunks(target, nullptr, 0);
	EXPECT_EQ(target._pUnspentSkillPoints, 0);
}

// Megaplan Phase 0.6: the CLX sprite scaler. The synthetic sprite deliberately uses palette index
// 0 as an OPAQUE pixel - it is the shadow colour in real art, and the scaler's whole reason for
// decoding runs (rather than colour-keying) is that shadows must survive.
TEST(OracoolSpriteScale, DoublesDimensionsAndPreservesShadowAndTransparency)
{
	// A 4x4, 2-frame source: frame 0 has an opaque index-0 (shadow) top-left 2x2 block and an
	// opaque index-130 bottom-right 2x2 block; the other two 2x2 corners are transparent.
	OwnedSurface source(4, 8);
	constexpr uint8_t Key = 1; // the surface's transparent colour for encoding the SOURCE
	for (int y = 0; y < 8; y++) {
		uint8_t *row = &source[Point { 0, y }];
		for (int x = 0; x < 4; x++)
			row[x] = Key;
	}
	for (int y = 0; y < 2; y++) {
		for (int x = 0; x < 2; x++) {
			source[Point { x, y }] = 0;            // frame 0: shadow block, top-left
			source[Point { x + 2, y + 2 }] = 130;  // frame 0: colour block, bottom-right
			source[Point { x, y + 4 }] = 140;      // frame 1: colour block, top-left
		}
	}
	OwnedClxSpriteList original = SurfaceToClx(source, 2, Key);

	OwnedClxSpriteList scaled = oracool::ScaleClxList(ClxSpriteList(original), 200);
	ASSERT_EQ(ClxSpriteList(scaled).numSprites(), 2u);
	const ClxSprite frame0 = ClxSpriteList(scaled)[0];
	EXPECT_EQ(frame0.width(), 8);
	EXPECT_EQ(frame0.height(), 8);

	// Render onto a canvas of 77s: opaque pixels overwrite, transparent pixels leave 77.
	OwnedSurface canvas(8, 8);
	for (int y = 0; y < 8; y++) {
		uint8_t *row = &canvas[Point { 0, y }];
		for (int x = 0; x < 8; x++)
			row[x] = 77;
	}
	RenderClxSprite(canvas, frame0, { 0, 0 });

	EXPECT_EQ((canvas[Point { 0, 0 }]), 0) << "the opaque shadow block was lost";
	EXPECT_EQ((canvas[Point { 3, 3 }]), 0) << "shadow block did not scale to 4x4";
	EXPECT_EQ((canvas[Point { 7, 7 }]), 130) << "the colour block was lost";
	EXPECT_EQ((canvas[Point { 4, 4 }]), 130) << "colour block did not scale to 4x4";
	EXPECT_EQ((canvas[Point { 7, 0 }]), 77) << "a transparent corner became opaque";
	EXPECT_EQ((canvas[Point { 0, 7 }]), 77) << "a transparent corner became opaque";
}

TEST(OracoolSpriteScale, HalvingRoundsDownButNeverBelowOnePixel)
{
	OwnedSurface source(3, 3);
	for (int y = 0; y < 3; y++) {
		uint8_t *row = &source[Point { 0, y }];
		for (int x = 0; x < 3; x++)
			row[x] = 200;
	}
	OwnedClxSpriteList original = SurfaceToClx(source, 1, std::nullopt);

	OwnedClxSpriteList half = oracool::ScaleClxList(ClxSpriteList(original), 50);
	EXPECT_EQ(ClxSpriteList(half)[0].width(), 1);
	EXPECT_EQ(ClxSpriteList(half)[0].height(), 1);

	OwnedClxSpriteList quarterFloor = oracool::ScaleClxList(ClxSpriteList(original), 25);
	EXPECT_GE(ClxSpriteList(quarterFloor)[0].width(), 1) << "scaling floored below one pixel";
}

// Megaplan Phase 0.9: the telemetry CSV's one pure function - field escaping. Everything else in
// that module is file IO gated behind an option; the escaping is where a malformed row could
// corrupt the whole file for analysis.
TEST(OracoolTelemetry, CsvFieldEscaping)
{
	EXPECT_EQ(oracool::TelemetryEscapeCsvField("Fallen One"), "Fallen One");
	EXPECT_EQ(oracool::TelemetryEscapeCsvField("Sword, Rare"), "\"Sword, Rare\"");
	EXPECT_EQ(oracool::TelemetryEscapeCsvField("The \"Butcher\""), "\"The \"\"Butcher\"\"\"");
	EXPECT_EQ(oracool::TelemetryEscapeCsvField(""), "");
}

// Megaplan Phase 1: gems and sockets. These pin the three rules the system stands on: sockets
// only on plain equipment, host-dependent gem effects, and insertion filling in order.
TEST(OracoolGems, SocketsOnlyOnPlainEquipment)
{
	devilution::Item sword {};
	sword._itype = ItemType::Sword;
	sword._iMagical = ITEM_QUALITY_NORMAL;
	EXPECT_TRUE(oracool::CanItemHaveSockets(sword));

	sword._iMagical = ITEM_QUALITY_MAGIC;
	EXPECT_FALSE(oracool::CanItemHaveSockets(sword)) << "a magic item took sockets";

	sword._iMagical = ITEM_QUALITY_NORMAL;
	sword._iOracoolTier = OracoolItemTier::Rare;
	EXPECT_FALSE(oracool::CanItemHaveSockets(sword)) << "a tiered item took sockets";

	devilution::Item potion {};
	potion._itype = ItemType::Misc;
	potion._iMagical = ITEM_QUALITY_NORMAL;
	EXPECT_FALSE(oracool::CanItemHaveSockets(potion)) << "a misc item took sockets";
}

TEST(OracoolGems, RubyIsFireDamageInWeaponsAndFireResistInArmor)
{
	oracool::ItemBonusTotals weaponTotals;
	oracool::ApplyGemToTotals(IDI_ORACOOL_GEM_RUBY, oracool::SocketHost::Weapon, weaponTotals);
	EXPECT_GT(weaponTotals.fireMax, 0);
	EXPECT_EQ(weaponTotals.fireResist, 0);

	oracool::ItemBonusTotals armorTotals;
	oracool::ApplyGemToTotals(IDI_ORACOOL_GEM_RUBY, oracool::SocketHost::Armor, armorTotals);
	EXPECT_EQ(armorTotals.fireMax, 0);
	EXPECT_GT(armorTotals.fireResist, 0);

	oracool::ItemBonusTotals shieldTotals;
	oracool::ApplyGemToTotals(IDI_ORACOOL_GEM_RUBY, oracool::SocketHost::Shield, shieldTotals);
	EXPECT_GT(shieldTotals.fireResist, armorTotals.fireResist) << "the shield roll should be the bigger one";
}

TEST(OracoolGems, InsertionFillsInOrderAndStopsWhenFull)
{
	devilution::Item host {};
	host._itype = ItemType::Helm;
	host._iMagical = ITEM_QUALITY_NORMAL;
	host._iSocketCount = 2;

	devilution::Item ruby {};
	ruby._itype = ItemType::Misc;
	ruby.IDidx = IDI_ORACOOL_GEM_RUBY;

	EXPECT_TRUE(oracool::TrySocketGem(host, ruby));
	EXPECT_EQ(host._iSocketed[0], static_cast<uint16_t>(IDI_ORACOOL_GEM_RUBY));
	EXPECT_TRUE(oracool::TrySocketGem(host, ruby));
	EXPECT_EQ(host.socketedCount(), 2);
	EXPECT_FALSE(oracool::TrySocketGem(host, ruby)) << "a full item accepted a third gem";

	devilution::Item sword {};
	sword._itype = ItemType::Sword;
	sword.IDidx = IDI_SORCERER;
	devilution::Item target {};
	target._itype = ItemType::Helm;
	target._iSocketCount = 1;
	EXPECT_FALSE(oracool::TrySocketGem(target, sword)) << "a non-gem was socketed";
}

// The four derived small fonts (docs/THIRD_PARTY.md). These pin the WIRING, which is the part a
// merge can get wrong: the flag-to-font mapping, and that the original sizes still resolve exactly
// as they did before four more were appended.
TEST(OracoolFonts, SmallSizeFlagsMapToTheirFontsAndTheOldOnesAreUnmoved)
{
	EXPECT_EQ(GetFontSizeFromUiFlags(UiFlags::FontSize12), GameFont12);
	EXPECT_EQ(GetFontSizeFromUiFlags(UiFlags::FontSize24), GameFont24);
	EXPECT_EQ(GetFontSizeFromUiFlags(UiFlags::FontSize30), GameFont30);
	EXPECT_EQ(GetFontSizeFromUiFlags(UiFlags::FontSize42), GameFont42);
	EXPECT_EQ(GetFontSizeFromUiFlags(UiFlags::FontSize46), GameFont46);
	EXPECT_EQ(GetFontSizeFromUiFlags(UiFlags::FontSizeDialog), FontSizeDialog);

	EXPECT_EQ(GetFontSizeFromUiFlags(UiFlags::FontSize11), GameFont11);
	EXPECT_EQ(GetFontSizeFromUiFlags(UiFlags::FontSize10), GameFont10);
	EXPECT_EQ(GetFontSizeFromUiFlags(UiFlags::FontSize9), GameFont9);
	EXPECT_EQ(GetFontSizeFromUiFlags(UiFlags::FontSize8), GameFont8);

	// No size flag at all still means Font 12 - the default every existing caller relies on.
	EXPECT_EQ(GetFontSizeFromUiFlags(UiFlags::ColorWhite), GameFont12);

	// The new flags must not collide with the colour bits this fork added above them.
	EXPECT_EQ(static_cast<uint64_t>(UiFlags::FontSize11) & static_cast<uint64_t>(UiFlags::ColorOracoolGreen), 0u);
	EXPECT_EQ(static_cast<uint64_t>(UiFlags::FontSize8) & static_cast<uint64_t>(UiFlags::ColorOracoolYellow), 0u);
}

// The gem quality ladder (Gems.png): seven types, five qualities, one effect row per type scaled
// by quality. These pin the table's structure and the scaling, not the tuning numbers themselves.
TEST(OracoolGems, EveryTypeAndQualityHasItsOwnIndex)
{
	std::vector<uint16_t> seen;
	for (size_t t = 0; t < oracool::GemTypeCount; t++) {
		for (size_t q = 0; q < oracool::GemQualityCount; q++) {
			const uint16_t idx = oracool::GemIndexFor(static_cast<oracool::GemType>(t),
			    static_cast<oracool::GemQuality>(q));
			EXPECT_TRUE(IsOracoolGemIdx(idx)) << "a ladder entry is not recognised as a gem";
			EXPECT_EQ(std::count(seen.begin(), seen.end(), idx), 0) << "two ladder entries share an index";
			seen.push_back(idx);

			oracool::GemType backType;
			oracool::GemQuality backQuality;
			ASSERT_TRUE(oracool::GemTypeAndQuality(idx, backType, backQuality));
			EXPECT_EQ(static_cast<size_t>(backType), t);
			EXPECT_EQ(static_cast<size_t>(backQuality), q);
		}
	}
	EXPECT_EQ(seen.size(), oracool::GemTypeCount * oracool::GemQualityCount);
}

TEST(OracoolGems, QualityScalesEffectsAndNormalIsTheTunedRow)
{
	// The original five gems are the NORMAL quality of their type, so their tuned numbers must be
	// exactly what a normal gem still applies - the ladder was built around them, not over them.
	EXPECT_EQ(oracool::GemIndexFor(oracool::GemType::Ruby, oracool::GemQuality::Normal),
	    IDI_ORACOOL_GEM_RUBY);
	oracool::ItemBonusTotals normal;
	oracool::ApplyGemToTotals(IDI_ORACOOL_GEM_RUBY, oracool::SocketHost::Weapon, normal);
	EXPECT_EQ(normal.fireMin, 2);
	EXPECT_EQ(normal.fireMax, 6);

	oracool::ItemBonusTotals chipped;
	oracool::ApplyGemToTotals(IDI_ORACOOL_GEM_RUBY_CHIPPED, oracool::SocketHost::Weapon, chipped);
	oracool::ItemBonusTotals perfect;
	oracool::ApplyGemToTotals(IDI_ORACOOL_GEM_RUBY_PERFECT, oracool::SocketHost::Weapon, perfect);
	EXPECT_LT(chipped.fireMax, normal.fireMax) << "a chipped gem is not weaker than a normal one";
	EXPECT_GT(perfect.fireMax, normal.fireMax) << "a perfect gem is not stronger than a normal one";
	// Rounding must never erase an effect a gem is supposed to have.
	EXPECT_GT(chipped.fireMin, 0) << "scaling rounded a real effect away to nothing";

	// The two new types act, and on the channels their descriptions claim.
	oracool::ItemBonusTotals amethyst;
	oracool::ApplyGemToTotals(IDI_ORACOOL_GEM_AMETHYST_NORMAL, oracool::SocketHost::Armor, amethyst);
	EXPECT_GT(amethyst.dexterity, 0);
	oracool::ItemBonusTotals diamond;
	oracool::ApplyGemToTotals(IDI_ORACOOL_GEM_DIAMOND_NORMAL, oracool::SocketHost::Shield, diamond);
	EXPECT_GT(diamond.fireResist, 0);
	EXPECT_GT(diamond.lightningResist, 0);
	EXPECT_GT(diamond.magicResist, 0);
}

TEST(OracoolGems, RefiningWalksTheLadderAndStopsAtPerfect)
{
	uint16_t idx = oracool::GemIndexFor(oracool::GemType::Topaz, oracool::GemQuality::Chipped);
	for (int step = 0; step < 4; step++) {
		EXPECT_FALSE(oracool::IsPerfectGem(idx));
		const uint16_t next = oracool::NextGemQuality(idx);
		EXPECT_NE(next, idx) << "refining stalled below perfect";
		idx = next;
	}
	EXPECT_TRUE(oracool::IsPerfectGem(idx)) << "four refinements did not reach perfect";
	EXPECT_EQ(oracool::NextGemQuality(idx), idx) << "a perfect gem refined into something else";

	// A rune is not a gem and must not be walked by the gem recipe.
	EXPECT_FALSE(oracool::IsPerfectGem(IDI_ORACOOL_RUNE_EL));
	EXPECT_EQ(oracool::NextGemQuality(IDI_ORACOOL_RUNE_EL), IDI_ORACOOL_RUNE_EL);
}

// The runes carry Diablo II's own socket numbers (user directive 2026-08-16), with two documented
// adaptations: El's +50 Attack Rating maps to +5% to-hit, Sol's min-only damage is +9 flat.
TEST(OracoolGems, RunesCarryDiabloTwoNumbers)
{
	// Ral: adds 5-30 fire damage in weapons; Fire Resist +30% armor, +35% shield.
	oracool::ItemBonusTotals ralWeapon;
	oracool::ApplyGemToTotals(IDI_ORACOOL_RUNE_RAL, oracool::SocketHost::Weapon, ralWeapon);
	EXPECT_EQ(ralWeapon.fireMin, 5);
	EXPECT_EQ(ralWeapon.fireMax, 30);
	oracool::ItemBonusTotals ralArmor;
	oracool::ApplyGemToTotals(IDI_ORACOOL_RUNE_RAL, oracool::SocketHost::Armor, ralArmor);
	EXPECT_EQ(ralArmor.fireResist, 30);
	oracool::ItemBonusTotals ralShield;
	oracool::ApplyGemToTotals(IDI_ORACOOL_RUNE_RAL, oracool::SocketHost::Shield, ralShield);
	EXPECT_EQ(ralShield.fireResist, 35);

	// Ort: adds 1-50 lightning damage in weapons; Lightning Resist +30% armor, +35% shield.
	oracool::ItemBonusTotals ortWeapon;
	oracool::ApplyGemToTotals(IDI_ORACOOL_RUNE_ORT, oracool::SocketHost::Weapon, ortWeapon);
	EXPECT_EQ(ortWeapon.lightningMin, 1);
	EXPECT_EQ(ortWeapon.lightningMax, 50);
	oracool::ItemBonusTotals ortShield;
	oracool::ApplyGemToTotals(IDI_ORACOOL_RUNE_ORT, oracool::SocketHost::Shield, ortShield);
	EXPECT_EQ(ortShield.lightningResist, 35);

	// Sol: +9 damage in weapons; Damage Reduced by 7 in armor and shields (and never in weapons).
	oracool::ItemBonusTotals solWeapon;
	oracool::ApplyGemToTotals(IDI_ORACOOL_RUNE_SOL, oracool::SocketHost::Weapon, solWeapon);
	EXPECT_EQ(solWeapon.damageMod, 9);
	EXPECT_EQ(solWeapon.getHit, 0);
	oracool::ItemBonusTotals solArmor;
	oracool::ApplyGemToTotals(IDI_ORACOOL_RUNE_SOL, oracool::SocketHost::Armor, solArmor);
	EXPECT_EQ(solArmor.getHit, -7) << "Damage Reduced by 7 rides the beneficial-negative getHit channel";

	// El: +5% to-hit in weapons, +15 defense in armor and shields, +1 light radius everywhere.
	oracool::ItemBonusTotals elWeapon;
	oracool::ApplyGemToTotals(IDI_ORACOOL_RUNE_EL, oracool::SocketHost::Weapon, elWeapon);
	EXPECT_EQ(elWeapon.bonusToHit, 5);
	EXPECT_EQ(elWeapon.lightRadius, 1);
	oracool::ItemBonusTotals elShield;
	oracool::ApplyGemToTotals(IDI_ORACOOL_RUNE_EL, oracool::SocketHost::Shield, elShield);
	EXPECT_EQ(elShield.bonusArmor, 15);
	EXPECT_EQ(elShield.lightRadius, 1);
}

TEST(OracoolGems, TirGrantsManaPerKillFromWornSockets)
{
	Players.resize(1);
	devilution::Player &player = Players[0];
	for (devilution::Item &worn : player.InvBody)
		worn = {};
	devilution::Item &sword = player.InvBody[INVLOC_HAND_LEFT];
	sword._itype = ItemType::Sword;
	sword._iStatFlag = true;
	sword._iSocketCount = 2;
	sword._iSocketed[0] = static_cast<uint16_t>(IDI_ORACOOL_RUNE_TIR);
	sword._iSocketed[1] = static_cast<uint16_t>(IDI_ORACOOL_RUNE_TIR);
	EXPECT_EQ(oracool::RuneManaPerKill(player), 4) << "two Tirs should stack to +4 mana per kill";

	sword._iStatFlag = false;
	EXPECT_EQ(oracool::RuneManaPerKill(player), 0) << "an unusable item's sockets should stay inert";

	sword._iStatFlag = true;
	sword._iSocketed[1] = static_cast<uint16_t>(IDI_ORACOOL_RUNE_EL);
	EXPECT_EQ(oracool::RuneManaPerKill(player), 2) << "only Tir carries mana per kill";
}

// Diablo II's Paladin tree. These pin the shape of the tree itself - the pages, the tier gates and
// the two investment stores - rather than the exact effect numbers, which are tuning.
namespace {

/** @brief A level-30 Paladin with an empty tree and a pool of points to spend. */
devilution::Player &FreshHero(HeroClass heroClass, int unspent = 40)
{
	Players.resize(1);
	MyPlayer = &Players[0];
	devilution::Player &player = Players[0];
	player._pClass = heroClass;
	// Past the seventh tier's level 36, so nothing in any class's table is gated on level.
	player._pLevel = 50;
	player._pUnspentSkillPoints = static_cast<uint16_t>(unspent);
	player._pOracoolActiveAura = 0xFF;
	std::memset(player._pSkillInvestment, 0, sizeof(player._pSkillInvestment));
	std::memset(player._pClassTreeInvestment, 0, sizeof(player._pClassTreeInvestment));
	return player;
}

devilution::Player &FreshPaladin(int unspent = 40)
{
	Players.resize(1);
	MyPlayer = &Players[0];
	devilution::Player &player = Players[0];
	player._pClass = HeroClass::Warrior;
	player._pLevel = 30;
	player._pUnspentSkillPoints = static_cast<uint16_t>(unspent);
	player._pOracoolActiveAura = 0xFF;
	std::memset(player._pSkillInvestment, 0, sizeof(player._pSkillInvestment));
	std::memset(player._pClassTreeInvestment, 0, sizeof(player._pClassTreeInvestment));
	return player;
}

} // namespace

// Audit (2026-08-16): the inert-row rule is THE standing promise of the class-tree system - a row
// the engine has no channel for is listed, described and contributes nothing, so a player is never
// told a point bought something it did not. class_tree.h states it, and until now it was pinned by
// a hand-written list of five Paladin auras. There are 161 skills across six classes and most of
// the inert ones were on no list at all.
//
// Exhaustive instead of enumerated, so the rule cannot rot as rows are added: every row that
// declares implemented == false must leave the totals byte-identical to nothing. Deliberate
// adaptations are allowed by that rule, but they set implemented == true and say so in their own
// description - which is exactly the line this test draws.
TEST(OracoolClassTree, EveryInertRowContributesNothing)
{
	const oracool::ItemBonusTotals empty;
	int checked = 0;

	for (size_t i = 0; i < oracool::ClassTreeSkillCount; i++) {
		const auto skill = static_cast<oracool::ClassTreeSkill>(i);
		const oracool::ClassTreeSkillData &data = oracool::GetClassTreeSkillData(skill);
		if (data.implemented)
			continue;

		devilution::Player &player = FreshHero(data.heroClass);
		ASSERT_TRUE(oracool::InvestClassTreePoint(player, skill))
		    << _(data.name) << " could not take a point at level 50 with points in hand";
		// An aura contributes only while it burns, so an unlit one would pass trivially.
		if (data.kind == oracool::ClassTreeKind::Aura)
			ASSERT_TRUE(oracool::ToggleClassAura(player, skill)) << _(data.name) << " would not light";

		oracool::ItemBonusTotals totals;
		oracool::ApplyClassTreeToTotals(player, totals);
		EXPECT_EQ(std::memcmp(&totals, &empty, sizeof(empty)), 0)
		    << _(data.name) << " is marked inert but leaked an effect";
		checked++;
	}

	EXPECT_GT(checked, 50) << "almost nothing was inert - did the implemented flag get inverted?";
}


// Walks EVERY class tree - the generalization's own proof, and the check that catches a class
// added to the enum but forgotten in FirstSkillOf or the page builder.
TEST(OracoolClassTree, EveryPageIsPopulatedAndGridPositionsAreUnique)
{
	oracool::ClassTreeSkill skills[oracool::ClassTreeSkillCount];
	size_t total = 0;
	for (const HeroClass heroClass : { HeroClass::Warrior, HeroClass::Barbarian,
	         HeroClass::Sorcerer, HeroClass::Rogue, HeroClass::Bard, HeroClass::Monk }) {
	for (size_t p = 0; p < oracool::ClassTreePageCount; p++) {
		const size_t count = oracool::BuildClassTreePage(heroClass, static_cast<int>(p), skills);
		EXPECT_GT(count, 0u);
		total += count;
		// Two skills sharing a (tier, column) would draw on top of each other and only the second
		// would be clickable - the grid's one structural invariant.
		bool taken[oracool::ClassTreeTierCount][3] = {};
		for (size_t i = 0; i < count; i++) {
			const oracool::ClassTreeSkillData &data = oracool::GetClassTreeSkillData(skills[i]);
			ASSERT_GE(data.tier, 0);
			ASSERT_LT(data.tier, oracool::ClassTreeTierCount);
			ASSERT_GE(data.column, 0);
			ASSERT_LT(data.column, 3);
			EXPECT_FALSE(taken[data.tier][data.column])
			    << "two skills share tier " << data.tier << " column " << data.column;
			taken[data.tier][data.column] = true;
		}
	}
	}
	EXPECT_EQ(total, oracool::ClassTreeSkillCount) << "a skill is on no page, or on two";
}

TEST(OracoolClassTree, InvestmentRespectsClassLevelPoolAndCap)
{
	devilution::Player &player = FreshPaladin(3);

	player._pClass = HeroClass::Rogue;
	EXPECT_FALSE(oracool::InvestClassTreePoint(player, oracool::ClassTreeSkill::Might))
	    << "a Rogue spent a point in the Paladin tree";

	player._pClass = HeroClass::Warrior;
	player._pLevel = 1;
	EXPECT_FALSE(oracool::InvestClassTreePoint(player, oracool::ClassTreeSkill::Fanaticism))
	    << "a level-30 tier took a point at level 1";

	player._pLevel = 30;
	EXPECT_TRUE(oracool::InvestClassTreePoint(player, oracool::ClassTreeSkill::Might));
	EXPECT_EQ(oracool::ClassTreeInvestment(player, oracool::ClassTreeSkill::Might), 1);
	EXPECT_EQ(player._pUnspentSkillPoints, 2);

	player._pUnspentSkillPoints = 0;
	EXPECT_FALSE(oracool::InvestClassTreePoint(player, oracool::ClassTreeSkill::Might))
	    << "spent a point that was not there";
}

// maxRank is positional aggregate initialisation on a field appended after 140 rows already
// existed, so the whole scheme rests on 0 meaning "the usual cap". If that reading is ever lost,
// every pre-Monk skill silently drops to a zero-point cap and the trees stop taking points at all.
TEST(OracoolClassTree, PerSkillRankCapsApplyAndZeroStillMeansTheUsualCap)
{
	EXPECT_EQ(oracool::ClassTreeMaxRank(oracool::ClassTreeSkill::Might), oracool::MaxTreeInvestment)
	    << "a row that declares no cap stopped meaning MaxTreeInvestment";
	EXPECT_EQ(oracool::ClassTreeMaxRank(oracool::ClassTreeSkill::IronRobe), 5);
	EXPECT_EQ(oracool::ClassTreeMaxRank(oracool::ClassTreeSkill::Enlightenment), 1)
	    << "a branch capstone took more than its one rank";

	devilution::Player &player = FreshPaladin();
	player._pClass = HeroClass::Monk;
	player._pLevel = 50; // past tier 7's level 36, so nothing here is gated on level

	for (int i = 0; i < 5; i++) {
		EXPECT_TRUE(oracool::InvestClassTreePoint(player, oracool::ClassTreeSkill::IronRobe))
		    << "point " << i + 1 << " of five was refused";
	}
	EXPECT_FALSE(oracool::InvestClassTreePoint(player, oracool::ClassTreeSkill::IronRobe))
	    << "a five-rank skill took a sixth point";

	EXPECT_TRUE(oracool::InvestClassTreePoint(player, oracool::ClassTreeSkill::PerfectVessel));
	EXPECT_FALSE(oracool::InvestClassTreePoint(player, oracool::ClassTreeSkill::PerfectVessel))
	    << "a capstone took a second point";

	// The seventh tier is the other thing the Monk introduced; a level-35 character must not reach it.
	player._pLevel = 35;
	EXPECT_FALSE(oracool::IsClassTreeSkillUnlocked(player, oracool::ClassTreeSkill::MasterOfTheLongStaff))
	    << "tier 7 opened before level 36";
	player._pLevel = 36;
	EXPECT_TRUE(oracool::IsClassTreeSkillUnlocked(player, oracool::ClassTreeSkill::MasterOfTheLongStaff));
}

// The two stores, and why they exist: a skill with a spell slot must reach GetSpellLevel so every
// ladder that already scales with spell level scales with the tree.
TEST(OracoolClassTree, CastableSkillsInvestThroughTheSpellLevelSeam)
{
	devilution::Player &player = FreshPaladin();
	player._pISplLvlAdd = 0;
	const int before = player.GetSpellLevel(SpellID::HolyBolt);

	ASSERT_TRUE(oracool::InvestClassTreePoint(player, oracool::ClassTreeSkill::HolyBolt));
	ASSERT_TRUE(oracool::InvestClassTreePoint(player, oracool::ClassTreeSkill::HolyBolt));
	EXPECT_EQ(oracool::ClassTreeInvestment(player, oracool::ClassTreeSkill::HolyBolt), 2);
	EXPECT_EQ(player.GetSpellLevel(SpellID::HolyBolt), before + 2)
	    << "a castable tree skill's points did not reach GetSpellLevel";

	// An aura has no slot, so its points land in the tree's own array instead - indexed by the
	// skill's position within its class, which for Might is 9 (the nine combat skills precede it).
	ASSERT_TRUE(oracool::InvestClassTreePoint(player, oracool::ClassTreeSkill::Might));
	EXPECT_EQ(oracool::ClassTreeInvestment(player, oracool::ClassTreeSkill::Might), 1);
	const int mightSlot = oracool::ClassTreeIconIndex(oracool::ClassTreeSkill::Might);
	EXPECT_EQ(mightSlot, 9);
	EXPECT_EQ(player._pClassTreeInvestment[mightSlot], 1);
}

TEST(OracoolClassTree, AuraNeedsAPointBeforeItCanBurn)
{
	devilution::Player &player = FreshPaladin();

	EXPECT_FALSE(oracool::ToggleClassAura(player, oracool::ClassTreeSkill::Might))
	    << "an aura with nothing invested lit anyway";
	EXPECT_FALSE(oracool::ToggleClassAura(player, oracool::ClassTreeSkill::Zeal))
	    << "a combat skill was lit as an aura";

	ASSERT_TRUE(oracool::InvestClassTreePoint(player, oracool::ClassTreeSkill::Might));
	EXPECT_TRUE(oracool::ToggleClassAura(player, oracool::ClassTreeSkill::Might));
	EXPECT_EQ(oracool::GetActiveClassAura(player), oracool::ClassTreeSkill::Might);

	ASSERT_TRUE(oracool::InvestClassTreePoint(player, oracool::ClassTreeSkill::Defiance));
	EXPECT_TRUE(oracool::ToggleClassAura(player, oracool::ClassTreeSkill::Defiance)) << "activating replaces";
	EXPECT_EQ(oracool::GetActiveClassAura(player), oracool::ClassTreeSkill::Defiance);
	EXPECT_TRUE(oracool::ToggleClassAura(player, oracool::ClassTreeSkill::Defiance)) << "the second click clears";
	EXPECT_EQ(oracool::GetActiveClassAura(player), oracool::ClassTreeSkill::None);
}

TEST(OracoolClassTree, AuraEffectsScaleWithPointsAndInertOnesStaySilent)
{
	devilution::Player &player = FreshPaladin();

	// Nothing burning contributes nothing.
	oracool::ItemBonusTotals off;
	oracool::ApplyClassTreeToTotals(player, off);
	EXPECT_EQ(off.bonusDamage, 0);

	ASSERT_TRUE(oracool::InvestClassTreePoint(player, oracool::ClassTreeSkill::Might));
	ASSERT_TRUE(oracool::ToggleClassAura(player, oracool::ClassTreeSkill::Might));
	oracool::ItemBonusTotals onePoint;
	oracool::ApplyClassTreeToTotals(player, onePoint);
	EXPECT_GT(onePoint.bonusDamage, 0);

	ASSERT_TRUE(oracool::InvestClassTreePoint(player, oracool::ClassTreeSkill::Might));
	oracool::ItemBonusTotals twoPoints;
	oracool::ApplyClassTreeToTotals(player, twoPoints);
	EXPECT_GT(twoPoints.bonusDamage, onePoint.bonusDamage) << "the second point bought nothing";

	// Resist Cold is the documented remap onto magic resistance - this engine has no cold.
	devilution::Player &cold = FreshPaladin();
	ASSERT_TRUE(oracool::InvestClassTreePoint(cold, oracool::ClassTreeSkill::ResistCold));
	ASSERT_TRUE(oracool::ToggleClassAura(cold, oracool::ClassTreeSkill::ResistCold));
	oracool::ItemBonusTotals coldTotals;
	oracool::ApplyClassTreeToTotals(cold, coldTotals);
	EXPECT_GT(coldTotals.magicResist, 0);

	// The auras whose mechanics this engine has no channel for must contribute NOTHING, so a
	// player cannot be told a point bought something it did not.
	for (const oracool::ClassTreeSkill inert : { oracool::ClassTreeSkill::HolyFreeze,
	         oracool::ClassTreeSkill::Sanctuary, oracool::ClassTreeSkill::Conviction,
	         oracool::ClassTreeSkill::Cleansing, oracool::ClassTreeSkill::Redemption }) {
		devilution::Player &p = FreshPaladin();
		ASSERT_TRUE(oracool::InvestClassTreePoint(p, inert));
		ASSERT_TRUE(oracool::ToggleClassAura(p, inert));
		oracool::ItemBonusTotals inertTotals;
		oracool::ItemBonusTotals empty;
		oracool::ApplyClassTreeToTotals(p, inertTotals);
		EXPECT_EQ(std::memcmp(&inertTotals, &empty, sizeof(empty)), 0)
		    << _(oracool::GetClassTreeSkillData(inert).name) << " leaked an effect it does not have";
	}
}

TEST(OracoolClassTree, VigorRunsAndOnlyWhenPaidFor)
{
	devilution::Player &player = FreshPaladin();
	EXPECT_FALSE(oracool::IsClassTreeRunActive(player));

	ASSERT_TRUE(oracool::InvestClassTreePoint(player, oracool::ClassTreeSkill::Vigor));
	ASSERT_TRUE(oracool::ToggleClassAura(player, oracool::ClassTreeSkill::Vigor));
	EXPECT_TRUE(oracool::IsClassTreeRunActive(player));

	ASSERT_TRUE(oracool::ToggleClassAura(player, oracool::ClassTreeSkill::Vigor));
	EXPECT_FALSE(oracool::IsClassTreeRunActive(player)) << "Vigor kept running after it was put out";
}

TEST(OracoolClassTree, AuraStateRoundTripsThroughTheChunkTail)
{
	devilution::Player &writer = FreshPaladin();
	ASSERT_TRUE(oracool::InvestClassTreePoint(writer, oracool::ClassTreeSkill::HolyFire));
	ASSERT_TRUE(oracool::InvestClassTreePoint(writer, oracool::ClassTreeSkill::HolyFire));
	ASSERT_TRUE(oracool::ToggleClassAura(writer, oracool::ClassTreeSkill::HolyFire));
	const std::vector<uint8_t> tail = oracool::BuildHeroChunkTail(writer);

	Players.resize(2);
	devilution::Player &reader = Players[1];
	reader._pClass = HeroClass::Warrior;
	reader._pLevel = 30;
	reader._pOracoolActiveAura = 0xFF;
	std::memset(reader._pClassTreeInvestment, 0, sizeof(reader._pClassTreeInvestment));
	oracool::ApplyHeroChunks(reader, tail.data(), tail.size());

	EXPECT_EQ(oracool::GetActiveClassAura(reader), oracool::ClassTreeSkill::HolyFire);
	EXPECT_EQ(oracool::ClassTreeInvestment(reader, oracool::ClassTreeSkill::HolyFire), 2);
}

// Phase 2.1: skill points on level-up, feeding the existing ladders through GetSpellLevel.
TEST(OracoolSkillPoints, RetroGrantInvestRefundRoundTrip)
{
	Players.resize(1);
	devilution::Player &player = Players[0];
	player._pLevel = 5;
	player._pUnspentSkillPoints = 0;
	std::memset(player._pSkillInvestment, 0, sizeof(player._pSkillInvestment));
	player._pAblSpells = 0;
	std::memset(player._pSplLvl, 0, sizeof(player._pSplLvl));

	oracool::EnsureRetroactiveSkillPoints(player);
	EXPECT_EQ(player._pUnspentSkillPoints, 4) << "level 5 is owed 4 points";
	oracool::EnsureRetroactiveSkillPoints(player);
	EXPECT_EQ(player._pUnspentSkillPoints, 4) << "the retro grant must not pay twice";

	const auto firebolt = static_cast<size_t>(SpellID::Firebolt);
	EXPECT_FALSE(oracool::CanInvestSkillPoint(player, SpellID::Firebolt))
	    << "an unlearned spell took a point";
	player._pSplLvl[firebolt] = 1;
	ASSERT_TRUE(oracool::InvestSkillPoint(player, SpellID::Firebolt));
	ASSERT_TRUE(oracool::InvestSkillPoint(player, SpellID::Firebolt));
	EXPECT_EQ(player._pUnspentSkillPoints, 2);
	player._pISplLvlAdd = 0;
	EXPECT_EQ(player.GetSpellLevel(SpellID::Firebolt), 3)
	    << "book level 1 + 2 invested should reach the ladders as level 3";

	EXPECT_EQ(oracool::RespecCost(player), 1000) << "the floor price";
	oracool::RefundAllSkillPoints(player);
	EXPECT_EQ(player._pUnspentSkillPoints, 4);
	EXPECT_EQ(player.GetSpellLevel(SpellID::Firebolt), 1);
}

TEST(OracoolSkillPoints, ZealStrikesArePointDriven)
{
	Players.resize(1);
	devilution::Player &player = Players[0];
	player._pLevel = 20;
	std::memset(player._pSkillInvestment, 0, sizeof(player._pSkillInvestment));

	const auto zeal = static_cast<size_t>(oracool::GetPaladinSkillData(oracool::PaladinSkill::Zeal).spellId);
	EXPECT_EQ(oracool::ZealStrikeCount(player), 2)
	    << "an uninvested Zeal stays at the base burst whatever the character level";
	player._pSkillInvestment[zeal] = 2;
	EXPECT_EQ(oracool::ZealStrikeCount(player), 3);
	player._pSkillInvestment[zeal] = 20;
	EXPECT_EQ(oracool::ZealStrikeCount(player), 5) << "the cap holds";

	player._pLevel = 1;
	EXPECT_EQ(oracool::ZealStrikeCount(player), 0) << "the unlock gate is still character level";
}

// Phase 1 charms: the active cap IS the pouch. These pin the cap and the reading order.
TEST(OracoolCharms, OnlyFirstCapCharmsAreActive)
{
	Players.resize(1);
	devilution::Player &player = Players[0];
	player = {};
	// Five charms in the main backpack: positions 0-4 of InvList.
	for (int i = 0; i < 5; i++) {
		player.InvList[i] = {};
		player.InvList[i]._itype = ItemType::Misc;
		player.InvList[i].IDidx = IDI_ORACOOL_CHARM_VIGOR;
	}
	player._pNumInv = 5;

	int visited = 0;
	oracool::ForEachActiveCharm(player, [](uint16_t, void *context) { (*static_cast<int *>(context))++; }, &visited);
	EXPECT_EQ(visited, oracool::CharmActiveCap) << "the active cap leaked";

	EXPECT_TRUE(oracool::IsCharmActive(player, -1, 0));
	EXPECT_TRUE(oracool::IsCharmActive(player, -1, 2));
	EXPECT_FALSE(oracool::IsCharmActive(player, -1, 3)) << "the fourth charm claims to be active";
	EXPECT_FALSE(oracool::IsCharmActive(player, -1, 4));
}

TEST(OracoolCharms, CharmEffectsReachTheStatSheet)
{
	Players.resize(1);
	MyPlayer = &Players[0];
	devilution::Player &player = Players[0];
	player = {};
	player._pClass = HeroClass::Warrior;
	player._pLevel = 10;
	player._pBaseStr = 30;
	player._pLightRad = 10;
	player._pRSpell = SpellID::Invalid;
	player._pRSplType = SpellType::Invalid;

	player.InvList[0] = {};
	player.InvList[0]._itype = ItemType::Misc;
	player.InvList[0].IDidx = IDI_ORACOOL_CHARM_EMBERS; // +15% fire resist from the backpack
	player._pNumInv = 1;

	CalcPlrItemVals(player, false);
	EXPECT_EQ(player._pFireResist, 15) << "the charm's resist never reached the sheet";
}

// Phase 1 runewords: derived state, exact order, right host - the three rules in one test each.
TEST(OracoolRunewords, ExactSequenceInRightHostCompletes)
{
	devilution::Item sword {};
	sword._itype = ItemType::Sword;
	sword._iMagical = ITEM_QUALITY_NORMAL;
	sword._iSocketCount = 2;
	sword._iSocketed[0] = IDI_ORACOOL_RUNE_TIR;
	sword._iSocketed[1] = IDI_ORACOOL_RUNE_EL;
	const oracool::RunewordDefinition *steel = oracool::GetActiveRuneword(sword);
	ASSERT_NE(steel, nullptr) << "Tir+El in a 2-socket sword should be Steel";
	EXPECT_STREQ(steel->name, "Steel");

	// Wrong ORDER is no runeword at all.
	devilution::Item wrongOrder = sword;
	wrongOrder._iSocketed[0] = IDI_ORACOOL_RUNE_EL;
	wrongOrder._iSocketed[1] = IDI_ORACOOL_RUNE_TIR;
	EXPECT_EQ(oracool::GetActiveRuneword(wrongOrder), nullptr);

	// Right runes, wrong HOST: Tir+El in a helm is nothing.
	devilution::Item helm = sword;
	helm._itype = ItemType::Helm;
	EXPECT_EQ(oracool::GetActiveRuneword(helm), nullptr);

	// A 3-socket sword with the same two runes is INCOMPLETE, not Steel: socket count must match.
	devilution::Item threeSockets = sword;
	threeSockets._iSocketCount = 3;
	EXPECT_EQ(oracool::GetActiveRuneword(threeSockets), nullptr);
}

TEST(OracoolRunewords, CompletionRenamesAndRunesTeach)
{
	devilution::Item shield {};
	shield._itype = ItemType::Shield;
	shield._iMagical = ITEM_QUALITY_NORMAL;
	shield._iSocketCount = 3;
	shield._iSocketed[0] = IDI_ORACOOL_RUNE_RAL;
	shield._iSocketed[1] = IDI_ORACOOL_RUNE_ORT;
	shield._iSocketed[2] = IDI_ORACOOL_RUNE_EL;
	ASSERT_TRUE(oracool::TryCompleteRuneword(shield));
	EXPECT_STREQ(shield._iIName, "Ancient's Pledge");

	// El appears in Steel and Ancient's Pledge - its description must teach both.
	const std::string teaching = oracool::RuneTeachingLines(IDI_ORACOOL_RUNE_EL);
	EXPECT_NE(teaching.find("Steel"), std::string::npos);
	EXPECT_NE(teaching.find("Ancient's Pledge"), std::string::npos);
	// Sol appears only in Lore.
	const std::string solTeaching = oracool::RuneTeachingLines(IDI_ORACOOL_RUNE_SOL);
	EXPECT_NE(solTeaching.find("Lore"), std::string::npos);
	EXPECT_EQ(solTeaching.find("Steel"), std::string::npos);
}

// Phase 1 ethereal: the bargain is stamped at roll time and the refusals hold.
TEST(OracoolEthereal, RepairDeclinesEtherealItems)
{
	devilution::Item ghost {};
	ghost._itype = ItemType::Sword;
	ghost._iClass = ICLASS_WEAPON;
	ghost._iOracoolEthereal = true;
	ghost._iDurability = 5;
	ghost._iMaxDur = 20;

	RepairItem(ghost, 30);
	EXPECT_EQ(ghost._iDurability, 5) << "the Repair skill fixed a ghost";
	EXPECT_EQ(ghost._iMaxDur, 20) << "the Repair skill ground down a ghost's max durability";
}

// Phase 1 Magic/Gold Find: charm-fed, consumed only in the unseeded drop tail.
TEST(OracoolFindStats, CharmsFeedFindStatsToThePlayer)
{
	Players.resize(1);
	MyPlayer = &Players[0];
	devilution::Player &player = Players[0];
	player = {};
	player._pClass = HeroClass::Warrior;
	player._pLevel = 10;
	player._pLightRad = 10;
	player._pRSpell = SpellID::Invalid;
	player._pRSplType = SpellType::Invalid;

	player.InvList[0] = {};
	player.InvList[0]._itype = ItemType::Misc;
	player.InvList[0].IDidx = IDI_ORACOOL_CHARM_LUCK;
	player.InvList[1] = {};
	player.InvList[1]._itype = ItemType::Misc;
	player.InvList[1].IDidx = IDI_ORACOOL_CHARM_GREED;
	player._pNumInv = 2;

	CalcPlrItemVals(player, false);
	EXPECT_EQ(player._pMagicFind, 15);
	EXPECT_EQ(player._pGoldFind, 30);
}

TEST(OracoolFindStats, GoldFindScalesDroppedGold)
{
	Players.resize(1);
	MyPlayer = &Players[0];
	devilution::Player &player = Players[0];
	player = {};
	player._pGoldFind = 50;

	devilution::Item gold {};
	InitializeItem(gold, IDI_GOLD);
	gold._ivalue = 100;
	ApplyMagicAndGoldFindToDrop(gold, 5);
	EXPECT_EQ(gold._ivalue, 150) << "+50% gold find did not scale the pile";

	// And with no gold find, nothing moves.
	player._pGoldFind = 0;
	devilution::Item plain {};
	InitializeItem(plain, IDI_GOLD);
	plain._ivalue = 100;
	ApplyMagicAndGoldFindToDrop(plain, 5);
	EXPECT_EQ(plain._ivalue, 100);
}

// 2026-08-16 audit findings, pinned so they stay fixed.
TEST(OracoolAudit2, RunewordNameActuallyDisplays)
{
	devilution::Item sword {};
	sword._itype = ItemType::Sword;
	sword.IDidx = IDI_SORCERER; // any valid idx; the name path reads _iIName for a completed word
	sword._iMagical = ITEM_QUALITY_NORMAL;
	sword._iSocketCount = 2;
	sword._iSocketed[0] = IDI_ORACOOL_RUNE_TIR;
	sword._iSocketed[1] = IDI_ORACOOL_RUNE_EL;
	ASSERT_TRUE(oracool::TryCompleteRuneword(sword));

	const std::string displayed { std::string(sword.getName()) };
	EXPECT_EQ(displayed, "Steel") << "the runeword rename is invisible again - getName's NORMAL branch";
}

TEST(OracoolAudit2, LevelSpanIsSafeAtTheCap)
{
	Players.resize(1);
	devilution::Player &player = Players[0];
	player = {};
	player._pLevel = static_cast<int8_t>(MaxCharacterLevel);
	EXPECT_EQ(oracool::GetLevelExperienceSpan(player), 1u) << "the cap read past ExpLvlsTbl";
	player._pLevel = 0;
	EXPECT_EQ(oracool::GetLevelExperienceSpan(player), 1u);
}

// Phase 1 crafting: the recipe engine's three contracts - materials found by KIND, no partial
// consumes, and the ladder ascending exactly one rung.
TEST(OracoolCrafting, AscendRunesConsumesPairAndProducesNextRung)
{
	// The crafted player is deliberately NOT MyPlayer: the engine's placement/removal helpers
	// send network messages for the local player, and those layers are not spun up headlessly.
	// The crafting logic itself is player-agnostic, which is exactly what this proves.
	Players.resize(2);
	MyPlayer = &Players[1];
	devilution::Player &player = Players[0];
	player = {};
	player._pClass = HeroClass::Warrior;

	for (int i = 0; i < 2; i++) {
		player.InvList[i] = {};
		InitializeItem(player.InvList[i], IDI_ORACOOL_RUNE_EL);
		player.InvList[i]._itype = ItemType::Misc;
	}
	player._pNumInv = 2;
	// Give the grid a real free cell for the output.
	for (int8_t &cell : player.InvGrid)
		cell = 0;
	player.InvGrid[0] = 1;
	player.InvGrid[1] = 2;

	ASSERT_TRUE(oracool::CanCraft(player, 1));
	const std::string crafted = oracool::Craft(player, 1);
	EXPECT_FALSE(crafted.empty());

	int tirCount = 0;
	int elCount = 0;
	for (int i = 0; i < player._pNumInv; i++) {
		if (player.InvList[i].IDidx == IDI_ORACOOL_RUNE_TIR)
			tirCount++;
		if (player.InvList[i].IDidx == IDI_ORACOOL_RUNE_EL)
			elCount++;
	}
	EXPECT_EQ(tirCount, 1) << "two El runes should have become one Tir";
	EXPECT_EQ(elCount, 0) << "the consumed pair survived";
}

// Gems and runes became stackable at v1.7.30, and stacking asks a question potions never had to
// answer: what counts as "the same item". canStackWith compares _iMiscId, which is correct for
// potions (the item table carries duplicate indices for several) and catastrophic here - every gem
// and rune is IMISC_NONE, so that rule alone stacks a Ruby into an Emerald and silently converts it.
//
// For these two families the base-item INDEX is the whole identity, which is the same rule
// socketing already uses. This pins it in both directions.
TEST(OracoolCrafting, MaterialsStackOnlyWithTheirOwnKind)
{
	devilution::Item el {};
	InitializeItem(el, IDI_ORACOOL_RUNE_EL);
	devilution::Item el2 {};
	InitializeItem(el2, IDI_ORACOOL_RUNE_EL);
	devilution::Item tir {};
	InitializeItem(tir, IDI_ORACOOL_RUNE_TIR);

	EXPECT_TRUE(el.canStackWith(el2)) << "two El runes refused to stack with each other";
	EXPECT_FALSE(el.canStackWith(tir)) << "an El stacked with a Tir - the index is the identity";
	EXPECT_FALSE(tir.canStackWith(el)) << "the refusal must hold in both directions";

	// The same across families: a gem must never merge into a rune pile, and vice versa.
	devilution::Item gem {};
	InitializeItem(gem, static_cast<_item_indexes>(oracool::GemIndexFor(oracool::GemType::Ruby, oracool::GemQuality::Chipped)));
	devilution::Item otherGem {};
	InitializeItem(otherGem, static_cast<_item_indexes>(oracool::GemIndexFor(oracool::GemType::Emerald, oracool::GemQuality::Chipped)));
	devilution::Item sameGem {};
	InitializeItem(sameGem, static_cast<_item_indexes>(oracool::GemIndexFor(oracool::GemType::Ruby, oracool::GemQuality::Chipped)));

	EXPECT_TRUE(gem.canStackWith(sameGem)) << "two chipped Rubies refused to stack";
	EXPECT_FALSE(gem.canStackWith(otherGem)) << "a Ruby stacked with an Emerald";
	EXPECT_FALSE(gem.canStackWith(el)) << "a gem stacked with a rune";

	// A different QUALITY of the same gem is a different item too - the ladder depends on it.
	devilution::Item flawlessRuby {};
	InitializeItem(flawlessRuby, static_cast<_item_indexes>(oracool::GemIndexFor(oracool::GemType::Ruby, oracool::GemQuality::Flawless)));
	EXPECT_FALSE(gem.canStackWith(flawlessRuby)) << "two qualities of one gem merged into one stack";
}

TEST(OracoolCrafting, MixedGemsDoNotSatisfyThreeOfAKind)
{
	Players.resize(1);
	devilution::Player &player = Players[0];
	player = {};
	const int gems[3] = { IDI_ORACOOL_GEM_RUBY, IDI_ORACOOL_GEM_SAPPHIRE, IDI_ORACOOL_GEM_TOPAZ };
	for (int i = 0; i < 3; i++) {
		player.InvList[i] = {};
		player.InvList[i]._itype = ItemType::Misc;
		player.InvList[i].IDidx = static_cast<_item_indexes>(gems[i]);
	}
	player._pNumInv = 3;
	EXPECT_FALSE(oracool::CanCraft(player, 0)) << "three DIFFERENT gems satisfied 'three of one kind'";

	player.InvList[1].IDidx = IDI_ORACOOL_GEM_RUBY;
	player.InvList[2].IDidx = IDI_ORACOOL_GEM_RUBY;
	EXPECT_TRUE(oracool::CanCraft(player, 0));
	// Sol pairs must never ascend - there is nothing above Sol.
	player.InvList[0].IDidx = IDI_ORACOOL_RUNE_SOL;
	player.InvList[1].IDidx = IDI_ORACOOL_RUNE_SOL;
	player._pNumInv = 2;
	EXPECT_FALSE(oracool::CanCraft(player, 1)) << "a Sol pair offered an ascension past the ladder's top";
}

// Every 340x720 side panel draws over the same painted background, whose interior ends at
// oracool::SidePanelContentBottom - below that is the art's bottom ornament, and on the left-hand
// panels the health orb as well. The Abilities window was the last one still sizing its list as
// "panel height less a margin", so its rows ran to y=696, straight across that ornament. It also
// started its title band 16px lower than the other five, which is what the shared PanelTitleTop
// exists to prevent.
TEST(OracoolAudit2, AbilitiesWindowContentStaysInsideThePaintedFrame)
{
	const Rectangle panel = GetSpellBookPanelRect();
	const Rectangle content = GetSpellBookContentRect();

	// Relative to the panel, so this says nothing about where on screen the window sits.
	const int contentTop = content.position.y - panel.position.y;
	const int contentBottom = contentTop + content.size.height;

	EXPECT_GE(contentTop, oracool::PanelTitleTop + oracool::PanelTitleHeight)
	    << "the list starts inside the title band";
	EXPECT_EQ(contentBottom, oracool::SidePanelContentBottom)
	    << "the list runs past the background art's interior";
	EXPECT_EQ(panel.size.height, 720) << "no longer the shared side-panel size";
}

// Every row of the Paladin's Combat Skills tree that carries a spell slot is a second face on a skill
// oracool/paladin_skills.h owns - same slot, same investment store, same mana. They are NOT second
// implementations, so the tree must never disagree with that module about availability.
//
// All seven as of 2026-08-16: Hammer of Faith and Blessed Shield were given tree rows when the user
// removed the Skills sheet that had been their only home. The count below is the guard on that.
//
// They did. IsClassTreeSkillUnlocked stopped at the row's TIER level while the skill itself gates on
// its own minLevel and, for two of them, on a shield being held. The visible half was Smite: the
// tree lit it at level 1 bare-handed and let it be clicked onto a mouse button, where it then did
// nothing, while the Skills sheet greyed the very same skill out.
//
// Walks every level rather than spot-checking: an off-by-one in either gate is exactly the shape
// this bug had.
TEST(OracoolAudit2, TreeAndSkillsSheetAgreeOnBorrowedPaladinSkills)
{
	Players.resize(1);
	devilution::Player &player = Players[0];
	player = {};
	player._pClass = HeroClass::Warrior; // the Paladin's slot in this fork
	ASSERT_TRUE(oracool::ClassHasPaladinSkills(player)) << "the Paladin is no longer HeroClass::Warrior";

	// Pair them up through the spell slot, which is the thing they actually share - so a tree row
	// that later starts or stops borrowing needs no edit here.
	oracool::ClassTreeSkill page[oracool::ClassTreeSkillCount];
	const size_t count = oracool::BuildClassTreePage(player._pClass, 0, page);
	ASSERT_GT(count, 0u) << "the Paladin's Combat Skills page is empty";

	int paired = 0;
	for (size_t i = 0; i < count; i++) {
		// Deliberately NOT filtered through IsValidSpell: that also asks gbIsHellfire, which is false
		// in the test binary, and every Paladin skill's SpellID sits past SpellID::LastDiablo. The
		// slot is only being used to pair a tree row with a skill here, and SpellID::Invalid pairs
		// with nothing.
		const SpellID slot = oracool::ClassTreeSpellId(page[i]);
		for (size_t s = 0; s < oracool::PaladinSkillCount; s++) {
			const auto paladinSkill = static_cast<oracool::PaladinSkill>(s);
			if (oracool::GetPaladinSkillData(paladinSkill).spellId != slot)
				continue;
			paired++;
			// Both with and without a shield: two of the seven require one, and the tree knew
			// nothing about that requirement at all.
			for (const bool shield : { false, true }) {
				player.InvBody[INVLOC_HAND_RIGHT] = {};
				if (shield)
					player.InvBody[INVLOC_HAND_RIGHT]._itype = ItemType::Shield;
				for (int level = 1; level <= 40; level++) {
					player._pLevel = static_cast<int8_t>(level);
					EXPECT_EQ(oracool::IsClassTreeSkillUnlocked(player, page[i]),
					    oracool::IsPaladinSkillUnlocked(player, paladinSkill))
					    << "the tree and the Skills sheet disagree about "
					    << oracool::GetClassTreeSkillData(page[i]).name << " at level " << level
					    << (shield ? " holding a shield" : " bare-handed");
				}
			}
		}
	}
	EXPECT_EQ(paired, 7) << "the set of borrowed Paladin skills changed - is that deliberate?";
}

// The stash page grew from the vanilla 10x10 to 10x16 when it moved into the 340x720 theme, but
// AutoPlaceItemInStash's search rectangle kept the literal 10 in BOTH axes. Every auto-placement -
// Gillian's deposit, shift-click to stash, and the re-pack SortStash performs - therefore stopped
// at row 9 and spilled onto the next page while six rows sat empty.
//
// The user saw it through Sort, because Sort is the only one that clears the page first: it emptied
// the last six rows and then refused to refill them.
TEST(OracoolAudit2, StashAutoPlaceReachesEveryRowOfThePage)
{
	Players.resize(2);
	// Not MyPlayer: SortStash schedules an autosave for the local player, and the save layer is not
	// spun up headlessly. Placement itself is player-agnostic.
	MyPlayer = &Players[1];
	devilution::Player &player = Players[0];
	player = {};

	Stash.stashList.clear();
	Stash.stashGrids.clear();
	Stash.SetPage(0);

	devilution::Item potion {};
	InitializeItem(potion, IDI_HEAL);
	ASSERT_EQ(GetInventorySize(potion), (Size { 1, 1 })) << "test assumes a one-cell item";

	// Exactly one page's worth. Under the bug only 100 of these fit before the scan gave up and
	// wrapped to page 1.
	constexpr int CellsPerPage = StashGridColumns * StashGridRows;
	for (int i = 0; i < CellsPerPage; i++)
		ASSERT_TRUE(AutoPlaceItemInStash(player, potion, true)) << "placement failed at item " << i;

	EXPECT_EQ(Stash.stashGrids.size(), 1u) << "a full page's worth of items spilled onto a second page";
	for (int y = 0; y < StashGridRows; y++) {
		for (int x = 0; x < StashGridColumns; x++)
			EXPECT_NE(Stash.stashGrids[0][x][y], 0) << "cell " << x << "," << y << " was left empty";
	}

	// And the same holds through Sort, which is where it was reported: re-packing must not push
	// anything past the bottom row of page 0.
	SortStash(player);
	EXPECT_EQ(Stash.stashGrids.size(), 1u) << "Sort spilled a single page of items onto a second page";
	EXPECT_EQ(Stash.stashList.size(), static_cast<size_t>(CellsPerPage)) << "Sort lost items";
	for (int x = 0; x < StashGridColumns; x++)
		EXPECT_NE(Stash.stashGrids[0][x][StashGridRows - 1], 0) << "Sort left the bottom row empty at column " << x;

	Stash.stashList.clear();
	Stash.stashGrids.clear();
	Stash.SetPage(0);
}

// The scan rectangle shrinks by the item's own footprint so a multi-cell item can't hang off the
// bottom edge. That subtraction is exactly where the old literal hid, so pin the tall case too:
// a 2x3 item must be placeable with its last row on the page's last row, and no further.
TEST(OracoolAudit2, StashAutoPlaceSeatsTallItemsAgainstTheBottomEdge)
{
	Players.resize(2);
	MyPlayer = &Players[1];
	devilution::Player &player = Players[0];
	player = {};

	Stash.stashList.clear();
	Stash.stashGrids.clear();
	Stash.SetPage(0);

	devilution::Item tallItem {};
	InitializeItem(tallItem, IDI_ROGUE); // short bow - the tallest starting item
	const Size tallSize = GetInventorySize(tallItem);
	ASSERT_GT(tallSize.height, 1) << "test assumes a multi-row item";

	// Occupy every row above the last band the item could fit in, so the first-fit scan is forced
	// all the way down. These are grid marks with no matching stashList entries - the placement scan
	// only asks "is this cell non-zero", and starting well past any index it will assign keeps the
	// two from being confused with each other.
	const int rowsToBlock = StashGridRows - tallSize.height;
	uint16_t filler = 1000;
	for (int y = 0; y < rowsToBlock; y++) {
		for (int x = 0; x < StashGridColumns; x++)
			Stash.stashGrids[0][x][y] = filler++;
	}

	ASSERT_TRUE(AutoPlaceItemInStash(player, tallItem, true)) << "no room left for the item in the bottom band";
	// Under the bug this did NOT fail outright - it ran off the end of the truncated scan and wrapped
	// to page 1, landing at that page's top-left. Pin the page as well as the row, or the wrap reads
	// as a success.
	EXPECT_EQ(Stash.stashGrids.size(), 1u) << "the item wrapped onto a second page with rows still free here";
	// Items are anchored by their BOTTOM-left cell (see AutoPlaceItemInStash), so the recorded
	// position's y is the last row it covers.
	EXPECT_EQ(Stash.stashList.back().position.y, StashGridRows - 1)
	    << "the item did not reach the bottom band of the page";

	Stash.stashList.clear();
	Stash.stashGrids.clear();
	Stash.SetPage(0);
}

// ---------------------------------------------------------------------------------------------
// Phase 1 item sets: the keyword vocabulary.
//
// Fifteen sets (94 items) were delivered as data on 2026-08-16 and filed at
// Oracool.MPQ/02-source-art/item-sets/. Their stats are written in a vocabulary of their own, and
// oracool/item_set_stats.cpp is where each of those words is decided: exact, approximate, or inert.
//
// The list below is not retyped - it was EXTRACTED from the fifteen set-data.json files:
//
//   find . -name set-data.json -exec cat {} \; | grep -oE '"[a-z_]+:[^"]*"' \
//     | sed 's/^"//; s/:.*//' | sort -u
//
// so this test is the guard on the real content. A keyword the data uses and the table has never
// heard of is a stat that would silently vanish from an item, which is the one failure mode the
// whole table exists to prevent.
// ---------------------------------------------------------------------------------------------
const char *const DeliveredSetStatKeywords[] = {
	"acid_damage",
	"active_temper_resistance",
	"all_spell_levels",
	"armor_class_flat",
	"armor_per_held_petition",
	"attack_speed",
	"aura",
	"block",
	"block_speed",
	"boss_mute_duration",
	"bow_attack_speed",
	"bow_chance_to_hit",
	"bow_damage_flat",
	"bow_distance_penalty",
	"chance_to_hit",
	"condemned_duration",
	"condemned_target_direct_damage_per_spent",
	"confluence_duration",
	"confluence_physical_damage",
	"confluence_sequence",
	"confluence_set_elemental_damage",
	"counterstroke_damage",
	"counterstroke_duration",
	"crimson_compact_stacks",
	"crimson_melee_damage_per_stack",
	"crimson_recovery",
	"crowned_resolve",
	"curse_duration",
	"damage_taken_flat",
	"damage_vs_demons",
	"damage_vs_undead",
	"dexterity",
	"direct_damage_per_blight_stack",
	"direct_damage_per_petition",
	"eligible_vendor_sale_proceeds",
	"enhanced_armor",
	"enhanced_bow_damage",
	"enhanced_damage",
	"evasion",
	"final_audience_targets",
	"fire_damage",
	"flow_required",
	"gold_from_monsters",
	"hit_recovery",
	"hostile_damage_taken_per_greed_rank",
	"hostile_spell_damage_reduction",
	"hush_credits_required",
	"hush_duration",
	"interrupt",
	"knockback",
	"life",
	"light_radius",
	"lightning_damage",
	"magic",
	"mana",
	"max_life",
	"max_mana",
	"max_resist_fire",
	"melee_damage_flat",
	"memorized_spell_mana_cost",
	"minimum_petitions",
	"mute_duration",
	"off_hand_focus",
	"ordinary_door_action_range",
	"ordinary_trap_disarm_range",
	"penitent_cooldown_per_trigger",
	"penitent_engine_damage",
	"penitent_engine_triggers",
	"petition_cap",
	"petition_gain",
	"potion_healing",
	"primary_condemned_duration",
	"primary_condemned_kill_refund",
	"proc",
	"resist_all",
	"resist_fire",
	"resist_lightning",
	"resist_magic",
	"route_credit_tiles",
	"secondary_condemned_half_magnitude",
	"shed_charge_roots",
	"shedstrike",
	"shedstrike_duration",
	"shield_block",
	"skill",
	"spell_damage",
	"spell_mana_cost",
	"spell_reflect",
	"stance",
	"strength",
	"survey_radius",
	"temper_carrier",
	"temper_damage",
	"thorns",
	"to_hit",
	"trace_route",
	"trace_route_path_length",
	"unarmed_attack_speed",
	"unarmed_damage",
	"unarmed_to_hit",
	"vitality",
	"walk_speed",
	"weapon_damage",
	"weapon_hands",
	"wearer_direct_damage_taken_by_target_per_spent",
	"wyrmturn_cost",
	"wyrmturn_lockout",
};

TEST(OracoolItemSets, EveryDeliveredKeywordHasAMapping)
{
	for (const char *keyword : DeliveredSetStatKeywords) {
		EXPECT_NE(oracool::FindSetStat(keyword), nullptr)
		    << "set-data.json uses '" << keyword << "' and oracool/item_set_stats.cpp has no row for "
		    << "it - the stat would be dropped without a word";
	}
	EXPECT_EQ(std::size(DeliveredSetStatKeywords), oracool::SetStatMappingCount)
	    << "the table and the delivered vocabulary are different sizes";
}

// FindSetStat binary-searches, so the order is load-bearing rather than cosmetic. It is also how a
// human reads 107 rows.
TEST(OracoolItemSets, MappingTableIsSortedAndUnique)
{
	for (size_t i = 1; i < oracool::SetStatMappingCount; i++) {
		EXPECT_LT(std::string(oracool::SetStatMappings[i - 1].keyword),
		    std::string(oracool::SetStatMappings[i].keyword))
		    << "row " << i << " (" << oracool::SetStatMappings[i].keyword
		    << ") is out of order or duplicated - FindSetStat binary-searches this table";
	}
}

// The fidelity column and the power column have to agree, or a row lies about itself: an inert row
// carrying a real power would apply it, and a live row without one would apply nothing.
TEST(OracoolItemSets, FidelityAndPowerAgree)
{
	int live = 0;
	for (const oracool::SetStatMapping &row : oracool::SetStatMappings) {
		if (row.fidelity == oracool::SetStatFidelity::Inert) {
			EXPECT_EQ(row.power, IPL_INVALID) << row.keyword << " is marked inert but carries a power";
			EXPECT_NE(row.note, nullptr) << row.keyword << " is inert and does not say what it would need";
			EXPECT_FALSE(oracool::IsSetStatLive(row.keyword)) << row.keyword;
		} else {
			live++;
			EXPECT_NE(row.power, IPL_INVALID) << row.keyword << " claims to work but names no power";
			EXPECT_TRUE(oracool::IsSetStatLive(row.keyword)) << row.keyword;
		}
		// An approximation that does not say how it differs is just an undocumented lie.
		if (row.fidelity == oracool::SetStatFidelity::Approx)
			EXPECT_NE(row.note, nullptr) << row.keyword << " is approximate and does not say how";
	}
	// Not a target, just a tripwire: if a later pass implements the bespoke mechanics this should
	// climb, and if it ever DROPS someone has quietly disconnected a stat.
	EXPECT_GE(live, 30) << "fewer keywords work than when this table was written";
}

TEST(OracoolItemSets, UnknownKeywordIsRejectedRatherThanGuessed)
{
	EXPECT_EQ(oracool::FindSetStat("no_such_stat"), nullptr);
	EXPECT_FALSE(oracool::IsSetStatLive("no_such_stat"));
	// Prefixes and suffixes of real keys must not match either - lower_bound makes that a real risk.
	EXPECT_EQ(oracool::FindSetStat("stren"), nullptr);
	EXPECT_EQ(oracool::FindSetStat("strengthx"), nullptr);
}

// ---------------------------------------------------------------------------------------------
// Phase 1 item sets: the generated content tables.
//
// Source/oracool/item_sets_data.inc is machine-written from the fifteen set-data.json files, so
// these tests are not checking arithmetic - they are checking that the generator's OUTPUT still
// describes the content it was given, and that the hand-written accessors around it agree with the
// shape of that output.
//
// The spot-check values below were read off set-01's own set-data.json by hand. That is the point:
// one item verified against the source by eye anchors the other ninety-three, which were produced
// by the same code path.
// ---------------------------------------------------------------------------------------------

TEST(OracoolItemSets, SetRangesCoverEveryItemAndBonusExactlyOnce)
{
	int items = 0;
	int bonuses = 0;
	for (const oracool::ItemSetDefinition &set : oracool::ItemSets) {
		EXPECT_EQ(set.firstItem, items) << set.id << "'s items do not begin where the previous set's ended";
		EXPECT_EQ(set.firstBonus, bonuses) << set.id << "'s bonuses do not begin where the previous set's ended";
		EXPECT_GT(set.itemCount, 0) << set.id << " has no items";
		EXPECT_GT(set.bonusCount, 0) << set.id << " has no bonus ladder";
		items += set.itemCount;
		bonuses += set.bonusCount;
	}
	// Contiguous AND complete: a gap would silently orphan items, an overlap would double-count them.
	EXPECT_EQ(items, static_cast<int>(oracool::ItemSetItemCount));
	EXPECT_EQ(bonuses, static_cast<int>(oracool::ItemSetBonusCount));
}

TEST(OracoolItemSets, ItemIdsAreUniqueAndResolveToTheirOwnSet)
{
	for (size_t i = 0; i < oracool::ItemSetItemCount; i++) {
		const oracool::SetItemDefinition &item = oracool::ItemSetItems[i];
		EXPECT_EQ(oracool::FindSetItem(item.id), &item) << item.id << " resolves to a different row - duplicate id?";
		const oracool::ItemSetDefinition *owner = oracool::FindItemSetOwning(item.id);
		ASSERT_NE(owner, nullptr) << item.id << " belongs to no set";
		const auto index = static_cast<int>(i);
		EXPECT_GE(index, owner->firstItem);
		EXPECT_LT(index, owner->firstItem + owner->itemCount);
	}
	EXPECT_EQ(oracool::FindSetItem("SET_NO_SUCH_THING"), nullptr);
	EXPECT_EQ(oracool::FindItemSetOwning("SET_NO_SUCH_THING"), nullptr);
}

// Read off set-01-vestments-of-the-ashen-saint/set-data.json by hand. Every number here is one the
// generator had to transform, not copy: the percentage that became a speed TIER, the negative that
// became a positive because IPL_GETHIT subtracts, and the stat that vanished because it is inert.
TEST(OracoolItemSets, GeneratedValuesMatchTheDeliveredJson)
{
	const oracool::SetItemDefinition *helm = oracool::FindSetItem("SET_ASHEN_HELM");
	ASSERT_NE(helm, nullptr);
	EXPECT_EQ(helm->requiredLevel, 18);
	EXPECT_EQ(helm->requiredStrength, 30);
	EXPECT_EQ(helm->grid.width, 2);
	EXPECT_EQ(helm->grid.height, 2);
	EXPECT_EQ(helm->armorMin, 12);
	EXPECT_EQ(helm->armorMax, 16);
	EXPECT_EQ(helm->powers[0].type, IPL_VIT);
	EXPECT_EQ(helm->powers[0].param1, 8);
	// thorns:[1,3] - a RANGE, so the two parameters differ where every scalar stat repeats itself.
	EXPECT_EQ(helm->powers[2].type, IPL_THORNS);
	EXPECT_EQ(helm->powers[2].param1, 1);
	EXPECT_EQ(helm->powers[2].param2, 3);
	// light_radius:-1 keeps its sign.
	EXPECT_EQ(helm->powers[3].type, IPL_LIGHT);
	EXPECT_EQ(helm->powers[3].param1, -1);

	const oracool::SetItemDefinition *armor = oracool::FindSetItem("SET_ASHEN_ARMOR");
	ASSERT_NE(armor, nullptr);
	// enhanced_armor is IPL_TARGAC (-> _iPLEnAc), NOT IPL_ACP (-> _iPLAC). Getting this pair the
	// wrong way round would apply the bonus to the wrong field and nothing would look broken.
	EXPECT_EQ(armor->powers[0].type, IPL_TARGAC);
	EXPECT_EQ(armor->powers[0].param1, 20);
	// damage_taken_flat:-1 arrives POSITIVE: SaveItemPower does `_iPLGetHit -= r`.
	EXPECT_EQ(armor->powers[3].type, IPL_GETHIT);
	EXPECT_EQ(armor->powers[3].param1, 1);

	// attack_speed:+10% is a TIER, not ten of anything.
	const oracool::SetItemDefinition *gloves = oracool::FindSetItem("SET_ASHEN_GLOVES");
	ASSERT_NE(gloves, nullptr);
	EXPECT_EQ(gloves->powers[2].type, IPL_FASTATTACK);
	EXPECT_EQ(gloves->powers[2].param1, 1);

	// The belt declares four stats and carries three - potion_healing has no channel and is gone
	// rather than approximated.
	const oracool::SetItemDefinition *belt = oracool::FindSetItem("SET_ASHEN_BELT");
	ASSERT_NE(belt, nullptr);
	EXPECT_EQ(oracool::CountLivePowers(belt->powers, 6), 3);
}

// The bonus ladder replaces rather than stacks, and a rung can be entirely inert.
TEST(OracoolItemSets, BonusLadderPicksTheHighestRungReached)
{
	const oracool::ItemSetDefinition *ashen = oracool::FindItemSetOwning("SET_ASHEN_HELM");
	ASSERT_NE(ashen, nullptr);

	EXPECT_EQ(oracool::ActiveSetBonus(*ashen, 1), nullptr) << "one piece is not a set";
	const oracool::SetBonusDefinition *two = oracool::ActiveSetBonus(*ashen, 2);
	ASSERT_NE(two, nullptr);
	EXPECT_EQ(two->pieces, 2);
	EXPECT_EQ(oracool::ActiveSetBonus(*ashen, 5)->pieces, 5);
	// More pieces than the ladder has rungs still resolves to the top rung rather than to nothing.
	EXPECT_EQ(oracool::ActiveSetBonus(*ashen, 99)->pieces, 6);

	// "Cinderbrand", the four-piece, is a single proc: - named, earned, and inert. This is the
	// inert-row rule reaching the player, and it is deliberate.
	const oracool::SetBonusDefinition *four = oracool::ActiveSetBonus(*ashen, 4);
	ASSERT_NE(four, nullptr);
	EXPECT_EQ(oracool::CountLivePowers(four->powers, 4), 0)
	    << "Cinderbrand gained a working stat - if that is deliberate, update this test";
	EXPECT_NE(four->name, nullptr) << "an inert rung must still be named";
}

// Every power the generator emitted has to be one SaveItemPower will actually act on. An
// IPL_INVALID in a leading slot would mean a stat was dropped silently mid-list.
TEST(OracoolItemSets, PowerListsAreDenseAndValid)
{
	for (const oracool::SetItemDefinition &item : oracool::ItemSetItems) {
		bool seenEmpty = false;
		for (const ItemPower &power : item.powers) {
			if (power.type == IPL_INVALID) {
				seenEmpty = true;
				continue;
			}
			EXPECT_FALSE(seenEmpty) << item.id << " has a live power after an empty slot";
		}
		EXPECT_GT(oracool::CountLivePowers(item.powers, 6), 0)
		    << item.id << " carries no working stat at all - it would be a plain base item";
	}
}

// Every set item's icon must be its own, and must be the frame the tables say it is: the CEL, the
// ICURS_ ids and the width/height rows are three files generated in one pass, and if their order
// ever diverged every icon after the divergence would be silently wrong.
TEST(OracoolItemSets, CursorIdsAreUniqueContiguousAndRoundTrip)
{
	for (size_t i = 0; i < oracool::ItemSetItemCount; i++) {
		const oracool::SetItemDefinition &item = oracool::ItemSetItems[i];
		EXPECT_EQ(oracool::FindSetItemByCursor(item.cursor), &item)
		    << item.id << "'s cursor resolves to a different item";
		// Contiguous and in table order, which is what lets the lookup be an index.
		EXPECT_EQ(item.cursor, ICURS_ORACOOL_SET_ASHEN_HELM + static_cast<int>(i)) << item.id;
	}
	EXPECT_EQ(oracool::FindSetItemByCursor(ICURS_ORACOOL_SET_ASHEN_HELM - 1), nullptr);
	EXPECT_EQ(oracool::FindSetItemByCursor(ICURS_ORACOOL_SET_COURT_RELIQUARY + 1), nullptr);
}

// The icon's cell size has to match the footprint the item declares, or a 2x3 item draws through a
// 2x2 frame. cursor.cpp's tables are the ones the renderer reads; the set table is what the
// inventory grid reserves space from.
TEST(OracoolItemSets, IconCellSizeMatchesTheDeclaredFootprint)
{
	for (const oracool::SetItemDefinition &item : oracool::ItemSetItems) {
		const Size cell = GetInvItemSize(item.cursor + CURSOR_FIRSTITEM);
		EXPECT_EQ(cell.width, item.grid.width * 28) << item.id << " icon is the wrong width";
		EXPECT_EQ(cell.height, item.grid.height * 28) << item.id << " icon is the wrong height";
	}
}

// Set bonuses ride the Phase 0.4 provider seam, whose whole reason for having a condition hook was
// "three pieces worn?". Wearing nothing must contribute nothing, and the count must come from the
// equipment rather than from any stored state.
TEST(OracoolItemSets, BonusesRequireTheirPiecesToBeWorn)
{
	Players.resize(1);
	devilution::Player &player = Players[0];
	player = {};
	MyPlayer = &player;

	const oracool::ItemSetDefinition *ashen = oracool::FindItemSetOwning("SET_ASHEN_HELM");
	ASSERT_NE(ashen, nullptr);
	EXPECT_EQ(oracool::WornSetPieces(player, *ashen), 0);
	EXPECT_FALSE(oracool::AnySetBonusActive(player));

	// Carrying is not wearing: a piece in the backpack must not count.
	const oracool::SetItemDefinition *helm = oracool::FindSetItem("SET_ASHEN_HELM");
	ASSERT_NE(helm, nullptr);
	player.InvList[0] = {};
	InitializeItem(player.InvList[0], static_cast<_item_indexes>(oracool::BaseItemForSetSlot(helm->slot)));
	oracool::MakeSetItem(player.InvList[0], *helm);
	player._pNumInv = 1;
	EXPECT_EQ(oracool::WornSetPieces(player, *ashen), 0) << "a carried piece counted toward the set";

	// Worn, it counts - but one piece is still below the first rung.
	player.InvBody[INVLOC_HEAD] = player.InvList[0];
	EXPECT_EQ(oracool::WornSetPieces(player, *ashen), 1);
	EXPECT_FALSE(oracool::AnySetBonusActive(player)) << "one piece should not earn a rung";

	// Two worn pieces reach the two-piece rung.
	const oracool::SetItemDefinition *belt = oracool::FindSetItem("SET_ASHEN_BELT");
	ASSERT_NE(belt, nullptr);
	devilution::Item beltItem {};
	InitializeItem(beltItem, static_cast<_item_indexes>(oracool::BaseItemForSetSlot(belt->slot)));
	oracool::MakeSetItem(beltItem, *belt);
	player.InvBody[INVLOC_WAIST] = beltItem;
	EXPECT_EQ(oracool::WornSetPieces(player, *ashen), 2);
	EXPECT_TRUE(oracool::AnySetBonusActive(player));

	// And the rung's stats actually reach the totals. Warmth of the Reliquary is +15 fire resist
	// and +5 vitality; the two worn pieces carry fire resist of their own, so this checks the DELTA
	// the bonus adds rather than an absolute.
	oracool::ItemBonusTotals withBonus {};
	oracool::ApplySetBonusesToTotals(player, withBonus);
	EXPECT_EQ(withBonus.fireResist, 15) << "the two-piece rung's resistance did not reach the totals";
	EXPECT_EQ(withBonus.vitality, 5) << "the two-piece rung's vitality did not reach the totals";
}

// MakeSetItem has to produce something the rest of the engine recognises as a real item, not a
// half-filled struct: a name, the Set tier, its own icon, and its stats in the ordinary fields.
TEST(OracoolItemSets, MakeSetItemProducesARecognisableItem)
{
	Players.resize(1);
	MyPlayer = &Players[0];
	Players[0] = {};

	const oracool::SetItemDefinition *helm = oracool::FindSetItem("SET_ASHEN_HELM");
	ASSERT_NE(helm, nullptr);
	devilution::Item item {};
	InitializeItem(item, static_cast<_item_indexes>(oracool::BaseItemForSetSlot(helm->slot)));
	oracool::MakeSetItem(item, *helm);

	EXPECT_TRUE(oracool::IsSetItem(item));
	EXPECT_EQ(item._iOracoolTier, OracoolItemTier::Set);
	EXPECT_EQ(item._iCurs, helm->cursor);
	EXPECT_TRUE(item._iIdentified) << "a set item arrives identified - its stats are not a secret";
	EXPECT_STREQ(item._iIName, "Vhal's Blackened Halo");
	// vitality:+8 and resist_fire:+20 land in the ordinary fields, via the engine's own applier.
	EXPECT_EQ(item._iPLVit, 8);
	EXPECT_EQ(item._iPLFR, 20);
	// The helm's armour is a RANGE (12-16), so this checks the band rather than a value.
	EXPECT_GE(item._iAC, 12);
	EXPECT_LE(item._iAC, 16);
	// A base item is not a set item.
	devilution::Item plain {};
	InitializeItem(plain, IDI_ORACOOL_HELM);
	EXPECT_FALSE(oracool::IsSetItem(plain));
}
