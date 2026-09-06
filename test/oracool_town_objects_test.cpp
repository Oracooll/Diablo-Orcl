/**
 * @file oracool_town_objects_test.cpp
 *
 * Oracool: the object-instance harness. Builds town's object pool and asserts against the objects
 * that come out of it.
 *
 * ## Why this file exists
 *
 * Nothing in the suite asserted anything about a placed object AFTER AddObject. Placement,
 * selectability and operate routing were all invisible to 470 passing tests, and the fork has now
 * shipped the same class of bug four times in one week:
 *
 *   1.8.17  Levski's Roar drew perfectly and ignored every click. OBJ_STAND's table row carries
 *           selFlag 0 - in the Caves it is scenery, never something the player targets - and
 *           AddObject copies that straight onto the instance.
 *   1.8.19  It had a cursor and an outline but no hover name and no working operate.
 *   1.8.62  orclstash.cel was rebuilt, installed to both asset channels and packed into the MPQ,
 *           and nothing loaded any of it: ApplyStashChestGraphics was commented out.
 *   1.8.64  The monument was walked ONTO rather than approached, because _oSolidFlag had been
 *           cleared. In this engine that flag is really an activation-range switch.
 *
 * Every one of those passed a clean build and a clean suite. The tests below would have caught
 * three of the four.
 *
 * ## What this harness deliberately cannot check
 *
 * SPRITES. HeadlessMode short-circuits the whole graphics path - SetupObject skips its
 * ObjFileList lookup, EnsureObjectGraphicsLoaded returns immediately, and so do
 * ApplyStashChestGraphics and ApplyLevskiRoarGraphics. So _oAnimData is always empty here and
 * asserting on it would only re-state that tests are headless.
 *
 * That is exactly the gap 1.8.62 fell through, and it is stated plainly rather than papered over:
 * an assertion that passes for the wrong reason is worse than no assertion. Whether an object wears
 * the right art is still a question only the screen can answer. What this file CAN pin is
 * everything that decides whether the player can reach it and interact with it - and in the four
 * failures above, that was three times out of four the actual defect.
 *
 * ## Positions are PINS, not lookups
 *
 * The tile coordinates below are written out rather than read from the constants the code uses.
 * That is on purpose. Reading the constant would make the test agree with any move, silently; a
 * literal fails when furniture moves, which forces a conscious update. The monument has already
 * moved once (1.8.18), and that is precisely the moment a harness should speak up.
 */
#include <cstring>

#include <gtest/gtest.h>

#include "diablo.h"
#include "levels/gendung.h"
#include "objdat.h"
#include "objects.h"
#include "oracool/levski_roar.h"
#include "oracool/oracool.h"
#include "utils/attributes.h"

using namespace devilution;

