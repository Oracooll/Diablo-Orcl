#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "engine/random.hpp"
#include "levels/gendung.h"
#include "missiles.h"
#include "options.h"
#include "storm/storm_net.hpp"

using namespace devilution;
using ::testing::AllOf;
using ::testing::Gt;
using ::testing::Lt;
using ::testing::Pair;
using ::testing::UnorderedElementsAre;

void TestArrowRotatesUniformly(Missile &missile, int startingFrame, int leftFrame, int rightFrame)
{
	std::unordered_map<int, unsigned> observed {};
	for (auto i = 0; i < 100; i++) {
		missile._miAnimFrame = startingFrame;
		TestRotateBlockedMissile(missile);
		observed[missile._miAnimFrame]++;
	}

	EXPECT_THAT(observed, UnorderedElementsAre(Pair(leftFrame, AllOf(Gt(30), Lt(70))), Pair(rightFrame, AllOf(Gt(30), Lt(70))))) << "Arrows should rotate either direction roughly 50% of the time";
}

void TestAnimatedMissileRotatesUniformly(Missile &missile, int startingDir, int leftDir, int rightDir)
{
	std::unordered_map<int, unsigned> observed {};
	for (auto i = 0; i < 100; i++) {
		missile._mimfnum = startingDir;
		TestRotateBlockedMissile(missile);
		observed[missile._mimfnum]++;
	}

	EXPECT_THAT(observed, UnorderedElementsAre(Pair(leftDir, AllOf(Gt(30), Lt(70))), Pair(rightDir, AllOf(Gt(30), Lt(70))))) << "Animated missiles should rotate either direction roughly 50% of the time";
}

TEST(Missiles, RotateBlockedMissileArrow)
{
	Players.resize(1);
	MyPlayerId = 0;
	MyPlayer = &Players[MyPlayerId];
	*MyPlayer = {};

	devilution::Player &player = Players[0];
	// missile can be a copy or a reference, there's no nullptr check and the functions that use it don't expect the instance to be part of a global structure so it doesn't really matter for this use.
	Missile missile = *AddMissile({ 0, 0 }, { 0, 0 }, Direction::South, MissileID::Arrow, TARGET_MONSTERS, player.getId(), 0, 0);

	// Arrows have a hardcoded frame count and use 1-indexed sprites
	EXPECT_EQ(missile._miAnimFrame, 1);

	TestArrowRotatesUniformly(missile, 5, 4, 6);
	TestArrowRotatesUniformly(missile, 1, 16, 2);
	TestArrowRotatesUniformly(missile, 16, 15, 1);

	// All other missiles use the number of 0-indexed sprites defined in MissileSpriteData
	missile = *AddMissile({ 0, 0 }, { 0, 0 }, Direction::South, MissileID::Firebolt, TARGET_MONSTERS, player.getId(), 0, 0);
	EXPECT_EQ(missile._mimfnum, 0);
	TestAnimatedMissileRotatesUniformly(missile, 5, 4, 6);
	TestAnimatedMissileRotatesUniformly(missile, 0, 15, 1);
	TestAnimatedMissileRotatesUniformly(missile, 15, 14, 0);
}

namespace {

devilution::Item MakeStackedBeltPotion(item_misc_id miscId, _item_indexes idx, int count)
{
	devilution::Item item;
	item._itype = ItemType::Misc;
	item._iClass = ICLASS_MISC;
	item._iMiscId = miscId;
	item.IDidx = idx;
	item._iIdentified = true;
	item._iStatFlag = true;
	item.setStackCount(count);
	return item;
}

class StealPotionsTest : public ::testing::Test {
public:
	void SetUp() override
	{
		SNetInitializeProvider(SELCONN_LOOPBACK, nullptr);
		Players.resize(1);
		MyPlayerId = 0;
		MyPlayer = &Players[0];
		*MyPlayer = {};
		gbIsMultiplayer = false;

		MyPlayer->position.tile = { 5, 5 };
		dPlayer[5][5] = 1;

		missile.position.start = { 5, 5 };
	}

