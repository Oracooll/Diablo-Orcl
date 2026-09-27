#include "player_test.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <limits>
#include <string>

#include <gtest/gtest.h>

#include "options.h"
#include "oracool/combat_odds.h"
#include "oracool/furious_charge.h"
#include "oracool/gradual_healing.h"
#include "oracool/paladin_melee.h"
#include "oracool/paladin_skills.h"
#include "pack.h"
#include "playerdat.hpp"
#include "storm/storm_net.hpp"

using namespace devilution;

namespace devilution {
extern bool TestPlayerDoGotHit(Player &player);
extern bool TestShouldDropGoldOnDeath(Player &player);
}

int RunBlockTest(int frames, ItemSpecialEffect flags)
{
	devilution::Player &player = Players[0];

	player._pHFrames = frames;
	player._pIFlags = flags;
	StartPlrHit(player, 5, false);

	int i = 1;
	for (; i < 100; i++) {
		TestPlayerDoGotHit(player);
		if (player._pmode != PM_GOTHIT)
			break;
		player.AnimInfo.currentFrame++;
	}

	return i;
}

constexpr ItemSpecialEffect Normal = ItemSpecialEffect::None;
constexpr ItemSpecialEffect Balance = ItemSpecialEffect::FastHitRecovery;
constexpr ItemSpecialEffect Stability = ItemSpecialEffect::FasterHitRecovery;
constexpr ItemSpecialEffect Harmony = ItemSpecialEffect::FastestHitRecovery;
constexpr ItemSpecialEffect BalanceStability = Balance | Stability;
constexpr ItemSpecialEffect BalanceHarmony = Balance | Harmony;
constexpr ItemSpecialEffect StabilityHarmony = Stability | Harmony;

constexpr int Warrior = 6;
constexpr int Rogue = 7;
constexpr int Sorcerer = 8;

struct BlockTestCase {
	int expectedRecoveryFrame;
	int maxRecoveryFrame;
	ItemSpecialEffect itemFlags;
};

BlockTestCase BlockData[] = {
	{ 6, Warrior, Normal },
	{ 7, Rogue, Normal },
	{ 8, Sorcerer, Normal },

	{ 5, Warrior, Balance },
	{ 6, Rogue, Balance },
	{ 7, Sorcerer, Balance },

	{ 4, Warrior, Stability },
	{ 5, Rogue, Stability },
	{ 6, Sorcerer, Stability },

	{ 3, Warrior, Harmony },
	{ 4, Rogue, Harmony },
	{ 5, Sorcerer, Harmony },

	{ 4, Warrior, BalanceStability },
	{ 5, Rogue, BalanceStability },
	{ 6, Sorcerer, BalanceStability },

	{ 3, Warrior, BalanceHarmony },
	{ 4, Rogue, BalanceHarmony },
	{ 5, Sorcerer, BalanceHarmony },

	{ 3, Warrior, StabilityHarmony },
	{ 4, Rogue, StabilityHarmony },
	{ 5, Sorcerer, StabilityHarmony },
};

TEST(Player, PM_DoGotHit)
{
	Players.resize(1);
	// A FRESH player: resize keeps an existing element, and the recovery frame depends on state
	// other tests leave behind - a shuffled run of this binary saw frame 1 where 3-8 was expected
	// (external audit, 2026-09-06: QA-03).
	Players[0] = devilution::Player {};
	MyPlayer = &Players[0];
	for (size_t i = 0; i < sizeof(BlockData) / sizeof(*BlockData); i++) {
		EXPECT_EQ(BlockData[i].expectedRecoveryFrame, RunBlockTest(BlockData[i].maxRecoveryFrame, BlockData[i].itemFlags));
	}
}

