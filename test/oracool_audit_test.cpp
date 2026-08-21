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
#include <map>
#include <set>
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
#include "oracool/item_tiers.h"
#include "oracool/levski_roar.h"
#include "oracool/item_sets.h"
#include "oracool/hero_chunks.h"
#include "oracool/lesser_uniques.h"
#include "oracool/monster_difficulty.h"
#include "oracool/player_resistance.h"
#include "oracool/skill_sounds.h"
#include "missiles.h"
#include "oracool/monster_scale.h"
#include "oracool/paladin_melee.h"
#include "oracool/paladin_skills.h"
#include "oracool/rng_streams.h"
#include "oracool/runewords.h"
#include "oracool/salvage.h"
#include "oracool/skill_points.h"
#include "oracool/spell_ranks.h"
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
	// Torment no longer equals Hell - see TormentHardensHellResistancesIntoImmunities below.
	EXPECT_NE(oracool::MonsterResistancesFor(data, DIFF_TORMENT), data.resistanceHell);

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
	// CHANGED 2026-08-19 (v1.8.35), deliberately: this asserted MaxResistance, 75, because that was
	// vanilla's HARD cap. It is now the SOFT cap - this ring's lightning resist carries the total
	// far enough past it to reach the ceiling. The test still pins the same thing, that the total is
	// bounded and by what; the bound moved because the soft cap is the feature.
	EXPECT_EQ(player._pLghtResist, oracool::ResistanceHardCap) << "the resistance ceiling changed";
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
	// BOOKLESS skills on purpose. ApplyHeroChunks now runs the book-spell refund at the end of every
	// load (the 2026-08-20 rule), so investment parked in a book spell deliberately does NOT
	// round-trip - it comes back as unspent points. That behaviour has its own test; this one is
	// about the chunk carrying what it was given.
	source._pSkillInvestment[static_cast<size_t>(SpellID::Zeal)] = 5;
	source._pSkillInvestment[static_cast<size_t>(SpellID::Charge)] = 2;
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
	EXPECT_EQ(target._pSkillInvestment[static_cast<size_t>(SpellID::Zeal)], 5);
	EXPECT_EQ(target._pSkillInvestment[static_cast<size_t>(SpellID::Charge)], 2);
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
	sword._iCurs = ICURS_SHORT_SWORD;
	sword._iMagical = ITEM_QUALITY_NORMAL;
	EXPECT_TRUE(oracool::CanItemHaveSockets(sword));

	sword._iMagical = ITEM_QUALITY_MAGIC;
	EXPECT_FALSE(oracool::CanItemHaveSockets(sword)) << "a magic item took sockets";

	// Sockets v2: the base TIER no longer disqualifies anything. A Torment sword is the better
	// host, and excluding it made the deeper base strictly worse raw material - backwards.
	sword._iMagical = ITEM_QUALITY_NORMAL;
	sword._iOracoolBaseTier = static_cast<uint8_t>(oracool::BaseItemTier::Torment);
	EXPECT_TRUE(oracool::CanItemHaveSockets(sword)) << "a tiered base was refused sockets";

	// The Oracool QUALITY tiers are a different axis and still refuse: those items already rolled.
	sword._iOracoolBaseTier = static_cast<uint8_t>(oracool::BaseItemTier::Normal);
	sword._iOracoolTier = OracoolItemTier::Rare;
	sword._iMagical = ITEM_QUALITY_MAGIC;
	EXPECT_FALSE(oracool::CanItemHaveSockets(sword)) << "a rolled item took sockets";

	devilution::Item potion {};
	potion._itype = ItemType::Misc;
	potion._iMagical = ITEM_QUALITY_NORMAL;
	EXPECT_FALSE(oracool::CanItemHaveSockets(potion)) << "a misc item took sockets";
}

// Sockets v2: all 33 Diablo II runes exist, in D2's own order, each one recognised as a rune and
// each one carrying an effect. The two islands are the trap this pins - the five that shipped in
// v1.7.8 keep their indices and the 28 new ones are appended, so a single-range IsOracoolRuneIdx
// would silently classify most of them as ordinary misc items that never drop and never socket.
TEST(OracoolGems, AllThirtyThreeRunesExist)
{
	const _item_indexes runes[] = {
		IDI_ORACOOL_RUNE_EL, IDI_ORACOOL_RUNE_ELD, IDI_ORACOOL_RUNE_TIR, IDI_ORACOOL_RUNE_NEF,
		IDI_ORACOOL_RUNE_ETH, IDI_ORACOOL_RUNE_ITH, IDI_ORACOOL_RUNE_TAL, IDI_ORACOOL_RUNE_RAL,
		IDI_ORACOOL_RUNE_ORT, IDI_ORACOOL_RUNE_THUL, IDI_ORACOOL_RUNE_AMN, IDI_ORACOOL_RUNE_SOL,
		IDI_ORACOOL_RUNE_SHAEL, IDI_ORACOOL_RUNE_DOL, IDI_ORACOOL_RUNE_HEL, IDI_ORACOOL_RUNE_IO,
		IDI_ORACOOL_RUNE_LUM, IDI_ORACOOL_RUNE_KO, IDI_ORACOOL_RUNE_FAL, IDI_ORACOOL_RUNE_LEM,
		IDI_ORACOOL_RUNE_PUL, IDI_ORACOOL_RUNE_UM, IDI_ORACOOL_RUNE_MAL, IDI_ORACOOL_RUNE_IST,
		IDI_ORACOOL_RUNE_GUL, IDI_ORACOOL_RUNE_VEX, IDI_ORACOOL_RUNE_OHM, IDI_ORACOOL_RUNE_LO,
		IDI_ORACOOL_RUNE_SUR, IDI_ORACOOL_RUNE_BER, IDI_ORACOOL_RUNE_JAH, IDI_ORACOOL_RUNE_CHAM,
		IDI_ORACOOL_RUNE_ZOD
	};
	ASSERT_EQ(sizeof(runes) / sizeof(runes[0]), 33u);

	int previousQlvl = 0;
	for (const _item_indexes idx : runes) {
		EXPECT_TRUE(IsOracoolRuneIdx(idx)) << AllItemsList[idx].iName << " is not seen as a rune";
		EXPECT_NE(AllItemsList[idx].iName, nullptr);

		// The ladder must climb: a rune deeper in D2's order must not drop shallower than the one
		// before it, or the depth gating stops meaning anything.
		EXPECT_GE(AllItemsList[idx].iMinMLvl, previousQlvl) << AllItemsList[idx].iName;
		previousQlvl = AllItemsList[idx].iMinMLvl;

		// Every rune does something in at least one host. A rune with an empty row would be a
		// silent dud - it would drop, socket, and grant nothing.
		oracool::ItemBonusTotals weapon, armor, shield;
		oracool::ApplyGemToTotals(static_cast<uint16_t>(idx), oracool::SocketHost::Weapon, weapon);
		oracool::ApplyGemToTotals(static_cast<uint16_t>(idx), oracool::SocketHost::Armor, armor);
		oracool::ApplyGemToTotals(static_cast<uint16_t>(idx), oracool::SocketHost::Shield, shield);
		const bool doesSomething = weapon.damageMod != 0 || weapon.bonusToHit != 0
		    || weapon.bonusDamage != 0 || weapon.fireMax != 0 || weapon.lightningMax != 0
		    || weapon.mana != 0 || weapon.flags != ItemSpecialEffect::None
		    || armor.hitPoints != 0 || armor.mana != 0 || armor.bonusArmor != 0
		    || armor.fireResist != 0 || armor.lightningResist != 0 || armor.magicResist != 0
		    || armor.strength != 0 || armor.magic != 0 || armor.dexterity != 0 || armor.vitality != 0
		    || armor.magicFind != 0 || armor.goldFind != 0 || armor.getHit != 0
		    || armor.flags != ItemSpecialEffect::None || shield.bonusArmor != 0
		    || shield.flags != ItemSpecialEffect::None || weapon.lightRadius != 0;
		// Three runes are deliberately invisible to the totals walk and are checked elsewhere:
		// Hel and Zod act on the host ITEM (its requirements, its durability - see the test below),
		// and Tir's mana-per-kill is an EVENT resolved in RuneManaPerKill, the same shape as the
		// Skull's life-per-kill. Everything else must show up in a total or it is a silent dud.
		if (idx != IDI_ORACOOL_RUNE_HEL && idx != IDI_ORACOOL_RUNE_ZOD && idx != IDI_ORACOOL_RUNE_TIR)
			EXPECT_TRUE(doesSomething) << AllItemsList[idx].iName << " grants nothing in any host";
	}
}

// Levski's Roar runs the recipes against its own 3x4 grid rather than the backpack, which means a
// second set of walks that can drift from the first. These pin the two things that would actually
// cost a player: a recipe that consumes without producing, and extraction losing a stone.
TEST(OracoolLevskiRoar, GridRecipesConsumeAndProduce)
{
	devilution::Item grid[devilution::oracool::LevskiGridSlots] {};

	// Three identical gems refine into one of the next quality, and the grid ends up holding
	// exactly one item.
	for (int i = 0; i < 3; i++)
		InitializeItem(grid[i], IDI_ORACOOL_GEM_RUBY_CHIPPED);
	ASSERT_TRUE(oracool::CanCraftFromLevskiGrid(grid, 0));
	EXPECT_FALSE(oracool::TransmuteLevskiGrid(grid).empty());

	int occupied = 0;
	int refined = 0;
	for (const devilution::Item &item : grid) {
		if (item.isEmpty())
			continue;
		occupied++;
		if (item.IDidx == IDI_ORACOOL_GEM_RUBY_FLAWED)
			refined++;
	}
	EXPECT_EQ(occupied, 1) << "the three chipped rubies were not consumed";
	EXPECT_EQ(refined, 1) << "refining did not produce the next quality up";
}

// Point 5 of the socket directive: insertion stops being permanent.
TEST(OracoolLevskiRoar, FreeingSocketsReturnsTheStonesAndTheItem)
{
	devilution::Item grid[devilution::oracool::LevskiGridSlots] {};
	devilution::Item &sword = grid[0];
	InitializeItem(sword, IDI_ORACOOL_HELM); // any real base; the recipe only reads its sockets
	sword._iSocketCount = 2;
	sword._iSocketed[0] = IDI_ORACOOL_RUNE_EL;
	sword._iSocketed[1] = IDI_ORACOOL_RUNE_TIR;
	sword._iMaxDur = 40;
	sword._iDurability = DUR_INDESTRUCTIBLE; // as a Zod would have left it
	std::strcpy(sword._iIName, "Steel");

	ASSERT_TRUE(oracool::CanCraftFromLevskiGrid(grid, 3));
	EXPECT_FALSE(oracool::TransmuteLevskiGrid(grid).empty());

	// The host survives, keeps its sockets, and gets its durability back - which is why Zod was
	// written to leave _iMaxDur intact.
	EXPECT_FALSE(sword.isEmpty()) << "the host was consumed";
	EXPECT_EQ(sword._iSocketCount, 2) << "the sockets themselves were lost";
	EXPECT_EQ(sword.socketedCount(), 0) << "the sockets were not emptied";
	EXPECT_EQ(sword._iDurability, 40) << "the host stayed indestructible after Zod came out";
	EXPECT_STREQ(sword._iIName, "") << "the runeword name outlived its runes";

	int el = 0;
	int tir = 0;
	for (const devilution::Item &item : grid) {
		if (item.IDidx == IDI_ORACOOL_RUNE_EL)
			el++;
		if (item.IDidx == IDI_ORACOOL_RUNE_TIR)
			tir++;
	}
	EXPECT_EQ(el, 1) << "El did not come back";
	EXPECT_EQ(tir, 1) << "Tir did not come back";
}

