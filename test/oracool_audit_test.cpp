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

#include "engine/random.hpp"
#include "engine/render/clx_render.hpp"
#include "engine/surface.hpp"
#include "items.h"
#include "monstdat.h"
#include "monster.h"
#include "multi.h"
#include "oracool/paladin_tree.h"
#include "oracool/charms.h"
#include "oracool/class_skills.h"
#include "oracool/crafting.h"
#include "oracool/gradual_healing.h"
#include "oracool/gems.h"
#include "oracool/hero_chunks.h"
#include "oracool/lesser_uniques.h"
#include "oracool/paladin_melee.h"
#include "oracool/paladin_skills.h"
#include "oracool/rng_streams.h"
#include "oracool/runewords.h"
#include "oracool/skill_points.h"
#include "oracool/sprite_scale.h"
#include "oracool/stat_sheet.h"
#include "oracool/telemetry.h"
#include "oracool/xp_counter.h"
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
	std::memset(player._pPaladinAuraInvestment, 0, sizeof(player._pPaladinAuraInvestment));
	return player;
}

} // namespace

TEST(OracoolPaladinTree, EveryPageIsPopulatedAndGridPositionsAreUnique)
{
	oracool::PaladinTreeSkill skills[oracool::PaladinTreeSkillCount];
	size_t total = 0;
	for (size_t p = 0; p < oracool::PaladinTreePageCount; p++) {
		const auto page = static_cast<oracool::PaladinTreePage>(p);
		const size_t count = oracool::BuildPaladinTreePage(page, skills);
		EXPECT_GT(count, 0u);
		total += count;
		// Two skills sharing a (tier, column) would draw on top of each other and only the second
		// would be clickable - the grid's one structural invariant.
		bool taken[6][3] = {};
		for (size_t i = 0; i < count; i++) {
			const oracool::PaladinTreeSkillData &data = oracool::GetPaladinTreeSkillData(skills[i]);
			ASSERT_GE(data.tier, 0);
			ASSERT_LT(data.tier, 6);
			ASSERT_GE(data.column, 0);
			ASSERT_LT(data.column, 3);
			EXPECT_FALSE(taken[data.tier][data.column])
			    << "two skills share tier " << data.tier << " column " << data.column;
			taken[data.tier][data.column] = true;
		}
	}
	EXPECT_EQ(total, oracool::PaladinTreeSkillCount) << "a skill is on no page, or on two";
}

TEST(OracoolPaladinTree, InvestmentRespectsClassLevelPoolAndCap)
{
	devilution::Player &player = FreshPaladin(3);

	player._pClass = HeroClass::Rogue;
	EXPECT_FALSE(oracool::InvestPaladinTreePoint(player, oracool::PaladinTreeSkill::Might))
	    << "a Rogue spent a point in the Paladin tree";

	player._pClass = HeroClass::Warrior;
	player._pLevel = 1;
	EXPECT_FALSE(oracool::InvestPaladinTreePoint(player, oracool::PaladinTreeSkill::Fanaticism))
	    << "a level-30 tier took a point at level 1";

	player._pLevel = 30;
	EXPECT_TRUE(oracool::InvestPaladinTreePoint(player, oracool::PaladinTreeSkill::Might));
	EXPECT_EQ(oracool::PaladinTreeInvestment(player, oracool::PaladinTreeSkill::Might), 1);
	EXPECT_EQ(player._pUnspentSkillPoints, 2);

	player._pUnspentSkillPoints = 0;
	EXPECT_FALSE(oracool::InvestPaladinTreePoint(player, oracool::PaladinTreeSkill::Might))
	    << "spent a point that was not there";
}

// The two stores, and why they exist: a skill with a spell slot must reach GetSpellLevel so every
// ladder that already scales with spell level scales with the tree.
TEST(OracoolPaladinTree, CastableSkillsInvestThroughTheSpellLevelSeam)
{
	devilution::Player &player = FreshPaladin();
	player._pISplLvlAdd = 0;
	const int before = player.GetSpellLevel(SpellID::HolyBolt);

	ASSERT_TRUE(oracool::InvestPaladinTreePoint(player, oracool::PaladinTreeSkill::HolyBolt));
	ASSERT_TRUE(oracool::InvestPaladinTreePoint(player, oracool::PaladinTreeSkill::HolyBolt));
	EXPECT_EQ(oracool::PaladinTreeInvestment(player, oracool::PaladinTreeSkill::HolyBolt), 2);
	EXPECT_EQ(player.GetSpellLevel(SpellID::HolyBolt), before + 2)
	    << "a castable tree skill's points did not reach GetSpellLevel";

	// An aura has no slot, so its points must NOT land in the spell array.
	ASSERT_TRUE(oracool::InvestPaladinTreePoint(player, oracool::PaladinTreeSkill::Might));
	EXPECT_EQ(oracool::PaladinTreeInvestment(player, oracool::PaladinTreeSkill::Might), 1);
	EXPECT_EQ(player._pPaladinAuraInvestment[0], 1) << "Might is the first aura in the array";
}