namespace {

/** @brief The town tiles this fork pins furniture to. Deliberately literals - see the file header. */
constexpr Point StashChestTile { 55, 68 }; // moved again on the user's call, v1.9.139
constexpr Point LevskiRoarTile { 55, 66 };
constexpr Point WaypointSigilTile { 61, 80 };
/**
 * @brief Where a new character is created - SetupLocalPositions' spawns[0] (multi.cpp), which
 * CreateTown's ENTRY_MAIN ViewPosition also centres on.
 *
 * A literal like the three above, and for the same reason. This one exists because moving the chest
 * to {56,67} in v1.9.115 put it exactly on the spawn, and a fresh character was created standing
 * inside a solid object. Nothing said so: the furniture-collision test below only compared
 * furniture with furniture, and the chest's own placement guard logs a collision and then places
 * the chest regardless.
 */
constexpr Point NewGamePlayerSpawnTile { 57, 67 }; // 2026-09-06: the user's "57,67"

/**
 * @brief Puts the world into "fresh town" and places the three Oracool town objects.
 *
 * This is the same order diablo.cpp's town branch uses. InitTownObjectPool clears the pool and is
 * itself guarded on being in town, so calling it is also a check that the guard agrees with us.
 */
void BuildTown()
{
	leveltype = DTYPE_TOWN;
	currlevel = 0;
	setlevel = false;

	// dObject FIRST, and it is not optional.
	//
	// ClrAllObjects - which is all InitTownObjectPool does - resets Objects, ActiveObjectCount,
	// ActiveObjects and AvailableObjects, and does NOT touch dObject. In a running game that is
	// correct, because level generation zeroes the dungeon arrays before objects are placed. A
	// harness has no level generation, so without this the tile map keeps every index from the
	// previous test.
	//
	// The symptom is worth writing down, because it cost a debugging round: every placement test
	// passed in isolation and two failed in sequence. AddLevskiRoarObject skips any candidate whose
	// dObject is non-zero, so on the second BuildTown its requested tile looked occupied, it walked
	// its fallback list, and after a few rounds every candidate looked occupied and it placed
	// nothing at all. The tests were then asserting against whatever stale object the tile map still
	// pointed at.
	memset(dObject, 0, sizeof(dObject));

	oracool::InitTownObjectPool();
	oracool::AddStashChestObject();
	oracool::AddLevskiRoarObject();
	oracool::AddWaypointSigilObject();
}

/** @brief Leaves the globals somewhere harmless so ordering between tests cannot matter. */
void TearDownTown()
{
	leveltype = DTYPE_TOWN;
	currlevel = 0;
	setlevel = false;
	oracool::InitTownObjectPool();
}

/**
 * @brief The predicate msg.cpp:1532 actually uses to decide where a click walks you.
 *
 * `MakePlrPath(player, position, !object->_oSolidFlag && !object->_oDoorFlag)` - the third argument
 * is endsAtTarget. True means the path finishes ON the object's tile; false means it stops beside
 * it. Spelled out here rather than referenced, because the whole point is to pin the value this
 * expression produces for our furniture.
 */
bool PathWouldEndOnTheObject(const Object &object)
{
	return !object._oSolidFlag && !object._oDoorFlag;
}

/** @brief cursor.cpp rejects an object outright at selFlag 0; 1 and 3 are the pickable values. */
bool IsSelectable(const Object &object)
{
	return object._oSelFlag == 1 || object._oSelFlag == 3;
}

class TownObjects : public ::testing::Test {
protected:
	void SetUp() override { BuildTown(); }
	void TearDown() override { TearDownTown(); }
};

} // namespace

// ---------------------------------------------------------------------------------------------
// Placement
// ---------------------------------------------------------------------------------------------

TEST_F(TownObjects, StashChestIsPlacedOnItsTile)
{
	Object *chest = FindObjectAtPosition(StashChestTile);
	ASSERT_NE(chest, nullptr) << "no object at the stash chest's tile";
	EXPECT_EQ(chest->_otype, OBJ_CHEST3)
	    << "the stash chest is deliberately an ordinary OBJ_CHEST3 wearing its own art - a custom "
	       "type was tried and reverted because it drew in the wrong order";
}

TEST_F(TownObjects, LevskiRoarIsPlacedOnItsTile)
{
	Object *monument = FindObjectAtPosition(LevskiRoarTile);
	ASSERT_NE(monument, nullptr) << "no object at Levski's Roar's tile";
	EXPECT_EQ(monument->_otype, OBJ_STAND);
}

TEST_F(TownObjects, WaypointSigilIsPlacedOnItsTile)
{
	Object *sigil = FindObjectAtPosition(WaypointSigilTile);
	ASSERT_NE(sigil, nullptr) << "no object at the town waypoint's tile";
	EXPECT_EQ(sigil->_otype, OBJ_WAYPOINT);
	EXPECT_EQ(sigil->_oVar1, 0) << "the town sigil's travel-list index is 0, Tristram";
}