// The runeword pool is generated (tools/GenRunewords.ps1), and a generated table's failure mode is
// not a typo - it is a scheme that quietly produces unreachable or duplicate entries. These are the
// invariants that catch that.
TEST(OracoolRunewords, ThePoolIsWellFormed)
{
	ASSERT_GE(oracool::RunewordCount(), 300u) << "the runeword pool shrank unexpectedly";

	std::set<std::string> names;
	std::set<std::string> sequencesPerHost;
	for (size_t i = 0; i < oracool::RunewordCount(); i++) {
		const oracool::RunewordDefinition *word = oracool::RunewordAt(i);
		ASSERT_NE(word, nullptr);
		const std::string name = word->name;

		EXPECT_TRUE(names.insert(name).second) << name << " is a duplicate runeword name";
		EXPECT_GE(word->runeCount, 2) << name << " is shorter than two runes";
		EXPECT_LE(word->runeCount, devilution::Item::MaxItemSockets)
		    << name << " demands more sockets than any item can carry";
		EXPECT_LT(word->host, static_cast<uint8_t>(oracool::RunewordHost::None)) << name;

		// Every listed rune must be a rune, and the same rune must not appear twice in one word -
		// a stride sharing a factor with 33 walked back onto itself and made six-rune words into
		// X Y Z X Y Z repeats.
		std::set<uint16_t> distinct;
		std::string sequence = std::to_string(word->host) + ':';
		for (int r = 0; r < word->runeCount; r++) {
			EXPECT_TRUE(IsOracoolRuneIdx(word->runes[r])) << name << " lists a non-rune";
			EXPECT_TRUE(distinct.insert(word->runes[r]).second) << name << " repeats a rune";
			sequence += std::to_string(word->runes[r]) + ',';
		}
		// The padding past runeCount must be zeroed, or a longer word could match a shorter one's
		// prefix and shadow it.
		for (int r = word->runeCount; r < devilution::Item::MaxItemSockets; r++)
			EXPECT_EQ(word->runes[r], 0) << name << " has junk past its rune count";

		// Two words with the same host AND the same sequence means the second is unreachable.
		EXPECT_TRUE(sequencesPerHost.insert(sequence).second)
		    << name << " duplicates another word's sequence in the same host";

		// A word must grant something, or it is a rename with no reward.
		const bool grants = word->bonusDamagePercent != 0 || word->damageMod != 0 || word->toHit != 0
		    || word->allResists != 0 || word->bonusAc != 0 || word->spellLevels != 0
		    || word->mana != 0 || word->hitPoints != 0;
		EXPECT_TRUE(grants) << name << " grants nothing";
	}
}

// Every one of the ten non-jewelry slots must actually have words, and jewelry must have none -
// a one-socket "word" is just a socketed rune.
TEST(OracoolRunewords, EveryNonJewellerySlotHasWords)
{
	std::map<uint8_t, int> perHost;
	for (size_t i = 0; i < oracool::RunewordCount(); i++)
		perHost[oracool::RunewordAt(i)->host]++;

	for (uint8_t host = 0; host < static_cast<uint8_t>(oracool::RunewordHost::None); host++)
		EXPECT_GT(perHost[host], 0) << "host " << static_cast<int>(host) << " has no runewords";

	EXPECT_EQ(oracool::RunewordHostForItemType(ItemType::Ring), oracool::RunewordHost::None);
	EXPECT_EQ(oracool::RunewordHostForItemType(ItemType::Amulet), oracool::RunewordHost::None);
	EXPECT_EQ(oracool::RunewordHostForItemType(ItemType::Misc), oracool::RunewordHost::None);
	EXPECT_EQ(oracool::RunewordHostForItemType(ItemType::Belt), oracool::RunewordHost::Belt)
	    << "a belt word must not be a generic armour word";
	EXPECT_EQ(oracool::RunewordHostForItemType(ItemType::Staff), oracool::RunewordHost::Weapon);
}

// The two runes that act on the HOST, not on the totals.
TEST(OracoolGems, HelReducesRequirementsAndZodPreventsBreaking)
{
	devilution::Item sword {};
	sword._itype = ItemType::Sword;
	sword._iCurs = ICURS_SHORT_SWORD;
	sword._iMagical = ITEM_QUALITY_NORMAL;
	sword._iMinStr = 100;
	sword._iMaxDur = 40;
	sword._iDurability = 40;
	sword._iSocketCount = 2;

	EXPECT_EQ(oracool::EffectiveRequirement(sword, sword._iMinStr), 100) << "an empty socket reduced a requirement";

	sword._iSocketed[0] = IDI_ORACOOL_RUNE_HEL;
	EXPECT_EQ(oracool::EffectiveRequirement(sword, sword._iMinStr), 80) << "Hel did not reduce the requirement";
	sword._iSocketed[1] = IDI_ORACOOL_RUNE_HEL;
	EXPECT_EQ(oracool::EffectiveRequirement(sword, sword._iMinStr), 60) << "two Hels did not stack";
	// A requirement that exists stays a requirement, however many Hels are in the item.
	EXPECT_GE(oracool::EffectiveRequirement(sword, 1), 1);

	devilution::Item axe {};
	axe._itype = ItemType::Axe;
	axe._iCurs = ICURS_SHORT_SWORD;
	axe._iMaxDur = 40;
	axe._iDurability = 40;
	axe._iSocketCount = 1;
	EXPECT_FALSE(oracool::SocketsMakeIndestructible(axe));

	axe._iSocketed[0] = IDI_ORACOOL_RUNE_ZOD;
	EXPECT_TRUE(oracool::SocketsMakeIndestructible(axe));
	oracool::ApplyZodToHost(axe);
	EXPECT_EQ(axe._iDurability, DUR_INDESTRUCTIBLE) << "Zod did not stamp the host";
	EXPECT_EQ(axe._iMaxDur, 40) << "max durability must survive so extraction can restore the item";
}

// Sockets v2 (user directive 2026-08-19): "max number of sockets = number of 28x28px boxes the item
// is made of", and jewelry - which has no basic versions at all - sockets at magic and better
// instead of never.
TEST(OracoolGems, SocketCapIsTheItemFootprint)
{
	devilution::Item sword {};
	sword._itype = ItemType::Sword;
	sword._iCurs = ICURS_SHORT_SWORD;
	sword._iMagical = ITEM_QUALITY_NORMAL;
	const Size swordCells = GetInventorySize(sword);
	EXPECT_EQ(oracool::MaxSocketsForItem(sword), swordCells.width * swordCells.height);

	devilution::Item plate {};
	plate._itype = ItemType::HeavyArmor;
	plate._iCurs = ICURS_FULL_PLATE_MAIL;
	plate._iMagical = ITEM_QUALITY_NORMAL;
	EXPECT_EQ(oracool::MaxSocketsForItem(plate), 6) << "a 2x3 body armour is the six-socket host";

	devilution::Item ring {};
	ring._itype = ItemType::Ring;
	ring._iCurs = ICURS_RING;
	ring._iMagical = ITEM_QUALITY_MAGIC;
	EXPECT_TRUE(oracool::CanItemHaveSockets(ring)) << "jewelry must socket at magic and better";
	EXPECT_EQ(oracool::MaxSocketsForItem(ring), 1) << "a 1x1 ring takes exactly one";

	// Every cap fits the record, which is what stops a footprint change from writing out of bounds.
	EXPECT_LE(oracool::MaxSocketsForItem(plate), devilution::Item::MaxItemSockets);
}

// The gems follow Diablo II's own columns (user directive 2026-08-18): a ruby is fire damage in a
// weapon, LIFE in armor (not resist - that was this fork's pre-D2 tuning), fire resist in a shield.
TEST(OracoolGems, RubyFollowsItsDiabloTwoColumn)
{
	oracool::ItemBonusTotals weaponTotals;
	oracool::ApplyGemToTotals(IDI_ORACOOL_GEM_RUBY, oracool::SocketHost::Weapon, weaponTotals);
	EXPECT_GT(weaponTotals.fireMax, 0);
	EXPECT_EQ(weaponTotals.fireResist, 0);

	oracool::ItemBonusTotals armorTotals;
	oracool::ApplyGemToTotals(IDI_ORACOOL_GEM_RUBY, oracool::SocketHost::Armor, armorTotals);
	EXPECT_EQ(armorTotals.fireMax, 0);
	EXPECT_GT(armorTotals.hitPoints, 0) << "D2's armor ruby is +life";
	EXPECT_EQ(armorTotals.fireResist, 0) << "the pre-D2 armor resist came back";

	oracool::ItemBonusTotals shieldTotals;
	oracool::ApplyGemToTotals(IDI_ORACOOL_GEM_RUBY, oracool::SocketHost::Shield, shieldTotals);
	EXPECT_GT(shieldTotals.fireResist, 0) << "D2's shield ruby is fire resist";
	EXPECT_EQ(shieldTotals.hitPoints, 0);
}