static void AssertPlayer(devilution::Player &player)
{
	ASSERT_EQ(CountU8(player._pSplLvl, 64), 0);
	// Naked Heroes, on by default since 2026-08-19: a new character owns nothing at all - no bow in
	// the hand, no gold stack in the grid, no two healing potions in the belt. The counts here were
	// 1/1/1/2 while starting gear was still handed out.
	ASSERT_EQ(Count8(player.InvGrid, InventoryGridCells), 0);
	ASSERT_EQ(CountItems(player.InvBody, NUM_INVLOC), 0);
	ASSERT_EQ(CountItems(player.InvList, InventoryGridCells), 0);
	ASSERT_EQ(CountItems(player.SpdList, MaxBeltItems), 0);
	ASSERT_EQ(CountItems(&player.HoldItem, 1), 0);

	ASSERT_EQ(player.position.tile.x, 0);
	ASSERT_EQ(player.position.tile.y, 0);
	ASSERT_EQ(player.position.future.x, 0);
	ASSERT_EQ(player.position.future.y, 0);
	ASSERT_EQ(player.plrlevel, 0);
	ASSERT_EQ(player.destAction, 0);
	ASSERT_STREQ(player._pName, "");
	ASSERT_EQ(player._pClass, HeroClass::Rogue);
	ASSERT_EQ(player._pBaseStr, 20);
	ASSERT_EQ(player._pStrength, 20);
	ASSERT_EQ(player._pBaseMag, 15);
	ASSERT_EQ(player._pMagic, 15);
	ASSERT_EQ(player._pBaseDex, 30);
	ASSERT_EQ(player._pDexterity, 30);
	ASSERT_EQ(player._pBaseVit, 20);
	ASSERT_EQ(player._pVitality, 20);
	ASSERT_EQ(player._pLevel, 1);
	ASSERT_EQ(player._pStatPts, 0);
	ASSERT_EQ(player._pExperience, 0);
	// Zero, not the vanilla 100 - Naked Heroes covers the purse as well as the pack (2026-08-19).
	ASSERT_EQ(player._pGold, 0);
	ASSERT_EQ(player._pMaxHPBase, 2880);
	ASSERT_EQ(player._pHPBase, 2880);
	ASSERT_EQ(player._pBaseToBlk, 20);
	ASSERT_EQ(player._pMaxManaBase, 1440);
	ASSERT_EQ(player._pManaBase, 1440);
	ASSERT_EQ(player._pMemSpells, 0);
	ASSERT_EQ(player._pNumInv, 0); // no gold stack either - see the Naked Heroes note above
	ASSERT_EQ(player.wReflections, 0);
	ASSERT_EQ(player.pTownWarps, 0);
	ASSERT_EQ(player.pDungMsgs, 0);
	ASSERT_EQ(player.pDungMsgs2, 0);
	ASSERT_EQ(player.pLvlLoad, 0);
	ASSERT_EQ(player.pDiabloKillLevel, 0);
	ASSERT_EQ(player.pManaShield, 0);
	ASSERT_EQ(player.pDamAcFlags, ItemSpecialEffectHf::None);

	ASSERT_EQ(player._pmode, 0);
	ASSERT_EQ(Count8(player.walkpath, MaxPathLength), 0);
	ASSERT_EQ(player.queuedSpell.spellId, SpellID::Null);
	ASSERT_EQ(player.queuedSpell.spellType, SpellType::Skill);
	ASSERT_EQ(player.queuedSpell.spellFrom, 0);
	ASSERT_EQ(player.inventorySpell, SpellID::Null);
	// Nothing readied at creation (2026-08-19): the six vanilla class skills are retired, so the
	// Rogue no longer starts with Trap Disarm on her right button - or in her spell masks.
	ASSERT_EQ(player._pRSpell, SpellID::Invalid);
	ASSERT_EQ(player._pRSplType, SpellType::Invalid);
	ASSERT_EQ(player._pSBkSpell, SpellID::Null);
	// EMPTY (2026-08-19). The six vanilla class skills - Identify, Rage, Item Repair, Staff Recharge,
	// Trap Disarm, Search - were granted to every class from 2026-08-15 until the user asked for them
	// retired outright. Nothing else is innate at level 1: the Paladin skills are level-gated, and the
	// class trees are bought with points.
	ASSERT_EQ(player._pAblSpells, 0ULL);
	ASSERT_EQ(player._pScrlSpells, 0);
	ASSERT_EQ(player._pSpellFlags, SpellFlag::None);
	ASSERT_EQ(player._pBlockFlag, 0);
	ASSERT_EQ(player._pLightRad, 10);
	ASSERT_EQ(player._pDamageMod, 0);
	ASSERT_EQ(player._pHitPoints, 2880);
	ASSERT_EQ(player._pMaxHP, 2880);
	ASSERT_EQ(player._pMana, 1440);
	ASSERT_EQ(player._pMaxMana, 1440);
	ASSERT_EQ(player._pNextExper, 500); // Diablo II's table since 2026-09-20: 500 to level 2
	ASSERT_EQ(player._pMagResist, 0);
	ASSERT_EQ(player._pFireResist, 0);
	ASSERT_EQ(player._pLghtResist, 0);
	ASSERT_EQ(CountBool(player._pLvlVisited, NUMLEVELS), 0);
	ASSERT_EQ(CountBool(player._pSLvlVisited, NUMLEVELS), 0);
	ASSERT_EQ(player._pIMinDam, 1);
	ASSERT_EQ(player._pIMaxDam, 1);
	ASSERT_EQ(player._pIAC, 0);
	ASSERT_EQ(player._pIBonusDam, 0);
	ASSERT_EQ(player._pIBonusToHit, 0);
	ASSERT_EQ(player._pIBonusAC, 0);
	ASSERT_EQ(player._pIBonusDamMod, 0);
	ASSERT_EQ(player._pISpells, 0);
	ASSERT_EQ(player._pIFlags, ItemSpecialEffect::None);
	ASSERT_EQ(player._pIGetHit, 0);
	ASSERT_EQ(player._pISplLvlAdd, 0);
	ASSERT_EQ(player._pIEnAc, 0);
	ASSERT_EQ(player._pIFMinDam, 0);
	ASSERT_EQ(player._pIFMaxDam, 0);
	ASSERT_EQ(player._pILMinDam, 0);
	ASSERT_EQ(player._pILMaxDam, 0);
}