	void TearDown() override
	{
		dPlayer[5][5] = 0;
	}

	Missile missile;
	AddMissileParameter param {};
};

// A stolen stack must only lose one unit, never the whole stack (base-tier
// IMISC_HEAL/IMISC_MANA potions are removed outright rather than downgraded).
TEST_F(StealPotionsTest, StealFromStackedBaseTierPotionOnlyRemovesOneUnit)
{
	constexpr int StartingCount = 40;
	bool observedADecrement = false;
	for (int attempt = 0; attempt < 30 && !observedADecrement; attempt++) {
		for (auto &beltItem : MyPlayer->SpdList)
			beltItem = MakeStackedBeltPotion(IMISC_HEAL, IDI_HEAL, StartingCount);

		AddStealPotions(missile, param);

		for (const devilution::Item &beltItem : MyPlayer->SpdList) {
			if (beltItem.isEmpty()) {
				// The old bug removed the whole slot outright; that must never happen.
				FAIL() << "A stacked potion slot was fully removed instead of decremented";
			}
			ASSERT_GE(beltItem.stackCount(), StartingCount - 1);
			ASSERT_LE(beltItem.stackCount(), StartingCount);
			if (beltItem.stackCount() == StartingCount - 1)
				observedADecrement = true;
		}
	}
	EXPECT_TRUE(observedADecrement) << "Expected at least one belt slot to be stolen from across 30 attempts";
}

// Stealing from a stacked upgraded-tier potion (which downgrades on steal) must only
// split off one unit; the remaining stack must keep its original identity and count.
TEST_F(StealPotionsTest, StealFromStackedUpgradedTierPotionOnlyDecrementsSourceStack)
{
	constexpr int StartingCount = 40;
	bool observedADecrement = false;
	for (int attempt = 0; attempt < 30 && !observedADecrement; attempt++) {
		for (auto &beltItem : MyPlayer->SpdList)
			beltItem.clear();
		MyPlayer->SpdList[0] = MakeStackedBeltPotion(IMISC_FULLHEAL, IDI_FULLHEAL, StartingCount);

		AddStealPotions(missile, param);

		devilution::Item &source = MyPlayer->SpdList[0];
		ASSERT_FALSE(source.isEmpty()) << "The source stack must never be fully removed by a steal";
		ASSERT_EQ(source._iMiscId, IMISC_FULLHEAL) << "The source stack's identity must not change";
		ASSERT_GE(source.stackCount(), StartingCount - 1);
		ASSERT_LE(source.stackCount(), StartingCount);
		if (source.stackCount() == StartingCount - 1)
			observedADecrement = true;
	}
	EXPECT_TRUE(observedADecrement) << "Expected the source stack to be stolen from across 30 attempts";
}

} // namespace