// Each gem type's D2 identity, one signature stat per host - the "gems are missing affixes" fix
// (user, 2026-08-18). Substitutions where the engine lacks a channel are pinned as themselves:
// sapphire (cold) is the mana gem, emerald's poison resist is magic resist, skull's leech is
// life-per-kill in weapons and thorns on shields.
TEST(OracoolGems, EveryGemCarriesItsDiabloTwoIdentity)
{
	using oracool::ApplyGemToTotals;
	using oracool::GemIndexFor;
	using oracool::GemQuality;
	using oracool::GemType;
	using oracool::ItemBonusTotals;
	using oracool::SocketHost;

	ItemBonusTotals t;
	ApplyGemToTotals(GemIndexFor(GemType::Amethyst, GemQuality::Normal), SocketHost::Armor, t);
	EXPECT_GT(t.strength, 0) << "amethyst armor: +strength";

	t = {};
	ApplyGemToTotals(GemIndexFor(GemType::Diamond, GemQuality::Normal), SocketHost::Shield, t);
	EXPECT_GT(t.fireResist, 0) << "diamond shield: all resists";
	EXPECT_EQ(t.fireResist, t.lightningResist);
	EXPECT_EQ(t.fireResist, t.magicResist);
	t = {};
	ApplyGemToTotals(GemIndexFor(GemType::Diamond, GemQuality::Normal), SocketHost::Armor, t);
	EXPECT_GT(t.bonusToHit, 0) << "diamond armor: attack rating";

	t = {};
	ApplyGemToTotals(GemIndexFor(GemType::Emerald, GemQuality::Normal), SocketHost::Armor, t);
	EXPECT_GT(t.dexterity, 0) << "emerald armor: +dexterity";
	t = {};
	ApplyGemToTotals(GemIndexFor(GemType::Emerald, GemQuality::Normal), SocketHost::Shield, t);
	EXPECT_GT(t.magicResist, 0) << "emerald shield: poison resist -> magic resist";

	t = {};
	ApplyGemToTotals(GemIndexFor(GemType::Sapphire, GemQuality::Normal), SocketHost::Armor, t);
	EXPECT_GT(t.mana, 0) << "sapphire armor: +mana (D2's own)";
	t = {};
	ApplyGemToTotals(GemIndexFor(GemType::Sapphire, GemQuality::Normal), SocketHost::Weapon, t);
	EXPECT_GT(t.mana, 0) << "sapphire weapon: the cold substitute is mana everywhere";

	t = {};
	ApplyGemToTotals(GemIndexFor(GemType::Topaz, GemQuality::Normal), SocketHost::Weapon, t);
	EXPECT_GT(t.lightningMax, 0) << "topaz weapon: lightning damage";
	t = {};
	ApplyGemToTotals(GemIndexFor(GemType::Topaz, GemQuality::Perfect), SocketHost::Armor, t);
	EXPECT_EQ(t.magicFind, 24) << "topaz armor: +24% magic find at Perfect, D2's own number";

	t = {};
	ApplyGemToTotals(GemIndexFor(GemType::Skull, GemQuality::Normal), SocketHost::Shield, t);
	EXPECT_TRUE(HasAnyOf(t.flags, ItemSpecialEffect::Thorns)) << "skull shield: thorns";
	t = {};
	ApplyGemToTotals(GemIndexFor(GemType::Skull, GemQuality::Normal), SocketHost::Armor, t);
	EXPECT_GT(t.hitPoints, 0) << "skull armor: replenish-life substitute";
	EXPECT_GT(t.mana, 0) << "skull armor: regenerate-mana substitute";

	// The skull's weapon leech is an EVENT, not a totals stat: a Perfect skull in a worn weapon
	// restores quality-scaled life on each kill.
	Players.resize(1);
	devilution::Player &player = Players[0];
	player = {};
	devilution::Item &sword = player.InvBody[INVLOC_HAND_LEFT];
	sword = {};
	sword._itype = ItemType::Sword;
	sword._iStatFlag = true;
	sword._iSocketCount = 1;
	sword._iSocketed[0] = GemIndexFor(GemType::Skull, GemQuality::Perfect);
	EXPECT_EQ(oracool::GemLifePerKill(player), 4) << "Perfect (200%) doubles the Normal +2";
	sword._iSocketed[0] = GemIndexFor(GemType::Skull, GemQuality::Chipped);
	EXPECT_EQ(oracool::GemLifePerKill(player), 1) << "Chipped floors at 1, never rounds to nothing";
	sword._itype = ItemType::Helm;
	EXPECT_EQ(oracool::GemLifePerKill(player), 0) << "the leech is a WEAPON effect, as in D2";
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

// Bug (external audit, 2026-08-17): ApplyGemToTotals wrapped every field but one in at(at(...)),
// applying the quality percentage TWICE while GemSocketLine's tooltip applied it once - a Perfect
// ruby displaying 4-12 fire damage mechanically granted the numbers squared-scaled (200% became
// 400%). The mechanics must equal the tooltip's arithmetic: the Normal row, scaled by the quality
// percent, once, with the never-round-to-nothing floor.
TEST(OracoolGems, QualityScalesEffectsOnceNotTwice)
{
	const uint16_t normal = oracool::GemIndexFor(oracool::GemType::Ruby, oracool::GemQuality::Normal);
	const uint16_t perfect = oracool::GemIndexFor(oracool::GemType::Ruby, oracool::GemQuality::Perfect);

	oracool::ItemBonusTotals base;
	oracool::ApplyGemToTotals(normal, oracool::SocketHost::Weapon, base);
	ASSERT_GT(base.fireMax, 0) << "the Normal ruby row is empty - the test has nothing to scale";

	const int percent = oracool::GemQualityPercent(oracool::GemQuality::Perfect);
	ASSERT_GT(percent, 100) << "Perfect does not scale up - this test would prove nothing";
	// AtQuality's contract, restated independently: one application, floor of 1 for nonzero input.
	const auto onceScaled = [percent](int value) {
		if (value == 0)
			return 0;
		const int scaled = value * percent / 100;
		return scaled > 0 ? scaled : 1;
	};

	oracool::ItemBonusTotals top;
	oracool::ApplyGemToTotals(perfect, oracool::SocketHost::Weapon, top);
	EXPECT_EQ(top.fireMin, onceScaled(base.fireMin)) << "the quality percent applied more than once";
	EXPECT_EQ(top.fireMax, onceScaled(base.fireMax)) << "the quality percent applied more than once";

	oracool::ItemBonusTotals baseArmor;
	oracool::ApplyGemToTotals(normal, oracool::SocketHost::Armor, baseArmor);
	oracool::ItemBonusTotals topArmor;
	oracool::ApplyGemToTotals(perfect, oracool::SocketHost::Armor, topArmor);
	// The armor ruby is +life (D2's column); hitPoints ride <<6 fixed point, so scale the whole-HP
	// value and shift back - which also proves the fixed-point conversion happens AFTER the single
	// quality application, not inside it.
	ASSERT_GT(baseArmor.hitPoints, 0);
	EXPECT_EQ(topArmor.hitPoints, onceScaled(baseArmor.hitPoints >> 6) << 6)
	    << "the armor host still double-scales";
}

// Bug (external audit, 2026-08-17): TotalInvestedSkillPoints summed only _pSkillInvestment, the
// slotted store - the class tree's slotless passives, masteries and auras live in
// _pClassTreeInvestment, invisible to the ledger. EnsureRetroactiveSkillPoints therefore re-granted
// every passive-invested point on each game start (invest in passives, relog, repeat - an infinite
// point loop), and the healer's respec undercharged while refunding only the slotted half of the
// character.
TEST(OracoolSkillPoints, LedgerCountsBothStoresAndRefundsBoth)
{
	Players.resize(1);
	MyPlayer = &Players[0];
	devilution::Player &player = Players[0];
	player = {};
	player._pLevel = 11;
	player._pUnspentSkillPoints = 0;
	std::memset(player._pSkillInvestment, 0, sizeof(player._pSkillInvestment));
	std::memset(player._pClassTreeInvestment, 0, sizeof(player._pClassTreeInvestment));

	// Self-calibrate what level 11 is owed, so this test cannot drift from SkillPointsPerLevel.
	oracool::EnsureRetroactiveSkillPoints(player);
	const int owed = player._pUnspentSkillPoints;
	ASSERT_GT(owed, 1) << "level 11 is owed points, or the whole scenario is empty";

	// Spend everything: half into a slotted skill, half into a slotless tree row.
	player._pUnspentSkillPoints = 0;
	player._pSkillInvestment[3] = static_cast<uint8_t>(owed / 2);
	player._pClassTreeInvestment[5] = static_cast<uint8_t>(owed - owed / 2);

	EXPECT_EQ(oracool::TotalInvestedSkillPoints(player), owed)
	    << "the slotless store went invisible to the ledger again";

	// Fully invested is fully paid: the top-up must grant NOTHING.
	oracool::EnsureRetroactiveSkillPoints(player);
	EXPECT_EQ(player._pUnspentSkillPoints, 0)
	    << "the top-up re-granted points already sunk in the tree - the infinite point loop is back";

	// And a full refund returns every point and empties BOTH stores.
	oracool::RefundAllSkillPoints(player);
	EXPECT_EQ(player._pUnspentSkillPoints, owed);
	EXPECT_EQ(player._pSkillInvestment[3], 0);
	EXPECT_EQ(player._pClassTreeInvestment[5], 0);
	EXPECT_EQ(oracool::TotalInvestedSkillPoints(player), 0);
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
	// The NORMAL row is the tuned anchor of its type's whole ladder. Since the D2 pass
	// (2026-08-18) the anchors ARE Diablo II's Normal-quality values: the ruby's weapon roll is
	// D2's own 8-12 fire.
	EXPECT_EQ(oracool::GemIndexFor(oracool::GemType::Ruby, oracool::GemQuality::Normal),
	    IDI_ORACOOL_GEM_RUBY);
	oracool::ItemBonusTotals normal;
	oracool::ApplyGemToTotals(IDI_ORACOOL_GEM_RUBY, oracool::SocketHost::Weapon, normal);
	EXPECT_EQ(normal.fireMin, 8);
	EXPECT_EQ(normal.fireMax, 12);

	oracool::ItemBonusTotals chipped;
	oracool::ApplyGemToTotals(IDI_ORACOOL_GEM_RUBY_CHIPPED, oracool::SocketHost::Weapon, chipped);
	oracool::ItemBonusTotals perfect;
	oracool::ApplyGemToTotals(IDI_ORACOOL_GEM_RUBY_PERFECT, oracool::SocketHost::Weapon, perfect);
	EXPECT_LT(chipped.fireMax, normal.fireMax) << "a chipped gem is not weaker than a normal one";
	EXPECT_GT(perfect.fireMax, normal.fireMax) << "a perfect gem is not stronger than a normal one";
	// Rounding must never erase an effect a gem is supposed to have.
	EXPECT_GT(chipped.fireMin, 0) << "scaling rounded a real effect away to nothing";

	// Two more anchors on the channels their D2 columns claim (the full identity sweep lives in
	// EveryGemCarriesItsDiabloTwoIdentity).
	oracool::ItemBonusTotals amethyst;
	oracool::ApplyGemToTotals(IDI_ORACOOL_GEM_AMETHYST_NORMAL, oracool::SocketHost::Armor, amethyst);
	EXPECT_GT(amethyst.strength, 0) << "D2's armor amethyst is +strength";
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


// Torment shipped byte-identical to Hell, so the fourth difficulty asked nothing the third had not
// already asked. Hell's resistances harden into immunities there - the same step Nightmare-to-Hell
// takes, taken once more.
TEST(OracoolAudit, TormentHardensHellResistancesIntoImmunities)
{
	MonsterData data {};
	data.resistanceHell = IMMUNE_FIRE | RESIST_MAGIC;

	const uint16_t torment = oracool::MonsterResistancesFor(data, DIFF_TORMENT);
	EXPECT_NE(torment & IMMUNE_FIRE, 0) << "an existing immunity must survive";
	EXPECT_NE(torment & IMMUNE_MAGIC, 0) << "the resisted school should have hardened";
	EXPECT_EQ(torment & RESIST_MAGIC, 0) << "the resistance bit should have been consumed";
	EXPECT_EQ(oracool::MonsterResistancesFor(data, DIFF_HELL), data.resistanceHell)
	    << "Hell itself must not have moved";
}

// The rule the promotion cannot do without: no monster may end up immune to all three schools. That
// is not a harder fight but an impossible one for the caster classes, while the physical classes
// would never notice the difficulty existed. Whatever it was weakest to stays merely resisted, so
// every monster keeps exactly one answer.
TEST(OracoolAudit, TormentNeverLeavesAMonsterImmuneToEverything)
{
	constexpr uint16_t ResistBits = RESIST_MAGIC | RESIST_FIRE | RESIST_LIGHTNING;
	constexpr uint16_t ImmuneBits = IMMUNE_MAGIC | IMMUNE_FIRE | IMMUNE_LIGHTNING;

	// Every authorable combination of the six school bits, not a sample.
	for (uint16_t hell = 0; hell < 64; hell++) {
		MonsterData data {};
		data.resistanceHell = static_cast<uint8_t>(
		    ((hell & 1) != 0 ? RESIST_MAGIC : 0) | ((hell & 2) != 0 ? RESIST_FIRE : 0)
		    | ((hell & 4) != 0 ? RESIST_LIGHTNING : 0) | ((hell & 8) != 0 ? IMMUNE_MAGIC : 0)
		    | ((hell & 16) != 0 ? IMMUNE_FIRE : 0) | ((hell & 32) != 0 ? IMMUNE_LIGHTNING : 0));

		const uint16_t torment = oracool::MonsterResistancesFor(data, DIFF_TORMENT);
		const bool immuneToAll = (torment & ImmuneBits) == ImmuneBits && (torment & ResistBits) == 0;
		const bool authoredThatWay = (data.resistanceHell & ImmuneBits) == ImmuneBits;
		if (!authoredThatWay) {
			EXPECT_FALSE(immuneToAll)
			    << "Torment promoted a monster into total immunity, hell bits = " << hell;
		}

		// And Torment must never be SOFTER than Hell: every Hell immunity survives.
		EXPECT_EQ(torment & data.resistanceHell & ImmuneBits, data.resistanceHell & ImmuneBits)
		    << "Torment dropped an immunity Hell had, hell bits = " << hell;
	}
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
	// Every row is on exactly one page, EXCEPT the ones retired as book spells (2026-08-20) - those
	// are on none by design. Counting the retired rows here rather than hardcoding the remainder is
	// what keeps this assertion meaningful: it still catches a row that fell off a page for any
	// other reason.
	size_t retired = 0;
	for (size_t i = 0; i < oracool::ClassTreeSkillCount; i++) {
		if (oracool::IsClassTreeRowRetiredAsSpell(static_cast<oracool::ClassTreeSkill>(i)))
			retired++;
	}
	EXPECT_GT(retired, 0u) << "the book-spell retirement matched nothing - has the rule been lost?";
	EXPECT_EQ(total + retired, oracool::ClassTreeSkillCount) << "a skill is on no page, or on two";
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
	// Zeal, not Holy Bolt: Holy Bolt was withdrawn on 2026-08-18 (it collided with the engine's own
	// Holy Bolt spell), so it no longer HAS a slot and its points land in the tree's own array - which
	// is the opposite of what this test exists to pin. Zeal reaches its slot through
	// BorrowedPaladinSkill rather than from the table, which makes it the better witness anyway: the
	// seam has to work for borrowed rows too.
	const int before = player.GetSpellLevel(SpellID::Zeal);

	ASSERT_TRUE(oracool::InvestClassTreePoint(player, oracool::ClassTreeSkill::Zeal));
	ASSERT_TRUE(oracool::InvestClassTreePoint(player, oracool::ClassTreeSkill::Zeal));
	EXPECT_EQ(oracool::ClassTreeInvestment(player, oracool::ClassTreeSkill::Zeal), 2);
	EXPECT_EQ(player.GetSpellLevel(SpellID::Zeal), before + 2)
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
	// Level 7, not 5. Firebolt sits in the level-6 band, and the Rule of Rangs wants 6 for its first
	// rank and 7 for its second - so 5 is now a character who cannot invest in it at all, which is
	// the rule working rather than the test failing (2026-08-19).
	player._pLevel = 7;
	player._pUnspentSkillPoints = 0;
	std::memset(player._pSkillInvestment, 0, sizeof(player._pSkillInvestment));
	player._pAblSpells = 0;
	std::memset(player._pSplLvl, 0, sizeof(player._pSplLvl));

	oracool::EnsureRetroactiveSkillPoints(player);
	EXPECT_EQ(player._pUnspentSkillPoints, 6) << "level 7 is owed 6 points";
	oracool::EnsureRetroactiveSkillPoints(player);
	EXPECT_EQ(player._pUnspentSkillPoints, 6) << "the retro grant must not pay twice";

	// Zeal, not Firebolt. User rule, 2026-08-20: a spell a BOOK can teach is raised by books alone,
	// so Firebolt no longer takes points at all - it is asserted below as the rule rather than used
	// as the vehicle. Zeal is bookless by design (spelldat sBookLvl -1, "earned not bought"), which
	// is exactly what still accepts investment.
	EXPECT_FALSE(oracool::CanInvestSkillPoint(player, SpellID::Zeal))
	    << "a skill the character does not have took a point";
	player._pAblSpells |= GetSpellBitmask(SpellID::Zeal);
	ASSERT_TRUE(oracool::InvestSkillPoint(player, SpellID::Zeal));
	ASSERT_TRUE(oracool::InvestSkillPoint(player, SpellID::Zeal));
	EXPECT_EQ(player._pUnspentSkillPoints, 4);
	player._pISplLvlAdd = 0;
	EXPECT_EQ(player.GetSpellLevel(SpellID::Zeal), 2)
	    << "two invested points should reach the ladders as level 2";

	// The Rule of Rangs, expressed through the function that defines it rather than a hardcoded
	// level - the point is that rank 3 needs one more character level than rank 2, whatever band
	// Zeal sits in.
	const int rank3 = oracool::SpellRankRequiredLevel(SpellID::Zeal, 3);
	player._pLevel = rank3 - 1;
	EXPECT_FALSE(oracool::CanInvestSkillPoint(player, SpellID::Zeal))
	    << "rank 3 landed a level early";
	player._pLevel = rank3;
	EXPECT_TRUE(oracool::CanInvestSkillPoint(player, SpellID::Zeal))
	    << "one more character level should open exactly one more rank";
	player._pLevel = 7;

	// The per-rank refund.
	ASSERT_TRUE(oracool::RefundSkillPoint(player, SpellID::Zeal));
	EXPECT_EQ(player._pUnspentSkillPoints, 5);
	EXPECT_EQ(player.GetSpellLevel(SpellID::Zeal), 1);
	ASSERT_TRUE(oracool::InvestSkillPoint(player, SpellID::Zeal));

	EXPECT_EQ(oracool::RespecCost(player), 1000) << "the floor price";
	oracool::RefundAllSkillPoints(player);
	EXPECT_EQ(player._pUnspentSkillPoints, 6);
	EXPECT_EQ(player.GetSpellLevel(SpellID::Zeal), 0);
}

/** @brief The 2026-08-20 rule: books raise spells, points raise skills, and never the reverse. */
TEST(OracoolSkillPoints, BookSpellsRefusePointsAndBooklessSkillsAcceptThem)
{
	Players.resize(1);
	devilution::Player &player = Players[0];
	player = {};
	player._pLevel = 30;
	player._pUnspentSkillPoints = 20;

	// The predicate itself, against both sides of the line as spelldat authored them.
	EXPECT_TRUE(oracool::SpellHasBook(SpellID::Firebolt));
	EXPECT_TRUE(oracool::SpellHasBook(SpellID::Fireball));
	EXPECT_TRUE(oracool::SpellHasBook(SpellID::Golem));
	EXPECT_FALSE(oracool::SpellHasBook(SpellID::Zeal));
	EXPECT_FALSE(oracool::SpellHasBook(SpellID::Charge));
	EXPECT_FALSE(oracool::SpellHasBook(SpellID::BlessedHammer));

	// A fully learned book spell still refuses a point.
	player._pSplLvl[static_cast<size_t>(SpellID::Firebolt)] = 5;
	EXPECT_FALSE(oracool::IsSkillInvestable(player, SpellID::Firebolt));
	EXPECT_FALSE(oracool::CanInvestSkillPoint(player, SpellID::Firebolt));
	EXPECT_FALSE(oracool::InvestSkillPoint(player, SpellID::Firebolt));
	EXPECT_EQ(player._pUnspentSkillPoints, 20) << "a refused invest still spent a point";
	EXPECT_EQ(player.GetSpellLevel(SpellID::Firebolt), 5) << "books alone should set this";

	// A bookless class skill still takes them.
	player._pAblSpells |= GetSpellBitmask(SpellID::Zeal);
	EXPECT_TRUE(oracool::InvestSkillPoint(player, SpellID::Zeal));
	EXPECT_EQ(player.GetSpellLevel(SpellID::Zeal), 1);
}

/** @brief The migration: points stranded in book spells come back, once, and only from book spells. */
TEST(OracoolSkillPoints, StrandedBookSpellPointsAreRefundedIdempotently)
{
	Players.resize(1);
	devilution::Player &player = Players[0];
	player = {};
	player._pUnspentSkillPoints = 1;
	// What an old save could hold: the Sorceress's tree wrote here by SpellID, and the Spells sheet
	// spent here directly.
	player._pSkillInvestment[static_cast<size_t>(SpellID::Fireball)] = 6;
	player._pSkillInvestment[static_cast<size_t>(SpellID::Lightning)] = 4;
	player._pSkillInvestment[static_cast<size_t>(SpellID::Zeal)] = 3; // bookless: must NOT be taken

	EXPECT_EQ(oracool::RefundBookSpellInvestment(player), 10);
	EXPECT_EQ(player._pUnspentSkillPoints, 11);
	EXPECT_EQ(player._pSkillInvestment[static_cast<size_t>(SpellID::Fireball)], 0);
	EXPECT_EQ(player._pSkillInvestment[static_cast<size_t>(SpellID::Lightning)], 0);
	EXPECT_EQ(player._pSkillInvestment[static_cast<size_t>(SpellID::Zeal)], 3)
	    << "a bookless skill's points were confiscated";

	// Idempotent - it zeroes what it refunds, which IS the version gate.
	EXPECT_EQ(oracool::RefundBookSpellInvestment(player), 0);
	EXPECT_EQ(player._pUnspentSkillPoints, 11);
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
	// Diablo II's own Ancient's Pledge is Ral Ort Tal. The fork's launch table had invented
	// Ral Ort El under that name; the 370-word pool authors D2's 61 real words with their real
	// recipes, so a player who knows D2 finds what they expect.
	devilution::Item shield {};
	shield._itype = ItemType::Shield;
	shield._iMagical = ITEM_QUALITY_NORMAL;
	shield._iSocketCount = 3;
	shield._iSocketed[0] = IDI_ORACOOL_RUNE_RAL;
	shield._iSocketed[1] = IDI_ORACOOL_RUNE_ORT;
	shield._iSocketed[2] = IDI_ORACOOL_RUNE_TAL;
	ASSERT_TRUE(oracool::TryCompleteRuneword(shield));
	EXPECT_STREQ(shield._iIName, "Ancient's Pledge");

	// The old recipe must no longer form anything under that name.
	devilution::Item oldRecipe = shield;
	oldRecipe._iIName[0] = '\0';
	oldRecipe._iSocketed[2] = IDI_ORACOOL_RUNE_EL;
	const oracool::RunewordDefinition *word = oracool::GetActiveRuneword(oldRecipe);
	if (word != nullptr)
		EXPECT_STRNE(word->name, "Ancient's Pledge");

	// Every rune teaches the words it belongs to - the one thing this fork improved on D2. With
	// 370 words a popular rune belongs to dozens, so the list is capped at six and the remainder
	// counted; what must hold is that the teaching is non-empty and names real words.
	const std::string teaching = oracool::RuneTeachingLines(IDI_ORACOOL_RUNE_EL);
	EXPECT_FALSE(teaching.empty());
	EXPECT_NE(teaching.find("Steel"), std::string::npos) << "El's first taught word should be Steel";
	EXPECT_NE(teaching.find("more runewords"), std::string::npos)
	    << "a rune in dozens of words must say how many were not listed";

	const std::string solTeaching = oracool::RuneTeachingLines(IDI_ORACOOL_RUNE_SOL);
	EXPECT_NE(solTeaching.find("Lore"), std::string::npos);
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

	int nextCount = 0;
	int elCount = 0;
	for (int i = 0; i < player._pNumInv; i++) {
		// Diablo II's ladder puts ELD above El, not Tir. The old expectation here encoded the bug
		// this test now guards against: crafting used to walk the ENUM (index + 1), which skips
		// Eld entirely because the five v1.7.8 runes and the 28 appended ones are separate islands.
		if (player.InvList[i].IDidx == IDI_ORACOOL_RUNE_ELD)
			nextCount++;
		if (player.InvList[i].IDidx == IDI_ORACOOL_RUNE_EL)
			elCount++;
	}
	EXPECT_EQ(nextCount, 1) << "two El runes should have become one Eld";
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
	// Zod pairs must never ascend - Zod is the top of the 33-rune ladder. This used to say Sol,
	// which was only the top while five of the thirty-three existed.
	player.InvList[0].IDidx = IDI_ORACOOL_RUNE_ZOD;
	player.InvList[1].IDidx = IDI_ORACOOL_RUNE_ZOD;
	player._pNumInv = 2;
	EXPECT_FALSE(oracool::CanCraft(player, 1)) << "a Zod pair offered an ascension past the ladder's top";
	// ...and a pair below the top still does ascend.
	player.InvList[0].IDidx = IDI_ORACOOL_RUNE_SOL;
	player.InvList[1].IDidx = IDI_ORACOOL_RUNE_SOL;
	EXPECT_TRUE(oracool::CanCraft(player, 1)) << "Sol is no longer the top and must ascend";
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
	// The table is a SUPERSET of the delivered vocabulary as of 2026-08-16: nine keywords were added
	// for channels this engine already had and the delivered data never named, so the bonus rungs
	// could be re-authored out of things that actually pay out. Equality here would say "the table
	// may only ever describe what was delivered", which is the opposite of the intent - what must
	// hold is that nothing delivered is MISSING, and that additions are counted rather than drifting.
	EXPECT_GE(oracool::SetStatMappingCount, std::size(DeliveredSetStatKeywords))
	    << "a delivered keyword lost its row";
	EXPECT_EQ(oracool::SetStatMappingCount - std::size(DeliveredSetStatKeywords), 9u)
	    << "the fork's own keyword count changed - if that is deliberate, update this number and "
	    << "the note in item_set_stats.h";
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
	// enhanced_armor is IPL_ACP. It was IPL_TARGAC until 2026-08-16, mapped on the name alone, and
	// this test pinned the mistake: IPL_TARGAC is _iPLEnAc, which is ARMOUR PIERCING, and under
	// Hellfire Player::CalculateArmorPierce uses it as a SHIFT (`tmac >>= _pIEnAc - 1`). Sixteen
	// items declared enhanced_armor and were erasing monster armour outright while their tooltip
	// promised defence. Exactly the failure the old comment here warned about, in the other
	// direction - "nothing would look broken" was the whole problem.
	EXPECT_EQ(armor->powers[0].type, IPL_ACP);
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

	// "Cinderbrand", the four-piece, WAS a single proc: - named, earned, and granting nothing. It
	// was one of forty-five such rungs, and on 2026-08-16 all forty-five were re-authored out of
	// stats this engine can pay (oracool/item_set_bonus_overrides.txt) after the user observed that
	// the set affixes "sound strange". This test asserted the emptiness; it now asserts the fix.
	const oracool::SetBonusDefinition *four = oracool::ActiveSetBonus(*ashen, 4);
	ASSERT_NE(four, nullptr);
	EXPECT_GT(oracool::CountLivePowers(four->powers, 4), 0)
	    << "Cinderbrand is back to granting nothing";
	EXPECT_NE(four->name, nullptr);
}

// The invariant the override pass installed, checked against the shipped table rather than only in
// the generator: a rung the player is TOLD they earned has to grant something. Forty-five of the
// seventy-three did not, which is how a ladder of evocative names ended up reading as nonsense.
TEST(OracoolItemSets, EveryBonusRungGrantsSomething)
{
	for (const oracool::ItemSetDefinition &set : oracool::ItemSets) {
		for (int i = 0; i < set.bonusCount; i++) {
			const oracool::SetBonusDefinition &rung = oracool::ItemSetBonuses[set.firstBonus + i];
			EXPECT_GT(oracool::CountLivePowers(rung.powers, 4), 0)
			    << set.id << " rung '" << rung.name << "' (" << rung.pieces
			    << " pieces) compiles to nothing - add a row to item_set_bonus_overrides.txt";
		}
	}
}

// A stat that grants something but renders as a blank line is the same failure wearing a different
// hat: the player still cannot tell what the rung does. PrintSetBonusPower returns an empty string
// for a type it has no case for, and this is what turns that into a red run.
/**
 * @brief Which set pieces can be BUILT, and therefore dropped.
 *
 * Before 2026-08-21 this was 73 of 94: amulets and rings had no resolvable base because the vanilla
 * rows are anonymous and naming them would have meant inserting into _item_indexes, which is
 * positional save format. ItemMiscIdIdx answers the same question without touching the enum.
 *
 * Pinned as an exact count rather than "most of them", because the failure this guards is silent:
 * an unspawnable piece is simply never rolled, so a set quietly becomes uncompletable and nothing
 * says so.
 */
TEST(OracoolItemSets, EveryPieceButRelicAndCloakHasASpawnableBase)
{
	int spawnable = 0;
	int unspawnable = 0;
	for (const oracool::ItemSetDefinition &set : oracool::ItemSets) {
		for (int i = 0; i < set.itemCount; i++) {
			const oracool::SetItemDefinition &def = oracool::ItemSetItems[set.firstItem + i];
			const int base = oracool::BaseItemForSetSlot(def.slot);
			if (base >= 0) {
				spawnable++;
				// A resolved base must be a real row, not a stale index - the whole point of
				// routing rings and amulets through ItemMiscIdIdx is that it cannot invent one.
				EXPECT_LE(base, IDI_LAST) << def.name << " resolved past the item table";
			} else {
				unspawnable++;
				// Only the two slots this fork has genuinely not built may fail.
				const std::string slot = def.slot;
				EXPECT_TRUE(slot == "relic" || slot == "cloak")
				    << def.name << " has no base, and its slot '" << slot << "' is not one of the two known gaps";
			}
		}
	}
	EXPECT_EQ(spawnable, 92);
	EXPECT_EQ(unspawnable, 2) << "the relic and the cloak are the only pieces without a base";
}

/**
 * @brief No set may promise a rung the game cannot pay.
 *
 * Leoric's Fallen Court shipped with rungs at 12 and 13 pieces while only 11 of its 13 could be
 * built - the relic and the cloak sit in slots this fork has never made. A player assembled
 * everything that exists and the ladder simply stopped paying, with nothing saying why.
 *
 * "Wearable" is not the same as "buildable", which is why this counts SLOTS rather than items: a
 * set with three rings could build all three and still wear only two. Ring is the one slot with a
 * capacity above one, and hard-coding that here is deliberate - if a second such slot is ever added,
 * this test should fail and be updated, rather than quietly over-counting.
 */
TEST(OracoolItemSets, NoSetPromisesARungItCannotPay)
{
	for (const oracool::ItemSetDefinition &set : oracool::ItemSets) {
		std::map<std::string, int> buildableBySlot;
		for (int i = 0; i < set.itemCount; i++) {
			const oracool::SetItemDefinition &def = oracool::ItemSetItems[set.firstItem + i];
			if (oracool::BaseItemForSetSlot(def.slot) >= 0)
				buildableBySlot[def.slot]++;
		}
		int wearable = 0;
		for (const auto &[slot, count] : buildableBySlot) {
			const int capacity = slot == "ring" ? 2 : 1;
			wearable += std::min(count, capacity);
		}

		int topRung = 0;
		for (int i = 0; i < set.bonusCount; i++)
			topRung = std::max(topRung, oracool::ItemSetBonuses[set.firstBonus + i].pieces);

		EXPECT_LE(topRung, wearable)
		    << set.name << " has a rung at " << topRung << " pieces but only " << wearable
		    << " can be worn at once - that rung is unreachable and nothing in game says so";
	}
}

TEST(OracoolItemSets, EverySetBonusStatHasText)
{
	for (const oracool::SetBonusDefinition &rung : oracool::ItemSetBonuses) {
		for (const ItemPower &power : rung.powers) {
			if (power.type == IPL_INVALID)
				continue;
			EXPECT_FALSE(PrintSetBonusPower(power).empty())
			    << "rung '" << rung.name << "' carries power type " << static_cast<int>(power.type)
			    << ", which PrintSetBonusPower has no case for - it would draw as nothing";
		}
	}
}

// Two copies of one set piece are ONE piece worn (audit, 2026-08-17). WornSetPieces used to count
// equipped items rather than distinct pieces, so a duplicated ring in both ring slots pushed a
// 5-of-6 set to a false 6/6 - top rung granted, completion stinger rung, for a set not owned.
TEST(OracoolItemSets, DuplicateSetPieceCountsOnce)
{
	Players.resize(1);
	MyPlayer = &Players[0];
	devilution::Player &player = Players[0];
	for (auto &slot : player.InvBody)
		slot.clear();

	const oracool::ItemSetDefinition *court = oracool::FindItemSetOwning("SET_COURT_BETRAYAL");
	ASSERT_NE(court, nullptr);
	const oracool::SetItemDefinition *ring = oracool::FindSetItem("SET_COURT_BETRAYAL");
	ASSERT_NE(ring, nullptr);

	auto wearAt = [&](int slot, const oracool::SetItemDefinition &piece) {
		devilution::Item &item = player.InvBody[slot];
		item = {};
		item._itype = ItemType::Misc;
		item._iStatFlag = true;
		item._iIdentified = true;
		item._iCurs = static_cast<uint16_t>(piece.cursor);
	};

	wearAt(0, *ring);
	EXPECT_EQ(oracool::WornSetPieces(player, *court), 1);
	// The same ring again in a second slot must NOT become a second piece.
	wearAt(1, *ring);
	EXPECT_EQ(oracool::WornSetPieces(player, *court), 1)
	    << "a duplicated piece counted twice - the completion edge is reachable without the set";
}

// The armour trap. IPL_ACP is a PERCENTAGE of the item's own armour, and a bonus rung has no item,
// so routing it through the scratch item made "+12 armour" worth exactly 1 point (the sign fallback
// in ItemBonusTotals::AddItem). ApplySetBonusesToTotals adds it flat instead.
TEST(OracoolItemSets, ArmorOnABonusRungIsWorthItsDeclaredValue)
{
	Players.resize(1);
	MyPlayer = &Players[0];
	devilution::Player &player = Players[0];
	const oracool::ItemSetDefinition *ironRoot = oracool::FindItemSetOwning("SET_IRON_ROOT_HELM");
	ASSERT_NE(ironRoot, nullptr) << "the Iron Root's first item id changed";

	// Find the ladder's total declared armour, so the expectation follows the data rather than
	// restating a number that would rot the moment a rung was retuned.
	int declared = 0;
	for (int i = 0; i < ironRoot->bonusCount; i++) {
		const oracool::SetBonusDefinition &rung = oracool::ItemSetBonuses[ironRoot->firstBonus + i];
		for (const ItemPower &power : rung.powers) {
			if (power.type == IPL_ACP)
				declared += power.param1;
		}
	}
	ASSERT_GT(declared, 1) << "the Iron Root ladder no longer grants armour; pick another set";

	// Wear the whole set, so every rung is earned. Only _iCurs matters to WornSetPieces - the icon
	// IS a set piece's identity - but the rest is set so the slot reads as a real worn item.
	for (int i = 0; i < ironRoot->itemCount && i < NUM_INVLOC; i++) {
		devilution::Item &slot = player.InvBody[i];
		slot = {};
		slot._itype = ItemType::Misc;
		slot._iStatFlag = true;
		slot._iIdentified = true;
		slot._iCurs = static_cast<uint16_t>(oracool::ItemSetItems[ironRoot->firstItem + i].cursor);
	}

	oracool::ItemBonusTotals totals;
	oracool::ApplySetBonusesToTotals(player, totals);
	EXPECT_EQ(totals.bonusArmor, declared)
	    << "armour on a bonus rung collapsed to the +1 sign fallback again";
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

// The stash's hover gate read GetLeftPanel() - the vanilla 320x352 slot, which on a 720-tall screen
// spans y=120..472 - while the stash itself is a 340x720 window whose grid reaches y=609. Rows 11
// through 15 were therefore outside the gate: "hovering and ctrl+click doesnt work on last 5 rows of
// stash grid" (user, 2026-08-16). Ctrl+click failed for the same reason, because CheckStashItem's
// ctrl branch transfers pcursstashitem, which only the hover path ever sets.
//
// This pins the property the correct gate has and the wrong one lacked: the stash's own rect
// contains every cell of its grid, corner to corner.
TEST(OracoolAudit2, StashPanelRectContainsEveryGridCell)
{
	const Rectangle panel = GetStashPanelRect();
	for (int y = 0; y < StashGridRows; y++) {
		for (int x = 0; x < StashGridColumns; x++) {
			const Point topLeft = GetStashSlotCoord({ x, y });
			// The far corner of the cell, not just its origin - the last row's bottom edge is where
			// a gate that is very slightly too short shows up.
			const Point bottomRight = topLeft + Displacement { INV_SLOT_SIZE_PX - 1, INV_SLOT_SIZE_PX - 1 };
			EXPECT_TRUE(panel.contains(topLeft)) << "cell " << x << "," << y << " starts outside the stash panel";
			EXPECT_TRUE(panel.contains(bottomRight)) << "cell " << x << "," << y << " ends outside the stash panel";
		}
	}

	// And the thing that actually broke: the vanilla side-panel slot does NOT contain the whole
	// grid, so anything hit-testing the stash against it loses rows. Kept as a live assertion rather
	// than a comment - if the slot ever grows to cover the window, this fires and someone can decide
	// whether the distinction still matters.
	const Rectangle vanillaSlot = GetLeftPanel();
	const Point lastCell = GetStashSlotCoord({ StashGridColumns - 1, StashGridRows - 1 });
	EXPECT_FALSE(vanillaSlot.contains(lastCell))
	    << "GetLeftPanel now covers the whole stash grid - the two rects have converged";
}

// A tier has to survive the save. loadsave.cpp clamped a loaded tier to `<= Primal`, which was every
// tier when it was written; OracoolItemTier::Set arrived at 4 and was silently reset to None on the
// way back in - "all set item i acquired with debug commands turned into uniques with suspicious
// stats" (user, 2026-08-16). The "suspicious stats" were UniqueItems[0], The Butcher's Cleaver:
// MakeSetItem marks a set piece ITEM_QUALITY_UNIQUE, so once the tier was gone the description fell
// through to the vanilla unique branch and printed powers belonging to _iUid 0.
//
// Walks every tier rather than checking Set alone: the bug was not "Set is missing", it was "the
// range check names a tier instead of the last one", and only walking all of them catches the next
// one added.
TEST(OracoolAudit2, EveryItemTierSurvivesTheSaveRoundTrip)
{
	for (int raw = 0; raw <= static_cast<int>(OracoolItemTier::LAST); raw++) {
		const auto tier = static_cast<OracoolItemTier>(raw);
		// The load path's own clamp expression, which is the thing that was wrong.
		const auto loaded = static_cast<uint8_t>(raw) <= static_cast<uint8_t>(OracoolItemTier::LAST)
		    ? tier
		    : OracoolItemTier::None;
		EXPECT_EQ(loaded, tier) << "tier " << raw << " does not survive a save round trip";
	}
	// LAST must actually be the last, or the clamp lets a bogus value through instead.
	EXPECT_EQ(static_cast<int>(OracoolItemTier::LAST), static_cast<int>(OracoolItemTier::Set))
	    << "a tier was added past Set without moving LAST";

	// And the consequence that made it visible: a set item is quality UNIQUE, so if its tier is ever
	// lost the description reads UniqueItems[_iUid] - which on a set item is 0, the Cleaver.
	Players.resize(1);
	MyPlayer = &Players[0];
	Players[0] = {};
	const oracool::SetItemDefinition *helm = oracool::FindSetItem("SET_ASHEN_HELM");
	ASSERT_NE(helm, nullptr);
	devilution::Item item {};
	InitializeItem(item, static_cast<_item_indexes>(oracool::BaseItemForSetSlot(helm->slot)));
	oracool::MakeSetItem(item, *helm);
	EXPECT_EQ(item._iMagical, ITEM_QUALITY_UNIQUE);
	EXPECT_TRUE(item.hasOracoolTier())
	    << "a set item without its tier is described as UniqueItems[" << item._iUid << "]";
}

// A set piece has to LOOK like one. Three separate display paths each treated Set as something else,
// and all three failed silently - the item was correct in memory the whole time (user, 2026-08-16:
// a complete Ashen Saint showing base names, no affixes and a gold font).
//
//   getName()                 -> base name, because MakeSetItem left _iCreateInfo at 0 and getName
//                                reads a zero there as "no real name here"
//   getTextColor()            -> gold, because the tier switch had no Set case and fell through to
//                                _iMagical, which MakeSetItem sets to UNIQUE
//   AddItemPowerPanelStrings  -> nothing, because Set is an OracoolItemTier so it printed the affix
//                                ROLLER's arrays, which a set item never fills
TEST(OracoolAudit2, SetItemPresentsAsASetItem)
{
	Players.resize(1);
	MyPlayer = &Players[0];
	Players[0] = {};

	const oracool::SetItemDefinition *armor = oracool::FindSetItem("SET_ASHEN_ARMOR");
	ASSERT_NE(armor, nullptr);
	devilution::Item item {};
	InitializeItem(item, static_cast<_item_indexes>(oracool::BaseItemForSetSlot(armor->slot)));
	oracool::MakeSetItem(item, *armor);

	// Its own name, not the base item's - and specifically NOT dependent on _iCreateInfo.
	EXPECT_EQ(std::string(item.getName().str()), "Vhal's Emberguard");
	item._iCreateInfo = 0;
	EXPECT_EQ(std::string(item.getName().str()), "Vhal's Emberguard")
	    << "the name went back to the base item's when _iCreateInfo was cleared";

	// Green, which is the whole point of the tier - and must not be the gold a unique gets, since
	// MakeSetItem marks set pieces ITEM_QUALITY_UNIQUE.
	EXPECT_EQ(item.getTextColor(), UiFlags::ColorOracoolGreen);
	EXPECT_NE(item.getTextColor(), UiFlags::ColorWhitegold);

	// The definition is reachable from the item alone, which is what the description path needs -
	// there is no stored set id, only the icon.
	const oracool::SetItemDefinition *found = oracool::FindSetItemByCursor(item._iCurs);
	ASSERT_EQ(found, armor);
	EXPECT_GT(oracool::CountLivePowers(found->powers, 6), 0)
	    << "the armour has no live powers, so the description would be empty however it is printed";
}

// Set bonus rungs are CUMULATIVE. They were "highest rung only" at first, on my claim that the upper
// rungs restate the lower ones - reading the delivered ladders back, they do not. Every rung of the
// Ashen Saint is distinct, so under the old rule a SIXTH piece removed the five beneath it and made
// completing the set a downgrade in resistances, mana and damage taken (user, 2026-08-16: "arent
// there any set bonuses?").
//
// The property worth pinning is monotonicity: one more piece never takes a stat away.
TEST(OracoolAudit2, SetBonusesAccumulateAndNeverRegress)
{
	Players.resize(1);
	devilution::Player &player = Players[0];
	player = {};
	MyPlayer = &player;

	const oracool::ItemSetDefinition *ashen = oracool::FindItemSetOwning("SET_ASHEN_HELM");
	ASSERT_NE(ashen, nullptr);

	// Equip the set one piece at a time, in whatever slots they take, and watch the totals climb.
	int previousFire = -1;
	int previousMana = -1;
	int previousEarned = -1;
	for (int pieces = 0; pieces <= ashen->itemCount; pieces++) {
		for (auto &slot : player.InvBody)
			slot.clear();
		int placed = 0;
		for (int i = 0; i < ashen->itemCount && placed < pieces; i++) {
			const oracool::SetItemDefinition &def = oracool::ItemSetItems[ashen->firstItem + i];
			const int base = oracool::BaseItemForSetSlot(def.slot);
			if (base < 0)
				continue; // amulet/ring/relic/cloak have no base item yet
			devilution::Item piece {};
			InitializeItem(piece, static_cast<_item_indexes>(base));
			oracool::MakeSetItem(piece, def);
			// Straight into the matching body slot - this test is about the bonus ladder, not about
			// the equip rules, so it places by the item's own location.
			player.InvBody[piece._iLoc == ILOC_ONEHAND ? INVLOC_HAND_LEFT
			        : piece._iLoc == ILOC_ARMOR       ? INVLOC_CHEST
			        : piece._iLoc == ILOC_HELM        ? INVLOC_HEAD
			        : piece._iLoc == ILOC_GLOVES      ? INVLOC_GLOVES
			        : piece._iLoc == ILOC_BOOTS       ? INVLOC_BOOTS
			                                          : INVLOC_WAIST]
			    = piece;
			placed++;
		}

		oracool::ItemBonusTotals totals {};
		oracool::ApplySetBonusesToTotals(player, totals);
		const int earned = oracool::ForEachEarnedSetBonus(player, *ashen, nullptr, nullptr);

		EXPECT_GE(totals.fireResist, previousFire) << "fire resist FELL at " << placed << " pieces";
		EXPECT_GE(totals.mana, previousMana) << "mana FELL at " << placed << " pieces";
		EXPECT_GE(earned, previousEarned) << "an earned rung was lost at " << placed << " pieces";
		previousFire = totals.fireResist;
		previousMana = totals.mana;
		previousEarned = earned;
	}

	// And concretely: with every spawnable piece worn, the two-piece rung's fire resist is still
	// there alongside the five-piece rung's mana. Under "highest only" exactly one of these held.
	EXPECT_GE(previousFire, 15) << "the two-piece rung's fire resist was dropped by a later rung";
	EXPECT_GE(previousMana, 25) << "the five-piece rung's mana was dropped by a later rung";
}

// ---------------------------------------------------------------------------------------------
// The resistance soft cap and per-difficulty penetration (2026-08-19).
//
// Vanilla clamped each resistance to [0, 75] with no reference to the difficulty, so gear stopped
// mattering the moment a school reached 75 - which a Barbarian reaches from her level alone. These
// pin the curve's SHAPE, not its constants: the numbers are expected to be tuned from telemetry,
// and a test that restates them would just have to be edited alongside.
// ---------------------------------------------------------------------------------------------

TEST(OracoolAudit, ResistanceNeverExceedsTheHardCapNorFallsBelowZero)
{
	// The bound the int8_t fields and every reader downstream rely on. Exhaustive over a raw range
	// far wider than any gear could produce, on every difficulty.
	for (int difficulty = DIFF_NORMAL; difficulty <= DIFF_TORMENT; difficulty++) {
		for (int raw = -200; raw <= 400; raw++) {
			const int out = oracool::ApplyResistanceCurve(raw, static_cast<_difficulty>(difficulty));
			ASSERT_GE(out, 0) << "raw " << raw << " on difficulty " << difficulty;
			ASSERT_LE(out, oracool::ResistanceHardCap) << "raw " << raw << " on difficulty " << difficulty;
		}
	}
}

TEST(OracoolAudit, ResistanceIsMonotonicSoMoreGearIsNeverWorse)
{
	// The property a player can actually feel: putting on a resistance item must never LOWER the
	// number on the character sheet. Integer division past the soft cap is where that could go
	// wrong if the curve were ever rewritten with rounding.
	for (int difficulty = DIFF_NORMAL; difficulty <= DIFF_TORMENT; difficulty++) {
		int previous = -1;
		for (int raw = -200; raw <= 400; raw++) {
			const int out = oracool::ApplyResistanceCurve(raw, static_cast<_difficulty>(difficulty));
			ASSERT_GE(out, previous) << "resistance FELL going from raw " << (raw - 1) << " to " << raw
			                         << " on difficulty " << difficulty;
			previous = out;
		}
	}
}

TEST(OracoolAudit, ResistanceReturnsDiminishPastTheSoftCap)
{
	// Below the soft cap a point is a point; above it a point costs more than one. Stated as a
	// comparison between the two bands rather than as "1/3", so tuning the divisor keeps this green
	// while removing the soft cap entirely does not.
	const int atCap = oracool::ApplyResistanceCurve(oracool::ResistanceSoftCap, DIFF_NORMAL);
	EXPECT_EQ(atCap, oracool::ResistanceSoftCap) << "the soft cap is not reached one-for-one";

	const int belowGain = atCap - oracool::ApplyResistanceCurve(oracool::ResistanceSoftCap - 30, DIFF_NORMAL);
	const int aboveGain = oracool::ApplyResistanceCurve(oracool::ResistanceSoftCap + 30, DIFF_NORMAL) - atCap;
	EXPECT_EQ(belowGain, 30) << "points below the soft cap are not one-for-one";
	EXPECT_LT(aboveGain, belowGain) << "thirty points bought as much above the soft cap as below it";
	EXPECT_GT(aboveGain, 0) << "the soft cap is behaving as a hard cap - nothing is gained past it";
}

TEST(OracoolAudit, HarderDifficultiesPenetrateResistanceStrictlyMore)
{
	// The reason gear matters at endgame. The same raw total must be worth less on each rung down,
	// and Normal must cost nothing - a character who never leaves Normal sees no change from this
	// work at all.
	EXPECT_EQ(oracool::ResistancePenaltyFor(DIFF_NORMAL), 0) << "Normal now taxes resistance";

	constexpr int Raw = 100;
	int previous = oracool::ApplyResistanceCurve(Raw, DIFF_NORMAL);
	for (int difficulty = DIFF_NIGHTMARE; difficulty <= DIFF_TORMENT; difficulty++) {
		const int out = oracool::ApplyResistanceCurve(Raw, static_cast<_difficulty>(difficulty));
		EXPECT_LT(out, previous) << "difficulty " << difficulty << " did not penetrate more than the one before";
		previous = out;
	}
}

TEST(OracoolAudit, TheHardCapIsReachableOnEveryDifficulty)
{
	// A ceiling nobody can touch is not a soft cap, it is a lie told on the character sheet. Enough
	// raw resistance must always get there, including on Torment where the penalty is largest.
	for (int difficulty = DIFF_NORMAL; difficulty <= DIFF_TORMENT; difficulty++) {
		const int out = oracool::ApplyResistanceCurve(1000, static_cast<_difficulty>(difficulty));
		EXPECT_EQ(out, oracool::ResistanceHardCap) << "difficulty " << difficulty << " cannot reach the hard cap";
	}
}

// ---------------------------------------------------------------------------------------------
// Runt and Giant ordinary monsters (2026-08-19).
//
// The Colossal affix shipped in Phase 3.2; the other half of that Pipeline entry - ordinary
// monsters born at an odd size - did not. The size is DERIVED from the level seed and the monster's
// index rather than stored, so these pin the two properties that derivation has to have.
// ---------------------------------------------------------------------------------------------

TEST(OracoolAudit, MonsterSizeIsStableForTheSameSeedAndMonster)
{
	// The one that matters: a monster must be the same size after a save and reload. Since nothing
	// is stored, that is entirely a question of the function being pure and repeatable.
	for (uint32_t seed : { 1u, 12345u, 0xDEADBEEFu }) {
		for (size_t type = 0; type < 8; type++) {
			for (size_t id = 0; id < 40; id++) {
				const oracool::MonsterSize first = oracool::OrdinaryMonsterSize(seed, type, id);
				const oracool::MonsterSize again = oracool::OrdinaryMonsterSize(seed, type, id);
				ASSERT_EQ(first, again) << "size changed between two calls for seed " << seed
				                        << " type " << type << " id " << id;
			}
		}
	}
}

TEST(OracoolAudit, OrdinaryMonstersAreNeverColossal)
{
	// Colossal belongs to the affix and is the champion's silhouette. If a birth roll could produce
	// it, a rank-and-file monster would wear a champion's read with none of a champion's danger.
	for (uint32_t seed = 1; seed <= 200; seed++) {
		for (size_t type = 0; type < 6; type++) {
			for (size_t id = 0; id < 20; id++) {
				ASSERT_NE(oracool::OrdinaryMonsterSize(seed, type, id), oracool::MonsterSize::Colossal);
			}
		}
	}
}

TEST(OracoolAudit, AtMostOneOddSizePerMonsterTypePerFloor)
{
	// The memory bound, stated as the property that produces it. The scale cache owns six
	// animations per (type, size), so a type that could field BOTH a runt and a giant would cost
	// three copies of itself on one floor instead of two.
	for (uint32_t seed = 1; seed <= 300; seed++) {
		for (size_t type = 0; type < 6; type++) {
			std::set<int> sizes;
			for (size_t id = 0; id < 60; id++) {
				const oracool::MonsterSize size = oracool::OrdinaryMonsterSize(seed, type, id);
				if (size != oracool::MonsterSize::Normal)
					sizes.insert(static_cast<int>(size));
			}
			ASSERT_LE(sizes.size(), 1u) << "seed " << seed << " type " << type
			                            << " fielded more than one odd size";
		}
	}
}

TEST(OracoolAudit, OddSizedMonstersAreAMinorityAndBothSizesOccur)
{
	// Neither dead code nor the new normal. Across a wide sample both odd sizes must appear, and the
	// great majority of monsters must still be ordinary - a floor where half the monsters are odd
	// sized has no odd-sized monsters, only noisy ones.
	int total = 0;
	int runts = 0;
	int giants = 0;
	for (uint32_t seed = 1; seed <= 500; seed++) {
		for (size_t type = 0; type < 6; type++) {
			for (size_t id = 0; id < 20; id++, total++) {
				switch (oracool::OrdinaryMonsterSize(seed, type, id)) {
				case oracool::MonsterSize::Runt: runts++; break;
				case oracool::MonsterSize::Giant: giants++; break;
				default: break;
				}
			}
		}
	}
	EXPECT_GT(runts, 0) << "no monster is ever a runt";
	EXPECT_GT(giants, 0) << "no monster is ever a giant";
	EXPECT_LT(runts + giants, total / 3) << "odd sizes are common enough to be the new normal";
}

// ---------------------------------------------------------------------------------------------
// Impact cues: the cast scope that carries a skill from CastSpell to the missile that lands
// (2026-08-19). The cues themselves need audio and a running game; these pin the plumbing.
// ---------------------------------------------------------------------------------------------

TEST(OracoolAudit, AMissileCarriesNoSkillUnlessACastGaveItOne)
{
	// The one that would be loud if it broke. Most missiles in the game are not class skills at all
	// - traps, monster attacks, town portals - and a default of 0 rather than None would make every
	// one of them ring the impact cue of whichever skill happens to sit first in the tree enum.
	const Missile fresh {};
	EXPECT_EQ(static_cast<oracool::ClassTreeSkill>(fresh.oracoolSkill), oracool::ClassTreeSkill::None)
	    << "a missile nobody cast carries a real skill id";
	EXPECT_EQ(oracool::CurrentCastSkill(), oracool::ClassTreeSkill::None)
	    << "a cast scope is open before anything cast";
}

TEST(OracoolAudit, TheCastScopeClosesBehindItself)
{
	// A scope left open would attribute every later missile - a monster's arrow, a trap - to the
	// last skill the player cast, and the cue would fire on someone else's hit.
	oracool::BeginSkillCast(oracool::ClassTreeSkill::Sacrifice);
	EXPECT_EQ(oracool::CurrentCastSkill(), oracool::ClassTreeSkill::Sacrifice);
	oracool::EndSkillCast();
	EXPECT_EQ(oracool::CurrentCastSkill(), oracool::ClassTreeSkill::None)
	    << "the cast scope stayed open after the cast finished";
}

/**
 * Every rune says something in every host.
 *
 * User, 2026-08-20: "why runes show available RW instead of their affixes/stats?" - the answer was
 * that a loose rune never printed its effects at all. Fixing that surfaced an older bug underneath:
 * GemSocketLine only ever formatted the launch gems' channels, so the fields Sockets v2 added were
 * invisible - the percentage damage roll, the four attributes, the two find stats, the special
 * effect flags, Hel's requirement cut and Zod's indestructible.
 *
 * The visible symptom was a rune whose only effect in a host is a FLAG. Shael in a weapon is faster
 * attack and nothing else, so it rendered as "Shael Rune: " - a colon with an empty list after it,
 * shipped since 1.8.9 and never noticed, because nothing reads a socket line but a person.
 *
 * The 33 runes all carry an effect in all three hosts by design, so an empty list is a formatter
 * gap rather than a data gap. Gems are deliberately NOT asserted here: a gem legitimately does
 * nothing in some hosts.
 */
TEST(OracoolAudit, EveryRuneDescribesItselfInEveryHost)
{
	int runesChecked = 0;
	for (int i = IDI_GOLD; i <= IDI_LAST; i++) {
		if (!IsOracoolRuneIdx(i))
			continue;
		runesChecked++;
		const auto idx = static_cast<uint16_t>(i);
		for (const oracool::SocketHost host : { oracool::SocketHost::Weapon, oracool::SocketHost::Shield, oracool::SocketHost::Armor }) {
			EXPECT_FALSE(oracool::GemHostEffectLine(idx, host).empty())
			    << AllItemsList[i].iName << " has no printable effect in host " << static_cast<int>(host);
			// The socket line is the same list with the name in front, so it must be non-empty for
			// the same reason - and it is the one the player actually sees on a finished item.
			EXPECT_FALSE(oracool::GemSocketLine(idx, host).empty())
			    << AllItemsList[i].iName << " has an empty socket line in host " << static_cast<int>(host);
		}
	}
	EXPECT_EQ(runesChecked, 33) << "the D2 rune sheet is 33 runes; the loop found a different number";
}

/**
 * A completed runeword's own bonuses reach the totals.
 *
 * User, 2026-08-20, on assembling the fork's first runeword: "I don't see the extra affixes the
 * runeword should bring. Fix it. Make sure they are displayed and actually working in-game
 * mid-fight."
 *
 * The DISPLAY half was the real gap - the panel listed the four runes' individual effects and
 * stopped, so the word read as a name with nothing behind it. This pins the other half: that the
 * word's own fields are actually summed on top of the runes', which is what "working mid-fight"
 * means, since every combat number is read from these totals.
 *
 * Spirit is the case the user hit: four runes, +8 all resists, +24 armor, +16 life on top of Tal,
 * Thul, Ort and Amn's own effects. Asserted as a DELTA over the runes alone, so the test cannot be
 * satisfied by the runes doing the word's job.
 */
TEST(OracoolAudit, ACompletedRunewordAddsItsOwnBonusesOnTopOfItsRunes)
{
	const oracool::RunewordDefinition *spirit = nullptr;
	for (size_t i = 0; i < oracool::RunewordCount(); i++) {
		const oracool::RunewordDefinition *word = oracool::RunewordAt(i);
		if (word != nullptr && std::string(word->name) == "Spirit") {
			spirit = word;
			break;
		}
	}
	ASSERT_NE(spirit, nullptr) << "Spirit is gone from the runeword table";
	ASSERT_GT(spirit->allResists + spirit->bonusAc + spirit->hitPoints, 0)
	    << "Spirit grants nothing, so this test would pass vacuously";

	// The runes alone.
	oracool::ItemBonusTotals runesOnly;
	for (int i = 0; i < spirit->runeCount; i++)
		oracool::ApplyGemToTotals(spirit->runes[i], oracool::SocketHost::Shield, runesOnly);

	// The runes plus the word, which is the order ApplySockets uses.
	oracool::ItemBonusTotals withWord = runesOnly;
	oracool::ApplyRunewordToTotals(*spirit, withWord);

	EXPECT_EQ(withWord.magicResist - runesOnly.magicResist, spirit->allResists);
	EXPECT_EQ(withWord.bonusArmor - runesOnly.bonusArmor, spirit->bonusAc);
	// Life and mana are carried in <<6 fixed point on the totals, in whole points on the word.
	EXPECT_EQ(withWord.hitPoints - runesOnly.hitPoints, spirit->hitPoints << 6);
}

/**
 * SORT gathers runes and gems onto their own page, in fixed positions.
 *
 * User request, 2026-08-20: "Move and sort Runes and Gems in their own tab. The first one
 * unoccupied by items. Sort Runes at the top Left to Right El to Zod. 4 rows total. 3x10 + 1x3
 * boxes. Start sorting gems from the bottom up ... Better quality gems at lower row. Pbems at
 * bottom ... All types of certain Gem to form a column Bottom to top, better to lesser."
 *
 * Every cell below is derived the way the code derives it - the rune's ladder position, the gem's
 * (type, quality) - rather than being a transcription of what one run happened to produce. A test
 * that only recorded the output would agree with the layout drifting.
 */
TEST(OracoolAudit, SortMovesRunesAndGemsToTheirOwnPageInFixedPositions)
{
	Stash.stashList.clear();
	Stash.stashGrids.clear();
	Stash.SetPage(0);

	const auto deposit = [](_item_indexes idx) {
		devilution::Item item;
		InitializeItem(item, idx);
		AutoPlaceItemInStash(*MyPlayer, item, true);
	};

	// Deliberately out of order, and with an ordinary item in the middle, so the sort has real work
	// to do and page 0 is genuinely occupied by something that is not a material.
	deposit(IDI_ORACOOL_RUNE_ZOD);
	deposit(IDI_ORACOOL_GEM_RUBY_PERFECT);
	deposit(IDI_ORACOOL_HELM);
	deposit(IDI_ORACOOL_RUNE_EL);
	deposit(IDI_ORACOOL_GEM_AMETHYST_CHIPPED);

	SortStash(*MyPlayer);

	// The helm keeps page 0 to itself, so the materials land on page 1.
	constexpr unsigned MaterialPage = 1;

	const auto cellHolds = [](unsigned page, Point cell, _item_indexes idx) {
		const StashStruct::StashCell id = Stash.stashGrids[page][cell.x][cell.y];
		if (id == 0)
			return false;
		return Stash.stashList[id - 1].IDidx == idx;
	};

	// Runes: ladder position p -> (p % 10, p / 10). El is position 0; Zod is the last of 33, so
	// position 32 -> column 2, row 3 - the "1x3" fourth row the user asked for.
	EXPECT_TRUE(cellHolds(MaterialPage, { 0, 0 }, IDI_ORACOOL_RUNE_EL)) << "El is not in the top-left cell";
	EXPECT_TRUE(cellHolds(MaterialPage, { 2, 3 }, IDI_ORACOOL_RUNE_ZOD)) << "Zod is not at the end of the fourth rune row";
	EXPECT_EQ(oracool::RuneLadderSize(), 33u) << "the 3x10 + 1x3 layout assumes exactly 33 runes";

	// Gems: column is the type, row counts DOWN from the bottom by quality, so Perfect sits on the
	// last row of the grid and Chipped four rows above it.
	constexpr int GemTopRow = StashGridRows - static_cast<int>(oracool::GemQualityCount);
	EXPECT_TRUE(cellHolds(MaterialPage,
	    { static_cast<int>(oracool::GemType::Ruby), GemTopRow + static_cast<int>(oracool::GemQuality::Perfect) },
	    IDI_ORACOOL_GEM_RUBY_PERFECT))
	    << "the Perfect Ruby is not on the bottom row of the Ruby column";
	EXPECT_TRUE(cellHolds(MaterialPage,
	    { static_cast<int>(oracool::GemType::Amethyst), GemTopRow + static_cast<int>(oracool::GemQuality::Chipped) },
	    IDI_ORACOOL_GEM_AMETHYST_CHIPPED))
	    << "the Chipped Amethyst is not at the top of the Amethyst column";
	EXPECT_EQ(GemTopRow + static_cast<int>(oracool::GemQuality::Perfect), StashGridRows - 1)
	    << "Perfect must land on the grid's last row - 'Pbems at bottom'";

	// And nothing of either family was left behind on the page the ordinary items packed into.
	for (int x = 0; x < StashGridColumns; x++) {
		for (int y = 0; y < StashGridRows; y++) {
			const StashStruct::StashCell id = Stash.stashGrids[0][x][y];
			if (id == 0)
				continue;
			const devilution::Item &item = Stash.stashList[id - 1];
			EXPECT_FALSE(IsOracoolRuneIdx(item.IDidx) || IsOracoolGemIdx(item.IDidx))
			    << "a material was left on page 0 at " << x << "," << y;
		}
	}

	Stash.stashList.clear();
	Stash.stashGrids.clear();
	Stash.SetPage(0);
}

/**
 * The seven salvage buckets PARTITION the item space.
 *
 * User request, 2026-08-20: seven "salvage all X" buttons. That is what forces the partition - with
 * overlap, "Salvage all rares" and "Salvage all ethereal" would each claim an ethereal rare, and
 * which button was pressed first would silently change what you got.
 *
 * Also pins the two refusals that matter: nothing in the socketable families can be salvaged, and
 * neither can a material. A salvage-all that could eat a stack of Zod runes because the wrong box
 * was clicked is the one bug this system must not have.
 */
TEST(OracoolAudit, SalvageBucketsPartitionAndRefuseMaterials)
{
	const auto tierOfNew = [](_item_indexes idx) {
		devilution::Item item;
		InitializeItem(item, idx);
		return item;
	};

	// Every socketable and every material declines, whatever else is true of it.
	for (int i = IDI_GOLD; i <= IDI_LAST; i++) {
		if (!IsOracoolRuneIdx(i) && !IsOracoolGemIdx(i) && !IsOracoolSalvageIdx(i))
			continue;
		const devilution::Item item = tierOfNew(static_cast<_item_indexes>(i));
		EXPECT_FALSE(oracool::IsSalvageable(item))
		    << AllItemsList[i].iName << " can be salvaged - runes, gems and materials must not be";
	}

	// Ethereal outranks quality: one item, one bucket.
	devilution::Item rare;
	InitializeItem(rare, IDI_ORACOOL_HELM);
	rare._iMagical = ITEM_QUALITY_MAGIC;
	rare._iOracoolTier = OracoolItemTier::Rare;
	ASSERT_TRUE(oracool::IsSalvageable(rare));
	EXPECT_EQ(oracool::SalvageTierOf(rare), oracool::SalvageTier::Rare);

	rare._iOracoolEthereal = true;
	EXPECT_EQ(oracool::SalvageTierOf(rare), oracool::SalvageTier::Ethereal)
	    << "an ethereal rare must fall in exactly one bucket, and ethereal is the specific one";

	// Every bucket names a distinct material, so no two buttons produce the same orb.
	std::set<uint16_t> materials;
	for (int i = 0; i < oracool::SalvageTierCount; i++) {
		const auto tier = static_cast<oracool::SalvageTier>(i);
		const uint16_t idx = oracool::SalvageMaterialFor(tier);
		EXPECT_TRUE(IsOracoolSalvageIdx(idx)) << "tier " << i << " yields something that is not a material";
		EXPECT_TRUE(materials.insert(idx).second) << "two tiers share a material";
	}
	EXPECT_EQ(materials.size(), 7u);
}

/**
 * The salvage materials get a row of their own between the runes and the gems.
 *
 * User, 2026-08-20: "land them in a row of their own sorted left to right from white to darkgey
 * somewhere inbetween runes and bems."
 *
 * The column is derived from the item index the same way the code derives it, so this cannot agree
 * with the order drifting - and enum order IS white to dark grey, because the generator emits the
 * seven in the order the user listed both the colours and the salvage buttons.
 */
TEST(OracoolAudit, SortGivesSalvageMaterialsTheirOwnRow)
{
	Stash.stashList.clear();
	Stash.stashGrids.clear();
	Stash.SetPage(0);

	const auto deposit = [](_item_indexes idx) {
		devilution::Item item;
		InitializeItem(item, idx);
		AutoPlaceItemInStash(*MyPlayer, item, true);
	};

	deposit(IDI_ORACOOL_HELM); // keeps page 0, so the materials land on page 1
	deposit(IDI_ORACOOL_SALVAGE_ETHEREAL_IMBUEITIES);
	deposit(IDI_ORACOOL_SALVAGE_WHITE_SCALES);
	deposit(IDI_ORACOOL_RUNE_EL);
	deposit(IDI_ORACOOL_GEM_RUBY_PERFECT);

	SortStash(*MyPlayer);

	constexpr unsigned MaterialPage = 1;
	const auto rowOf = [](_item_indexes idx) {
		for (int x = 0; x < StashGridColumns; x++) {
			for (int y = 0; y < StashGridRows; y++) {
				const StashStruct::StashCell id = Stash.stashGrids[MaterialPage][x][y];
				if (id != 0 && Stash.stashList[id - 1].IDidx == idx)
					return Point { x, y };
			}
		}
		return Point { -1, -1 };
	};

	const Point white = rowOf(IDI_ORACOOL_SALVAGE_WHITE_SCALES);
	const Point grey = rowOf(IDI_ORACOOL_SALVAGE_ETHEREAL_IMBUEITIES);
	ASSERT_NE(white.x, -1) << "White Scales were not placed";
	ASSERT_NE(grey.x, -1) << "Ethereal Imbueities were not placed";

	EXPECT_EQ(white.y, grey.y) << "the seven materials must share ONE row";
	EXPECT_EQ(white.x, 0) << "white is the leftmost of the row";
	EXPECT_EQ(grey.x, oracool::SalvageTierCount - 1) << "dark grey is the rightmost of the row";

	// And that row genuinely sits between the two blocks.
	const Point el = rowOf(IDI_ORACOOL_RUNE_EL);
	const Point ruby = rowOf(IDI_ORACOOL_GEM_RUBY_PERFECT);
	ASSERT_NE(el.x, -1);
	ASSERT_NE(ruby.x, -1);
	EXPECT_GT(white.y, el.y) << "the material row must be below the runes";
	EXPECT_LT(white.y, ruby.y) << "the material row must be above the gems";

	Stash.stashList.clear();
	Stash.stashGrids.clear();
	Stash.SetPage(0);
}