TEST(Player, CreatePlayer)
{
	Players.resize(1);
	CreatePlayer(Players[0], HeroClass::Rogue);
	AssertPlayer(Players[0]);
}

TEST(Player, ResetPlayerStats_OnlyRemovesManuallySpentPoints)
{
	SNetInitializeProvider(SELCONN_LOOPBACK, nullptr);
	Players.resize(1);
	CreatePlayer(Players[0], HeroClass::Rogue);
	devilution::Player &player = Players[0];
	MyPlayer = &player;
	gbIsMultiplayer = false;
	sgOptions.Oracool.resetStatsButton.SetValue(true);

	const int baseStrAtCreation = player._pBaseStr;

	// A permanent quest/shrine bonus reaches _pBaseStr through ModifyPlrStr directly and is never
	// tracked - it must survive a reset.
	ModifyPlrStr(player, 3);

	// Manually spending points via the "+" button is the only path that increments the tracked
	// counter (normally done by msg.cpp's OnAddStrength); simulate that here.
	player._pStatPtsSpentStr = 5;
	ModifyPlrStr(player, 5);
	player._pStatPts = 0;

	ResetPlayerStats(player);

	EXPECT_EQ(player._pBaseStr, baseStrAtCreation + 3);
	EXPECT_EQ(player._pStatPts, 5);
	EXPECT_EQ(player._pStatPtsSpentStr, 0);
}

// The grouped sheet's left-pointing triangle (2026-09-26): one stat, a point at a time, never below what the player
// spent on it - a quest or shrine gain stays, as it does through RESET.
TEST(Player, RefundStatPoints_TakesBackOnlySpentPointsOfOneStat)
{
	SNetInitializeProvider(SELCONN_LOOPBACK, nullptr);
	Players.resize(1);
	CreatePlayer(Players[0], HeroClass::Rogue);
	devilution::Player &player = Players[0];
	MyPlayer = &player;
	gbIsMultiplayer = false;

	const int baseStrAtCreation = player._pBaseStr;
	const int baseDexAtCreation = player._pBaseDex;
	ModifyPlrStr(player, 3); // a shrine's gain - untracked, never refunded
	player._pStatPtsSpentStr = 4;
	ModifyPlrStr(player, 4);
	player._pStatPtsSpentDex = 2;
	ModifyPlrDex(player, 2);
	player._pStatPts = 0;

	EXPECT_EQ(RefundStatPoints(player, CharacterAttribute::Strength, 1), 1);
	EXPECT_EQ(player._pBaseStr, baseStrAtCreation + 6);
	EXPECT_EQ(player._pStatPtsSpentStr, 3);
	EXPECT_EQ(player._pStatPts, 1);

	EXPECT_EQ(RefundStatPoints(player, CharacterAttribute::Strength, 255), 3) << "shift-click: all of that stat's spent points";
	EXPECT_EQ(player._pBaseStr, baseStrAtCreation + 3) << "the shrine's 3 stay";
	EXPECT_EQ(player._pStatPtsSpentStr, 0);
	EXPECT_EQ(player._pStatPts, 4);
	EXPECT_EQ(RefundStatPoints(player, CharacterAttribute::Strength, 1), 0) << "nothing spent, nothing back";
	EXPECT_EQ(player._pBaseDex, baseDexAtCreation + 2) << "the other stats are untouched";

	gbIsMultiplayer = true;
	EXPECT_EQ(RefundStatPoints(player, CharacterAttribute::Dexterity, 1), 0) << "single player only";
	gbIsMultiplayer = false;
}