TEST(OracoolPaladinTree, AuraNeedsAPointBeforeItCanBurn)
{
	devilution::Player &player = FreshPaladin();

	EXPECT_FALSE(oracool::TogglePaladinAura(player, oracool::PaladinTreeSkill::Might))
	    << "an aura with nothing invested lit anyway";
	EXPECT_FALSE(oracool::TogglePaladinAura(player, oracool::PaladinTreeSkill::Zeal))
	    << "a combat skill was lit as an aura";

	ASSERT_TRUE(oracool::InvestPaladinTreePoint(player, oracool::PaladinTreeSkill::Might));
	EXPECT_TRUE(oracool::TogglePaladinAura(player, oracool::PaladinTreeSkill::Might));
	EXPECT_EQ(oracool::GetActivePaladinAura(player), oracool::PaladinTreeSkill::Might);

	ASSERT_TRUE(oracool::InvestPaladinTreePoint(player, oracool::PaladinTreeSkill::Defiance));
	EXPECT_TRUE(oracool::TogglePaladinAura(player, oracool::PaladinTreeSkill::Defiance)) << "activating replaces";
	EXPECT_EQ(oracool::GetActivePaladinAura(player), oracool::PaladinTreeSkill::Defiance);
	EXPECT_TRUE(oracool::TogglePaladinAura(player, oracool::PaladinTreeSkill::Defiance)) << "the second click clears";
	EXPECT_EQ(oracool::GetActivePaladinAura(player), oracool::PaladinTreeSkill::None);
}

TEST(OracoolPaladinTree, AuraEffectsScaleWithPointsAndInertOnesStaySilent)
{
	devilution::Player &player = FreshPaladin();

	// Nothing burning contributes nothing.
	oracool::ItemBonusTotals off;
	oracool::ApplyPaladinAuraToTotals(player, off);
	EXPECT_EQ(off.bonusDamage, 0);

	ASSERT_TRUE(oracool::InvestPaladinTreePoint(player, oracool::PaladinTreeSkill::Might));
	ASSERT_TRUE(oracool::TogglePaladinAura(player, oracool::PaladinTreeSkill::Might));
	oracool::ItemBonusTotals onePoint;
	oracool::ApplyPaladinAuraToTotals(player, onePoint);
	EXPECT_GT(onePoint.bonusDamage, 0);

	ASSERT_TRUE(oracool::InvestPaladinTreePoint(player, oracool::PaladinTreeSkill::Might));
	oracool::ItemBonusTotals twoPoints;
	oracool::ApplyPaladinAuraToTotals(player, twoPoints);
	EXPECT_GT(twoPoints.bonusDamage, onePoint.bonusDamage) << "the second point bought nothing";

	// Resist Cold is the documented remap onto magic resistance - this engine has no cold.
	devilution::Player &cold = FreshPaladin();
	ASSERT_TRUE(oracool::InvestPaladinTreePoint(cold, oracool::PaladinTreeSkill::ResistCold));
	ASSERT_TRUE(oracool::TogglePaladinAura(cold, oracool::PaladinTreeSkill::ResistCold));
	oracool::ItemBonusTotals coldTotals;
	oracool::ApplyPaladinAuraToTotals(cold, coldTotals);
	EXPECT_GT(coldTotals.magicResist, 0);

	// The auras whose mechanics this engine has no channel for must contribute NOTHING, so a
	// player cannot be told a point bought something it did not.
	for (const oracool::PaladinTreeSkill inert : { oracool::PaladinTreeSkill::HolyFreeze,
	         oracool::PaladinTreeSkill::Sanctuary, oracool::PaladinTreeSkill::Conviction,
	         oracool::PaladinTreeSkill::Cleansing, oracool::PaladinTreeSkill::Redemption }) {
		devilution::Player &p = FreshPaladin();
		ASSERT_TRUE(oracool::InvestPaladinTreePoint(p, inert));
		ASSERT_TRUE(oracool::TogglePaladinAura(p, inert));
		oracool::ItemBonusTotals inertTotals;
		oracool::ItemBonusTotals empty;
		oracool::ApplyPaladinAuraToTotals(p, inertTotals);
		EXPECT_EQ(std::memcmp(&inertTotals, &empty, sizeof(empty)), 0)
		    << _(oracool::GetPaladinTreeSkillData(inert).name) << " leaked an effect it does not have";
	}
}

TEST(OracoolPaladinTree, VigorRunsAndOnlyWhenPaidFor)
{
	devilution::Player &player = FreshPaladin();
	EXPECT_FALSE(oracool::IsPaladinVigorActive(player));

	ASSERT_TRUE(oracool::InvestPaladinTreePoint(player, oracool::PaladinTreeSkill::Vigor));
	ASSERT_TRUE(oracool::TogglePaladinAura(player, oracool::PaladinTreeSkill::Vigor));
	EXPECT_TRUE(oracool::IsPaladinVigorActive(player));

	ASSERT_TRUE(oracool::TogglePaladinAura(player, oracool::PaladinTreeSkill::Vigor));
	EXPECT_FALSE(oracool::IsPaladinVigorActive(player)) << "Vigor kept running after it was put out";
}

TEST(OracoolPaladinTree, AuraStateRoundTripsThroughTheChunkTail)
{
	devilution::Player &writer = FreshPaladin();
	ASSERT_TRUE(oracool::InvestPaladinTreePoint(writer, oracool::PaladinTreeSkill::HolyFire));
	ASSERT_TRUE(oracool::InvestPaladinTreePoint(writer, oracool::PaladinTreeSkill::HolyFire));
	ASSERT_TRUE(oracool::TogglePaladinAura(writer, oracool::PaladinTreeSkill::HolyFire));
	const std::vector<uint8_t> tail = oracool::BuildHeroChunkTail(writer);

	Players.resize(2);
	devilution::Player &reader = Players[1];
	reader._pClass = HeroClass::Warrior;
	reader._pLevel = 30;
	reader._pOracoolActiveAura = 0xFF;
	std::memset(reader._pPaladinAuraInvestment, 0, sizeof(reader._pPaladinAuraInvestment));
	oracool::ApplyHeroChunks(reader, tail.data(), tail.size());

	EXPECT_EQ(oracool::GetActivePaladinAura(reader), oracool::PaladinTreeSkill::HolyFire);
	EXPECT_EQ(oracool::PaladinTreeInvestment(reader, oracool::PaladinTreeSkill::HolyFire), 2);
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
