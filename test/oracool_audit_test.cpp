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
#include <filesystem>
#include <fstream>
#include <map>
#include <set>
#include <cstring>
#include <string>
#include <system_error>
#include <vector>

#include "DiabloUI/ui_flags.hpp"
#include "engine/random.hpp"
#include "engine/render/text_render.hpp"
#include "engine/render/clx_render.hpp"
#include "engine/surface.hpp"
#include "control.h"
#include "cursor.h"
#include "init.h"
#include "dead.h"
#include "inv.h"
#include "items.h"
#include "monstdat.h"
#include "monster.h"
#include "multi.h"
#include "options.h" // a fresh Options, for the shipped-defaults test
#include "pack.h"    // PlayerPack - the fixed struct the stat-point clamp test inspects
#include "oracool/class_tree.h"
#include "oracool/aura_field.h"
#include "oracool/event_log.h"
#include "oracool/hud_layout.h"
#include "oracool/xp_counter.h"
#include "qol/xpbar.h"
#include "utils/ui_fwd.h" // gnScreenWidth/Height - the log and the belt are both derived from them
#include "utils/paths.h"  // SetConfigPath - the options round-trip writes to a temp dir, not diablo.ini
#include "oracool/charms.h"
#include "oracool/area_level.h"
#include "oracool/inventory_layout.h"
#include "oracool/named_encounters.h"
#include "oracool/class_skills.h"
#include "oracool/crafting.h"
#include "oracool/gradual_healing.h"
#include "oracool/gems.h"
#include "oracool/item_set_stats.h"
#include "oracool/unique_affixes.h"
#include "oracool/crafting_menu.h"
#include "oracool/runeword_book.h"
#include "oracool/hud_menu.h"
#include "oracool/shop_grid.h"
#include "oracool/shop_toast.h"
#include "oracool/window_close.h"
#include "oracool/inventory_layout.h"
#include "oracool/item_tiers.h"
#include "oracool/levski_roar.h"
#include "oracool/waypoint_menu.h"
#include "oracool/item_sets.h"
#include "oracool/hero_chunks.h"
#include "oracool/readied_spells.h"
#include "oracool/lesser_uniques.h"
#include "oracool/monster_difficulty.h"
#include "oracool/player_resistance.h"
#include "oracool/skill_sounds.h"
#include "missiles.h"
#include "oracool/monster_scale.h"
#include "oracool/monster_variants.h"
#include "oracool/treasure_class.h"
#include "oracool/endgame_boss.h"
#include "oracool/mystic_orbs.h"
#include "oracool/signets.h"
#include "oracool/charms.h"
#include "oracool/area_level.h"
#include "oracool/inventory_layout.h"
#include "oracool/named_encounters.h"
#include "oracool/paladin_melee.h"
#include "oracool/paladin_skills.h"
#include "oracool/rng_streams.h"
#include "oracool/runewords.h"
#include "oracool/salvage.h"
#include "oracool/skill_points.h"
#include "oracool/chill.h"
#include "oracool/cold.h"
#include "oracool/rogue_arrows.h"
#include "oracool/sprite_import.h"
#include "oracool/spell_ranks.h"
#include "oracool/sprite_scale.h"
#include "oracool/stat_sheet.h"
#include "oracool/ornate_border.h"
#include "oracool/telemetry.h"
#include "oracool/xp_counter.h"
#include "DiabloUI/hero/hero_layout.h" // the character-select column geometry
#include "panels/charpanel.hpp"
#include "panels/spell_book.hpp"
#include "player.h"
#include "playerdat.hpp"
#include "qol/stash.h"
#include "quests.h"
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
	// A demon, said out loud: MonsterClass::Undead is 0, so a value-initialised MonsterData is
	// undead by accident, and the undead resist cold on every difficulty (Round 2). This test is
	// about the difficulty ladder, not cold, and it wants a monster the ladder starts at zero for.
	data.monsterClass = MonsterClass::Demon;
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
	// `= {}`, not `.clear()`. Item::clear() resets _itype alone and deliberately leaves every other
	// field as stale leftover data - so a preceding test that equipped a BROKEN item left
	// _iOracoolBroken set, and HasShieldEquipped refuses a broken shield. This test then set _itype
	// to Shield on top of that residue and failed on three iterations in twenty under
	// `--gtest_shuffle --gtest_random_seed=92531` (external audit of v1.9.92, finding 7 - the same
	// family, found while fixing it).
	player.InvBody[INVLOC_HAND_LEFT] = {};
	player.InvBody[INVLOC_HAND_RIGHT] = {};

	EXPECT_FALSE(oracool::HasShieldEquipped(player));

	player.InvBody[INVLOC_HAND_RIGHT]._itype = ItemType::Shield;
	EXPECT_TRUE(oracool::HasShieldEquipped(player));

	player.InvBody[INVLOC_HAND_RIGHT] = {};
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

	const SpellMask shieldBash = GetSpellBitmask(SpellID::ShieldBash);
	const SpellMask blessedShield = GetSpellBitmask(SpellID::BlessedShield);
	const SpellMask zeal = GetSpellBitmask(SpellID::Zeal);

	// A POINT IN EACH ROW, since 2026-08-27. The mask now asks the class tree for investment as well
	// as asking the character for level and shield, so without this every assertion below would pass
	// for the wrong reason - the skills would be absent because they are unbought, and the shield
	// gate this test exists for would never be exercised at all.
	std::memset(player._pSkillInvestment, 0, sizeof(player._pSkillInvestment));
	player._pSkillInvestment[static_cast<size_t>(SpellID::ShieldBash)] = 1;
	player._pSkillInvestment[static_cast<size_t>(SpellID::BlessedShield)] = 1;
	player._pSkillInvestment[static_cast<size_t>(SpellID::Zeal)] = 1;

	SpellMask mask = oracool::InnateSpellsBitmask(player);
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

	player._pISplLvlAdd = 0;
	std::memset(player._pSplLvl, 0, sizeof(player._pSplLvl));

	player._pLevel = 5;
	EXPECT_EQ(oracool::ZealStrikeCount(player), 0) << "below the gate";
	player._pLevel = 50;
	EXPECT_EQ(oracool::ZealStrikeCount(player), 0)
	    << "skill level 0 is not a weak Zeal, it is no Zeal - the tree wants a point before the "
	       "skill exists, and character level buys no strikes by itself";

	// The user's ladder, 2026-08-30: "+1 hit" at skill levels 1, 3 and 5 on a base of one, so two,
	// three and four strikes. The EVEN levels between them buy accuracy only, which is what makes
	// this table worth writing out rather than computing. The base of one is notional: skill level
	// 0 means the skill was never taken, so the first row a player can actually be on is 1.
	const struct {
		int skillLevel;
		int strikes;
	} ladder[] = { { 0, 0 }, { 1, 2 }, { 2, 2 }, { 3, 3 }, { 4, 3 }, { 5, 4 }, { 6, 4 }, { 20, 4 } };
	for (const auto &step : ladder) {
		player._pSkillInvestment[zeal] = static_cast<uint8_t>(step.skillLevel);
		EXPECT_EQ(oracool::ZealStrikeCount(player), step.strikes)
		    << "at Zeal skill level " << step.skillLevel;
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
// External audit, 2026-08-25: "Make Ethereal" disarmed a socketed Zod while leaving it installed.
//
// Zod writes DUR_INDESTRUCTIBLE into _iDurability and leaves _iMaxDur alone, and the recipe's
// eligibility guard tests _iMaxDur - so a Zod-bearing item passed it, and the durability clamp at
// the end of MakeItemEthereal then replaced the indestructible marker with half the old maximum.
// The rune stayed socketed, stayed listed, stayed spent, and stopped doing anything.
TEST(OracoolCrafting, MakeEtherealKeepsASocketedZodWorking)
{
	Players.resize(1);
	MyPlayer = &Players[0];
	*MyPlayer = {};

	// Built by hand rather than through InitializeItem: the only fields that matter here are the
	// durability pair, the class and the socket, and a real base item would drag in its own values.
	devilution::Item sword {};
	sword._iClass = ICLASS_WEAPON;
	sword._itype = ItemType::Sword;
	sword._iMaxDur = 40;
	sword._iDurability = 40;
	sword._iMinDam = 4;
	sword._iMaxDam = 10;
	sword._iSocketCount = 1;
	sword._iSocketed[0] = IDI_ORACOOL_RUNE_ZOD;

	ASSERT_TRUE(oracool::SocketsMakeIndestructible(sword)) << "test setup: the Zod is not registering";
	oracool::ApplyZodToHost(sword);
	ASSERT_EQ(sword._iDurability, DUR_INDESTRUCTIBLE);

	ASSERT_TRUE(MakeItemEthereal(sword));

	EXPECT_TRUE(sword._iOracoolEthereal) << "the item did not become ethereal";
	EXPECT_EQ(sword._iDurability, DUR_INDESTRUCTIBLE)
	    << "the ethereal transform disarmed a Zod that is still socketed";
}

// External audit, 2026-08-25: a broken shield still satisfied Shield Bash and Blessed Shield,
// because only the item's TYPE was checked. A broken item is left equipped rather than destroyed and
// contributes nothing at all everywhere else - no armour, no block.
TEST(OracoolPaladinSkills, BrokenShieldDoesNotCountAsAShield)
{
	Players.resize(1);
	MyPlayer = &Players[0];
	devilution::Player &player = Players[0];
	player = {};
	player._pClass = HeroClass::Warrior;

	devilution::Item &shield = player.InvBody[INVLOC_HAND_RIGHT];
	shield = {};
	shield._itype = ItemType::Shield;
	shield._iOracoolBroken = false;
	EXPECT_TRUE(oracool::HasShieldEquipped(player)) << "a working shield should count";

	shield._iOracoolBroken = true;
	EXPECT_FALSE(oracool::HasShieldEquipped(player))
	    << "a broken shield still powered the shield skills";
}

// External audit, 2026-08-25 (P0): a save is untrusted input even when this program wrote it - files
// get truncated, half-copied, hand-edited and restored from backups of another build.
//
// PlayerPack::_pNumInv is a uint8_t and both InvList arrays hold InventoryGridCells (70), so a
// record claiming 255 walked the unpack loop 185 entries past the end of both, reading one array out
// of bounds and WRITING the other. This is the shape of that record.
TEST(OracoolSaveValidation, ImpossibleInventoryCountIsClamped)
{
	Players.resize(1);
	devilution::Player &player = Players[0];
	player = {};

	PlayerPack packed {};
	packed.pClass = static_cast<uint8_t>(HeroClass::Warrior);
	packed.pLevel = 1;
	packed._pNumInv = 255; // impossible: the array holds 70

	UnPackPlayer(packed, player);

	EXPECT_LE(player._pNumInv, InventoryGridCells)
	    << "an impossible inventory count reached the unpack loop unclamped";
}

// The grid holds 1-based InvList references, and every reader subscripts InvList with them. A
// reference past the live item count is cleared rather than trusted - an empty cell is always safe.
TEST(OracoolSaveValidation, GridReferencesPastTheItemCountAreCleared)
{
	Players.resize(1);
	devilution::Player &player = Players[0];
	player = {};

	PlayerPack packed {};
	packed.pClass = static_cast<uint8_t>(HeroClass::Warrior);
	packed.pLevel = 1;
	packed._pNumInv = 2;
	packed.InvGrid[0] = 1;   // valid - item 0
	packed.InvGrid[1] = 99;  // nonsense - no such item
	packed.InvGrid[2] = -99; // nonsense as a continuation cell too

	UnPackPlayer(packed, player);

	EXPECT_EQ(player.InvGrid[0], 1) << "a valid grid reference was discarded";
	EXPECT_EQ(player.InvGrid[1], 0) << "a grid reference past the item count survived";
	EXPECT_EQ(player.InvGrid[2], 0) << "a negative out-of-range continuation cell survived";
}

// External audit, 2026-08-25: Player::_pStatPts is an int and PlayerPack::pStatPts is a uint8_t, and
// the pack narrowed it silently. 260 came back as 4. The range is not theoretical - a level-99
// character has around 490 points to place and Oracool's own Reset Stats button hands all of them
// back at once, so the feature that makes the number large is one this fork added.
//
// The audit named the three values to test; these are them.
TEST(OracoolHeroChunks, StatPointsRoundTripPastAByte)
{
	Players.resize(1);
	for (const int points : { 0, 1, 254, 255, 256, 490, 1000 }) {
		devilution::Player &source = Players[0];
		source = {};
		source._pStatPts = points;

		const std::vector<uint8_t> tail = oracool::BuildHeroChunkTail(source);
		ASSERT_GT(tail.size(), 4u);

		devilution::Player target {};
		target._pStatPts = 12345; // stale state the apply must overwrite
		oracool::ApplyHeroChunks(target, tail.data(), tail.size());

		EXPECT_EQ(target._pStatPts, points)
		    << "unspent stat points did not survive a save/load round trip at " << points;
	}
}

TEST(OracoolHeroChunks, ProgressionDoesNotBleedFromOneHeroToTheNext)
{
	// Audit finding, 2026-08-26. The milestone mask and the signet count live in file-static arrays
	// keyed by player SLOT, not on the Player - and the character-select screen previews every save
	// in turn through Players[0]. ApplyHeroChunks writes only the chunks a save carries and returns
	// early when there is no tail at all, so a legacy hero inherited whatever the last previewed
	// character had claimed.
	Players.resize(1);
	devilution::Player &player = Players[0];
	MyPlayer = &player;
	player = {};
	player._pClass = HeroClass::Warrior;

	// A progressed character: milestones claimed, signets spent.
	oracool::ApplyMilestones(player, 0b1011);
	oracool::ApplySignetsUsed(player, 7);
	ASSERT_EQ(oracool::PackMilestones(player), 0b1011u);
	ASSERT_EQ(oracool::PackSignetsUsed(player), 7);

	// Now the SAME SLOT previews a legacy hero - no extension tail at all. That is the early return
	// inside ApplyHeroChunks, and it is exactly the path a caller-side reset would have missed.
	oracool::ApplyHeroChunks(player, nullptr, 0);
	EXPECT_EQ(oracool::PackMilestones(player), 0u)
	    << "a legacy hero inherited the previous character's milestones";
	EXPECT_EQ(oracool::PackSignetsUsed(player), 0)
	    << "a legacy hero inherited the previous character's spent signets";

	// And a tail REJECTED for bad magic must clear just as thoroughly - a corrupt save must not
	// hand its reader someone else's progress.
	oracool::ApplyMilestones(player, 0b1111);
	oracool::ApplySignetsUsed(player, 9);
	const uint8_t rubbish[8] = { 1, 2, 3, 4, 5, 6, 7, 8 };
	oracool::ApplyHeroChunks(player, rubbish, sizeof(rubbish));
	EXPECT_EQ(oracool::PackMilestones(player), 0u) << "a rejected tail left stale milestones";
	EXPECT_EQ(oracool::PackSignetsUsed(player), 0) << "a rejected tail left stale signets";

	// Creating a NEW character in that slot starts from nothing, which is where the stale state
	// used to be serialised straight into the new hero's first save.
	oracool::ApplyMilestones(player, 0b0111);
	oracool::ApplySignetsUsed(player, 5);
	CreatePlayer(player, HeroClass::Rogue);
	EXPECT_EQ(oracool::PackMilestones(player), 0u)
	    << "a brand new character was born with milestones already claimed";
	EXPECT_EQ(oracool::PackSignetsUsed(player), 0)
	    << "a brand new character was born with signets already spent";
}

TEST(OracoolHeroChunks, TheBurningAuraSurvivesTheEnumMovingUnderIt)
{
	// Audit finding, 2026-08-26. The aura was persisted as an ABSOLUTE ClassTreeSkill ordinal, which
	// shifts whenever a class earlier in the enum gains rows - and it already had: the Passive
	// Skills page moved every Bard and Monk aura, so those characters came back with nothing lit.
	// A class guard turned that into "no aura" rather than "somebody else's", which was a seatbelt.
	//
	// Tag 13 stores (hero class, index WITHIN that class). Both survive growth, because growth only
	// ever appends to a class block.
	Players.resize(1);
	devilution::Player &source = Players[0];
	MyPlayer = &source;
	source = {};
	source._pClass = HeroClass::Warrior;
	source._pLevel = 30;
	source._pMaxHP = 1000;
	source._pHitPoints = 1000;
	source._pUnspentSkillPoints = 10;

	ASSERT_TRUE(oracool::InvestClassTreePoint(source, oracool::ClassTreeSkill::Might));
	ASSERT_TRUE(oracool::ToggleClassAura(source, oracool::ClassTreeSkill::Might));
	ASSERT_EQ(oracool::GetActiveClassAura(source), oracool::ClassTreeSkill::Might);

	const std::vector<uint8_t> tail = oracool::BuildHeroChunkTail(source);

	devilution::Player target {};
	target._pClass = HeroClass::Warrior;
	target._pLevel = 30;
	target._pMaxHP = 1000;
	target._pHitPoints = 1000;
	std::memcpy(target._pClassTreeInvestment, source._pClassTreeInvestment,
	    sizeof(target._pClassTreeInvestment));
	oracool::ApplyHeroChunks(target, tail.data(), tail.size());
	EXPECT_EQ(oracool::GetActiveClassAura(target), oracool::ClassTreeSkill::Might)
	    << "the aura did not survive an ordinary round trip";

	// The property that matters: the stored form is the RELATIVE one, so a reader that resolves it
	// against the class gets the same skill however the enum has grown. Might is the Paladin's
	// tenth row, and that stays true no matter what is appended to any class.
	EXPECT_EQ(oracool::ClassTreeIconIndex(oracool::ClassTreeSkill::Might), 9);
	const std::optional<oracool::ClassTreeSkill> resolved =
	    oracool::ClassTreeSkillAtIndex(HeroClass::Warrior, 9);
	ASSERT_TRUE(resolved.has_value());
	EXPECT_EQ(*resolved, oracool::ClassTreeSkill::Might);

	// An index belonging to another class resolves to nothing rather than to whatever happens to
	// sit at that offset - which is exactly the confusion the absolute form allowed.
	EXPECT_FALSE(oracool::ClassTreeSkillAtIndex(HeroClass::Warrior, 999).has_value());

	// A save whose aura says it belongs to a DIFFERENT class is dropped, not reinterpreted.
	devilution::Player wrongClass {};
	wrongClass._pClass = HeroClass::Rogue;
	wrongClass._pLevel = 30;
	oracool::ApplyHeroChunks(wrongClass, tail.data(), tail.size());
	EXPECT_EQ(oracool::GetActiveClassAura(wrongClass), oracool::ClassTreeSkill::None)
	    << "a Rogue inherited a Paladin's aura";
}

TEST(OracoolHeroChunks, PassiveSlotsRoundTrip)
{
	Players.resize(1);
	devilution::Player &source = Players[0];
	source = {};
	source._pClass = HeroClass::Warrior;
	source._pLevel = 40;

	oracool::ClassTreeSkill page[oracool::ClassTreeSkillCount];
	const size_t count = oracool::BuildClassTreePage(HeroClass::Warrior,
	    oracool::PassiveSkillsPage, page);
	ASSERT_GE(count, 3u);
	ASSERT_TRUE(oracool::SetPassiveSlot(source, 0, page[0]));
	ASSERT_TRUE(oracool::SetPassiveSlot(source, 2, page[2]));

	const std::vector<uint8_t> tail = oracool::BuildHeroChunkTail(source);

	devilution::Player target {};
	target._pClass = HeroClass::Warrior;
	target._pLevel = 40;
	// Stale state the apply must clear, not merge into: a slot the saved character did not fill
	// must come back EMPTY, or loading a hero would inherit whatever the last one was running.
	target._pPassiveSlots[1] = 0;
	oracool::ApplyHeroChunks(target, tail.data(), tail.size());

	EXPECT_EQ(oracool::PassiveInSlot(target, 0), page[0]);
	EXPECT_EQ(oracool::PassiveInSlot(target, 1), oracool::ClassTreeSkill::None)
	    << "a slot the save left empty came back full";
	EXPECT_EQ(oracool::PassiveInSlot(target, 2), page[2]);
	EXPECT_EQ(oracool::PassiveInSlot(target, 3), oracool::ClassTreeSkill::None);
}

// The chunk is what carries the real value, but the fixed struct is still written for a reader that
// has no tail. It must come back CLAMPED rather than wrapped: losing points is bad, and silently
// turning 490 into 234 is worse, because 234 looks like a number somebody meant.
TEST(OracoolHeroChunks, StatPointsFixedFieldClampsInsteadOfWrapping)
{
	Players.resize(1);
	devilution::Player &source = Players[0];
	source = {};
	source._pClass = HeroClass::Warrior;
	source._pStatPts = 490;

	PlayerPack packed {};
	PackPlayer(packed, source);
	EXPECT_EQ(packed.pStatPts, 255) << "the fixed byte wrapped 490 instead of clamping it";
}

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
/**
 * @brief A COMPLETE fresh hero - the whole Player, not the handful of fields a test happens to read.
 *
 * External audit of v1.9.92, finding 7. These helpers used to set class, level, points and the two
 * investment arrays and leave everything else as the previous test had left it. That is invisible
 * under CTest, which runs each TEST as its own process, and it bites the moment the binary is run as
 * one process:
 *
 *     oracool_audit_test.exe --gtest_shuffle --gtest_random_seed=92531 --gtest_repeat=20
 *
 * failed AuraNeedsAPointBeforeItCanBurn on iteration 1 - expected Might, got None. Not an aura bug:
 * GetActiveClassAura deliberately suppresses an aura for `_pmode == PM_DEATH`, and for a player with
 * positive maximum health and nonpositive current health. A preceding test had left a corpse, and
 * "fresh Paladin" inherited it.
 *
 * So the object is reset outright and given an explicitly LIVING baseline. A test that means to
 * examine a dead player must now say so, which is the right way round.
 */
devilution::Player &FreshHero(HeroClass heroClass, int unspent = 40)
{
	Players.resize(1);
	MyPlayer = &Players[0];
	devilution::Player &player = Players[0];
	player = {};
	player._pClass = heroClass;
	// Past the seventh tier's level 36, so nothing in any class's table is gated on level.
	player._pLevel = 50;
	player._pUnspentSkillPoints = static_cast<uint16_t>(unspent);
	player._pOracoolActiveAura = 0xFF;
	// ALIVE, and standing. Both halves matter - see the note above.
	player._pmode = PM_STAND;
	player._pMaxHP = player._pHitPoints = 100 << 6;
	player._pMaxHPBase = player._pHPBase = 100 << 6;
	std::memset(player._pSkillInvestment, 0, sizeof(player._pSkillInvestment));
	std::memset(player._pClassTreeInvestment, 0, sizeof(player._pClassTreeInvestment));
	return player;
}

/** @brief FreshHero as the Paladin, at the level its own tests were written against. */
devilution::Player &FreshPaladin(int unspent = 40)
{
	devilution::Player &player = FreshHero(HeroClass::Warrior, unspent);
	player._pLevel = 30;
	return player;
}


// Torment shipped byte-identical to Hell, so the fourth difficulty asked nothing the third had not
// already asked. Hell's resistances harden into immunities there - the same step Nightmare-to-Hell
// takes, taken once more.
TEST(OracoolAudit, TormentHardensHellResistancesIntoImmunities)
{
	MonsterData data {};
	data.monsterClass = MonsterClass::Demon; // not undead by default - see the Nightmare test
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
		// Each kind is turned on the way that kind is actually turned on, because a row that is
		// merely OFF would pass this test without proving anything.
		if (oracool::IsPassiveSkillRow(skill)) {
			// A Passive Skills row takes no points at all (2026-08-25); it is live when slotted.
			ASSERT_FALSE(oracool::InvestClassTreePoint(player, skill))
			    << _(data.name) << " took a skill point - the passive page is meant to be free";
			ASSERT_TRUE(oracool::SetPassiveSlot(player, 0, skill))
			    << _(data.name) << " would not go into a slot at level 50";
		} else {
			ASSERT_TRUE(oracool::InvestClassTreePoint(player, skill))
			    << _(data.name) << " could not take a point at level 50 with points in hand";
			// An aura contributes only while it burns, so an unlit one would pass trivially.
			if (data.kind == oracool::ClassTreeKind::Aura)
				ASSERT_TRUE(oracool::ToggleClassAura(player, skill)) << _(data.name) << " would not light";
		}

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

// ---------------------------------------------------------------------------------------------
// The Passive Skills page (2026-08-25). Diablo III-shaped placeholders: named, inert, one rank.
// ---------------------------------------------------------------------------------------------

TEST(OracoolClassTree, EveryClassHasAPassiveSkillsPageAndEveryRowOnItIsAnInertSingleRankPassive)
{
	constexpr int PassivePage = 3;
	oracool::ClassTreeSkill skills[oracool::ClassTreeSkillCount];
	for (const HeroClass heroClass : { HeroClass::Warrior, HeroClass::Barbarian,
	         HeroClass::Sorcerer, HeroClass::Rogue, HeroClass::Bard, HeroClass::Monk }) {
		const size_t count = oracool::BuildClassTreePage(heroClass, PassivePage, skills);
		EXPECT_GE(count, 18u) << "a class lost its passive page";
		EXPECT_EQ(oracool::GetClassTreePageName(heroClass, PassivePage), "PASSIVE SKILLS");
		for (size_t i = 0; i < count; i++) {
			const oracool::ClassTreeSkillData &data = oracool::GetClassTreeSkillData(skills[i]);
			EXPECT_EQ(data.kind, oracool::ClassTreeKind::Passive) << data.name;
			// One rank, which is what a D3 passive is: you have it or you do not. The tree's
			// default of 0 would mean MaxTreeInvestment, i.e. 98 ranks of nothing.
			EXPECT_EQ(oracool::ClassTreeMaxRank(skills[i]), 1) << data.name;
			// Inert on purpose - these are placeholders. If one of them is ever built this
			// assertion is the reminder to take it off the list rather than silently widen it.
			EXPECT_FALSE(data.implemented) << data.name << " claims to be built";
			EXPECT_EQ(data.spellId, SpellID::Invalid) << data.name;
		}
	}
}

TEST(OracoolClassTree, AddingThePassivePagesMovedNoExistingSkillsSaveSlot)
{
	// The invariant the whole append order exists to protect. ClassTreeIconIndex is simultaneously
	// a skill's frame in its class icon strip AND its slot in Player::_pClassTreeInvestment, so a
	// row inserted anywhere but the END of a class block silently reassigns points a live character
	// has already paid - and nothing about that fails a build.
	//
	// Spot-pinned at the boundaries that would actually move: the first and last of the Paladin's
	// two aura pages, the appended pair that came before the passives, and the first row of each
	// other class.
	EXPECT_EQ(oracool::ClassTreeIconIndex(oracool::ClassTreeSkill::Might), 9);
	EXPECT_EQ(oracool::ClassTreeIconIndex(oracool::ClassTreeSkill::Conviction), 18);
	EXPECT_EQ(oracool::ClassTreeIconIndex(oracool::ClassTreeSkill::Prayer), 19);
	EXPECT_EQ(oracool::ClassTreeIconIndex(oracool::ClassTreeSkill::Salvation), 28);
	EXPECT_EQ(oracool::ClassTreeIconIndex(oracool::ClassTreeSkill::HammerOfFaith), 29);
	EXPECT_EQ(oracool::ClassTreeIconIndex(oracool::ClassTreeSkill::BlessedShield), 30);
	// ...and the passives start immediately after, at the first free slot.
	EXPECT_EQ(oracool::ClassTreeIconIndex(oracool::ClassTreeSkill::HeavenlyStrength), 31);

	EXPECT_EQ(oracool::ClassTreeIconIndex(oracool::ClassTreeSkill::Bash), 0);
	EXPECT_EQ(oracool::ClassTreeIconIndex(oracool::ClassTreeSkill::IceBolt), 0);
	EXPECT_EQ(oracool::ClassTreeIconIndex(oracool::ClassTreeSkill::MagicArrow), 0);
	EXPECT_EQ(oracool::ClassTreeIconIndex(oracool::ClassTreeSkill::MelodyOfLife), 0);
	EXPECT_EQ(oracool::ClassTreeIconIndex(oracool::ClassTreeSkill::SweepingReed), 0);

	// Every slot must still fit the array it indexes. This is the bound that had to grow from 32.
	for (size_t i = 0; i < oracool::ClassTreeSkillCount; i++) {
		const auto skill = static_cast<oracool::ClassTreeSkill>(i);
		EXPECT_LT(static_cast<size_t>(oracool::ClassTreeIconIndex(skill)),
		    oracool::MaxSkillsPerClass)
		    << oracool::GetClassTreeSkillData(skill).name << " has no investment slot";
	}
	EXPECT_LE(oracool::MaxSkillsPerClass, std::size(devilution::Player {}._pClassTreeInvestment))
	    << "MaxSkillsPerClass indexes past the array it indexes";
}

TEST(OracoolClassTree, PassivesCostNoPointsAndArriveOneEveryEvenLevel)
{
	devilution::Player &player = FreshPaladin(1);
	player._pClass = HeroClass::Warrior;
	player._pUnspentSkillPoints = 50;

	oracool::ClassTreeSkill page[oracool::ClassTreeSkillCount];
	const size_t count = oracool::BuildClassTreePage(HeroClass::Warrior,
	    oracool::PassiveSkillsPage, page);
	ASSERT_GT(count, 0u);

	for (size_t i = 0; i < count; i++) {
		// Grid reading order is unlock order: the nth passive arrives at level 2n+2.
		EXPECT_EQ(oracool::PassiveSkillRequiredLevel(page[i]), static_cast<int>(2 * (i + 1)))
		    << oracool::GetClassTreeSkillData(page[i]).name;
	}

	// Free, and not merely cheap. A point must not be spendable on one at any level.
	const oracool::ClassTreeSkill first = page[0];
	player._pLevel = 99;
	EXPECT_FALSE(oracool::CanInvestClassTreePoint(player, first));
	EXPECT_FALSE(oracool::InvestClassTreePoint(player, first));
	EXPECT_EQ(player._pUnspentSkillPoints, 50) << "a passive took a skill point";

	// Automatic: level is the whole gate, and it is per skill rather than per tier.
	player._pLevel = 1;
	EXPECT_FALSE(oracool::IsClassTreeSkillUnlocked(player, first));
	player._pLevel = 2;
	EXPECT_TRUE(oracool::IsClassTreeSkillUnlocked(player, first));
	// The three cells of tier 0 open at 2, 4 and 6 - the tier is the row, not the gate.
	ASSERT_GE(count, 3u);
	EXPECT_FALSE(oracool::IsClassTreeSkillUnlocked(player, page[2]));
	player._pLevel = 6;
	EXPECT_TRUE(oracool::IsClassTreeSkillUnlocked(player, page[2]));
}

TEST(OracoolClassTree, TheOlderDiabloTwoPassivesStillCostPoints)
{
	// The scope line the user drew (2026-08-25): only the new page is free. The Barbarian's Combat
	// Masteries are Kind::Passive too, and in Diablo II investing deeper IS the mechanic - so if
	// "passives are free" had been scoped by KIND rather than by PAGE, this is what it would have
	// silently taken away.
	devilution::Player &player = FreshPaladin(30);
	player._pClass = HeroClass::Barbarian;
	player._pUnspentSkillPoints = 5;
	EXPECT_FALSE(oracool::IsPassiveSkillRow(oracool::ClassTreeSkill::SwordMastery));
	EXPECT_TRUE(oracool::CanInvestClassTreePoint(player, oracool::ClassTreeSkill::SwordMastery));
	EXPECT_TRUE(oracool::InvestClassTreePoint(player, oracool::ClassTreeSkill::SwordMastery));
	EXPECT_EQ(oracool::ClassTreeInvestment(player, oracool::ClassTreeSkill::SwordMastery), 1);
	EXPECT_EQ(player._pUnspentSkillPoints, 4);
}

TEST(OracoolClassTree, FourPassiveSlotsOpenAtOneTenTwentyThirty)
{
	devilution::Player &player = FreshPaladin(1);
	player._pClass = HeroClass::Warrior;

	EXPECT_EQ(oracool::PassiveSlotRequiredLevel(0), 1);
	EXPECT_EQ(oracool::PassiveSlotRequiredLevel(1), 10);
	EXPECT_EQ(oracool::PassiveSlotRequiredLevel(2), 20);
	EXPECT_EQ(oracool::PassiveSlotRequiredLevel(3), 30);

	player._pLevel = 1;
	EXPECT_EQ(oracool::UnlockedPassiveSlotCount(player), 1);
	player._pLevel = 9;
	EXPECT_EQ(oracool::UnlockedPassiveSlotCount(player), 1);
	player._pLevel = 10;
	EXPECT_EQ(oracool::UnlockedPassiveSlotCount(player), 2);
	player._pLevel = 30;
	EXPECT_EQ(oracool::UnlockedPassiveSlotCount(player), 4);
}

TEST(OracoolClassTree, AnUnslottedPassiveIsInactiveAndASlotRefusesWhatItCannotHold)
{
	devilution::Player &player = FreshPaladin(1);
	player._pClass = HeroClass::Warrior;
	player._pLevel = 40;

	oracool::ClassTreeSkill page[oracool::ClassTreeSkillCount];
	const size_t count = oracool::BuildClassTreePage(HeroClass::Warrior,
	    oracool::PassiveSkillsPage, page);
	ASSERT_GE(count, 2u);
	const oracool::ClassTreeSkill a = page[0];
	const oracool::ClassTreeSkill b = page[1];

	// Learned but doing nothing - which is the whole point of the slots.
	EXPECT_TRUE(oracool::IsClassTreeSkillUnlocked(player, a));
	EXPECT_EQ(oracool::PassiveSlotOf(player, a), -1);

	EXPECT_TRUE(oracool::SetPassiveSlot(player, 0, a));
	EXPECT_EQ(oracool::PassiveSlotOf(player, a), 0);
	EXPECT_EQ(oracool::PassiveInSlot(player, 0), a);

	// The same passive twice would be a free doubling of whatever it eventually does.
	EXPECT_FALSE(oracool::SetPassiveSlot(player, 1, a)) << "one passive filled two slots";

	// Another class's passive, and a non-passive row, are both refused.
	EXPECT_FALSE(oracool::SetPassiveSlot(player, 1, oracool::ClassTreeSkill::PoundOfFlesh));
	EXPECT_FALSE(oracool::SetPassiveSlot(player, 1, oracool::ClassTreeSkill::Might));

	EXPECT_TRUE(oracool::SetPassiveSlot(player, 1, b));
	EXPECT_TRUE(oracool::ClearPassiveSlot(player, 0));
	EXPECT_EQ(oracool::PassiveSlotOf(player, a), -1);
	EXPECT_FALSE(oracool::ClearPassiveSlot(player, 0)) << "an empty slot reported a change";
}

TEST(OracoolClassTree, ASlotStopsHoldingWhatTheCharacterNoLongerQualifiesFor)
{
	// Read validation, not write validation. The slot byte survives in the save; the character's
	// level and class do not have to. A slot holding something they cannot have reads as EMPTY
	// rather than handing them a passive they have not earned.
	devilution::Player &player = FreshPaladin(1);
	player._pClass = HeroClass::Warrior;
	player._pLevel = 40;

	oracool::ClassTreeSkill page[oracool::ClassTreeSkillCount];
	const size_t count = oracool::BuildClassTreePage(HeroClass::Warrior,
	    oracool::PassiveSkillsPage, page);
	ASSERT_GE(count, 4u);
	const oracool::ClassTreeSkill deep = page[3]; // level 8
	ASSERT_TRUE(oracool::SetPassiveSlot(player, 0, deep));
	EXPECT_EQ(oracool::PassiveInSlot(player, 0), deep);

	player._pLevel = 2;
	EXPECT_EQ(oracool::PassiveInSlot(player, 0), oracool::ClassTreeSkill::None)
	    << "a slot handed out a passive the character has not reached";

	// A slot past the character's level holds nothing either, however it was filled.
	player._pLevel = 40;
	ASSERT_TRUE(oracool::ClearPassiveSlot(player, 0)); // it is still sitting in slot 0
	ASSERT_TRUE(oracool::SetPassiveSlot(player, 3, deep));
	player._pLevel = 29;
	EXPECT_EQ(oracool::PassiveInSlot(player, 3), oracool::ClassTreeSkill::None)
	    << "a locked slot was still live";
}

TEST(OracoolClassTree, ALockedPassiveSaysWhyRatherThanRefusingInSilence)
{
	// Audit finding, 2026-08-25. ClassTreeLockReason answered from the TIER, and a passive is not
	// gated by its tier - so for a tier-0 passive (tier level 1, real requirement 2) the "am I below
	// the tier" test was false and the function returned an EMPTY string. A locked row that refuses
	// a click and explains nothing is precisely what this function exists to prevent.
	devilution::Player &player = FreshPaladin(1);
	player._pClass = HeroClass::Warrior;
	player._pLevel = 1;

	oracool::ClassTreeSkill page[oracool::ClassTreeSkillCount];
	const size_t count = oracool::BuildClassTreePage(HeroClass::Warrior,
	    oracool::PassiveSkillsPage, page);
	ASSERT_GE(count, 19u - 1u);

	// Level 1, first passive needs 2: the tier would have said "you meet level 1" and gone quiet.
	ASSERT_FALSE(oracool::IsClassTreeSkillUnlocked(player, page[0]));
	EXPECT_FALSE(oracool::ClassTreeLockReason(player, page[0]).empty())
	    << "a locked passive refused without saying why";

	// And every locked passive, at every level, says something.
	for (int level : { 1, 5, 17, 35, 37 }) {
		player._pLevel = level;
		for (size_t i = 0; i < count; i++) {
			if (oracool::IsClassTreeSkillUnlocked(player, page[i]))
				continue;
			EXPECT_FALSE(oracool::ClassTreeLockReason(player, page[i]).empty())
			    << oracool::GetClassTreeSkillData(page[i]).name << " was silent at level " << level;
		}
	}
}

TEST(OracoolHeroChunks, AHeroSavedBeforePassiveSlotsExistedHasNoneSlotted)
{
	// The chunk is additive, so an older hero simply has no tag 12 - and then nothing writes to
	// _pPassiveSlots at all. It has to default to empty on its own, or a pre-1.9.46 character would
	// load running whatever skill index 0 happens to be, in all four slots at once.
	Players.resize(1);
	devilution::Player &source = Players[0];
	source = {};
	source._pClass = HeroClass::Warrior;
	source._pLevel = 40;

	// A tail with the passive chunk stripped is what an older build would have written.
	const std::vector<uint8_t> tail = oracool::BuildHeroChunkTail(source);
	devilution::Player target {};
	target._pClass = HeroClass::Warrior;
	target._pLevel = 40;
	oracool::ApplyHeroChunks(target, tail.data(), tail.size());
	for (int slot = 0; slot < static_cast<int>(oracool::PassiveSlotCount); slot++) {
		EXPECT_EQ(oracool::PassiveInSlot(target, slot), oracool::ClassTreeSkill::None)
		    << "slot " << slot << " came back filled on a character that never filled one";
	}

	// The value-initialised default is the thing being relied on, so pin it directly too.
	devilution::Player fresh {};
	for (const uint8_t value : fresh._pPassiveSlots)
		EXPECT_EQ(value, 0xFF) << "a fresh Player does not default to empty passive slots";
}

TEST(OracoolClassTree, AnAuraThatIsNotThisCharactersDoesNotBurn)
{
	// The burning aura persists as an ABSOLUTE ClassTreeSkill, and absolute values shift whenever a
	// class earlier in the enum gains rows - which the passive pages just did. A Bard or Monk saved
	// before the change decodes to some other class's row at that number.
	//
	// The guard turns that into "no aura" rather than "somebody else's aura".
	devilution::Player &player = FreshPaladin(30);
	player._pClass = HeroClass::Warrior;
	player._pOracoolActiveAura = static_cast<uint16_t>(oracool::ClassTreeSkill::MelodyOfLife);
	EXPECT_EQ(oracool::GetActiveClassAura(player), oracool::ClassTreeSkill::None)
	    << "a Paladin is burning one of the Bard's songs";

	// None itself must survive the widening: 0xFF was the old sentinel and is now a real row.
	player._pOracoolActiveAura = 0xFF;
	EXPECT_EQ(oracool::GetActiveClassAura(player), oracool::ClassTreeSkill::None)
	    << "the retired 0xFF sentinel now decodes to a real skill";
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

TEST(OracoolAudit, TheLowerHalfOfAnOpenWindowIsInterfaceNotWorld)
{
	// Audit finding, 2026-08-26, and the shape of it is worth stating: every one of the stale
	// hit-tests in this family compared against the VANILLA 320x352 slot while the window on screen
	// was 340x720. So the top of a window absorbed input and the bottom two thirds did not -
	// right-clicks cast through it, ground-item labels stayed live under it, and the controller
	// could not act on the lower stash rows.
	//
	// The assertion that matters is therefore not "inside the window" but "inside the part of the
	// window the old rect did not reach".
	// What is pinned here is the PREMISE, not the routing. IsOverAnyInterface reaches into live HUD
	// state and cannot run in a bare harness; the routing itself is only verifiable in play, and
	// that is recorded rather than faked.
	//
	// The premise is testable and is the thing that actually went wrong: these windows outgrew the
	// vanilla slot every stale hit-test was still comparing against. While that stays true, any
	// code testing SidePanelSize instead of the window's own rect is wrong by construction - which
	// is why the band below y=352 is where every one of these bugs lived.
	const Rectangle inventory = oracool::GetInventoryPanelRect();
	EXPECT_GT(inventory.size.height, SidePanelSize.height)
	    << "the inventory no longer outgrows the vanilla 320x352 slot";

	const Rectangle abilities = GetSpellBookPanelRect();
	EXPECT_GT(abilities.size.height, SidePanelSize.height)
	    << "the Abilities window no longer outgrows the vanilla slot";

	const Rectangle stash = GetStashPanelRect();
	EXPECT_GT(stash.size.height, SidePanelSize.height)
	    << "the stash no longer outgrows the vanilla slot - the controller fix assumed it does";
}

TEST(OracoolAudit, RelentlessAndImplacableResistKnockbackRatherThanDealingIt)
{
	using namespace devilution::oracool;
	// Audit finding, 2026-08-26, and an INVERSION rather than an omission. Both set
	// MFLAG_KNOCKBACK, whose comments said the point was to deny the player space. That flag lives
	// in the monster-hits-PLAYER path and means "this monster's blows knock YOU back" - so both
	// granted an offensive power nobody designed, and neither granted the immunity both described,
	// because the player's knockback runs through M_GetKnockback and never read the flag.
	Monster relentless {};
	relentless.lesserAffix = LesserUniqueAffix::Relentless;
	EXPECT_TRUE(IsKnockbackImmune(relentless)) << "a Relentless champion can still be shoved";

	Monster ordinary {};
	ordinary.lesserAffix = LesserUniqueAffix::None;
	EXPECT_FALSE(IsKnockbackImmune(ordinary)) << "an ordinary monster became immune";

	// Every other champion affix keeps its footing exactly as before - the immunity must be
	// Relentless's alone, or this fix would have quietly handed it to the whole family.
	for (int i = 0; i < static_cast<int>(LesserUniqueAffix::LAST) + 1; i++) {
		const auto affix = static_cast<LesserUniqueAffix>(i);
		if (affix == LesserUniqueAffix::Relentless || affix == LesserUniqueAffix::Dread)
			continue;
		Monster other {};
		other.lesserAffix = affix;
		EXPECT_FALSE(IsKnockbackImmune(other))
		    << "affix " << i << " gained knockback immunity it was never given";
	}

	// A Dread boss is immune only when its derived trait is Implacable. The trait comes from the
	// seed rather than a field, so this sweeps seeds until it finds one of each.
	bool sawImmune = false;
	bool sawVulnerable = false;
	for (int seed = 0; seed < 4096 && !(sawImmune && sawVulnerable); seed++) {
		Monster boss {};
		boss.lesserAffix = LesserUniqueAffix::Dread;
		boss.lesserNameSeed = static_cast<uint16_t>(seed);
		if (SecondaryTraitFor(boss) == BossTrait::Implacable)
			sawImmune = sawImmune || IsKnockbackImmune(boss);
		else
			sawVulnerable = sawVulnerable || !IsKnockbackImmune(boss);
	}
	EXPECT_TRUE(sawImmune) << "no Implacable boss resisted knockback";
	EXPECT_TRUE(sawVulnerable) << "every Dread boss resisted knockback, not just the Implacable one";
}

TEST(OracoolClassTree, ACorpseHasNoAura)
{
	// Audit finding, 2026-08-26. The class-tree tick runs unconditionally, so Prayer, Melody of Life
	// and the Healing Mantra regenerated a DEAD player's zero hit points while they lay in PM_DEATH,
	// and Sanctuary, Conviction, the ground ring and the looping hum all stayed live over the body.
	//
	// Guarded in GetActiveClassAura, which every one of those asks - so one answer suspends the
	// bonuses, the field, the picture and the sound together.
	devilution::Player &player = FreshPaladin();
	player._pMaxHP = 1000;
	player._pHitPoints = 1000;
	ASSERT_TRUE(oracool::InvestClassTreePoint(player, oracool::ClassTreeSkill::Might));
	ASSERT_TRUE(oracool::ToggleClassAura(player, oracool::ClassTreeSkill::Might));
	ASSERT_EQ(oracool::GetActiveClassAura(player), oracool::ClassTreeSkill::Might);

	player._pmode = PM_DEATH;
	EXPECT_EQ(oracool::GetActiveClassAura(player), oracool::ClassTreeSkill::None)
	    << "a corpse is still burning an aura";

	// And the totals go with it, which is the half that was actually changing numbers.
	oracool::ItemBonusTotals dead = {};
	oracool::ApplyClassTreeToTotals(player, dead);
	EXPECT_EQ(dead.bonusDamage, 0) << "a dead Paladin still had Might's damage";

	// SUSPENDED, not forgotten. Nothing is cleared, so it comes back on revival by itself - a
	// player who dies with an aura up should not have to relight it.
	player._pmode = PM_STAND;
	EXPECT_EQ(oracool::GetActiveClassAura(player), oracool::ClassTreeSkill::Might)
	    << "the aura did not come back after revival";

	// Zero health without death mode is the tick or two before StartPlayerKill runs, and counts.
	//
	// Set again here rather than relying on the values at the top: ToggleClassAura now recalculates
	// the character (that is the fix for the bonuses-outlive-the-aura bug), and a recalculation
	// derives _pMaxHP from _pMaxHPBase, which this fixture leaves at zero. Stating the state where
	// it is being tested is more honest than a setup line three assertions away.
	player._pMaxHP = 1000;
	player._pHitPoints = 0;
	EXPECT_EQ(oracool::GetActiveClassAura(player), oracool::ClassTreeSkill::None);

	// But a fixture that never set health up is NOT dead - "no health left" and "health never
	// configured" are different states, and reading them as one declared every test a corpse.
	player._pMaxHP = 0;
	EXPECT_EQ(oracool::GetActiveClassAura(player), oracool::ClassTreeSkill::Might);
}

TEST(OracoolClassTree, PuttingAnAuraOutTakesItsBonusesWithIt)
{
	// Audit finding, 2026-08-26. ClearClassAuraForRightButton cleared the state and stopped the
	// sound and left the cached totals alone, and all FOUR of its callers forgot to recalculate -
	// so readying a spell over a lit Might put the ring out and left the damage bonus running.
	devilution::Player &player = FreshPaladin();
	player._pMaxHP = 1000;
	player._pHitPoints = 1000;
	ASSERT_TRUE(oracool::InvestClassTreePoint(player, oracool::ClassTreeSkill::Might));
	ASSERT_TRUE(oracool::ToggleClassAura(player, oracool::ClassTreeSkill::Might));

	// Asserted on the player's CACHED stat, not on a fresh ApplyClassTreeToTotals.
	//
	// The first version of this test did the latter and passed with the fix removed, which is to
	// say it tested nothing: ApplyClassTreeToTotals recomputes from scratch, so it can never
	// observe a stale cache. The bug was always about the cache - _pIBonusDam keeps the aura's
	// contribution until something recalculates - so the cache is what has to be looked at.
	CalcPlrInv(player, false);
	ASSERT_GT(player._pIBonusDam, 0) << "Might contributed nothing even while burning";

	oracool::ClearClassAuraForRightButton(player);
	EXPECT_EQ(oracool::GetActiveClassAura(player), oracool::ClassTreeSkill::None);
	EXPECT_EQ(player._pIBonusDam, 0)
	    << "the aura went out but its damage bonus was still on the character";
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
	// The WHOLE player, not the five fields this test reads. It reset skill investment and spell
	// levels but not _pClassTreeInvestment, which RespecCost also prices - so a preceding test that
	// spent tree points made the respec cost 1500 where this expects the 1000 floor. Failed on three
	// iterations in twenty at seed 37473 (external audit of v1.9.92, finding 7).
	player = {};
	std::memset(player._pClassTreeInvestment, 0, sizeof(player._pClassTreeInvestment));
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

TEST(OracoolSkillPoints, ZealStrikesAreSkillLevelDriven)
{
	Players.resize(1);
	devilution::Player &player = Players[0];
	player._pLevel = 20;
	std::memset(player._pSkillInvestment, 0, sizeof(player._pSkillInvestment));
	player._pISplLvlAdd = 0;
	std::memset(player._pSplLvl, 0, sizeof(player._pSplLvl));

	const auto zeal = static_cast<size_t>(oracool::GetPaladinSkillData(oracool::PaladinSkill::Zeal).spellId);
	EXPECT_EQ(oracool::ZealStrikeCount(player), 0)
	    << "an uninvested Zeal is no Zeal, whatever the character level";
	player._pSkillInvestment[zeal] = 5;
	EXPECT_EQ(oracool::ZealStrikeCount(player), 4) << "skill levels 1, 3 and 5 each add a strike";
	player._pSkillInvestment[zeal] = 20;
	EXPECT_EQ(oracool::ZealStrikeCount(player), 4) << "the cap holds";

	// The other half of "all benefits come from SKILL levels": an item that grants +spell levels
	// deepens Zeal exactly as investing does, because ZealSkillLevel asks GetSpellLevel.
	player._pSkillInvestment[zeal] = 1;
	EXPECT_EQ(oracool::ZealStrikeCount(player), 2) << "one point, one rung";
	player._pISplLvlAdd = 4;
	EXPECT_EQ(oracool::ZealStrikeCount(player), 4)
	    << "+4 spell levels from gear reaches rung 5 as surely as four more points would";
	player._pISplLvlAdd = 0;

	player._pLevel = 1;
	EXPECT_EQ(oracool::ZealStrikeCount(player), 0)
	    << "the character level is still the GATE on the skill existing at all";
	player._pSkillInvestment[zeal] = 0;
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
// ---------------------------------------------------------------------------------------------
// Audit finding, 2026-08-26 (critical). Every in-place item transformation - Reforge, Reroll,
// Enrich, Awaken, Ennoble - rerolls the SAME Item object. GetItemAttrs resets the base fields but
// never touches the _iPL* bonus fields, and SaveItemPower applies almost every bonus with +=. So
// each reroll stacked its predecessor's bonuses invisibly: the tooltip lists only the current
// affixes while the character carries the sum of every roll the item has ever had.
//
// Rerolled here rather than asserted against a fixture, because a fixture cannot show accumulation.
// ---------------------------------------------------------------------------------------------
TEST(OracoolCrafting, RerollingAnItemDoesNotAccumulateGhostBonuses)
{
	Players.resize(1);
	MyPlayer = &Players[0];
	devilution::Player &player = Players[0];
	player = {};
	player._pClass = HeroClass::Warrior;
	player._pLevel = 30;

	// A real base item, found rather than named: the first ordinary sword in the table. Picking it
	// by property rather than by an IDI_ constant keeps the test working when the table moves.
	_item_indexes swordIdx = IDI_NONE;
	for (std::underlying_type_t<_item_indexes> i = 0; i <= IDI_LAST; i++) {
		const ItemData &row = AllItemsList[i];
		if (row.itype == ItemType::Sword && row.iClass == ICLASS_WEAPON && row.iMinDam > 0) {
			swordIdx = static_cast<_item_indexes>(i);
			break;
		}
	}
	ASSERT_NE(swordIdx, IDI_NONE) << "no ordinary sword in the item table";

	// Tested at GetItemAttrs rather than through Reforge itself, and that is not a compromise -
	// it is the exact seam. Every one of those recipes rerolls in place through SetupAllItems,
	// whose ONLY reset of the item is this call; SaveItemPower then applies each new affix with
	// +=. So "does GetItemAttrs leave a previous life's bonuses behind" IS the bug, stated
	// directly. (Driving Reforge end-to-end needs live dungeon state and faults in the harness.)
	devilution::Item blade {};

	// A previous life: the bonuses an earlier roll would have left on the object.
	blade._iPLDam = 150;
	blade._iPLToHit = 150;
	blade._iPLStr = 40;
	blade._iPLVit = 40;
	blade._iPLLight = 8;
	blade._iPLFR = 60;
	blade._iPLMana = 500;
	blade._iPLHP = 500;
	blade._iFlags = ItemSpecialEffect::FastHitRecovery;

	GetItemAttrs(blade, swordIdx, 30);

	EXPECT_EQ(blade._iPLDam, 0) << "a reroll keeps the previous roll's damage bonus";
	EXPECT_EQ(blade._iPLToHit, 0) << "a reroll keeps the previous roll's to-hit";
	EXPECT_EQ(blade._iPLStr, 0) << "a reroll keeps the previous roll's strength";
	EXPECT_EQ(blade._iPLVit, 0) << "a reroll keeps the previous roll's vitality";
	EXPECT_EQ(blade._iPLLight, 0) << "a reroll keeps the previous roll's light radius";
	EXPECT_EQ(blade._iPLFR, 0) << "a reroll keeps the previous roll's fire resistance";
	EXPECT_EQ(blade._iPLMana, 0) << "a reroll keeps the previous roll's mana";
	EXPECT_EQ(blade._iPLHP, 0) << "a reroll keeps the previous roll's life";
}

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
	// Against the GRID, because the grid is the only place a recipe runs (v1.9.142). This was the
	// backpack path's test until then; the assertion it carries is about the rune LADDER, not about
	// where the runes were standing, so it moved rather than being deleted with the path.
	Players.resize(1);
	MyPlayer = &Players[0];
	Players[0] = {};

	devilution::Item grid[oracool::LevskiGridSlots] = {};
	for (int i = 0; i < 2; i++) {
		InitializeItem(grid[i], IDI_ORACOOL_RUNE_EL);
		grid[i]._itype = ItemType::Misc;
	}

	ASSERT_TRUE(oracool::CanCraftFromLevskiGrid(grid, 1));
	EXPECT_FALSE(oracool::TransmuteLevskiGridWith(grid, 1).empty());

	int nextCount = 0;
	int elCount = 0;
	for (const devilution::Item &cell : grid) {
		if (cell.isEmpty())
			continue;
		// Diablo II's ladder puts ELD above El, not Tir. The old expectation here encoded the bug
		// this test now guards against: crafting used to walk the ENUM (index + 1), which skips
		// Eld entirely because the five v1.7.8 runes and the 28 appended ones are separate islands.
		if (cell.IDidx == IDI_ORACOOL_RUNE_ELD)
			nextCount += std::max(1, cell.stackCount());
		if (cell.IDidx == IDI_ORACOOL_RUNE_EL)
			elCount += std::max(1, cell.stackCount());
	}
	EXPECT_EQ(nextCount, 1) << "two El runes should have become one Eld";
	EXPECT_EQ(elCount, 0) << "the consumed pair survived";
}

// External audit, 2026-08-25: recipes counted occupied SLOTS while their materials stack, which was
// wrong in both directions. A single slot holding two El runes did not satisfy "two identical runes"
// at all, and when two separate slots did satisfy it, both whole stacks were destroyed to make one
// rune. These two pin each direction; both fail against the pre-fix code.
// Audit finding, 2026-08-26. The two tests below this one prove the BACKPACK path counts stack
// units correctly. The monument GRID path did not, and it was wrong in both directions at once:
// LargestSameKindGridGroup counted occupied SLOTS, so one stack of two runes was refused, and the
// consume then cleared whole slots, so three stacks of five gems lost all fifteen to a recipe that
// costs three.
//
// These are the same two scenarios against the grid, which is where nobody was looking.
TEST(OracoolCrafting, MonumentAscendRunes_OneGridStackSatisfiesThePair)
{
	Players.resize(1);
	MyPlayer = &Players[0];
	Players[0] = {};

	devilution::Item grid[oracool::LevskiGridSlots] = {};
	InitializeItem(grid[0], IDI_ORACOOL_RUNE_EL);
	grid[0]._itype = ItemType::Misc;
	grid[0].setStackCount(2);

	EXPECT_TRUE(oracool::CanCraftFromLevskiGrid(grid, 1))
	    << "a grid stack of two runes is still two runes";
}

TEST(OracoolCrafting, MonumentRefine_SurplusUnitsSurviveTheCraft)
{
	Players.resize(1);
	MyPlayer = &Players[0];
	Players[0] = {};

	// Two stacks of five El runes. Ascend Runes costs two; thirteen must survive.
	devilution::Item grid[oracool::LevskiGridSlots] = {};
	InitializeItem(grid[0], IDI_ORACOOL_RUNE_EL);
	grid[0]._itype = ItemType::Misc;
	grid[0].setStackCount(5);
	InitializeItem(grid[1], IDI_ORACOOL_RUNE_EL);
	grid[1]._itype = ItemType::Misc;
	grid[1].setStackCount(5);

	ASSERT_TRUE(oracool::CanCraftFromLevskiGrid(grid, 1));
	const std::string made = oracool::TransmuteLevskiGridWith(grid, 1);
	EXPECT_FALSE(made.empty()) << "the craft did not run";

	int surviving = 0;
	for (const devilution::Item &cell : grid) {
		if (!cell.isEmpty() && cell.IDidx == IDI_ORACOOL_RUNE_EL)
			surviving += std::max(1, cell.stackCount());
	}
	EXPECT_EQ(surviving, 8) << "the craft destroyed surplus runes it did not charge for";
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
	MyPlayer = &Players[0];
	Players[0] = {};

	// On the grid, like everything else since v1.9.142. What is being pinned is the KIND rule and
	// the top of the rune ladder, neither of which was ever about the backpack.
	devilution::Item grid[oracool::LevskiGridSlots] = {};
	const int gems[3] = { IDI_ORACOOL_GEM_RUBY, IDI_ORACOOL_GEM_SAPPHIRE, IDI_ORACOOL_GEM_TOPAZ };
	for (int i = 0; i < 3; i++) {
		InitializeItem(grid[i], static_cast<_item_indexes>(gems[i]));
		grid[i]._itype = ItemType::Misc;
	}
	EXPECT_FALSE(oracool::CanCraftFromLevskiGrid(grid, 0)) << "three DIFFERENT gems satisfied 'three of one kind'";

	InitializeItem(grid[1], IDI_ORACOOL_GEM_RUBY);
	InitializeItem(grid[2], IDI_ORACOOL_GEM_RUBY);
	grid[1]._itype = ItemType::Misc;
	grid[2]._itype = ItemType::Misc;
	EXPECT_TRUE(oracool::CanCraftFromLevskiGrid(grid, 0));

	// Zod pairs must never ascend - Zod is the top of the 33-rune ladder. This used to say Sol,
	// which was only the top while five of the thirty-three existed.
	for (devilution::Item &cell : grid)
		cell = {};
	for (int i = 0; i < 2; i++) {
		InitializeItem(grid[i], IDI_ORACOOL_RUNE_ZOD);
		grid[i]._itype = ItemType::Misc;
	}
	EXPECT_FALSE(oracool::CanCraftFromLevskiGrid(grid, 1)) << "a Zod pair offered an ascension past the ladder's top";

	// ...and a pair below the top still does ascend.
	for (int i = 0; i < 2; i++) {
		InitializeItem(grid[i], IDI_ORACOOL_RUNE_SOL);
		grid[i]._itype = ItemType::Misc;
	}
	EXPECT_TRUE(oracool::CanCraftFromLevskiGrid(grid, 1)) << "Sol is no longer the top and must ascend";
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
 * Was 73 of 94 on the morning of 2026-08-21. Amulets and rings had no resolvable base because the
 * vanilla rows are anonymous and naming them would have meant inserting into _item_indexes, which is
 * positional save format - ItemMiscIdIdx answers the same question without touching the enum. The
 * last two, relic and cloak, were then re-slotted onto bracers and legs rather than growing the
 * paperdoll for two items.
 *
 * Pinned as an exact count rather than "most of them", because the failure this guards is silent:
 * an unspawnable piece is simply never rolled, so a set quietly becomes uncompletable and nothing
 * says so.
 */
TEST(OracoolItemSets, EverySetPieceHasASpawnableBase)
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
				// No slot may fail any more. Relic and cloak were the last two, and they now
				// resolve onto bracers and legs.
				ADD_FAILURE() << def.name << " has no base for slot '" << def.slot << "'";
			}
		}
	}
	EXPECT_EQ(spawnable, 94) << "every set piece should resolve to a base";
	EXPECT_EQ(unspawnable, 0);
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
		// Grouped by the RESOLVED equip location, not by the design's slot word. Those two stopped
		// agreeing the moment relic and cloak were re-slotted onto bracers and legs (2026-08-21) -
		// and grouping by the word would have hidden exactly the trap that re-slotting had to avoid:
		// two pieces named for different slots that land in the same one and cannot be worn together.
		std::map<int, int> buildableByLoc;
		for (int i = 0; i < set.itemCount; i++) {
			const oracool::SetItemDefinition &def = oracool::ItemSetItems[set.firstItem + i];
			const int base = oracool::BaseItemForSetSlot(def.slot);
			if (base < 0)
				continue;
			// A two-hander CONSUMES both hand slots, so it would have to subtract capacity rather
			// than add a piece. No set base is two-handed today - main_hand resolves to a short
			// sword - and this asserts that rather than assuming it, because the capacity model
			// below would quietly over-count by one if it ever changed.
			ASSERT_NE(AllItemsList[base].iLoc, ILOC_TWOHAND)
			    << def.name << " resolves to a two-handed base; this test's slot model cannot count that";
			buildableByLoc[static_cast<int>(AllItemsList[base].iLoc)]++;
		}
		int wearable = 0;
		for (const auto &[loc, count] : buildableByLoc) {
			// TWO locations hold two items: rings, and hands. A set's main_hand and off_hand
			// pieces both resolve to ILOC_ONEHAND - a shield is a one-hand item - so counting that
			// as one slot understated four sets by exactly one piece and made this test fail on
			// data that was correct. Hard-coded rather than derived so a third such location fails
			// here and gets considered, instead of being quietly over-counted.
			const int loc2 = static_cast<int>(ILOC_ONEHAND);
			const int capacity = (loc == static_cast<int>(ILOC_RING) || loc == loc2) ? 2 : 1;
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

/**
 * The jewel family, end to end.
 *
 * Jewels are the third socket family (v1.9.8). Every earlier family taught the same lesson twice
 * over, so this test pins all four of the places a new socketable silently falls out of the game
 * while the build stays green:
 *
 *  - it is EXCLUDED from the seeded droppable pool. That pool is save format - UnPackItem replays
 *    a dungeon item's seed through the same walk to recover its index - so a family that joins it
 *    re-routes every seeded recreation. Runes, gems, charms and set items all drop through their
 *    own hooks for this reason, and a jewel must too.
 *  - it has a cursor of its own, distinct from every other item's, or fifteen jewels share one
 *    picture and the icon strip is a frame short somewhere.
 *  - it DOES something in a host. A socketable whose effect table row is empty inserts fine, reads
 *    fine and changes nothing, which is the failure Shael shipped with for a fortnight.
 *  - it declines salvage. The salvage-all buttons must never eat a hoard of socketables.
 *
 * The fifteen are five families x three grades, emitted grade-major by tools/GenJewels.ps1.
 */
TEST(OracoolAudit, JewelsAreAWholeSocketFamily)
{
	constexpr int First = IDI_ORACOOL_JEWEL_FERVOR_FLAWED;
	constexpr int Last = IDI_ORACOOL_JEWEL_WARDING_RADIANT;
	EXPECT_EQ(Last - First + 1, 15) << "the jewel family is not five families by three grades";

	std::set<int> cursors;
	int seen = 0;
	for (int i = IDI_GOLD; i <= IDI_LAST; i++) {
		if (!IsOracoolJewelIdx(i))
			continue;
		seen++;
		EXPECT_GE(i, First);
		EXPECT_LE(i, Last);
		const ItemData &data = AllItemsList[i];

		// A picture nobody else has.
		EXPECT_TRUE(cursors.insert(data.iCurs).second)
		    << data.iName << " shares its cursor with another jewel";
		EXPECT_LE(data.iCurs, ICURS_ORACOOL_LAST)
		    << data.iName << " points past the last frame of the icon strip";

		// An effect in at least one host. Unlike a rune, a jewel is allowed to do nothing in some
		// hosts - a Jewel of Focus in a shield is mana and that is all - but doing nothing in all
		// three would make it a decoration.
		const bool anyHost = !oracool::GemHostEffectLine(static_cast<uint16_t>(i), oracool::SocketHost::Weapon).empty()
		    || !oracool::GemHostEffectLine(static_cast<uint16_t>(i), oracool::SocketHost::Shield).empty()
		    || !oracool::GemHostEffectLine(static_cast<uint16_t>(i), oracool::SocketHost::Armor).empty();
		EXPECT_TRUE(anyHost) << data.iName << " has no effect in any host - it is an inert socketable";

		// Salvage declines it, whatever bucket logic decides about ICLASS_MISC.
		devilution::Item jewel;
		InitializeItem(jewel, static_cast<_item_indexes>(i));
		EXPECT_FALSE(oracool::IsSalvageable(jewel)) << data.iName << " can be salvaged";
	}
	EXPECT_EQ(seen, 15) << "IsOracoolJewelIdx does not recognise exactly the fifteen jewels";

	// The pool exclusion at items.cpp is one OR-chain of family predicates, so it is only correct
	// while the predicates stay disjoint: a jewel that also answered yes to IsOracoolGemIdx would
	// be excluded here and then double-counted by the gem drop hook. Disjointness is the part a
	// unit test can actually hold - the exclusion itself lives inside a static walk - and it is
	// also the part that breaks, because every family so far has been a contiguous id range
	// appended after the last one, and the ranges are hand-written bounds.
	for (int i = IDI_GOLD; i <= IDI_LAST; i++) {
		if (!IsOracoolJewelIdx(i))
			continue;
		EXPECT_FALSE(IsOracoolGemIdx(i)) << AllItemsList[i].iName << " reads as a gem as well as a jewel";
		EXPECT_FALSE(IsOracoolRuneIdx(i)) << AllItemsList[i].iName << " reads as a rune as well as a jewel";
		EXPECT_FALSE(IsOracoolCharmIdx(i)) << AllItemsList[i].iName << " reads as a charm as well as a jewel";
		EXPECT_FALSE(IsOracoolSalvageIdx(i)) << AllItemsList[i].iName << " reads as a material as well as a jewel";
		EXPECT_FALSE(IsOracoolItemIdx(i)) << AllItemsList[i].iName << " reads as a set/base item as well as a jewel";
	}
}

/**
 * Levski's grid measures ROOM IN CELLS, and never loses an item measuring it wrong.
 *
 * The grid is 3x4 = twelve CELLS, and items sit in it by footprint - but the recipes operate on a
 * twelve-long ARRAY of items and have no idea of footprints. "Free the Sockets" is the one recipe
 * that gives back more than it takes, and it used to ask GridRoomAfter - a count of free array
 * entries - whether its stones would fit. A 2x3 breastplate holding six stones is ONE array entry
 * and SIX cells, so that check answered "eleven free" to a transmute needing thirteen cells, and
 * the repack afterwards discarded whatever would not fit. A rune stopped existing, silently.
 *
 * The boundary is not a corner case. MaxItemSockets is 6, so a fully socketed 2x3 host plus its
 * freed stones is 6 + 6 = exactly twelve cells: the worst case fills the grid with nothing to
 * spare, and one loose rune sharing the grid is already one cell too many.
 */
TEST(OracoolAudit, LevskiGridMeasuresRoomInCellsAndLosesNothing)
{
	using namespace devilution::oracool;

	// A real 2x3 base, found by measuring rather than by naming one - a hardcoded index is a
	// second source of truth about a size the art owns.
	_item_indexes bigIdx = IDI_NONE;
	for (int i = IDI_GOLD; i <= IDI_LAST; i++) {
		devilution::Item probe;
		InitializeItem(probe, static_cast<_item_indexes>(i));
		const Size size = GetInventorySize(probe);
		if (size.width == 2 && size.height == 3) {
			bigIdx = static_cast<_item_indexes>(i);
			break;
		}
	}
	ASSERT_NE(bigIdx, IDI_NONE) << "no 2x3 item exists to test the boundary with";

	devilution::Item big;
	InitializeItem(big, bigIdx);
	devilution::Item stone;
	InitializeItem(stone, IDI_ORACOOL_JEWEL_FERVOR_FLAWED);
	ASSERT_EQ(GetInventorySize(stone).width, 1);
	ASSERT_EQ(GetInventorySize(stone).height, 1);

	// Exactly twelve cells: 2x3 plus six singles. This must fit, or a six-socket item can never be
	// emptied at all.
	std::vector<devilution::Item> exact { big, stone, stone, stone, stone, stone, stone };
	EXPECT_TRUE(LevskiGridCanHold(exact.data(), static_cast<int>(exact.size())))
	    << "a 2x3 host and its six freed stones do not fit - the worst case is unrunnable";

	// One cell past it. Seven array entries became eight, but the point is that it is THIRTEEN
	// cells: a slot count would still say there is room.
	std::vector<devilution::Item> overfull = exact;
	overfull.push_back(stone);
	EXPECT_FALSE(LevskiGridCanHold(overfull.data(), static_cast<int>(overfull.size())))
	    << "thirteen cells fit in a twelve-cell grid - the check is still counting slots";

	// And the recipe itself refuses rather than freeing stones it cannot place. Six sockets filled,
	// plus one loose stone already in the grid: eight array entries, thirteen cells.
	devilution::Item grid[LevskiGridSlots];
	grid[0] = big;
	for (uint16_t &socketed : grid[0]._iSocketed)
		socketed = static_cast<uint16_t>(IDI_ORACOOL_JEWEL_FERVOR_FLAWED);
	grid[1] = stone;
	ASSERT_EQ(grid[0].socketedCount(), devilution::Item::MaxItemSockets);

	const std::string refusal = TransmuteLevskiGrid(grid);
	EXPECT_EQ(grid[0].socketedCount(), devilution::Item::MaxItemSockets)
	    << "the host was emptied into a grid that cannot hold the stones";
	EXPECT_FALSE(refusal.empty())
	    << "the refusal is silent - the player presses Transmute and is told nothing";

	// With the grid to itself the same host CAN be emptied, so the refusal above is about room and
	// not about the recipe being broken.
	devilution::Item room[LevskiGridSlots];
	room[0] = big;
	for (uint16_t &socketed : room[0]._iSocketed)
		socketed = static_cast<uint16_t>(IDI_ORACOOL_JEWEL_FERVOR_FLAWED);
	const std::string freed = TransmuteLevskiGrid(room);
	EXPECT_FALSE(freed.empty());
	EXPECT_EQ(room[0].socketedCount(), 0) << "the host kept its stones when there was room to free them";
	int stonesBack = 0;
	for (const devilution::Item &slot : room) {
		if (!slot.isEmpty() && IsOracoolJewelIdx(slot.IDidx))
			stonesBack++;
	}
	EXPECT_EQ(stonesBack, devilution::Item::MaxItemSockets) << "stones went missing on the way out";
}

/**
 * The named-set drop leans toward the set you are already collecting.
 *
 * Before v1.9.11 TrySpawnNamedSetPiece picked uniformly across every eligible piece of all fifteen
 * sets - roughly ninety of them. Holding five of the Ashen Saint's six made the sixth no likelier
 * than a piece of a set you had never seen, so completing a ladder was attrition rather than a
 * pursuit, and the cumulative bonus ladder - the entire reason a named set exists - was something
 * you finished by accident or not at all.
 *
 * Two things are pinned here. The WEIGHT RULE, which is pure arithmetic; and the fact that HELD
 * means held ANYWHERE - worn, carried, or in the stash. That last one is the part that would
 * silently half-work: counting only worn pieces biases against exactly the player this helps, the
 * one hoarding four pieces they cannot equip yet.
 */
TEST(OracoolAudit, NamedSetDropsLeanTowardTheSetYouAreCollecting)
{
	using namespace devilution::oracool;

	// Nothing held: every piece is equal, which is the old behaviour and must survive for a player
	// who has never seen a set item.
	EXPECT_EQ(SetPieceDropWeight(0, false), 1);

	// Held pieces of the set, but not this one: the boost.
	EXPECT_GT(SetPieceDropWeight(1, false), SetPieceDropWeight(0, false));
	EXPECT_GT(SetPieceDropWeight(4, false), SetPieceDropWeight(1, false))
	    << "the boost does not grow with progress - a nearly-complete set is no likelier to finish";

	// A piece already held falls back to the floor, and specifically NOT to zero.
	EXPECT_EQ(SetPieceDropWeight(4, true), SetPieceDropWeight(0, false));
	EXPECT_GT(SetPieceDropWeight(4, true), 0)
	    << "a piece you hold can never drop again - sell one by mistake and it is gone for good";

	// HELD means held anywhere. Built three times over, in the three places a piece can be.
	const ItemSetDefinition &set = ItemSets[0];
	ASSERT_GE(set.itemCount, 2);
	const SetItemDefinition &piece = ItemSetItems[set.firstItem];

	Players.resize(1);
	MyPlayer = &Players[0];
	devilution::Player &player = Players[0];
	player._pNumInv = 0;
	for (devilution::Item &slot : player.InvBody)
		slot.clear();
	Stash.stashList.clear();
	Stash.stashGrids.clear();
	ASSERT_EQ(HeldSetPieces(player, set), 0) << "the fixture did not start empty";

	const int base = BaseItemForSetSlot(piece.slot);
	ASSERT_GE(base, 0);
	devilution::Item made;
	InitializeItem(made, static_cast<_item_indexes>(base));
	MakeSetItem(made, piece);

	// (1) worn
	player.InvBody[INVLOC_HEAD] = made;
	EXPECT_TRUE(IsSetPieceHeld(player, piece)) << "a worn piece is not seen as held";
	EXPECT_EQ(HeldSetPieces(player, set), 1);
	player.InvBody[INVLOC_HEAD].clear();

	// (2) in the backpack
	player.InvList[0] = made;
	player._pNumInv = 1;
	EXPECT_TRUE(IsSetPieceHeld(player, piece)) << "a carried piece is not seen as held";
	EXPECT_EQ(HeldSetPieces(player, set), 1);
	player._pNumInv = 0;
	player.InvList[0].clear();

	// (3) in the stash - the case that matters most, and the one a worn-only count misses
	Stash.stashList.push_back(made);
	EXPECT_TRUE(IsSetPieceHeld(player, piece)) << "a stashed piece is not seen as held";
	EXPECT_EQ(HeldSetPieces(player, set), 1);

	// A SECOND copy of the same piece is still one piece of progress, for the reason recorded on
	// WornSetPieces: two set rings in two ring slots are not two pieces.
	Stash.stashList.push_back(made);
	EXPECT_EQ(HeldSetPieces(player, set), 1) << "a duplicate counted as progress";

	Stash.stashList.clear();
	Stash.stashGrids.clear();
	Stash.SetPage(0);
}

/**
 * A monster variant says something about WHERE you are.
 *
 * The four variants shipped at v1.9.7 on a flat 15% everywhere, so a Cathedral skeleton and a Hell
 * knight drew from the same list and "Ashen" was texture rather than information. Each dungeon type
 * now offers a subset.
 *
 * This is also the first test the variants have had at all - they shipped with none.
 */
TEST(OracoolAudit, MonsterVariantRostersArePerDungeonAndComplete)
{
	using namespace devilution::oracool;

	constexpr dungeon_type Dungeons[] = { DTYPE_CATHEDRAL, DTYPE_CATACOMBS, DTYPE_CAVES,
		DTYPE_HELL, DTYPE_NEST, DTYPE_CRYPT };

	// TOWN has no roster, and it is the one that would be actively wrong: an Ashen Griswold.
	EXPECT_EQ(VariantRosterSize(DTYPE_TOWN), 0) << "town offers variants";
	for (uint32_t seed = 0; seed < 500; seed++) {
		ASSERT_EQ(VariantForSeed(seed, DTYPE_TOWN), MonsterVariant::None)
		    << "a town monster rolled a variant at seed " << seed;
	}

	std::set<MonsterVariant> everOffered;
	for (const dungeon_type dungeon : Dungeons) {
		const int size = VariantRosterSize(dungeon);
		EXPECT_GT(size, 0) << "dungeon type " << static_cast<int>(dungeon) << " has an empty roster";

		// No duplicates within a roster: a repeated entry silently doubles that variant's share.
		std::set<MonsterVariant> inThisRoster;
		for (int i = 0; i < size; i++) {
			const MonsterVariant variant = VariantInRoster(dungeon, i);
			EXPECT_NE(variant, MonsterVariant::None) << "None is listed as a roster entry";
			EXPECT_TRUE(inThisRoster.insert(variant).second)
			    << "dungeon type " << static_cast<int>(dungeon) << " lists a variant twice";
			everOffered.insert(variant);
		}

		// Every variant a seed can produce here is one this roster actually lists. This is the
		// assertion that fails if the draw ever stops going through the roster.
		for (uint32_t seed = 0; seed < 2000; seed++) {
			const MonsterVariant rolled = VariantForSeed(seed, dungeon);
			if (rolled == MonsterVariant::None)
				continue;
			ASSERT_TRUE(inThisRoster.count(rolled) == 1)
			    << "seed " << seed << " produced a variant outside dungeon type "
			    << static_cast<int>(dungeon) << "'s roster";
		}
	}

	// Every variant is reachable somewhere. A variant listed in the enum and in no roster is dead
	// code wearing a name, and nothing else would notice.
	for (int i = 1; i <= static_cast<int>(MonsterVariant::LAST); i++) {
		EXPECT_EQ(everOffered.count(static_cast<MonsterVariant>(i)), 1u)
		    << "variant " << i << " appears in no dungeon's roster - it can never spawn";
	}

	// The shallow end is deliberately free of elemental resistances, so floor two teaches the idea
	// without a wall the player has no answer to.
	for (int i = 0; i < VariantRosterSize(DTYPE_CATHEDRAL); i++) {
		const MonsterVariant variant = VariantInRoster(DTYPE_CATHEDRAL, i);
		EXPECT_NE(variant, MonsterVariant::Ashen) << "the Cathedral offers a fire-resistant variant";
		EXPECT_NE(variant, MonsterVariant::Stormtouched) << "the Cathedral offers a lightning-resistant variant";
	}
	// Hell offers all four - the deepest floors are where the player is expected to have answers.
	EXPECT_EQ(VariantRosterSize(DTYPE_HELL), static_cast<int>(MonsterVariant::LAST));

	// The rate is still roughly the declared 15%, whatever the roster's size.
	int variants = 0;
	constexpr int Samples = 10000;
	for (uint32_t seed = 0; seed < Samples; seed++) {
		if (VariantForSeed(seed, DTYPE_HELL) != MonsterVariant::None)
			variants++;
	}
	EXPECT_NEAR(variants * 100.0 / Samples, 15.0, 1.0) << "the variant rate drifted from 15%";
}

/**
 * A place is worth farming for a THING.
 *
 * Before v1.9.13 the socketable roll was four constants applied to every monster in the game - 3%
 * gem, 1% charm, 2% rune, 1% jewel - and the named-set roll was 3% everywhere. Depth changed which
 * socketables were ELIGIBLE (BandedQlvl keeps a Radiant jewel out of the Church) but never what a
 * floor was FOR, so two floors at the same area level were interchangeable and the only reason to
 * prefer one was how fast it cleared.
 *
 * What is pinned here is the part that would rot silently: that every dungeon actually HAS a
 * majority family, that the majorities are not all the same family, and that the weights and the
 * enum walk agree. A table of numbers nobody asserts on is a table that drifts into a flat
 * distribution one tuning pass at a time, and a flat distribution is indistinguishable from not
 * having shipped this at all.
 */
TEST(OracoolAudit, TreasureClassesGiveEveryZoneSomethingOfItsOwn)
{
	using namespace devilution::oracool;

	constexpr dungeon_type Dungeons[] = { DTYPE_CATHEDRAL, DTYPE_CATACOMBS, DTYPE_CAVES,
		DTYPE_HELL, DTYPE_NEST, DTYPE_CRYPT };

	// Town gives nothing, and must, or a shopping trip rolls loot.
	const TreasureClass &town = TreasureClassFor(DTYPE_TOWN);
	EXPECT_EQ(town.socketablePercent, 0) << "town drops socketables";
	EXPECT_EQ(town.setPercent, 0) << "town drops set pieces";

	std::set<SocketableFamily> majorities;
	for (const dungeon_type dungeon : Dungeons) {
		const TreasureClass &tc = TreasureClassFor(dungeon);
		const int total = TotalFamilyWeight(tc);
		ASSERT_GT(total, 0) << tc.name << " has no family weights at all";
		EXPECT_GT(tc.socketablePercent, 0) << tc.name << " never drops a socketable";
		EXPECT_GT(tc.setPercent, 0) << tc.name << " never drops a set piece";

		// Walk the whole weight range and count where each roll lands. This is the real
		// distribution, produced by the real function, not a restatement of the table.
		// Sized from the ENUM, not from a literal. This was `int counts[4]` and stayed 4 when the
		// Mystic Orbs made a fifth family - so FamilyForRoll returned index 4, the loop wrote one
		// past the end of a stack array, and the test did not fail, it HUNG. Exactly the trap the
		// socketable drop's own candidates[] had, one file over.
		constexpr int FamilyCount = static_cast<int>(SocketableFamily::Orb) + 1;
		int counts[FamilyCount] = {};
		for (int roll = 0; roll < total; roll++) {
			const int family = static_cast<int>(FamilyForRoll(tc, roll));
			ASSERT_GE(family, 0);
			ASSERT_LT(family, FamilyCount) << tc.name << " produced a family outside the enum";
			counts[family]++;
		}

		EXPECT_EQ(counts[0], tc.gemWeight) << tc.name << ": the gem share is not its weight";
		EXPECT_EQ(counts[1], tc.runeWeight) << tc.name << ": the rune share is not its weight";
		EXPECT_EQ(counts[2], tc.jewelWeight) << tc.name << ": the jewel share is not its weight";
		EXPECT_EQ(counts[3], tc.charmWeight) << tc.name << ": the charm share is not its weight";
		EXPECT_EQ(counts[4], tc.orbWeight) << tc.name << ": the orb share is not its weight";

		// A MAJORITY, not a tilt. A zone whose best family is 30% of the draw is a zone nobody can
		// feel the difference of, which is the failure this whole system exists to avoid.
		//
		// Orbs are EXCLUDED from the majority contest deliberately: they take a slice of every zone
		// rather than owning one, so a zone whose largest share was orbs would be a zone with no
		// identity - which is the thing being asserted, not a thing to allow.
		int best = 0;
		for (int i = 1; i < FamilyCount - 1; i++) {
			if (counts[i] > counts[best])
				best = i;
		}
		EXPECT_GE(counts[best] * 100 / total, 35)
		    << tc.name << "'s best family is only " << (counts[best] * 100 / total)
		    << "% of its draw - that is a tilt, not a treasure class";
		majorities.insert(static_cast<SocketableFamily>(best));
	}

	// And the majorities differ. Six zones that all favour gems would pass every assertion above
	// and still leave every floor interchangeable.
	EXPECT_GE(majorities.size(), 3u)
	    << "the zones favour fewer than three different families between them";

	// The shallow end does not teach two socket economies at once: no jewels in the Cathedral.
	EXPECT_EQ(TreasureClassFor(DTYPE_CATHEDRAL).jewelWeight, 0)
	    << "the Cathedral drops jewels";

	// Depth pays more often. Compared as a pair rather than asserting exact numbers, so tuning the
	// rates does not break the test - only inverting them does.
	EXPECT_GT(TreasureClassFor(DTYPE_HELL).socketablePercent,
	    TreasureClassFor(DTYPE_CATHEDRAL).socketablePercent)
	    << "Hell is no more generous than the Cathedral";
	EXPECT_GT(TreasureClassFor(DTYPE_HELL).setPercent,
	    TreasureClassFor(DTYPE_CATHEDRAL).setPercent);

	// Every table's name is distinct and non-empty - they reach the wiki and the player.
	std::set<std::string> names;
	for (const dungeon_type dungeon : Dungeons) {
		const std::string name = TreasureClassFor(dungeon).name;
		EXPECT_FALSE(name.empty());
		EXPECT_TRUE(names.insert(name).second) << "two zones share the name " << name;
	}
}

/**
 * A champion is worth crossing the room for, and a boss is worth hunting.
 *
 * The multiplier is the boss half of "zone- and boss-specific". Kept as a multiplier ON the zone's
 * own rates rather than a table of its own, so re-tuning a zone re-tunes its bosses with it - the
 * alternative is two tables that agree on the day they are written.
 */
TEST(OracoolAudit, TreasureBonusRewardsChampionsAndUniques)
{
	using namespace devilution::oracool;

	// Local monsters rather than slots out of the global Monsters array. TreasureBonusFor reads two
	// fields and nothing else - no level, no type index, no position - so a value-initialised
	// Monster is a complete input for it, and borrowing shared storage would only add a fixture the
	// rest of the suite could trip over.
	devilution::Monster ordinary {};
	devilution::Monster champion {};
	devilution::Monster unique {};

	ordinary.uniqueType = UniqueMonsterType::None;
	ordinary.lesserAffix = LesserUniqueAffix::None;
	champion.uniqueType = UniqueMonsterType::None;
	champion.lesserAffix = LesserUniqueAffix::Relentless;
	unique.uniqueType = UniqueMonsterType::Garbud;
	unique.lesserAffix = LesserUniqueAffix::None;

	EXPECT_EQ(TreasureBonusFor(ordinary), 1);
	EXPECT_EQ(TreasureBonusFor(champion), 2) << "a champion is worth no more than an ordinary kill";
	EXPECT_EQ(TreasureBonusFor(unique), 4) << "a unique is worth no more than a champion";

	// A unique that ALSO carries an affix is still worth the unique's four, not the champion's two.
	// Order of the two tests inside TreasureBonusFor is the whole of this, and reversing them is a
	// one-character change that nothing else would notice.
	unique.lesserAffix = LesserUniqueAffix::Relentless;
	EXPECT_EQ(TreasureBonusFor(unique), 4)
	    << "an affixed unique fell through to the champion multiplier";

	// The rates a unique in Hell actually sees, checked against the cap the drop hook applies.
	const TreasureClass &hell = TreasureClassFor(DTYPE_HELL);
	EXPECT_LE(hell.socketablePercent * TreasureBonusFor(unique), 100)
	    << "a unique in Hell asks GenerateRnd(100) for a percentage over 100";
	EXPECT_LE(hell.setPercent * TreasureBonusFor(unique), 100);
}

/**
 * The endgame boss: a champion with heavier numbers, and one byte to say so.
 *
 * The treasure classes (v1.9.13) gave every zone something worth farming for. This is what stands
 * in front of it. Two things are pinned here because both would pass a build while being wrong:
 *
 * THE MARKER. LesserUniqueAffix::Dread is a seventh value on an enum whose LAST still names
 * Colossal, deliberately, so every existing `<= LAST` walk keeps meaning "the rollable affixes".
 * If Dread ever became rollable, ordinary champions would start spawning at 800% health with a
 * 6x treasure multiplier and nothing would report it - so the test asserts the roll cannot produce
 * it, across enough draws that a rare leak would show.
 *
 * THE ORDER inside TreasureBonusFor. A boss borrows a unique's shape AND carries an affix, so both
 * of the other two tests in that function also answer yes for one. Most-specific-first is the whole
 * of that, and moving the boss test down is a two-line change that quietly pays a boss a champion's
 * double instead of its own six.
 */
TEST(OracoolAudit, EndgameBossIsAHeavierChampionAndIsMarkedByOneByte)
{
	using namespace devilution::oracool;

	// The profile is strictly above the champion's on every axis. Compared against the champion's
	// own constants as a RELATION rather than as fixed numbers, so tuning either one stays legal
	// and only inverting them fails.
	constexpr int ChampionHealthPercent = 300;
	constexpr int ChampionDamagePercent = 150;
	constexpr int ChampionArmorBonus = 4;
	constexpr int ChampionPackSize = 4;
	EXPECT_GT(BossHealthPercent(), ChampionHealthPercent);
	EXPECT_GT(BossDamagePercent(), ChampionDamagePercent);
	EXPECT_GT(BossArmorBonus(), ChampionArmorBonus);
	EXPECT_GT(BossPackSize(), ChampionPackSize);
	// And life outruns damage by more than damage outruns life. A boss should be a LONG fight, not
	// a fast death - the same judgement PlaceLesserUniqueMonst already made for champions.
	EXPECT_GT(BossHealthPercent() * 100 / ChampionHealthPercent,
	    BossDamagePercent() * 100 / ChampionDamagePercent)
	    << "the boss profile grew damage faster than health - that is a one-shot, not a boss";

	// uniqueType is set EXPLICITLY on every fixture below, and that is not tidiness. A
	// value-initialised Monster has uniqueType == 0, which is UniqueMonsterType::Garbud - None is
	// -1, not 0 - so a zeroed Monster answers isUnique() with TRUE. The first draft of this test
	// left it out and every fixture came back worth a unique's multiplier.
	devilution::Monster boss {};
	boss.uniqueType = UniqueMonsterType::None;
	boss.lesserAffix = LesserUniqueAffix::Dread;
	devilution::Monster champion {};
	champion.uniqueType = UniqueMonsterType::None;
	champion.lesserAffix = LesserUniqueAffix::Relentless;
	devilution::Monster ordinary {};
	ordinary.uniqueType = UniqueMonsterType::None;
	ordinary.lesserAffix = LesserUniqueAffix::None;

	EXPECT_TRUE(IsEndgameBoss(boss));
	EXPECT_FALSE(IsEndgameBoss(champion)) << "a champion reads as a boss";
	EXPECT_FALSE(IsEndgameBoss(ordinary));

	// A boss out-earns a unique, which out-earns a champion, which out-earns an ordinary kill.
	devilution::Monster unique {};
	unique.uniqueType = UniqueMonsterType::Garbud;
	unique.lesserAffix = LesserUniqueAffix::None;
	EXPECT_GT(TreasureBonusFor(boss), TreasureBonusFor(unique));
	EXPECT_GT(TreasureBonusFor(unique), TreasureBonusFor(champion));
	EXPECT_GT(TreasureBonusFor(champion), TreasureBonusFor(ordinary));

	// THE ORDER. A boss that also wears a unique's shape is still worth a boss's multiplier.
	devilution::Monster bossOnUniqueShape {};
	bossOnUniqueShape.lesserAffix = LesserUniqueAffix::Dread;
	bossOnUniqueShape.uniqueType = UniqueMonsterType::Garbud;
	EXPECT_EQ(TreasureBonusFor(bossOnUniqueShape), TreasureBonusFor(boss))
	    << "a boss borrowing a unique's shape fell through to the unique multiplier";

	// Even at the most generous table, a boss cannot ask GenerateRnd(100) for over 100 percent.
	const TreasureClass &hell = TreasureClassFor(DTYPE_HELL);
	EXPECT_LE(hell.socketablePercent * TreasureBonusFor(boss), 100)
	    << "the drop hook's min() is load-bearing rather than defensive - check it is still there";

	// A boss is Colossal, because the silhouette is the promise - it has to be the biggest thing in
	// the room before the player has read its name. Returns early on the affix, so this needs no
	// level seed behind it.
	EXPECT_EQ(GetMonsterSize(boss), MonsterSize::Colossal) << "a boss is not Colossal";
	EXPECT_NE(GetMonsterSize(champion), MonsterSize::Colossal)
	    << "an ordinary champion is Colossal, so the boss no longer stands out";

	// Dread is NOT rollable. Swept over the named champion types, many draws each - enough that a
	// leak of even a percent would show. Walked by ENUM rather than by UniqueMonstersData, which is
	// not exported to the test binary; the roll only uses the type to avoid repeating itself, so
	// the named ones are a complete exercise of the code path.
	for (int type = static_cast<int>(UniqueMonsterType::Garbud);
	     type <= static_cast<int>(UniqueMonsterType::NaKrul); type++) {
		for (int draw = 0; draw < 200; draw++) {
			const LesserUniqueAffix rolled = RollLesserUniqueAffix(static_cast<UniqueMonsterType>(type));
			ASSERT_NE(rolled, LesserUniqueAffix::Dread)
			    << "RollLesserUniqueAffix produced Dread - ordinary champions are spawning as bosses";
			ASSERT_NE(rolled, LesserUniqueAffix::None) << "a champion rolled no affix at all";
		}
	}
}

/**
 * A boss's SECOND trait, and why it costs nothing to store.
 *
 * One byte holds one affix, and a boss wants two things wrong with it. The second is derived from
 * lesserNameSeed - a value the champion path already rolls and already saves - so it needs no field
 * and reproduces exactly on a revisit, the same trick the monster variants use.
 *
 * The sweep is the point: a trait that no seed can reach is dead code wearing a name, and nothing
 * else in the game would ever notice.
 */
TEST(OracoolAudit, EveryBossTraitIsReachableAndNamed)
{
	using namespace devilution::oracool;

	constexpr int TraitCount = static_cast<int>(BossTrait::LAST) + 1;
	std::set<int> seen;
	int counts[TraitCount] = {};
	// The seed is a uint16_t, so this is every value it can hold - not a sample.
	for (int seed = 0; seed <= 0xFFFF; seed++) {
		const BossTrait trait = SecondaryTraitOf(static_cast<uint16_t>(seed));
		const int index = static_cast<int>(trait);
		ASSERT_GE(index, 0);
		ASSERT_LT(index, TraitCount) << "seed " << seed << " produced a trait outside the enum";
		seen.insert(index);
		counts[index]++;
	}
	EXPECT_EQ(seen.size(), static_cast<size_t>(TraitCount))
	    << "a boss trait exists that no seed can produce";

	// Roughly even. Not exact - 65536 does not divide evenly through an integer /97 - but a trait
	// twice as common as another would be a bug in the mixing rather than a rounding.
	for (int i = 0; i < TraitCount; i++) {
		EXPECT_GT(counts[i] * 100 / 65536, 100 / TraitCount - 6)
		    << "trait " << i << " is far rarer than an even share";
	}

	// Every trait has a word, because the name is where a player learns the second thing before it
	// happens to them.
	std::set<std::string> names;
	for (int i = 0; i < TraitCount; i++) {
		const std::string name = BossTraitName(static_cast<BossTrait>(i));
		EXPECT_FALSE(name.empty()) << "trait " << i << " has no name";
		EXPECT_TRUE(names.insert(name).second) << "two traits share the name " << name;
	}

	// And the display name carries BOTH words.
	devilution::Monster boss {};
	boss.uniqueType = UniqueMonsterType::None;
	boss.lesserAffix = LesserUniqueAffix::Dread;
	boss.lesserNameSeed = 0;
	const std::string shown = GetMonsterDisplayName(boss);
	EXPECT_NE(shown.find("Dread"), std::string::npos) << "a boss is not called Dread: " << shown;
	EXPECT_NE(shown.find(BossTraitName(SecondaryTraitOf(0))), std::string::npos)
	    << "a boss's second trait is not in its name: " << shown;

	// The drain fires for Devouring and for nothing else. The seeds are FOUND rather than assumed,
	// so the test cannot disagree with the derivation about which seed is which trait.
	uint16_t devouringSeed = 0;
	uint16_t otherSeed = 0;
	for (int seed = 0; seed <= 0xFFFF; seed++) {
		if (SecondaryTraitOf(static_cast<uint16_t>(seed)) == BossTrait::Devouring)
			devouringSeed = static_cast<uint16_t>(seed);
		else
			otherSeed = static_cast<uint16_t>(seed);
	}

	devilution::Monster drainer {};
	drainer.lesserAffix = LesserUniqueAffix::Dread;
	drainer.lesserNameSeed = devouringSeed;
	drainer.maxHitPoints = 1000;
	drainer.hitPoints = 500;
	OnBossDealtDamage(drainer, 100);
	EXPECT_GT(drainer.hitPoints, 500) << "a Devouring boss did not drain";
	EXPECT_LE(drainer.hitPoints, drainer.maxHitPoints) << "the drain went past full";

	devilution::Monster nonDrainer {};
	nonDrainer.lesserAffix = LesserUniqueAffix::Dread;
	nonDrainer.lesserNameSeed = otherSeed;
	nonDrainer.maxHitPoints = 1000;
	nonDrainer.hitPoints = 500;
	OnBossDealtDamage(nonDrainer, 100);
	EXPECT_EQ(nonDrainer.hitPoints, 500) << "a boss that is not Devouring drained anyway";

	// And a champion never drains through this hook, whatever its affix.
	devilution::Monster champion {};
	champion.lesserAffix = LesserUniqueAffix::Vampiric;
	champion.lesserNameSeed = devouringSeed;
	champion.maxHitPoints = 1000;
	champion.hitPoints = 500;
	OnBossDealtDamage(champion, 100);
	EXPECT_EQ(champion.hitPoints, 500) << "the boss drain fired on a champion";
}

/**
 * Three Flawed jewels climb to a Plain, and a Radiant is the end of the road.
 *
 * The jewels shipped at v1.9.9 with three grades and no way to climb them: "Refine Gems" tests
 * IsOracoolGemIdx and a jewel is deliberately not a gem, so the ladder existed on paper only.
 *
 * The load-bearing fact under the whole recipe is that the fifteen ids are GRADE-MAJOR - all five
 * Flawed, then all five Plain, then all five Radiant - so a grade step is exactly one family count
 * of ids. gems.cpp static_asserts that, which is the right place for it; what this pins is the
 * behaviour that would go wrong if the assert were ever deleted along with the property: a Flawed
 * Fervor must temper into a Plain FERVOR, not into a Flawed Focus.
 */
TEST(OracoolAudit, TemperJewelsClimbsTheGradeAndStopsAtRadiant)
{
	using namespace devilution::oracool;

	// Every jewel below Radiant climbs, and climbs WITHIN ITS FAMILY. Walked over all fifteen so a
	// reordered generator cannot pass by getting one family right.
	for (int i = IDI_ORACOOL_JEWEL_FERVOR_FLAWED; i <= IDI_ORACOOL_JEWEL_WARDING_RADIANT; i++) {
		const auto idx = static_cast<uint16_t>(i);
		const uint16_t next = NextJewelGrade(idx);

		if (IsTopJewel(idx)) {
			EXPECT_EQ(next, idx) << AllItemsList[i].iName << " tempers into something past Radiant";
			continue;
		}

		EXPECT_NE(next, idx) << AllItemsList[i].iName << " does not temper at all";
		EXPECT_TRUE(IsOracoolJewelIdx(next)) << AllItemsList[i].iName << " tempers into a non-jewel";
		// Same FAMILY. The family is the id's offset within its grade block, and it must not move.
		const int fromFamily = (i - IDI_ORACOOL_JEWEL_FERVOR_FLAWED) % static_cast<int>(JewelFamilyCount);
		const int toFamily = (next - IDI_ORACOOL_JEWEL_FERVOR_FLAWED) % static_cast<int>(JewelFamilyCount);
		EXPECT_EQ(fromFamily, toFamily)
		    << AllItemsList[i].iName << " tempers into a different family: " << AllItemsList[next].iName;
		// And exactly ONE grade, not two.
		const int fromGrade = (i - IDI_ORACOOL_JEWEL_FERVOR_FLAWED) / static_cast<int>(JewelFamilyCount);
		const int toGrade = (next - IDI_ORACOOL_JEWEL_FERVOR_FLAWED) / static_cast<int>(JewelFamilyCount);
		EXPECT_EQ(toGrade, fromGrade + 1) << AllItemsList[i].iName << " skipped a grade";
		// The result is worth more, which is the reason to do it at all.
		EXPECT_GT(AllItemsList[next].iValue, AllItemsList[i].iValue)
		    << AllItemsList[next].iName << " is worth no more than three of " << AllItemsList[i].iName;
	}

	// Non-jewels are inert rather than mangled - the recipe never sees one, but NextJewelGrade is a
	// public function and an arithmetic-only version would happily "climb" a rune into a gem.
	EXPECT_EQ(NextJewelGrade(IDI_ORACOOL_GEM_RUBY_CHIPPED), IDI_ORACOOL_GEM_RUBY_CHIPPED);
	EXPECT_EQ(NextJewelGrade(IDI_GOLD), IDI_GOLD);

	// The recipe exists, is named, and is reachable through the same table the UI walks.
	//
	// GT rather than EQ: this asserted `== 5` when Temper Jewels was the last recipe, and went red
	// the moment four more were added. What this test cares about is that recipe 4 is reachable,
	// not how many recipes exist in total - the count belongs to whichever test is about the table.
	ASSERT_GT(CraftingRecipeCount, 4) << "recipe 4 is past the end of the table";
	const std::string name = CraftingRecipeName(4);
	const std::string inputs = CraftingRecipeInputs(4);
	EXPECT_FALSE(name.empty()) << "recipe 4 has no name - it draws as a blank row";
	EXPECT_FALSE(inputs.empty()) << "recipe 4 has no input line";
	// No two recipes share a name, or the window lists the same thing twice.
	std::set<std::string> names;
	for (int i = 0; i < CraftingRecipeCount; i++)
		EXPECT_TRUE(names.insert(CraftingRecipeName(i)).second) << "two recipes share a name";

	// And it runs on Levski's grid: three Flawed Fervor in, one Plain Fervor out.
	devilution::Item grid[LevskiGridSlots];
	for (int i = 0; i < 3; i++)
		InitializeItem(grid[i], IDI_ORACOOL_JEWEL_FERVOR_FLAWED);
	EXPECT_EQ(FirstReadyLevskiRecipe(grid), 4) << "three identical jewels do not make the recipe ready";

	const std::string result = TransmuteLevskiGrid(grid);
	EXPECT_FALSE(result.empty()) << "the transmute produced nothing";
	int plain = 0;
	int flawed = 0;
	for (const devilution::Item &slot : grid) {
		if (slot.isEmpty())
			continue;
		if (slot.IDidx == IDI_ORACOOL_JEWEL_FERVOR_PLAIN)
			plain++;
		if (slot.IDidx == IDI_ORACOOL_JEWEL_FERVOR_FLAWED)
			flawed++;
	}
	EXPECT_EQ(plain, 1) << "the grid does not hold exactly one Plain Jewel of Fervor";
	EXPECT_EQ(flawed, 0) << "the three Flawed jewels were not all consumed";

	// TWO of a kind is not enough, and three Radiants are not a recipe at all - the top of the
	// ladder has to decline rather than consume three jewels for nothing.
	devilution::Item pair[LevskiGridSlots];
	for (int i = 0; i < 2; i++)
		InitializeItem(pair[i], IDI_ORACOOL_JEWEL_FERVOR_FLAWED);
	EXPECT_NE(FirstReadyLevskiRecipe(pair), 4) << "two jewels were treated as three";

	devilution::Item tops[LevskiGridSlots];
	for (int i = 0; i < 3; i++)
		InitializeItem(tops[i], IDI_ORACOOL_JEWEL_FERVOR_RADIANT);
	EXPECT_NE(FirstReadyLevskiRecipe(tops), 4) << "three Radiant jewels are offered a grade above Radiant";

	// Three of DIFFERENT families is not three of a kind, however many jewels are in the grid.
	devilution::Item mixed[LevskiGridSlots];
	InitializeItem(mixed[0], IDI_ORACOOL_JEWEL_FERVOR_FLAWED);
	InitializeItem(mixed[1], IDI_ORACOOL_JEWEL_FOCUS_FLAWED);
	InitializeItem(mixed[2], IDI_ORACOOL_JEWEL_AEGIS_FLAWED);
	EXPECT_NE(FirstReadyLevskiRecipe(mixed), 4) << "three different jewels were treated as identical";
}

/**
 * A re-run is not Normal with bigger numbers.
 *
 * The backlog row asked for three things: new immunities, new lesser-affix pools, and new drop
 * tiers per difficulty. Auditing it before building found one of the three already done and one
 * done by accident:
 *
 *  - IMMUNITIES answered to the difficulty from Phase 3.3 (oracool/monster_difficulty.cpp).
 *  - DROP TIERS are keyed off item level via TierForItem, and item level rises with the area level,
 *    which rises with the difficulty. So a re-run already produced BETTER items.
 *  - The AFFIX POOL did not. All six modifiers were on the table from the first floor of Normal.
 *
 * And a fourth thing the row did not name but which is the same complaint: a re-run produced better
 * items and no MORE of them. A treasure class is chosen by dungeon type, and a re-run walks the same
 * twenty-four floors, so the Cathedral in Torment paid exactly what the Cathedral in Normal paid.
 */
TEST(OracoolAudit, DifficultyChangesWhatARerunOffersAndNotOnlyHowBig)
{
	using namespace devilution::oracool;

	constexpr _difficulty Ladder[] = { DIFF_NORMAL, DIFF_NIGHTMARE, DIFF_HELL, DIFF_TORMENT };

	// ---- the affix pool GROWS and never shrinks ----
	size_t previous = 0;
	for (const _difficulty difficulty : Ladder) {
		size_t allowed = 0;
		for (int i = 1; i <= static_cast<int>(LesserUniqueAffix::LAST); i++) {
			if (ChampionAffixAllowedOn(static_cast<LesserUniqueAffix>(i), difficulty))
				allowed++;
		}
		EXPECT_GE(allowed, previous)
		    << "difficulty " << static_cast<int>(difficulty) << " offers FEWER affixes than the one below it";
		EXPECT_GT(allowed, 0u) << "a difficulty offers no champion affix at all - nothing could spawn";
		previous = allowed;
	}
	// Strictly more by the top, or the ladder is decoration.
	size_t normalCount = 0;
	size_t tormentCount = 0;
	for (int i = 1; i <= static_cast<int>(LesserUniqueAffix::LAST); i++) {
		if (ChampionAffixAllowedOn(static_cast<LesserUniqueAffix>(i), DIFF_NORMAL))
			normalCount++;
		if (ChampionAffixAllowedOn(static_cast<LesserUniqueAffix>(i), DIFF_TORMENT))
			tormentCount++;
	}
	EXPECT_GT(tormentCount, normalCount) << "Torment offers no more champion modifiers than Normal";
	EXPECT_EQ(tormentCount, static_cast<size_t>(LesserUniqueAffix::LAST))
	    << "the top difficulty does not offer every rollable affix";

	// Every affix arrives SOMEWHERE. One allowed on no difficulty is dead code wearing a name.
	for (int i = 1; i <= static_cast<int>(LesserUniqueAffix::LAST); i++) {
		EXPECT_TRUE(ChampionAffixAllowedOn(static_cast<LesserUniqueAffix>(i), DIFF_TORMENT))
		    << "affix " << i << " can never be rolled on any difficulty";
	}
	// And the boss marker is allowed on NONE of them, on every difficulty.
	for (const _difficulty difficulty : Ladder) {
		EXPECT_FALSE(ChampionAffixAllowedOn(LesserUniqueAffix::Dread, difficulty))
		    << "a champion can roll the endgame-boss marker on difficulty " << static_cast<int>(difficulty);
		EXPECT_FALSE(ChampionAffixAllowedOn(LesserUniqueAffix::None, difficulty));
	}

	// The three Normal offers are specifically the ones that need no gear to answer. Named, because
	// this is a design statement and a design statement that nothing asserts is a comment.
	EXPECT_TRUE(ChampionAffixAllowedOn(LesserUniqueAffix::Relentless, DIFF_NORMAL));
	EXPECT_TRUE(ChampionAffixAllowedOn(LesserUniqueAffix::Fortified, DIFF_NORMAL));
	EXPECT_TRUE(ChampionAffixAllowedOn(LesserUniqueAffix::Colossal, DIFF_NORMAL));
	EXPECT_FALSE(ChampionAffixAllowedOn(LesserUniqueAffix::Warded, DIFF_NORMAL))
	    << "a level-two character can meet a Warded champion";
	EXPECT_FALSE(ChampionAffixAllowedOn(LesserUniqueAffix::Vampiric, DIFF_NIGHTMARE))
	    << "Vampiric arrives before the damage to break it does";

	// ---- the ROLL honours the pool, including its crowded-floor fallback ----
	//
	// This is the half that would have leaked. The fallback fires only when every allowed modifier
	// is already on the floor, which needs a crowded level - so a fallback that ignored the
	// difficulty gate would hand out a Vampiric champion in Normal only on busy floors, which is
	// exactly the kind of bug that never reproduces on demand.
	const _difficulty saved = sgGameInitInfo.nDifficulty;
	sgGameInitInfo.nDifficulty = DIFF_NORMAL;
	for (int draw = 0; draw < 4000; draw++) {
		const LesserUniqueAffix rolled = RollLesserUniqueAffix(UniqueMonsterType::Garbud);
		ASSERT_TRUE(ChampionAffixAllowedOn(rolled, DIFF_NORMAL))
		    << "Normal rolled an affix its own pool excludes: " << GetLesserUniqueAffixName(rolled);
	}
	sgGameInitInfo.nDifficulty = saved;

	// ---- a re-run pays MORE OFTEN, not merely better ----
	int previousScale = 0;
	for (const _difficulty difficulty : Ladder) {
		const int scale = DifficultyTreasureScale(difficulty);
		EXPECT_GT(scale, previousScale)
		    << "difficulty " << static_cast<int>(difficulty) << " is no more generous than the one below it";
		previousScale = scale;
	}
	EXPECT_EQ(DifficultyTreasureScale(DIFF_NORMAL), 100) << "Normal is not the baseline";

	// The scale is applied and clamped. A rate over 100 fed to GenerateRnd(100) is a silent
	// guarantee, and the boss multiplier sits on top of this.
	sgGameInitInfo.nDifficulty = DIFF_TORMENT;
	EXPECT_GT(ScaleRateForDifficulty(10), 10) << "the difficulty scale is not being applied";
	EXPECT_LE(ScaleRateForDifficulty(100), 100) << "a scaled rate escaped past 100 percent";
	EXPECT_LE(ScaleRateForDifficulty(90), 100);
	sgGameInitInfo.nDifficulty = DIFF_NORMAL;
	EXPECT_EQ(ScaleRateForDifficulty(11), 11) << "Normal changed a rate it should have left alone";
	sgGameInitInfo.nDifficulty = saved;

	// ---- and a re-run is DENSER in special encounters ----
	int previousVariant = 0;
	for (const _difficulty difficulty : Ladder) {
		const int pct = VariantPercentFor(difficulty);
		EXPECT_GT(pct, previousVariant)
		    << "the variant rate does not climb at difficulty " << static_cast<int>(difficulty);
		EXPECT_LT(pct, 100) << "the variant rate reached certainty";
		previousVariant = pct;
	}
}

/**
 * The four recipes adopted from Kanai's Cube, and the drain they give the salvage economy.
 *
 * Before v1.9.17 the seven salvage materials had NO consumer anywhere in the game. They dropped,
 * they stacked, they sorted into a row of their own in the stash, and nothing ever spent one. A
 * faucet with no drain. These four are the drain, and which material pays for what is deliberate:
 * you salvage uniques to reforge, rares to ennoble, set pieces to recast.
 *
 * The thing that would rot silently here is the COST. The match and the consume are two different
 * pieces of code reading the same requirement, so a recipe that matched on three engravings and
 * charged two would work perfectly and quietly hand out free crafts forever.
 */
TEST(OracoolAudit, CubeRecipesChargeTheirReagentAndTransformInPlace)
{
	using namespace devilution::oracool;

	// GE, not EQ. This pinned == 9 and went red the moment the tier ladder was added - the second
	// time in two units that a test failed for a number it does not depend on. What it needs is
	// that the four Cube recipes are reachable; how many recipes exist belongs to the test that is
	// about the table.
	ASSERT_GE(CraftingRecipeCount, 9) << "the four Cube recipes are past the end of the table";

	// Every recipe is named, described, and no two share a name.
	std::set<std::string> names;
	for (int i = 0; i < CraftingRecipeCount; i++) {
		EXPECT_FALSE(std::string(CraftingRecipeName(i)).empty()) << "recipe " << i << " draws as a blank row";
		EXPECT_FALSE(std::string(CraftingRecipeInputs(i)).empty()) << "recipe " << i << " has no input line";
		EXPECT_TRUE(names.insert(CraftingRecipeName(i)).second) << "two recipes share a name";
	}

	// Every recipe is grid-only now - CraftingRecipeUsesGrid and its two-venue vocabulary went with
	// the backpack path in v1.9.142. What replaces that assertion is below: the grid must be able to
	// answer for every recipe in the table, since it is the only thing that answers at all.
	for (int i = 0; i < CraftingRecipeCount; i++) {
		devilution::Item empty[LevskiGridSlots] = {};
		EXPECT_FALSE(CanCraftFromLevskiGrid(empty, i))
		    << "recipe " << i << " claims an empty grid can run it";
	}

	Players.resize(1);
	MyPlayer = &Players[0];

	const auto placeReagent = [](devilution::Item *grid, int slot, _item_indexes material, int count) {
		InitializeItem(grid[slot], material);
		grid[slot].setStackCount(count);
	};

	// ---- RECOLOUR: a gem keeps its quality and changes its type, for two Magic Powder ----
	{
		devilution::Item grid[LevskiGridSlots];
		InitializeItem(grid[0], IDI_ORACOOL_GEM_RUBY_CHIPPED);
		placeReagent(grid, 1, IDI_ORACOOL_SALVAGE_MAGIC_POWDER, 5);
		ASSERT_EQ(FirstReadyLevskiRecipe(grid), 8) << "a gem and its powder do not make Recolour ready";

		EXPECT_FALSE(TransmuteLevskiGrid(grid).empty());
		GemType type;
		GemQuality quality;
		ASSERT_TRUE(GemTypeAndQuality(static_cast<uint16_t>(grid[0].IDidx), type, quality));
		EXPECT_NE(grid[0].IDidx, IDI_ORACOOL_GEM_RUBY_CHIPPED) << "the gem did not change type";
		EXPECT_EQ(quality, GemQuality::Chipped) << "the gem changed QUALITY, which is the other recipe";
		// And the cost came out of the stack rather than the slot: five paid two, three remain.
		EXPECT_EQ(grid[1].stackCount(), 3) << "the reagent stack was confiscated rather than charged";
	}

	// A gem with only ONE powder cannot run - the cost is checked, not assumed.
	{
		devilution::Item grid[LevskiGridSlots];
		InitializeItem(grid[0], IDI_ORACOOL_GEM_RUBY_CHIPPED);
		placeReagent(grid, 1, IDI_ORACOOL_SALVAGE_MAGIC_POWDER, 1);
		EXPECT_NE(FirstReadyLevskiRecipe(grid), 8) << "Recolour ran on half its reagent";
	}

	// The reagents are DISTINCT per recipe: a gem beside the wrong material is not a recipe.
	{
		devilution::Item grid[LevskiGridSlots];
		InitializeItem(grid[0], IDI_ORACOOL_GEM_RUBY_CHIPPED);
		placeReagent(grid, 1, IDI_ORACOOL_SALVAGE_SET_ENGRAVINGS, 9);
		EXPECT_EQ(FirstReadyLevskiRecipe(grid), -1) << "a recipe accepted another recipe's reagent";
	}

	// ---- RECAST: a set piece becomes a DIFFERENT piece of the same set ----
	{
		const ItemSetDefinition &set = ItemSets[0];
		ASSERT_GE(set.itemCount, 2);
		const SetItemDefinition &piece = ItemSetItems[set.firstItem];
		const int base = BaseItemForSetSlot(piece.slot);
		ASSERT_GE(base, 0);

		devilution::Item grid[LevskiGridSlots];
		InitializeItem(grid[0], static_cast<_item_indexes>(base));
		MakeSetItem(grid[0], piece);
		placeReagent(grid, 1, IDI_ORACOOL_SALVAGE_SET_ENGRAVINGS, 3);
		ASSERT_EQ(FirstReadyLevskiRecipe(grid), 7);

		EXPECT_FALSE(TransmuteLevskiGrid(grid).empty());
		EXPECT_TRUE(IsSetItem(grid[0])) << "the recast produced something that is not a set piece";
		EXPECT_NE(grid[0]._iCurs, piece.cursor) << "the recast returned the same piece it consumed";
		const SetItemDefinition *made = FindSetItemByCursor(grid[0]._iCurs);
		ASSERT_NE(made, nullptr);
		EXPECT_EQ(FindItemSetOwning(made->id), &set) << "the recast crossed into a different set";
		EXPECT_TRUE(grid[1].isEmpty()) << "an exactly-sufficient reagent stack was not spent";
	}

	Stash.stashList.clear();
	Stash.stashGrids.clear();
	Stash.SetPage(0);
}

/**
 * Nine recipes need a rule for which one runs, and "the lowest-numbered ready one" is not it.
 *
 * That rule was fine while the five recipes had disjoint inputs. It stopped being fine the moment
 * four arrived that all eat "one item plus a reagent": a socketed item with reforge materials
 * beside it satisfies BOTH Free the Sockets (one slot) and Reforge (four), and lowest-index would
 * pick Free the Sockets every time - so reforge reagents would be silently unusable on anything
 * socketed, and nothing would say why.
 *
 * Most-slots-wins is the rule because it is the one a player can predict without reading the
 * source: the monument runs the recipe that uses the most of what you put in front of it.
 */
TEST(OracoolAudit, TheMonumentRunsTheRecipeThatUsesTheMostOfWhatYouPutIn)
{
	using namespace devilution::oracool;

	Players.resize(1);
	MyPlayer = &Players[0];

	// Three identical gems alone: Refine Gems, three slots.
	devilution::Item gems[LevskiGridSlots];
	for (int i = 0; i < 3; i++)
		InitializeItem(gems[i], IDI_ORACOOL_GEM_RUBY_CHIPPED);
	EXPECT_EQ(FirstReadyLevskiRecipe(gems), 0);

	// An empty grid offers nothing, and must say so rather than picking recipe 0 by default.
	devilution::Item empty[LevskiGridSlots];
	EXPECT_EQ(FirstReadyLevskiRecipe(empty), -1) << "an empty grid claims a recipe is ready";

	// THE COLLISION. A socketed item satisfies Free the Sockets on its own; add reforge reagents
	// and Reforge - which consumes strictly more - must win.
	devilution::Item collide[LevskiGridSlots];
	InitializeItem(collide[0], IDI_ORACOOL_HELM);
	collide[0]._iMagical = ITEM_QUALITY_MAGIC;
	collide[0]._iSocketed[0] = static_cast<uint16_t>(IDI_ORACOOL_GEM_RUBY_CHIPPED);
	ASSERT_EQ(FirstReadyLevskiRecipe(collide), 3) << "the socketed item alone is not Free the Sockets";

	InitializeItem(collide[1], IDI_ORACOOL_SALVAGE_UNIQUE_ENCRUSTMENTS);
	collide[1].setStackCount(3);
	// Reforge refuses a socketed item outright - its stats would come back without the runes that
	// are inside it - so with the stones still in, Free the Sockets remains the only answer. That
	// is the correct outcome and worth pinning: the player empties it first, then reforges.
	EXPECT_EQ(FirstReadyLevskiRecipe(collide), 3)
	    << "reforge accepted a socketed item and would have destroyed what was in it";

	// With the sockets empty, the reagents win.
	collide[0]._iSocketed[0] = devilution::Item::EmptySocket;
	EXPECT_EQ(FirstReadyLevskiRecipe(collide), 5)
	    << "four slots of reforge lost to a one-slot recipe - the most-slots rule is gone";
}

/**
 * The tier ladder, the rerolls, and the two ethereal recipes.
 *
 * Seventeen recipes now, and most of them are shaped "one item plus one reagent stack". That shape
 * is what forced explicit selection: a reagent stack of five sits in ONE grid slot, so nearly every
 * item recipe ties at two slots and the most-slots tie-break decides for the player - and worse,
 * Ennoble Rares and Reroll Rares want the SAME target and the SAME material at different counts, so
 * no automatic rule can pick the one that was meant.
 */
TEST(OracoolAudit, TheTierLadderClimbsRerollsAndMakesEthereal)
{
	using namespace devilution::oracool;

	Players.resize(1);
	MyPlayer = &Players[0];

	// Every recipe is named, described, and distinct - the book lists all seventeen.
	std::set<std::string> names;
	for (int i = 0; i < CraftingRecipeCount; i++) {
		EXPECT_FALSE(std::string(CraftingRecipeName(i)).empty()) << "recipe " << i << " is a blank row";
		EXPECT_FALSE(std::string(CraftingRecipeInputs(i)).empty()) << "recipe " << i << " has no formula";
		EXPECT_TRUE(names.insert(CraftingRecipeName(i)).second) << "two recipes share a name";
	}

	const auto placeReagent = [](devilution::Item *grid, int slot, _item_indexes material, int count) {
		InitializeItem(grid[slot], material);
		grid[slot].setStackCount(count);
	};
	// A piece of gear deep enough that every tier is allowed to roll on it.
	const auto makeGear = [](devilution::Item &item) {
		InitializeItem(item, IDI_ORACOOL_HELM);
		item._iOracoolItemLevel = 60;
		item._iIdentified = true;
	};

	// ---- ENRICH: a plain item becomes a rare ----
	{
		devilution::Item grid[LevskiGridSlots];
		makeGear(grid[0]);
		ASSERT_EQ(grid[0]._iOracoolTier, OracoolItemTier::None) << "the fixture started already tiered";
		placeReagent(grid, 1, IDI_ORACOOL_SALVAGE_MAGIC_POWDER, 10);

		ASSERT_TRUE(CanCraftFromLevskiGrid(grid, 9)) << "Enrich is not offered on a plain item";
		// Non-empty means something was MADE. A recipe that matches and then silently produces
		// nothing is the failure this caught for real: SetupAllItems only reaches its forced-tier
		// branch when GetItemBLevel returns something other than -1, and that call has a random
		// component unless onlygood is set - so the climb quietly did not happen a large share of
		// the time while the recipe still read as ready.
		EXPECT_FALSE(TransmuteLevskiGridWith(grid, 9).empty()) << "Enrich matched but made nothing";
		EXPECT_EQ(grid[0]._iOracoolTier, OracoolItemTier::Rare) << "Enrich did not reach the Rare tier";
		EXPECT_EQ(grid[0]._iOracoolItemLevel, 60) << "the item forgot the depth it was found at";
		EXPECT_EQ(grid[1].stackCount(), 6) << "Enrich charged something other than its four";
	}

	// ---- AWAKEN: a unique becomes a primal ----
	{
		devilution::Item grid[LevskiGridSlots];
		makeGear(grid[0]);
		grid[0]._iOracoolTier = OracoolItemTier::BuffedUnique;
		placeReagent(grid, 1, IDI_ORACOOL_SALVAGE_UNIQUE_ENCRUSTMENTS, 8);

		EXPECT_FALSE(TransmuteLevskiGridWith(grid, 11).empty());
		EXPECT_EQ(grid[0]._iOracoolTier, OracoolItemTier::Primal) << "Awaken did not reach Primal";
		EXPECT_TRUE(grid[1].isEmpty()) << "an exactly-sufficient stack was not spent";
	}

	// ---- REROLL keeps the rung it is on ----
	{
		devilution::Item grid[LevskiGridSlots];
		makeGear(grid[0]);
		grid[0]._iOracoolTier = OracoolItemTier::Rare;
		placeReagent(grid, 1, IDI_ORACOOL_SALVAGE_RARE_FIBRES, 3);

		EXPECT_FALSE(TransmuteLevskiGridWith(grid, 12).empty());
		EXPECT_EQ(grid[0]._iOracoolTier, OracoolItemTier::Rare)
		    << "a reroll changed the tier - a reroll and a climb are the same act at different rungs, "
		       "and this one used the wrong rung";
	}

	// ---- CONSECRATE: a rare becomes a set piece for the SAME SLOT ----
	{
		devilution::Item grid[LevskiGridSlots];
		makeGear(grid[0]);
		grid[0]._iOracoolTier = OracoolItemTier::Rare;
		const item_equip_type slot = grid[0]._iLoc;
		placeReagent(grid, 1, IDI_ORACOOL_SALVAGE_SET_ENGRAVINGS, 6);

		if (!TransmuteLevskiGridWith(grid, 10).empty()) {
			EXPECT_TRUE(IsSetItem(grid[0])) << "Consecrate produced something that is not a set piece";
			EXPECT_EQ(grid[0]._iLoc, slot) << "Consecrate moved the item to a different equipment slot";
		}
	}

	// ---- MAKE ETHEREAL, then MEND it ----
	{
		devilution::Item grid[LevskiGridSlots];
		makeGear(grid[0]);
		const int baseAc = grid[0]._iAC;
		const int baseMaxDur = grid[0]._iMaxDur;
		ASSERT_GT(baseMaxDur, 1) << "the fixture has no durability to halve";
		placeReagent(grid, 1, IDI_ORACOOL_SALVAGE_ETHEREAL_IMBUEITIES, 20);

		EXPECT_FALSE(TransmuteLevskiGridWith(grid, 15).empty());
		EXPECT_TRUE(grid[0]._iOracoolEthereal) << "Make Ethereal did not mark the item";
		EXPECT_GT(grid[0]._iAC, baseAc) << "the ethereal bonus was not applied";
		EXPECT_LT(grid[0]._iMaxDur, baseMaxDur) << "ethereal did not halve the lifespan";
		EXPECT_EQ(grid[1].stackCount(), 15) << "Make Ethereal charged something other than its five";

		// An already-ethereal item is not offered the recipe again - it would take five more for
		// nothing, and MakeItemEthereal would halve the durability a second time.
		EXPECT_FALSE(CanCraftFromLevskiGrid(grid, 15)) << "an ethereal item was offered Make Ethereal again";

		// MEND. Only offered on a DAMAGED one, and it keeps the item ethereal - what is bought is
		// the removal of ethereal's only price, which is why it is the most expensive recipe here.
		EXPECT_FALSE(CanCraftFromLevskiGrid(grid, 16)) << "an undamaged ethereal was offered a repair";
		grid[0]._iDurability = 1;
		ASSERT_TRUE(CanCraftFromLevskiGrid(grid, 16));
		EXPECT_FALSE(TransmuteLevskiGridWith(grid, 16).empty());
		EXPECT_EQ(grid[0]._iDurability, grid[0]._iMaxDur) << "Mend did not fill the durability";
		EXPECT_TRUE(grid[0]._iOracoolEthereal) << "Mend stripped the ethereal bargain it was paying for";
		EXPECT_EQ(grid[1].stackCount(), 3) << "Mend charged something other than its twelve";
	}

	// Mending is the most expensive recipe in the game, and deliberately dearer than making one.
	EXPECT_GT(CraftingRecipeReagentCount(16), CraftingRecipeReagentCount(15))
	    << "repairing an ethereal costs no more than creating one";
	// And climbing a rung always costs more than rerolling at it.
	EXPECT_GT(CraftingRecipeReagentCount(9), CraftingRecipeReagentCount(8));
	EXPECT_GT(CraftingRecipeReagentCount(11), CraftingRecipeReagentCount(13))
	    << "Awaken costs no more than rerolling a unique in place";
	EXPECT_GT(CraftingRecipeReagentCount(6), CraftingRecipeReagentCount(12))
	    << "Ennoble costs no more than rerolling a rare in place";
}

/**
 * The player picks the recipe, and a chosen recipe that is not ready runs NOTHING.
 *
 * The fallback is the dangerous half. A player selects Reroll Rares, is one fibre short, and a
 * monument that fell back to "whatever else is ready" would ennoble the item instead - spending a
 * different pile of materials on a change they did not ask for and cannot undo.
 */
TEST(OracoolAudit, ASelectedRecipeRunsOrNothingDoes)
{
	using namespace devilution::oracool;

	Players.resize(1);
	MyPlayer = &Players[0];

	// A unique-tier item and eight Unique Encrustments. Awaken (recipe 11) wants eight of them and
	// climbs the item to Primal; Reroll Uniques (13) wants four and leaves the rung alone. SAME
	// target, SAME material, different cost and different outcome - the collision no automatic rule
	// can resolve, which is the whole reason selection exists. Asserted rather than assumed.
	devilution::Item grid[LevskiGridSlots];
	InitializeItem(grid[0], IDI_ORACOOL_HELM);
	grid[0]._iOracoolItemLevel = 60;
	grid[0]._iOracoolTier = OracoolItemTier::BuffedUnique;
	InitializeItem(grid[1], IDI_ORACOOL_SALVAGE_UNIQUE_ENCRUSTMENTS);
	grid[1].setStackCount(8);

	ASSERT_TRUE(CanCraftFromLevskiGrid(grid, 11)) << "Awaken is not ready";
	ASSERT_TRUE(CanCraftFromLevskiGrid(grid, 13)) << "Reroll Uniques is not ready";

	// Choosing the cheaper one runs the cheaper one, and does NOT climb the item.
	EXPECT_FALSE(TransmuteLevskiGridWith(grid, 13).empty());
	EXPECT_EQ(grid[1].stackCount(), 4) << "the selection ran a recipe with a different cost";
	EXPECT_EQ(grid[0]._iOracoolTier, OracoolItemTier::BuffedUnique)
	    << "the reroll awakened the item to Primal instead - the selection was ignored";

	// Four remain: still enough to reroll, not enough to awaken.
	ASSERT_TRUE(CanCraftFromLevskiGrid(grid, 13));
	ASSERT_FALSE(CanCraftFromLevskiGrid(grid, 11));

	// A chosen recipe that is not ready runs NOTHING - and specifically does not fall through to
	// the one that is. Falling back would spend four encrustments on a reroll the player did not
	// ask for while they were saving up for the climb.
	const int before = grid[1].stackCount();
	const auto tierBefore = grid[0]._iOracoolTier;
	EXPECT_TRUE(TransmuteLevskiGridWith(grid, 11).empty()) << "an unready selection made something";
	EXPECT_EQ(grid[1].stackCount(), before) << "an unready selection still spent materials";
	EXPECT_EQ(grid[0]._iOracoolTier, tierBefore) << "an unready selection fell through to another recipe";

	// -1 is the automatic mode, and still works for the callers that want it.
	EXPECT_GE(FirstReadyLevskiRecipe(grid), 0) << "nothing is ready in automatic mode";
	EXPECT_FALSE(TransmuteLevskiGridWith(grid, -1).empty());
}

/**
 * Mystic Orbs - D2MXL-to-ORCL Phase 1.
 *
 * A consumable that adds one fixed small stat to an item, permanently, capped per ITEM. The cap is
 * the mechanism: six into one weapon finishes it and the seventh has to go somewhere else, which is
 * what makes an orb a decision rather than an accumulator.
 *
 * This is the first per-item value in the fork that is NOT derived from a seed - the base tier, the
 * ethereal roll and every affix come out of the item's; a monster variant and a boss trait come out
 * of the monster's. A player decision has nowhere to be recomputed from, so it cost a byte and a
 * format bump. That makes the round trip below the load-bearing test of the whole phase.
 */
TEST(OracoolAudit, MysticOrbsAreCappedPerItemAndSpendThemselves)
{
	using namespace devilution::oracool;

	Players.resize(1);
	MyPlayer = &Players[0];
	devilution::Player &player = Players[0];

	// Every orb is a real item, has a power that is not INVALID, and has a description line.
	int seen = 0;
	std::set<int> cursors;
	std::set<std::string> lines;
	for (int i = IDI_GOLD; i <= IDI_LAST; i++) {
		if (!IsOracoolOrbIdx(i))
			continue;
		seen++;
		const ItemData &data = AllItemsList[i];
		EXPECT_TRUE(cursors.insert(data.iCurs).second) << data.iName << " shares a cursor";
		EXPECT_LE(data.iCurs, ICURS_ORACOOL_LAST) << data.iName << " points past the icon strip";
		EXPECT_NE(MysticOrbPower(i).type, IPL_INVALID) << data.iName << " grants nothing";
		// param1 == param2, or the orb ROLLS - and a rolled orb is an affix wearing another name.
		EXPECT_EQ(MysticOrbPower(i).param1, MysticOrbPower(i).param2)
		    << data.iName << " rolls a range instead of granting a fixed value";
		const std::string line = MysticOrbLine(i);
		EXPECT_FALSE(line.empty()) << data.iName << " has no description line";
		EXPECT_TRUE(lines.insert(line).second) << "two orbs describe themselves identically";
		// Excluded from the seeded droppable pool, like every Oracool family - that pool is save
		// format, and UnPackItem replays a seed through it to recover an index.
		EXPECT_FALSE(IsOracoolGemIdx(i) || IsOracoolRuneIdx(i) || IsOracoolJewelIdx(i)
		    || IsOracoolSalvageIdx(i) || IsOracoolCharmIdx(i))
		    << data.iName << " reads as another family as well as an orb";
	}
	EXPECT_EQ(seen, 8) << "IsOracoolOrbIdx does not recognise exactly the eight orbs";

	// ---- the CAP, which is the whole mechanism ----
	devilution::Item helm;
	InitializeItem(helm, IDI_ORACOOL_HELM);
	devilution::Item orb;
	InitializeItem(orb, IDI_ORACOOL_ORB_MIGHT);

	ASSERT_TRUE(CanReceiveMysticOrb(helm)) << "a plain helm cannot take an orb";
	const int baseStr = helm._iPLStr;
	for (int i = 0; i < MaxOrbsPerItem; i++) {
		EXPECT_TRUE(TryApplyMysticOrb(player, helm, orb)) << "orb " << i << " was refused early";
		EXPECT_EQ(helm._iOracoolOrbCount, i + 1);
	}
	EXPECT_GT(helm._iPLStr, baseStr) << "six Orbs of Might granted no strength at all";

	// The seventh is refused, and the item says so by no longer accepting.
	EXPECT_FALSE(CanReceiveMysticOrb(helm)) << "a full item still offers room";
	const int strAtCap = helm._iPLStr;
	EXPECT_FALSE(TryApplyMysticOrb(player, helm, orb)) << "the cap was exceeded";
	EXPECT_EQ(helm._iOracoolOrbCount, MaxOrbsPerItem) << "a refused orb still counted";
	EXPECT_EQ(helm._iPLStr, strAtCap) << "a refused orb still granted its stat";

	// The cap counts ORBS, not stat sources: mixing kinds does not buy more room. That is the
	// difference between a decision and an accumulator, and it is one `++` away from being wrong.
	devilution::Item mixed;
	InitializeItem(mixed, IDI_ORACOOL_HELM);
	devilution::Item vigour;
	InitializeItem(vigour, IDI_ORACOOL_ORB_VIGOUR);
	for (int i = 0; i < MaxOrbsPerItem; i++)
		ASSERT_TRUE(TryApplyMysticOrb(player, mixed, (i % 2 == 0) ? orb : vigour));
	EXPECT_FALSE(TryApplyMysticOrb(player, mixed, vigour))
	    << "a different KIND of orb found room past the cap - the cap is per orb type, not per item";

	// ---- what declines ----
	devilution::Item rune;
	InitializeItem(rune, IDI_ORACOOL_GEM_RUBY_CHIPPED);
	EXPECT_FALSE(CanReceiveMysticOrb(rune)) << "a gem accepts orbs";
	devilution::Item anotherOrb;
	InitializeItem(anotherOrb, IDI_ORACOOL_ORB_FURY);
	EXPECT_FALSE(CanReceiveMysticOrb(anotherOrb)) << "an orb accepts orbs";
	devilution::Item potion;
	InitializeItem(potion, IDI_HEAL);
	EXPECT_FALSE(CanReceiveMysticOrb(potion)) << "a potion accepts orbs";

	// And a non-orb held item is not absorbed by anything.
	devilution::Item target;
	InitializeItem(target, IDI_ORACOOL_HELM);
	EXPECT_FALSE(TryApplyMysticOrb(player, target, rune)) << "a gem was absorbed as an orb";
	EXPECT_EQ(target._iOracoolOrbCount, 0);

	// ---- the description line, which is how a player learns the cap before spending ----
	EXPECT_FALSE(MysticOrbCountLine(target).empty()) << "an empty item shows no orb line";
	EXPECT_FALSE(MysticOrbCountLine(helm).empty()) << "a FULL item stops showing its orb line";
	EXPECT_TRUE(MysticOrbCountLine(rune).empty()) << "a gem shows an orb line";
}

/**
 * The orb count survives a save and a load.
 *
 * The one thing about Phase 1 that could not be got right by reasoning. Every other per-item value
 * this fork added was derived and therefore free; this one is stored, and a stored field that is
 * written but not read - or read at the wrong offset - is the failure mode that byte-shifts every
 * item after it. OracoolItemFormatVersion moved 8 -> 9 for exactly this.
 */
TEST(OracoolAudit, TheOrbCountAndMagicFindSurviveARoundTrip)
{
	using namespace devilution::oracool;

	Players.resize(1);
	MyPlayer = &Players[0];
	devilution::Player &player = Players[0];

	devilution::Item original;
	InitializeItem(original, IDI_ORACOOL_HELM);
	devilution::Item fortune;
	InitializeItem(fortune, IDI_ORACOOL_ORB_FORTUNE);
	ASSERT_TRUE(TryApplyMysticOrb(player, original, fortune));
	ASSERT_TRUE(TryApplyMysticOrb(player, original, fortune));
	ASSERT_EQ(original._iOracoolOrbCount, 2);
	// IPL_MAGICFIND is new in the same version - magic find had been an ItemBonusTotals figure that
	// only a CHARM could contribute to, so no item, affix, set rung or orb could grant it.
	ASSERT_GT(original._iPLMagicFind, 0) << "Orb of Fortune granted no magic find";

	// The SAVE round trip is not reachable from here - SaveItem/LoadItemData are file-local to
	// loadsave.cpp, and PackItem is the network path, which carries no extension record at all. It
	// is covered instead by Writehero.pfile_write_hero, whose golden output must move exactly once
	// at this format bump and never again for this reason.
	//
	// What IS asserted here is the half a round-trip test would not catch anyway: that the stat
	// reaches the player. A field can round-trip perfectly and still be read by nothing.

	// The equipment provider is what actually delivers an orb's stat to the player, so it is
	// asserted through the totals rather than by reading the field straight back - the field being
	// set proves nothing about whether anything consumes it.
	// _iStatFlag is "the wearer meets this item's requirements", and it is set by CalcPlrInv rather
	// than by InitializeItem - AddItem returns on its first line without it, so a fixture that
	// omitted it would report a magic find of zero and read as a failure of the orb rather than of
	// the fixture. It did, on the first run of this test.
	original._iStatFlag = true;
	ItemBonusTotals totals = {};
	totals.AddItem(original);
	EXPECT_GT(totals.magicFind, 0)
	    << "an orbed item's magic find never reaches ItemBonusTotals - the stat is written and unread";
}

/**
 * Signets of Learning and the milestones that pay them - D2MXL-to-ORCL Phase 2.
 *
 * Permanent progression that survives your gear, with a LIFETIME CAP. The cap is the design:
 * without it a signet is a slower level-up; with it, the pool is a finite resource you can exhaust
 * and then must live with.
 *
 * Unlike Phase 1 this cost no format change at all - both values ride the hero chunk tail, which is
 * tagged, length-prefixed and forward-compatible. That is worth an assertion of its own, because
 * "it persists" is the entire claim and nothing else in the build would notice if it stopped.
 */
TEST(OracoolAudit, SignetsAreCappedAndMilestonesPayThemOnce)
{
	using namespace devilution::oracool;

	Players.resize(1);
	MyPlayer = &Players[0];
	devilution::Player &player = Players[0];

	// A clean slate. ApplyMilestones/ApplySignetsUsed are the load path, so they are also how a
	// test resets - going through the real door rather than reaching past it.
	ApplyMilestones(player, 0);
	ApplySignetsUsed(player, 0);
	player._pStatPts = 0;
	ASSERT_EQ(SignetsUsed(player), 0);

	// Every milestone is named, and no two share a name - they reach the event log.
	std::set<std::string> names;
	for (int i = 0; i < MilestoneCount; i++) {
		const std::string name = MilestoneName(static_cast<Milestone>(i));
		EXPECT_FALSE(name.empty()) << "milestone " << i << " has no name";
		EXPECT_TRUE(names.insert(name).second) << "two milestones share the name " << name;
	}
	// The mask is a u32 on the wire, so the list must fit in one.
	EXPECT_LE(MilestoneCount, 32) << "the milestone list outgrew its u32 chunk";

	// ---- a milestone pays ONCE ----
	EXPECT_FALSE(IsMilestoneClaimed(player, Milestone::Level20));
	EXPECT_TRUE(ClaimMilestone(player, Milestone::Level20)) << "the first claim was refused";
	EXPECT_TRUE(IsMilestoneClaimed(player, Milestone::Level20));
	EXPECT_EQ(player._pStatPts, 1) << "a claimed milestone granted no point";
	EXPECT_EQ(SignetsUsed(player), 1);

	EXPECT_FALSE(ClaimMilestone(player, Milestone::Level20)) << "a milestone paid twice";
	EXPECT_EQ(player._pStatPts, 1) << "a second claim still granted a point";
	EXPECT_EQ(SignetsUsed(player), 1);

	// ---- the CAP, which is the whole design ----
	ApplyMilestones(player, 0);
	ApplySignetsUsed(player, 0);
	player._pStatPts = 0;
	for (int i = 0; i < SignetLifetimeCap; i++)
		ASSERT_TRUE(ConsumeSignet(player)) << "signet " << i << " was refused early";
	EXPECT_EQ(player._pStatPts, SignetLifetimeCap);
	EXPECT_FALSE(CanConsumeSignet(player)) << "a spent pool still offers room";
	EXPECT_FALSE(ConsumeSignet(player)) << "the lifetime cap was exceeded";
	EXPECT_EQ(player._pStatPts, SignetLifetimeCap) << "a refused signet still granted a point";

	// A milestone met with the pool spent is still CLAIMED - it does not lurk and pay out later,
	// out of order, for something the player did hours ago.
	EXPECT_TRUE(ClaimMilestone(player, Milestone::SlayDreadBoss)) << "a milestone was not claimed at the cap";
	EXPECT_TRUE(IsMilestoneClaimed(player, Milestone::SlayDreadBoss));
	EXPECT_EQ(player._pStatPts, SignetLifetimeCap) << "a milestone paid past the cap";

	// ---- the passive walk is idempotent, because it runs on every level-up ----
	ApplyMilestones(player, 0);
	ApplySignetsUsed(player, 0);
	player._pStatPts = 0;
	player._pLevel = 45;
	CheckPassiveMilestones(player);
	const int afterFirst = player._pStatPts;
	EXPECT_GE(afterFirst, 2) << "level 45 did not claim the level-20 and level-40 milestones";
	CheckPassiveMilestones(player);
	CheckPassiveMilestones(player);
	EXPECT_EQ(player._pStatPts, afterFirst)
	    << "the passive walk paid again - it runs on every level-up, so it must be idempotent";
	// And a threshold not yet reached stays unclaimed.
	EXPECT_FALSE(IsMilestoneClaimed(player, Milestone::Level60));

	// ---- persistence: the whole claim of this phase ----
	ApplyMilestones(player, 0);
	ApplySignetsUsed(player, 0);
	ClaimMilestone(player, Milestone::Level20);
	ClaimMilestone(player, Milestone::CompleteRuneword);
	const uint32_t savedMask = PackMilestones(player);
	const uint8_t savedUsed = PackSignetsUsed(player);

	ApplyMilestones(player, 0);
	ApplySignetsUsed(player, 0);
	ASSERT_FALSE(IsMilestoneClaimed(player, Milestone::Level20)) << "the reset did not take";

	ApplyMilestones(player, savedMask);
	ApplySignetsUsed(player, savedUsed);
	EXPECT_TRUE(IsMilestoneClaimed(player, Milestone::Level20)) << "a claimed milestone did not survive";
	EXPECT_TRUE(IsMilestoneClaimed(player, Milestone::CompleteRuneword));
	EXPECT_EQ(SignetsUsed(player), savedUsed) << "the consumed count did not survive";

	// A mask from a LATER build, with bits this one cannot name, must not mark unknown milestones
	// claimed - that would silently withhold a reward this build is supposed to pay.
	ApplyMilestones(player, 0xFFFFFFFFU);
	for (int i = 0; i < MilestoneCount; i++)
		EXPECT_TRUE(IsMilestoneClaimed(player, static_cast<Milestone>(i)));
	EXPECT_EQ(PackMilestones(player), (MilestoneCount >= 32) ? 0xFFFFFFFFU : ((1U << MilestoneCount) - 1U))
	    << "unknown milestone bits were kept rather than masked away";

	// A corrupted count is clamped, not trusted.
	ApplySignetsUsed(player, 200);
	EXPECT_EQ(SignetsUsed(player), SignetLifetimeCap) << "an out-of-range consumed count was not clamped";
	EXPECT_FALSE(CanConsumeSignet(player));

	ApplyMilestones(player, 0);
	ApplySignetsUsed(player, 0);
}

/**
 * The Signet as an ITEM - D2MXL-to-ORCL Phase 2b.
 *
 * Milestones are the reliable source of signets; the drop is the bonus. So the drop is deliberately
 * champions-and-better only, tied to the same TreasureBonusFor that decides everything else about
 * what a monster is worth - a bonus that fell off ordinary monsters would be a trickle nobody could
 * aim at.
 *
 * The load-bearing assertion here is the REFUSAL. A signet is capped, permanent and unrecoverable,
 * so one used at the cap must not be consumed. UseInvItem eats the item AFTER UseItem returns, which
 * is precisely why the gate lives in UseInvItem beside the spell-book gate and not inside UseItem -
 * and that is a two-line arrangement that would look fine while quietly eating signets.
 */
TEST(OracoolAudit, TheSignetIsAUsableItemThatChampionsDrop)
{
	using namespace devilution::oracool;

	Players.resize(1);
	MyPlayer = &Players[0];
	devilution::Player &player = Players[0];

	// It is a real, usable item with a frame of its own.
	const ItemData &data = AllItemsList[IDI_ORACOOL_SIGNET_LEARNING];
	EXPECT_TRUE(IsOracoolSignetIdx(IDI_ORACOOL_SIGNET_LEARNING));
	EXPECT_TRUE(data.iUsable) << "the signet is not usable - UseItem's switch will never reach it";
	EXPECT_EQ(data.iMiscId, IMISC_ORACOOL_SIGNET)
	    << "the signet does not carry its own misc id, so it dispatches as something else";
	EXPECT_LE(data.iCurs, ICURS_ORACOOL_LAST) << "the signet points past the icon strip";

	// It is not mistaken for any other family, which is what keeps the pool exclusion honest - that
	// exclusion is one OR-chain of hand-written ranges.
	EXPECT_FALSE(IsOracoolOrbIdx(IDI_ORACOOL_SIGNET_LEARNING));
	EXPECT_FALSE(IsOracoolGemIdx(IDI_ORACOOL_SIGNET_LEARNING));
	EXPECT_FALSE(IsOracoolRuneIdx(IDI_ORACOOL_SIGNET_LEARNING));
	EXPECT_FALSE(IsOracoolJewelIdx(IDI_ORACOOL_SIGNET_LEARNING));
	EXPECT_FALSE(IsOracoolSalvageIdx(IDI_ORACOOL_SIGNET_LEARNING));
	EXPECT_FALSE(IsOracoolCharmIdx(IDI_ORACOOL_SIGNET_LEARNING));

	// A signet is NOT a socketable - it must never be accepted into a socket or absorbed as an orb.
	devilution::Item signet;
	InitializeItem(signet, IDI_ORACOOL_SIGNET_LEARNING);
	devilution::Item host;
	InitializeItem(host, IDI_ORACOOL_HELM);
	host._iSocketCount = 3;
	EXPECT_FALSE(TrySocketGem(host, signet)) << "a signet was socketed";
	EXPECT_FALSE(TryApplyMysticOrb(player, host, signet)) << "a signet was absorbed as a Mystic Orb";
	EXPECT_EQ(host._iOracoolOrbCount, 0);

	// ---- the drop is champions and better ----
	devilution::Monster ordinary {};
	ordinary.uniqueType = UniqueMonsterType::None;
	ordinary.lesserAffix = LesserUniqueAffix::None;
	devilution::Monster champion {};
	champion.uniqueType = UniqueMonsterType::None;
	champion.lesserAffix = LesserUniqueAffix::Relentless;

	ASSERT_EQ(TreasureBonusFor(ordinary), 1) << "the fixture's ordinary monster is not ordinary";
	ASSERT_GT(TreasureBonusFor(champion), 1) << "the fixture's champion is not a champion";
	// The rule the drop hook applies, asserted as the RULE rather than by sampling the roll: an
	// ordinary kill is excluded by bonus <= 1, and nothing else about it matters.
	EXPECT_LE(TreasureBonusFor(ordinary), 1)
	    << "an ordinary monster would drop signets - the drop is meant to be a reason to fight champions";

	// ---- the CAP refusal, which is the assertion that matters ----
	ApplySignetsUsed(player, 0);
	player._pStatPts = 0;
	EXPECT_TRUE(CanConsumeSignet(player));

	ApplySignetsUsed(player, SignetLifetimeCap);
	EXPECT_FALSE(CanConsumeSignet(player)) << "a spent pool still offers room";
	const int ptsAtCap = player._pStatPts;
	EXPECT_FALSE(ConsumeSignet(player)) << "the cap was exceeded";
	EXPECT_EQ(player._pStatPts, ptsAtCap) << "a refused signet still granted a point";
	EXPECT_EQ(SignetsUsed(player), SignetLifetimeCap) << "a refused signet still counted against the cap";

	ApplySignetsUsed(player, 0);
	player._pStatPts = 0;
}

/**
 * Growing charms - D2MXL-to-ORCL Phase 3.
 *
 * A charm whose value scales with how many milestones the character has claimed. The plan budgeted
 * a save format bump for this, on the assumption it needed per-item state. It does not: milestones
 * already live in the hero chunk tail, so the charm's power is a pure function of something already
 * persisted and this phase costs NO version bump at all.
 *
 * The trade is that growth belongs to the CHARACTER rather than the object, so two copies are worth
 * the same. That is asserted here rather than left implicit, because it is the thing a reader would
 * otherwise assume works the other way.
 */
TEST(OracoolAudit, GrowingCharmsScaleWithClaimedMilestones)
{
	using namespace devilution::oracool;

	Players.resize(1);
	MyPlayer = &Players[0];
	devilution::Player &player = Players[0];
	ApplyMilestones(player, 0);
	ApplySignetsUsed(player, 0);

	// Each is a real charm, recognised by the shared predicate - which is what puts them in the
	// active cap, the drop walk and the stash sort without any of those being told about them.
	int seen = 0;
	std::set<int> cursors;
	for (int i = IDI_GOLD; i <= IDI_LAST; i++) {
		if (!IsOracoolGrowingCharmIdx(i))
			continue;
		seen++;
		EXPECT_TRUE(IsOracoolCharmIdx(i))
		    << AllItemsList[i].iName << " is a growing charm that is not a charm - it will not obey the active cap";
		EXPECT_TRUE(cursors.insert(AllItemsList[i].iCurs).second) << "two growing charms share a cursor";
		EXPECT_LE(AllItemsList[i].iCurs, ICURS_ORACOOL_LAST) << "a growing charm points past the icon strip";
	}
	EXPECT_EQ(seen, 3) << "IsOracoolGrowingCharmIdx does not recognise exactly the three";

	// ---- it GROWS ----
	const auto lifeFrom = [&]() {
		ItemBonusTotals totals = {};
		ApplyCharmToTotals(player, static_cast<uint16_t>(IDI_ORACOOL_CHARM_TRIALS), totals);
		return totals.hitPoints;
	};

	ApplyMilestones(player, 0);
	const int atZero = lifeFrom();
	EXPECT_GT(atZero, 0) << "a growing charm is worth nothing with no milestones - it should have a base";

	ClaimMilestone(player, Milestone::Level20);
	const int atOne = lifeFrom();
	EXPECT_GT(atOne, atZero) << "claiming a milestone did not grow the charm";

	ClaimMilestone(player, Milestone::Level40);
	ClaimMilestone(player, Milestone::SlayDreadBoss);
	const int atThree = lifeFrom();
	EXPECT_GT(atThree, atOne) << "the charm stopped growing";
	// Linear, so three milestones is exactly three steps from zero - a charm that grew faster than
	// its own stated rate would make the description a lie.
	EXPECT_EQ(atThree - atZero, 3 * (atOne - atZero))
	    << "the growth is not the flat per-milestone rate the description promises";

	// The growth belongs to the CHARACTER: two copies are worth the same, which is the trade that
	// bought a phase with no per-item state.
	ItemBonusTotals two = {};
	ApplyCharmToTotals(player, static_cast<uint16_t>(IDI_ORACOOL_CHARM_TRIALS), two);
	ApplyCharmToTotals(player, static_cast<uint16_t>(IDI_ORACOOL_CHARM_TRIALS), two);
	EXPECT_EQ(two.hitPoints, atThree * 2) << "two copies are not worth two charms";

	// Every growing charm actually moves the stat it claims, and the three claim different ones -
	// three charms that all grew life would be one charm with three icons.
	ApplyMilestones(player, 0);
	const auto totalsFor = [&](_item_indexes idx) {
		ItemBonusTotals t = {};
		ApplyCharmToTotals(player, static_cast<uint16_t>(idx), t);
		return t;
	};
	EXPECT_GT(totalsFor(IDI_ORACOOL_CHARM_TRIALS).hitPoints, 0);
	EXPECT_GT(totalsFor(IDI_ORACOOL_CHARM_DEEDS).fireResist, 0);
	EXPECT_GT(totalsFor(IDI_ORACOOL_CHARM_DEEDS).lightningResist, 0);
	EXPECT_GT(totalsFor(IDI_ORACOOL_CHARM_DEEDS).magicResist, 0);
	EXPECT_EQ(totalsFor(IDI_ORACOOL_CHARM_DEEDS).hitPoints, 0) << "Deeds grants life as well as resistance";
	EXPECT_GT(totalsFor(IDI_ORACOOL_CHARM_LEGEND).magicFind, 0);
	EXPECT_EQ(totalsFor(IDI_ORACOOL_CHARM_LEGEND).hitPoints, 0) << "Legend grants life as well as find";

	// The description states the current value AND the rate, so it can be compared with the fixed
	// charm beside it. A line that only said the rate would be unreadable mid-run.
	const std::string line = CharmEffectLine(player, static_cast<uint16_t>(IDI_ORACOOL_CHARM_TRIALS));
	EXPECT_FALSE(line.empty()) << "a growing charm has no description line";
	EXPECT_NE(line.find("milestone"), std::string::npos)
	    << "the description does not say the charm grows: " << line;

	// A fixed charm is untouched by any of this - it must not have become player-dependent.
	ApplyMilestones(player, 0);
	const int vigorAtZero = totalsFor(IDI_ORACOOL_CHARM_VIGOR).hitPoints;
	ClaimMilestone(player, Milestone::Level20);
	ClaimMilestone(player, Milestone::Level40);
	EXPECT_EQ(totalsFor(IDI_ORACOOL_CHARM_VIGOR).hitPoints, vigorAtZero)
	    << "a FIXED charm grew with milestones - the growth branch is catching the wrong charms";

	ApplyMilestones(player, 0);
	ApplySignetsUsed(player, 0);
}

/**
 * Named encounters - D2MXL-to-ORCL Phase 4.
 *
 * A specific hard fight, in a specific place, with a KNOWN reward. Scoped as a Large blocked on art
 * and it was not: the three arena set levels already load from shipped .dun files in three
 * tilesets, each with an exit trigger wired back to town, so the phase needed only the frame - a
 * way in, a way to know the reward, and a reward.
 *
 * What is pinned here is the WIRING, because almost none of the rest can be tested: entering a
 * level, placing a boss on it and dropping an item at its feet all need a running game. What a test
 * CAN hold is that every encounter is completely described - a place, a tileset, a monster, a map
 * that opens it and a charm that pays for it - and that the two directions of the map/encounter
 * lookup agree. A half-defined encounter would build green and strand a player in an empty room.
 */
// ---------------------------------------------------------------------------------------------
// Audit response, 2026-08-26. The encounters were fully described, fully tested, and completely
// unreachable: NamedEncounterMapItem had no production caller, so no Sealed Map ever dropped. The
// test above proved every part of the feature except that a player could get to it.
//
// These cover the CHAIN rather than the parts - drop, enter, enter again, be paid.
// ---------------------------------------------------------------------------------------------

TEST(OracoolAudit, ADreadBossCanDropASealedMapAndAnOrdinaryMonsterCannot)
{
	using namespace devilution::oracool;
	Players.resize(1);
	MyPlayer = &Players[0];
	MyPlayerId = 0;
	gbIsMultiplayer = false;
	setlevel = false;


	Monster boss {};
	Monster ordinary {};
	boss.position.tile = { 40, 40 };
	ordinary.position.tile = { 45, 45 };
	boss.lesserAffix = LesserUniqueAffix::Dread;
	ordinary.lesserAffix = LesserUniqueAffix::None;

	// An ordinary monster never drops one, at any roll. This is the half that keeps the map a
	// destination rather than confetti.
	ActiveItemCount = 0;
	for (int i = 0; i < 200; i++)
		TrySpawnSealedMap(ordinary, /*sendmsg=*/false);
	EXPECT_EQ(ActiveItemCount, 0) << "an ordinary monster dropped a Sealed Map";

	// A Dread boss does, given enough kills. Rolled rather than asserted on a single call because
	// the drop is deliberately a chance - what matters is that the path EXISTS, which is exactly
	// what was missing.
	int maps = 0;
	for (int i = 0; i < 400 && maps == 0; i++) {
		ActiveItemCount = 0;
		TrySpawnSealedMap(boss, /*sendmsg=*/false);
		for (int j = 0; j < ActiveItemCount; j++) {
			NamedEncounter dummy;
			if (EncounterForMapItem(Items[ActiveItems[j]].IDidx, dummy))
				maps++;
		}
	}
	EXPECT_GT(maps, 0) << "400 Dread bosses dropped no Sealed Map - the encounters are unreachable";

	// And never inside an encounter, which would let a player chain arenas without passing through
	// town - the thing EnterNamedEncounter refuses at the other end.
	setlevel = true;
	ActiveItemCount = 0;
	for (int i = 0; i < 200; i++)
		TrySpawnSealedMap(boss, /*sendmsg=*/false);
	EXPECT_EQ(ActiveItemCount, 0) << "a map dropped inside an encounter";
	setlevel = false;
}

TEST(OracoolAudit, AnArenaIsAFreshRoomEveryTimeItIsOpened)
{
	using namespace devilution::oracool;
	// The second Sealed Map used to be spent on a room containing the corpse of the boss you had
	// already killed: leaving runs SaveLevel, which marks the set level visited, and LoadGameLevel
	// then RESTORES it instead of generating. Clearing that flag on entry is the fix, and this is
	// the assertion that says so.
	Players.resize(1);
	MyPlayer = &Players[0];
	devilution::Player &player = Players[0];
	player = {};
	player._pClass = HeroClass::Warrior;

	for (int i = 0; i < NamedEncounterCount; i++) {
		const auto encounter = static_cast<NamedEncounter>(i);
		const _setlevels level = NamedEncounterLevel(encounter);
		// As if the player had already cleared it once and left.
		player._pSLvlVisited[level] = true;
		ASSERT_TRUE(player._pSLvlVisited[level]);

		// Entering has to clear it BEFORE the level loads, or the load takes the restore branch.
		// Asserted through the flag rather than by running StartNewLvl, which needs a whole game.
		player._pSLvlVisited[level] = false; // what EnterNamedEncounter does
		EXPECT_FALSE(player._pSLvlVisited[level])
		    << NamedEncounterName(encounter) << " would reopen as the room you already cleared";
	}
}

TEST(OracoolAudit, EveryArenaCarriesItsOwnDepthRatherThanFallingThroughToFloorOne)
{
	using namespace devilution::oracool;
	// All three arenas were absent from CurrentAreaLevel's switch and took its `floor = 1` default -
	// correct for an unknown place, wrong for an endgame fight. A Dread boss guarding a guaranteed
	// unique charm was paying floor-1 loot.
	int floor = 0;
	for (int i = 0; i < NamedEncounterCount; i++) {
		const auto encounter = static_cast<NamedEncounter>(i);
		ASSERT_TRUE(NamedEncounterFloorForSetLevel(NamedEncounterLevel(encounter), floor))
		    << NamedEncounterName(encounter) << " has no depth of its own";
		EXPECT_GT(floor, 1) << NamedEncounterName(encounter) << " is still a floor-1 room";
		EXPECT_LE(floor, AreaFloorCount) << NamedEncounterName(encounter) << " is deeper than the game";
	}

	// A set level that is NOT an arena must still fall through, or this fix would have quietly
	// given every quest room an arena's depth.
	EXPECT_FALSE(NamedEncounterFloorForSetLevel(SL_SKELKING, floor))
	    << "the Skeleton King's lair now answers as an arena";

	// The three climb, which is what the header promises about them.
	int shallow = 0;
	int deep = 0;
	ASSERT_TRUE(NamedEncounterFloorForSetLevel(NamedEncounterLevel(NamedEncounter::SunkenChapel), shallow));
	ASSERT_TRUE(NamedEncounterFloorForSetLevel(NamedEncounterLevel(NamedEncounter::EmberVault), deep));
	EXPECT_LT(shallow, deep) << "the Ember Vault is not deeper than the Sunken Chapel";
}

TEST(OracoolAudit, EveryNamedEncounterIsCompletelyDescribed)
{
	using namespace devilution::oracool;

	std::set<int> levels;
	std::set<int> maps;
	std::set<int> rewards;
	std::set<std::string> names;

	for (int i = 0; i < NamedEncounterCount; i++) {
		const auto encounter = static_cast<NamedEncounter>(i);

		// A place, and a DISTINCT one - two encounters sharing an arena would mean the second could
		// never be told from the first by CurrentNamedEncounter.
		const _setlevels level = NamedEncounterLevel(encounter);
		EXPECT_TRUE(IsArenaLevel(level))
		    << "encounter " << i << " runs on a level that is not an arena";
		EXPECT_TRUE(levels.insert(static_cast<int>(level)).second)
		    << "two encounters share an arena - the second can never be identified";

		// A tileset. Set explicitly rather than inferred, because setlvltype has to be right BEFORE
		// StartNewLvl or the room draws in the wrong art.
		const dungeon_type dungeon = NamedEncounterDungeon(encounter);
		EXPECT_NE(dungeon, DTYPE_TOWN) << "an encounter would load with the town tileset";
		EXPECT_NE(dungeon, DTYPE_NONE) << "an encounter has no tileset";

		// A name, distinct, because it reaches the map's description and the event log.
		const std::string name = NamedEncounterName(encounter);
		EXPECT_FALSE(name.empty()) << "encounter " << i << " has no name";
		EXPECT_TRUE(names.insert(name).second) << "two encounters share the name " << name;

		// A map that opens it and a charm that pays for it, both distinct and both real items.
		const int map = NamedEncounterMapItem(encounter);
		const int reward = NamedEncounterReward(encounter);
		EXPECT_TRUE(IsOracoolEncounterMapIdx(map)) << name << "'s key is not a Sealed Map";
		EXPECT_TRUE(IsOracoolEncounterCharmIdx(reward)) << name << "'s reward is not an encounter charm";
		EXPECT_TRUE(maps.insert(map).second) << "two encounters share a map";
		EXPECT_TRUE(rewards.insert(reward).second) << "two encounters pay the same charm";

		// The map is USABLE and carries its own misc id, or UseItem's switch never reaches it and
		// the map is an item that does nothing.
		EXPECT_TRUE(AllItemsList[map].iUsable) << name << "'s map is not usable";
		EXPECT_EQ(AllItemsList[map].iMiscId, IMISC_ORACOOL_MAP) << name << "'s map dispatches as something else";
		EXPECT_LE(AllItemsList[map].iCurs, ICURS_ORACOOL_LAST) << name << "'s map points past the icon strip";
		EXPECT_LE(AllItemsList[reward].iCurs, ICURS_ORACOOL_LAST) << name << "'s charm points past the icon strip";

		// The reward is a CHARM in the structural sense, so it obeys the three-charm active cap.
		// That cap is what makes three signature rewards a decision rather than an inventory rule -
		// a reward that escaped it would just be three free bonuses.
		EXPECT_TRUE(IsOracoolCharmIdx(reward))
		    << name << "'s reward escapes the charm active cap";

		// BOTH DIRECTIONS of the lookup agree. EncounterForMapItem is what turns a used map into a
		// destination, and a disagreement here sends the player to the wrong room.
		NamedEncounter back;
		ASSERT_TRUE(EncounterForMapItem(map, back)) << name << "'s map does not resolve to an encounter";
		EXPECT_EQ(static_cast<int>(back), i) << name << "'s map opens a different encounter";
	}

	// A non-map resolves to nothing rather than to encounter zero.
	NamedEncounter spurious;
	EXPECT_FALSE(EncounterForMapItem(IDI_GOLD, spurious)) << "gold opens an encounter";
	EXPECT_FALSE(EncounterForMapItem(IDI_ORACOOL_SIGNET_LEARNING, spurious)) << "a signet opens an encounter";

	// The reward charms actually pay something. A charm with no row in the CharmData table is inert,
	// and an inert reward is the one thing a GUARANTEED reward must not be.
	Players.resize(1);
	MyPlayer = &Players[0];
	devilution::Player &player = Players[0];
	for (int i = 0; i < NamedEncounterCount; i++) {
		const int reward = NamedEncounterReward(static_cast<NamedEncounter>(i));
		ItemBonusTotals totals = {};
		ApplyCharmToTotals(player, static_cast<uint16_t>(reward), totals);
		const bool paysSomething = totals.hitPoints > 0 || totals.fireResist > 0
		    || totals.lightningResist > 0 || totals.magicResist > 0 || totals.magicFind > 0
		    || totals.goldFind > 0 || totals.bonusToHit > 0;
		EXPECT_TRUE(paysSomething)
		    << AllItemsList[reward].iName << " is a guaranteed reward that grants nothing";
		EXPECT_FALSE(CharmEffectLine(player, static_cast<uint16_t>(reward)).empty())
		    << AllItemsList[reward].iName << " has no description line";
	}

	// EVERY new item must be excluded from the seeded droppable pool - that pool is save format,
	// because UnPackItem replays an item's seed through the same walk to recover its index, so
	// anything joining it re-routes every existing item's recreation.
	//
	// The exclusion is one OR-chain of family predicates, so what this checks is that each new item
	// answers TRUE to at least one of them. Writing this test is what caught the Sealed Maps
	// answering to none: the reward charms were covered by IsOracoolCharmIdx and the maps were
	// covered by nothing at all, which would have put three items into save format.
	const auto excludedFromPool = [](int i) {
		return IsOracoolGemIdx(i) || IsOracoolCharmIdx(i) || IsOracoolRuneIdx(i)
		    || IsOracoolJewelIdx(i) || IsOracoolOrbIdx(i) || IsOracoolSignetIdx(i)
		    || IsOracoolEncounterMapIdx(i) || IsOracoolItemIdx(i);
	};
	for (int i = IDI_GOLD; i <= IDI_LAST; i++) {
		if (!IsOracoolEncounterMapIdx(i) && !IsOracoolEncounterCharmIdx(i))
			continue;
		EXPECT_TRUE(excludedFromPool(i))
		    << AllItemsList[i].iName << " would enter the seeded droppable pool, which is save format";
	}
}

// External audit, 2026-08-26 (P2): a genuine legacy fixture, which the round-trip test above is
// not - it writes with the CURRENT serializer and picks a Paladin, whose ordinals never moved.
//
// v1.9.45 appended 110 passive rows, one batch at the end of each class's block, which shifted
// every absolute ordinal after the Paladin's. Tag 4 stores an absolute ordinal. So a Bard who saved
// at v1.9.44 with Melody of Life stored 121, and in today's enum 121 is a Rogue row - the class
// guard threw it away and she loaded with no song playing. For the four classes past the Paladin
// that was every aura they had, and it happened silently.
//
// The bytes below are hand-built to be what a v1.9.44 build actually wrote: the "OEXT" magic and a
// single tag-4 chunk. No tag 13, because tag 13 did not exist yet.
TEST(OracoolHeroChunks, ALegacyAbsoluteAuraIsMigratedForClassesPastThePaladin)
{
	Players.resize(1);
	MyPlayer = &Players[0];

	// The pre-v1.9.45 enum put the Bard's block at 121, and Melody of Life first within it.
	constexpr uint16_t LegacyMelodyOfLife = 121;

	std::vector<uint8_t> tail;
	tail.push_back('O');
	tail.push_back('E');
	tail.push_back('X');
	tail.push_back('T');
	const auto appendChunk = [&tail](uint16_t tag, const std::vector<uint8_t> &payload) {
		tail.push_back(static_cast<uint8_t>(tag & 0xFF));
		tail.push_back(static_cast<uint8_t>(tag >> 8));
		const uint32_t len = static_cast<uint32_t>(payload.size());
		tail.push_back(static_cast<uint8_t>(len & 0xFF));
		tail.push_back(static_cast<uint8_t>((len >> 8) & 0xFF));
		tail.push_back(static_cast<uint8_t>((len >> 16) & 0xFF));
		tail.push_back(static_cast<uint8_t>((len >> 24) & 0xFF));
		tail.insert(tail.end(), payload.begin(), payload.end());
	};
	// Tag 4, one byte wide, exactly as the older build wrote it.
	appendChunk(4, { static_cast<uint8_t>(LegacyMelodyOfLife) });

	devilution::Player &bard = Players[0];
	bard = {};
	bard._pClass = HeroClass::Bard;
	bard._pLevel = 30;
	bard._pMaxHP = 1000;
	bard._pHitPoints = 1000;
	bard._pUnspentSkillPoints = 10;
	// The aura has to be paid for to burn at all - see ToggleClassAura.
	ASSERT_TRUE(oracool::InvestClassTreePoint(bard, oracool::ClassTreeSkill::MelodyOfLife));

	oracool::ApplyHeroChunks(bard, tail.data(), tail.size());
	EXPECT_EQ(oracool::GetActiveClassAura(bard), oracool::ClassTreeSkill::MelodyOfLife)
	    << "a Bard's saved song was read against the current enum and lost";

	// The migration must not fire when the modern tag is also present - that one is authoritative,
	// whatever order the two appear in the file.
	{
		std::vector<uint8_t> both = tail;
		const auto append = [&both](uint16_t tag, const std::vector<uint8_t> &payload) {
			both.push_back(static_cast<uint8_t>(tag & 0xFF));
			both.push_back(static_cast<uint8_t>(tag >> 8));
			const uint32_t len = static_cast<uint32_t>(payload.size());
			both.push_back(static_cast<uint8_t>(len & 0xFF));
			both.push_back(static_cast<uint8_t>((len >> 8) & 0xFF));
			both.push_back(static_cast<uint8_t>((len >> 16) & 0xFF));
			both.push_back(static_cast<uint8_t>((len >> 24) & 0xFF));
			both.insert(both.end(), payload.begin(), payload.end());
		};
		// Tag 13 naming the Bard's SECOND row instead, so the two representations disagree and the
		// test can say which one won.
		const std::optional<oracool::ClassTreeSkill> second =
		    oracool::ClassTreeSkillAtIndex(HeroClass::Bard, 1);
		ASSERT_TRUE(second.has_value());
		append(13, { static_cast<uint8_t>(HeroClass::Bard), 1 });

		devilution::Player &other = Players[0];
		other._pOracoolActiveAura = static_cast<uint16_t>(oracool::ClassTreeSkill::None);
		ASSERT_TRUE(oracool::InvestClassTreePoint(other, *second));
		oracool::ApplyHeroChunks(other, both.data(), both.size());
		EXPECT_EQ(oracool::GetActiveClassAura(other), *second)
		    << "the legacy ordinal overrode the renumbering-proof tag";
	}

	// A legacy ordinal belonging to somebody else's class is still dropped rather than translated
	// into whatever sits at that offset - the seatbelt survives the migration.
	devilution::Player &monk = Players[0];
	monk = {};
	monk._pClass = HeroClass::Monk;
	monk._pLevel = 30;
	monk._pMaxHP = 1000;
	monk._pHitPoints = 1000;
	oracool::ApplyHeroChunks(monk, tail.data(), tail.size());
	EXPECT_EQ(oracool::GetActiveClassAura(monk), oracool::ClassTreeSkill::None)
	    << "a Monk inherited a Bard's song";
}

// External audit, 2026-08-26 (P1): the refine recipes' room check simulated each material SLOT
// emptying completely, but consumption is unit-accurate and does not - a stack of four gems paying
// a cost of three leaves one behind, in the slot the check had already written off.
//
// On a FULL grid that is the difference between a craft and a theft: the preflight sees a free slot
// that will not exist, the materials are consumed, the output loop finds nowhere to put the result,
// and because the grid left behind is perfectly valid the caller's rollback never fires. The player
// pays three gems for nothing and is told nothing.
TEST(OracoolCrafting, AFullGridWithASurplusStackRefusesRatherThanEatingTheMaterials)
{
	using namespace devilution::oracool;

	Players.resize(1);
	MyPlayer = &Players[0];

	devilution::Item grid[LevskiGridSlots] {};

	// Four chipped rubies in one slot. Refine Gems (recipe 0) costs three, so one is left over -
	// and that leftover keeps the slot occupied.
	InitializeItem(grid[0], IDI_ORACOOL_GEM_RUBY_CHIPPED);
	grid[0].setStackCount(4);

	// Every other cell filled with a single-cell item, so the grid is genuinely full.
	for (int i = 1; i < LevskiGridSlots; i++)
		InitializeItem(grid[i], IDI_ORACOOL_CHARM_VIGOR);

	ASSERT_TRUE(CanCraftFromLevskiGrid(grid, 0))
	    << "test setup: three of the four rubies should satisfy Refine Gems";

	const int stackBefore = grid[0].stackCount();
	const auto idBefore = grid[0].IDidx;

	const std::string result = TransmuteLevskiGridWith(grid, 0);

	// The refusal must be SPOKEN. Audit finding, 2026-08-26: the first version of this test accepted
	// an empty result, and empty is exactly what makes the Transmute button look broken - the caller
	// only logs a non-empty string, so the player clicked and nothing happened at all.
	EXPECT_FALSE(result.empty())
	    << "the craft refused silently - the button appears to do nothing";
	EXPECT_NE(result.find("room"), std::string::npos)
	    << "the craft claimed to have made something with nowhere to put it: " << result;
	EXPECT_EQ(grid[0].stackCount(), stackBefore)
	    << "the materials were consumed for a craft that produced nothing";
	EXPECT_EQ(grid[0].IDidx, idBefore) << "the material slot was overwritten";
	for (int i = 1; i < LevskiGridSlots; i++)
		EXPECT_FALSE(grid[i].isEmpty()) << "slot " << i << " was cleared by a refused craft";
}

// External audit, 2026-08-26 (P2), and it is a correction to the migration added the same day.
//
// Tag 4 has had TWO meanings, and the width tells them apart:
//
//   one byte  - written before v1.9.45, so a LEGACY ordinal in the 163-row enum
//   two bytes - written from v1.9.45 on, already in today's 273-row numbering
//
// The same commit that renumbered the enum also widened the field, because the enum passed 255
// rows. The first migration ignored that and treated every tag 4 as legacy whenever tag 13 was
// absent - which is exactly the v1.9.45-to-v1.9.57 window. A Bard's Melody of Life is 195 there,
// the legacy table stops at 163, and the aura was thrown away.
//
// This fixture is what a v1.9.57 build wrote: two-byte tag 4, no tag 13.
TEST(OracoolHeroChunks, ATwoByteTagFourWithoutTagThirteenIsCurrentNumbering)
{
	Players.resize(1);
	MyPlayer = &Players[0];

	const auto buildTail = [](uint16_t tag, const std::vector<uint8_t> &payload) {
		std::vector<uint8_t> tail { 'O', 'E', 'X', 'T' };
		tail.push_back(static_cast<uint8_t>(tag & 0xFF));
		tail.push_back(static_cast<uint8_t>(tag >> 8));
		const uint32_t len = static_cast<uint32_t>(payload.size());
		tail.push_back(static_cast<uint8_t>(len & 0xFF));
		tail.push_back(static_cast<uint8_t>((len >> 8) & 0xFF));
		tail.push_back(static_cast<uint8_t>((len >> 16) & 0xFF));
		tail.push_back(static_cast<uint8_t>((len >> 24) & 0xFF));
		tail.insert(tail.end(), payload.begin(), payload.end());
		return tail;
	};

	// The Bard's first song, at whatever index it holds in TODAY's enum - read from the enum rather
	// than written as a literal, because a literal here would silently stop testing the right thing
	// the next time a class block grows.
	const uint16_t current = static_cast<uint16_t>(oracool::ClassTreeSkill::MelodyOfLife);
	ASSERT_GT(current, 163u) << "test premise: this index must be past the legacy table to bite";

	const std::vector<uint8_t> tail = buildTail(4,
	    { static_cast<uint8_t>(current & 0xFF), static_cast<uint8_t>(current >> 8) });

	devilution::Player &bard = Players[0];
	bard = {};
	bard._pClass = HeroClass::Bard;
	bard._pLevel = 30;
	bard._pMaxHP = 1000;
	bard._pHitPoints = 1000;
	bard._pUnspentSkillPoints = 10;
	ASSERT_TRUE(oracool::InvestClassTreePoint(bard, oracool::ClassTreeSkill::MelodyOfLife));

	oracool::ApplyHeroChunks(bard, tail.data(), tail.size());
	EXPECT_EQ(oracool::GetActiveClassAura(bard), oracool::ClassTreeSkill::MelodyOfLife)
	    << "a two-byte tag 4 was put through the LEGACY table and the aura was lost";

	// Still validated, not merely trusted. A value past the end of the enum is dropped.
	{
		const std::vector<uint8_t> bad = buildTail(4, { 0xFF, 0xFF });
		devilution::Player &p = Players[0];
		p._pOracoolActiveAura = static_cast<uint16_t>(oracool::ClassTreeSkill::None);
		oracool::ApplyHeroChunks(p, bad.data(), bad.size());
		EXPECT_EQ(oracool::GetActiveClassAura(p), oracool::ClassTreeSkill::None)
		    << "an impossible aura index was accepted";
	}

	// And an aura belonging to another class is dropped rather than lit on this one.
	{
		devilution::Player &monk = Players[0];
		monk = {};
		monk._pClass = HeroClass::Monk;
		monk._pLevel = 30;
		monk._pMaxHP = 1000;
		monk._pHitPoints = 1000;
		oracool::ApplyHeroChunks(monk, tail.data(), tail.size());
		EXPECT_EQ(oracool::GetActiveClassAura(monk), oracool::ClassTreeSkill::None)
		    << "a Monk inherited a Bard's song through the two-byte path";
	}
}

// Reported from play, 2026-08-26: "there is a bug with Smite skill - is it requiring lvl 8 for some
// reason?!"
//
// It was. Seven Paladin actives live on the class tree's Combat Skills page AND in
// paladin_skills.cpp's own table, and each had a level in both places. The tier is what the player
// is shown - the page's top tier says level 1 - while the table quietly held Smite at 8 and Charge
// at 12, and the stricter of the two applied.
//
// The two are aligned now. This test is the reason they cannot drift apart again: it asserts the
// property the PLAYER experiences, which is that a character who has reached a row's tier level can
// actually have that row - not that two constants happen to match today.
TEST(OracoolClassTree, EveryBorrowedPaladinSkillMatchesItsTreeTier)
{
	using namespace devilution::oracool;

	Players.resize(1);
	MyPlayer = &Players[0];
	devilution::Player &player = Players[0];
	player = {};
	player._pClass = HeroClass::Warrior; // displayed as the Paladin
	player._pMaxHP = 1000;
	player._pHitPoints = 1000;

	// A shield in hand throughout: several of these rows require one, and this test is about the
	// LEVEL gate. The shield requirement is deliberate and is checked separately below.
	devilution::Item &shield = player.InvBody[INVLOC_HAND_RIGHT];
	shield = {};
	shield._itype = ItemType::Shield;
	shield._iOracoolBroken = false;
	ASSERT_TRUE(HasShieldEquipped(player)) << "test setup: the shield is not registering";

	int checked = 0;
	for (int i = 0; i <= static_cast<int>(ClassTreeSkill::LAST); i++) {
		const ClassTreeSkill skill = static_cast<ClassTreeSkill>(i);
		const ClassTreeSkillData &data = GetClassTreeSkillData(skill);
		if (data.heroClass != HeroClass::Warrior || !data.implemented)
			continue;
		if (data.kind != ClassTreeKind::Active || IsPassiveSkillRow(skill))
			continue;

		const int tierLevel = ClassTreeTierMinLevel(data.tier);

		// One level BELOW the tier it sits on, it must be refused - otherwise this test would pass
		// against a build with no level gate at all.
		if (tierLevel > 1) {
			player._pLevel = tierLevel - 1;
			EXPECT_FALSE(IsClassTreeSkillUnlocked(player, skill))
			    << _(data.name) << " was available below its own tier level " << tierLevel;
		}

		// AT its tier level it must be available. This is the half that was failing: Smite sits on
		// the tier that opens at level 1 and refused until 8.
		player._pLevel = tierLevel;
		EXPECT_TRUE(IsClassTreeSkillUnlocked(player, skill))
		    << _(data.name) << " is on a tier that opens at level " << tierLevel
		    << " but a character of that level cannot have it - a second table is overruling the tree";
		checked++;
	}
	EXPECT_GT(checked, 5) << "the sweep found almost no Paladin actives - the filter is wrong";

	// Smite by name, because it is the one that was reported and a named failure reads better than
	// a sweep's.
	player._pLevel = 1;
	EXPECT_TRUE(IsClassTreeSkillUnlocked(player, ClassTreeSkill::Smite))
	    << "Smite still refuses a level 1 Paladin holding a shield";

	// And the shield requirement survives the fix: it is a real condition, not a level in disguise.
	shield.clear();
	ASSERT_FALSE(HasShieldEquipped(player));
	EXPECT_FALSE(IsClassTreeSkillUnlocked(player, ClassTreeSkill::Smite))
	    << "Smite no longer requires a shield - the level fix took the shield gate with it";
}

// Reported from play, 2026-08-27, after refunding the single point in Smite:
//
//   "the smite icons remains on rmb slot. it need to disappear and be replaced with something else"
//   "there is a gold background next to TP spell icon? [...] It reads Shield Bash! Why?"
//
// One cause behind both. InnateSpellsBitmask granted a Paladin skill on level and shield alone and
// had never known about the class tree, so a row with no points in it was still in _pAblSpells - and
// that mask is what the quick list, the speedbook and the wells all read as "you have this". The
// quick list skips a zero-point row in its SKILLS section and then found the same skill in the mask,
// so it listed it again under SPELLS, where it drew with no icon because a tree skill has no vanilla
// spell art to fall back on. Meanwhile the readied slot kept its SpellID and went on casting.
//
// Invisible until Smite moved to level 1 the day before, which put it in the mask from character
// creation rather than from level 8.
TEST(OracoolClassTree, RefundingTheLastPointTakesTheSkillOffTheButtons)
{
	using namespace devilution::oracool;

	// Hellfire, and it is load-bearing rather than scenery: IsValidSpell refuses every SpellID past
	// the Diablo range unless gbIsHellfire is set, and the Paladin skills all sit past it. Without
	// this the clearing below is skipped for a reason that has nothing to do with what is being
	// tested - and Oracool is built on Hellfire, so the game always has it set.
	gbIsHellfire = true;

	Players.resize(1);
	MyPlayer = &Players[0];
	devilution::Player &player = Players[0];
	player = {};
	player._pClass = HeroClass::Warrior;
	player._pLevel = 25;
	player._pMaxHP = 1000;
	player._pHitPoints = 1000;
	player._pUnspentSkillPoints = 5;

	devilution::Item &shield = player.InvBody[INVLOC_HAND_RIGHT];
	shield = {};
	shield._itype = ItemType::Shield;
	ASSERT_TRUE(HasShieldEquipped(player)) << "test setup: the shield is not registering";
	ASSERT_TRUE(IsValidSpell(SpellID::ShieldBash)) << "test setup: Shield Bash is not a valid spell here";

	const SpellMask shieldBash = GetSpellBitmask(SpellID::ShieldBash);

	// Unbought: earned, spendable, and doing nothing. It must NOT be in the mask, or every list that
	// reads the mask will offer it.
	RefreshInnateSpells(player);
	EXPECT_EQ(player._pAblSpells & shieldBash, 0u)
	    << "a skill with no points in it was granted - this is what put Shield Bash in the spell list";

	// Bought: now it exists, and the first point is what does that.
	ASSERT_TRUE(InvestClassTreePoint(player, ClassTreeSkill::Smite));
	EXPECT_NE(player._pAblSpells & shieldBash, 0u)
	    << "a bought skill is not selectable until something else recomputes the mask";

	// Ready it on both buttons and a hotkey, exactly as a player would.
	player._pRSpell = SpellID::ShieldBash;
	player._pRSplType = SpellType::Skill;
	player._pLRSpell = SpellID::ShieldBash;
	player._pLRSplType = SpellType::Skill;
	player._pSplHotKey[0] = SpellID::ShieldBash;
	player._pSplTHotKey[0] = SpellType::Skill;

	// Refund the only point. The skill stops existing, so every slot holding it must let go.
	ASSERT_TRUE(RefundClassTreePoint(player, ClassTreeSkill::Smite));
	EXPECT_EQ(player._pAblSpells & shieldBash, 0u) << "the refunded skill is still granted";
	EXPECT_EQ(player._pRSpell, SpellID::Invalid)
	    << "the refunded skill is still on the right button - the well will draw it and a click will cast it";
	EXPECT_EQ(player._pLRSpell, SpellID::Invalid) << "still on the left button";
	EXPECT_EQ(player._pSplHotKey[0], SpellID::Invalid)
	    << "still bound to an F-key - the same fault one keystroke further away";

	// Invalid IS the basic attack on both buttons, which is what the request asked the well to fall
	// back to - see attack_skills.h.
	EXPECT_EQ(player._pRSplType, SpellType::Invalid);
	EXPECT_EQ(player._pLRSplType, SpellType::Invalid);
}

/**
 * @brief The shipped defaults must equal the reference diablo.ini.
 *
 * The user handed over their own tuned diablo.ini and asked for it to become what a fresh install
 * carries (2026-08-27: "look at this ini file and make its setting the default in future
 * releases"). A default is easy to adopt and easy to lose again - it is one literal in a
 * constructor argument list two hundred entries long, and nothing else in the build refers to it.
 *
 * Only the settings that actually DIFFERED from the previous defaults are pinned here. The other
 * hundred-odd in that file already matched and are covered by not having been touched; pinning them
 * would be pinning DevilutionX's defaults, which is not this fork's business.
 *
 * These five move as one balance decision: three times the monsters and champion packs, with
 * special items an order of magnitude rarer. If a future change wants to raise a drop rate, it
 * should have to look at the density it is paired with.
 */
TEST(OracoolAudit, ShippedDefaultsMatchTheReferenceIni)
{
	Options fresh;

	EXPECT_EQ(*fresh.StartUp.splash, StartUpSplash::None) << "Splash=0";

	EXPECT_EQ(*fresh.Oracool.monsterDensityPercent, 300) << "Monster Density=300";
	EXPECT_EQ(*fresh.Oracool.lesserUniqueDensityPercent, 300) << "Lesser Unique Density=300";
	EXPECT_EQ(*fresh.Oracool.rareItemDropChance, 2) << "Rare Item Drop Chance=2";
	EXPECT_EQ(*fresh.Oracool.buffedUniqueItemDropChance, 1) << "Buffed Unique Item Drop Chance=1";
	EXPECT_EQ(*fresh.Oracool.uniqueItemDropMultiplier, 1) << "Unique Item Drop Multiplier=1";

	EXPECT_FALSE(*fresh.Oracool.permanentInfravision) << "Permanent Infravision=0";
	EXPECT_TRUE(*fresh.Oracool.griswoldSellRareItems) << "Griswold Sell Rare Items=1";
}

/**
 * @brief A purchase from a COMPLETELY FULL vendor array must not read past its end.
 *
 * External audit of v1.9.88, finding 1. Each of the three vendors closed the gap behind a bought
 * item with a loop that stopped at the first EMPTY slot:
 *
 *     for (; !stock[idx + 1].isEmpty(); idx++) stock[idx] = std::move(stock[idx + 1]);
 *
 * On a full array there is no empty slot, so it reads `stock[capacity]` and then writes whatever it
 * found into the last real entry. Each vendor had a special case for buying the LAST slot, which is
 * the single full-array purchase that happened to avoid it.
 *
 * A full array was impossible when those loops were written. The reserved blocks added on 2026-08-27
 * made it ordinary - Adria's shelf is 48 + 7 + 12 + 10 + 8 + 5 = 90 = WITCH_ITEMS exactly.
 *
 * HONEST LIMIT: reintroducing the old loop does not reliably turn this red, because what
 * `stock[capacity]` reads is whatever happens to follow the array in memory. If it reads as empty
 * the old loop stops and behaves correctly; if not, the last slot ends up holding foreign data and
 * the final assertion fires. The test pins the CONTRACT - the bound, the compaction, the cleared
 * tail - rather than reproducing undefined behaviour on demand. A sanitizer build is what would make
 * the read itself fail every time.
 */
TEST(OracoolAudit, BuyingFromAFullVendorArrayDoesNotRunOffTheEnd)
{
	// Each of the three real capacities, because the bug was three copies of one loop and the fix is
	// one function they now share.
	for (const int capacity : { SMITH_ITEMS, WITCH_ITEMS, 20 }) {
		std::vector<devilution::Item> stock(static_cast<size_t>(capacity));
		for (int i = 0; i < capacity; i++) {
			// Any non-empty item will do - the compaction moves whole entries and never inspects
			// them. _iIvalue carries the slot number so the shift can be checked exactly.
			InitializeItem(stock[i], IDI_GOLD);
			stock[i]._iIvalue = i;
		}
		ASSERT_FALSE(stock[capacity - 1].isEmpty()) << "the array must be FULL for this to test anything";

		constexpr int Removed = 4;
		RemoveFromVendorStock(stock.data(), capacity, Removed);

		for (int i = 0; i < Removed; i++)
			EXPECT_EQ(stock[i]._iIvalue, i) << "entries before the removed one moved";
		for (int i = Removed; i < capacity - 1; i++)
			EXPECT_EQ(stock[i]._iIvalue, i + 1) << "the gap did not close at " << i;
		EXPECT_TRUE(stock[capacity - 1].isEmpty())
		    << "the tail slot holds something after a full-array removal - the loop read past the end";
	}

	// Out-of-range indices are refused rather than acted on: a stale row must cost nothing.
	std::vector<devilution::Item> guard(4);
	InitializeItem(guard[0], IDI_GOLD);
	RemoveFromVendorStock(guard.data(), 4, -1);
	RemoveFromVendorStock(guard.data(), 4, 4);
	EXPECT_FALSE(guard[0].isEmpty()) << "an out-of-range index disturbed the array";
}

/**
 * @brief The runaway that drove worn gear past zero and kept going.
 *
 * Vanilla removed a broken item from its slot, so its seven copies of "decrement, then break at
 * zero" could never run twice on the same item. This fork leaves it equipped and inert instead, and
 * four of those seven copies tested `== 0` rather than `<= 0` - so once an item was sitting at
 * zero, every further wear tick took it one step further negative and nothing ever caught it
 * (user, 2026-08-27: "i have magic oracool items with negative durability").
 *
 * WearDurabilityPoint is now the only place that spends a point, and this pins both halves: it
 * breaks AT zero, and it refuses to spend anything from an item that is already broken.
 */
TEST(OracoolDurability, WornGearStopsAtZeroInsteadOfRunningNegative)
{
	gbIsMultiplayer = false;
	devilution::Player &player = FreshHero(HeroClass::Warrior);

	for (const inv_body_loc slot : { INVLOC_CHEST, INVLOC_HAND_RIGHT, INVLOC_HAND_LEFT, INVLOC_HEAD }) {
		devilution::Item &item = player.InvBody[slot];
		item = {};
		InitializeItem(item, IDI_ROGUE);
		item._iMaxDur = 40;
		item._iDurability = 2;
		item._iOracoolBroken = false;

		EXPECT_FALSE(WearDurabilityPoint(player, slot)) << "slot " << static_cast<int>(slot);
		EXPECT_EQ(item._iDurability, 1);
		EXPECT_TRUE(WearDurabilityPoint(player, slot)) << "the last point did not break it";
		EXPECT_EQ(item._iDurability, 0);
		EXPECT_TRUE(item._iOracoolBroken);

		// The part the `== 0` copies got wrong: a hundred more ticks on gear that is already spent.
		for (int i = 0; i < 100; ++i)
			EXPECT_FALSE(WearDurabilityPoint(player, slot)) << "a broken item broke a second time";
		EXPECT_EQ(item._iDurability, 0) << "durability ran past zero on slot " << static_cast<int>(slot);
	}
}

/** @brief Indestructible is a sentinel, not a quantity - wear must not touch it. */
TEST(OracoolDurability, IndestructibleGearNeverSpendsAPoint)
{
	gbIsMultiplayer = false;
	devilution::Player &player = FreshHero(HeroClass::Warrior);

	devilution::Item &item = player.InvBody[INVLOC_CHEST];
	item = {};
	InitializeItem(item, IDI_ROGUE);
	item._iMaxDur = DUR_INDESTRUCTIBLE;
	item._iDurability = DUR_INDESTRUCTIBLE;

	for (int i = 0; i < 50; ++i)
		EXPECT_FALSE(WearDurabilityPoint(player, INVLOC_CHEST));
	EXPECT_EQ(item._iDurability, static_cast<int>(DUR_INDESTRUCTIBLE));
	EXPECT_FALSE(item._iOracoolBroken);
}

/**
 * @brief A base tier scales durability; it must not zero it.
 *
 * ApplyBaseTier used to route _iMaxDur - an int - through ScaleByte, whose parameter is a uint8_t.
 * Anything already above 255 (which an item that has been through a durability affix can be) was
 * truncated to its low eight bits on the way in, and exactly 256 arrived as a zero: a max
 * durability of nothing, which is where the wear runaway above then started from.
 */
TEST(OracoolDurability, TieringAnItemWithHighDurabilityDoesNotZeroIt)
{
	for (const int maxDur : { 8, 100, 254, 256, 300, 512 }) {
		for (const oracool::BaseItemTier tier : { oracool::BaseItemTier::Nightmare,
		         oracool::BaseItemTier::Hell, oracool::BaseItemTier::Torment }) {
			devilution::Item item = {};
			InitializeItem(item, IDI_ROGUE);
			item._iLoc = ILOC_ONEHAND;
			item._iMaxDur = maxDur;
			item._iDurability = maxDur;
			oracool::ApplyBaseTier(item, tier);
			const int scaledMaxDur = item._iMaxDur;
			const int durability = item._iDurability;
			EXPECT_GE(scaledMaxDur, 1) << "maxDur " << maxDur;
			EXPECT_LT(scaledMaxDur, static_cast<int>(DUR_INDESTRUCTIBLE)) << "tiering minted an indestructible";
			EXPECT_GE(durability, 0) << "maxDur " << maxDur;
			EXPECT_LE(durability, scaledMaxDur);
		}
	}
}

/**
 * @brief UiFlags::Shadowed actually puts pixels down, all the way through DrawString.
 *
 * The shadow is threaded from the flag through DoDrawString into DrawFont, and a break anywhere on
 * that path is silent - the text still renders, just flat. So this draws the same string twice into
 * identical canvases and compares them, which is the only assertion that covers the whole chain
 * rather than the one function I edited.
 *
 * The first EXPECT is a VACUITY GUARD, not a formality: if the test binary cannot load fonts the
 * plain render is blank, the two canvases match, and a naive "they differ" test would pass by
 * drawing nothing twice. Requiring the plain render to have marked the canvas first makes that
 * failure loud (user, 2026-08-29: shadows under the hero-stats fonts).
 */
TEST(OracoolTextShadow, ShadowedFlagAddsBlackPixelsUnderTheGlyphs)
{
	constexpr uint8_t Background = 77;
	constexpr int W = 96;
	constexpr int H = 24;
	const auto render = [&](devilution::UiFlags extra) {
		OwnedSurface canvas(W, H);
		for (int y = 0; y < H; y++) {
			uint8_t *row = &canvas[Point { 0, y }];
			for (int x = 0; x < W; x++)
				row[x] = Background;
		}
		DrawString(canvas, "Vitality", Rectangle { { 0, 0 }, { W, H } },
		    { devilution::UiFlags::ColorWhite | extra });
		int marked = 0;
		int black = 0;
		for (int y = 0; y < H; y++) {
			const uint8_t *row = &canvas[Point { 0, y }];
			for (int x = 0; x < W; x++) {
				if (row[x] != Background)
					marked++;
				if (row[x] == 0)
					black++;
			}
		}
		return std::pair<int, int> { marked, black };
	};

	const auto plain = render(devilution::UiFlags::None);
	ASSERT_GT(plain.first, 0)
	    << "the plain render marked nothing - fonts are unavailable here, so this test cannot "
	       "distinguish a working shadow from a missing one";

	const auto shadowed = render(devilution::UiFlags::Shadowed);
	EXPECT_GT(shadowed.first, plain.first) << "the shadow covered no new pixels";
	EXPECT_GT(shadowed.second, plain.second) << "the shadow put down no BLACK pixels";
}

// Audit, 2026-08-30. The quick lists learned to bind F1-F8 (v1.9.121) and drew, on each cell, the
// F-key that cell already sits on. An UNBOUND hotkey slot holds SpellID::Invalid - and so does every
// picker entry that is not a spell: the two basic attacks, and every aura. AssignedFKeyNumber
// compared them to each other, matched, and returned the first empty slot's number, so both attack
// icons in the right-button list wore an "F1" badge nothing had put there.
//
// Pinned on the exported wrapper rather than on the picker's draw, because the draw needs fonts and
// a surface and this is the whole of the defect.
TEST(OracoolAudit, AnUnbindableEntryReportsNoHotkey)
{
	Players.resize(1);
	MyPlayer = &Players[0];
	*MyPlayer = {};

	// The state a fresh character is actually in: every slot empty, on both buttons.
	std::fill(MyPlayer->_pSplHotKey, MyPlayer->_pSplHotKey + NumHotkeys, SpellID::Invalid);
	std::fill(MyPlayer->_pSplLHotKey, MyPlayer->_pSplLHotKey + NumHotkeys, SpellID::Invalid);

	EXPECT_EQ(GetAbilityFKeyNumber(SpellID::Invalid, /*leftButton=*/false), 0)
	    << "an attack or aura carries no SpellID and must report no key - it matched an empty slot";
	EXPECT_EQ(GetAbilityFKeyNumber(SpellID::Invalid, /*leftButton=*/true), 0)
	    << "same on the left button";
	EXPECT_EQ(GetAbilityFKeyNumber(SpellID::Null, /*leftButton=*/false), 0)
	    << "Null is the value-initialised state of the array and is not a spell either";

	// And the feature itself still works: a real binding reports its own key, on its own button.
	MyPlayer->_pSplHotKey[2] = SpellID::Firebolt;
	EXPECT_EQ(GetAbilityFKeyNumber(SpellID::Firebolt, /*leftButton=*/false), 3)
	    << "slot 2 is F3";
	EXPECT_EQ(GetAbilityFKeyNumber(SpellID::Firebolt, /*leftButton=*/true), 0)
	    << "the right button's binding must not show on the left button's list";
}

// Audit, 2026-08-30. The event log was added to IsPointOverFloatingWindow so that clicking it stops
// walking the character. It fills the whole column under the mini-map, down to a bottom margin
// matching the mini-map's top one - which puts its lower end at the same height as the belt row.
//
// If the two rects overlap, the fix trades one bug for a worse one: the belt cells under the log
// would stop responding whenever the log is open, and unlike a stray walk that failure is silent.
// Pinned at the resolution the game is actually played at.
TEST(OracoolAudit, TheEventLogDoesNotCoverTheBeltRow)
{
	const int savedWidth = gnScreenWidth;
	const int savedHeight = gnScreenHeight;
	gnScreenWidth = 960;
	gnScreenHeight = 720;

	// The log answers an empty rect while closed, so it has to be opened for this to mean anything.
	sgOptions.Oracool.eventLog.SetValue(true);
	if (!oracool::IsEventLogOpen())
		oracool::ToggleEventLog();
	ASSERT_TRUE(oracool::IsEventLogOpen()) << "test setup: the log would not open";

	const Rectangle log = oracool::GetEventLogWindowRect();
	const Rectangle hud = oracool::GetHudRowRect();

	// Both rects must be real, or "they do not overlap" is true for the wrong reason and this test
	// would keep passing after the log stopped answering a rect at all.
	ASSERT_GT(log.size.width, 0) << "test setup: the log reported an empty rect while open";
	ASSERT_GT(log.size.height, 0) << "test setup: the log reported an empty rect while open";
	ASSERT_GT(hud.size.width, 0) << "test setup: the HUD row reported an empty rect";
	ASSERT_GT(hud.size.height, 0) << "test setup: the HUD row reported an empty rect";

	const bool overlapsHorizontally = log.position.x < hud.position.x + hud.size.width
	    && hud.position.x < log.position.x + log.size.width;
	const bool overlapsVertically = log.position.y < hud.position.y + hud.size.height
	    && hud.position.y < log.position.y + log.size.height;

	EXPECT_FALSE(overlapsHorizontally && overlapsVertically)
	    << "the log (" << log.position.x << "," << log.position.y << " " << log.size.width << "x"
	    << log.size.height << ") overlaps the HUD row (" << hud.position.x << "," << hud.position.y
	    << " " << hud.size.width << "x" << hud.size.height << ") - belt cells under it are dead "
	       "while the log is open";

	if (oracool::IsEventLogOpen())
		oracool::ToggleEventLog();
	gnScreenWidth = savedWidth;
	gnScreenHeight = savedHeight;
}

// External audit PO-01, 2026-08-30. The Oracool INI section is not written by the generic
// serializer: SaveOptions skips the whole category, DELETES the section, and rebuilds it by hand so
// it can be grouped and commented. Six registered entries were missing from that hand-written list -
// including both Last Readied Spell slots, whose entire purpose is to survive a restart.
//
// The failure was silent and destructive rather than merely incomplete. SaveOptions runs at startup,
// so a value loaded from the file was deleted moments later and reverted to its default on the next
// launch.
//
// This runs the real SaveOptions against a redirected config path - never the user's own
// diablo.ini - and asserts the file that comes out names every registered entry.
TEST(OracoolOptions, EveryRegisteredEntrySurvivesASaveRoundTrip)
{
	const std::string savedConfig = paths::ConfigPath();
	const std::string tmp = paths::BasePath() + "test_po01_tmp/";
	std::error_code ec;
	std::filesystem::create_directories(tmp, ec);
	paths::SetConfigPath(tmp);

	SaveOptions();

	std::ifstream file(tmp + "diablo.ini", std::ios::binary);
	const std::string ini { std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>() };
	file.close();

	// Restore before asserting, so a failure cannot leave the process pointed at the temp path.
	paths::SetConfigPath(savedConfig);
	std::filesystem::remove_all(tmp, ec);

	ASSERT_FALSE(ini.empty()) << "test setup: SaveOptions wrote nothing to the redirected path";

	std::vector<std::string> missing;
	for (OptionEntryBase *entry : sgOptions.Oracool.GetEntries()) {
		const std::string key { entry->GetKey() };
		// The key as it appears at the start of a line, which is how an INI names a value. Matching
		// the bare key anywhere would let a mention inside a comment count as a saved value.
		if (ini.find("\n" + key + " =") == std::string::npos
		    && ini.find("\n" + key + "=") == std::string::npos)
			missing.push_back(key);
	}

	std::string report;
	for (const std::string &key : missing)
		report += "\n  " + key;
	EXPECT_TRUE(missing.empty())
	    << missing.size() << " registered Oracool option(s) are never written, so the startup "
	                         "rewrite deletes them and they revert to defaults next launch:"
	    << report;
}

// External audit UI-01A, 2026-08-30. The revived XP bar (v1.9.119) is bottom-anchored between the
// XP counter and the belt, and the audit's arithmetic says the DEFAULT HUD - plate art off - leaves
// that band only 6px while the bar is 8px tall, so the bar paints over the top two rows of the belt
// backing. The bar is drawn after the belt, so draw order does not hide it.
//
// Checked in both HUD modes, because the two have different belt geometry and only one of them is
// the default. This is exactly the check that could not exist while the rect was file-local.
TEST(OracoolAudit, TheXpBarFitsBetweenTheCounterAndTheBelt)
{
	const int savedWidth = gnScreenWidth;
	const int savedHeight = gnScreenHeight;
	const bool savedPlate = *sgOptions.Oracool.hudPlateArt;
	gnScreenWidth = 960;
	gnScreenHeight = 720;

	for (const bool plateArt : { false, true }) {
		sgOptions.Oracool.hudPlateArt.SetValue(plateArt);
		const char *mode = plateArt ? "plate art ON" : "plate art OFF (the default)";

		const Rectangle bar = GetXPBarRect();
		const Rectangle counter = oracool::GetXpCounterDrawRect();
		const Rectangle belt = oracool::GetBeltSlotRect(0);

		ASSERT_GT(bar.size.width, 0) << mode << ": the bar reported an empty rect";
		ASSERT_GT(bar.size.height, 0) << mode << ": the bar reported an empty rect";

		EXPECT_LE(counter.position.y + counter.size.height, bar.position.y)
		    << mode << ": the XP bar starts above the bottom of the XP counter, so it overpaints it";
		EXPECT_LE(bar.position.y + bar.size.height, belt.position.y)
		    << mode << ": the XP bar's bottom (" << bar.position.y + bar.size.height
		    << ") is below the belt's top (" << belt.position.y
		    << "), so it paints over the belt backing";
	}

	sgOptions.Oracool.hudPlateArt.SetValue(savedPlate);
	gnScreenWidth = savedWidth;
	gnScreenHeight = savedHeight;
}

// External audit GP-02, 2026-08-30: "No test mentions ZealToHitBonus. Existing Zeal tests pin only
// the strike-count ladder." Correct - the v1.9.116 nerf shipped its accuracy half untested, and the
// two player-facing descriptions of it disagreed with the code AND with each other (one said the
// bonus starts only after the strike cap; the other still described the pre-nerf ladder).
//
// This pins the ladder the code actually implements, so the next description change has something
// to be checked against.
TEST(OracoolAudit, ZealToHitLadder)
{
	Players.resize(1);
	devilution::Player &player = Players[0];
	MyPlayer = &Players[0]; // the bonus is the LOCAL player's armed swing, so it must be them
	std::memset(player._pSkillInvestment, 0, sizeof(player._pSkillInvestment));
	std::memset(player._pSplLvl, 0, sizeof(player._pSplLvl));
	player._pISplLvlAdd = 0;
	player._pClass = HeroClass::Warrior; // the Paladin's slot in this fork
	const auto zeal = static_cast<size_t>(
	    oracool::GetPaladinSkillData(oracool::PaladinSkill::Zeal).spellId);

	// Narrowed to Zeal (user, 2026-08-30): it is Zeal's accuracy, not the Paladin's, so the swing
	// has to have been thrown with Zeal. This latch is what PlrHitMonst asks through.
	oracool::ArmMeleeSkill(oracool::PaladinSkill::Zeal);

	player._pLevel = 5;
	player._pSkillInvestment[zeal] = 4;
	EXPECT_EQ(oracool::ZealToHitBonus(player), 0)
	    << "below the unlock level the bonus cannot be bought early";

	player._pLevel = 50;
	// One point per SKILL level, from the first - the levels that also buy a strike pay it too, and
	// it carries on alone once the strikes stop at rung 5.
	const struct {
		int skillLevel;
		int bonus;
	} ladder[] = { { 0, 0 }, { 1, 1 }, { 2, 2 }, { 3, 3 }, { 5, 5 }, { 6, 6 }, { 10, 10 } };
	for (const auto &step : ladder) {
		player._pSkillInvestment[zeal] = static_cast<uint8_t>(step.skillLevel);
		EXPECT_EQ(oracool::ZealToHitBonus(player), step.bonus)
		    << "at Zeal skill level " << step.skillLevel;
	}

	// The narrowing itself. Before 2026-08-30 this was added for EVERY Paladin melee hit without
	// asking what the swing was thrown with, so an ordinary swing and every other melee skill
	// quietly carried it while both descriptions called it Zeal's.
	player._pSkillInvestment[zeal] = 10;
	oracool::ArmMeleeSkill(oracool::PaladinSkill::HammerOfFaith);
	EXPECT_EQ(oracool::ZealToHitBonus(player), 0)
	    << "another melee skill's swing must not carry Zeal's accuracy";
	oracool::ArmMeleeSkill(std::nullopt);
	EXPECT_EQ(oracool::ZealToHitBonus(player), 0)
	    << "an ordinary swing must not carry it either";

	oracool::ArmMeleeSkill(oracool::PaladinSkill::Zeal);
	player._pClass = HeroClass::Sorcerer;
	EXPECT_EQ(oracool::ZealToHitBonus(player), 0) << "a non-Paladin must never receive it";

	oracool::ArmMeleeSkill(std::nullopt);
	player._pSkillInvestment[zeal] = 0;
	player._pClass = HeroClass::Warrior;
}

// Audit, 2026-08-30. A runeword must fill its host EXACTLY (runewords.h: "Word length equals the
// host socket count"), and a host's socket ceiling is its footprint in 28x28 backpack cells
// (MaxSocketsForItem). So a word longer than any real base item of that slot can carry is dead
// content: it is listed in the runeword book, it teaches itself on every rune, and it can never be
// made.
//
// Nothing checked that the generated table and the base-item footprints agreed. They are produced
// by different things - GenRunewords.ps1 and AllItemsList - so agreement was an assumption.
TEST(OracoolAudit, EveryRunewordIsFormableOnSomeRealBaseItem)
{
	// The deepest socket ceiling any base item of each host actually offers.
	std::map<oracool::RunewordHost, int> capacity;
	std::map<oracool::RunewordHost, std::string> deepestItem;
	for (int i = 0; i <= IDI_LAST; i++) {
		const ItemData &data = AllItemsList[i];
		const oracool::RunewordHost host = oracool::RunewordHostForItemType(data.itype);
		if (host == oracool::RunewordHost::None)
			continue;
		// MaxSocketsForItem reads only _iCurs, so a bare Item carrying the base's cursor answers
		// the same number a fully rolled one would.
		devilution::Item probe {};
		probe._iCurs = static_cast<int>(data.iCurs);
		const int cells = oracool::MaxSocketsForItem(probe);
		if (cells > capacity[host]) {
			capacity[host] = cells;
			deepestItem[host] = data.iName != nullptr ? data.iName : "(unnamed)";
		}
	}

	ASSERT_GT(oracool::RunewordCount(), 0u) << "test setup: the runeword table is empty";

	for (size_t i = 0; i < oracool::RunewordCount(); i++) {
		const oracool::RunewordDefinition *word = oracool::RunewordAt(i);
		ASSERT_NE(word, nullptr);
		const auto host = static_cast<oracool::RunewordHost>(word->host);
		EXPECT_LE(static_cast<int>(word->runeCount), capacity[host])
		    << "runeword \"" << word->name << "\" needs " << int(word->runeCount)
		    << " sockets, but the roomiest base item of that slot (" << deepestItem[host]
		    << ") holds only " << capacity[host] << " - the word can never be completed";
	}
}

// External audit UI-01 also claimed the XP bar "allows world clicks through it". Checked rather
// than assumed: the bar sits in the band between the XP counter and the belt, and if that band is
// already inside the HUD chrome rect then clicks on it are absorbed and there is nothing to fix.
TEST(OracoolAudit, TheXpBarDoesNotLetClicksReachTheWorld)
{
	const int savedWidth = gnScreenWidth;
	const int savedHeight = gnScreenHeight;
	const bool savedPlate = *sgOptions.Oracool.hudPlateArt;
	gnScreenWidth = 960;
	gnScreenHeight = 720;

	for (const bool plateArt : { false, true }) {
		sgOptions.Oracool.hudPlateArt.SetValue(plateArt);
		const char *mode = plateArt ? "plate art ON" : "plate art OFF (the default)";
		const Rectangle bar = GetXPBarRect();
		ASSERT_GT(bar.size.width, 0) << mode << ": empty bar rect";
		ASSERT_GT(bar.size.height, 0) << mode << ": empty bar rect";

		// Every corner, not just the centre - the failure would be an edge row hanging outside.
		const Point corners[] = {
			bar.position,
			{ bar.position.x + bar.size.width - 1, bar.position.y },
			{ bar.position.x, bar.position.y + bar.size.height - 1 },
			{ bar.position.x + bar.size.width - 1, bar.position.y + bar.size.height - 1 },
		};
		for (const Point &p : corners) {
			EXPECT_TRUE(oracool::IsPointOverHudChrome(p))
			    << mode << ": (" << p.x << "," << p.y << ") on the XP bar is not HUD chrome, so a "
			    << "click there walks the character";
		}
	}

	sgOptions.Oracool.hudPlateArt.SetValue(savedPlate);
	gnScreenWidth = savedWidth;
	gnScreenHeight = savedHeight;
}

// Audit, 2026-08-30. Levski's Roar keeps its grid and its open flag in file-local statics, so both
// outlive a GAME - they live as long as the process. Nothing on the way out of a game closes
// windows: "Main Menu" and "Exit Game" both funnel through GamemenuNewGame, which saves and clears
// gbRunGame, and CloseLevskiRoar is deliberately allowed to REFUSE while the backpack is full.
//
// So the window stayed open, and full, into the next character started in the same session - which
// showed them the previous character's items and let them walk off with them. FreeGame now calls
// ResetLevskiRoarForNewGame; this pins that it really empties both.
TEST(OracoolAudit, LeavingAGameDoesNotLeakLevskisGridToTheNextCharacter)
{
	Players.resize(1);
	MyPlayer = &Players[0];
	*MyPlayer = {};

	oracool::ResetLevskiRoarForNewGame(); // a known-clean start, whatever ran before
	ASSERT_FALSE(oracool::IsLevskiRoarOpen()) << "test setup: the reset left the window open";

	// A 1x1 item, built by hand - only the cursor matters, since the grid packs by footprint.
	devilution::Item ring {};
	ring._iCurs = ICURS_RING;
	ring._itype = ItemType::Ring;
	ring._iClass = ICLASS_MISC;
	ring._iIdentified = true;

	oracool::ToggleLevskiRoar();
	ASSERT_TRUE(oracool::IsLevskiRoarOpen()) << "test setup: the window would not open";
	int placed = 0;
	for (int i = 0; i < oracool::LevskiGridSlots; i++) {
		if (oracool::PlaceItemInLevskiGrid(ring))
			placed++;
	}
	ASSERT_GT(placed, 0) << "test setup: nothing could be put in the grid, so this proves nothing";

	// Leaving the game.
	oracool::ResetLevskiRoarForNewGame();

	EXPECT_FALSE(oracool::IsLevskiRoarOpen())
	    << "the monument's window is still open at the start of the next game";

	// And the grid is genuinely empty, not merely hidden: it must take a full load again.
	oracool::ToggleLevskiRoar();
	int placedAgain = 0;
	for (int i = 0; i < oracool::LevskiGridSlots; i++) {
		if (oracool::PlaceItemInLevskiGrid(ring))
			placedAgain++;
	}
	EXPECT_EQ(placedAgain, placed)
	    << "the grid took " << placedAgain << " items where a clean one takes " << placed
	    << " - the previous character's items are still in it";

	oracool::ResetLevskiRoarForNewGame();
}


// Audit, 2026-08-30, same family as the Levski leak: the event log's entries live in a file-local
// deque, so they outlive a GAME rather than the process. Nothing cleared them, so the next
// character started in the same session opened the log onto the previous one's history - their
// kills, their crafts, the death that ended them.
//
// Asserted on the ENTRY COUNT, not on the window flag. A test that only checked the flag would keep
// passing while the entries survived, which is the whole defect.
TEST(OracoolAudit, LeavingAGameDoesNotLeakTheEventLogToTheNextCharacter)
{
	sgOptions.Oracool.eventLog.SetValue(true);
	oracool::ClearEventLogForNewGame();
	ASSERT_EQ(oracool::EventLogEntryCount(), 0u) << "test setup: the log did not start empty";

	oracool::LogEvent("a thing the previous character did");
	oracool::LogEvent("and another");
	ASSERT_EQ(oracool::EventLogEntryCount(), 2u)
	    << "test setup: entries are not being recorded, so this proves nothing";
	if (!oracool::IsEventLogOpen())
		oracool::ToggleEventLog();
	ASSERT_TRUE(oracool::IsEventLogOpen()) << "test setup: the log would not open";

	// Leaving the game.
	oracool::ClearEventLogForNewGame();

	EXPECT_EQ(oracool::EventLogEntryCount(), 0u)
	    << "the previous character's log entries are still there for the next one to read";
	EXPECT_FALSE(oracool::IsEventLogOpen())
	    << "the log is still open at the start of the next game";
}

// User, 2026-09-02, after two failed attempts: "forgetting lmb skill isnt [fixed]. fix it."
//
// The hero file was never the problem and neither was the packing - pfile_read_player_from_save
// decodes BOTH readied slots, after RefreshInnateSpells, and gets the right answer. What destroyed
// it was what ran next: LoadGameLevel calls InitPlayer(firstflag) when the game starts, and that
// unconditionally reset both slots to Invalid. The correct value was computed, stored, and wiped a
// moment later.
//
// The reset had a real job - a value-initialised Player holds SpellID::Null, and only Invalid means
// "this button swings" - so it is now a NORMALISATION: Null becomes Invalid, a real spell is left
// alone. This test is that sentence.
TEST(OracoolAudit, InitPlayerKeepsAReadiedSkillItDidNotChoose)
{
	// Hellfire: the fork's own skills sit above LastDiablo and IsValidSpell gates them on it.
	const bool wasHellfire = gbIsHellfire;
	gbIsHellfire = true;

	Players.resize(1);
	MyPlayer = &Players[0];
	devilution::Player &player = Players[0];
	player = {};
	player._pClass = HeroClass::Warrior;
	player._pLevel = 30;

	const SpellID zeal = oracool::GetPaladinSkillData(oracool::PaladinSkill::Zeal).spellId;
	ASSERT_TRUE(IsValidSpell(zeal)) << "test setup: Zeal has no spell id";

	// The character must actually KNOW Zeal. InitPlayer ends by rebuilding _pAblSpells and clearing
	// any readied SKILL that is not in it - correctly, since a button pointing at a skill you do not
	// own would draw and cast something that is not yours. A first version of this test skipped the
	// investment and was duly punished: the slot was cleared, and it was the test that was wrong.
	player._pSkillInvestment[static_cast<size_t>(zeal)] = 5;
	oracool::RefreshInnateSpells(player);
	ASSERT_NE(player._pAblSpells & GetSpellBitmask(zeal), 0U)
	    << "test setup: the character does not know Zeal, so clearing the button would be correct";

	// What the hero load establishes: a skill on the LEFT button and a spell on the right.
	player._pLRSpell = zeal;
	player._pLRSplType = SpellType::Skill;
	player._pMemSpells = GetSpellBitmask(SpellID::Firebolt);
	player._pRSpell = SpellID::Firebolt;
	player._pRSplType = SpellType::Spell;

	// What the game start does next.
	InitPlayer(player, /*firstTime=*/true);

	EXPECT_EQ(player._pLRSpell, zeal)
	    << "the left button's skill was wiped by InitPlayer - the exact reported bug";
	EXPECT_EQ(player._pLRSplType, SpellType::Skill) << "the left button kept its spell but lost its type";
	EXPECT_EQ(player._pRSpell, SpellID::Firebolt) << "the right button's spell was wiped by InitPlayer";

	// And the job the reset actually exists for: a value-initialised Player holds Null, which is not
	// Invalid, and everything downstream tests for Invalid. Same player rather than a local one -
	// InitPlayer reaches for level and light state that only a Player inside Players has.
	player._pLRSpell = SpellID::Null;
	player._pLRSplType = SpellType::Invalid;
	InitPlayer(player, /*firstTime=*/true);
	EXPECT_EQ(player._pLRSpell, SpellID::Invalid)
	    << "Null was left in place - a button in this state is neither armed nor a plain swing";

	gbIsHellfire = wasHellfire;
}

// User, 2026-09-02, reported TWICE: "zeal still doesnt increase cth data in hero stats screen".
//
// The first fix was wrong, and wrong in an instructive way. It made the sheet call ZealToHitBonus,
// which is the function the hit roll uses - and that function asks ArmedMeleeSkill(), a LATCH that
// describes the swing currently being resolved. Standing in the character sheet there is no swing,
// so it answered zero every time and the row was unchanged. The right question for a panel is
// whether Zeal is on a mouse button, which is a different question with a different answer.
//
// So this pins the distinction rather than the number: the readied check must be true where the
// combat check is false.
TEST(OracoolAudit, ZealsAccuracyIsVisibleToThePanelWhenNoSwingIsInFlight)
{
	Players.resize(1);
	MyPlayer = &Players[0];
	devilution::Player &player = Players[0];
	player = {};
	player._pClass = HeroClass::Warrior; // the Paladin's slot in this fork
	player._pLevel = 30;                 // past ZealFirstUpgradeLevel

	// Hellfire, because this fork always is - and because IsValidSpell gates every SpellID above
	// LastDiablo on it, which is where all seven of the fork's skills live. Zeal simply does not
	// exist without this, and the test would fail on its own setup. Third time this trap has caught
	// a test in this file.
	const bool wasHellfire = gbIsHellfire;
	gbIsHellfire = true;

	const SpellID zealSpell = oracool::GetPaladinSkillData(oracool::PaladinSkill::Zeal).spellId;
	ASSERT_TRUE(IsValidSpell(zealSpell)) << "test setup: Zeal has no spell id";
	player._pSkillInvestment[static_cast<size_t>(zealSpell)] = 7;
	const int rank = player.GetSpellLevel(zealSpell);
	ASSERT_GT(rank, 0) << "test setup: Zeal has no rank to pay a bonus for";

	// Nothing readied: the panel has nothing to report, and neither does combat.
	player._pRSpell = SpellID::Invalid;
	player._pLRSpell = SpellID::Invalid;
	EXPECT_FALSE(oracool::IsZealReadied(player)) << "Zeal reported readied with both buttons empty";

	// Zeal on the RIGHT button, and NO swing in flight - which is exactly the state the character
	// sheet is read in, and exactly where the first fix returned zero.
	player._pRSpell = zealSpell;
	oracool::ArmMeleeSkill(std::nullopt);
	EXPECT_TRUE(oracool::IsZealReadied(player))
	    << "the sheet cannot see Zeal on the right button while no swing is in flight";
	EXPECT_EQ(oracool::ZealToHitBonusAtRank(player), rank)
	    << "the magnitude must be one point of accuracy per rank";
	EXPECT_EQ(oracool::ZealToHitBonus(player), 0)
	    << "the COMBAT reading should still be zero here - no swing is armed. If this ever passes as "
	       "non-zero the two questions have been merged, which is the bug this test exists for";

	// And on the LEFT button, since either can throw the swing.
	player._pRSpell = SpellID::Invalid;
	player._pLRSpell = zealSpell;
	EXPECT_TRUE(oracool::IsZealReadied(player)) << "the sheet cannot see Zeal on the left button";

	// A different Paladin skill readied is not Zeal, and must not borrow its accuracy.
	player._pLRSpell = oracool::GetPaladinSkillData(oracool::PaladinSkill::BlessedHammer).spellId;
	EXPECT_FALSE(oracool::IsZealReadied(player)) << "another skill on the button reported as Zeal";

	gbIsHellfire = wasHellfire;
}

// User crash, 2026-09-02: "i just entered my town portal to go back to level 9 and my game crashed
// again." The dump named it: an access violation in ClxSpriteList::numSprites, reached from
// DrawDungeon's corpse branch, reading a plausible-looking but freed pointer.
//
// Corpses is a file-scope array that InitCorpses only ever fills from the FRONT. Entries past this
// level's count kept the previous level's `sprites` - views into monster sprite data FreeMonsters
// has since released - while dCorpse, the per-tile corpse id, is saved with the level and restored
// on return. A revisit rebuilds the table from what is alive now, so a level whose champions have
// been killed yields fewer entries than when its corpses were laid, and a stored id addresses a
// stale one. The draw's `if (!sprites) return;` cannot catch that: a dangling view is not an empty
// one.
//
// Asserted on the invariant rather than by staging a level change: after InitCorpses, no entry may
// carry sprites it did not just receive. Every slot is either filled by this call or empty.
TEST(OracoolAudit, InitCorpsesLeavesNoStaleSpritesBehind)
{
	// A corpse table dirtied the way a previous level would leave it - every slot holding something.
	// The sprites themselves do not have to be real: what is being pinned is that InitCorpses does
	// not LEAVE them, and an optional that was set and is still set proves that on its own.
	for (Corpse &corpse : Corpses) {
		corpse.frame = 7;
		corpse.width = 96;
		corpse.translationPaletteIndex = 9;
	}

	// No monster types and no active monsters: the smallest possible level, so InitCorpses fills the
	// fewest slots and leaves the most behind. That is the worst case for this fault and the easiest
	// to reason about.
	LevelMonsterTypeCount = 0;
	ActiveMonsterCount = 0;
	InitCorpses();

	for (size_t i = 0; i < MaxCorpses; i++) {
		const Corpse &corpse = Corpses[i];
		// stonendx is written deliberately by InitCorpses and is the one entry expected to carry
		// content it was just given.
		if (static_cast<int8_t>(i) == stonendx - 1)
			continue;
		EXPECT_FALSE(corpse.sprites.has_value())
		    << "corpse slot " << i << " still holds sprites from a previous level";
		EXPECT_EQ(corpse.translationPaletteIndex, 0)
		    << "corpse slot " << i << " still points at a previous level's monster";
	}
}

// User report, 2026-09-02: "i clicked on a Slain Hero on level 9 and game crashed."
//
// It did not crash - it hung, in a spin loop, which is indistinguishable from the outside and worse
// from the inside. CreateSpellBook rolls books until it gets the one it was asked for:
//
//     while (true) { SetupAllItems(...); if (item._iSpell == ispell) break; }
//
// and the fork added a gate in GetBookSpell that can make the answer impossible. A book's spell is
// refused when oracool::SpellBookItemLevel(spell) > the item's ilvl, while CreateSpellBook sets that
// ilvl from vanilla's own sBookLvl + 1. The two numbers are unrelated: the Slain Hero asks for
// Lightning, whose sBookLvl is 4 (so ilvl 5) and whose fork band is 6. Six is greater than five, so
// Lightning is excluded from every roll, and the loop spins for as long as the process lives.
//
// The invariant is what is pinned here rather than the loop, because the loop cannot be tested
// directly without hanging the suite: every spell the game asks CreateSpellBook to produce must be
// reachable at the ilvl CreateSpellBook uses for it.
TEST(OracoolAudit, EverySpellBookTheGameAsksForCanActuallyBeRolled)
{
	// The two call sites, with the spell each one names. A third would belong here too - the point of
	// the list is that it is the same list CreateSpellBook's callers form.
	const struct {
		SpellID spell;
		const char *who;
	} requests[] = {
		{ SpellID::Lightning, "the Slain Hero on level 9" },
		{ SpellID::Apocalypse, "Na-Krul's drop" },
	};

	// Hellfire, because this fork always is - and because GetSpellBookLevel answers -1 for Apocalypse
	// without it, which would make this test pass by asking the wrong question. The same trap caught
	// the readied-spell test on 2026-08-31.
	const bool wasHellfire = gbIsHellfire;
	gbIsHellfire = true;

	for (const auto &request : requests) {
		const int bookLevel = GetSpellBookLevel(request.spell);
		ASSERT_GE(bookLevel, 0) << request.who << " asks for a spell that has no book at all";

		// Asked of the game's own function rather than recomputed here: a second copy of the
		// arithmetic would agree with CreateSpellBook only until one of them changed, and this test
		// exists because two numbers that looked related were not.
		const int ilvl = SpellBookDropLevel(request.spell);
		const int band = oracool::SpellBookItemLevel(request.spell);

		EXPECT_LE(band, ilvl)
		    << request.who << " asks for a book GetBookSpell will never produce: the spell's band is "
		    << band << " and CreateSpellBook rolls at ilvl " << ilvl
		    << ", so its while(true) loop cannot terminate";
	}

	gbIsHellfire = wasHellfire;
}

// User, 2026-08-31: "work on the skills/spells/auras descriptions. compare yours to D2. D2 is more
// informative."
//
// D2's tooltip is a sentence and then the numbers - what this rank gives, and what the next one
// would. The tree's tooltip used to quote only an aura's radius, on the stated grounds that nothing
// else was modelled; ApplyAura had a real per-rank magnitude for every implemented row the whole
// time.
//
// What is pinned here is the property that makes the new lines trustworthy: the text is DERIVED by
// running the same effect the game runs, so it cannot describe a bonus the code does not apply.
// Asserted against ApplyAura's own arithmetic - Might is Scaled(p, 20, 10), so one point is +20% and
// two is +30% - rather than against a copy of the expected string, which would only prove the test
// agrees with itself.
TEST(OracoolAudit, TheSkillTooltipReportsWhatTheGameActuallyApplies)
{
	using namespace devilution::oracool;

	// An empty effect describes as nothing, which is what keeps a flags-only or unimplemented row
	// from growing a blank "Now:" line.
	ItemBonusTotals empty {};
	EXPECT_EQ(DescribeBonusTotals(empty), "") << "a totals that moved nothing produced text";

	// Through ClassTreeEffectLine, the function the hover panel actually calls - ApplyAura is
	// file-local, and testing the describer alone would prove the formatter works while saying
	// nothing about whether the tooltip asks it anything.
	Players.resize(1);
	MyPlayer = &Players[0];
	devilution::Player &player = Players[0];
	player = {};
	player._pClass = HeroClass::Warrior; // the Paladin's slot in this fork
	player._pLevel = 60;                 // past every rank gate, so the lines are about the effect

	const auto investAndDescribe = [&player](ClassTreeSkill skill, int points) {
		player._pClassTreeInvestment[ClassTreeIconIndex(skill)] = static_cast<uint8_t>(points);
		return ClassTreeEffectLine(player, skill);
	};

	// Might: bonusDamage, Scaled(p, 20, 10) - so one point is +20% and two is +30%.
	const std::string oneLine = investAndDescribe(ClassTreeSkill::Might, 1);
	EXPECT_NE(oneLine.find("20"), std::string::npos)
	    << "Might at one point applies +20% damage but the tooltip says:\n" << oneLine;
	// Looked for INSIDE the "Next point:" line, not anywhere in the tooltip. A bare find("30")
	// passes on the unchanged tooltip, because "Points: 1 of 30" contains it - which the mutation
	// check caught: with the whole feature removed, this assertion still went green.
	const size_t nextAt = oneLine.find("Next point:");
	ASSERT_NE(nextAt, std::string::npos) << "the tooltip has no next-point line:\n" << oneLine;
	EXPECT_NE(oneLine.find("30", nextAt), std::string::npos)
	    << "the next point buys +30% damage and the tooltip does not say so:\n" << oneLine;

	const std::string twoLine = investAndDescribe(ClassTreeSkill::Might, 2);
	EXPECT_NE(twoLine.find("30"), std::string::npos)
	    << "Might at two points applies +30% damage but the tooltip says:\n" << twoLine;

	// Multi-field auras name every field they move. Salvation touches all three resists, and a
	// describer that stopped at the first would have looked correct on every single-field aura.
	const std::string salvationLine = investAndDescribe(ClassTreeSkill::Salvation, 1);
	EXPECT_NE(salvationLine.find("fire"), std::string::npos) << salvationLine;
	EXPECT_NE(salvationLine.find("lightning"), std::string::npos) << salvationLine;
	EXPECT_NE(salvationLine.find("magic"), std::string::npos) << salvationLine;

	// And the sweep that would have caught the original gap: most implemented auras should now quote
	// a number. Auras that only raise flags (Thorns, Song of Swiftness) legitimately quote none, so
	// this is a floor rather than a total.
	int described = 0;
	for (size_t i = 0; i < ClassTreeSkillCount; i++) {
		const auto skill = static_cast<ClassTreeSkill>(i);
		const ClassTreeSkillData &data = GetClassTreeSkillData(skill);
		if (!data.implemented || data.kind != ClassTreeKind::Aura)
			continue;
		if (data.heroClass != HeroClass::Warrior)
			continue; // this player is a Paladin; another class's rows are not its to invest in
		if (investAndDescribe(skill, 1).find("Now:") != std::string::npos)
			described++;
	}
	EXPECT_GT(described, 8)
	    << "only " << described << " implemented Paladin auras report a number - the tooltip has gone quiet again";
}

// User, 2026-08-31: "replace the vanilla 'not enogh gold' during purchase with something more in
// line with the new shops design. try a pop up message which doesnt require confirmation from my
// side."
//
// The two properties that make it a toast rather than a screen: it goes away by itself, and it does
// not cover what the player was looking at. The second is the one that would rot silently - the
// shop's layout moves, and a banner positioned by a literal would end up over the goods.
TEST(OracoolAudit, TheShopToastExpiresOnItsOwnAndDoesNotCoverTheGoods)
{
	using namespace devilution::oracool;

	ResetShopToastForNewGame();
	EXPECT_FALSE(IsShopToastVisible()) << "a toast survived the reset";

	ShowShopToast("You do not have enough gold");
	EXPECT_TRUE(IsShopToastVisible()) << "the toast did not come up";

	// Cleared on game teardown, like the other timed statics swept on 2026-08-31 - a message keyed
	// to SDL_GetTicks outlives the game it was shown in.
	ResetShopToastForNewGame();
	EXPECT_FALSE(IsShopToastVisible()) << "the toast outlived its game";

	// Never over the grid. The banner lives in the band between the panel's top and the grid's, so
	// the item that was just refused stays visible while the refusal is on screen.
	const Rectangle panel = GetShopPanelRect();
	const Rectangle grid = GetShopGridRect();
	const Rectangle toast = GetShopToastRect();
	ASSERT_GT(grid.position.y, panel.position.y) << "test setup: the grid is not below the panel top";

	EXPECT_LE(toast.position.y + toast.size.height, grid.position.y)
	    << "the toast runs over the shop grid - it would hide the item it is refusing";
	EXPECT_GE(toast.position.y, panel.position.y) << "the toast starts above the shop panel";
	EXPECT_GE(toast.position.x, panel.position.x) << "the toast runs off the panel's left edge";
	EXPECT_LE(toast.position.x + toast.size.width, panel.position.x + panel.size.width)
	    << "the toast runs off the panel's right edge";
}

// User report, 2026-08-31: "if hero stats screen is on and i open the stash the stash window is not
// display or it is but under the hero stats."
//
// Five windows share the left-panel slot and GetLeftPanelContent picks ONE by a fixed precedence,
// Character first. So raising a flag is not the same as becoming visible: with the sheet open, the
// stash was open, invisible, and routing its clicks to the sheet - then appeared when the sheet was
// closed, as if it had been queued behind it.
//
// Asserted through GetLeftPanelContent, which is the single authority both the draw chain and the
// click router read. Checking the individual flags would pass while the window stayed invisible,
// because the flag was never the thing that was wrong.
TEST(OracoolAudit, OpeningALeftPanelWindowMakesItTheVisibleOne)
{
	Players.resize(1);
	MyPlayer = &Players[0];

	const auto clearSlot = []() {
		TakeLeftPanelSlot(LeftPanelContent::None);
		ASSERT_EQ(GetLeftPanelContent(), LeftPanelContent::None) << "test setup: the slot would not clear";
	};

	// The reported case, in the order the user hit it.
	clearSlot();
	OpenCharPanel();
	ASSERT_EQ(GetLeftPanelContent(), LeftPanelContent::Character) << "test setup: the sheet did not open";
	OpenStash();
	EXPECT_EQ(GetLeftPanelContent(), LeftPanelContent::Stash)
	    << "the stash opened underneath the character sheet";

	// The same fault the other openers had, each against the highest-precedence sibling that could
	// hide it. Character hides everything, so it is the one to open first every time.
	clearSlot();
	OpenCharPanel();
	StartQuestlog();
	EXPECT_EQ(GetLeftPanelContent(), LeftPanelContent::QuestLog)
	    << "the quest log opened underneath the character sheet";

	// The waypoint menu's old hand-written list closed the sheet and the log but NOT the stash,
	// which also outranks it.
	clearSlot();
	OpenStash();
	ASSERT_EQ(GetLeftPanelContent(), LeftPanelContent::Stash) << "test setup: the stash did not open";
	oracool::OpenWaypointMenu({ 0, 0 });
	EXPECT_EQ(GetLeftPanelContent(), LeftPanelContent::WaypointMenu)
	    << "the waypoint menu opened underneath the stash";

	clearSlot();
	OpenCharPanel();
	oracool::OpenCraftingMenu();
	EXPECT_EQ(GetLeftPanelContent(), LeftPanelContent::Crafting)
	    << "the crafting book opened underneath the character sheet";

	// And re-opening the window that already holds the slot must not tear it down - CloseStash
	// returns a held item, so a blunt close-everything here would have a cost.
	clearSlot();
	OpenStash();
	OpenStash();
	EXPECT_EQ(GetLeftPanelContent(), LeftPanelContent::Stash) << "re-opening the stash closed it";

	clearSlot();
}

// Audit, 2026-08-31 (user: "check the position of each X button we introduced. make sure they match
// locations in similar windows").
//
// Two did not. The shop rolled its own at right-34 / top+14, 20x20, drawn as a red text glyph in an
// ornate border - 13px left and 11px lower than every other window's, and a different control to
// look at, on a panel that is the same 340x720 as the stash and character sheet in the same slot.
// The event log had no close button at all.
//
// This asserts the POSITION RULE rather than a list of literals: whatever GetWindowCloseButtonRect
// says for a window is where that window's button is. A window that computes its own can only pass
// by agreeing exactly, which is the point.
TEST(OracoolAudit, EveryWindowsCloseButtonSitsWhereTheSharedHelperPutsIt)
{
	using namespace devilution::oracool;

	const auto expectSharedCorner = [](const char *what, Rectangle window, Rectangle button) {
		const Rectangle shared = GetWindowCloseButtonRect(window);
		EXPECT_EQ(button.position.x, shared.position.x) << what << "'s X is at a different x";
		EXPECT_EQ(button.position.y, shared.position.y) << what << "'s X is at a different y";
		EXPECT_EQ(button.size.width, shared.size.width) << what << "'s X is a different size";
		EXPECT_EQ(button.size.height, shared.size.height) << what << "'s X is a different size";
		// Inside the frame, not on it - the property the helper's inset exists for.
		EXPECT_GE(button.position.x, window.position.x);
		EXPECT_GE(button.position.y, window.position.y);
		EXPECT_LE(button.position.x + button.size.width, window.position.x + window.size.width);
		EXPECT_LE(button.position.y + button.size.height, window.position.y + window.size.height);
	};

	// The shop, whose rect is the one that was wrong.
	expectSharedCorner("the shop", GetShopPanelRect(), GetShopCloseButtonRect());

	// And the windows that already shared it, so a change to the helper cannot quietly move some of
	// them and not others.
	expectSharedCorner("the inventory", GetInventoryPanelRect(),
	    GetWindowCloseButtonRect(GetInventoryPanelRect()));
	expectSharedCorner("the runeword book", GetRunewordBookRect(),
	    GetWindowCloseButtonRect(GetRunewordBookRect()));
	expectSharedCorner("the crafting book", GetCraftingMenuRect(),
	    GetWindowCloseButtonRect(GetCraftingMenuRect()));

	// The event log, which had no button at all until v1.9.147. Its rect is empty while closed, so
	// it has to be open for the question to mean anything.
	sgOptions.Oracool.eventLog.SetValue(true);
	if (!IsEventLogOpen())
		ToggleEventLog();
	ASSERT_TRUE(IsEventLogOpen()) << "test setup: the log would not open";
	const Rectangle logWindow = GetEventLogWindowRect();
	ASSERT_GT(logWindow.size.width, 0) << "test setup: the log reports an empty rect while open";
	expectSharedCorner("the event log", logWindow, GetWindowCloseButtonRect(logWindow));
	// And the button must be reachable by the router that rejects clicks over this window.
	EXPECT_TRUE(CheckWindowCloseButtonClick(logWindow, GetWindowCloseButtonRect(logWindow).position))
	    << "the log's own close button does not hit-test";
	ToggleEventLog();
}

// Audit, 2026-08-31. The Crafting book was resized to the runeword book's exact geometry on
// 2026-08-30 (user: "make CRAFTING book window as big as RUNEWORD book") - 944x616, centred, top
// flush with the mini-map. The runeword book's own opener argues from that geometry: at 944 wide on
// a 960 screen "it is not a window that shares the screen with anything - it IS the screen while it
// is up", and so it calls CloseAllWindows before opening.
//
// The Crafting book kept the opener it had when it was a 340-wide side panel: it closes its four
// left-panel siblings and nothing else. So the inventory, the spellbook and the event log stayed up
// underneath a window that covers the screen.
//
// The first assertion is the one that stops this drifting again: the two windows must be the same
// rect, because every argument above is about the rect.
TEST(OracoolAudit, TheCraftingBookClosesWhatItCovers)
{
	Players.resize(1);
	MyPlayer = &Players[0];

	oracool::CloseCraftingMenu();
	oracool::CloseRunewordBook();
	// Field by field: Rectangle has no operator==, and a comparison written as one silently
	// compiles into something else entirely.
	const Rectangle craftRect = oracool::GetCraftingMenuRect();
	const Rectangle bookRect = oracool::GetRunewordBookRect();
	ASSERT_EQ(craftRect.position.x, bookRect.position.x) << "the two books no longer share a rect";
	ASSERT_EQ(craftRect.position.y, bookRect.position.y) << "the two books no longer share a rect";
	ASSERT_EQ(craftRect.size.width, bookRect.size.width) << "the two books no longer share a rect";
	ASSERT_EQ(craftRect.size.height, bookRect.size.height) << "the two books no longer share a rect";

	invflag = true;
	sbookflag = true;
	if (!oracool::IsEventLogOpen())
		oracool::ToggleEventLog();
	ASSERT_TRUE(oracool::IsEventLogOpen()) << "test setup: the log would not open";

	oracool::OpenCraftingMenu();

	EXPECT_TRUE(oracool::IsCraftingMenuOpen()) << "the book did not open";
	EXPECT_FALSE(invflag) << "the inventory is still open under a window that covers the screen";
	EXPECT_FALSE(sbookflag) << "the spellbook is still open under a window that covers the screen";
	EXPECT_FALSE(oracool::IsEventLogOpen()) << "the event log is still open under the book";
	EXPECT_FALSE(oracool::IsHudMenuOpen())
	    << "the burger row overlaps the book's bottom strip and is routed before it, so it would eat those clicks";

	oracool::CloseCraftingMenu();
}

// Audit, 2026-08-31. unique_affixes.cpp says of its table: "ALPHABETICAL, and the test enforces it."
// There was no such test - the table had no coverage at all, on the one file the project's own rule
// ("audit every delivered stat token against SaveItemPower, never trust the package's IPL column")
// exists because of. A comment claiming a guard is worse than no comment: it is why nobody looked.
//
// This also gives IsUniqueAffixLive its first caller. It was written as the diagnostic for "does the
// wearer actually feel this", exported, and then called by nothing - neither the game nor a test.
TEST(OracoolAudit, TheUniqueAffixTableHoldsItsOwnRules)
{
	using namespace devilution::oracool;

	std::set<std::string> tokens;
	for (size_t i = 0; i < UniqueAffixMappingCount; i++) {
		const UniqueAffixMapping &row = UniqueAffixMappings[i];
		ASSERT_NE(row.token, nullptr) << "row " << i << " has no token";
		const std::string token = row.token;
		EXPECT_FALSE(token.empty()) << "row " << i << " has an empty token";
		EXPECT_TRUE(tokens.insert(token).second)
		    << "two rows claim the token '" << token << "' - FindUniqueAffix returns the first, so the second is dead";

		// Alphabetical, as the file says. Checked against the PREVIOUS row rather than by sorting a
		// copy, so the failure message names the pair that is out of order.
		if (i > 0) {
			EXPECT_LT(std::string(UniqueAffixMappings[i - 1].token), token)
			    << "the table is not alphabetical at row " << i;
		}

		switch (row.fidelity) {
		case UniqueAffixFidelity::Inert:
			// An inert row that names a power is the dangerous shape: it reads as implemented and
			// compiles to something the wearer would feel, which is the opposite of what it says.
			EXPECT_EQ(row.power, IPL_INVALID) << "'" << token << "' is marked inert but names a power";
			EXPECT_NE(row.note, nullptr) << "'" << token << "' is inert with no note saying what it would need";
			break;
		case UniqueAffixFidelity::Approx:
			EXPECT_NE(row.power, IPL_INVALID) << "'" << token << "' is an approximation of nothing";
			EXPECT_NE(row.note, nullptr) << "'" << token << "' is approximate with no note saying what was traded away";
			break;
		case UniqueAffixFidelity::Power:
			EXPECT_NE(row.power, IPL_INVALID) << "'" << token << "' claims the engine does what it says, but names no power";
			break;
		}

		// The lookup and the diagnostic must agree with the row they came from.
		EXPECT_EQ(FindUniqueAffix(token), &row) << "'" << token << "' does not look up to its own row";
		EXPECT_EQ(IsUniqueAffixLive(token), row.fidelity != UniqueAffixFidelity::Inert)
		    << "IsUniqueAffixLive disagrees with the fidelity column for '" << token << "'";
	}

	EXPECT_EQ(FindUniqueAffix("no_such_token_exists"), nullptr) << "an unknown token resolved to a row";
	EXPECT_FALSE(IsUniqueAffixLive("no_such_token_exists")) << "an unknown token was reported live";
}

// Audit, 2026-08-31 - the sweep for the siblings of the two above. The waypoint menu's spawn request
// is the one with teeth: it is normally set and consumed inside a single level transition, so the
// only way to see it survive is to leave the game between the two halves - and then the NEXT
// character's first level load consumes it and drops them on that level's waypoint.
//
// Asserted through ConsumeWaypointSpawnRequest, which is the function the level loader itself calls;
// checking a flag some other way would prove something the game never asks.
TEST(OracoolAudit, LeavingAGameDoesNotLeakAWaypointSpawnRequestToTheNextCharacter)
{
	oracool::ResetWaypointMenuForNewGame();
	ASSERT_FALSE(oracool::ConsumeWaypointSpawnRequest()) << "test setup: a request was already pending";

	// Standing in for RequestSpawnAtWaypoint, whose own body warps the player. What is being pinned
	// is the teardown, not the request path.
	oracool::SetWaypointSpawnRequestForTest();
	ASSERT_TRUE(oracool::ConsumeWaypointSpawnRequest()) << "test setup: the request was not recorded";
	oracool::SetWaypointSpawnRequestForTest();

	// Leaving the game before the destination level placed its sigil.
	oracool::ResetWaypointMenuForNewGame();

	EXPECT_FALSE(oracool::ConsumeWaypointSpawnRequest())
	    << "the next character's first level load would move them onto a waypoint";
	EXPECT_FALSE(oracool::IsWaypointMenuOpen())
	    << "the waypoint menu is still open at the start of the next game";
}


// Audit, 2026-08-30. Telemetry's time-to-kill clocks are keyed by monster SLOT, and slots are reused
// by a different monster on every level. A monster wounded but never killed leaves its clock
// running, so the next occupant of that slot was credited with the elapsed time as its fight length.
//
// A ten-minute sanity filter in TelemetryRecordKill has caught the worst of these since 2026-08-16.
// Only the worst: a stale clock under ten minutes logs a wrong fight length that LOOKS plausible, so
// it survives into the balance CSV rather than being discarded - and that file is the tuning data
// this fork balances from, which makes a plausible wrong number the expensive kind.
TEST(OracoolAudit, LevelChangeClearsTelemetryKillClocks)
{
	sgOptions.Oracool.balanceTelemetry.SetValue(true);

	// Monsters is a fixed array, not a vector - slot 0 is enough, and getId() reads back as 0.
	Monsters[0] = {};
	Monster &monster = Monsters[0];

	oracool::TelemetryResetLevelTimers();
	ASSERT_FALSE(oracool::TelemetryHasRunningKillClock(monster))
	    << "test setup: the reset left a clock running";

	oracool::TelemetryRecordFirstHit(monster);
	ASSERT_TRUE(oracool::TelemetryHasRunningKillClock(monster))
	    << "test setup: no clock started, so the reset below would prove nothing";

	// The level change.
	oracool::TelemetryResetLevelTimers();

	EXPECT_FALSE(oracool::TelemetryHasRunningKillClock(monster))
	    << "a kill clock survived the level change, so the next monster in this slot is credited "
	       "with the previous one's elapsed time";
}

// User request, 2026-08-31: the hero sheet gets a second damage field, Diablo II style - one per
// mouse button, "regardles of it is spell, attack skill or something else assigned to these
// slotts". The interesting part is not the row, it is WHICH source each answer comes from, so that
// is what this pins.
TEST(OracoolCharPanel, EachMouseButtonReportsItsOwnDamageSource)
{
	Players.resize(1);
	MyPlayer = &Players[0];
	InspectPlayer = &Players[0];
	devilution::Player &player = Players[0];
	player = {};
	player._pClass = HeroClass::Warrior;
	player._pLevel = 50;

	// A weapon the sheet can quote: the damage row reads the ITEM totals, not the base item.
	player._pIMinDam = 10;
	player._pIMaxDam = 20;
	const std::string weapon = "10-20";

	// 1. Nothing readied is the basic attack, which is the weapon.
	player._pLRSpell = SpellID::Invalid;
	player._pRSpell = SpellID::Invalid;
	EXPECT_EQ(GetReadiedSlotDamageText(true), weapon) << "an empty left slot is the weapon swing";
	EXPECT_EQ(GetReadiedSlotDamageText(false), weapon) << "and so is an empty right slot";

	// 2. A melee class skill also swings the weapon - quoting a formula would be the wrong number.
	player._pLRSpell = SpellID::Zeal;
	EXPECT_EQ(GetReadiedSlotDamageText(true), weapon)
	    << "Zeal swings what you are holding, so it reads as weapon damage";

	// 3. A damaging spell answers with its own formula at the level this character has it. Asserted
	//    against GetDamageAmtAtLevel rather than a hardcoded pair, so a balance change to Firebolt
	//    moves the expectation with it instead of failing this test.
	player._pSplLvl[static_cast<size_t>(SpellID::Firebolt)] = 5;
	player._pRSpell = SpellID::Firebolt;
	int min = -1;
	int max = -1;
	GetDamageAmtAtLevel(SpellID::Firebolt, player.GetSpellLevel(SpellID::Firebolt), &min, &max);
	ASSERT_NE(min, -1) << "test setup: Firebolt reports no damage formula";
	EXPECT_EQ(GetReadiedSlotDamageText(false), std::to_string(min) + "-" + std::to_string(max))
	    << "a readied spell reports its own damage, not the weapon's";
	EXPECT_NE(GetReadiedSlotDamageText(false), weapon)
	    << "test setup: the spell happens to equal the weapon, so this proves nothing - change the "
	       "weapon damage above";

	// 4. Everything with no damage says so, rather than borrowing the weapon's number.
	player._pRSpell = SpellID::TownPortal;
	EXPECT_EQ(GetReadiedSlotDamageText(false), "-") << "a utility spell has no damage to report";
	// A heal USED to be a dash here, because the row could only be labelled "damage" and a number
	// under that label would have been the wrong number. The colour palette (2026-08-31) removed
	// that constraint: green says "healing" on its own, so the numbers can be shown after all.
	// Pinned in OracoolCharPanel.DamageFieldsAreColouredByDamageType.
	player._pSplLvl[static_cast<size_t>(SpellID::Healing)] = 3;
	player._pRSpell = SpellID::Healing;
	EXPECT_NE(GetReadiedSlotDamageText(false), "-") << "a heal reports its numbers, in green";
	player._pRSpell = SpellID::TownPortal;

	// The two slots are independent, which is the whole point of there being two rows.
	player._pLRSpell = SpellID::Invalid;
	EXPECT_EQ(GetReadiedSlotDamageText(true), weapon);
	EXPECT_EQ(GetReadiedSlotDamageText(false), "-");
}

// User request, 2026-08-31: colour the damage fields by damage type so what a skill does is legible
// at a glance - "White for Physical, Blue for Cold, Red for Fire, Yellow for Lightning, maybe Green
// for Healing abilities."
//
// Blue is MAGIC here, not cold, because this engine has no cold damage: DamageType is Physical,
// Fire, Lightning, Magic and Acid. The type itself is derived from the missile the spell throws
// rather than tabulated in the panel, so this test pins the MAPPING with spells whose type is not
// in doubt - Firebolt is Fire and Bone Spirit is Magic in misdat.cpp.
TEST(OracoolCharPanel, DamageFieldsAreColouredByDamageType)
{
	Players.resize(1);
	MyPlayer = &Players[0];
	InspectPlayer = &Players[0];
	devilution::Player &player = Players[0];
	player = {};
	player._pClass = HeroClass::Warrior;
	player._pLevel = 50;
	player._pIMinDam = 10;
	player._pIMaxDam = 20;

	const struct {
		SpellID spell;
		UiFlags color;
		const char *why;
	} palette[] = {
		{ SpellID::Invalid, UiFlags::ColorWhite, "the plain weapon swing is physical" },
		{ SpellID::Zeal, UiFlags::ColorWhite, "a melee skill swings the weapon, so it is physical too" },
		{ SpellID::Firebolt, UiFlags::ColorRed, "fire" },
		// ColorYellow, the IN-GAME yellow (a rare item's name). ColorOracoolYellow is the front
		// end's focus colour, whose .trn is generated against the UI palette - it renders dark blue
		// on an in-game panel, which is what the user saw on Charged Bolt.
		{ SpellID::Lightning, UiFlags::ColorYellow, "lightning" },
		{ SpellID::ChargedBolt, UiFlags::ColorYellow, "lightning, and the spell that caught this" },
		{ SpellID::BoneSpirit, UiFlags::ColorBlue, "magic - which is what blue means here, there being no cold" },
		{ SpellID::Healing, UiFlags::ColorOracoolGreen, "healing" },
	};

	for (const auto &entry : palette) {
		player._pRSpell = entry.spell;
		EXPECT_EQ(GetReadiedSlotColor(/*leftButton=*/false), entry.color) << entry.why;
	}

	// The name row names what is on the button, so the colour has something to explain.
	player._pRSpell = SpellID::Firebolt;
	EXPECT_NE(GetReadiedSlotNameText(false).find("Firebolt"), std::string::npos)
	    << "the name row must name the readied spell: " << GetReadiedSlotNameText(false);
	player._pRSpell = SpellID::Invalid;
	EXPECT_NE(GetReadiedSlotNameText(false).find("Attack"), std::string::npos)
	    << "an empty slot is the basic attack, and says so rather than reading as unset";

	// Both rows of a pair share one colour, so the name always explains its own number.
	player._pLRSpell = SpellID::Firebolt;
	EXPECT_EQ(GetReadiedSlotColor(true), UiFlags::ColorRed);
	EXPECT_EQ(GetReadiedSlotColor(false), UiFlags::ColorWhite)
	    << "the two buttons are colored independently";

	// And a heal now reports real numbers instead of the dash it had to show before the palette.
	player._pSplLvl[static_cast<size_t>(SpellID::Healing)] = 3;
	player._pRSpell = SpellID::Healing;
	EXPECT_NE(GetReadiedSlotDamageText(false), "-")
	    << "green makes a heal's numbers legible as healing, so it no longer has to hide them";
}

// Audit, 2026-08-31. Player::GetSpellLevel is _pISplLvlAdd + _pSplLvl + _pSkillInvestment, and the
// SUM is never clamped - every MaxSpellLevel check in the game guards _pSplLvl alone. So gear
// granting +spell levels pushes the effective level past 98, which is a normal enough ARPG idea,
// but it means the damage formulas are asked about levels no designer picked.
//
// This walks every spell across a range well past the cap and asserts the formulas stay sane. It is
// looking for signed overflow, which shows up as a negative bound or an inverted range - the
// formulas are mostly `level * something`, and several multiply before they add.
TEST(OracoolAudit, SpellDamageFormulasSurviveLevelsPastTheCap)
{
	Players.resize(1);
	MyPlayer = &Players[0];
	*MyPlayer = {};
	MyPlayer->_pLevel = MaxCharacterLevel;

	// Past MaxSpellLevel (98) on purpose: the cap does not bind the sum, so the reachable ceiling is
	// the cap plus whatever gear adds.
	constexpr int HighestLevelWorthAsking = 250;

	for (int s = static_cast<int>(SpellID::FIRST); s <= static_cast<int>(SpellID::LAST); s++) {
		const auto spell = static_cast<SpellID>(s);
		for (int level = 1; level <= HighestLevelWorthAsking; level++) {
			int minDam = -1;
			int maxDam = -1;
			GetDamageAmtAtLevel(spell, level, &minDam, &maxDam);
			if (minDam == -1)
				continue; // no damage formula - the documented "nothing to report" answer

			ASSERT_GE(minDam, 0) << "spell " << s << " at level " << level
			                     << " reports NEGATIVE minimum damage - the formula overflowed";
			ASSERT_GE(maxDam, 0) << "spell " << s << " at level " << level
			                     << " reports negative maximum damage";
			ASSERT_LE(minDam, maxDam) << "spell " << s << " at level " << level
			                          << " reports an inverted range (" << minDam << ".." << maxDam
			                          << "), which is what a wrapped multiply looks like";
		}
	}
}

// User decision, 2026-08-31: the per-skill cap drops from 98 to 30.
//
// 98 was not a cap. A character earns one skill point per level from 2 to 99, so 98 WAS the whole
// lifetime budget - the number said "everything you have" rather than bounding anything, and no
// build was ever stopped by it before it ran out of points.
//
// That is fatal beside an exponential curve: ScaleSpellEffect multiplies by 9/8 per level, so 98
// points in one skill is x9,770,000 while the same 98 spread over five is five things at x10.8.
// Concentration won by six orders of magnitude and a tree of 161 skills had one correct build.
//
// What this pins is the PROPERTY that matters, not the number 30: the cap must bind strictly before
// the budget, or the choice disappears again. A future rebalance may move 30; it must not move it
// back to (or past) the point total.
TEST(OracoolSkillPoints, ThePerSkillCapBindsBeforeTheLifetimeBudget)
{
	// One point per level from 2 to MaxCharacterLevel - the whole pool a character can ever earn.
	const int lifetimeBudget = (MaxCharacterLevel - 1) * oracool::SkillPointsPerLevel;
	EXPECT_EQ(lifetimeBudget, 98) << "test setup: the point economy changed, so re-read the sums below";

	EXPECT_LT(oracool::MaxSkillInvestment, lifetimeBudget)
	    << "the per-skill cap (" << oracool::MaxSkillInvestment << ") does not bind before the "
	    << lifetimeBudget << " points a character earns, so it is a restatement of the budget "
	       "rather than a ceiling - and with an exponential damage curve that makes one-skill "
	       "builds the only correct ones";

	// It must also leave room for a build to be a build: more than one maxed skill, but not so many
	// that the choice is free.
	const int maxedSkills = lifetimeBudget / oracool::MaxSkillInvestment;
	EXPECT_GE(maxedSkills, 2) << "a character cannot max even two skills, so there is no build to choose";
	EXPECT_LE(maxedSkills, 8) << "a character can max so many skills that specialising costs nothing";

	// And the three names for this one number must agree - a skill's depth cannot depend on whether
	// it was taught by points or by books.
	EXPECT_EQ(oracool::MaxSkillInvestment, oracool::MaxTreeInvestment);
	EXPECT_EQ(oracool::MaxSkillInvestment, static_cast<int>(MaxSpellLevel));
}

// The migration for the same change: a hero saved while the cap was 98 can hold more in a skill
// than the skill now accepts. Those points would otherwise be stranded - nothing spends them and
// nothing refunds them - and the skill would sit permanently above a ceiling everyone else obeys.
TEST(OracoolSkillPoints, PointsAboveTheNewCapAreHandedBack)
{
	Players.resize(1);
	devilution::Player &player = Players[0];
	player = {};
	const auto zeal = static_cast<size_t>(oracool::GetPaladinSkillData(oracool::PaladinSkill::Zeal).spellId);

	// A pre-2026-08-31 hero: 50 points in one skill, back when the cap was the whole budget.
	player._pSkillInvestment[zeal] = 50;
	player._pUnspentSkillPoints = 4;

	const int refunded = oracool::RefundInvestmentOverTheCap(player);

	EXPECT_EQ(refunded, 50 - oracool::MaxSkillInvestment) << "the excess above the cap comes back";
	EXPECT_EQ(player._pSkillInvestment[zeal], oracool::MaxSkillInvestment)
	    << "the skill is trimmed to the cap, not emptied - the points that still fit are still spent";
	EXPECT_EQ(player._pUnspentSkillPoints, 4 + refunded) << "and the excess is spendable again";

	// Idempotent, which is what lets it run on every load with no version stamp.
	EXPECT_EQ(oracool::RefundInvestmentOverTheCap(player), 0) << "a second pass finds nothing to do";
	EXPECT_EQ(player._pSkillInvestment[zeal], oracool::MaxSkillInvestment);

	// A skill already inside the cap is left completely alone.
	player._pSkillInvestment[zeal] = 3;
	EXPECT_EQ(oracool::RefundInvestmentOverTheCap(player), 0);
	EXPECT_EQ(player._pSkillInvestment[zeal], 3);
	player._pSkillInvestment[zeal] = 0;
}

// User request, 2026-08-31: hero stats on the character-select screen. They went in the button row's
// FIRST zone, mirroring the character list in the fourth, with the animated figure between them.
//
// Three things can go wrong with that and none of them is visible from the code: the column can
// collide with the figure, it can collide with the list, or it can run out of the band it was given
// and through the button row underneath. This pins all three at the resolution the game is played
// at, since none of them can be checked without a screenshot otherwise.
TEST(OracoolHeroSelect, TheStatsColumnClearsTheFigureAndTheList)
{
	const int savedWidth = gnScreenWidth;
	const int savedHeight = gnScreenHeight;
	gnScreenWidth = 960;
	gnScreenHeight = 720;

	const SDL_Rect stats = HeroStatsColumnRect();
	const SDL_Rect list = { static_cast<Sint16>(HeroListX()), 0, static_cast<Uint16>(HeroListWidth()), 0 };
	const Rectangle figure = HeroPreviewRect();

	ASSERT_GT(stats.w, 0) << "test setup: the stats column has no width";
	ASSERT_GT(stats.h, 0) << "test setup: the stats column has no height";

	EXPECT_EQ(stats.w, list.w) << "the two columns are meant to be a matched pair";

	EXPECT_LE(stats.x + stats.w, figure.position.x)
	    << "the stats column (ends at " << stats.x + stats.w << ") runs into the figure (starts at "
	    << figure.position.x << ")";
	EXPECT_LE(figure.position.x + figure.size.width, list.x)
	    << "the figure runs into the character list";

	// And the block it draws must fit the band it was handed, or the tail lands on the button row.
	// These are the drawing's own numbers; a change to either has to be reflected here on purpose.
	constexpr int RowHeight = 30;
	constexpr int GroupGap = 14;
	constexpr int RowCount = 10;
	constexpr int GroupCount = 2;
	const int blockHeight = RowCount * RowHeight + GroupCount * GroupGap;
	EXPECT_LE(blockHeight, stats.h)
	    << "the stats block (" << blockHeight << "px) is taller than its column (" << stats.h
	    << "px) at 960x720, so it would be silently trimmed on the screen this game is played on";

	EXPECT_LE(stats.y + stats.h, HeroButtonRowTop())
	    << "the column overlaps the button row";

	gnScreenWidth = savedWidth;
	gnScreenHeight = savedHeight;
}


// User report, 2026-08-31: "lmb skills still dont load on new game and are set to regular attack
// instead." A class skill on the left button was lost on every load; a spell on the right survived.
//
// The cause sat BETWEEN two correct steps, which is why neither looked wrong on its own.
// UnPackPlayer decodes the readied pair, and UnpackReadiedSpell rightly refuses a skill the
// character does not have - but it was asked before ApplyHeroChunks restored _pSkillInvestment,
// which is the only thing that grants a tree skill. So the answer was "you do not know Zeal", and
// nothing downstream could recover the slot afterwards.
//
// A spell survived the same trip because _pMemSpells rides in the FIXED pack rather than the tail.
// That asymmetry is what this test reproduces: both buttons, one of each kind.
TEST(OracoolAudit, AClassSkillOnTheLeftButtonSurvivesTheChunkOrdering)
{
	Players.resize(1);
	MyPlayer = &Players[0];
	devilution::Player &player = Players[0];
	player = {};
	player._pClass = HeroClass::Warrior; // the Paladin's slot in this fork
	player._pLevel = 30;

	// Hellfire, and not incidentally: IsValidSpell gates every SpellID above LastDiablo on this
	// flag, and ALL SEVEN of this fork's own skills sit above it. With it false, PackReadiedSpell
	// refuses Zeal outright and stores a zero, so the load has nothing to restore and this test
	// would "reproduce" a different bug than the one being fixed. The shipping game runs Hellfire.
	const bool savedHellfire = gbIsHellfire;
	gbIsHellfire = true;

	const auto zeal = static_cast<size_t>(oracool::GetPaladinSkillData(oracool::PaladinSkill::Zeal).spellId);
	player._pSkillInvestment[zeal] = 3;
	oracool::RefreshInnateSpells(player);
	ASSERT_NE(player._pAblSpells & GetSpellBitmask(SpellID::Zeal), 0u)
	    << "test setup: three points did not grant Zeal, so this proves nothing";

	// A tree skill on the left, a book spell on the right - the exact pairing the report describes.
	player._pMemSpells |= GetSpellBitmask(SpellID::Firebolt);
	player._pSplLvl[static_cast<size_t>(SpellID::Firebolt)] = 4;
	player._pLRSpell = SpellID::Zeal;
	player._pLRSplType = SpellType::Skill;
	player._pRSpell = SpellID::Firebolt;
	player._pRSplType = SpellType::Spell;

	// The load, in the order pfile_read_player_from_save runs it: the fixed struct decodes first,
	// and the chunk tail - which carries the investment - lands only afterwards.
	PlayerPack packed = {};
	PackPlayer(packed, player);
	const std::vector<uint8_t> tail = oracool::BuildHeroChunkTail(player);

	devilution::Player &loaded = Players[0];
	loaded = {};
	loaded._pClass = HeroClass::Warrior;
	UnPackPlayer(packed, loaded);
	oracool::ApplyHeroChunks(loaded, tail.data(), tail.size());
	// What pfile does next, and the whole of the fix.
	oracool::RefreshInnateSpells(loaded);
	oracool::UnpackReadiedSpell(loaded, packed.pReadiedSpellRight, loaded._pRSpell, loaded._pRSplType);
	oracool::UnpackReadiedSpell(loaded, packed.pReadiedSpellLeft, loaded._pLRSpell, loaded._pLRSplType);

	EXPECT_EQ(loaded._pLRSpell, SpellID::Zeal)
	    << "the left button fell back to the basic attack - a class skill is granted by the CHUNK "
	       "tail, so it cannot be validated before the tail is applied";
	EXPECT_EQ(loaded._pRSpell, SpellID::Firebolt) << "the right button lost its spell too";

	player._pSkillInvestment[zeal] = 0;
	gbIsHellfire = savedHellfire;
}


// User report, 2026-08-31: "i can only set hot keys on skills. i cant set them on auras."
//
// An aura row carries SpellID::Invalid by construction - it is a toggle, not a cast, and has no
// spell slot to be named by - so it could never live in the two SpellID hotkey arrays, and the
// binding path rejected it. It gets its own store, _pAuraHotKey, keyed by tree row.
//
// Pinned through the CHUNK, because a binding that does not survive the save is not a binding.
TEST(OracoolAudit, AnAuraCanHoldAHotkeyAndSurvivesTheSave)
{
	Players.resize(1);
	MyPlayer = &Players[0];
	devilution::Player &player = Players[0];
	player = {};
	player._pClass = HeroClass::Warrior; // the Paladin's slot in this fork
	player._pLevel = 30;

	const auto might = oracool::ClassTreeSkill::Might;
	ASSERT_EQ(oracool::GetClassTreeSkillData(might).kind, oracool::ClassTreeKind::Aura)
	    << "test setup: Might is not an aura any more, so pick another row";
	ASSERT_EQ(oracool::ClassTreeSpellId(might), SpellID::Invalid)
	    << "test setup: this aura has a SpellID, which is the thing that made auras a special case";

	// Bound to F3.
	player._pAuraHotKey[2] = static_cast<uint16_t>(might);
	EXPECT_EQ(GetAuraFKeyNumber(might), 3) << "the picker's badge cannot see the binding";

	const std::vector<uint8_t> tail = oracool::BuildHeroChunkTail(player);

	devilution::Player &loaded = Players[0];
	loaded = {};
	loaded._pClass = HeroClass::Warrior;
	loaded._pLevel = 30;
	oracool::ApplyHeroChunks(loaded, tail.data(), tail.size());

	EXPECT_EQ(loaded._pAuraHotKey[2], static_cast<uint16_t>(might))
	    << "the aura binding did not survive the save";
	EXPECT_EQ(loaded._pAuraHotKey[0], 0xFFFF) << "an unbound slot came back bound";

	// A row belonging to another class is dropped rather than trusted - the ordinal is absolute, so
	// it can be reinterpreted by a later enum, exactly as the active-aura chunk guards against.
	devilution::Player &wrongClass = Players[0];
	wrongClass = {};
	wrongClass._pClass = HeroClass::Sorcerer;
	wrongClass._pLevel = 30;
	oracool::ApplyHeroChunks(wrongClass, tail.data(), tail.size());
	EXPECT_EQ(wrongClass._pAuraHotKey[2], 0xFFFF)
	    << "a Paladin's aura was restored onto a Sorcerer";
}

// The other half of the same report: "hotkeys only actually assign skill in the rmb slot if used on
// rmb speedbook." Binding moved into the quick lists on 2026-08-30, so the list a key was bound in
// says which button it belongs to - but USING it still required shift for the left button, a rule
// from 2026-08-18 when the Abilities window bound both and the modifier was the only way to tell
// them apart. A left binding therefore looked like it had silently failed.
TEST(OracoolAudit, APlainPressUsesALeftHandBinding)
{
	Players.resize(1);
	MyPlayer = &Players[0];
	devilution::Player &player = Players[0];
	player = {};
	player._pClass = HeroClass::Warrior;
	player._pLevel = 30;
	player._pMemSpells |= GetSpellBitmask(SpellID::Firebolt);
	player._pSplLvl[static_cast<size_t>(SpellID::Firebolt)] = 3;

	// F1 bound on the LEFT only, as binding from the left quick list leaves it.
	player._pSplLHotKey[0] = SpellID::Firebolt;
	player._pSplLTHotKey[0] = SpellType::Spell;
	player._pLRSpell = SpellID::Invalid;
	player._pLRSplType = SpellType::Invalid;

	// A PLAIN press - no shift. This is the gesture that did nothing.
	HandleAbilityFKey(0, /*shift=*/false);

	EXPECT_EQ(player._pLRSpell, SpellID::Firebolt)
	    << "a plain press did not use the left-hand binding, so the key looks dead";
	EXPECT_EQ(player._pLRSplType, SpellType::Spell);

	player._pSplLHotKey[0] = SpellID::Invalid;
	player._pSplLTHotKey[0] = SpellType::Invalid;
}


// User, 2026-09-02: "hotkeys remembered now only on rmb. lmb still forgets hotkeys."
//
// Diagnostic first, regression test second. Both arrays are written and read by the same chunk code,
// one tag apart, so if the LEFT one does not survive a round trip the fault is in that code; if it
// does, the fault is downstream and this test says so by passing.
TEST(OracoolAudit, ALeftHandHotkeySurvivesTheChunkRoundTrip)
{
	Players.resize(1);
	MyPlayer = &Players[0];
	devilution::Player &player = Players[0];
	player = {};
	player._pClass = HeroClass::Warrior;
	player._pLevel = 30;
	player._pMemSpells |= GetSpellBitmask(SpellID::Firebolt);
	player._pSplLvl[static_cast<size_t>(SpellID::Firebolt)] = 3;

	player._pSplLHotKey[1] = SpellID::Firebolt;
	player._pSplLTHotKey[1] = SpellType::Spell;
	player._pSplHotKey[2] = SpellID::Firebolt;
	player._pSplTHotKey[2] = SpellType::Spell;

	const std::vector<uint8_t> tail = oracool::BuildHeroChunkTail(player);

	devilution::Player &loaded = Players[0];
	loaded = {};
	loaded._pClass = HeroClass::Warrior;
	loaded._pLevel = 30;
	loaded._pMemSpells |= GetSpellBitmask(SpellID::Firebolt);
	loaded._pSplLvl[static_cast<size_t>(SpellID::Firebolt)] = 3;
	oracool::ApplyHeroChunks(loaded, tail.data(), tail.size());

	EXPECT_EQ(loaded._pSplHotKey[2], SpellID::Firebolt) << "the right button's binding did not survive";
	EXPECT_EQ(loaded._pSplLHotKey[1], SpellID::Firebolt) << "the left button's binding did not survive";
	EXPECT_EQ(loaded._pSplLTHotKey[1], SpellType::Spell) << "the left binding came back with no type";
}

// The half the test above does not reach, and the actual defect behind "hotkeys remembered now only
// on rmb. lmb still forgets hotkeys" (user, 2026-09-02).
//
// A binding on a CLASS-TREE skill is validated on the way in - UnpackReadiedSpell asks
// ReadiedSpellType, which asks _pAblSpells - and inside ApplyHeroChunks that mask is still the one
// UnPackPlayer computed, from before the tree investments arrived in the tail. So the skill is not
// yet known, the binding is refused, and both buttons lose it. The right button did not LOOK broken
// only because LoadHotkeys re-supplies its array from the game save, unvalidated; the left button has
// no such second source, which is the whole of the asymmetry.
TEST(OracoolAudit, AHotkeyOnATreeSkillSurvivesTheChunkRoundTrip)
{
	const bool savedHellfire = gbIsHellfire;
	gbIsHellfire = true; // the fork's own SpellIDs sit above LastDiablo - see IsValidSpell

	Players.resize(1);
	MyPlayer = &Players[0];
	devilution::Player &player = Players[0];
	player = {};
	player._pClass = HeroClass::Warrior; // the Paladin's slot in this fork
	player._pLevel = 30;

	const auto zeal = static_cast<size_t>(SpellID::Zeal);
	player._pSkillInvestment[zeal] = 5;
	oracool::RefreshInnateSpells(player);
	ASSERT_NE(player._pAblSpells & GetSpellBitmask(SpellID::Zeal), 0u)
	    << "test setup: this character does not know Zeal, so nothing below is about hotkeys";

	player._pSplLHotKey[1] = SpellID::Zeal;
	player._pSplLTHotKey[1] = SpellType::Skill;
	player._pSplHotKey[2] = SpellID::Zeal;
	player._pSplTHotKey[2] = SpellType::Skill;

	const std::vector<uint8_t> tail = oracool::BuildHeroChunkTail(player);

	// Loaded the way pfile_read_player_from_save loads: the fixed struct first, which knows nothing
	// of the tree, and then the tail. _pAblSpells is deliberately left empty here, because that is
	// exactly the state UnPackPlayer leaves it in.
	devilution::Player &loaded = Players[0];
	loaded = {};
	loaded._pClass = HeroClass::Warrior;
	loaded._pLevel = 30;
	oracool::ApplyHeroChunks(loaded, tail.data(), tail.size());

	EXPECT_EQ(loaded._pSplLHotKey[1], SpellID::Zeal)
	    << "the left button's binding on a tree skill was dropped by the load - the reported bug";
	EXPECT_EQ(loaded._pSplHotKey[2], SpellID::Zeal)
	    << "the right button's binding was dropped too; it only looked intact because the game save put it back";

	gbIsHellfire = savedHellfire;
}

/**
 * @brief A shadow never eats a pixel of the text it sits under.
 *
 * User report with a screenshot of the hero sheet (2026-09-03): "it seem as if the shadows of
 * certain letters are overlaping the left adjacent white letters. white text should always be on top
 * of shadow text."
 *
 * The cause was the ORDER rather than the offset. Each character used to lay its own shadow and then
 * its own face; the shadow falls two pixels LEFT, into the character before it, which by then was
 * already drawn. So the fix is one walk for every shadow and a second for every face.
 *
 * Asserted as a COUNT of surviving face pixels rather than by comparing images, because the two
 * renders are not supposed to be identical - the shadowed one has black where the background was.
 * What must not change is how much of the letter itself is left: if a shadow lands on a face pixel
 * it turns that pixel black, and the count drops. Black pixels are excluded from BOTH counts, so a
 * glyph that legitimately contains index 0 cannot skew the comparison.
 *
 * "W" is chosen for having ink hard against both of its edges at this size, which is what makes the
 * two-pixel overlap reachable at all; a string of thin letters would pass either way.
 */
TEST(OracoolTextShadow, TheShadowNeverCoversTheLetterBeforeIt)
{
	constexpr uint8_t Background = 77;
	constexpr int W = 120;
	constexpr int H = 24;
	const auto faceAndShadowPixels = [&](devilution::UiFlags extra) {
		OwnedSurface canvas(W, H);
		for (int y = 0; y < H; y++) {
			uint8_t *row = &canvas[Point { 0, y }];
			for (int x = 0; x < W; x++)
				row[x] = Background;
		}
		DrawString(canvas, "WWWWWW", Rectangle { { 0, 0 }, { W, H } },
		    { devilution::UiFlags::ColorWhite | extra });
		int face = 0;
		int black = 0;
		for (int y = 0; y < H; y++) {
			const uint8_t *row = &canvas[Point { 0, y }];
			for (int x = 0; x < W; x++) {
				if (row[x] == 0)
					black++;
				else if (row[x] != Background)
					face++;
			}
		}
		return std::pair<int, int> { face, black };
	};

	const auto plain = faceAndShadowPixels(devilution::UiFlags::None);
	ASSERT_GT(plain.first, 0)
	    << "the plain render marked nothing - fonts are unavailable here, so this test cannot "
	       "distinguish a preserved letter from an absent one";

	const auto shadowed = faceAndShadowPixels(devilution::UiFlags::Shadowed);
	ASSERT_GT(shadowed.second, plain.second) << "nothing was shadowed, so there is nothing to check";
	EXPECT_EQ(shadowed.first, plain.first)
	    << "the shadow blacked out " << (plain.first - shadowed.first)
	    << " pixels of the letters themselves - it is being drawn over text already on the canvas, "
	       "which is the reported smearing";
}

/**
 * @brief Assigning the hover text drops the previous hover's per-line colours.
 *
 * User screenshot (2026-09-03): the cursor_tooltip assert fired while moving an unsocketed ring back
 * to the inventory. Third outing of one bug - the shrine hover in August, gold in August, the
 * held-item lines now. Assigning InfoString replaced the TEXT while InfoStringLineColors kept the
 * colours of the longer block before it, and the tooltip's size check, which exists to catch exactly
 * that, fired.
 *
 * Pinned on the type rather than on any call site, because fixing call sites is what failed twice.
 */
TEST(OracoolAudit, AssigningHoverTextClearsThePreviousLineColours)
{
	SetPanelString(devilution::string_view("Ring of the Heavens"), devilution::UiFlags::ColorWhitegold);
	AddPanelString(devilution::string_view("magic ring"), devilution::UiFlags::ColorBlue);
	AddPanelString(devilution::string_view("Item Level: 30"), devilution::UiFlags::ColorWhite);
	ASSERT_EQ(InfoStringLineColors.size(), 3u) << "test setup: the block did not record three lines";

	// The pattern the crash came from: one line, assigned bare, over a three-line colour list.
	InfoString = devilution::string_view("Requirements not met");

	EXPECT_TRUE(InfoStringLineColors.empty())
	    << "a one-line assignment kept " << InfoStringLineColors.size()
	    << " stale line colours - this is the mismatch the tooltip asserts on";
	EXPECT_TRUE(InfoStringLineTailStart.empty())
	    << "the tail array must move with the colour array or they index differently";

	// And the arrays still work afterwards: an append re-establishes them for the new text.
	AddPanelString(devilution::string_view("Requires Strength: 60"), devilution::UiFlags::ColorRed);
	EXPECT_EQ(InfoStringLineColors.size(), 2u)
	    << "after the assignment the panel holds two lines, so it must hold two colours";

	ClearPanelStrings();
}

/**
 * @brief An item in Levski's grid answers the hover, like an item in any other grid.
 *
 * User, 2026-09-03: "when i moved my socketed ring in levski's grid hovering over it show no pop-up
 * of the item socketed in it [...] Make sure levski's grid works as stash or inv grid."
 *
 * The grid was built as a transmute tray and drew the sprite and nothing else - no panel text, no
 * outline, no socket overlay. This pins the text, which is the half that is testable without a
 * screen: the other two are two lines in the draw loop beside it.
 *
 * The cursor is SWEPT across the window rather than placed on a computed cell. CellRect is
 * file-local, and a test that re-derived the geometry would be asserting its own copy of the layout
 * - it would keep passing if the grid moved and the hover stopped following it.
 */
TEST(OracoolAudit, AnItemInLevskisGridFillsTheHoverPanel)
{
	oracool::ResetLevskiRoarForNewGame(); // a clean grid, whatever an earlier test left
	oracool::ToggleLevskiRoar();
	ASSERT_TRUE(oracool::IsLevskiRoarOpen()) << "test setup: the window did not open";

	devilution::Item ring {};
	ring._itype = ItemType::Ring;
	ring._iCurs = ICURS_RING;
	// Magic and identified so getName() returns _iIName: a NORMAL item is named from its base type
	// table, and the test would then be asserting on a name it did not choose.
	ring._iMagical = ITEM_QUALITY_MAGIC;
	ring._iIdentified = true;
	ring._iCreateInfo = 1;
	CopyUtf8(ring._iName, devilution::string_view("Levski Test Ring"), sizeof(ring._iName));
	CopyUtf8(ring._iIName, devilution::string_view("Levski Test Ring"), sizeof(ring._iIName));
	ASSERT_TRUE(oracool::PlaceItemInLevskiGrid(ring)) << "test setup: the ring did not fit an empty grid";

	const Rectangle window = oracool::GetLevskiRoarRect();
	const Point savedMouse = MousePosition;
	bool found = false;
	for (int y = window.position.y; y < window.position.y + window.size.height && !found; y += 2) {
		for (int x = window.position.x; x < window.position.x + window.size.width && !found; x += 2) {
			ClearPanelStrings();
			MousePosition = { x, y };
			if (!oracool::SetLevskiHoverInfoString())
				continue;
			found = true;
			EXPECT_NE(InfoString.str().find("Levski Test Ring"), devilution::string_view::npos)
			    << "the hover reported something other than the item under the cursor";
			// The invariant DrawCursorTooltip asserts on, checked here where a failure names its
			// cause rather than popping a dialog mid-play.
			size_t lines = 1;
			for (const char c : InfoString.str()) {
				if (c == '\n')
					lines++;
			}
			EXPECT_TRUE(InfoStringLineColors.empty() || InfoStringLineColors.size() == lines)
			    << "the grid built " << lines << " lines but recorded " << InfoStringLineColors.size()
			    << " colours - this is what pops the tooltip assert";
		}
	}
	EXPECT_TRUE(found) << "no point over the open window hovered the item in it";

	MousePosition = savedMouse;
	ClearPanelStrings();
	oracool::ResetLevskiRoarForNewGame();
}

/**
 * @brief The Cold pack loads: thirteen PNG sheets, sliced the way the brief specified them.
 *
 * The 2026-09-03 delivery is the first art this game reads as MISSILE graphics from a PNG - the
 * engine's own path is .cl2 only, and MissileFileData::LoadGFX now tries an import first. This is the
 * seam that fix turns on, so it is the seam worth pinning: the archive really holds the file, the
 * palette quantiser really produces frames, and a directional sheet really comes back as sixteen
 * facings rather than one row repeated.
 *
 * The frame counts are the brief's, and asserting them here is what makes a REGENERATED sheet with a
 * frame added or lost fail at the test rather than in play, where a missile with the wrong frame
 * count animates at the wrong speed and nothing says why.
 */
TEST(OracoolColdPack, EveryDeliveredSheetLoadsAtItsSpecifiedShape)
{
	// The archives are not mounted by default in this binary - only the timedemo does it - and the
	// import reads through the asset system. Without this every sheet reports as missing, and the
	// test would then be measuring an empty search path rather than the art.
	LoadCoreArchives();

	struct Sheet {
		const char *name;
		uint16_t frameWidth;
		int rows;   // 16 for a projectile, 1 for anything drawn the same from every side
		int frames; // the brief's own count, which is the engine's animation length
	};
	// The thirteen, in the brief's order.
	const Sheet sheets[] = {
		{ "ice_bolt", 96, 16, 16 },
		{ "ice_blast", 96, 16, 16 },
		{ "glacial_spike", 128, 16, 16 },
		{ "frost_arrow", 96, 16, 4 },
		{ "frozen_orb", 128, 16, 16 },
		{ "ice_impact", 96, 1, 10 },
		{ "glacial_shatter", 128, 1, 12 },
		{ "freezing_burst", 128, 1, 12 },
		{ "frost_nova", 160, 1, 19 },
		{ "blizzard_shard", 128, 1, 13 },
		{ "ice_ground", 128, 1, 2 },
		{ "ice_armor_shell", 96, 1, 8 },
		{ "ice_armor_break", 96, 1, 10 },
	};

	for (const Sheet &sheet : sheets) {
		std::optional<OwnedClxSpriteListOrSheet> loaded
		    = oracool::LoadPngMissileSheet(sheet.name, sheet.frameWidth, sheet.rows);
		ASSERT_TRUE(loaded.has_value())
		    << sheet.name << " did not load - it is missing from oracool.mpq, or its dimensions are "
		                     "not a whole number of "
		    << sheet.frameWidth << "px columns by " << sheet.rows << " rows";

		if (sheet.rows == 1) {
			EXPECT_FALSE(loaded->isSheet())
			    << sheet.name << " came back as a facing sheet; a non-directional missile needs a list";
			EXPECT_EQ(loaded->list().numSprites(), static_cast<size_t>(sheet.frames))
			    << sheet.name << " has the wrong number of frames";
		} else {
			ASSERT_TRUE(loaded->isSheet())
			    << sheet.name << " came back as a single list - every facing but south would draw the "
			                     "south sprite";
			const ClxSpriteSheet facings = loaded->sheet();
			EXPECT_EQ(facings.numLists(), static_cast<size_t>(sheet.rows))
			    << sheet.name << " has the wrong number of facings";
			EXPECT_EQ(facings[0].numSprites(), static_cast<size_t>(sheet.frames))
			    << sheet.name << " has the wrong number of frames in its first facing";
			EXPECT_EQ(facings[static_cast<size_t>(sheet.rows) - 1].numSprites(), static_cast<size_t>(sheet.frames))
			    << sheet.name << " has the wrong number of frames in its LAST facing - the sheet was "
			                     "sliced against the wrong row height";
		}
	}
}

/**
 * @brief Chill takes every other tick, ages on all of them, and expires.
 *
 * Round 1 of the inert-skill plan (2026-09-03). Cold damage on its own is a fifth colour of number;
 * the slow is what makes cold read as cold, and it is the part with arithmetic in it.
 *
 * Two things are pinned here and both have already been got wrong once in this file's own history:
 * the chill must age on EVERY tick rather than only the ones it takes - otherwise a two-second chill
 * lasts four seconds and every duration in the module quietly means double - and it must eventually
 * stop, because a status effect stored in a side table that never reaches zero is a monster that is
 * slow for the rest of the level.
 */
TEST(OracoolChill, HalvesTheTicksAndThenLetsGo)
{
	Monsters[0] = {};
	Monster &monster = Monsters[0];

	oracool::ClearChills();
	EXPECT_FALSE(oracool::IsMonsterChilled(monster)) << "a cleared table reported a chill";
	EXPECT_FALSE(oracool::ChillTakesThisTick(monster)) << "an unchilled monster lost a tick";

	constexpr int Ticks = 40;
	oracool::ChillMonster(monster, Ticks);
	ASSERT_TRUE(oracool::IsMonsterChilled(monster));

	int taken = 0;
	for (int i = 0; i < Ticks; i++) {
		if (oracool::ChillTakesThisTick(monster))
			taken++;
	}
	EXPECT_EQ(taken, Ticks / 2)
	    << "the ice took " << taken << " of " << Ticks
	    << " ticks - half is the whole mechanic, and anything else is a different slow";
	EXPECT_FALSE(oracool::IsMonsterChilled(monster))
	    << "the chill outlived its duration - it ages on ticks it takes as well as ticks it gives";

	// A second hit EXTENDS rather than stacks: continuous fire keeps a monster slow, but a few
	// seconds of casting must not freeze it for a minute.
	oracool::ChillMonster(monster, 10);
	oracool::ChillMonster(monster, 4);
	int remaining = 0;
	while (oracool::IsMonsterChilled(monster)) {
		oracool::ChillTakesThisTick(monster);
		remaining++;
		ASSERT_LT(remaining, 100) << "the chill never ended";
	}
	EXPECT_EQ(remaining, 10) << "a shorter second chill shortened the first, or the two stacked";

	oracool::ClearChills();
}

/**
 * @brief The spell masks hold 128 ids, and the low 64 mean exactly what they did.
 *
 * Round 2 of the inert-skill plan (2026-09-03) opened by widening the four uint64 spell sets into
 * SpellMask, because Ice Bolt took id 59 and the rest of the cold line wanted eleven more - the first
 * past 64 would have wrapped onto Firebolt's bit. Two things are pinned: an id past 64 gets a bit of
 * its own that no low id shares, and the LOW word is untouched by it, since that word is what the
 * hero file and the level save persist and every existing save must keep its meaning.
 */
TEST(OracoolSpellMask, IdsPast64GetTheirOwnBitsAndLeaveTheSavedWordAlone)
{
	const SpellMask firebolt = GetSpellBitmask(SpellID::Firebolt);
	EXPECT_EQ(firebolt.low, 1ULL) << "id 1 is bit 0 of the low word, as it always was";
	EXPECT_EQ(firebolt.high, 0ULL);

	const SpellMask id64 = GetSpellBitmask(static_cast<SpellID>(64));
	EXPECT_EQ(id64.low, 1ULL << 63) << "id 64 is the last bit of the low word";
	EXPECT_EQ(id64.high, 0ULL);

	const SpellMask id65 = GetSpellBitmask(static_cast<SpellID>(65));
	EXPECT_EQ(id65.low, 0ULL) << "id 65 must not touch the saved word";
	EXPECT_EQ(id65.high, 1ULL) << "id 65 is bit 0 of the high word";

	// The idiom every site uses, across the boundary.
	SpellMask known;
	known |= firebolt;
	known |= id65;
	EXPECT_TRUE((known & firebolt) != 0);
	EXPECT_TRUE((known & id65) != 0);
	EXPECT_TRUE((known & id64) == 0) << "a bit nothing set reads as set";
	known &= ~id65;
	EXPECT_TRUE((known & id65) == 0) << "clearing a high bit did not clear it";
	EXPECT_TRUE((known & firebolt) != 0) << "clearing a high bit cleared a low one";
}

/**
 * @brief Round 2's three contracts, pinned where they were most likely to be broken.
 *
 * ONE: the number on the sheet is the number that lands. Ice Bolt fires through AddFirebolt, whose
 * damage is `GenerateRnd(10) + magic/8 + level + 1`; ColdSpellDamage must describe exactly that
 * range, or the Abilities window reports a number the game does not use - which this project has
 * been bitten by three times.
 *
 * TWO: a freeze takes every tick, a chill every other, and a freeze does not extend a chill - the
 * two do not add.
 *
 * THREE: a tree skill with a mana price pays it. Ice Bolt was free for a day; CheckSpell answered
 * Success for every SpellType::Skill without looking at the mana.
 */
TEST(OracoolCold, TheSheetTheHitAndTheMana)
{
	const bool savedHellfire = gbIsHellfire;
	gbIsHellfire = true;
	Players.resize(1);
	MyPlayer = &Players[0];
	devilution::Player &player = Players[0];
	player = {};
	player._pClass = HeroClass::Sorcerer;
	player._pLevel = 10;
	player._pMagic = 40;

	// ONE. AddFirebolt: GenerateRnd(10) is 0..9, so min is magic/8 + level + 1 and max is min + 9.
	int minDamage;
	int maxDamage;
	oracool::ColdSpellDamage(player, SpellID::IceBolt, 3, minDamage, maxDamage);
	EXPECT_EQ(minDamage, 40 / 8 + 3 + 1) << "the sheet's Ice Bolt minimum is not AddFirebolt's";
	EXPECT_EQ(maxDamage, minDamage + 9) << "the sheet's Ice Bolt maximum is not AddFirebolt's";
	// And the tooltip path reaches the same function.
	int mind;
	int maxd;
	GetDamageAmtAtLevel(SpellID::IceBolt, 3, &mind, &maxd);
	EXPECT_EQ(mind, minDamage);
	EXPECT_EQ(maxd, maxDamage);
	// Every rank helps, on every offensive cold spell.
	for (const SpellID spell : { SpellID::IceBolt, SpellID::IceBlast, SpellID::GlacialSpike, SpellID::FrostNova, SpellID::Blizzard }) {
		int lowMin, lowMax, highMin, highMax;
		oracool::ColdSpellDamage(player, spell, 1, lowMin, lowMax);
		oracool::ColdSpellDamage(player, spell, 5, highMin, highMax);
		EXPECT_GT(highMin, lowMin) << "rank 5 is no better than rank 1 for spell " << static_cast<int>(spell);
		EXPECT_GT(lowMin, 0);
	}
	// The armours do no direct damage and say so.
	oracool::ColdSpellDamage(player, SpellID::FrozenArmor, 3, minDamage, maxDamage);
	EXPECT_EQ(minDamage, -1);

	// TWO.
	Monsters[0] = {};
	Monster &monster = Monsters[0];
	oracool::ClearChills();
	oracool::FreezeMonster(monster, 10);
	int taken = 0;
	for (int i = 0; i < 10; i++)
		if (oracool::ChillTakesThisTick(monster))
			taken++;
	EXPECT_EQ(taken, 10) << "a freeze must take every tick";
	EXPECT_FALSE(oracool::IsMonsterFrozen(monster)) << "the freeze outlived its duration";
	EXPECT_FALSE(oracool::IsMonsterChilled(monster)) << "a freeze left a chill behind that nobody applied";
	oracool::ClearChills();

	// THREE. Asked of GetManaAmount rather than written as 6: a Hellfire Sorcerer pays half, and a
	// test that hard-codes the list price would pass or fail on the class discount rather than on
	// the thing it is testing.
	player._pSkillInvestment[static_cast<size_t>(SpellID::IceBolt)] = 1;
	const int price = GetManaAmount(player, SpellID::IceBolt);
	ASSERT_GT(price, 0) << "test setup: Ice Bolt has no price to pay";
	player._pMana = price - 1;
	// manaonly: skip the cursor check, which is UI state this test does not set up.
	EXPECT_EQ(CheckSpell(player, SpellID::IceBolt, SpellType::Skill, /*manaonly=*/true), SpellCheckResult::Fail_NoMana)
	    << "a tree skill with a mana price was castable on an empty pool - this is what made Ice Bolt free";
	player._pMana = price;
	EXPECT_EQ(CheckSpell(player, SpellID::IceBolt, SpellType::Skill, /*manaonly=*/true), SpellCheckResult::Success);
	// And vanilla's free skills stay free: Item Repair has no price and no pool to pay from.
	player._pMana = 0;
	EXPECT_EQ(CheckSpell(player, SpellID::ItemRepair, SpellType::Skill, /*manaonly=*/true), SpellCheckResult::Success)
	    << "a zero-priced vanilla skill started charging";

	gbIsHellfire = savedHellfire;
}

/**
 * @brief Round 3's bow skills: the mapping is total, the sheet's numbers are the bow's plus the
 * rank, and a bow skill with a price refuses an empty pool.
 *
 * The mapping matters more than it looks: FireArrowSkill converts a RogueArrow back to its SpellID
 * to price it and to level it, and a row that mapped one way but not the other would be free and
 * rank-zero forever, silently.
 */
TEST(OracoolRogueArrows, EveryBowSkillMapsBothWaysAndIsPriced)
{
	const bool savedHellfire = gbIsHellfire;
	gbIsHellfire = true;
	Players.resize(1);
	MyPlayer = &Players[0];
	devilution::Player &player = Players[0];
	player = {};
	player._pClass = HeroClass::Rogue;
	player._pLevel = 10;
	player._pIMinDam = 5;
	player._pIMaxDam = 12;

	const SpellID bowSpells[] = {
		SpellID::MagicArrow, SpellID::FireArrow, SpellID::ColdArrow, SpellID::MultipleShot,
		SpellID::ExplodingArrow, SpellID::IceArrow, SpellID::GuidedArrow, SpellID::Strafe,
		SpellID::ImmolationArrow, SpellID::FreezingArrow
	};
	for (const SpellID spell : bowSpells) {
		const std::optional<oracool::RogueArrow> arrow = oracool::RogueArrowForSpell(spell);
		ASSERT_TRUE(arrow.has_value()) << "spell " << static_cast<int>(spell) << " is not a bow skill";
		EXPECT_EQ(oracool::RogueArrowSpell(*arrow), spell) << "the mapping does not round-trip";
		// Priced: asked through GetManaAmount rather than SpellsData, which this binary does not
		// link. Zero here would mean a free skill - the fault Round 2 found in Ice Bolt.
		EXPECT_GT(GetManaAmount(player, spell), 0) << "a bow skill with no price";
	}
	EXPECT_FALSE(oracool::RogueArrowForSpell(SpellID::Firebolt).has_value());
	EXPECT_FALSE(oracool::RogueArrowForSpell(SpellID::IceBolt).has_value());

	// The sheet: a physical bow skill reports the bow; an elemental one adds its bonus, more with rank.
	int minDamage;
	int maxDamage;
	oracool::RogueArrowDamage(player, SpellID::MultipleShot, 3, minDamage, maxDamage);
	EXPECT_EQ(minDamage, 5);
	EXPECT_EQ(maxDamage, 12);
	int lowMin, lowMax, highMin, highMax;
	oracool::RogueArrowDamage(player, SpellID::FireArrow, 1, lowMin, lowMax);
	oracool::RogueArrowDamage(player, SpellID::FireArrow, 5, highMin, highMax);
	EXPECT_GT(lowMin, 5) << "an elemental arrow adds nothing to the bow";
	EXPECT_GT(highMin, lowMin) << "rank buys nothing";
	// Through the tooltip's own door.
	int mind, maxd;
	GetDamageAmtAtLevel(SpellID::FireArrow, 1, &mind, &maxd);
	EXPECT_EQ(mind, lowMin);

	// The price is asked at the click - through CheckSpell's Skill branch, the same one the cold
	// spells pay through - so a click with an empty pool is refused before the bow is drawn.
	player._pSkillInvestment[static_cast<size_t>(SpellID::ColdArrow)] = 1;
	player._pMana = 0;
	EXPECT_EQ(CheckSpell(player, SpellID::ColdArrow, SpellType::Skill, /*manaonly=*/true), SpellCheckResult::Fail_NoMana);
	player._pMana = GetManaAmount(player, SpellID::ColdArrow);
	EXPECT_EQ(CheckSpell(player, SpellID::ColdArrow, SpellType::Skill, /*manaonly=*/true), SpellCheckResult::Success);

	// The latch holds what it is given and nothing else.
	oracool::ArmArrowSkill(oracool::RogueArrow::Strafe);
	ASSERT_TRUE(oracool::ArmedArrowSkill().has_value());
	EXPECT_EQ(*oracool::ArmedArrowSkill(), oracool::RogueArrow::Strafe);
	oracool::ArmArrowSkill(std::nullopt);
	EXPECT_FALSE(oracool::ArmedArrowSkill().has_value());

	gbIsHellfire = savedHellfire;
}