// The grouped sheet's + presses with no points left, and a plain click spent 1 regardless: the pool went negative
// (user, 2026-09-27 dev note: "stat points just increase negativly if i do it").
// Taking Vitality back takes its life with it, current life included: a hurt hero shift-clicking the - died from the
// click (audit, 2026-09-27). The refund stops one point of life short.
TEST(Player, RefundStatPoints_VitalityNeverTakesTheLastPointOfLife)
{
	SNetInitializeProvider(SELCONN_LOOPBACK, nullptr);
	Players.resize(1);
	CreatePlayer(Players[0], HeroClass::Warrior);
	devilution::Player &player = Players[0];
	MyPlayer = &player;
	gbIsMultiplayer = false;

	player._pStatPtsSpentVit = 20;
	ModifyPlrVit(player, 20);
	player._pStatPts = 0;
	const int lifePerPoint = PlayersData[static_cast<size_t>(player._pClass)].chrLife;
	ASSERT_GT(lifePerPoint, 0);
	// Hurt: three points' worth of life and a sliver more. CalcPlrInv rebuilds current life from _pHPBase.
	const int hurt = 3 * lifePerPoint + (1 << 6) / 2;
	player._pHPBase -= player._pHitPoints - hurt;
	CalcPlrInv(player, true);
	ASSERT_EQ(player._pHitPoints, hurt);

	const int refunded = RefundStatPoints(player, CharacterAttribute::Vitality, 10);
	EXPECT_LT(refunded, 10) << "the whole shift-click would have killed";
	EXPECT_GE(player._pHitPoints, 1 << 6) << "at least one point of life is left";
	EXPECT_EQ(player._pStatPts, refunded);
	EXPECT_EQ(player._pStatPtsSpentVit, 20 - refunded) << "what could not come back stays spent, for later";
}

// RESET used to hand back every spent point although a curse can have taken the base below what was spent, which made
// points (audit, 2026-09-27). It goes through RefundStatPoints now, stat by stat.
TEST(Player, ResetPlayerStats_NeverReturnsMoreThanTheBaseHolds)
{
	SNetInitializeProvider(SELCONN_LOOPBACK, nullptr);
	Players.resize(1);
	CreatePlayer(Players[0], HeroClass::Rogue);
	devilution::Player &player = Players[0];
	MyPlayer = &player;
	gbIsMultiplayer = false;
	sgOptions.Oracool.resetStatsButton.SetValue(true);

	player._pStatPtsSpentMag = 30;
	ModifyPlrMag(player, 30);
	player._pStatPts = 0;
	ModifyPlrMag(player, -(player._pBaseMag - 10)); // a curse: the base falls to 10, below the 30 spent
	ASSERT_EQ(player._pBaseMag, 10);

	ResetPlayerStats(player);

	EXPECT_EQ(player._pStatPts, 10) << "only what the base still held";
	EXPECT_EQ(player._pBaseMag, 0);
}

// The Armor class odds bar is the chance to be HIT, not to be reached: a blow that lands can still be blocked (user,
// 2026-09-27: "fix all four" - it read the landing chance alone and overstated how often a shield user is hit).
TEST(Player, ChanceToBeHit_CountsTheShieldsBlock)
{
	SNetInitializeProvider(SELCONN_LOOPBACK, nullptr);
	Players.resize(1);
	CreatePlayer(Players[0], HeroClass::Warrior);
	devilution::Player &player = Players[0];
	MyPlayer = &player;
	gbIsMultiplayer = false;
	oracool::ClearCombatOdds();
	oracool::NoteAttacker("Fallen One", 200, 1, false, false, 0); // lands every time

	int chance = 0;
	std::string name;
	player._pBlockFlag = false;
	ASSERT_TRUE(oracool::ChanceToBeHit(player, chance, name));
	EXPECT_EQ(chance, 100) << "no shield: every landed blow hits";

	player._pBlockFlag = true;
	ASSERT_GT(player.GetBlockChance(), 2) << "a Warrior blocks";
	ASSERT_TRUE(oracool::ChanceToBeHit(player, chance, name));
	const int blocks = std::clamp(player.GetBlockChance() - 2, 0, 100);
	EXPECT_EQ(chance, 100 - blocks) << "the shield's block comes off";
	oracool::ClearCombatOdds();
}

