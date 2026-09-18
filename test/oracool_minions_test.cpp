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

// ---- The Summoning page (phase N5, v1.12.035) ----

#include "oracool/corpses.h"
#include "oracool/hero_chunks.h"
#include "oracool/necro_summoning.h"
#include "oracool/readied_spells.h"
#include "oracool/class_skills.h"
#include "spells.h"

TEST(OracoolNecroSummoning, TheRaisedCountClimbsOneEveryThreeRanksToTheCap)
{
	EXPECT_EQ(oracool::RaisedCountAtRank(1, 8), 1);
	EXPECT_EQ(oracool::RaisedCountAtRank(3, 8), 1);
	EXPECT_EQ(oracool::RaisedCountAtRank(4, 8), 2);
	EXPECT_EQ(oracool::RaisedCountAtRank(22, 8), 8);
	EXPECT_EQ(oracool::RaisedCountAtRank(40, 8), 8) << "never past the cap";
	EXPECT_EQ(oracool::RaisedCountAtRank(28, 10), 10);
	EXPECT_EQ(oracool::RaisedCountAtRank(1, 1), 1);
}

TEST(OracoolNecroSummoning, ThirteenActivesAllHaveAnIdPastTheOldCeiling)
{
	int count = 0;
	for (int i = 0; i <= static_cast<int>(SpellID::LAST); i++) {
		if (oracool::IsNecromancerSummoning(static_cast<SpellID>(i)))
			count++;
	}
	EXPECT_EQ(count, 13);
	EXPECT_GT(static_cast<int>(SpellID::ArmyOfTheDead), 254) << "the page is what took the enum past a byte";
	EXPECT_EQ(static_cast<int>(SpellID::LAST) + 1, MAX_SPELLS);
}

// The ceilings the page broke through, each with the form that carries it now.
TEST(OracoolNecroSummoning, AReadiedSpellPastTheByteRoundTripsThroughTheTail)
{
	gbIsHellfire = true; // IsValidSpell admits the fork's ids only in Hellfire mode, as the game always runs
	const SpellID high = SpellID::ArmyOfTheDead;
	EXPECT_EQ(oracool::PackReadiedSpell(high), 0) << "the byte cannot carry it, and must not wrap into another spell";
	EXPECT_EQ(oracool::PackReadiedSpell16(high), static_cast<uint16_t>(static_cast<int>(high) + 1));
	EXPECT_NE(GetSpellBitmask(high), SpellMask {}) << "the fifth mask word";
	EXPECT_EQ(GetSpellBitmask(high) & GetSpellBitmask(SpellID::Firebolt), SpellMask {});

	Players.resize(1);
	devilution::Player &source = Players[0];
	source = {};
	source._pClass = HeroClass::Necromancer;
	source._pLevel = 30;
	source._pSkillInvestment[static_cast<size_t>(high)] = 3;
	source._pSkillInvestment[static_cast<size_t>(SpellID::RaiseSkeleton)] = 5;
	oracool::RefreshInnateSpells(source);
	ASSERT_NE(source._pAblSpells & GetSpellBitmask(high), SpellMask {}) << "test setup: the invested skill is known";
	source._pRSpell = high;
	source._pRSplType = SpellType::Skill;
	source._pSplHotKey[3] = high;
	source._pSplTHotKey[3] = SpellType::Skill;

	const std::vector<uint8_t> tail = oracool::BuildHeroChunkTail(source);
	devilution::Player loaded {};
	loaded._pClass = HeroClass::Necromancer;
	loaded._pLevel = 30;
	oracool::ApplyHeroChunks(loaded, tail.data(), tail.size());
	EXPECT_EQ(loaded._pSkillInvestment[static_cast<size_t>(high)], 3) << "the investment past entry 255 - tag 18";
	EXPECT_EQ(loaded._pSkillInvestment[static_cast<size_t>(SpellID::RaiseSkeleton)], 5);
	EXPECT_EQ(loaded._pRSpell, high) << "the readied spell - tag 15";
	EXPECT_EQ(loaded._pSplHotKey[3], high) << "the hotkey - tag 16";
}

TEST(OracoolNecroSummoning, TheCorpseTableStartsEmptyAndIsEmptyAgainOnALevel)
{
	oracool::ClearCorpses();
	EXPECT_EQ(oracool::CorpseCount(), 0);
	EXPECT_FALSE(oracool::CorpseNear({ 40, 40 }, 3, false));
	EXPECT_FALSE(oracool::TakeCorpseNear({ 40, 40 }, 3, true).has_value());
}