TEST_F(TownObjects, TownFurnitureDoesNotShareTiles)
{
	// Two operable objects on one tile is a silent failure: dObject holds one index, so whichever
	// was placed second is the only one reachable and the first is invisible to every click.
	EXPECT_NE(StashChestTile, LevskiRoarTile);
	EXPECT_NE(StashChestTile, WaypointSigilTile);
	EXPECT_NE(LevskiRoarTile, WaypointSigilTile);

	// And none of them may stand on the tile a new character is created on. Every one of these
	// three is solid - in this engine _oSolidFlag is really the activation-range switch, so an
	// operable object always has it - and a character created inside one starts the game stuck in
	// the furniture it is supposed to walk up to. The chest did exactly this from v1.9.115 to
	// v1.9.123; external audit GP-01, 2026-08-30.
	EXPECT_NE(StashChestTile, NewGamePlayerSpawnTile)
	    << "the Stash Chest is on the new-game spawn tile";
	EXPECT_NE(LevskiRoarTile, NewGamePlayerSpawnTile)
	    << "Levski's Roar is on the new-game spawn tile";
	EXPECT_NE(WaypointSigilTile, NewGamePlayerSpawnTile)
	    << "the town sigil is on the new-game spawn tile";

	// The spawn must also be free of ANY object, not just the three named above - the check that
	// does not need updating when a fourth piece of furniture arrives.
	EXPECT_EQ(FindObjectAtPosition(NewGamePlayerSpawnTile), nullptr)
	    << "some object occupies the tile a new character is created on";

	Object *chest = FindObjectAtPosition(StashChestTile);
	Object *monument = FindObjectAtPosition(LevskiRoarTile);
	Object *sigil = FindObjectAtPosition(WaypointSigilTile);
	ASSERT_NE(chest, nullptr);
	ASSERT_NE(monument, nullptr);
	ASSERT_NE(sigil, nullptr);
	EXPECT_NE(chest, monument);
	EXPECT_NE(chest, sigil);
	EXPECT_NE(monument, sigil);
}

// ---------------------------------------------------------------------------------------------
// Selectability - the 1.8.17 and 1.8.19 failures
// ---------------------------------------------------------------------------------------------

TEST_F(TownObjects, LevskiRoarIsSelectable)
{
	Object *monument = FindObjectAtPosition(LevskiRoarTile);
	ASSERT_NE(monument, nullptr);

	// THE 1.8.17 REGRESSION. AllObjects[OBJ_STAND].selFlag is 0 and AddObject copies it onto the
	// instance, so the monument drew perfectly and was completely unreachable. AddLevskiRoarObject
	// overrides it to 3 - the bookstand's value, whole tile takes a click.
	EXPECT_EQ(monument->_oSelFlag, 3)
	    << "OBJ_STAND ships with selFlag 0; the override in AddLevskiRoarObject is what makes the "
	       "monument clickable at all";
	EXPECT_TRUE(IsSelectable(*monument));
}

TEST_F(TownObjects, StashChestIsSelectable)
{
	Object *chest = FindObjectAtPosition(StashChestTile);
	ASSERT_NE(chest, nullptr);
	EXPECT_TRUE(IsSelectable(*chest)) << "selFlag " << int { chest->_oSelFlag };
}

TEST_F(TownObjects, WaypointSigilIsSelectable)
{
	Object *sigil = FindObjectAtPosition(WaypointSigilTile);
	ASSERT_NE(sigil, nullptr);
	EXPECT_TRUE(IsSelectable(*sigil)) << "selFlag " << int { sigil->_oSelFlag };
}

// ---------------------------------------------------------------------------------------------
// Operate routing
// ---------------------------------------------------------------------------------------------

TEST_F(TownObjects, LevskiRoarOperatesRatherThanTakingAHit)
{
	Object *monument = FindObjectAtPosition(LevskiRoarTile);
	ASSERT_NE(monument, nullptr);

	// ACTION_OPERATE in player.cpp swings at the object instead of operating it when _oBreak is 1.
	// A monument that could be attacked would never open its window.
	EXPECT_EQ(monument->_oBreak, 0) << "a breakable object is attacked, not operated";
}

TEST_F(TownObjects, LevskiRoarIsApproachedNotWalkedOnto)
{
	Object *monument = FindObjectAtPosition(LevskiRoarTile);
	ASSERT_NE(monument, nullptr);

	// THE 1.8.64 REGRESSION, and the reason this file exists in the shape it does. _oSolidFlag was
	// cleared on 2026-08-19 as insurance against a bug that had already been fixed (see
	// AddLevskiRoarObject). The cost was invisible while the monument was a knee-high rock stand
	// and obvious the moment it became a statue on a plaza: the hero walked through the middle of
	// it to activate it.
	EXPECT_TRUE(monument->_oSolidFlag)
	    << "solid is this engine's one-tile activation range - see PathWouldEndOnTheObject";
	EXPECT_FALSE(PathWouldEndOnTheObject(*monument))
	    << "the click path must stop BESIDE Levski's Roar, not on it";
}