TEST(Player, StatPointsToSpend_NeverMoreThanUnspentNorPastTheCap)
{
	Players.resize(1);
	CreatePlayer(Players[0], HeroClass::Warrior);
	devilution::Player &player = Players[0];

	player._pStatPts = 0;
	EXPECT_EQ(StatPointsToSpend(player, CharacterAttribute::Strength, 1), 0) << "a plain click with nothing to spend";
	EXPECT_EQ(StatPointsToSpend(player, CharacterAttribute::Strength, 10), 0);

	player._pStatPts = 3;
	EXPECT_EQ(StatPointsToSpend(player, CharacterAttribute::Strength, 1), 1);
	EXPECT_EQ(StatPointsToSpend(player, CharacterAttribute::Strength, 5), 3) << "ctrl with only 3 left";

	player._pStatPts = 20;
	player._pBaseStr = 250;
	EXPECT_EQ(StatPointsToSpend(player, CharacterAttribute::Strength, 10), 5) << "stops at the base cap of 255";
	player._pBaseStr = 255;
	EXPECT_EQ(StatPointsToSpend(player, CharacterAttribute::Strength, 1), 0);

	player._pStatPts = -4; // a hero the bug already took below zero
	EXPECT_EQ(StatPointsToSpend(player, CharacterAttribute::Magic, 1), 0);
}

// A New Game cut every base stat back to the class row's vanilla maximum (audit, 2026-09-27), while every other path
// allows 255 - a Paladin's Vitality of 130 came back as 100, the points gone.
TEST(Player, UnPackPlayer_KeepsBaseStatsPastTheClassRowsMaximum)
{
	SNetInitializeProvider(SELCONN_LOOPBACK, nullptr);
	Players.resize(1);
	CreatePlayer(Players[0], HeroClass::Warrior);
	devilution::Player &player = Players[0];
	MyPlayer = &player;
	gbIsMultiplayer = false;
	ASSERT_LT(player.GetMaximumAttributeValue(CharacterAttribute::Vitality), 150);
	ModifyPlrVit(player, 150 - player._pBaseVit);
	ASSERT_EQ(player._pBaseVit, 150);

	PlayerPack packed;
	PackPlayer(packed, player);
	UnPackPlayer(packed, player);

	EXPECT_EQ(player._pBaseVit, 150);
}

TEST(Player, ResetPlayerStats_SurvivesPlayerPackRoundTrip)
{
	// Bug report: starting a New Game with an existing hero goes through PackPlayer/UnPackPlayer
	// (pack.cpp) rather than the full save's LoadPlayer (loadsave.cpp) - only the latter used to
	// carry the manually-spent-points counters, so every "New Game" with an existing hero reset
	// them to zero while leaving the actual attribute values (which PlayerPack does carry) intact,
	// making Reset Stats refund far fewer points than the player had actually spent.
	SNetInitializeProvider(SELCONN_LOOPBACK, nullptr);
	Players.resize(1);
	CreatePlayer(Players[0], HeroClass::Rogue);
	devilution::Player &player = Players[0];
	MyPlayer = &player;
	gbIsMultiplayer = false;
	sgOptions.Oracool.resetStatsButton.SetValue(true);

	player._pStatPtsSpentStr = 40;
	player._pStatPtsSpentMag = 30;
	player._pStatPtsSpentDex = 20;
	player._pStatPtsSpentVit = 10;
	ModifyPlrStr(player, 40);
	ModifyPlrMag(player, 30);
	ModifyPlrDex(player, 20);
	ModifyPlrVit(player, 10);
	player._pStatPts = 0;

	PlayerPack packed;
	PackPlayer(packed, player);
	UnPackPlayer(packed, player);

	ResetPlayerStats(player);

	EXPECT_EQ(player._pStatPts, 100);
	EXPECT_EQ(player._pStatPtsSpentStr, 0);
	EXPECT_EQ(player._pStatPtsSpentMag, 0);
	EXPECT_EQ(player._pStatPtsSpentDex, 0);
	EXPECT_EQ(player._pStatPtsSpentVit, 0);
}

