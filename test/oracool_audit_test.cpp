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

#include <cstring>
#include <string>

#include "engine/random.hpp"
#include "items.h"
#include "monstdat.h"
#include "monster.h"
#include "multi.h"
#include "oracool/class_skills.h"
#include "oracool/gradual_healing.h"
#include "oracool/lesser_uniques.h"
#include "oracool/paladin_melee.h"
#include "oracool/paladin_skills.h"
#include "oracool/rng_streams.h"
#include "player.h"
#include "qol/stash.h"
#include "spells.h"
#include "stores.h"

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

// The Zeal strike ladder the user specified: nothing below the level-6 gate, two strikes at the
// gate, one more every two levels, capped at five.
TEST(OracoolAudit, ZealStrikeLadder)
{
	Players.resize(1);
	devilution::Player &player = Players[0];

	const struct {
		int level;
		int strikes;
	} ladder[] = { { 1, 0 }, { 5, 0 }, { 6, 2 }, { 7, 2 }, { 8, 3 }, { 10, 4 }, { 12, 5 }, { 14, 5 }, { 50, 5 } };
	for (const auto &step : ladder) {
		player._pLevel = static_cast<int8_t>(step.level);
		EXPECT_EQ(oracool::ZealStrikeCount(player), step.strikes) << "at character level " << step.level;
	}
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
