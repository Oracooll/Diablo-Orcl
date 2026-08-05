#include "player_test.h"

#include <cstdint>
#include <limits>

#include <gtest/gtest.h>

#include "options.h"
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
	MyPlayer = &Players[0];
	for (size_t i = 0; i < sizeof(BlockData) / sizeof(*BlockData); i++) {
		EXPECT_EQ(BlockData[i].expectedRecoveryFrame, RunBlockTest(BlockData[i].maxRecoveryFrame, BlockData[i].itemFlags));
	}
}

static void AssertPlayer(devilution::Player &player)
{
	ASSERT_EQ(CountU8(player._pSplLvl, 64), 0);
	ASSERT_EQ(Count8(player.InvGrid, InventoryGridCells), 1);
	ASSERT_EQ(CountItems(player.InvBody, NUM_INVLOC), 1);
	ASSERT_EQ(CountItems(player.InvList, InventoryGridCells), 1);
	ASSERT_EQ(CountItems(player.SpdList, MaxBeltItems), 2);
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
	ASSERT_EQ(player._pGold, 100);
	ASSERT_EQ(player._pMaxHPBase, 2880);
	ASSERT_EQ(player._pHPBase, 2880);
	ASSERT_EQ(player._pBaseToBlk, 20);
	ASSERT_EQ(player._pMaxManaBase, 1440);
	ASSERT_EQ(player._pManaBase, 1440);
	ASSERT_EQ(player._pMemSpells, 0);
	ASSERT_EQ(player._pNumInv, 1);
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
	ASSERT_EQ(player._pRSpell, SpellID::TrapDisarm);
	ASSERT_EQ(player._pRSplType, SpellType::Skill);
	ASSERT_EQ(player._pSBkSpell, SpellID::Null);
	ASSERT_EQ(player._pAblSpells, 134217728);
	ASSERT_EQ(player._pScrlSpells, 0);
	ASSERT_EQ(player._pSpellFlags, SpellFlag::None);
	ASSERT_EQ(player._pBlockFlag, 0);
	ASSERT_EQ(player._pLightRad, 10);
	ASSERT_EQ(player._pDamageMod, 0);
	ASSERT_EQ(player._pHitPoints, 2880);
	ASSERT_EQ(player._pMaxHP, 2880);
	ASSERT_EQ(player._pMana, 1440);
	ASSERT_EQ(player._pMaxMana, 1440);
	ASSERT_EQ(player._pNextExper, 2000);
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

TEST(Player, ExpLvlsTbl_HasNinetyNineLevelsAndVanillaPrefixUnchanged)
{
	ASSERT_EQ(MaxCharacterLevel, 99);

	// Levels 1-50 must be byte-for-byte the original vanilla table.
	constexpr uint64_t VanillaPrefix[] = {
		0, 2000, 4620, 8040, 12489, 18258, 25712, 35309, 47622, 63364,
		83419, 108879, 141086, 181683, 231075, 313656, 424067, 571190, 766569, 1025154,
		1366227, 1814568, 2401895, 3168651, 4166200, 5459523, 7130496, 9281874, 12042092, 15571031,
		20066900, 25774405, 32994399, 42095202, 53525811, 67831218, 85670061, 107834823, 135274799, 169122009,
		210720231, 261657253, 323800420, 399335440, 490808349, 601170414, 733825617, 892680222, 1082908612, 1310707109
	};
	for (size_t i = 0; i < std::size(VanillaPrefix); i++)
		EXPECT_EQ(ExpLvlsTbl[i], VanillaPrefix[i]) << "level " << (i + 1);
}

TEST(Player, ExpLvlsTbl_ExtendedRangeIsMonotonicAndFitsUint64)
{
	for (int i = 1; i < MaxCharacterLevel; i++) {
		EXPECT_GT(ExpLvlsTbl[i], ExpLvlsTbl[i - 1]) << "level " << (i + 1) << " did not require more experience than the previous level";
	}
	// Comfortably below UINT64_MAX - just confirms the curve didn't accidentally wrap/overflow.
	EXPECT_LT(ExpLvlsTbl[MaxCharacterLevel - 1], std::numeric_limits<uint64_t>::max() / 2);
}