// Oracool (2026-08-11): Furious Charge stopped being a settings toggle - it is destined to be an
// acquirable skill unlocked through progression once the Skills system lands, and until then the
// Paladin's free slot stays vanilla Item Repair. These tests therefore assert the gate is closed
// and that nothing substitutes; the dash/cooldown mechanics below remain covered, since they are
// still intact and will be reused verbatim once the skill can be learned.
TEST(Player, FuriousCharge_Disabled_UntilSkillsSystemExists)
{
	using namespace devilution::oracool;

	// The gate reads MyPlayer's level: a fresh level-0 player, whatever the last test left
	// (external audit, 2026-09-06: QA-03).
	Players.resize(1);
	Players[0] = devilution::Player {};
	MyPlayer = &Players[0];
	ResetFuriousChargeForNewGame();
	gbIsMultiplayer = false;
	EXPECT_FALSE(IsFuriousChargeEnabled()) << "not yet acquirable - the gate must stay closed";
	EXPECT_FALSE(IsFuriousChargeSpell(SpellID::ItemRepair)) << "the Paladin's free slot must remain vanilla Item Repair";
	EXPECT_FALSE(IsFuriousChargeSpell(SpellID::Firebolt)) << "only the ItemRepair slot was ever a candidate";
	EXPECT_FALSE(IsFuriousChargeSpell(SpellID::StaffRecharge)) << "the Sorcerer's innate skill must never be affected";

	gbIsMultiplayer = true;
	EXPECT_FALSE(IsFuriousChargeSpell(SpellID::ItemRepair)) << "multiplayer keeps vanilla Item Repair too";
	gbIsMultiplayer = false;
}

TEST(Player, FuriousCharge_GetSpellDisplayName_ShowsVanillaNameWhileDisabled)
{
	using namespace devilution::oracool;

	gbIsMultiplayer = false;
	EXPECT_NE(GetSpellDisplayName(SpellID::ItemRepair), "Furious Charge") << "disabled - vanilla Item Repair's own name must show";
	EXPECT_NE(GetSpellDisplayName(SpellID::Firebolt), "Furious Charge") << "an unrelated spell's real name must be untouched";
}

TEST(Player, FuriousCharge_DashAndCooldownStateTransitions)
{
	using namespace devilution::oracool;

	StartFuriousChargeDash();
	EXPECT_TRUE(IsFuriousChargeDashing());
	StopFuriousChargeDash();
	EXPECT_FALSE(IsFuriousChargeDashing());

	StartFuriousChargeCooldown();
	EXPECT_TRUE(IsFuriousChargeOnCooldown());
	EXPECT_LT(GetFuriousChargeCooldownProgress(), 1.0F) << "just-started cooldown must not already read as ready";
}

TEST(Player, GradualHealing_IsEnabled_GatedByOptionAndMultiplayer)
{
	using namespace devilution::oracool;

	gbIsMultiplayer = false;
	sgOptions.Oracool.gradualHealing.SetValue(false);
	EXPECT_FALSE(IsGradualHealingEnabled()) << "off by default-off setting - potions must stay instant";

	sgOptions.Oracool.gradualHealing.SetValue(true);
	EXPECT_TRUE(IsGradualHealingEnabled()) << "option on, single-player - this is the only case that drips";

	gbIsMultiplayer = true;
	EXPECT_FALSE(IsGradualHealingEnabled()) << "multiplayer always keeps vanilla instant potions regardless of the option";

	gbIsMultiplayer = false;
	sgOptions.Oracool.gradualHealing.SetValue(false);
}

