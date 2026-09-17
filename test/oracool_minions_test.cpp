/**
 * @file oracool_minions_test.cpp
 *
 * The army engine (Necromancer phase N4, v1.12.034): the pool above the enemies' 200, the user's group caps,
 * the thinking budget, and the rule that nothing of an army exists in town or outlives a game.
 */

#include <gtest/gtest.h>

#include "levels/gendung.h"
#include "monster.h"
#include "oracool/minions.h"
#include "player.h"

using namespace devilution;

TEST(OracoolMinions, TheArmyHasItsOwnPoolAboveTheEnemies)
{
	EXPECT_EQ(MaxEnemyMonsters, 200U) << "a floor keeps every monster slot it had before armies existed";
	EXPECT_EQ(MaxMinionBodies, 32U);
	EXPECT_EQ(MaxMonsters, MaxEnemyMonsters + MaxMinionBodies);
	EXPECT_LE(MaxMonsters, 255U) << "Monster::enemy is a uint8_t index into Monsters[]";
}

// Decision D7: 8 Skeletons, 8 Mages, 1 Golem, 10 Revived = 27, inside the pool of 32.
TEST(OracoolMinions, TheGroupCapsAreTheUsersAndFitThePool)
{
	EXPECT_EQ(oracool::MinionGroupCap(oracool::MinionGroup::Skeleton), 8);
	EXPECT_EQ(oracool::MinionGroupCap(oracool::MinionGroup::Mage), 8);
	EXPECT_EQ(oracool::MinionGroupCap(oracool::MinionGroup::Golem), 1);
	EXPECT_EQ(oracool::MinionGroupCap(oracool::MinionGroup::Revived), 10);
	int total = 0;
	for (size_t g = 0; g < oracool::MinionGroupCount; g++)
		total += oracool::MinionGroupCap(static_cast<oracool::MinionGroup>(g));
	EXPECT_EQ(total, 27);
	EXPECT_LE(static_cast<size_t>(total), MaxMinionBodies);
}

// An idle minion thinks on exactly one tick in three, and the army is spread evenly over the three.
TEST(OracoolMinions, TheThinkingBudgetIsAThirdAndEvenlySpread)
{
	int perPhase[3] = {};
	for (size_t id = MaxEnemyMonsters; id < MaxMonsters; id++) {
		int thinks = 0;
		for (uint32_t tick = 0; tick < 3; tick++) {
			if (oracool::MinionThinksThisTick(Monsters[id], tick)) {
				thinks++;
				perPhase[tick]++;
			}
		}
		EXPECT_EQ(thinks, 1) << "slot " << id;
	}
	for (const int count : perPhase)
		EXPECT_NEAR(count, static_cast<int>(MaxMinionBodies) / 3, 1);
}

TEST(OracoolMinions, NoArmyInTownAndNoneAfterForgetting)
{
	Players.resize(1);
	devilution::Player &player = Players[0];
	player = {};
	oracool::ForgetMinions();
	leveltype = DTYPE_TOWN;
	const oracool::MinionSpec spec { oracool::MinionGroup::Skeleton, MT_WSKELAX, 30, 2, 6, 60, 10 };
	EXPECT_FALSE(oracool::SummonMinion(player, spec, { 40, 40 }));
	EXPECT_EQ(oracool::MinionCount(player), 0);
	EXPECT_EQ(oracool::ActiveMinionBodies(), 0U);
	for (size_t id = 0; id < MaxMonsters; id++)
		EXPECT_FALSE(oracool::IsMinion(Monsters[id]));
	EXPECT_EQ(oracool::MinionOwner(Monsters[MaxMonsters - 1]), nullptr);
}