TEST(Missiles, GetDirection8)
{
	EXPECT_EQ(Direction::South, GetDirection({ 0, 0 }, { 15, 15 }));
	EXPECT_EQ(Direction::SouthWest, GetDirection({ 0, 0 }, { 0, 15 }));
	EXPECT_EQ(Direction::South, GetDirection({ 0, 0 }, { 8, 15 }));
	EXPECT_EQ(Direction::South, GetDirection({ 0, 0 }, { 8, 8 }));
	EXPECT_EQ(Direction::South, GetDirection({ 0, 0 }, { 15, 8 }));
	EXPECT_EQ(Direction::South, GetDirection({ 0, 0 }, { 15, 7 }));
	EXPECT_EQ(Direction::South, GetDirection({ 0, 0 }, { 11, 7 }));
	EXPECT_EQ(Direction::South, GetDirection({ 0, 0 }, { 8, 11 }));
	EXPECT_EQ(Direction::North, GetDirection({ 15, 15 }, { 0, 0 }));
	EXPECT_EQ(Direction::NorthEast, GetDirection({ 0, 15 }, { 0, 0 }));
	EXPECT_EQ(Direction::North, GetDirection({ 8, 15 }, { 0, 0 }));
	EXPECT_EQ(Direction::North, GetDirection({ 8, 8 }, { 0, 0 }));
	EXPECT_EQ(Direction::North, GetDirection({ 15, 8 }, { 0, 0 }));
	EXPECT_EQ(Direction::North, GetDirection({ 15, 7 }, { 0, 0 }));
	EXPECT_EQ(Direction::North, GetDirection({ 11, 7 }, { 0, 0 }));
	EXPECT_EQ(Direction::North, GetDirection({ 8, 11 }, { 0, 0 }));
	EXPECT_EQ(Direction::East, GetDirection({ 0, 15 }, { 15, 0 }));
	EXPECT_EQ(Direction::SouthEast, GetDirection({ 0, 0 }, { 15, 0 }));
	EXPECT_EQ(Direction::East, GetDirection({ 0, 8 }, { 15, 0 }));
	EXPECT_EQ(Direction::East, GetDirection({ 0, 8 }, { 8, 0 }));
	EXPECT_EQ(Direction::East, GetDirection({ 0, 15 }, { 8, 0 }));
	EXPECT_EQ(Direction::East, GetDirection({ 0, 15 }, { 7, 0 }));
	EXPECT_EQ(Direction::East, GetDirection({ 0, 11 }, { 7, 0 }));
	EXPECT_EQ(Direction::East, GetDirection({ 0, 8 }, { 11, 0 }));

	EXPECT_EQ(Direction::South, GetDirection({ 1, 1 }, { 2, 2 }));
	EXPECT_EQ(Direction::SouthWest, GetDirection({ 1, 1 }, { 1, 2 }));
	EXPECT_EQ(Direction::West, GetDirection({ 1, 1 }, { 0, 2 }));
	EXPECT_EQ(Direction::NorthWest, GetDirection({ 1, 1 }, { 0, 1 }));
	EXPECT_EQ(Direction::North, GetDirection({ 1, 1 }, { 0, 0 }));
	EXPECT_EQ(Direction::NorthEast, GetDirection({ 1, 1 }, { 1, 0 }));
	EXPECT_EQ(Direction::East, GetDirection({ 1, 1 }, { 2, 0 }));
	EXPECT_EQ(Direction::SouthEast, GetDirection({ 1, 1 }, { 2, 1 }));

	EXPECT_EQ(Direction::SouthWest, GetDirection({ 0, 0 }, { 0, 0 })) << "GetDirection is expected to default to Direction::SouthWest when the points occupy the same tile";
}