// Oracool (2026-08-15): Zeal is ACTIVE ONLY - it carries a swing to neighbours when it is the skill
// readied on the button that swung, and not otherwise. This replaces a test that asserted the older
// "not acquirable at all" state; that one had stopped testing anything real, because it passed on a
// level-0 fixture whatever the gate did.
TEST(Player, Zeal_GatedByClass_Level_Mana_AndSinglePlayer)
{
	using namespace devilution::oracool;

	Players.resize(1);
	devilution::Player &paladin = Players[0];
	paladin._pClass = HeroClass::Warrior; // "Paladin" is a display name only - see paladin_skills.h
	paladin._pLevel = GetPaladinSkillData(PaladinSkill::Zeal).minLevel;
	paladin._pMana = 64 * GetPaladinSkillData(PaladinSkill::Zeal).manaCost;
	gbIsMultiplayer = false;

	EXPECT_TRUE(CanUsePaladinSkill(paladin, PaladinSkill::Zeal)) << "earned, funded and single player";

	paladin._pLevel--;
	EXPECT_FALSE(CanUsePaladinSkill(paladin, PaladinSkill::Zeal)) << "one level short of the gate";
	paladin._pLevel++;

	paladin._pMana--;
	EXPECT_FALSE(CanUsePaladinSkill(paladin, PaladinSkill::Zeal)) << "one point short of the price";
	paladin._pMana++;

	paladin._pClass = HeroClass::Sorcerer;
	EXPECT_FALSE(CanUsePaladinSkill(paladin, PaladinSkill::Zeal)) << "never for other classes";
	paladin._pClass = HeroClass::Warrior;

	gbIsMultiplayer = true;
	EXPECT_FALSE(CanUsePaladinSkill(paladin, PaladinSkill::Zeal)) << "multiplayer keeps vanilla melee";
	gbIsMultiplayer = false;
}

// The Zeal ladder after Phase 2.1 (supersedes the 2026-08-15 character-level table): the level-6
// gate buys the 2-strike burst, and INVESTED skill points buy the rest - one strike per two points,
// capped at five. Character level beyond the gate no longer adds strikes on its own.
TEST(Player, Zeal_StrikeCountLadder)
{
	using namespace devilution::oracool;

	Players.resize(1);
	devilution::Player &paladin = Players[0];
	paladin._pClass = HeroClass::Warrior;
	std::memset(paladin._pSkillInvestment, 0, sizeof(paladin._pSkillInvestment));
	const auto zeal = static_cast<size_t>(GetPaladinSkillData(PaladinSkill::Zeal).spellId);

	paladin._pISplLvlAdd = 0;
	std::memset(paladin._pSplLvl, 0, sizeof(paladin._pSplLvl));

	paladin._pLevel = 5;
	EXPECT_EQ(ZealStrikeCount(paladin), 0) << "below the gate: no Zeal at all";
	paladin._pLevel = 6;
	EXPECT_EQ(ZealStrikeCount(paladin), 0) << "past the gate, but no point spent yet";
	paladin._pLevel = 50;
	EXPECT_EQ(ZealStrikeCount(paladin), 0) << "level alone must not add strikes";

	// Skill levels 1, 3 and 5 each add a strike, on a base of one (user, 2026-08-30). Because Zeal
	// unlocks at character level 6 and rank R wants character level 6 + R - 1, those three rungs are
	// character levels 6, 8 and 10 - which is how the same ladder was first described.
	const struct {
		int skillLevel;
		int expected;
	} ladder[] = { { 0, 0 }, { 1, 2 }, { 2, 2 }, { 3, 3 }, { 4, 3 }, { 5, 4 }, { 6, 4 }, { 20, 4 } };
	for (const auto &step : ladder) {
		paladin._pSkillInvestment[zeal] = static_cast<uint8_t>(step.skillLevel);
		EXPECT_EQ(ZealStrikeCount(paladin), step.expected)
		    << "at Zeal skill level " << step.skillLevel;
	}
	paladin._pSkillInvestment[zeal] = 0;
}

// The half of "active only" that CanUsePaladinSkill cannot express: being able to afford Zeal is not
// the same as having thrown this swing with it. The latch is what carries that from the click to the
// hit frame, several frames later, and a swing that armed nothing must find nothing armed.
TEST(Player, MeleeSkillLatch_RemembersWhatTheSwingWasThrownWith)
{
	using namespace devilution::oracool;

	ArmMeleeSkill(std::nullopt);
	EXPECT_FALSE(ArmedMeleeSkill().has_value()) << "a plain swing arms nothing";

	ArmMeleeSkill(PaladinSkill::Zeal);
	ASSERT_TRUE(ArmedMeleeSkill().has_value());
	EXPECT_EQ(*ArmedMeleeSkill(), PaladinSkill::Zeal);

	// Overwritten rather than accumulated: the latch describes the CURRENT swing, so switching
	// buttons mid-fight must not leave the previous skill applying to the next blow.
	ArmMeleeSkill(PaladinSkill::ShieldBash);
	ASSERT_TRUE(ArmedMeleeSkill().has_value());
	EXPECT_EQ(*ArmedMeleeSkill(), PaladinSkill::ShieldBash);

	ArmMeleeSkill(std::nullopt);
	EXPECT_FALSE(ArmedMeleeSkill().has_value()) << "and a plain swing clears it again";
}