TEST_F(TownObjects, StashChestIsApproachedNotWalkedOnto)
{
	Object *chest = FindObjectAtPosition(StashChestTile);
	ASSERT_NE(chest, nullptr);
	EXPECT_FALSE(PathWouldEndOnTheObject(*chest))
	    << "the click path must stop beside the stash chest, not on it";
}

TEST_F(TownObjects, StashChestStartsClosedOnItsPinnedFrame)
{
	Object *chest = FindObjectAtPosition(StashChestTile);
	ASSERT_NE(chest, nullptr);

	// chest3.cel carries two closed-chest variants and AddChest picks one with a coin flip.
	// AddStashChestObject pins frame 4, the variant the user chose, so the town chest is the same
	// every game. OperateStashChest steps 4 -> 6 and CloseStashChestObject steps back, so this
	// number is also the anchor both of those depend on.
	EXPECT_EQ(chest->_oAnimFrame, 4u) << "the stash chest must start on its pinned closed frame";
}

// ---------------------------------------------------------------------------------------------
// The paths that only run when something has gone wrong
// ---------------------------------------------------------------------------------------------

TEST_F(TownObjects, LevskiRoarFallsBackWhenItsOwnTileIsTaken)
{
	// AddLevskiRoarObject walks a candidate list when its requested tile is occupied. That branch
	// has never run in play, which is the usual condition for a branch to be broken - so it is
	// driven here directly.
	oracool::InitTownObjectPool();
	AddObject(OBJ_BARREL, LevskiRoarTile);
	ASSERT_NE(FindObjectAtPosition(LevskiRoarTile), nullptr);

	oracool::AddLevskiRoarObject();

	Object *monument = nullptr;
	for (int i = 0; i < ActiveObjectCount; i++) {
		Object &candidate = Objects[ActiveObjects[i]];
		if (candidate._otype == OBJ_STAND) {
			monument = &candidate;
			break;
		}
	}
	ASSERT_NE(monument, nullptr) << "the monument was dropped entirely when its tile was occupied";
	EXPECT_NE(monument->position, LevskiRoarTile) << "it should have fallen back to another tile";

	// A fallback placement is still a real monument: it must stay clickable and stay solid, or the
	// fallback quietly ships a monument nobody can use. This is also why SyncObjectAnim matches the
	// monument on TYPE rather than on a position constant.
	EXPECT_EQ(monument->_oSelFlag, 3);
	EXPECT_TRUE(monument->_oSolidFlag);
	EXPECT_EQ(monument->_oBreak, 0);
}

TEST_F(TownObjects, NoTownFurnitureIsPlacedOutsideTown)
{
	// Every one of these is guarded on currlevel == 0. A guard that stopped working would put a
	// stash chest and a monument on a dungeon floor, which is the kind of thing that reads as level
	// generation having gone mad rather than as a missing early-out.
	oracool::InitTownObjectPool();
	const int before = ActiveObjectCount;

	leveltype = DTYPE_CATHEDRAL;
	currlevel = 1;
	oracool::AddStashChestObject();
	oracool::AddLevskiRoarObject();

	EXPECT_EQ(ActiveObjectCount, before)
	    << "town furniture was placed on a dungeon level";
}

TEST_F(TownObjects, TownObjectPoolIsOnlyClearedInTown)
{
	// InitTownObjectPool wipes the pool, and its own comment says calling it twice in one town
	// generation would erase objects placed earlier. Its guard is therefore load-bearing: a
	// dungeon call must be a no-op, not a wipe.
	leveltype = DTYPE_CATHEDRAL;
	currlevel = 1;
	AddObject(OBJ_BARREL, { 40, 40 });
	const int before = ActiveObjectCount;
	ASSERT_GT(before, 0);

	oracool::InitTownObjectPool();

	EXPECT_EQ(ActiveObjectCount, before)
	    << "InitTownObjectPool cleared a dungeon level's objects";
}