TEST(Missiles, GetDirection16)
{
	EXPECT_EQ(Direction16::South, GetDirection16({ 0, 0 }, { 15, 15 }));
	EXPECT_EQ(Direction16::SouthWest, GetDirection16({ 0, 0 }, { 0, 15 }));
	EXPECT_EQ(Direction16::South_SouthWest, GetDirection16({ 0, 0 }, { 8, 15 }));
	EXPECT_EQ(Direction16::South, GetDirection16({ 0, 0 }, { 8, 8 }));
	EXPECT_EQ(Direction16::South_SouthEast, GetDirection16({ 0, 0 }, { 15, 8 }));
	EXPECT_EQ(Direction16::South_SouthEast, GetDirection16({ 0, 0 }, { 15, 7 }));
	EXPECT_EQ(Direction16::South_SouthEast, GetDirection16({ 0, 0 }, { 11, 7 }));
	EXPECT_EQ(Direction16::South, GetDirection16({ 0, 0 }, { 8, 11 }));
	EXPECT_EQ(Direction16::North, GetDirection16({ 15, 15 }, { 0, 0 }));
	EXPECT_EQ(Direction16::NorthEast, GetDirection16({ 0, 15 }, { 0, 0 }));
	EXPECT_EQ(Direction16::North_NorthEast, GetDirection16({ 8, 15 }, { 0, 0 }));
	EXPECT_EQ(Direction16::North, GetDirection16({ 8, 8 }, { 0, 0 }));
	EXPECT_EQ(Direction16::North_NorthWest, GetDirection16({ 15, 8 }, { 0, 0 }));
	EXPECT_EQ(Direction16::North_NorthWest, GetDirection16({ 15, 7 }, { 0, 0 }));
	EXPECT_EQ(Direction16::North_NorthWest, GetDirection16({ 11, 7 }, { 0, 0 }));
	EXPECT_EQ(Direction16::North, GetDirection16({ 8, 11 }, { 0, 0 }));
	EXPECT_EQ(Direction16::East, GetDirection16({ 0, 15 }, { 15, 0 }));
	EXPECT_EQ(Direction16::SouthEast, GetDirection16({ 0, 0 }, { 15, 0 }));
	EXPECT_EQ(Direction16::East_SouthEast, GetDirection16({ 0, 8 }, { 15, 0 }));
	EXPECT_EQ(Direction16::East, GetDirection16({ 0, 8 }, { 8, 0 }));
	EXPECT_EQ(Direction16::East_NorthEast, GetDirection16({ 0, 15 }, { 8, 0 }));
	EXPECT_EQ(Direction16::East_NorthEast, GetDirection16({ 0, 15 }, { 7, 0 }));
	EXPECT_EQ(Direction16::East_NorthEast, GetDirection16({ 0, 11 }, { 7, 0 }));
	EXPECT_EQ(Direction16::East, GetDirection16({ 0, 8 }, { 11, 0 }));

	EXPECT_EQ(Direction16::South, GetDirection16({ 2, 2 }, { 3, 3 }));
	EXPECT_EQ(Direction16::South_SouthWest, GetDirection16({ 2, 2 }, { 3, 4 }));
	EXPECT_EQ(Direction16::SouthWest, GetDirection16({ 2, 2 }, { 2, 4 }));
	EXPECT_EQ(Direction16::West_SouthWest, GetDirection16({ 2, 2 }, { 1, 4 }));
	EXPECT_EQ(Direction16::West, GetDirection16({ 2, 2 }, { 1, 3 }));
	EXPECT_EQ(Direction16::West_NorthWest, GetDirection16({ 2, 2 }, { 0, 3 }));
	EXPECT_EQ(Direction16::NorthWest, GetDirection16({ 2, 2 }, { 0, 2 }));
	EXPECT_EQ(Direction16::North_NorthWest, GetDirection16({ 2, 2 }, { 0, 1 }));
	EXPECT_EQ(Direction16::North, GetDirection16({ 2, 2 }, { 1, 1 }));
	EXPECT_EQ(Direction16::North_NorthEast, GetDirection16({ 2, 2 }, { 1, 0 }));
	EXPECT_EQ(Direction16::NorthEast, GetDirection16({ 2, 2 }, { 2, 0 }));
	EXPECT_EQ(Direction16::East_NorthEast, GetDirection16({ 2, 2 }, { 3, 0 }));
	EXPECT_EQ(Direction16::East, GetDirection16({ 2, 2 }, { 3, 1 }));
	EXPECT_EQ(Direction16::East_SouthEast, GetDirection16({ 2, 2 }, { 4, 1 }));
	EXPECT_EQ(Direction16::SouthEast, GetDirection16({ 2, 2 }, { 4, 2 }));
	EXPECT_EQ(Direction16::South_SouthEast, GetDirection16({ 2, 2 }, { 4, 3 }));

	EXPECT_EQ(Direction16::South_SouthWest, GetDirection16({ 0, 0 }, { 0, 0 })) << "GetDirection16 is expected to default to Direction16::South_SouthWest when the points occupy the same tile";
}