TEST(Player, GradualHealing_QueueAndDrain_DeliversFullAmountGraduallyNotInstantly)
{
	using namespace devilution::oracool;

	Players.resize(1);
	devilution::Player &player = Players[0];
	player._pMaxHP = 100 << 6;
	player._pMaxHPBase = 100 << 6;
	player._pHitPoints = 10 << 6;
	player._pHPBase = 10 << 6;

	constexpr int QueuedAmount = 60 << 6; // 60 HP, comfortably larger than the tick count so it drips visibly
	QueueGradualHeal(QueuedAmount);

	ProcessGradualHealing(player);
	EXPECT_LT(player._pHitPoints, (10 << 6) + QueuedAmount) << "a single tick must not deliver the whole amount at once";
	EXPECT_GT(player._pHitPoints, 10 << 6) << "but it must have delivered something on the very first tick";

	// Drain well past any reasonable duration - the pool must be fully delivered and capped at max.
	for (int i = 0; i < 200; i++)
		ProcessGradualHealing(player);

	EXPECT_EQ(player._pHitPoints, std::min((10 << 6) + QueuedAmount, player._pMaxHP)) << "the full queued amount must eventually land, capped at max HP";
}

TEST(Player, GradualHealing_Mana_RespectsNoManaFlagButStillDrainsTheQueue)
{
	using namespace devilution::oracool;

	Players.resize(1);
	devilution::Player &player = Players[0];
	player._pMaxMana = 100 << 6;
	player._pMaxManaBase = 100 << 6;
	player._pMana = 0;
	player._pManaBase = 0;
	player._pIFlags = ItemSpecialEffect::NoMana;

	QueueGradualMana(60 << 6);
	for (int i = 0; i < 200; i++)
		ProcessGradualHealing(player);

	EXPECT_EQ(player._pMana, 0) << "NoMana must waste the queued potion exactly like RestorePartialMana already does, not silently ignore the flag";

	player._pIFlags = ItemSpecialEffect::None;
}

TEST(Player, ShouldDropGoldOnDeath_SinglePlayerNeverDrops)
{
	Players.resize(1);
	MyPlayer = &Players[0];
	gbIsMultiplayer = false;

	EXPECT_FALSE(TestShouldDropGoldOnDeath(Players[0]));
}

TEST(Player, ShouldDropGoldOnDeath_MultiplayerAlwaysDrops)
{
	Players.resize(1);
	MyPlayer = &Players[0];
	gbIsMultiplayer = true;

	EXPECT_TRUE(TestShouldDropGoldOnDeath(Players[0]));

	gbIsMultiplayer = false;
}

TEST(Player, ExpLvlsTbl_HasNinetyNineLevelsAndIsDiabloTwos)
{
	ASSERT_EQ(MaxCharacterLevel, 99);

	// Diablo II's table since 2026-09-20 (the Level Requirements plan, decision D3): the first
	// twenty levels and the cap, from the Arreat Summit. Vanilla Diablo's curve (2000 to level 2,
	// 1,310,707,109 to level 50) is gone.
	constexpr uint64_t DiabloTwoPrefix[] = {
		0, 500, 1500, 3750, 7875, 14175, 22680, 32886, 44396, 57715,
		72144, 90180, 112725, 140906, 176132, 220165, 275207, 344008, 430010, 537513
	};
	for (size_t i = 0; i < std::size(DiabloTwoPrefix); i++)
		EXPECT_EQ(ExpLvlsTbl[i], DiabloTwoPrefix[i]) << "level " << (i + 1);
	EXPECT_EQ(ExpLvlsTbl[49], 47116709u) << "level 50";
	EXPECT_EQ(ExpLvlsTbl[98], 3520485254u) << "level 99";
}

TEST(Player, ExpLvlsTbl_ExtendedRangeIsMonotonicAndFitsUint64)
{
	for (int i = 1; i < MaxCharacterLevel; i++) {
		EXPECT_GT(ExpLvlsTbl[i], ExpLvlsTbl[i - 1]) << "level " << (i + 1) << " did not require more experience than the previous level";
	}
	// Comfortably below UINT64_MAX - just confirms the curve didn't accidentally wrap/overflow.
	EXPECT_LT(ExpLvlsTbl[MaxCharacterLevel - 1], std::numeric_limits<uint64_t>::max() / 2);
}
