/**
 * @file oracool/woods.cpp
 *
 * The Orcl Act's woods (oracool/woods.h): the generator, the way in and out, and the true-colour ground and trees.
 */
#include "oracool/woods.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <string>
#include <memory>
#include <unordered_map>
#include <utility>
#include <vector>

#include <SDL.h>

#include "diablo.h"
#include "engine/palette.h"
#include "engine/render/dun_render.hpp" // RenderTile - the Mixed floor's cache is drawn by it once
#include "engine/random.hpp"
#include "engine/surface.hpp"
#include "init.h" // gbIsHellfire - the cave mouth is Hellfire's town art
#include "interfac.h"
#include "items.h" // CreateRndItem - what the houses hold
#include "levels/gendung.h"
#include "levels/trigs.h"
#include "missiles.h" // FrostMissileFor - the ice versions of the monsters' shots
#include "monstdat.h"
#include "monster.h" // ScaleWoodsMonster, the boss's spot
#include "oracool/area_level.h"
#include "oracool/borrowed_tiles.h" // the Caves' fences on the woods floors
#include "oracool/oracool.h"       // AddGeneratedLevelWaypointSigil, EnsureWaypointGraphicsLoaded
#include "oracool/stairless.h"     // OpenFloorNear - the landing by the cave mouth
#include "oracool/waypoint_menu.h" // FirstWoodsWaypoint
#include "oracool/woods_caves.h"   // the caves under the floors
#include "lighting.h"
#include "options.h"
#include "oracool/colour_light.h"
#include "oracool/scene_fx.h" // ExplorationMemoryLight - the forest past the map's edge
#include "oracool/colour_light.h" // TownIsDark - the wisps and the ring brighter by night
#include "oracool/splats.h"       // the wisps' glow handed to the graphics card with the other lights
#include "objects.h"              // the vanilla objects along the roads
#include "oracool/missile_tint.h" // LitPaletteTable - the area objects' hover outline
#include "oracool/rift_bosses.h"  // the rift roster rows the area bosses wear
#include "engine/backbuffer_state.hpp" // RedrawComponent - the fairy ring's mana
#include "oracool/rage.h"                // UsesRage - the Barbarian has no mana for the ring to speed
#include "player.h"
#include "plrmsg.h"
#include "quests.h"
#include "utils/log.hpp"
#include "utils/png.h"

namespace devilution::oracool {

uint16_t WoodsLookPiece[MAXDUNX][MAXDUNY];
uint16_t WoodsShell[MAXDUNX][MAXDUNY];
int8_t WoodsShellOwner[MAXDUNX][MAXDUNY];
uint8_t WoodsShellState[2] = { 2, 2 };

namespace {

// ---------------------------------------------------------------- the per-game state

/** @brief An area's floors: four each (the woods', the desert's). */
constexpr int WoodsFloors = SL_LAST_WOODS - SL_FIRST_WOODS + 1;
static_assert(SL_LAST_DESERT - SL_FIRST_DESERT + 1 == WoodsFloors, "the desert has as many floors as the woods");
static_assert(SL_LAST_FROST - SL_FIRST_FROST + 1 == WoodsFloors, "the Frost Caves have as many floors as the woods");
constexpr int OrclAreaCount = 3;

/** @brief One game's Orcl Act: each area's floors' seeds, rolled on their first visit and kept for the rest of the game (D4). */
struct WoodsGame {
	std::array<std::array<uint32_t, WoodsFloors>, OrclAreaCount> seed {};
	/** @brief Each floor's rung (WoodsBandFloor), 0 until it is first entered. */
	std::array<std::array<uint8_t, WoodsFloors>, OrclAreaCount> rung {};
};
WoodsGame Game;

int AreaIndex(OrclArea area)
{
	return area == OrclArea::Frost ? 2 : area == OrclArea::Desert ? 1 : 0;
}

/** @brief @p level's floor within its area, 0-3 (a cave's is its own area's first). */
int FloorIndex(_setlevels level)
{
	const int first = IsFrostLevel(level) ? static_cast<int>(SL_FIRST_FROST) : IsDesertLevel(level) ? static_cast<int>(SL_FIRST_DESERT) : static_cast<int>(SL_FIRST_WOODS);
	return std::clamp(static_cast<int>(level) - first, 0, WoodsFloors - 1);
}

/** @brief The floor last built (or being built) is the desert's: its generator, its art, its rules. */
bool Desert = false;
/** @brief The floor last built (or being built) is the Frost Caves': underground, its own caverns and art (2026-10-17). */
bool Frost = false;
/** @brief The floor is drawn from its own painted art alone, never Tristram's pieces: the desert's and the Frost Caves'. */
bool OwnLook()
{
	return Desert || Frost;
}
/** @brief The floor being generated, 0-3 within its area. */
int GenFloor = 0;

// ---------------------------------------------------------------- the map of the floor last built

/**
 * @brief The ground materials, in the order of the textures. On the desert the first five are its own (RfA-65) in the woods' roles -
 * Grass the sand, GrassDark the dune ripples, GrassDry the cracked clay, Path the salt road, Forest the bedrock - and four more are
 * the desert's alone (RfA-68): the oasis's grass, mud and reeds, and the buried ruins' paving.
 */
enum Material : uint8_t {
	Grass,
	GrassDark,
	GrassDry,
	Path,
	Forest,
	OasisGrass,
	OasisMud,
	Reeds,
	Paving,
	MaterialCount,
};

/**
 * @brief What stands on a tile and blocks it: the woods' trees (RfA-63), the desert's rocks, palms and bones in their place (RfA-66,
 * 67, 68), and a Blocker - a tile under a wide prop's footprint or the oasis pool, solid and never drawn.
 */
enum TreeKind : uint8_t {
	NoTree,
	OakLarge,
	OakSmall,
	PineTall,
	PineSmall,
	DeadLarge,
	DeadSmall,
	Hoodoo,
	BoulderD,
	Outcrop,
	PalmDead,
	PalmBroken,
	Acacia,
	Petrified,
	Ribcage,
	RocksD,
	Thicket,
	SkullGiant,
	Stakes,
	Totem,
	PalmOasis,
	// The Salt Road's (RfA-69): the travellers who never arrived.
	Wagon,
	CargoSpill,
	TarpPoles,
	Milestone,
	WayShrine,
	Gallows,
	WellStone,
	// The buried city's stones (RfA-70): the Glass Flats' ruins and the Tomb of the Sun-Eater's.
	ColumnBroken,
	ColumnFallen,
	StatueHead,
	Obelisk,
	SteleEclipse,
	ArchRuined,
	StatueHeadless,
	Brazier,
	// More of it (RfA-70b): the guardian, the aqueduct and the city's lesser stones.
	GuardianLion,
	AqueductArch,
	BurialUrns,
	SundialPlinth,
	Cairn,
	WallStub,
	WallBroken,
	// The Ash Dunes' (RfA-72): what burned here.
	StumpCharred,
	BoulderAsh,
	SiegeBurnt,
	// The Glass Flats' (RfA-72b): sand fused by fire from the sky.
	ObsidianSpire,
	GlassShards,
	Fulgurite,
	CrystalPale,
	CorpseGlassed,
	// The nomad camps' (RfA-73): what is left of them.
	TentOpen,
	TentCollapsed,
	LanternPole,
	WaterskinsRack,
	SaltPiles,
	HitchingPost,
	// The Sun-Eater's arena (RfA-74): the black pillars, the temple front, the altar; the broken wall's corner and end.
	PillarBlackA,
	PillarBlackB,
	PillarBlackBroken,
	TempleFront,
	AltarEclipse,
	WallCorner,
	WallEnd,
	// The Frost Caves' (RfA-77, 78, 78b, 80, 80b): the cave walls, the landmarks, the clutter and the war that froze here.
	FrostWallA,
	FrostWallB,
	FrostWallLow,
	IceStalagmite,
	IceStalagmites,
	IceBoulder,
	IcePillar,
	CrystalCluster,
	FrozenWaterfall,
	BeastInIce,
	RocksSnow,
	IcicleLedge,
	CrystalsSmall,
	IceSlab,
	ShrubFrozen,
	CairnSkull,
	BarbarianInIce,
	WarriorSlumped,
	BannerTorn,
	BarricadeStakes,
	CatapultBroken,
	CratesBarrels,
	SledFrozen,
	WeaponRack,
	ShieldsSpears,
	PassageOn,
	// The Glacial Hollows' (RfA-81): a hole in the lake's ice, a crevasse's lip, a fish in a block of ice, a crystal spire.
	IceHole,
	CrevasseLip,
	FishInIce,
	CrystalSpire,
	// The Frostbound Deep's (RfA-81b): the frozen dead, the northern people's graves, what was chained here.
	WallFaces,
	BurialMound,
	TombDoor,
	ChainsRock,
	CorpseKneeling,
	RuneStone,
	// The Throne of Winter's (RfA-82): its ice cathedral.
	CathedralPillar,
	CathedralPillarBroken,
	ThroneFrozen,
	ArchIce,
	// The rest of it (RfA-82b): the arena's ring stones, the frozen braziers, the old king.
	RingStoneA,
	RingStoneB,
	RingStoneC,
	BrazierFrozen,
	KingStatue,
	Blocker,
	TreeKindCount,
};

/**
 * @brief The desert's things that stand on the sand and do not block (RfA-67, 68), after the scatter sheet's twelve cells: the
 * Scatter map holds a cell + 1, so these are 13 on.
 */
enum DesertDecor : uint8_t {
	DecorSaltbush = 12,
	DecorDuneGrass,
	DecorBones,
	DecorUrn,
	DecorBasket,
	DecorWheel, // a cart wheel lying in the sand (RfA-69)
	DecorLid,   // a sarcophagus lid lying in the sand (RfA-70b)
	DecorCampfire, // the Ash Dunes' lying things (RfA-72): a burnt-out campfire, charred bones, a smoking vent
	DecorBonesCharred,
	DecorVent,
	DecorRug,   // the camps' rug with its pots (RfA-73)
	DecorReeds, // a clump of dry reeds by the water (RfA-73)
	DecorDriftRock, // the Frost Caves' lying things (RfA-78b, 80b): a drift against a rock, bones under frost, a frozen campfire
	DecorBonesFrost,
	DecorCampfireFrozen,
	DecorLakeStones, // a frozen lake's edge stones (RfA-81)
	ScatterSlots,
};

/**
 * @brief The floor's playable box in world tiles: 50 x 50 megatiles from tile 6 (user, 2026-10-14: "go with 100x100 for now";
 * it was the dungeon's 40 x 40 from 16), with trees all round it. Past the map's edge the forest is drawn mirrored
 * (DrawWoodsBeyondMap), so the six tiles left over are enough.
 */
constexpr int Lo = 6;
constexpr int Hi = 105;
/** @brief The box in megatiles (the woods' own cell grids, not the dungeon's 40 x 40 dungeon[][]). */
constexpr int WoodsCells = (Hi - Lo + 1) / 2;
static_assert(WoodsCells <= MaxPlayAreaCells && Lo % 2 == 0);

uint8_t Ground[MAXDUNX][MAXDUNY];
uint8_t Tree[MAXDUNX][MAXDUNY];
/** @brief A small thing on the ground: a scatter cell + 1 (RfA-62's sheet), 0 for none. */
uint8_t Scatter[MAXDUNX][MAXDUNY];
/** @brief How often each ground detail is laid, as the Orcl Testing Ground counted them (2026-10-14): weights. */
constexpr int ScatterWeight[12] = { 3, 3, 3, 3, 3, 3, 3, 3, 2, 4, 5, 2 };
/** @brief Each tree's small offset from its tile's centre, so the woods do not stand in rows. */
int8_t TreeJitterX[MAXDUNX][MAXDUNY];
int8_t TreeJitterY[MAXDUNX][MAXDUNY];
/**
 * @brief A tree that is there (it blocks) but is not drawn: deep in the forest, hidden behind the trees in front of it. The
 * user's play test, 2026-10-13: by the map's edge every tile was a tree and the frame rate fell; three tiles into solid forest
 * only one tree in four is drawn, and the canopies still meet.
 */
bool TreeHidden[MAXDUNX][MAXDUNY];

Point Arrival { 88, 48 };
/** @brief The glade's middle: its side path's end, the cave mouth's clearing, the fairy ring's place. */
Point GladeCentre {};
/** @brief The floor last built was built from Tristram's own tiles (the Woods Painted Art option off). */
bool BuiltVanilla = true;
/** @brief ... and its trees and ground details are the painted ones, over Tristram's ground (the Mixed look). */
bool BuiltMixed = false;
/** @brief The next woods floor built is entered from the deeper one: the hero stands at its way on, not its arrival. */
bool EnterAtWayOn = false;
/** @brief ... or from its cave: by the cave's mouth. */
bool EnterAtCaveMouth = false;

/**
 * @brief The cave mouth (oracool/woods_caves.h): Tristram's own way down to the Caves - the dark hole in the rock outcrop by the
 * river, town tiles (12..19, 66..73), copied as the town has them (OracoolPreview.DISABLED_WoodsMixedView prints the block).
 * It replaced Hellfire's Hive mound on 2026-10-14: the mound is a blighted 9 x 6 slab that read as a rectangle on the grass.
 * Rows of 8, y then x; its entrance - the tile the town's caves trigger stands on, (17, 69) - is (5, 3) into it.
 */
constexpr uint16_t MouthStamp[8][8] = {
	{ 223, 25, 230, 231, 226, 227, 219, 220 },
	{ 224, 225, 232, 233, 228, 229, 18, 19 },
	{ 219, 220, 221, 21, 1209, 1210, 28, 29 },
	{ 18, 19, 22, 23, 1211, 1212, 30, 31 },
	{ 223, 25, 1202, 1199, 1205, 1206, 217, 25 },
	{ 224, 225, 1203, 1204, 1207, 1208, 218, 27 },
	{ 230, 231, 222, 25, 230, 231, 223, 25 },
	{ 232, 233, 26, 27, 232, 233, 224, 225 },
};
constexpr int MouthWidth = 8;
constexpr int MouthHeight = 8;
constexpr Displacement MouthEntranceOffset { 5, 3 };
/**
 * @brief The fences (user, 2026-10-14: "outline levels borders with fences and maybe put a fence door to next area. we can also
 * have fenced areas with doors sporadically dispersed among the level as we have in levels 9-12"): the Caves' own fence
 * megatiles, by dungeon cell (the 40 x 40 grid of 2 x 2 tiles), 0 where there is none. Borrowed from the Caves tileset
 * (oracool/borrowed_tiles.h), each over the woods' own ground.
 */
uint8_t FenceCell[WoodsCells][WoodsCells];
/** @brief The minimap's type of each megatile (automap.cpp, WoodsAutomapCell): dungeon[][] is the vanilla 40 x 40. */
uint8_t MapCell[WoodsCells][WoodsCells];
/** @brief The Caves' fence megatiles (drlg_l3.cpp Fence): along x, along y, a post where two meet, the gates in each. */
constexpr uint8_t FenceAlongX[] = { 134, 136 };
constexpr uint8_t FenceAlongY[] = { 135, 137 };
constexpr uint8_t FencePost = 138;
constexpr uint8_t GateAlongX = 146;
constexpr uint8_t GateAlongY = 147;
/** @brief The Caves' gate leaves, closed to open (objects.cpp OperateL3Door): the woods' gates stand open. */
constexpr std::pair<uint16_t, uint16_t> GateOpen[] = { { 530, 537 }, { 533, 540 } };
/** @brief The ring's cells: one in from the dungeon's edge, so the playable box's outermost tiles are the forest beyond it. */
constexpr int RingLo = 1;
constexpr int RingHi = WoodsCells - 2;

/**
 * @brief The river (user, 2026-10-13: "grass, rivers, bridges"): Tristram's own stream, its cells as the town lays them
 * (OracoolPreview.DISABLED_WoodsMixedView traced the west river): a diagonal of two cells a row - M3 M4, then M1 M2 under them,
 * then the pair one cell east - with straight stretches of one cell a row between, and the town's wooden bridge in a straight
 * stretch where a road crosses. Each a 2 x 2 of town pieces: top, right, left, bottom.
 */
enum RiverPart : uint8_t {
	NoRiver,
	RiverM1,
	RiverM2,
	RiverM3,
	RiverM4,
	RiverStraight,
	RiverBridge,
};
constexpr uint16_t RiverPieces[7][4] = {
	{ 0, 0, 0, 0 },
	{ 277, 278, 279, 280 },
	{ 285, 286, 287, 288 },
	{ 281, 282, 283, 284 },
	{ 289, 290, 291, 292 },
	{ 261, 262, 263, 264 },
	{ 309, 310, 311, 312 }, // crossed along its bottom row (311, 312); 309 and 310 are its rail
};
uint8_t RiverCell[WoodsCells][WoodsCells];
/**
 * @brief The houses (user, 2026-10-13: "houses which can be entered and their roof fading away when in. chests inside. stone
 * walls"; then "walk-in building using wall assets from catacombs and gamedrawn roof, fade-away roof"): rooms of the Catacombs'
 * own wall megatiles (drlg_l2.cpp's patterns: 1 a wall on a cell's west edge, 2 on its north edge, 3 floor, 5 an arch in a
 * north-edge wall, 8 / 7 / 9 / 6 the north-west, north-east, south-west and south-east corners), borrowed onto the woods
 * (oracool/borrowed_tiles.h). Cells x0..x1, y0..y1 by dungeon cell, walls on the outer ring of them; the front walls' outer
 * halves stand on the woods' grass. The roof is the game's own, drawn over it (DrawWoodsRoofAfterTile).
 */
struct House {
	int x0, y0, x1, y1; // the ring of wall cells, inclusive
	int doorY;          // the arch, in the east wall (column x1)
};
constexpr int MaxHouses = 2;
std::array<House, MaxHouses> Houses;
int HouseCount = 0;
/** @brief @p tile is under a house's walls or inside them (its ring of wall cells, inclusive). */
bool InHouseFootprint(Point tile)
{
	for (int i = 0; i < HouseCount; i++) {
		const House &house = Houses[i];
		if (tile.x >= Lo + 2 * house.x0 && tile.x <= Lo + 2 * house.x1 + 1 && tile.y >= Lo + 2 * house.y0 && tile.y <= Lo + 2 * house.y1 + 1)
			return true;
	}
	return false;
}

/** @brief The fenced pens, by cell (their fence ring inclusive) and the tile in their middle - what stands in them (PlanWoodsWorld). */
struct Pen {
	int mx0, my0, x1, y1;
	Point centre;
};
constexpr int MaxPens = 3;
std::array<Pen, MaxPens> Pens;
int PenCount = 0;

/**
 * @brief What a house looks like from outside (user, 2026-10-14: "Game drawn roofs dont look good. We remove them. We do have
 * vanilla roofs in tristram houses. Can we use those?"): Tristram's own hip-roofed cottage, its town tiles (36..43, 64..69) as
 * the town lays them - roof, walls, door and the rock beside it. Drawn in place of the house's tiles while the hero is outside
 * it, see-through while he stands behind it, gone while he is inside, where the Catacombs room under it shows. Its walls stand
 * on the room's ring: the cottage's walls are 38..42 by 64..68, its door on the east face at 66..67. Rows y, then x.
 */
constexpr uint16_t CottageShell[6][8] = {
	{ 230, 231, 476, 477, 218, 218, 484, 485 },
	{ 232, 233, 478, 479, 218, 218, 486, 487 },
	{ 465, 25, 218, 218, 218, 218, 480, 481 },
	{ 466, 467, 218, 218, 218, 218, 482, 483 },
	{ 457, 458, 468, 469, 472, 473, 453, 454 },
	{ 459, 460, 470, 471, 474, 475, 455, 456 },
};
/** @brief The cottage block's place in a house's 5 x 4 cells: two tiles in from the west, two from the north. */
constexpr Displacement CottageShellOffset { 2, 2 };

/** @brief -1: the seed decides whether a floor has a river; 0 or 1: the test hook's answer. */
int RiverForced = -1;

/** @brief The floor being generated has a cave mouth (BuildWoodsLevel sets it from WoodsCaveOnFloor). */
int MouthFloor = -1;
bool MouthOnFloor = false;
Point MouthOrigin {};
Point MouthEntrance {};

/**
 * @brief The desert's oasis (user, 2026-10-16; RfA-68's ground): a pool of dark water ringed by mud, reeds and grass, drawn by the
 * game into the ground (DrawGroundAt) - its tiles a Blocker, so no one walks on the water. 0 radius: none on the floor.
 */
Point OasisCentre {};
float OasisRadius = 0.0F;
/** @brief The buried ruins' middle (their paving), or {0,0}. */
Point RuinsCentre {};
/** @brief The Tomb of the Sun-Eater's arena (RfA-74), where its boss waits by the way on; {} on every other floor. */
Point ArenaCentre {};
/** @brief The Tomb's braziers (RfA-70), lit by the game: their fire drawn with the glows, their light one of the night's lamps. */
std::vector<Point> Braziers;
/** @brief The Ash Dunes' ember-cracked boulders and smoking vents, the Glass Flats' pale crystals (RfA-72, 72b): glows and smoke. */
std::vector<Point> EmberRocks;
std::vector<Point> SmokeVents;
std::vector<Point> PaleCrystals;
/** @brief The Glass Flats' scorched craters (RfA-72b), laid into the ground as the ways down are. */
std::vector<Point> Craters;
/**
 * @brief The Frost Caves' light shafts (RfA-79/79b): a heap of fresh snow under a hole in the roof, laid into the ground as the
 * craters are, with a light of its own over it - the game draws the light; the picture is only the snow.
 */
std::vector<Point> LightShafts;
/** @brief The desert's lone sights placed so far this floor (FindDesertSpot): each keeps its room from the next (audit round 2). */
std::vector<Point> DesertSpots;
/**
 * @brief The Frost Caves' build masks (audit, 2026-10-17): the open floor the trodden way reaches before the sights go in (a sight
 * set in a cavern the way never crossed was sealed into the rock with it), the wide props' invisible footprints (the wall pass took
 * them for rock facing open ground and stood wall pieces round every one), and the tiles the wall pass made wall (the rock's depth and
 * the tomb door told wall from clutter by a range of kinds that missed one wall kind and took in a clutter one).
 */
bool FrostReach[MAXDUNX][MAXDUNY];
bool FrostReachValid = false;
bool FrostFootprint[MAXDUNX][MAXDUNY];
bool FrostWallTile[MAXDUNX][MAXDUNY];
/** @brief The nomad camps' lanterns (RfA-73 paints them cold): lit by night, a small flame and a night lamp. */
std::vector<Point> CampLanterns;

/** @brief The sand hole's radius, in tiles: the pit the hero walks to the middle of to go down (the desert's cave mouth). */
constexpr float SandHoleRadius = 1.7F;

bool InRiver(Point tile)
{
	if (tile.x < Lo || tile.y < Lo || tile.x > Hi || tile.y > Hi)
		return false;
	return RiverCell[(tile.x - Lo) / 2][(tile.y - Lo) / 2] != NoRiver;
}

bool InMouth(Point tile)
{
	if (!MouthOnFloor)
		return false;
	if (OwnLook()) // the sand hole or the hole in the ice: its pit and its lip
		return std::max(std::abs(tile.x - MouthEntrance.x), std::abs(tile.y - MouthEntrance.y)) <= 1;
	return tile.x >= MouthOrigin.x && tile.x < MouthOrigin.x + MouthWidth && tile.y >= MouthOrigin.y && tile.y < MouthOrigin.y + MouthHeight;
}

/** @brief The river's town pieces over the floor's tiles. */
void StampRiver()
{
	for (int my = 0; my < WoodsCells; my++) {
		for (int mx = 0; mx < WoodsCells; mx++) {
			const uint8_t part = RiverCell[mx][my];
			if (part == NoRiver)
				continue;
			for (int t = 0; t < 4; t++) {
				const int x = Lo + 2 * mx + (t & 1);
				const int y = Lo + 2 * my + (t >> 1);
				dPiece[x][y] = RiverPieces[part][t];
				WoodsLookPiece[x][y] = 0;
			}
		}
	}
}

/**
 * @brief Which of the outcrop's tiles carry its pieces: its rocks and its hole. Its flat ground - Tristram's grass and dark earth,
 * a square slab on the painted woods (the seam survey, 2026-10-14) - is left to the woods' own ground in the painted look.
 */
bool MouthTileStamped[MouthHeight][MouthWidth] {};

bool MouthPieceIsFlat(uint16_t piece)
{
	for (size_t i = 2; i < std::size(DPieceMicros[piece].mt); i++)
		if (LevelCelBlock { DPieceMicros[piece].mt[i] }.hasValue())
			return false;
	return !TileHasAny(piece, TileProperties::Solid);
}

/** @brief The outcrop's town pieces over the floor's tiles: all of them in the Tristram looks, its rocks and its hole in the painted. */
void StampCaveMouth()
{
	if (!MouthOnFloor || OwnLook()) // the sand hole is the desert's ground, drawn by the game, not Tristram's pieces (the Frost Caves' too)
		return;
	for (int dy = 0; dy < MouthHeight; dy++) {
		for (int dx = 0; dx < MouthWidth; dx++) {
			const Point tile = MouthOrigin + Displacement { dx, dy };
			const uint16_t piece = MouthStamp[dy][dx];
			const bool nearHole = std::abs(dx - MouthEntranceOffset.deltaX) <= 1 && std::abs(dy - MouthEntranceOffset.deltaY) <= 1;
			MouthTileStamped[dy][dx] = BuiltVanilla || nearHole || !MouthPieceIsFlat(piece);
			if (!MouthTileStamped[dy][dx])
				continue;
			dPiece[tile.x][tile.y] = piece;
			WoodsLookPiece[tile.x][tile.y] = 0;
		}
	}
}

/** @brief @p tile is one of the outcrop's stamped tiles (drawn from its cels). */
bool InMouthStamp(Point tile)
{
	if (OwnLook() || !InMouth(tile))
		return false;
	return MouthTileStamped[tile.y - MouthOrigin.y][tile.x - MouthOrigin.x];
}
Point WayOn { 22, 62 };

// ---------------------------------------------------------------- deterministic noise, independent of the game's stream

uint32_t Hash(uint32_t a, uint32_t b, uint32_t c)
{
	uint32_t h = a * 0x9E3779B1U ^ (b + 0x7F4A7C15U) * 0x85EBCA77U ^ (c + 0x165667B1U) * 0xC2B2AE3DU;
	h ^= h >> 15;
	h *= 0x2C1B3C6DU;
	h ^= h >> 12;
	h *= 0x297A2D39U;
	h ^= h >> 15;
	return h;
}

float Unit(uint32_t h)
{
	return static_cast<float>(h & 0xFFFFFF) / 16777216.0F;
}

/** @brief Smooth value noise over world tiles, 0..1, features about @p scale tiles across. */
float ValueNoise(int x, int y, float scale, uint32_t seed)
{
	const float fx = static_cast<float>(x) / scale;
	const float fy = static_cast<float>(y) / scale;
	const int ix = static_cast<int>(std::floor(fx));
	const int iy = static_cast<int>(std::floor(fy));
	const float tx = fx - static_cast<float>(ix);
	const float ty = fy - static_cast<float>(iy);
	const auto at = [&](int cx, int cy) { return Unit(Hash(static_cast<uint32_t>(cx), static_cast<uint32_t>(cy), seed)); };
	const float sx = tx * tx * (3 - 2 * tx);
	const float sy = ty * ty * (3 - 2 * ty);
	const float top = at(ix, iy) + (at(ix + 1, iy) - at(ix, iy)) * sx;
	const float bottom = at(ix, iy + 1) + (at(ix + 1, iy + 1) - at(ix, iy + 1)) * sx;
	return top + (bottom - top) * sy;
}

bool Inside(Point p)
{
	return p.x >= Lo && p.y >= Lo && p.x <= Hi && p.y <= Hi;
}

// ---------------------------------------------------------------- the generator

/** @brief A piece the town tileset makes solid: every tree tile and everything outside the box is it. Never drawn. */
uint16_t SolidPiece = 0;
/** @brief The four plain grass megatiles of the town tileset (DrlgTPass3 fills the closed Hell entrance with them). */
constexpr int GrassMegatiles = 4;

void FindSolidPiece()
{
	// Solid and missile-blocking but letting light through if the tileset has one, so a trunk does not cast a tile-wide
	// black shadow; any solid piece otherwise.
	uint16_t anySolid = 0;
	for (int i = 1; i < MAXTILES; i++) {
		if (!TileHasAny(i, TileProperties::Solid))
			continue;
		if (anySolid == 0)
			anySolid = static_cast<uint16_t>(i);
		if (TileHasAny(i, TileProperties::BlockMissile) && !TileHasAny(i, TileProperties::BlockLight)) {
			SolidPiece = static_cast<uint16_t>(i);
			return;
		}
	}
	SolidPiece = anySolid;
}

void FillMegatile(int x, int y, int megatile)
{
	const MegaTile mega = pMegaTiles[megatile - 1];
	dPiece[x + 0][y + 0] = SDL_SwapLE16(mega.micro1);
	dPiece[x + 1][y + 0] = SDL_SwapLE16(mega.micro2);
	dPiece[x + 0][y + 1] = SDL_SwapLE16(mega.micro3);
	dPiece[x + 1][y + 1] = SDL_SwapLE16(mega.micro4);
}

/** @brief Clears a round patch of trees, its ground set to @p material where it is not already a path. */
void Clearing(Point centre, int radius, Material material)
{
	for (int dy = -radius; dy <= radius; dy++) {
		for (int dx = -radius; dx <= radius; dx++) {
			const Point p = centre + Displacement { dx, dy };
			if (!Inside(p) || dx * dx + dy * dy > radius * radius)
				continue;
			Tree[p.x][p.y] = NoTree;
			if (Ground[p.x][p.y] != Path)
				Ground[p.x][p.y] = material;
		}
	}
}

/** @brief A wandering path from @p from to @p to, @p width tiles wide, cleared of trees. */
void LayPath(Point from, Point to, int width, uint32_t seed)
{
	float x = static_cast<float>(from.x);
	float y = static_cast<float>(from.y);
	const float tx = static_cast<float>(to.x);
	const float ty = static_cast<float>(to.y);
	for (int step = 0; step < 600; step++) {
		const float dx = tx - x;
		const float dy = ty - y;
		const float dist = std::sqrt(dx * dx + dy * dy);
		if (dist < 1.0F)
			break;
		// Toward the goal, bent by the noise so the path winds.
		const float bend = (ValueNoise(static_cast<int>(x), static_cast<int>(y), 7.0F, seed) - 0.5F) * 2.2F;
		const float ax = dx / dist - dy / dist * bend;
		const float ay = dy / dist + dx / dist * bend;
		const float al = std::sqrt(ax * ax + ay * ay);
		x += ax / al * 0.7F;
		y += ay / al * 0.7F;
		const Point here { static_cast<int>(std::lround(x)), static_cast<int>(std::lround(y)) };
		for (int oy = -width / 2; oy <= width / 2; oy++) {
			for (int ox = -width / 2; ox <= width / 2; ox++) {
				const Point p = here + Displacement { ox, oy };
				if (!Inside(p))
					continue;
				Ground[p.x][p.y] = Path;
				Tree[p.x][p.y] = NoTree;
			}
		}
		// Trees step back a tile beside the path.
		for (int oy = -width / 2 - 1; oy <= width / 2 + 1; oy++) {
			for (int ox = -width / 2 - 1; ox <= width / 2 + 1; ox++) {
				const Point p = here + Displacement { ox, oy };
				if (Inside(p))
					Tree[p.x][p.y] = NoTree;
			}
		}
	}
}

TreeKind PickTree(Material ground, uint32_t h)
{
	(void)ground;
	// The mix the user set on the Orcl Testing Ground (2026-10-14): large oak 10, tall pine 12, small oak 12, small pine 2, large
	// dead 12, small dead 11 - in the groves and in the open alike.
	const float u = Unit(h);
	return u < 0.169F ? OakLarge : u < 0.373F ? PineTall : u < 0.576F ? OakSmall : u < 0.610F ? PineSmall : u < 0.814F ? DeadLarge : DeadSmall;
}

/** @brief Every walkable tile reachable from the arrival; the rest of the open ground becomes thicket. */
void SealUnreachable()
{
	static bool seen[MAXDUNX][MAXDUNY];
	std::memset(seen, 0, sizeof(seen));
	std::vector<Point> stack { Arrival };
	seen[Arrival.x][Arrival.y] = true;
	while (!stack.empty()) {
		const Point p = stack.back();
		stack.pop_back();
		for (const Displacement d : { Displacement { 1, 0 }, Displacement { -1, 0 }, Displacement { 0, 1 }, Displacement { 0, -1 } }) {
			const Point n = p + d;
			if (!Inside(n) || seen[n.x][n.y] || Tree[n.x][n.y] != NoTree)
				continue;
			seen[n.x][n.y] = true;
			stack.push_back(n);
		}
	}
	for (int y = Lo; y <= Hi; y++) {
		for (int x = Lo; x <= Hi; x++) {
			if (Tree[x][y] == NoTree && !seen[x][y])
				Tree[x][y] = Frost ? Blocker : Desert ? RocksD : PineSmall;
		}
	}
}

/** @brief A dungeon cell's four tiles cleared for a fence: no tree, no ground detail. */
void ClearCell(int mx, int my)
{
	for (int t = 0; t < 4; t++) {
		const int x = Lo + 2 * mx + (t & 1);
		const int y = Lo + 2 * my + (t >> 1);
		Tree[x][y] = NoTree;
		Scatter[x][y] = 0;
	}
}

bool CellHasPath(int mx, int my)
{
	for (int t = 0; t < 4; t++) {
		if (Ground[Lo + 2 * mx + (t & 1)][Lo + 2 * my + (t >> 1)] == Path)
			return true;
	}
	return false;
}

bool CellOpen(int mx, int my)
{
	for (int dy = -1; dy <= 1; dy++)
		for (int dx = -1; dx <= 1; dx++)
			if (mx + dx >= 0 && my + dy >= 0 && mx + dx < WoodsCells && my + dy < WoodsCells && RiverCell[mx + dx][my + dy] != NoRiver)
				return false;
	for (int t = 0; t < 4; t++) {
		const int x = Lo + 2 * mx + (t & 1);
		const int y = Lo + 2 * my + (t >> 1);
		if (Tree[x][y] != NoTree || Ground[x][y] == Path || Ground[x][y] == Forest)
			return false;
	}
	return true;
}

/** @brief A road on any of the cell's four tiles. */
bool PathInCell(int mx, int my)
{
	if (mx < 0 || my < 0 || mx >= WoodsCells || my >= WoodsCells)
		return false;
	return CellHasPath(mx, my);
}

/**
 * @brief The river from the floor's top edge to its bottom (cell rows 0-39), drifting east as it goes: two diagonal rows,
 * then straight stretches. A straight stretch wherever the next diagonal cells hold a road, bridged where the road crosses.
 * Kept clear of both roads' ends, the glade's cave mouth and the ring's corners; tried again from another column when it
 * cannot be, and left out after 24 tries. Trees and ground details cleared off it and its banks.
 */
void PlanRiver(uint32_t seed)
{
	std::memset(RiverCell, 0, sizeof(RiverCell));
	const bool wanted = RiverForced >= 0 ? RiverForced != 0 : Hash(seed, 0x51BE, 3) % 10 < 7;
	if (!wanted)
		return;
	const int arrivalCell = (Arrival.x - Lo) / 2;
	const int wayOnCell = (WayOn.x - Lo) / 2;
	bool accepted = false;
	for (int attempt = 0; attempt < 24; attempt++) {
		std::memset(RiverCell, 0, sizeof(RiverCell));
		int c = wayOnCell + 5 + static_cast<int>(Hash(seed, static_cast<uint32_t>(attempt), 0x51BE) % 8U);
		bool ok = true;
		bool diagonalNext = true;
		int j = 0;
		while (j < WoodsCells && ok) {
			if (diagonalNext) {
				if (PathInCell(c, j) || PathInCell(c + 1, j) || PathInCell(c, j + 1) || PathInCell(c + 1, j + 1)) {
					ok = false; // a road on a diagonal: another column
					break;
				}
				RiverCell[c][j] = RiverM3;
				RiverCell[c + 1][j] = RiverM4;
				if (j + 1 < WoodsCells) {
					RiverCell[c][j + 1] = RiverM1;
					RiverCell[c + 1][j + 1] = RiverM2;
				}
				j += 2;
				diagonalNext = false;
				continue;
			}
			// After a diagonal: straight down column c + 1, or the next diagonal one cell east.
			const bool roadAhead = PathInCell(c + 1, j) || PathInCell(c + 2, j) || PathInCell(c + 1, j + 1) || PathInCell(c + 2, j + 1);
			const bool straight = roadAhead || Unit(Hash(seed ^ 0x57A1U, static_cast<uint32_t>(j), static_cast<uint32_t>(attempt))) < 0.45F;
			if (straight) {
				RiverCell[c + 1][j] = PathInCell(c + 1, j) ? RiverBridge : RiverStraight;
				j++;
			} else {
				c++;
				diagonalNext = true;
			}
			ok = c + 2 < arrivalCell - 3;
		}
		if (ok) {
			// Off the mouth (and a cell round it), never through a road's end.
			for (int my = 0; my < WoodsCells && ok; my++) {
				for (int mx = 0; mx < WoodsCells && ok; mx++) {
					if (RiverCell[mx][my] == NoRiver)
						continue;
					const Point centre { Lo + 2 * mx + 1, Lo + 2 * my + 1 };
					if (MouthOnFloor && centre.x >= MouthOrigin.x - 3 && centre.x < MouthOrigin.x + MouthWidth + 3 && centre.y >= MouthOrigin.y - 3
					    && centre.y < MouthOrigin.y + MouthHeight + 3)
						ok = false;
					if (centre.WalkingDistance(Arrival) < 8 || centre.WalkingDistance(WayOn) < 8)
						ok = false;
				}
			}
		}
		if (ok) {
			accepted = true;
			break;
		}
	}
	if (!accepted) {
		std::memset(RiverCell, 0, sizeof(RiverCell)); // no try fitted: this floor has no river
		return;
	}
	// Its banks: a tile either side of each river cell kept open.
	for (int my = 0; my < WoodsCells; my++) {
		for (int mx = 0; mx < WoodsCells; mx++) {
			if (RiverCell[mx][my] == NoRiver)
				continue;
			for (int y = Lo + 2 * my; y < Lo + 2 + 2 * my; y++) {
				for (int x = Lo - 1 + 2 * mx; x < Lo + 3 + 2 * mx; x++) {
					if (InDungeonBounds({ x, y })) {
						Tree[x][y] = NoTree;
						Scatter[x][y] = 0;
					}
				}
			}
		}
	}
}

/** @brief Ground a house can be built on: no road, grove or river on or beside it; trees may stand there, they are cleared. */
bool CellBuildable(int mx, int my)
{
	if (mx < 0 || my < 0 || mx >= WoodsCells || my >= WoodsCells)
		return false;
	for (int dy = -1; dy <= 1; dy++)
		for (int dx = -1; dx <= 1; dx++)
			if (mx + dx >= 0 && my + dy >= 0 && mx + dx < WoodsCells && my + dy < WoodsCells && RiverCell[mx + dx][my + dy] != NoRiver)
				return false;
	for (int t = 0; t < 4; t++) {
		const int x = Lo + 2 * mx + (t & 1);
		const int y = Lo + 2 * my + (t >> 1);
		if (Ground[x][y] == Path || Ground[x][y] == Forest)
			return false;
	}
	return true;
}

/**
 * @brief One or two houses on open ground, away from the roads, the glade, the mouth, the waypoint and the pens: four or five
 * cells by four, an arch in the south wall, open ground in front of it.
 */
void PlanHouses(uint32_t seed)
{
	HouseCount = 0;
	const Point keepAway[] = { Arrival, WayOn, MouthOnFloor ? MouthEntrance : Arrival, Arrival + Displacement { -2, -2 } };
	const int want = 1 + static_cast<int>(Hash(seed, 9, 9) % 2);
	// Every position, from a seeded start: random spots seldom missed both roads, the groves and the river at once.
	constexpr int W = 5; // the cottage's block, in cells
	constexpr int D = 4;
	const int positions = (WoodsCells - 12) * (WoodsCells - 12);
	const int start = static_cast<int>(Hash(seed, 0x40E, 1) % static_cast<uint32_t>(positions));
	for (int k = 0; k < positions && HouseCount < want; k++) {
		const int at = (start + k * 7919) % positions; // a stride prime to the count: every position once, scattered
		const int bx = 4 + at % (WoodsCells - 12);
		const int by = 4 + at / (WoodsCells - 12);
		if (bx + W + 2 >= WoodsCells - 3 || by + D + 1 >= WoodsCells - 3)
			continue;
		bool fits = true;
		// The block, a cell round it, and one more in front of the east door: no road, grove, river or fence there.
		for (int my = by - 1; my <= by + D && fits; my++)
			for (int mx = bx - 1; mx <= bx + W + 1 && fits; mx++)
				fits = CellBuildable(mx, my) && FenceCell[mx][my] == 0;
		const Point centre { Lo + 2 * bx + W, Lo + 2 * by + D };
		for (const Point p : keepAway)
			fits = fits && centre.WalkingDistance(p) > 10;
		if (MouthOnFloor)
			fits = fits && std::abs(centre.x - (MouthOrigin.x + MouthWidth / 2)) + std::abs(centre.y - (MouthOrigin.y + MouthHeight / 2)) > 14;
		for (int i = 0; i < HouseCount && fits; i++)
			fits = std::abs(Houses[i].x0 - bx) > W + 3 || std::abs(Houses[i].y0 - by) > D + 3;
		if (!fits)
			continue;
		// The ring under the cottage's walls (38..42 by 64..68 of its block 34..43 by 62..69): cells 2..4 by 1..3 of the block.
		Houses[HouseCount++] = { bx + 2, by + 1, bx + 4, by + 3, by + 2 };
		for (int my = by - 1; my <= by + D; my++)
			for (int mx = bx - 1; mx <= bx + W + 1; mx++)
				if (mx >= 0 && my >= 0 && mx < WoodsCells && my < WoodsCells)
					ClearCell(mx, my);
	}
}

/** @brief The Catacombs megatile of a house's cell, or 0 off it. @p outside is set for the front walls' cells. */
int HouseMegatile(const House &house, int mx, int my, bool &outside)
{
	outside = false;
	if (mx < house.x0 || mx > house.x1 || my < house.y0 || my > house.y1)
		return 0;
	const bool west = mx == house.x0;
	const bool east = mx == house.x1;
	const bool north = my == house.y0;
	const bool south = my == house.y1;
	outside = east || south;
	if (north && west)
		return 8;
	if (north && east)
		return 7;
	if (south && west)
		return 9;
	if (south && east)
		return 6;
	if (east)
		return my == house.doorY ? 4 : 1; // the arch where the cottage's door is
	if (west)
		return 1;
	if (north || south)
		return 2;
	return 3;
}

/**
 * @brief The fence ring round the floor with a gate where each road crosses it, and two or three fenced pens with a gate each
 * on open grass, away from the roads, the glade, the cave mouth and the waypoint.
 */
void PlanFences(uint32_t seed)
{
	std::memset(FenceCell, 0, sizeof(FenceCell));
	const auto pick = [seed](const uint8_t(&from)[2], int mx, int my) { return from[Hash(static_cast<uint32_t>(mx), static_cast<uint32_t>(my), seed ^ 0xFE7CU) & 1]; };
	for (int m = RingLo; m <= RingHi; m++) {
		FenceCell[m][RingLo] = pick(FenceAlongX, m, RingLo);
		FenceCell[m][RingHi] = pick(FenceAlongX, m, RingHi);
		FenceCell[RingLo][m] = pick(FenceAlongY, RingLo, m);
		FenceCell[RingHi][m] = pick(FenceAlongY, RingHi, m);
	}
	for (const int a : { RingLo, RingHi })
		for (const int b : { RingLo, RingHi })
			FenceCell[a][b] = FencePost;
	// Where a road crosses the ring: the gate on the road's middle cell, the ring open where the road's other tiles run.
	const auto gateAt = [](int mx, int my, bool alongY) {
		for (int d = -2; d <= 2; d++) {
			const int cx = alongY ? mx : mx + d;
			const int cy = alongY ? my + d : my;
			if (cx < 0 || cy < 0 || cx >= WoodsCells || cy >= WoodsCells)
				continue;
			if (d != 0 && CellHasPath(cx, cy))
				FenceCell[cx][cy] = 0;
		}
		FenceCell[mx][my] = alongY ? GateAlongY : GateAlongX;
	};
	gateAt(RingHi, (Arrival.y - Lo) / 2, true);
	gateAt(RingLo, (WayOn.y - Lo) / 2, true);
	for (int my = 0; my < WoodsCells; my++)
		for (int mx = 0; mx < WoodsCells; mx++) {
			if (RiverCell[mx][my] != NoRiver)
				FenceCell[mx][my] = 0; // the river runs under the ring
			if (FenceCell[mx][my] != 0)
				ClearCell(mx, my);
		}

	// The pens.
	const Point keepAway[] = { Arrival, WayOn, MouthOnFloor ? MouthEntrance : Arrival, Arrival + Displacement { -2, -2 } };
	int pens = 0;
	PenCount = 0;
	const int want = 2 + static_cast<int>(Hash(seed, 7, 7) % 2);
	for (int attempt = 0; attempt < 60 && pens < want; attempt++) {
		const uint32_t h = Hash(seed, static_cast<uint32_t>(attempt), 0xBE2U);
		const int w = 3 + static_cast<int>(h % 2);
		const int d = 3 + static_cast<int>((h >> 3) % 2);
		const int mx0 = 4 + static_cast<int>((h >> 6) % static_cast<uint32_t>(WoodsCells - 8 - w));
		const int my0 = 4 + static_cast<int>((h >> 14) % static_cast<uint32_t>(WoodsCells - 8 - d));
		bool fits = true;
		for (int my = my0 - 1; my <= my0 + d && fits; my++)
			for (int mx = mx0 - 1; mx <= mx0 + w && fits; mx++)
				fits = CellOpen(mx, my) && FenceCell[mx][my] == 0;
		const Point centre { Lo + 2 * mx0 + w, Lo + 2 * my0 + d };
		for (const Point p : keepAway)
			fits = fits && centre.WalkingDistance(p) > 12;
		if (MouthOnFloor)
			fits = fits && std::abs(centre.x - (MouthOrigin.x + MouthWidth / 2)) + std::abs(centre.y - (MouthOrigin.y + MouthHeight / 2)) > 16;
		if (!fits)
			continue;
		const int x1 = mx0 + w - 1;
		const int y1 = my0 + d - 1;
		for (int mx = mx0; mx <= x1; mx++) {
			FenceCell[mx][my0] = pick(FenceAlongX, mx, my0);
			FenceCell[mx][y1] = pick(FenceAlongX, mx, y1);
		}
		for (int my = my0; my <= y1; my++) {
			FenceCell[mx0][my] = pick(FenceAlongY, mx0, my);
			FenceCell[x1][my] = pick(FenceAlongY, x1, my);
		}
		for (const int a : { mx0, x1 })
			for (const int b : { my0, y1 })
				FenceCell[a][b] = FencePost;
		// One gate, mid-side, on a side picked by the seed.
		switch ((h >> 20) % 4) {
		case 0: FenceCell[mx0 + w / 2][my0] = GateAlongX; break;
		case 1: FenceCell[mx0 + w / 2][y1] = GateAlongX; break;
		case 2: FenceCell[mx0][my0 + d / 2] = GateAlongY; break;
		default: FenceCell[x1][my0 + d / 2] = GateAlongY; break;
		}
		for (int my = my0; my <= y1; my++)
			for (int mx = mx0; mx <= x1; mx++)
				ClearCell(mx, my);
		if (PenCount < MaxPens)
			Pens[PenCount++] = { mx0, my0, x1, y1, centre };
		pens++;
	}
	PlanHouses(seed);
}

/** @brief The planned houses laid over the built floor: each cell the Catacombs' pieces, grass under the front walls' outer halves. */
void LayHouses()
{
	std::memset(WoodsShell, 0, sizeof(WoodsShell));
	std::memset(WoodsShellOwner, -1, sizeof(WoodsShellOwner));
	for (int i = 0; i < HouseCount; i++) {
		const House &house = Houses[i];
		for (int my = house.y0; my <= house.y1; my++) {
			for (int mx = house.x0; mx <= house.x1; mx++) {
				bool outside = false;
				const int mega = HouseMegatile(house, mx, my, outside);
				uint16_t pieces[4];
				if (mega == 0 || !BorrowedMegatilePieces(DTYPE_CATACOMBS, mega, pieces))
					continue;
				for (int t = 0; t < 4; t++) {
					const int x = Lo + 2 * mx + (t & 1);
					const int y = Lo + 2 * my + (t >> 1);
					const uint16_t id = BorrowPiece(DTYPE_CATACOMBS, pieces[t]);
					if (id == 0)
						continue;
					// A front wall's open half is outside the house: the woods' ground. Inside, the Catacombs' stone floor.
					if (outside && !TileHasAny(id, TileProperties::Solid) && WoodsLookPiece[x][y] == 0)
						WoodsLookPiece[x][y] = dPiece[x][y];
					else if (!outside)
						WoodsLookPiece[x][y] = 0;
					dPiece[x][y] = id;
				}
			}
		}
		// The cottage over it: its block from two tiles west and north of the ring's west and north walls.
		const Point origin { Lo + 2 * (house.x0 - 2) + CottageShellOffset.deltaX, Lo + 2 * (house.y0 - 1) + CottageShellOffset.deltaY };
		// A worn yard round it (user, 2026-10-14: "sharp borders between assets and terain"): dry, trodden grass over the block and
		// a ragged ring past it, so the walls stand in their own ground and not on fresh green.
		for (int ry = -2; ry < 8; ry++) {
			for (int rx = -2; rx < 10; rx++) {
				const Point tile = origin + Displacement { rx, ry };
				if (!InDungeonBounds(tile) || !Inside(tile) || (Ground[tile.x][tile.y] != Grass && Ground[tile.x][tile.y] != GrassDark))
					continue;
				const int out = std::max({ -rx, -ry, rx - 7, ry - 5, 0 });
				const uint32_t h = Hash(static_cast<uint32_t>(tile.x), static_cast<uint32_t>(tile.y), 0xEA2DU);
				if (out == 0 || (out == 1 && h % 100 < 70) || (out == 2 && h % 100 < 30))
					Ground[tile.x][tile.y] = GrassDry;
			}
		}
		for (int ry = 0; ry < 6; ry++) {
			for (int rx = 0; rx < 8; rx++) {
				const Point tile = origin + Displacement { rx, ry };
				if (!InDungeonBounds(tile))
					continue;
				WoodsShell[tile.x][tile.y] = CottageShell[ry][rx];
				WoodsShellOwner[tile.x][tile.y] = static_cast<int8_t>(i);
			}
		}
	}
}

/**
 * @brief The planned fences laid over the built floor: each tile the Caves' fence piece for walking and its upright, the woods'
 * own ground for its floor (WoodsLookPiece), a gate's leaf swapped for the open one.
 */
void LayFences()
{
	for (int my = 0; my < WoodsCells; my++) {
		for (int mx = 0; mx < WoodsCells; mx++) {
			const uint8_t mega = FenceCell[mx][my];
			uint16_t pieces[4];
			if (mega == 0 || !BorrowedMegatilePieces(DTYPE_CAVES, mega, pieces))
				continue;
			for (int t = 0; t < 4; t++) {
				const int x = Lo + 2 * mx + (t & 1);
				const int y = Lo + 2 * my + (t >> 1);
				uint16_t native = pieces[t];
				for (const auto &[closed, open] : GateOpen) {
					if (native == closed)
						native = open;
				}
				const uint16_t id = BorrowPiece(DTYPE_CAVES, native);
				if (id == 0)
					continue;
				if (WoodsLookPiece[x][y] == 0)
					WoodsLookPiece[x][y] = dPiece[x][y]; // the ground under the fence
				// Worn ground along it, so its posts' dark earth stands in its own (the sharp borders, 2026-10-14): the fence's
				// tiles dry grass, half of those beside them too.
				for (int dy = -1; dy <= 1; dy++) {
					for (int dx = -1; dx <= 1; dx++) {
						const Point p { x + dx, y + dy };
						if (!InDungeonBounds(p) || (Ground[p.x][p.y] != Grass && Ground[p.x][p.y] != GrassDark))
							continue;
						if ((dx == 0 && dy == 0) || Hash(static_cast<uint32_t>(p.x), static_cast<uint32_t>(p.y), 0xFE2CU) % 2 == 0)
							Ground[p.x][p.y] = GrassDry;
					}
				}
				dPiece[x][y] = id;
			}
		}
	}
}

/** @brief Fills TreeHidden: a tree three tiles or more from any open tile keeps only the 2 x 2 lattice's one tree in four. */
void HideDeepForest()
{
	// Distance in tiles (8-neighbour) to the nearest tile without a tree, by two sweeps.
	static uint8_t depth[MAXDUNX][MAXDUNY];
	for (int y = 0; y < MAXDUNY; y++)
		for (int x = 0; x < MAXDUNX; x++)
			depth[x][y] = Tree[x][y] == NoTree ? 0 : 255;
	for (int y = 0; y < MAXDUNY; y++)
		for (int x = 0; x < MAXDUNX; x++)
			for (const Displacement d : { Displacement { -1, 0 }, Displacement { 0, -1 }, Displacement { -1, -1 }, Displacement { 1, -1 } }) {
				const Point n = Point { x, y } + d;
				if (InDungeonBounds(n))
					depth[x][y] = static_cast<uint8_t>(std::min<int>(depth[x][y], depth[n.x][n.y] + 1));
			}
	for (int y = MAXDUNY - 1; y >= 0; y--)
		for (int x = MAXDUNX - 1; x >= 0; x--)
			for (const Displacement d : { Displacement { 1, 0 }, Displacement { 0, 1 }, Displacement { 1, 1 }, Displacement { -1, 1 } }) {
				const Point n = Point { x, y } + d;
				if (InDungeonBounds(n))
					depth[x][y] = static_cast<uint8_t>(std::min<int>(depth[x][y], depth[n.x][n.y] + 1));
			}
	for (int y = 0; y < MAXDUNY; y++)
		for (int x = 0; x < MAXDUNX; x++)
			// Three tiles in, one tree in four (the 2 x 2 lattice); seven in, one in nine (the 3 x 3). At the Testing Ground's 29-35 %
			// (v1.201.019) the canopies still meet; one in two drew hundreds of them by any edge (the play test of 2026-10-14).
			TreeHidden[x][y] = (depth[x][y] >= 7 && (x % 3 != 0 || y % 3 != 0)) || (depth[x][y] >= 3 && ((x | y) & 1) != 0);
}

// ---------------------------------------------------------------- the desert's generator

/** @brief The desert's edge (its forest): rock walls - pillars, outcrops, boulders and rock clusters. */
TreeKind PickDesertEdge(uint32_t h)
{
	const float u = Unit(h);
	return u < 0.30F ? Hoodoo : u < 0.52F ? Outcrop : u < 0.78F ? BoulderD : RocksD;
}

/** @brief What stands on the bedrock patches. */
TreeKind PickDesertRock(uint32_t h)
{
	const float u = Unit(h);
	return u < 0.38F ? RocksD : u < 0.68F ? BoulderD : u < 0.88F ? Hoodoo : Outcrop;
}

/**
 * @brief A lone thing in the open sand, by the floor: dead palms and acacias along the Salt Road and the Ash Dunes, petrified trees
 * on the Glass Flats, thorn and rock round the Sun-Eater's tomb.
 */
TreeKind PickDesertOpen(uint32_t h, int floor)
{
	const float u = Unit(h);
	switch (floor) {
	case 1:
		// The Ash Dunes: what burned (RfA-72) among the dead palms.
		return u < 0.20F ? StumpCharred : u < 0.36F ? BoulderAsh : u < 0.48F ? PalmDead : u < 0.58F ? Acacia : u < 0.70F ? Thicket : u < 0.84F ? BoulderD : RocksD;
	case 2:
		// The Glass Flats: the fallen fire's glass (RfA-72b) among the petrified trees.
		return u < 0.20F ? Petrified : u < 0.30F ? ObsidianSpire : u < 0.44F ? GlassShards : u < 0.54F ? Fulgurite : u < 0.62F ? CrystalPale : u < 0.74F ? RocksD
		    : u < 0.82F ? Thicket : u < 0.92F ? BoulderD : Hoodoo;
	case 3:
		return u < 0.25F ? Thicket : u < 0.45F ? RocksD : u < 0.60F ? PalmDead : u < 0.74F ? Acacia : u < 0.88F ? BoulderD : Petrified;
	default:
		return u < 0.22F ? PalmDead : u < 0.36F ? PalmBroken : u < 0.54F ? Acacia : u < 0.74F ? Thicket : u < 0.88F ? BoulderD : RocksD;
	}
}

/** @brief A wide prop's footprint: the tiles either side of it along the screen's row (x + 1, y - 1 and x - 1, y + 1) solid too. */
void BlockFootprint(Point at)
{
	for (const Displacement d : { Displacement { 1, -1 }, Displacement { -1, 1 } }) {
		const Point p = at + d;
		if (InDungeonBounds(p) && Tree[p.x][p.y] == NoTree && Ground[p.x][p.y] != Path) {
			Tree[p.x][p.y] = Blocker;
			FrostFootprint[p.x][p.y] = true;
		}
	}
}

/** @brief A round patch cleared of whatever stands or lies on it: a place keeps its ground and nothing else. */
void ClearRound(Point centre, int radius)
{
	for (int dy = -radius; dy <= radius; dy++)
		for (int dx = -radius; dx <= radius; dx++) {
			const Point p = centre + Displacement { dx, dy };
			if (Inside(p) && dx * dx + dy * dy <= radius * radius) {
				Tree[p.x][p.y] = NoTree;
				Scatter[p.x][p.y] = 0;
			}
		}
}

/**
 * @brief A place for one of the floor's sights: open ground eleven tiles or more in from the edge, away from the road (five tiles),
 * the glade, the roads' ends and the sand hole. Every candidate from a seeded start, so a floor always finds one if it has room.
 */
std::optional<Point> FindDesertSpot(uint32_t seed, uint32_t salt)
{
	constexpr int Margin = 11;
	const int side = Hi - Lo + 1 - 2 * Margin;
	if (side <= 0)
		return std::nullopt;
	const int positions = side * side;
	const int start = static_cast<int>(Hash(seed, salt, 0x5B07U) % static_cast<uint32_t>(positions));
	for (int k = 0; k < positions; k++) {
		const int at = (start + k * 7919) % positions;
		const Point p { Lo + Margin + at % side, Lo + Margin + at / side };
		if (p.WalkingDistance(GladeCentre) < 16 || p.WalkingDistance(Arrival) < 14 || p.WalkingDistance(WayOn) < 14)
			continue;
		if (MouthOnFloor && p.WalkingDistance(MouthEntrance) < 12)
			continue;
		if ((OasisRadius > 0.0F && p.WalkingDistance(OasisCentre) < 16) || (RuinsCentre != Point {} && p.WalkingDistance(RuinsCentre) < 16)
		    || (ArenaCentre != Point {} && p.WalkingDistance(ArenaCentre) < 14))
			continue;
		// Nor on a crater, an earlier sight, or anything already standing (audit round 2: a column on a crater's middle, a siege
		// engine on the rib cage, one lone piece over another).
		if (Tree[p.x][p.y] != NoTree || std::any_of(Craters.begin(), Craters.end(), [&](Point c) { return c.WalkingDistance(p) < 8; })
		    || std::any_of(DesertSpots.begin(), DesertSpots.end(), [&](Point c) { return c.WalkingDistance(p) < 6; }))
			continue;
		bool nearRoad = false;
		for (int dy = -5; dy <= 5 && !nearRoad; dy++)
			for (int dx = -5; dx <= 5 && !nearRoad; dx++)
				nearRoad = Ground[p.x + dx][p.y + dy] == Path;
		if (!nearRoad) {
			DesertSpots.push_back(p);
			return p;
		}
	}
	return std::nullopt;
}

/**
 * @brief The oasis round @p centre (user, 2026-10-16; RfA-68): the pool - drawn by the game into the ground, its tiles solid - its
 * wet mud, a ring of reeds, the oasis grass fading out into the sand along a wandering edge, three living palms round it, and urns
 * and a basket by the water.
 */
void PlanOasis(Point centre, uint32_t seed)
{
	OasisCentre = centre;
	OasisRadius = 2.2F;
	for (int dy = -8; dy <= 8; dy++) {
		for (int dx = -8; dx <= 8; dx++) {
			const Point p = centre + Displacement { dx, dy };
			const float r = std::sqrt(static_cast<float>(dx * dx + dy * dy));
			// The road keeps its way past the oasis; a path that ends in it (the glade's) ends at the water's edge.
			if (!Inside(p) || (Ground[p.x][p.y] == Path && r > 3.4F))
				continue;
			const float d = r + (ValueNoise(p.x, p.y, 3.0F, seed ^ 0x0A51U) - 0.5F) * 1.6F;
			if (d > 7.0F)
				continue;
			Tree[p.x][p.y] = r <= OasisRadius ? Blocker : NoTree;
			Scatter[p.x][p.y] = 0;
			Ground[p.x][p.y] = d <= 3.4F ? OasisMud : d <= 4.7F ? Reeds : OasisGrass;
		}
	}
	for (uint32_t k = 0; k < 3; k++) {
		const float a = 6.2831853F * (static_cast<float>(k) / 3.0F + Unit(Hash(seed, k, 0x9A1U)) * 0.18F);
		const Point p = centre + Displacement { static_cast<int>(std::lround(std::cos(a) * 4.6F)), static_cast<int>(std::lround(std::sin(a) * 4.6F)) };
		if (Inside(p) && Ground[p.x][p.y] != Path)
			Tree[p.x][p.y] = PalmOasis;
	}
	const uint8_t byWater[] = { DecorUrn + 1, DecorBasket + 1, DecorUrn + 1 };
	for (uint32_t k = 0; k < 3; k++) {
		const float a = 6.2831853F * (static_cast<float>(k) / 3.0F + 0.5F + Unit(Hash(seed, k, 0x9A2U)) * 0.15F);
		const Point p = centre + Displacement { static_cast<int>(std::lround(std::cos(a) * 3.0F)), static_cast<int>(std::lround(std::sin(a) * 3.0F)) };
		if (Inside(p) && Tree[p.x][p.y] == NoTree)
			Scatter[p.x][p.y] = byWater[k];
	}
}

/**
 * @brief The buried ruins round @p centre: old paving showing through the drifts (RfA-68), the bones of whoever kept them - the giant
 * skull, skulls on stakes, a bone totem (RfA-67) - among broken urns, and the city's stones (RfA-70). On the Glass Flats (@p floor 2)
 * a broken column, a fallen one, a toppled head and an eclipse stele; on the Tomb of the Sun-Eater (the last floor) a court: the arch
 * at its head between two broken columns, the obelisk, the stele, two headless statues, the fallen column and the head, and two
 * braziers burning either side of its middle.
 */
void PlanRuins(Point centre, uint32_t seed, int floor)
{
	RuinsCentre = centre;
	ClearRound(centre, 7);
	for (int dy = -7; dy <= 7; dy++) {
		for (int dx = -7; dx <= 7; dx++) {
			const Point p = centre + Displacement { dx, dy };
			if (!Inside(p) || Ground[p.x][p.y] == Path)
				continue;
			const float d = std::sqrt(static_cast<float>(dx * dx + dy * dy)) + (ValueNoise(p.x, p.y, 3.0F, seed ^ 0x2B1AU) - 0.5F) * 2.4F;
			if (d <= 5.5F)
				Ground[p.x][p.y] = Paving;
		}
	}
	const auto stand = [&](Displacement d, TreeKind kind) {
		const Point p = centre + d;
		if (!Inside(p) || Ground[p.x][p.y] == Path || Tree[p.x][p.y] != NoTree)
			return;
		Tree[p.x][p.y] = kind;
		if (kind == Brazier)
			Braziers.push_back(p);
	};
	if (floor >= 3) {
		const std::pair<Displacement, TreeKind> court[] = {
			{ { -4, -4 }, ArchRuined }, { { -6, -1 }, ColumnBroken }, { { -1, -6 }, ColumnBroken }, { { 4, -4 }, Obelisk },
			{ { -4, 3 }, SteleEclipse }, { { 5, 1 }, StatueHeadless }, { { -2, 5 }, StatueHeadless }, { { 2, 6 }, ColumnFallen },
			{ { 6, 4 }, StatueHead }, { { 2, -5 }, SkullGiant }, { { -5, 0 }, Stakes },
			// RfA-70b: the guardian lying beside the arch, looking out over the court; urns, the sundial and a stub of wall in it.
			{ { -1, -4 }, GuardianLion }, { { 4, 3 }, BurialUrns }, { { -3, 6 }, BurialUrns }, { { 3, -2 }, SundialPlinth }, { { 6, -1 }, WallStub },
			// The court's back wall: a broken run along its far edge, one piece every two tiles (each spans two).
			{ { -7, -7 }, WallBroken }, { { -5, -7 }, WallBroken }, { { 1, -7 }, WallBroken },
		};
		for (const auto &[d, kind] : court)
			stand(d, kind);
		// The braziers face each other across the court's middle, off the roads that may cross it: the nearest such pair; failing
		// one, the two nearest open tiles.
		const auto open = [&](Displacement d) {
			const Point p = centre + d;
			return Inside(p) && Ground[p.x][p.y] != Path && Tree[p.x][p.y] == NoTree;
		};
		std::vector<Displacement> round;
		for (int dy = -5; dy <= 5; dy++)
			for (int dx = -5; dx <= 5; dx++)
				if (std::max(std::abs(dx), std::abs(dy)) >= 2)
					round.push_back({ dx, dy });
		std::stable_sort(round.begin(), round.end(), [](Displacement a, Displacement b) {
			return a.deltaX * a.deltaX + a.deltaY * a.deltaY < b.deltaX * b.deltaX + b.deltaY * b.deltaY;
		});
		bool lit = false;
		for (const Displacement d : round) {
			const Displacement e { -d.deltaX, -d.deltaY };
			if (open(d) && open(e)) {
				stand(d, Brazier);
				stand(e, Brazier);
				lit = true;
				break;
			}
		}
		for (size_t k = 0, placed = 0; !lit && k < round.size() && placed < 2; k++) {
			if (open(round[k]) && open(round[k] + Displacement { 1, 1 }) && open(round[k] - Displacement { 1, 1 })) {
				stand(round[k], Brazier);
				placed++;
			}
		}
	} else {
		const std::pair<Displacement, TreeKind> standing[] = {
			{ { 2, -2 }, SkullGiant }, { { -3, 1 }, Stakes }, { { 1, 3 }, Totem }, { { -2, -3 }, RocksD },
		};
		for (const auto &[d, kind] : standing)
			stand(d, kind);
		if (floor == 2) {
			const std::pair<Displacement, TreeKind> stones[] = { { { -5, -1 }, ColumnBroken }, { { 3, -5 }, ColumnFallen }, { { 5, 2 }, StatueHead },
				{ { -1, -5 }, SteleEclipse }, { { 4, 4 }, BurialUrns }, { { -4, 4 }, SundialPlinth }, { { 6, -2 }, WallStub }, { { -6, -4 }, WallBroken },
				{ { -4, -4 }, WallBroken } };
			for (const auto &[d, kind] : stones)
				stand(d, kind);
		}
	}
	const std::pair<Displacement, uint8_t> lying[] = { { { 0, 0 }, DecorBones + 1 }, { { 3, 1 }, DecorUrn + 1 }, { { -1, 4 }, DecorBones + 1 }, { { 4, -1 }, DecorBasket + 1 },
		{ { -2, 2 }, DecorLid + 1 } };
	for (const auto &[d, decor] : lying) {
		const Point p = centre + d;
		if (Inside(p) && Tree[p.x][p.y] == NoTree)
			Scatter[p.x][p.y] = decor;
	}
}

/**
 * @brief The Tomb of the Sun-Eater's arena (RfA-74), where the desert's boss waits by the way on: a round of old paving ringed by six
 * black pillars (two kinds whole, one broken), the eclipse altar in its middle with a brazier burning either side, the temple's front
 * at its back and a run of the city's broken wall along one side. Its middle is off the road; the road may cross its edge, where
 * nothing stands.
 */
void PlanArena(uint32_t seed)
{
	const Point aim = WayOn + Displacement { 11, 0 };
	const auto fits = [](Point c) {
		if (c.x - 8 < Lo || c.y - 8 < Lo || c.x + 8 > Hi || c.y + 8 > Hi || c.WalkingDistance(WayOn) < 7)
			return false;
		for (int dy = -2; dy <= 2; dy++)
			for (int dx = -2; dx <= 2; dx++)
				if (Ground[c.x + dx][c.y + dy] == Path)
					return false;
		return !(RuinsCentre != Point {} && c.WalkingDistance(RuinsCentre) < 16);
	};
	std::optional<Point> centre;
	for (int radius = 0; radius <= 12 && !centre; radius++)
		for (int dy = -radius; dy <= radius && !centre; dy++)
			for (int dx = -radius; dx <= radius && !centre; dx++)
				if (std::max(std::abs(dx), std::abs(dy)) == radius && fits(aim + Displacement { dx, dy }))
					centre = aim + Displacement { dx, dy };
	if (!centre)
		return;
	const Point c = *centre;
	ArenaCentre = c;
	ClearRound(c, 7);
	for (int dy = -6; dy <= 6; dy++)
		for (int dx = -6; dx <= 6; dx++) {
			const Point p = c + Displacement { dx, dy };
			const float d = std::sqrt(static_cast<float>(dx * dx + dy * dy)) + (ValueNoise(p.x, p.y, 3.0F, seed ^ 0xA7E4U) - 0.5F) * 1.6F;
			if (d <= 5.6F && Ground[p.x][p.y] != Path)
				Ground[p.x][p.y] = Paving;
		}
	const auto stand = [](Point p, TreeKind kind) {
		if (!InDungeonBounds(p) || Ground[p.x][p.y] == Path || Tree[p.x][p.y] != NoTree)
			return false;
		Tree[p.x][p.y] = kind;
		if (kind == Brazier)
			Braziers.push_back(p);
		return true;
	};
	stand(c, AltarEclipse);
	// The braziers either side of the altar on screen (one step up-right and down-left of it), lit by the game.
	stand(c + Displacement { 2, -2 }, Brazier);
	stand(c + Displacement { -2, 2 }, Brazier);
	// The ring: six pillars round it four tiles out, one broken.
	// Where the road crosses the ring, a pillar shifts in or out, or a little round it, to the nearest open tile (the first preview:
	// half the ring fell on the road and was left out).
	const int broken = static_cast<int>(Hash(seed, 0xA7E5U, 6) % 6U);
	for (int k = 0; k < 6; k++) {
		const TreeKind kind = k == broken ? PillarBlackBroken : (k & 1) != 0 ? PillarBlackB : PillarBlackA;
		bool placed = false;
		for (const float radius : { 4.2F, 3.4F, 5.0F })
			for (const float turn : { 0.0F, 0.22F, -0.22F })
				if (!placed) {
					const float a = 6.2831853F * (static_cast<float>(k) + 0.5F) / 6.0F + turn;
					placed = stand(c + Displacement { static_cast<int>(std::lround(std::cos(a) * radius)), static_cast<int>(std::lround(std::sin(a) * radius)) }, kind);
				}
	}
	// The temple's front at the arena's back (up the screen: -x and -y), facing the altar.
	stand(c + Displacement { -6, -6 }, TempleFront);
	// The broken wall down its right-hand side, one piece two tiles apart: its end, its run, its corner.
	// On whichever side the road leaves clear: the right-hand side first, else the left (+x runs on either).
	for (const int side : { 6, -6 }) {
		bool clear = true;
		for (int dx = -2; dx <= 5 && clear; dx++)
			clear = Ground[c.x + dx][c.y + side] != Path && Tree[c.x + dx][c.y + side] == NoTree;
		if (!clear)
			continue;
		stand(c + Displacement { -2, side }, WallEnd);
		stand(c + Displacement { 0, side }, WallBroken);
		stand(c + Displacement { 2, side }, WallBroken);
		stand(c + Displacement { 4, side }, WallCorner);
		break;
	}
}

/**
 * @brief The Salt Road's sights (user, 2026-10-16; RfA-69): what lies beside the road from the arrival to the way on, @p road its tiles
 * from the arrival on. Milestones a quarter of the way apart on every floor; a wrecked caravan - the wagon on its side, its wheel, its
 * spilled cargo, its torn tarp - on the first two floors; a wayside shrine on the first and third; the gallows where the glade's path
 * meets the road on the first and the last; a stone well by the oasis. Each stands two to four tiles off the road, never on it.
 */
void PlaceRoadside(const std::vector<Point> &road, uint32_t seed, int floor)
{
	if (road.size() < 20)
		return;
	const auto free = [](Point p) {
		if (!Inside(p) || Tree[p.x][p.y] != NoTree || Ground[p.x][p.y] == Path || Ground[p.x][p.y] == OasisMud)
			return false;
		if (p.WalkingDistance(Arrival) < 5 || p.WalkingDistance(WayOn) < 5 || (MouthOnFloor && p.WalkingDistance(MouthEntrance) < 4)
		    || (ArenaCentre != Point {} && p.WalkingDistance(ArenaCentre) < 8))
			return false;
		for (int dy = -1; dy <= 1; dy++)
			for (int dx = -1; dx <= 1; dx++) {
				const Point q = p + Displacement { dx, dy };
				if (!Inside(q) || Ground[q.x][q.y] == Path || Tree[q.x][q.y] != NoTree)
					return false;
			}
		return true;
	};
	const auto beside = [&](Point from, uint32_t h) -> std::optional<Point> {
		for (int d = 2; d <= 4; d++) {
			std::vector<Point> ring;
			for (int dy = -d; dy <= d; dy++)
				for (int dx = -d; dx <= d; dx++)
					if (std::max(std::abs(dx), std::abs(dy)) == d)
						ring.push_back(from + Displacement { dx, dy });
			const size_t start = h % ring.size();
			for (size_t k = 0; k < ring.size(); k++)
				if (free(ring[(start + k) % ring.size()]))
					return ring[(start + k) % ring.size()];
		}
		return std::nullopt;
	};
	const auto along = [&](int percent) { return road[std::min(road.size() - 1, road.size() * static_cast<size_t>(percent) / 100)]; };
	const auto stand = [&](Point p, TreeKind kind) {
		if (free(p))
			Tree[p.x][p.y] = kind;
	};
	for (const int percent : { 12, 37, 62, 87 })
		if (const std::optional<Point> at = beside(along(percent), Hash(seed, 0x3117U, static_cast<uint32_t>(percent))))
			Tree[at->x][at->y] = Milestone;
	// A travellers' cairn between the milestones, twice a floor (RfA-70b).
	for (const int percent : { 25, 75 })
		if (const std::optional<Point> at = beside(along(percent), Hash(seed, 0xCA1EU, static_cast<uint32_t>(percent))))
			Tree[at->x][at->y] = Cairn;
	if (floor <= 1) {
		if (const std::optional<Point> at = beside(along(45), Hash(seed, 0x3A6U, 1))) {
			Tree[at->x][at->y] = Wagon;
			stand(*at + Displacement { 2, 1 }, CargoSpill);
			stand(*at + Displacement { -1, 2 }, TarpPoles);
			const Point wheel = *at + Displacement { 1, -2 };
			if (free(wheel))
				Scatter[wheel.x][wheel.y] = DecorWheel + 1;
		}
	}
	if (floor == 0 || floor == 2)
		if (const std::optional<Point> at = beside(along(70), Hash(seed, 0x5A1U, 2)))
			Tree[at->x][at->y] = WayShrine;
	if (floor == 0 || floor == 3) {
		const Point fork { (Arrival.x + WayOn.x) / 2, (Arrival.y + WayOn.y) / 2 }; // where the glade's path ends on the road
		if (const std::optional<Point> at = beside(fork, Hash(seed, 0x6A1U, 3)))
			Tree[at->x][at->y] = Gallows;
	}
	if (OasisRadius > 0.0F) {
		for (int k = 0; k < 12; k++) {
			const float a = 6.2831853F * (static_cast<float>(k) / 12.0F + Unit(Hash(seed, 0x3E1U, 4)));
			const Point p = OasisCentre + Displacement { static_cast<int>(std::lround(std::cos(a) * 6.0F)), static_cast<int>(std::lround(std::sin(a) * 6.0F)) };
			if (free(p)) {
				Tree[p.x][p.y] = WellStone;
				break;
			}
		}
		// Dry reeds round the dying water (RfA-73), in the mud's ring and just past it.
		for (int k = 0; k < 10; k++) {
			const float a = 6.2831853F * (static_cast<float>(k) / 10.0F + 0.37F * Unit(Hash(seed, 0x4EEDU, static_cast<uint32_t>(k))));
			const float r = OasisRadius + 0.6F + 1.6F * Unit(Hash(seed, 0x4EEEU, static_cast<uint32_t>(k)));
			const Point q = OasisCentre + Displacement { static_cast<int>(std::lround(std::cos(a) * r)), static_cast<int>(std::lround(std::sin(a) * r)) };
			if (Inside(q) && Tree[q.x][q.y] == NoTree && Ground[q.x][q.y] != Path && Scatter[q.x][q.y] == 0)
				Scatter[q.x][q.y] = DecorReeds + 1;
		}
	}
	// The nomad camps (RfA-73): a few tents round a dead fire, six to nine tiles off the road, on the first three floors - two on the
	// Salt Road, one on each of the next two. Nobody is there; the lantern burns by night.
	const auto campSpot = [&](Point from, uint32_t h) -> std::optional<Point> {
		for (int d = 6; d <= 9; d++) {
			std::vector<Point> ring;
			for (int dy = -d; dy <= d; dy++)
				for (int dx = -d; dx <= d; dx++)
					if (std::max(std::abs(dx), std::abs(dy)) == d)
						ring.push_back(from + Displacement { dx, dy });
			const size_t start = h % ring.size();
			for (size_t k = 0; k < ring.size(); k++) {
				const Point c = ring[(start + k) % ring.size()];
				bool open = c.x - 4 >= Lo && c.x + 4 <= Hi && c.y - 4 >= Lo && c.y + 4 <= Hi && c.WalkingDistance(Arrival) >= 10 && c.WalkingDistance(WayOn) >= 10
				    && !(MouthOnFloor && c.WalkingDistance(MouthEntrance) < 10) && !(OasisRadius > 0.0F && c.WalkingDistance(OasisCentre) < 10)
				    && !(RuinsCentre != Point {} && c.WalkingDistance(RuinsCentre) < 12)
				    && std::none_of(Craters.begin(), Craters.end(), [&](Point cr) { return cr.WalkingDistance(c) < 9; }); // nor on a crater (audit round 2)
				for (int dy = -4; dy <= 4 && open; dy++)
					for (int dx = -4; dx <= 4 && open; dx++) {
						const Point q = c + Displacement { dx, dy };
						open = Ground[q.x][q.y] != Path && Ground[q.x][q.y] != OasisMud && Tree[q.x][q.y] == NoTree;
					}
				if (open)
					return c;
			}
		}
		return std::nullopt;
	};
	const int campCount = floor == 0 ? 2 : floor <= 2 ? 1 : 0;
	for (int i = 0; i < campCount; i++) {
		const std::optional<Point> at = campSpot(along(i == 0 ? 30 : 72), Hash(seed, 0xCA4FU, static_cast<uint32_t>(i)));
		if (!at)
			continue;
		const Point c = *at;
		ClearRound(c, 3);
		const std::pair<Displacement, TreeKind> standing[] = {
			{ { -1, -3 }, TentOpen }, { { 3, -1 }, TentCollapsed }, { { 1, -3 }, LanternPole }, { { -3, 0 }, WaterskinsRack },
			{ { 2, 3 }, SaltPiles }, { { -3, 3 }, HitchingPost },
		};
		for (const auto &[d, kind] : standing) {
			const Point q = c + d;
			Tree[q.x][q.y] = kind;
			if (kind == LanternPole)
				CampLanterns.push_back(q);
		}
		Scatter[c.x][c.y] = DecorCampfire + 1;
		Scatter[c.x - 1][c.y + 1] = DecorRug + 1;
	}
}

/** @brief The giant beast's rib cage (RfA-66), a landmark in the open: room round it, its footprint solid. */
void PlaceRibcage(Point at)
{
	ClearRound(at, 3);
	Tree[at.x][at.y] = Ribcage;
}

/**
 * @brief The desert floor (user, 2026-10-16): sand under dune ripples, cracked clay and bedrock, each floor its own mix - the Ash
 * Dunes more dunes, the Glass Flats more clay and rock, the Sun-Eater's tomb old paving in the sand; rock walls round the edge where
 * the woods have forest; the salt road from the arrival to the way on, a side path to the glade; and each floor's sights - the sand
 * hole down to the caves on the first and third, the oasis on the first two, the ruins on the last two, the rib cage on the second
 * and the last. No river, fences or cottages.
 */
void GenerateDesert(uint32_t seed)
{
	const int floor = GenFloor;
	std::memset(Ground, Grass, sizeof(Ground));
	std::memset(Tree, NoTree, sizeof(Tree));
	std::memset(Scatter, 0, sizeof(Scatter));
	std::memset(TreeJitterX, 0, sizeof(TreeJitterX));
	std::memset(TreeJitterY, 0, sizeof(TreeJitterY));
	std::memset(RiverCell, 0, sizeof(RiverCell));
	std::memset(FenceCell, 0, sizeof(FenceCell));
	HouseCount = 0;
	PenCount = 0;
	OasisCentre = {};
	OasisRadius = 0.0F;
	RuinsCentre = {};
	ArenaCentre = {};
	Braziers.clear();
	EmberRocks.clear();
	SmokeVents.clear();
	PaleCrystals.clear();
	Craters.clear();
	LightShafts.clear();
	DesertSpots.clear();
	CampLanterns.clear();

	const int span = Hi - Lo;
	Arrival = { Hi - 4, Lo + span / 4 + static_cast<int>(Unit(Hash(seed, 1, 0)) * static_cast<float>(span) / 2) };
	WayOn = { Lo + 4, Lo + span / 4 + static_cast<int>(Unit(Hash(seed, 2, 0)) * static_cast<float>(span) / 2) };

	const float duneAt = floor == 1 ? 0.46F : 0.60F;
	const float clayAt = floor == 2 ? 0.40F : 0.26F;
	const float rockAt = floor == 2 ? 0.66F : 0.74F;
	for (int y = 0; y < MAXDUNY; y++) {
		for (int x = 0; x < MAXDUNX; x++) {
			const Point p { x, y };
			const uint32_t h = Hash(static_cast<uint32_t>(x), static_cast<uint32_t>(y), seed ^ 0xDE5EU);
			if (!Inside(p)) {
				Tree[x][y] = PickDesertEdge(h);
				Ground[x][y] = Forest;
				continue;
			}
			uint8_t ground = Grass;
			if (ValueNoise(x, y, 9.0F, seed ^ 0xD11EU) > duneAt)
				ground = GrassDark;
			if (ValueNoise(x, y, 13.0F, seed ^ 0xF1A7U) < clayAt)
				ground = GrassDry;
			if (ValueNoise(x, y, 7.0F, seed ^ 0x50C4U) > rockAt)
				ground = Forest;
			if (floor == 3 && ValueNoise(x, y, 10.0F, seed ^ 0x9A7EU) > 0.70F)
				ground = Paving;
			Ground[x][y] = ground;
			const int edge = std::min({ x - Lo, y - Lo, Hi - x, Hi - y });
			const float borderDepth = 1.0F + 2.5F * ValueNoise(x, y, 5.0F, seed ^ 0xB0DEU);
			// The bedrock under the walls reaches out past them a wandering way (the seams of 2026-10-14: no line along the edge).
			const float shelfDepth = 1.0F + 4.0F * ValueNoise(x, y, 11.0F, seed ^ 0x1177U) + 2.0F * ValueNoise(x, y, 4.0F, seed ^ 0x1178U);
			if (static_cast<float>(edge) < borderDepth) {
				Ground[x][y] = Forest;
				Tree[x][y] = PickDesertEdge(h);
			} else if (static_cast<float>(edge) < shelfDepth) {
				Ground[x][y] = Forest;
				if (Unit(h) < 0.05F)
					Tree[x][y] = PickDesertRock(h >> 3);
			} else if (ground == Forest) {
				if (Unit(h) < 0.10F)
					Tree[x][y] = PickDesertRock(h >> 3);
			} else if (Unit(h) < 0.012F) {
				Tree[x][y] = PickDesertOpen(h >> 3, floor);
			}
		}
	}

	LayPath(Arrival, WayOn, 3, seed ^ 0x9A7EU);
	// The salt road's tiles from the arrival on, before the glade's path joins it: what stands beside it is placed along it.
	std::vector<Point> road;
	for (int y = Lo; y <= Hi; y++)
		for (int x = Lo; x <= Hi; x++)
			if (Ground[x][y] == Path)
				road.push_back({ x, y });
	std::sort(road.begin(), road.end(), [](Point a, Point b) {
		const int da = a.WalkingDistance(Arrival), db = b.WalkingDistance(Arrival);
		return da != db ? da < db : (a.y != b.y ? a.y < b.y : a.x < b.x);
	});
	const Point glade { Lo + span / 2 + static_cast<int>((Unit(Hash(seed, 3, 0)) - 0.5F) * 20), Lo + span / 2 + static_cast<int>((Unit(Hash(seed, 4, 0)) - 0.5F) * 30) };
	// How near the salt road passes the glade, before the glade's own path joins it: the oasis does not sit on the road.
	int roadToGlade = 99;
	for (int dy = -8; dy <= 8; dy++)
		for (int dx = -8; dx <= 8; dx++) {
			const Point p = glade + Displacement { dx, dy };
			if (Inside(p) && Ground[p.x][p.y] == Path)
				roadToGlade = std::min(roadToGlade, std::max(std::abs(dx), std::abs(dy)));
		}
	Clearing(glade, 6, Grass);
	GladeCentre = glade;
	LayPath(glade, { (Arrival.x + WayOn.x) / 2, (Arrival.y + WayOn.y) / 2 }, 2, seed ^ 0x51DEU);
	// The sand hole at the glade's edge, on open sand off both paths (audit, 2026-10-16: on the road, a click that ended on its middle
	// tile dropped the hero into the caves): the first of a few places round the glade with no road within two tiles.
	MouthOnFloor = MouthFloor >= 0;
	if (MouthOnFloor) {
		const auto offRoad = [](Point p) {
			for (int dy = -2; dy <= 2; dy++)
				for (int dx = -2; dx <= 2; dx++)
					if (!Inside(p + Displacement { dx, dy }) || Ground[p.x + dx][p.y + dy] == Path)
						return false;
			return true;
		};
		MouthEntrance = glade + Displacement { -4, -1 };
		bool placed = false;
		for (const Displacement d : { Displacement { -4, -1 }, Displacement { 4, 1 }, Displacement { -1, -4 }, Displacement { 1, 4 }, Displacement { -4, 3 },
		         Displacement { 4, -3 }, Displacement { -3, -4 }, Displacement { 3, 4 } }) {
			if (offRoad(glade + d)) {
				MouthEntrance = glade + d;
				placed = true;
				break;
			}
		}
		// None of the eight off the road (audit round 2: the hole then fell on the road, and a click past it dropped the hero in):
		// the nearest ring further out that is.
		for (int r = 5; r <= 9 && !placed; r++)
			for (int dy = -r; dy <= r && !placed; dy++)
				for (int dx = -r; dx <= r && !placed; dx++)
					if (std::max(std::abs(dx), std::abs(dy)) == r && offRoad(glade + Displacement { dx, dy })) {
						MouthEntrance = glade + Displacement { dx, dy };
						placed = true;
					}
		MouthOrigin = MouthEntrance - Displacement { 2, 2 };
		for (int dy = -3; dy <= 3; dy++)
			for (int dx = -3; dx <= 3; dx++) {
				const Point p = MouthEntrance + Displacement { dx, dy };
				if (!Inside(p))
					continue;
				Tree[p.x][p.y] = NoTree;
				if (Ground[p.x][p.y] != Path)
					Ground[p.x][p.y] = Grass;
			}
	}
	switch (floor) {
	case 0:
		if (const std::optional<Point> at = FindDesertSpot(seed, 0x0A5U))
			PlanOasis(*at, seed);
		break;
	case 1:
		// At the glade, unless the salt road passes within reach of the water; then somewhere off it.
		if (roadToGlade > 7)
			PlanOasis(glade, seed);
		else if (const std::optional<Point> at = FindDesertSpot(seed, 0x0A6U))
			PlanOasis(*at, seed);
		if (const std::optional<Point> at = FindDesertSpot(seed, 0x81BU))
			PlaceRibcage(*at);
		break;
	case 2:
		if (const std::optional<Point> at = FindDesertSpot(seed, 0x2B1U))
			PlanRuins(*at, seed, floor);
		break;
	default:
		PlanRuins(glade, seed, floor);
		PlanArena(seed);
		if (const std::optional<Point> at = FindDesertSpot(seed, 0x81CU))
			PlaceRibcage(*at);
		break;
	}
	// The Ash Dunes' burnt siege engines, two out in the dunes (RfA-72).
	if (floor == 1) {
		for (const uint32_t salt : { 0x72A1U, 0x72A2U })
			if (const std::optional<Point> at = FindDesertSpot(seed, salt))
				Tree[at->x][at->y] = SiegeBurnt;
	}
	// The Glass Flats' craters (RfA-72b), three, each with the glassed dead and the shards round it: where the fire fell.
	if (floor == 2) {
		// Spread out: each at least fourteen tiles from the others (the first preview: three side by side).
		for (uint32_t salt = 0x72B1U; salt < 0x72B1U + 24U && Craters.size() < 3; salt++) {
			const std::optional<Point> at = FindDesertSpot(seed, salt);
			if (!at)
				continue;
			if (std::any_of(Craters.begin(), Craters.end(), [&](Point c) { return c.WalkingDistance(*at) < 14; })) {
				DesertSpots.pop_back(); // not taken: its room stays free for the lone pieces after (audit round 3)
				continue;
			}
			ClearRound(*at, 2);
			Craters.push_back(*at);
			const Point corpse = *at + Displacement { 2, -2 };
			if (Inside(corpse) && Ground[corpse.x][corpse.y] != Path)
				Tree[corpse.x][corpse.y] = CorpseGlassed;
			const Point shards = *at + Displacement { -2, 2 };
			if (Inside(shards) && Ground[shards.x][shards.y] != Path)
				Tree[shards.x][shards.y] = GlassShards;
		}
	}
	// The buried city further out on its two floors (RfA-70): a broken column with its fallen drums beside it, and on the Tomb's
	// floor a lone obelisk and a lone stele out in the dunes.
	if (floor >= 2) {
		const auto lone = [&](uint32_t salt, TreeKind kind, std::optional<TreeKind> beside) {
			const std::optional<Point> at = FindDesertSpot(seed, salt);
			if (!at)
				return;
			Tree[at->x][at->y] = kind;
			if (beside) {
				const Point q = *at + Displacement { 2, 1 };
				if (Inside(q) && Tree[q.x][q.y] == NoTree && Ground[q.x][q.y] != Path)
					Tree[q.x][q.y] = *beside;
			}
		};
		lone(0x70A1U, ColumnBroken, ColumnFallen);
		lone(0x70B1U, AqueductArch, WallStub); // RfA-70b
		if (floor == 2) {
			lone(0x70B2U, WallBroken, WallBroken);
		}
		if (floor >= 3) {
			lone(0x70A2U, Obelisk, std::nullopt);
			lone(0x70A3U, SteleEclipse, std::nullopt);
			lone(0x70B3U, GuardianLion, BurialUrns);
		}
	}
	Clearing(Arrival, 4, Path);
	Clearing(WayOn, 4, Path);
	for (int x = Arrival.x; x <= Hi; x++)
		for (int y = Arrival.y - 1; y <= Arrival.y + 1; y++) {
			Ground[x][y] = Path;
			Tree[x][y] = NoTree;
		}
	for (int x = Lo; x <= WayOn.x; x++)
		for (int y = WayOn.y - 1; y <= WayOn.y + 1; y++) {
			Ground[x][y] = Path;
			Tree[x][y] = NoTree;
		}
	PlaceRoadside(road, seed, floor);
	// Each road's end past the box kept clear (as the woods'), before the footprints: a cleared prop leaves no Blocker behind.
	for (const Point end : { Point { Hi, Arrival.y }, Point { Lo, WayOn.y } }) {
		for (int dy = -6; dy <= 6; dy++) {
			for (int dx = -6; dx <= 6; dx++) {
				const Point p = end + Displacement { dx, dy };
				if (InDungeonBounds(p) && dx * dx + dy * dy <= 36)
					Tree[p.x][p.y] = NoTree;
			}
		}
	}
	// The wide props' footprints before the walk is sealed, so a gap they close is sealed with it.
	for (int y = Lo; y <= Hi; y++)
		for (int x = Lo; x <= Hi; x++)
			if (Tree[x][y] == Outcrop || Tree[x][y] == Ribcage || Tree[x][y] == Wagon || Tree[x][y] == ArchRuined || Tree[x][y] == ColumnFallen
			    || Tree[x][y] == StatueHead || Tree[x][y] == GuardianLion || Tree[x][y] == AqueductArch || Tree[x][y] == WallStub || Tree[x][y] == SiegeBurnt
			    || Tree[x][y] == TentOpen || Tree[x][y] == TentCollapsed || Tree[x][y] == TempleFront || Tree[x][y] == AltarEclipse)
				BlockFootprint({ x, y });
	// The broken wall runs along the ground's diagonal (top-left to bottom-right on screen: +x), each piece over two tiles: the next
	// one along it is solid too, so a run of pieces two apart leaves no gap (RfA-70b).
	for (int y = Lo; y <= Hi; y++)
		for (int x = Lo; x < Hi; x++)
			if ((Tree[x][y] == WallBroken || Tree[x][y] == WallEnd) && Tree[x + 1][y] == NoTree && Ground[x + 1][y] != Path)
				Tree[x + 1][y] = Blocker;
	SealUnreachable();

	// The ground details (RfA-65's sheet) and the things that lie on the sand (RfA-67, 68); none on the road, the hole or the water.
	constexpr int DesertScatterWeight[12] = { 3, 3, 2, 3, 4, 4, 2, 2, 2, 2, 2, 1 };
	const auto inHole = [](Point p) { return MouthOnFloor && p.WalkingDistance(MouthEntrance) <= 3; };
	for (int y = 0; y < MAXDUNY; y++) {
		for (int x = 0; x < MAXDUNX; x++) {
			const uint32_t h = Hash(static_cast<uint32_t>(x), static_cast<uint32_t>(y), seed ^ 0x5CA7U);
			if (Tree[x][y] != NoTree) {
				TreeJitterX[x][y] = static_cast<int8_t>(static_cast<int>(h & 15) - 8);
				TreeJitterY[x][y] = static_cast<int8_t>(static_cast<int>((h >> 4) & 7) - 4);
				continue;
			}
			if (!Inside({ x, y }) || Ground[x][y] == Path || Scatter[x][y] != 0 || inHole({ x, y }) || Ground[x][y] == OasisMud)
				continue;
			if (std::any_of(Craters.begin(), Craters.end(), [&](Point c) { return c.WalkingDistance(Point { x, y }) <= 2; }))
				continue;
			const float u = Unit(h >> 8);
			if (u < 0.06F) {
				int weights[12];
				std::copy(std::begin(DesertScatterWeight), std::end(DesertScatterWeight), weights);
				weights[6] = floor == 2 ? 6 : 1; // the black glass lies on the Glass Flats
				int total = 0;
				for (const int w : weights)
					total += w;
				int pick = static_cast<int>((h >> 16) % static_cast<uint32_t>(total));
				int cell = 0;
				while (cell < 11 && pick >= weights[cell])
					pick -= weights[cell++];
				Scatter[x][y] = static_cast<uint8_t>(cell + 1);
			} else if (u < 0.075F) {
				// A saltbush on sand and clay, dune grass in the ripples, bones anywhere.
				const uint32_t k = (h >> 20) % 4U;
				Scatter[x][y] = static_cast<uint8_t>((Ground[x][y] == GrassDark && k < 2 ? DecorDuneGrass : k == 3 ? DecorBones : DecorSaltbush) + 1);
			} else if (floor == 1 && u < 0.085F) {
				// The Ash Dunes' burnt leavings (RfA-72): campfires and charred bones, a vent smoking now and then.
				const uint32_t k = (h >> 20) % 5U;
				Scatter[x][y] = static_cast<uint8_t>((k < 2 ? DecorCampfire : k < 4 ? DecorBonesCharred : DecorVent) + 1);
			}
		}
	}
	// What the game lights or smokes (RfA-72, 72b): found once the floor is laid.
	for (int y = Lo; y <= Hi; y++)
		for (int x = Lo; x <= Hi; x++) {
			if (Tree[x][y] == BoulderAsh)
				EmberRocks.push_back({ x, y });
			else if (Tree[x][y] == CrystalPale)
				PaleCrystals.push_back({ x, y });
			if (Scatter[x][y] == DecorVent + 1)
				SmokeVents.push_back({ x, y });
		}
	HideDeepForest();
}

/** @brief The Frost Caves' lights (2026-10-17): the crystals' cold glow, lit as a dungeon's lights are (BuildWoodsLevel). */
std::vector<Point> FrostLights;

/** @brief How deep each tile lies in the Frost Caves' rock: 0 open, 1 the wall's face, more behind (the dark over the rock). */
uint8_t FrostRockDepth[MAXDUNX][MAXDUNY];

/** @brief A wall piece for a tile of the caverns' wall that faces open ground (RfA-77): the two masses most, the low spur, the ice spikes. */
TreeKind PickFrostWall(uint32_t h)
{
	const float u = Unit(h);
	return u < 0.40F ? FrostWallA : u < 0.76F ? FrostWallB : u < 0.88F ? FrostWallLow : u < 0.95F ? IceStalagmites : IceStalagmite;
}

/**
 * @brief Open cave floor round @p p: every tile within @p clear open (no wall, no prop), none of it the trodden way, far enough from
 * the floor's ends, its way down and the places already taken.
 */
bool FrostSpotFits(Point p, int clear, const std::vector<Point> &taken, int apart)
{
	if (p.x - clear < Lo + 2 || p.y - clear < Lo + 2 || p.x + clear > Hi - 2 || p.y + clear > Hi - 2)
		return false;
	if (p.WalkingDistance(Arrival) < 12 || p.WalkingDistance(WayOn) < 12 || (MouthOnFloor && p.WalkingDistance(MouthEntrance) < 8))
		return false;
	for (const Point t : taken)
		if (p.WalkingDistance(t) < apart)
			return false;
	if (FrostReachValid && !FrostReach[p.x][p.y])
		return false; // a cavern the trodden way never crosses: the seal fills it
	for (int dy = -clear; dy <= clear; dy++)
		for (int dx = -clear; dx <= clear; dx++) {
			const Point q = p + Displacement { dx, dy };
			if (Tree[q.x][q.y] != NoTree || Ground[q.x][q.y] == Path)
				return false;
		}
	return true;
}

/** @brief A place for one of the floor's sights: every candidate from a seeded start, so a floor finds one if it has room. */
std::optional<Point> FindFrostSpot(uint32_t seed, uint32_t salt, int clear, const std::vector<Point> &taken, int apart)
{
	constexpr int Margin = 6;
	const int side = Hi - Lo + 1 - 2 * Margin;
	const int positions = side * side;
	const int start = static_cast<int>(Hash(seed, salt, 0xF5B07U) % static_cast<uint32_t>(positions));
	for (int k = 0; k < positions; k++) {
		const int at = (start + k * 7919) % positions;
		const Point p { Lo + Margin + at % side, Lo + Margin + at / side };
		if (FrostSpotFits(p, clear, taken, apart))
			return p;
	}
	return std::nullopt;
}

/**
 * @brief The Frost Caves' floor (user, 2026-10-16: "4 100x100 underground levels in D2 Act 5 frost zones style"): caverns, not a
 * field. A cellular cave on the megatile cells - halls and throats of open ground in solid rock - with the trodden way wandering from
 * the arrival at the east edge to the way on at the west, cut through wherever the rock stood in its way, so the two always join.
 * The rock is a Blocker everywhere but where it faces open ground: there it is the RfA-77 wall pieces, packed every other tile so
 * they read as one wall. Each floor its character: the Rime Gate the war that froze there (RfA-80, 80b), the Glacial Hollows wide
 * halls of glacier ice, crystals and frozen falls (RfA-78), the Frostbound Deep narrow and dark with the beast in the ice, the Throne
 * of Winter its carved paving and pillars (its throne and ring stones when RfA-82 is in). A hole or ice stairs to the frozen river
 * below (RfA-79), and the clutter (RfA-78b) and the scatter sheet (RfA-76) over the rest.
 */
void GenerateFrost(uint32_t seed)
{
	const int floor = GenFloor;
	FrostReachValid = false;
	std::memset(FrostFootprint, 0, sizeof(FrostFootprint));
	std::memset(FrostWallTile, 0, sizeof(FrostWallTile));
	std::memset(Ground, Grass, sizeof(Ground));
	std::memset(Tree, NoTree, sizeof(Tree));
	std::memset(Scatter, 0, sizeof(Scatter));
	std::memset(TreeJitterX, 0, sizeof(TreeJitterX));
	std::memset(TreeJitterY, 0, sizeof(TreeJitterY));
	std::memset(RiverCell, 0, sizeof(RiverCell));
	std::memset(FenceCell, 0, sizeof(FenceCell));
	HouseCount = 0;
	PenCount = 0;
	OasisCentre = {};
	OasisRadius = 0.0F;
	RuinsCentre = {};
	ArenaCentre = {};
	Braziers.clear();
	EmberRocks.clear();
	SmokeVents.clear();
	PaleCrystals.clear();
	Craters.clear();
	LightShafts.clear();
	DesertSpots.clear();
	CampLanterns.clear();
	FrostLights.clear();

	const int span = Hi - Lo;
	Arrival = { Hi - 4, Lo + span / 4 + static_cast<int>(Unit(Hash(seed, 1, 0)) * static_cast<float>(span) / 2) };
	WayOn = { Lo + 4, Lo + span / 4 + static_cast<int>(Unit(Hash(seed, 2, 0)) * static_cast<float>(span) / 2) };

	// The caverns on the cells: a seeded fill, smoothed five times (a cell turns to rock with five or more rock neighbours, opens with
	// three or fewer). The Hollows the widest, the Deep the narrowest.
	constexpr float Fill[WoodsFloors] = { 0.50F, 0.54F, 0.50F, 0.51F };
	static bool open[WoodsCells][WoodsCells];
	static bool next[WoodsCells][WoodsCells];
	for (int my = 0; my < WoodsCells; my++)
		for (int mx = 0; mx < WoodsCells; mx++)
			open[mx][my] = mx > 0 && my > 0 && mx < WoodsCells - 1 && my < WoodsCells - 1
			    && Unit(Hash(static_cast<uint32_t>(mx), static_cast<uint32_t>(my), seed ^ 0xF205U)) < Fill[floor];
	for (int pass = 0; pass < 5; pass++) {
		for (int my = 0; my < WoodsCells; my++)
			for (int mx = 0; mx < WoodsCells; mx++) {
				int rock = 0;
				for (int dy = -1; dy <= 1; dy++)
					for (int dx = -1; dx <= 1; dx++) {
						if (dx == 0 && dy == 0)
							continue;
						const int nx = mx + dx, ny = my + dy;
						if (nx < 0 || ny < 0 || nx >= WoodsCells || ny >= WoodsCells || !open[nx][ny])
							rock++;
					}
				const bool edge = mx == 0 || my == 0 || mx == WoodsCells - 1 || my == WoodsCells - 1;
				next[mx][my] = !edge && (rock >= 5 ? false : rock <= 3 ? true : open[mx][my]);
			}
		std::memcpy(open, next, sizeof(open));
	}

	// The ground, and the rock as Blockers for now: open cells are snow over rock, drifts in the wind's lee, gravel in the throats,
	// rimed rock beside the walls; the Hollows' floors glacier ice, the Throne's carved paving.
	for (int y = 0; y < MAXDUNY; y++) {
		for (int x = 0; x < MAXDUNX; x++) {
			const Point p { x, y };
			if (!Inside(p)) {
				Ground[x][y] = Forest;
				Tree[x][y] = Blocker;
				continue;
			}
			uint8_t ground = Grass;
			if (ValueNoise(x, y, 9.0F, seed ^ 0xD21FU) > 0.62F)
				ground = GrassDark;
			if (ValueNoise(x, y, 11.0F, seed ^ 0x6A7EU) < 0.28F)
				ground = GrassDry;
			if (floor == 1 && ValueNoise(x, y, 14.0F, seed ^ 0x61AC1U) > 0.50F)
				ground = OasisGrass;
			if (floor == 3 && ValueNoise(x, y, 12.0F, seed ^ 0x7A12U) > 0.55F)
				ground = Paving;
			Ground[x][y] = ground;
			const int mx = (x - Lo) / 2, my = (y - Lo) / 2;
			if (!open[mx][my])
				Tree[x][y] = Blocker;
		}
	}
	// The cave's edges irregular, not the cells' squares: a rock tile beside open ground opens half the time, an open one beside
	// rock closes a fifth of it.
	for (int y = Lo + 1; y < Hi; y++)
		for (int x = Lo + 1; x < Hi; x++) {
			int rockBeside = 0;
			for (const Displacement d : { Displacement { 1, 0 }, Displacement { -1, 0 }, Displacement { 0, 1 }, Displacement { 0, -1 } })
				rockBeside += Tree[x + d.deltaX][y + d.deltaY] == Blocker ? 1 : 0;
			const float u = Unit(Hash(static_cast<uint32_t>(x), static_cast<uint32_t>(y), seed ^ 0xE06EU));
			if (Tree[x][y] == Blocker && rockBeside <= 2 && u < 0.3F)
				Tree[x][y] = NoTree;
			else if (Tree[x][y] == NoTree && rockBeside >= 2 && u < 0.2F)
				Tree[x][y] = Blocker;
		}

	// The trodden way, through the rock where it must be, and the strips out at both edges.
	LayPath(Arrival, WayOn, 3, seed ^ 0x9A7EU);
	for (int x = Arrival.x; x <= Hi; x++)
		for (int y = Arrival.y - 1; y <= Arrival.y + 1; y++) {
			Ground[x][y] = Path;
			Tree[x][y] = NoTree;
		}
	for (int x = Lo; x <= WayOn.x; x++)
		for (int y = WayOn.y - 1; y <= WayOn.y + 1; y++) {
			Ground[x][y] = Path;
			Tree[x][y] = NoTree;
		}
	Clearing(Arrival, 4, Path);
	Clearing(WayOn, 4, Path);
	GladeCentre = { (Arrival.x + WayOn.x) / 2, (Arrival.y + WayOn.y) / 2 };

	// The way down to the frozen river: open floor off the trodden way, the hole or the ice stairs (RfA-79).
	MouthOnFloor = MouthFloor >= 0;
	std::vector<Point> taken;
	if (MouthOnFloor) {
		MouthEntrance = GladeCentre;
		if (const std::optional<Point> at = FindFrostSpot(seed, 0xD0D0U, 3, taken, 0))
			MouthEntrance = *at;
		MouthOrigin = MouthEntrance - Displacement { 2, 2 };
		for (int dy = -3; dy <= 3; dy++)
			for (int dx = -3; dx <= 3; dx++) {
				const Point q = MouthEntrance + Displacement { dx, dy };
				if (!Inside(q))
					continue;
				Tree[q.x][q.y] = NoTree;
				if (Ground[q.x][q.y] != Path)
					Ground[q.x][q.y] = Grass;
			}
		taken.push_back(MouthEntrance);
		// A trail from it to the nearest of the trodden way (the first test: on the Deep it fell in a pocket of its own, sealed).
		Point nearest = Arrival;
		int best = 1 << 30;
		for (int y = Lo; y <= Hi; y++)
			for (int x = Lo; x <= Hi; x++)
				if (Ground[x][y] == Path && Point { x, y }.WalkingDistance(MouthEntrance) < best && Point { x, y }.WalkingDistance(MouthEntrance) > 3) {
					best = Point { x, y }.WalkingDistance(MouthEntrance);
					nearest = { x, y };
				}
		LayPath(MouthEntrance, nearest, 1, seed ^ 0x7A1CU);
	}

	// What the trodden way reaches now, through open floor: every sight below must stand in it.
	{
		std::memset(FrostReach, 0, sizeof(FrostReach));
		std::vector<Point> queue { Arrival };
		if (Inside(Arrival))
			FrostReach[Arrival.x][Arrival.y] = true;
		for (size_t head = 0; head < queue.size(); head++) {
			const Point at = queue[head];
			for (const Displacement d : { Displacement { 1, 0 }, Displacement { -1, 0 }, Displacement { 0, 1 }, Displacement { 0, -1 } }) {
				const Point q = at + d;
				if (!Inside(q) || FrostReach[q.x][q.y] || Tree[q.x][q.y] != NoTree)
					continue;
				FrostReach[q.x][q.y] = true;
				queue.push_back(q);
			}
		}
		FrostReachValid = true;
	}

	// The floor's sights, each in an open place of its own.
	const auto place = [&](uint32_t salt, int clear, std::initializer_list<std::pair<Displacement, TreeKind>> pieces) {
		const std::optional<Point> at = FindFrostSpot(seed, salt, clear, taken, 14);
		if (!at)
			return;
		taken.push_back(*at);
		for (const auto &[d, kind] : pieces) {
			const Point q = *at + d;
			if (Inside(q) && Tree[q.x][q.y] == NoTree && Ground[q.x][q.y] != Path)
				Tree[q.x][q.y] = kind;
		}
	};
	switch (floor) {
	case 0:
		// The Rime Gate: the last stand at the caves' mouth (RfA-80, 80b).
		place(0xF0A1U, 4, { { { 0, 0 }, BarricadeStakes }, { { -2, -2 }, BannerTorn }, { { 2, 2 }, WarriorSlumped }, { { -3, 2 }, CatapultBroken } });
		place(0xF0A2U, 3, { { { 0, 0 }, CratesBarrels }, { { 2, -1 }, SledFrozen }, { { -2, 1 }, WeaponRack }, { { 1, 2 }, ShieldsSpears } });
		place(0xF0A3U, 2, { { { 0, 0 }, BarbarianInIce } });
		place(0xF0A4U, 2, { { { 0, 0 }, ShieldsSpears }, { { 2, 1 }, BannerTorn } });
		break;
	case 1:
		// The Glacial Hollows' frozen lakes (RfA-81): black ice ringed with stones, a hole or two of dark water in it, a fish frozen in a
		// block on its shore.
		for (const uint32_t salt : { 0xF1B1U, 0xF1B2U }) {
			const std::optional<Point> at = FindFrostSpot(seed, salt, 5, taken, 16);
			if (!at)
				continue;
			taken.push_back(*at);
			const float radius = 3.6F + 1.2F * Unit(Hash(seed, salt, 7));
			for (int dy = -6; dy <= 6; dy++)
				for (int dx = -6; dx <= 6; dx++) {
					const Point q = *at + Displacement { dx, dy };
					if (!Inside(q) || Tree[q.x][q.y] != NoTree || Ground[q.x][q.y] == Path)
						continue;
					const float d = std::sqrt(static_cast<float>(dx * dx + dy * dy)) + (ValueNoise(q.x, q.y, 3.0F, seed ^ salt) - 0.5F) * 1.4F;
					if (d <= radius)
						Ground[q.x][q.y] = OasisMud; // the lakes' black ice
					else if (d <= radius + 1.0F && ((q.x + q.y) % 3) == 0)
						Scatter[q.x][q.y] = DecorLakeStones + 1;
				}
			for (const Displacement d : { Displacement { 1, -1 }, Displacement { -2, 1 } }) {
				const Point q = *at + d;
				if (Inside(q) && Tree[q.x][q.y] == NoTree && Ground[q.x][q.y] != Path && Unit(Hash(seed, salt, static_cast<uint32_t>(d.deltaX + 8))) < 0.7F)
					Tree[q.x][q.y] = IceHole; // over nothing already standing (audit round 4)
			}
			const Point shore = *at + Displacement { static_cast<int>(radius) + 1, 1 };
			if (Inside(shore) && Tree[shore.x][shore.y] == NoTree && Ground[shore.x][shore.y] != Path)
				Tree[shore.x][shore.y] = FishInIce;
		}
		place(0xF1C1U, 3, { { { 0, 0 }, CrevasseLip } });
		place(0xF1C2U, 3, { { { 0, 0 }, CrevasseLip } });
		place(0xF1C3U, 2, { { { 0, 0 }, CrystalSpire }, { { 1, 2 }, CrystalsSmall } });
		place(0xF1C4U, 2, { { { 0, 0 }, CrystalSpire } });
		// The Glacial Hollows: wide halls of ice, frozen falls, the crystals' cold light (RfA-78).
		place(0xF1A1U, 3, { { { 0, 0 }, FrozenWaterfall } });
		place(0xF1A2U, 3, { { { 0, 0 }, FrozenWaterfall } });
		place(0xF1A3U, 2, { { { 0, 0 }, IcePillar } });
		place(0xF1A4U, 2, { { { 0, 0 }, IcePillar } });
		place(0xF1A5U, 2, { { { 0, 0 }, CrystalCluster }, { { 2, 1 }, CrystalsSmall } });
		place(0xF1A6U, 2, { { { 0, 0 }, CrystalCluster }, { { -1, 2 }, CrystalsSmall } });
		place(0xF1A7U, 2, { { { 0, 0 }, CrystalCluster } });
		break;
	case 2:
		// The Frostbound Deep's graves and what was chained there (RfA-81b): a barrow field, a rune stone, the kneeling dead, chains.
		place(0xF2B1U, 3, { { { 0, 0 }, BurialMound }, { { 2, 1 }, RuneStone }, { { -2, 1 }, BurialMound } });
		place(0xF2B2U, 2, { { { 0, 0 }, ChainsRock }, { { 1, 2 }, CorpseKneeling } });
		place(0xF2B3U, 2, { { { 0, 0 }, CorpseKneeling } });
		place(0xF2B4U, 2, { { { 0, 0 }, RuneStone } });
		// The Frostbound Deep: narrow and dark; the beast in the ice, the frozen dead (RfA-78, 80).
		place(0xF2A1U, 4, { { { 0, 0 }, BeastInIce } });
		place(0xF2A2U, 2, { { { 0, 0 }, BarbarianInIce } });
		place(0xF2A3U, 2, { { { 0, 0 }, CairnSkull }, { { 2, 1 }, WarriorSlumped } });
		place(0xF2A4U, 2, { { { 0, 0 }, CrystalCluster } });
		place(0xF2A5U, 2, { { { 0, 0 }, IcePillar } });
		break;
	default: {
		// The Throne of Winter's hall (RfA-82): the frozen throne at its head (up the screen, -x -y), two rows of carved pillars, some
		// broken, leading down to it from the arch at its foot; its floor the carved paving. The boss waits before the throne
		// (WoodsBossSpot, ArenaCentre).
		if (const std::optional<Point> hall = FindFrostSpot(seed, 0xF3C1U, 6, taken, 16)) {
			taken.push_back(*hall);
			ArenaCentre = *hall;
			for (int dy = -6; dy <= 6; dy++)
				for (int dx = -6; dx <= 6; dx++) {
					const Point q = *hall + Displacement { dx, dy };
					if (Inside(q) && Ground[q.x][q.y] != Path && dx * dx + dy * dy <= 40)
						Ground[q.x][q.y] = Paving;
				}
			const auto stand = [&](Displacement d, TreeKind kind) {
				const Point q = *hall + d;
				if (Inside(q) && Tree[q.x][q.y] == NoTree && Ground[q.x][q.y] != Path)
					Tree[q.x][q.y] = kind;
			};
			// The hall runs along x, down and to the right on the screen, so its rows of pillars stand across the picture (along the
			// screen's vertical, the first render, each side's stood in one line and read as a single column): the throne at its head
			// with a frozen brazier either side, the boss's ring of six stones in its middle (RfA-82b), the pillars down both sides, the
			// arch at its foot and the old king's statue by it.
			stand({ -5, 0 }, ThroneFrozen);
			stand({ -5, -2 }, BrazierFrozen);
			stand({ -5, 2 }, BrazierFrozen);
			constexpr Displacement Ring[6] = { { 3, 2 }, { 0, 3 }, { -3, 2 }, { -3, -2 }, { 0, -3 }, { 3, -2 } };
			const int cracked = static_cast<int>(Hash(seed, 0xF3C3U, 0U) % 6U); // the one cracked stone, rolled once (audit)
			for (int k = 0; k < 6; k++)
				stand(Ring[k], k == cracked ? RingStoneC : (k & 1) != 0 ? RingStoneB : RingStoneA);
			for (int k = 0; k < 3; k++) {
				const int along = -4 + 4 * k;
				const bool broken = Unit(Hash(seed, 0xF3C2U, static_cast<uint32_t>(k))) < 0.34F;
				stand({ along, 5 }, broken ? CathedralPillarBroken : CathedralPillar);
				stand({ along, -5 }, CathedralPillar);
			}
			stand({ 2, -5 }, KingStatue);
			stand({ 6, 0 }, ArchIce);
		}
		for (uint32_t k = 0; k < 4; k++)
			place(0xF3A1U + k, 2, { { { 0, 0 }, IcePillar } });
		place(0xF3B1U, 2, { { { 0, 0 }, BarbarianInIce } });
		place(0xF3B2U, 2, { { { 0, 0 }, CrystalCluster } });
		break;
	}
	}


	// The clutter on open floor (RfA-78b, 80b), each floor its own mix.
	for (int y = Lo + 2; y <= Hi - 2; y++)
		for (int x = Lo + 2; x <= Hi - 2; x++) {
			if (Tree[x][y] != NoTree || Ground[x][y] == Path)
				continue;
			const Point p { x, y };
			if (p.WalkingDistance(Arrival) < 6 || p.WalkingDistance(WayOn) < 6 || (MouthOnFloor && p.WalkingDistance(MouthEntrance) < 5))
				continue;
			if (ArenaCentre != Point {} && p.WalkingDistance(ArenaCentre) <= 7)
				continue; // the throne hall's floor stays clear (audit)
			const uint32_t h = Hash(static_cast<uint32_t>(x), static_cast<uint32_t>(y), seed ^ 0xC1A7U);
			if (Unit(h) >= 0.014F)
				continue;
			const float u = Unit(h >> 8);
			TreeKind kind = RocksSnow;
			switch (floor) {
			case 0: kind = u < 0.25F ? RocksSnow : u < 0.45F ? ShrubFrozen : u < 0.60F ? IceBoulder : u < 0.75F ? IcicleLedge : u < 0.88F ? CairnSkull : IceStalagmite; break;
			case 1: kind = u < 0.22F ? IceSlab : u < 0.42F ? CrystalsSmall : u < 0.60F ? IceStalagmite : u < 0.75F ? IcicleLedge : u < 0.88F ? IceBoulder : RocksSnow; break;
			case 2: kind = u < 0.25F ? RocksSnow : u < 0.45F ? CairnSkull : u < 0.62F ? IceStalagmites : u < 0.78F ? IcicleLedge : u < 0.90F ? CrystalsSmall : IceBoulder; break;
			default: kind = u < 0.30F ? IceStalagmite : u < 0.50F ? IcicleLedge : u < 0.70F ? RocksSnow : u < 0.85F ? IceBoulder : CrystalsSmall; break;
			}
			Tree[x][y] = kind;
		}

	// The wide ones block their footprints, before the walk is sealed.
	for (int y = Lo; y <= Hi; y++)
		for (int x = Lo; x <= Hi; x++)
			if (Tree[x][y] == BeastInIce || Tree[x][y] == FrozenWaterfall || Tree[x][y] == CatapultBroken || Tree[x][y] == BarricadeStakes
			    || Tree[x][y] == SledFrozen || Tree[x][y] == CrevasseLip || Tree[x][y] == BurialMound || Tree[x][y] == ThroneFrozen || Tree[x][y] == ArchIce)
				BlockFootprint({ x, y });
	SealUnreachable();

	// Two light shafts a floor (RfA-79b): snow heaped under a hole in the roof, in the open, apart from each
	// other and the sights; after the seal, so never in a pocket it walled up.
	for (uint32_t salt = 0x5AF7U; salt < 0x5AF7U + 16U && LightShafts.size() < 2; salt++)
		if (const std::optional<Point> at = FindFrostSpot(seed, salt, 2, taken, 12)) {
			taken.push_back(*at);
			LightShafts.push_back(*at);
		}

	// The rock that faces open ground stands as the wall pieces, every other tile and a third of the rest, so the wall reads whole;
	// the rest of the rock stays unseen.
	for (int y = Lo; y <= Hi; y++)
		for (int x = Lo; x <= Hi; x++) {
			if (Tree[x][y] != Blocker || FrostFootprint[x][y])
				continue;
			bool facesOpen = false;
			for (int dy = -1; dy <= 1 && !facesOpen; dy++)
				for (int dx = -1; dx <= 1 && !facesOpen; dx++)
					facesOpen = InDungeonBounds({ x + dx, y + dy }) && Tree[x + dx][y + dy] == NoTree;
			if (!facesOpen)
				continue;
			const uint32_t h = Hash(static_cast<uint32_t>(x), static_cast<uint32_t>(y), seed ^ 0x7A11U);
			Ground[x][y] = Forest;
			Tree[x][y] = PickFrostWall(h >> 4);
			FrostWallTile[x][y] = true;
			// The Deep's walls hold its dead (RfA-81b): one face in six.
			if (floor == 2 && Unit(Hash(static_cast<uint32_t>(x), static_cast<uint32_t>(y), seed ^ 0xFACEU)) < 0.17F && (Tree[x][y] == FrostWallA || Tree[x][y] == FrostWallB))
				Tree[x][y] = WallFaces;
		}
	// A tomb door cut in the Deep's wall (RfA-81b): the first wall face with open floor in front of it (down the screen) and wall on
	// both its sides, from a seeded start.
	if (floor == 2) {
		const int side = Hi - Lo + 1;
		const int start = static_cast<int>(Hash(seed, 0x70DBU, 1) % static_cast<uint32_t>(side * side));
		for (int k = 0; k < side * side; k++) {
			const int at = (start + k * 7919) % (side * side);
			const int x = Lo + at % side, y = Lo + at / side;
			if (x <= Lo + 2 || y <= Lo + 2 || x >= Hi - 2 || y >= Hi - 2)
				continue;
			const auto wall = [](int wx, int wy) { return FrostWallTile[wx][wy]; };
			if (!wall(x, y) || Tree[x + 1][y + 1] != NoTree || Tree[x + 2][y + 2] != NoTree || !wall(x + 1, y - 1) || !wall(x - 1, y + 1))
				continue;
			Tree[x][y] = TombDoor;
			break;
		}
	}
	// How deep each tile lies in the rock: 0 open (a prop on open floor too), 1 the wall's face, more behind it.
	std::memset(FrostRockDepth, 0, sizeof(FrostRockDepth));
	{
		const auto rock = [](int x, int y) {
			const uint8_t t = Tree[x][y];
			return (t == Blocker && !FrostFootprint[x][y]) || FrostWallTile[x][y] || t == TombDoor;
		};
		std::vector<Point> frontier;
		for (int y = 0; y < MAXDUNY; y++)
			for (int x = 0; x < MAXDUNX; x++) {
				if (!rock(x, y))
					continue;
				FrostRockDepth[x][y] = 255;
				bool face = false;
				for (int dy = -1; dy <= 1 && !face; dy++)
					for (int dx = -1; dx <= 1 && !face; dx++)
						face = InDungeonBounds({ x + dx, y + dy }) && !rock(x + dx, y + dy) && !FrostFootprint[x + dx][y + dy]; // a prop's footprint is no open floor to face (audit round 2)
				if (face) {
					FrostRockDepth[x][y] = 1;
					frontier.push_back({ x, y });
				}
			}
		for (size_t i = 0; i < frontier.size(); i++) {
			const Point p = frontier[i];
			for (const Displacement d : { Displacement { 1, 0 }, Displacement { -1, 0 }, Displacement { 0, 1 }, Displacement { 0, -1 } }) {
				const Point n = p + d;
				if (!InDungeonBounds(n) || FrostRockDepth[n.x][n.y] <= FrostRockDepth[p.x][p.y] + 1)
					continue;
				FrostRockDepth[n.x][n.y] = static_cast<uint8_t>(FrostRockDepth[p.x][p.y] + 1);
				frontier.push_back(n);
			}
		}
	}
	// The wall's second row: two tiles in five behind its face stand too, so the wall has mass.
	for (int y = Lo; y <= Hi; y++)
		for (int x = Lo; x <= Hi; x++)
			if (Tree[x][y] == Blocker && !FrostFootprint[x][y] && FrostRockDepth[x][y] == 2
			    && Unit(Hash(static_cast<uint32_t>(x), static_cast<uint32_t>(y), seed ^ 0x7A12U)) < 0.4F) {
				Tree[x][y] = Unit(Hash(static_cast<uint32_t>(x), static_cast<uint32_t>(y), seed ^ 0x7A13U)) < 0.5F ? FrostWallA : FrostWallB;
				FrostWallTile[x][y] = true;
			}

	// The way on (RfA-79): the passage into the rock at the trodden way's far end, just past its last tile, on every floor but the
	// last (its throne hall ends the caves).
	if (floor < WoodsFloors - 1)
		Tree[Lo - 1][WayOn.y] = PassageOn;

	// The ground details (RfA-76's sheet) and the lying things (RfA-78b, 80b); none on the way, the hole or the walls.
	constexpr int FrostScatterWeight[12] = { 4, 2, 3, 3, 2, 2, 1, 1, 2, 2, 2, 1 };
	for (int y = 0; y < MAXDUNY; y++) {
		for (int x = 0; x < MAXDUNX; x++) {
			const uint32_t h = Hash(static_cast<uint32_t>(x), static_cast<uint32_t>(y), seed ^ 0x5CA7U);
			if (Tree[x][y] != NoTree) {
				TreeJitterX[x][y] = static_cast<int8_t>(static_cast<int>(h & 15) - 8);
				TreeJitterY[x][y] = static_cast<int8_t>(static_cast<int>((h >> 4) & 7) - 4);
				continue;
			}
			if (!Inside({ x, y }) || Ground[x][y] == Path || Ground[x][y] == OasisMud || Scatter[x][y] != 0 || (MouthOnFloor && Point { x, y }.WalkingDistance(MouthEntrance) <= 3))
				continue;
			const float u = Unit(h >> 8);
			if (u < 0.05F) {
				int total = 0;
				for (const int w : FrostScatterWeight)
					total += w;
				int pick = static_cast<int>((h >> 16) % static_cast<uint32_t>(total));
				int cell = 0;
				while (cell < 11 && pick >= FrostScatterWeight[cell])
					pick -= FrostScatterWeight[cell++];
				Scatter[x][y] = static_cast<uint8_t>(cell + 1);
			} else if (u < 0.062F) {
				const uint32_t k = (h >> 20) % 5U;
				Scatter[x][y] = static_cast<uint8_t>((k < 2 ? DecorDriftRock : k < 4 ? DecorBonesFrost : (floor == 0 ? DecorCampfireFrozen : DecorBonesFrost)) + 1);
			}
		}
	}
	// What lights the dark (BuildWoodsLevel): the crystals.
	for (int y = Lo; y <= Hi; y++)
		for (int x = Lo; x <= Hi; x++)
			if (Tree[x][y] == CrystalCluster || Tree[x][y] == CrystalsSmall || Tree[x][y] == CrystalSpire)
				FrostLights.push_back({ x, y });
	HideDeepForest();
}

void Generate(uint32_t seed)
{
	if (Frost) {
		GenerateFrost(seed);
		return;
	}
	if (Desert) {
		GenerateDesert(seed);
		return;
	}
	OasisCentre = {};
	OasisRadius = 0.0F;
	RuinsCentre = {};
	ArenaCentre = {};
	Braziers.clear();
	EmberRocks.clear();
	SmokeVents.clear();
	PaleCrystals.clear();
	Craters.clear();
	LightShafts.clear();
	DesertSpots.clear();
	CampLanterns.clear();
	std::memset(Ground, Grass, sizeof(Ground));
	std::memset(Tree, NoTree, sizeof(Tree));
	std::memset(Scatter, 0, sizeof(Scatter));
	std::memset(TreeJitterX, 0, sizeof(TreeJitterX));
	std::memset(TreeJitterY, 0, sizeof(TreeJitterY));

	// The arrival on the town's side of the floor (east on screen: high x, low y), the way on at the far side (west on
	// screen: low x, high y), each at a seeded spot along its edge.
	const int span = Hi - Lo;
	Arrival = { Hi - 4, Lo + span / 4 + static_cast<int>(Unit(Hash(seed, 1, 0)) * static_cast<float>(span) / 2) };
	WayOn = { Lo + 4, Lo + span / 4 + static_cast<int>(Unit(Hash(seed, 2, 0)) * static_cast<float>(span) / 2) };

	for (int y = 0; y < MAXDUNY; y++) {
		for (int x = 0; x < MAXDUNX; x++) {
			const Point p { x, y };
			if (!Inside(p)) {
				// The wood beyond the map: every tile a tree, of every kind, so its edge is a forest and not a fence.
				Tree[x][y] = PickTree(Forest, Hash(static_cast<uint32_t>(x), static_cast<uint32_t>(y), seed ^ 0x0E7EU));
				Ground[x][y] = Forest;
				continue;
			}
			// Moisture picks the grass; groves are their own noise.
			const float moist = ValueNoise(x, y, 11.0F, seed ^ 0x3331U);
			// The grasses as the user shared them on the Testing Ground (53 grass, 44 dark, 15 dry): about half, four in ten, one in ten.
			Ground[x][y] = moist > 0.56F ? GrassDark : moist < 0.30F ? GrassDry : Grass;
			const float grove = ValueNoise(x, y, 8.0F, seed ^ 0x6A7EU);
			const int edge = std::min({ x - Lo, y - Lo, Hi - x, Hi - y });
			// Thinner than at first (user, 2026-10-14: "reduce number of trees on woods levels, and outline levels borders
			// with fences"): a band of one to three tiles inside the fence, the forest proper beyond it.
			const float borderDepth = 1.0F + 2.0F * ValueNoise(x, y, 5.0F, seed ^ 0xB0DEU);
			// The forest's floor reaches further than its trees, by a wandering depth of one to eight tiles (the sharp borders,
			// 2026-10-14: the litter's edge ran parallel to the map's, one long straight line on the screen).
			const float litterDepth = 1.0F + 5.0F * ValueNoise(x, y, 11.0F, seed ^ 0x1177U) + 2.0F * ValueNoise(x, y, 4.0F, seed ^ 0x1178U);
			const uint32_t h = Hash(static_cast<uint32_t>(x), static_cast<uint32_t>(y), seed ^ 0x7EE5U);
			if (static_cast<float>(edge) < borderDepth) {
				Ground[x][y] = Forest;
				Tree[x][y] = PickTree(Forest, h);
			} else if (static_cast<float>(edge) < litterDepth && grove <= 0.64F) {
				Ground[x][y] = Forest;
				if (Unit(h) < 0.015F)
					Tree[x][y] = PickTree(Forest, h >> 3);
			} else if (grove > 0.64F) {
				Ground[x][y] = Forest;
				if (Unit(h) < 0.17F) // a grove you can see into (the play test: at 0.42 the canopies were one carpet; 0.22 then)
					Tree[x][y] = PickTree(Forest, h >> 3);
			} else if (Unit(h) < 0.015F) {
				Tree[x][y] = PickTree(static_cast<Material>(Ground[x][y]), h >> 3); // a lone tree in the grass
			}
		}
	}

	LayPath(Arrival, WayOn, 3, seed ^ 0x9A7EU);
	// A side path to a clearing, so the floor is more than one road.
	const Point glade { Lo + span / 2 + static_cast<int>((Unit(Hash(seed, 3, 0)) - 0.5F) * 20), Lo + span / 2 + static_cast<int>((Unit(Hash(seed, 4, 0)) - 0.5F) * 30) };
	Clearing(glade, 6, Grass);
	GladeCentre = glade;
	LayPath(glade, { (Arrival.x + WayOn.x) / 2, (Arrival.y + WayOn.y) / 2 }, 2, seed ^ 0x51DEU);
	// The cave mouth on the glade's far side, its entrance facing into the glade; trees and ground details kept off it.
	MouthOnFloor = MouthFloor >= 0;
	if (MouthOnFloor) {
		MouthOrigin = glade + Displacement { -9, -4 }; // the entrance (5, 3) in, facing east into the glade
		MouthEntrance = MouthOrigin + MouthEntranceOffset;
		for (int y = MouthOrigin.y - 2; y < MouthOrigin.y + MouthHeight + 2; y++) {
			for (int x = MouthOrigin.x - 2; x < MouthOrigin.x + MouthWidth + 3; x++) {
				if (!Inside({ x, y }))
					continue;
				Tree[x][y] = NoTree;
				if (Ground[x][y] == Forest)
					Ground[x][y] = Grass;
			}
		}
	}
	Clearing(Arrival, 4, Path);
	Clearing(WayOn, 4, Path);
	// The roads off the map's edge: back toward Tristram and on into the woods.
	for (int x = Arrival.x; x <= Hi; x++)
		for (int y = Arrival.y - 1; y <= Arrival.y + 1; y++) {
			Ground[x][y] = Path;
			Tree[x][y] = NoTree;
		}
	for (int x = Lo; x <= WayOn.x; x++)
		for (int y = WayOn.y - 1; y <= WayOn.y + 1; y++) {
			Ground[x][y] = Path;
			Tree[x][y] = NoTree;
		}
	SealUnreachable();

	for (int y = 0; y < MAXDUNY; y++) {
		for (int x = 0; x < MAXDUNX; x++) {
			const uint32_t h = Hash(static_cast<uint32_t>(x), static_cast<uint32_t>(y), seed ^ 0x5CA7U);
			if (Tree[x][y] != NoTree) {
				TreeJitterX[x][y] = static_cast<int8_t>(static_cast<int>(h & 15) - 8);
				TreeJitterY[x][y] = static_cast<int8_t>(static_cast<int>((h >> 4) & 7) - 4);
			} else if (Inside({ x, y }) && Ground[x][y] != Path && Unit(h >> 8) < 0.07F) {
				// Weighted as the Testing Ground counted them (ScatterWeight).
				int total = 0;
				for (const int w : ScatterWeight)
					total += w;
				int pick = static_cast<int>((h >> 16) % static_cast<uint32_t>(total));
				int cell = 0;
				while (cell < 11 && pick >= ScatterWeight[cell])
					pick -= ScatterWeight[cell++];
				Scatter[x][y] = static_cast<uint8_t>(cell + 1);
			}
		}
	}
	// Each road's end past the fence ring kept clear of trees: the forest beyond the ring stands in front of it on screen and
	// hid the gate and the way out (preview, 2026-10-14).
	for (const Point end : { Point { Hi, Arrival.y }, Point { Lo, WayOn.y } }) {
		for (int dy = -6; dy <= 6; dy++) {
			for (int dx = -6; dx <= 6; dx++) {
				const Point p = end + Displacement { dx, dy };
				if (InDungeonBounds(p) && dx * dx + dy * dy <= 36)
					Tree[p.x][p.y] = NoTree;
			}
		}
	}
	PlanRiver(seed);
	PlanFences(seed);
	HideDeepForest();
}

// ---------------------------------------------------------------- the art

struct Picture {
	int w = 0;
	int h = 0;
	std::vector<uint32_t> px; // 0x00RRGGBB, with the top byte 0xFF where opaque in a sprite
	/** @brief A sprite's opaque columns per row, [first, last); empty rows have first == last. */
	std::vector<int16_t> spanFirst;
	std::vector<int16_t> spanLast;
	int footX = 0;
	int footY = 0;
};

/** @brief Each area's ground textures, in Material's order, and the scale each is drawn at (percent of the painting). */
struct GroundFile {
	const char *path;
	int percent;
};
constexpr GroundFile GroundFiles[OrclAreaCount][MaterialCount] = {
	{
		// The scales the user chose on the Orcl Testing Ground (its settings/current, saved 2026-10-14 15:19): the grasses at
		// 13-14, the forest floor at 16, the dirt road at 25. The woods have no oasis or paving.
		{ "levels\\woods\\grass_a.png", 14 },
		{ "levels\\woods\\grass_b.png", 13 },
		{ "levels\\woods\\grass_dry.png", 14 },
		{ "levels\\woods\\dirt_path.png", 25 },
		{ "levels\\woods\\forest_floor.png", 16 },
		{ nullptr, 0 },
		{ nullptr, 0 },
		{ nullptr, 0 },
		{ nullptr, 0 },
	},
	{
		// The Orcl Act Level Designer's Desert tab's starting values (2026-10-16), until the user saves his own.
		{ "levels\\desert\\sand_a.png", 14 },
		{ "levels\\desert\\sand_ripple.png", 20 },
		{ "levels\\desert\\cracked_earth.png", 16 },
		{ "levels\\desert\\salt_road.png", 25 },
		{ "levels\\desert\\rock_shelf.png", 16 },
		{ "levels\\desert\\oasis_grass.png", 14 },
		{ "levels\\desert\\oasis_mud.png", 16 },
		{ "levels\\desert\\marsh_reeds.png", 14 },
		{ "levels\\desert\\buried_paving.png", 20 },
	},
	{
		// The Frost Caves (RfA-76, 76b, 76c, 76d), in the woods' roles: Grass the packed snow, GrassDark the drifts, GrassDry the
		// frozen gravel, Path the trodden way, Forest the rimed rock by the walls; the glacier ice and the lakes' black ice where the
		// desert has its oasis, and the Throne's carved paving. The desert's scales until the Designer's Frost tab saves its own.
		{ "levels\\frost\\snow_packed.png", 14 },
		{ "levels\\frost\\snowdrift.png", 18 },
		{ "levels\\frost\\frozen_gravel.png", 14 },
		{ "levels\\frost\\trodden_path.png", 22 },
		{ "levels\\frost\\rime_rock.png", 16 },
		{ "levels\\frost\\glacier_ice.png", 18 },
		{ "levels\\frost\\black_ice.png", 18 },
		{ nullptr, 0 },
		{ "levels\\frost\\frost_paving.png", 20 },
	},
};

/** @brief A standing picture: its file, its foot point in the painting, its scale, whether it sways in the wind, its area. */
struct TreeFile {
	const char *path;
	int footX;
	int footY;
	int percent;
	bool sway;
	OrclArea area;
};
constexpr TreeFile TreeFiles[TreeKindCount] = {
	{ nullptr, 0, 0, 0, false, OrclArea::Woods },
	// The woods' trees, each its own scale (the Testing Ground, v1.201.019).
	{ "levels\\woods\\oak_large.png", 160, 432, 29, true, OrclArea::Woods },
	{ "levels\\woods\\oak_small.png", 112, 306, 29, true, OrclArea::Woods },
	{ "levels\\woods\\pine_tall.png", 96, 464, 35, true, OrclArea::Woods },
	{ "levels\\woods\\pine_small.png", 72, 274, 28, true, OrclArea::Woods },
	{ "levels\\woods\\dead_large.png", 144, 400, 17, true, OrclArea::Woods },
	{ "levels\\woods\\dead_small.png", 96, 274, 25, true, OrclArea::Woods },
	// The desert's (RfA-66, 67, 68), at the trees' 29 %: the rocks stand still; the palms, the acacia, the thorn and the totem's
	// rags move in the wind.
	{ "levels\\desert\\hoodoo_pillar.png", 96, 368, 29, false, OrclArea::Desert },
	{ "levels\\desert\\boulder_desert.png", 96, 148, 29, false, OrclArea::Desert },
	{ "levels\\desert\\rock_outcrop.png", 160, 208, 29, false, OrclArea::Desert },
	{ "levels\\desert\\palm_dead_tall.png", 112, 432, 29, true, OrclArea::Desert },
	{ "levels\\desert\\palm_broken.png", 64, 164, 29, false, OrclArea::Desert },
	{ "levels\\desert\\acacia_dead.png", 144, 304, 29, true, OrclArea::Desert },
	{ "levels\\desert\\tree_petrified.png", 112, 336, 29, false, OrclArea::Desert },
	{ "levels\\desert\\ribcage_giant.png", 192, 236, 29, false, OrclArea::Desert },
	{ "levels\\desert\\rocks_cluster_desert.png", 80, 100, 29, false, OrclArea::Desert },
	{ "levels\\desert\\thorn_thicket.png", 88, 132, 29, true, OrclArea::Desert },
	{ "levels\\desert\\skull_giant.png", 96, 132, 29, false, OrclArea::Desert },
	{ "levels\\desert\\skulls_on_stakes.png", 64, 212, 29, false, OrclArea::Desert },
	{ "levels\\desert\\bone_totem.png", 56, 244, 29, true, OrclArea::Desert },
	{ "levels\\desert\\palm_oasis.png", 112, 432, 29, true, OrclArea::Desert },
	{ "levels\\desert\\wagon_wrecked.png", 160, 208, 29, false, OrclArea::Desert },
	{ "levels\\desert\\cargo_spill.png", 80, 116, 29, false, OrclArea::Desert },
	{ "levels\\desert\\tarp_poles.png", 96, 212, 29, true, OrclArea::Desert },
	{ "levels\\desert\\milestone.png", 40, 150, 29, false, OrclArea::Desert },
	{ "levels\\desert\\wayside_shrine.png", 64, 212, 29, false, OrclArea::Desert },
	{ "levels\\desert\\gallows.png", 96, 340, 29, false, OrclArea::Desert },
	{ "levels\\desert\\well_stone.png", 80, 180, 29, false, OrclArea::Desert },
	{ "levels\\desert\\column_broken.png", 64, 372, 29, false, OrclArea::Desert },
	{ "levels\\desert\\column_fallen.png", 160, 148, 29, false, OrclArea::Desert },
	{ "levels\\desert\\statue_head.png", 128, 212, 29, false, OrclArea::Desert },
	{ "levels\\desert\\obelisk.png", 64, 436, 29, false, OrclArea::Desert },
	{ "levels\\desert\\stele_eclipse.png", 64, 212, 29, false, OrclArea::Desert },
	{ "levels\\desert\\arch_ruined.png", 144, 340, 29, false, OrclArea::Desert },
	{ "levels\\desert\\statue_headless.png", 64, 276, 29, false, OrclArea::Desert },
	{ "levels\\desert\\brazier_stand.png", 40, 164, 29, false, OrclArea::Desert },
	{ "levels\\desert\\guardian_lion.png", 192, 276, 29, false, OrclArea::Desert },
	{ "levels\\desert\\aqueduct_arch.png", 192, 340, 29, false, OrclArea::Desert },
	{ "levels\\desert\\burial_urns.png", 72, 116, 29, false, OrclArea::Desert },
	{ "levels\\desert\\sundial_plinth.png", 64, 148, 29, false, OrclArea::Desert },
	{ "levels\\desert\\cairn.png", 56, 132, 29, false, OrclArea::Desert },
	{ "levels\\desert\\wall_stub.png", 112, 148, 29, false, OrclArea::Desert },
	{ "levels\\desert\\wall_broken.png", 112, 212, 29, false, OrclArea::Desert },
	{ "levels\\desert\\stump_charred.png", 64, 132, 29, false, OrclArea::Desert },
	{ "levels\\desert\\boulder_ash.png", 96, 148, 29, false, OrclArea::Desert },
	{ "levels\\desert\\siege_burnt.png", 192, 244, 29, false, OrclArea::Desert },
	{ "levels\\desert\\obsidian_spire.png", 80, 372, 29, false, OrclArea::Desert },
	{ "levels\\desert\\glass_shards.png", 80, 132, 29, false, OrclArea::Desert },
	{ "levels\\desert\\fulgurite.png", 64, 164, 29, false, OrclArea::Desert },
	{ "levels\\desert\\crystal_pale.png", 88, 180, 29, false, OrclArea::Desert },
	{ "levels\\desert\\corpse_glassed.png", 64, 212, 29, false, OrclArea::Desert },
	{ "levels\\desert\\tent_open.png", 160, 244, 29, false, OrclArea::Desert },
	{ "levels\\desert\\tent_collapsed.png", 144, 148, 29, false, OrclArea::Desert },
	{ "levels\\desert\\lantern_pole.png", 32, 180, 29, false, OrclArea::Desert },
	{ "levels\\desert\\waterskins_rack.png", 64, 164, 29, false, OrclArea::Desert },
	{ "levels\\desert\\salt_piles.png", 72, 84, 29, false, OrclArea::Desert },
	{ "levels\\desert\\hitching_post.png", 48, 148, 29, false, OrclArea::Desert },
	{ "levels\\desert\\pillar_black_a.png", 56, 388, 29, false, OrclArea::Desert },
	{ "levels\\desert\\pillar_black_b.png", 56, 388, 29, false, OrclArea::Desert },
	{ "levels\\desert\\pillar_black_broken.png", 56, 244, 29, false, OrclArea::Desert },
	{ "levels\\desert\\temple_front.png", 224, 436, 29, false, OrclArea::Desert },
	{ "levels\\desert\\altar_eclipse.png", 96, 148, 29, false, OrclArea::Desert },
	{ "levels\\desert\\wall_corner.png", 112, 212, 29, false, OrclArea::Desert },
	{ "levels\\desert\\wall_end.png", 80, 212, 29, false, OrclArea::Desert },
	{ "levels\\frost\\wall_mass_a.png", 128, 308, 46, false, OrclArea::Frost },
	{ "levels\\frost\\wall_mass_b.png", 128, 276, 46, false, OrclArea::Frost },
	{ "levels\\frost\\wall_low.png", 160, 212, 44, false, OrclArea::Frost },
	{ "levels\\frost\\stalagmite.png", 56, 276, 40, false, OrclArea::Frost },
	{ "levels\\frost\\stalagmites_three.png", 96, 212, 40, false, OrclArea::Frost },
	{ "levels\\frost\\boulder_ice.png", 96, 148, 36, false, OrclArea::Frost },
	{ "levels\\frost\\ice_pillar.png", 80, 372, 40, false, OrclArea::Frost },
	{ "levels\\frost\\crystal_cluster.png", 96, 244, 36, false, OrclArea::Frost },
	{ "levels\\frost\\frozen_waterfall.png", 144, 372, 42, false, OrclArea::Frost },
	{ "levels\\frost\\beast_in_ice.png", 192, 276, 40, false, OrclArea::Frost },
	{ "levels\\frost\\rocks_snow.png", 80, 100, 29, false, OrclArea::Frost },
	{ "levels\\frost\\icicle_ledge.png", 80, 164, 29, false, OrclArea::Frost },
	{ "levels\\frost\\crystals_small.png", 56, 116, 29, false, OrclArea::Frost },
	{ "levels\\frost\\ice_slab.png", 88, 132, 29, false, OrclArea::Frost },
	{ "levels\\frost\\shrub_frozen.png", 72, 148, 29, false, OrclArea::Frost },
	{ "levels\\frost\\cairn_skull.png", 56, 164, 29, false, OrclArea::Frost },
	{ "levels\\frost\\barbarian_in_ice.png", 64, 276, 29, false, OrclArea::Frost },
	{ "levels\\frost\\warrior_slumped.png", 80, 116, 29, false, OrclArea::Frost },
	{ "levels\\frost\\banner_torn.png", 48, 276, 29, false, OrclArea::Frost },
	{ "levels\\frost\\barricade_stakes.png", 128, 148, 29, false, OrclArea::Frost },
	{ "levels\\frost\\catapult_broken.png", 144, 180, 29, false, OrclArea::Frost },
	{ "levels\\frost\\crates_barrels.png", 80, 132, 29, false, OrclArea::Frost },
	{ "levels\\frost\\sled_frozen.png", 112, 116, 29, false, OrclArea::Frost },
	{ "levels\\frost\\weapon_rack.png", 80, 148, 29, false, OrclArea::Frost },
	{ "levels\\frost\\shields_spears.png", 80, 148, 29, false, OrclArea::Frost },
	{ "levels\\frost\\passage_on.png", 160, 308, 29, false, OrclArea::Frost },
	{ "levels\\frost\\ice_hole.png", 96, 100, 32, false, OrclArea::Frost },
	{ "levels\\frost\\crevasse_lip.png", 160, 148, 34, false, OrclArea::Frost },
	{ "levels\\frost\\fish_in_ice.png", 56, 84, 32, false, OrclArea::Frost },
	{ "levels\\frost\\crystal_spire.png", 64, 340, 36, false, OrclArea::Frost },
	{ "levels\\frost\\wall_faces.png", 128, 308, 46, false, OrclArea::Frost },
	{ "levels\\frost\\burial_mound.png", 96, 148, 32, false, OrclArea::Frost },
	{ "levels\\frost\\tomb_door.png", 112, 276, 40, false, OrclArea::Frost },
	{ "levels\\frost\\chains_rock.png", 64, 180, 32, false, OrclArea::Frost },
	{ "levels\\frost\\corpse_kneeling.png", 64, 180, 32, false, OrclArea::Frost },
	{ "levels\\frost\\rune_stone.png", 56, 244, 36, false, OrclArea::Frost },
	{ "levels\\frost\\cathedral_pillar.png", 80, 436, 40, false, OrclArea::Frost },
	{ "levels\\frost\\cathedral_pillar_broken.png", 80, 244, 40, false, OrclArea::Frost },
	{ "levels\\frost\\throne_frozen.png", 160, 340, 44, false, OrclArea::Frost },
	{ "levels\\frost\\arch_ice.png", 144, 340, 40, false, OrclArea::Frost },
	{ "levels\\frost\\ring_stone_a.png", 64, 212, 34, false, OrclArea::Frost },
	{ "levels\\frost\\ring_stone_b.png", 64, 212, 34, false, OrclArea::Frost },
	{ "levels\\frost\\ring_stone_c.png", 64, 212, 34, false, OrclArea::Frost },
	{ "levels\\frost\\brazier_frozen.png", 48, 180, 32, false, OrclArea::Frost },
	{ "levels\\frost\\king_statue.png", 80, 340, 40, false, OrclArea::Frost },
	{ nullptr, 0, 0, 0, false, OrclArea::Desert }, // Blocker: never drawn
};

/** @brief The desert's things that lie on the sand (DesertDecor), after the scatter sheet's cells. */
constexpr TreeFile DecorFiles[ScatterSlots - 12] = {
	{ "levels\\desert\\saltbush.png", 72, 100, 29, false, OrclArea::Desert },
	{ "levels\\desert\\dune_grass_tall.png", 72, 116, 29, false, OrclArea::Desert },
	{ "levels\\desert\\bones_spear.png", 80, 84, 29, false, OrclArea::Desert },
	{ "levels\\desert\\urn_clay.png", 40, 102, 29, false, OrclArea::Desert },
	{ "levels\\desert\\basket_woven.png", 32, 56, 29, false, OrclArea::Desert },
	{ "levels\\desert\\cart_wheel.png", 56, 70, 29, false, OrclArea::Desert },
	{ "levels\\desert\\sarcophagus_lid.png", 112, 100, 29, false, OrclArea::Desert },
	{ "levels\\desert\\campfire_burnt.png", 48, 54, 29, false, OrclArea::Desert },
	{ "levels\\desert\\bones_charred.png", 64, 70, 29, false, OrclArea::Desert },
	{ "levels\\desert\\smoke_vent.png", 48, 54, 29, false, OrclArea::Desert },
	{ "levels\\desert\\rug_pots.png", 80, 84, 29, false, OrclArea::Desert },
	{ "levels\\desert\\reed_clump.png", 48, 116, 29, true, OrclArea::Desert },
	{ "levels\\frost\\drift_rock.png", 88, 100, 29, false, OrclArea::Frost },
	{ "levels\\frost\\bones_frost.png", 80, 84, 29, false, OrclArea::Frost },
	{ "levels\\frost\\campfire_frozen.png", 64, 84, 29, false, OrclArea::Frost },
	{ "levels\\frost\\lake_stones.png", 96, 100, 32, false, OrclArea::Frost },
};

/** @brief One area's pictures, loaded the first time one of its floors is built. */
struct AreaArtSet {
	std::array<Picture, MaterialCount> ground;
	std::array<Picture, TreeKindCount> trees;
	std::array<Picture, ScatterSlots> scatter; // the sheet's twelve cells, each at its own scale, then the desert's decor
	/** @brief The desert's two ways down (RfA-71b), soft alpha in the top byte, footX/footY the middle the hero stands on. */
	std::array<Picture, 2> entrances;
	/** @brief The Glass Flats' scorched crater (RfA-72b), the same kind of ground picture. */
	Picture crater;
	/** @brief The Frost Caves' light shaft's snow heap (RfA-79b), the same kind again. */
	Picture shaft;
	bool loaded = false;
	bool tried = false;
};
std::array<AreaArtSet, OrclAreaCount> AreaArt;

/** @brief The pictures of the floor last built's area. */
AreaArtSet &Art()
{
	return AreaArt[Frost ? 2 : Desert ? 1 : 0];
}

/** @brief A PNG from the archives as 32-bit RGBA, or nullptr (logged). The caller frees it. */
SDL_Surface *LoadRgba(const char *path)
{
	SDL_Surface *png = LoadPNG(path);
	if (png == nullptr) {
		LogWarn("Woods art missing: {}", path);
		return nullptr;
	}
	SDL_Surface *rgba = SDL_ConvertSurfaceFormat(png, SDL_PIXELFORMAT_ABGR8888, 0);
	SDL_FreeSurface(png);
	return rgba;
}

/**
 * @brief The rectangle (@p rx, @p ry, @p rw, @p rh) of a loaded painting at @p percent of its size, each pixel the average of
 * the block it covers (an area resample, any ratio). A sprite pixel is opaque where at least half its block was.
 */
void ResampleRgba(const SDL_Surface *rgba, int rx, int ry, int rw, int rh, Picture &out, bool sprite, int percent)
{
	out.w = std::max(1, (rw * percent + 50) / 100);
	out.h = std::max(1, (rh * percent + 50) / 100);
	out.px.assign(static_cast<size_t>(out.w) * out.h, 0);
	const auto *bytes = static_cast<const uint8_t *>(rgba->pixels);
	for (int y = 0; y < out.h; y++) {
		const int sy0 = ry + y * rh / out.h;
		const int sy1 = std::max(sy0 + 1, ry + (y + 1) * rh / out.h);
		for (int x = 0; x < out.w; x++) {
			const int sx0 = rx + x * rw / out.w;
			const int sx1 = std::max(sx0 + 1, rx + (x + 1) * rw / out.w);
			int r = 0, g = 0, b = 0, n = 0;
			for (int sy = sy0; sy < sy1; sy++) {
				for (int sx = sx0; sx < sx1; sx++) {
					const uint8_t *q = bytes + static_cast<size_t>(sy) * rgba->pitch + static_cast<size_t>(sx) * 4;
					if (sprite && q[3] < 128)
						continue;
					r += q[0];
					g += q[1];
					b += q[2];
					n++;
				}
			}
			// The ground is always opaque.
			if (n == 0 || (sprite && 2 * n < (sx1 - sx0) * (sy1 - sy0)))
				continue;
			out.px[static_cast<size_t>(y) * out.w + x] = 0xFF000000U | (static_cast<uint32_t>(r / n) << 16) | (static_cast<uint32_t>(g / n) << 8) | static_cast<uint32_t>(b / n);
		}
	}
	out.spanFirst.assign(static_cast<size_t>(out.h), 0);
	out.spanLast.assign(static_cast<size_t>(out.h), 0);
	for (int y = 0; y < out.h; y++) {
		const uint32_t *row = &out.px[static_cast<size_t>(y) * out.w];
		int first = 0;
		while (first < out.w && (row[first] & 0xFF000000U) == 0)
			first++;
		int last = out.w;
		while (last > first && (row[last - 1] & 0xFF000000U) == 0)
			last--;
		out.spanFirst[y] = static_cast<int16_t>(first);
		out.spanLast[y] = static_cast<int16_t>(last);
	}
}

/** @brief A PNG from the archives at @p percent of its painted size (ResampleRgba over the whole of it). */
bool LoadScaled(const char *path, Picture &out, bool sprite, int percent)
{
	SDL_Surface *rgba = LoadRgba(path);
	if (rgba == nullptr)
		return false;
	ResampleRgba(rgba, 0, 0, rgba->w, rgba->h, out, sprite, percent);
	SDL_FreeSurface(rgba);
	return true;
}

/**
 * @brief The ground details, each its own scale (the Orcl Testing Ground's second save, 2026-10-14), in percent of the painting:
 * the sheet's twelve 64 px cells, 4 x 3, cut apart and each scaled on its own. The desert's at the Level Designer's 80.
 */
constexpr int ScatterPercent[OrclAreaCount][12] = {
	{ 73, 88, 72, 79, 120, 120, 65, 43, 93, 85, 120, 50 },
	{ 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80 },
	{ 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80 },
};

uint32_t ScaleRgb(uint32_t c, float kr, float kg, float kb); // below, with the desert's ground features
extern uint8_t FrostRockDepth[MAXDUNX][MAXDUNY]; // below, with the Frost Caves' generator

/** @brief A standing picture @p file into @p out at its scale, its foot point scaled with it. */
bool LoadStanding(const TreeFile &file, Picture &out)
{
	if (!LoadScaled(file.path, out, true, file.percent))
		return false;
	out.footX = file.footX * file.percent / 100;
	out.footY = file.footY * file.percent / 100;
	return true;
}

/** @brief This floor's painted way down (RfA-71b), or nullptr: the sinkhole above the Sand Warrens, the stairway above the Buried Halls. */
const Picture *DesertEntrancePicture()
{
	if (!MouthOnFloor || !OwnLook())
		return nullptr;
	// The desert's by its cave; the Frost Caves' the hole and the stairs in turn, floor by floor.
	const int which = Frost ? GenFloor % 2 : WoodsCaveOnFloor(GenFloor, OrclArea::Desert) == SL_ORCL_DESERT_CAVE_1 ? 0 : 1;
	const Picture &pic = Art().entrances[which];
	return pic.px.empty() ? nullptr : &pic;
}

/**
 * @brief The desert's two ways down, painted (RfA-71b): the Salt Road's sinkhole into the Sand Warrens and the Glass Flats' stairway
 * into the Buried Halls. Ground pictures with a soft edge: each is laid into the floor's ground at the sand hole's entrance in place
 * of the game-drawn funnel, its outer fade melting into the sand. At half the painted size, which puts the hero (150 px painted) at
 * the game's own height and the sinkhole's mouth at the game-drawn pit's width.
 */
struct EntranceFile {
	const char *path;
	int middleX;
	int middleY;
};
constexpr EntranceFile EntranceFiles[2] = {
	{ "levels\\desert\\sinkhole_warrens.png", 224, 128 },
	{ "levels\\desert\\stairs_buried_halls.png", 224, 192 },
};
/** @brief The Frost Caves' ways down to their frozen rivers (RfA-79): a hole with a rope ladder, and ice stairs - every other floor. */
constexpr EntranceFile FrostEntranceFiles[2] = {
	{ "levels\\frost\\hole_ladder.png", 224, 128 },
	{ "levels\\frost\\stairs_ice.png", 224, 192 },
};
constexpr int EntrancePercent = 50;

/**
 * @brief A soft-edged ground picture at @p percent: each pixel its block's alpha-weighted colour, its alpha the block's mean. Then
 * matched to the floor's own sand @p sand (its mean colour; the first in-game view, 2026-10-16: the painting's sand_a showed as a
 * paler rectangle on the game's sand) - the whole picture tinted so its outer sand has the floor's colour - and faded out in an oval
 * well inside its edge, so what is left of its own sand melts into the floor's.
 */
bool LoadDecal(const EntranceFile &file, Picture &out, int percent, const std::array<double, 3> &sand)
{
	SDL_Surface *rgba = LoadRgba(file.path);
	if (rgba == nullptr)
		return false;
	out.w = std::max(1, (rgba->w * percent + 50) / 100);
	out.h = std::max(1, (rgba->h * percent + 50) / 100);
	out.px.assign(static_cast<size_t>(out.w) * out.h, 0);
	const auto *bytes = static_cast<const uint8_t *>(rgba->pixels);
	for (int y = 0; y < out.h; y++) {
		const int sy0 = y * rgba->h / out.h;
		const int sy1 = std::max(sy0 + 1, (y + 1) * rgba->h / out.h);
		for (int x = 0; x < out.w; x++) {
			const int sx0 = x * rgba->w / out.w;
			const int sx1 = std::max(sx0 + 1, (x + 1) * rgba->w / out.w);
			int r = 0, g = 0, bl = 0, a = 0, n = 0;
			for (int sy = sy0; sy < sy1; sy++) {
				for (int sx = sx0; sx < sx1; sx++) {
					const uint8_t *q = bytes + static_cast<size_t>(sy) * rgba->pitch + static_cast<size_t>(sx) * 4;
					r += q[0] * q[3];
					g += q[1] * q[3];
					bl += q[2] * q[3];
					a += q[3];
					n++;
				}
			}
			if (a == 0)
				continue;
			out.px[static_cast<size_t>(y) * out.w + x] = (static_cast<uint32_t>(a / n) << 24) | (static_cast<uint32_t>(r / a) << 16) | (static_cast<uint32_t>(g / a) << 8)
			    | static_cast<uint32_t>(bl / a);
		}
	}
	SDL_FreeSurface(rgba);
	out.footX = file.middleX * percent / 100;
	out.footY = file.middleY * percent / 100;
	const float cx = static_cast<float>(out.w) * 0.5F, cy = static_cast<float>(out.h) * 0.5F;
	const auto oval = [&](int x, int y) {
		const float dx = (static_cast<float>(x) + 0.5F - cx) / cx, dy = (static_cast<float>(y) + 0.5F - cy) / cy;
		return std::sqrt(dx * dx + dy * dy);
	};
	// The painting's own sand: its opaque pixels in the band between the entrance and the fade.
	std::array<double, 3> own { 0, 0, 0 };
	int count = 0;
	for (int y = 0; y < out.h; y++)
		for (int x = 0; x < out.w; x++) {
			const uint32_t c = out.px[static_cast<size_t>(y) * out.w + x];
			const float r = oval(x, y);
			if ((c >> 24) == 255 && r >= 0.62F && r < 0.80F) {
				own[0] += (c >> 16) & 0xFF;
				own[1] += (c >> 8) & 0xFF;
				own[2] += c & 0xFF;
				count++;
			}
		}
	float k[3] = { 1.0F, 1.0F, 1.0F };
	if (count > 0 && sand[0] > 1.0)
		for (int i = 0; i < 3; i++)
			k[i] = std::clamp(static_cast<float>(sand[i] * count / std::max(own[i], 1.0)), 0.6F, 1.6F);
	for (int y = 0; y < out.h; y++)
		for (int x = 0; x < out.w; x++) {
			uint32_t &c = out.px[static_cast<size_t>(y) * out.w + x];
			const float t = std::clamp((0.86F - oval(x, y)) / 0.30F, 0.0F, 1.0F);
			const float fade = t * t * (3.0F - 2.0F * t);
			const uint32_t a = static_cast<uint32_t>(static_cast<float>(c >> 24) * fade);
			c = (a << 24) | ScaleRgb(c, k[0], k[1], k[2]);
		}
	return true;
}

/** @brief The pictures of the floor last built's area, the first time one of its floors is built. */
void LoadArt()
{
	AreaArtSet &art = Art();
	if (art.tried)
		return;
	art.tried = true;
	const OrclArea area = Frost ? OrclArea::Frost : Desert ? OrclArea::Desert : OrclArea::Woods;
	const int a = AreaIndex(area);
	bool ok = true;
	for (int m = 0; m < MaterialCount; m++) {
		if (GroundFiles[a][m].path != nullptr)
			ok = LoadScaled(GroundFiles[a][m].path, art.ground[m], false, GroundFiles[a][m].percent) && ok;
	}
	// A standing picture that fails to load is left out (DrawWoodsTileSprite skips an empty one; LoadRgba logs it): the floor keeps
	// its painted look. Only a missing ground or scatter sheet falls back (audit, 2026-10-16).
	for (int t = 1; t < TreeKindCount; t++) {
		if (TreeFiles[t].path != nullptr && TreeFiles[t].area == area)
			LoadStanding(TreeFiles[t], art.trees[t]);
	}
	if (SDL_Surface *sheet = LoadRgba(Frost ? "levels\\frost\\scatter_sheet.png" : Desert ? "levels\\desert\\scatter_sheet.png" : "levels\\woods\\scatter_sheet.png"); sheet != nullptr) {
		const int cell = sheet->w / 4;
		for (int c = 0; c < 12; c++) {
			ResampleRgba(sheet, (c % 4) * cell, (c / 4) * cell, cell, cell, art.scatter[c], true, ScatterPercent[a][c]);
			// Each stands at its cell's bottom centre, as on the sheet (32, 56 of 64).
			art.scatter[c].footX = art.scatter[c].w / 2;
			art.scatter[c].footY = art.scatter[c].h * 7 / 8;
		}
		SDL_FreeSurface(sheet);
	} else {
		ok = false;
	}
	if (OwnLook()) {
		// The area's own lying things (each area's decor slots its own).
		for (int d = 0; d < ScatterSlots - 12; d++)
			if (DecorFiles[d].area == area)
				LoadStanding(DecorFiles[d], art.scatter[12 + d]);
		// A way down that fails to load leaves the game-drawn funnel in its place.
		std::array<double, 3> sandRgb { 0, 0, 0 };
		for (const uint32_t c : art.ground[Grass].px) {
			sandRgb[0] += (c >> 16) & 0xFF;
			sandRgb[1] += (c >> 8) & 0xFF;
			sandRgb[2] += c & 0xFF;
		}
		for (double &ch : sandRgb)
			ch /= static_cast<double>(std::max<size_t>(art.ground[Grass].px.size(), 1));
		for (int e = 0; e < 2; e++)
			LoadDecal(Frost ? FrostEntranceFiles[e] : EntranceFiles[e], art.entrances[e], EntrancePercent, sandRgb);
		if (Desert) {
			constexpr EntranceFile CraterFile { "levels\\desert\\crater_scorched.png", 128, 64 };
			LoadDecal(CraterFile, art.crater, 40, sandRgb);
		}
		if (Frost) {
			// 256 x 256, its middle at (128, 160); at 40 % about a tile and a half across. Missing (RfA-79b not yet delivered), the
			// shafts keep only their light.
			constexpr EntranceFile ShaftFile { "levels\\frost\\light_shaft.png", 128, 160 };
			LoadDecal(ShaftFile, art.shaft, 40, sandRgb);
		}
		// Each ground near the sand's brightness (the first preview, 2026-10-16: the bedrock lay on the sand as dark slabs, and where
		// two grounds of such different light meet, the tile grid shows as straight lines): the salt road a little lighter so it
		// reads as a road, the mud darker as wet ground is, the rest within a few percent of the sand.
		constexpr float Target[MaterialCount] = { 1.00F, 1.00F, 0.96F, 1.14F, 0.93F, 0.95F, 0.72F, 0.90F, 1.03F };
		const auto meanLight = [](const Picture &pic) {
			double sum = 0;
			for (const uint32_t c : pic.px)
				sum += 0.30 * ((c >> 16) & 0xFF) + 0.59 * ((c >> 8) & 0xFF) + 0.11 * (c & 0xFF);
			return pic.px.empty() ? 0.0 : sum / static_cast<double>(pic.px.size());
		};
		const double sand = meanLight(art.ground[Grass]);
		for (int m = 1; m < MaterialCount && sand > 1.0; m++) {
			Picture &pic = art.ground[m];
			const double mean = meanLight(pic);
			if (mean <= 1.0)
				continue;
			const float k = std::clamp(static_cast<float>(sand * Target[m] / mean), 0.6F, 1.8F);
			for (uint32_t &c : pic.px)
				c = (c & 0xFF000000U) | ScaleRgb(c, k, k, k);
		}
	}
	art.loaded = ok;
}

/** @brief A world-pinned table of small offsets, in tiles, that makes the edges between materials irregular. */
constexpr int NoiseSize = 64;
std::array<std::array<int8_t, 2>, NoiseSize * NoiseSize> EdgeNoise {};
bool EdgeNoiseBuilt = false;

void BuildEdgeNoise()
{
	if (EdgeNoiseBuilt)
		return;
	EdgeNoiseBuilt = true;
	for (int y = 0; y < NoiseSize; y++) {
		for (int x = 0; x < NoiseSize; x++) {
			// Tileable: sampled on a torus of the value noise's period.
			const float a = ValueNoise(x, y, 8.0F, 0xED6EU) + 0.5F * ValueNoise(x, y, 3.0F, 0x5EEDU);
			const float b = ValueNoise(x + 101, y + 37, 8.0F, 0xED6FU) + 0.5F * ValueNoise(x + 13, y + 71, 3.0F, 0x5EEEU);
			EdgeNoise[static_cast<size_t>(y) * NoiseSize + x] = { static_cast<int8_t>((a / 1.5F - 0.5F) * 110), static_cast<int8_t>((b / 1.5F - 0.5F) * 110) };
		}
	}
}

/** @brief @p c at @p brightness of 256: red and blue in one multiply, green in another, never pure black (black is "nothing"). */
inline uint32_t Shade(uint32_t c, int brightness)
{
	const uint32_t b = static_cast<uint32_t>(brightness);
	return ((((c & 0xFF00FFU) * b) >> 8) & 0xFF00FFU) | ((((c & 0x00FF00U) * b) >> 8) & 0x00FF00U) | 0x010101U;
}

int BrightnessFor(int light)
{
	if (light < 0)
		return 256; // full brightness: the previews, which have no light tables
	if (light >= LightsMax)
		return 0;
	return SmoothLevelBrightness(static_cast<uint8_t>(std::max(light, 0)));
}

} // namespace

// ---------------------------------------------------------------- the game's calls

void ResetWoodsForNewGame()
{
	Game = {};
	EnterAtWayOn = false;
	EnterAtCaveMouth = false;
	ResetWoodsCavesForNewGame();
}

namespace {

/**
 * Tristram's own tiles for the woods (user, 2026-10-13: "generate Woods level 1 using vanilla assets ... Using tristram
 * textures"), found by OracoolPreview.DISABLED_TownSceneryMegatiles: the megatiles the town's sectors use past its walk
 * limit, and its grass. Ids as FillTile takes them (1-based).
 */
constexpr int VanillaGrass[] = { 1, 2, 3, 4 };
constexpr int VanillaWornGrass[] = { 5, 6, 7, 8 }; // grass with patches of bare earth
constexpr int VanillaDirt[] = { 306, 307 };
constexpr int VanillaBushes[] = { 27, 28, 29, 30 };
constexpr int VanillaSmallRocks[] = { 57, 58, 59, 60 };
constexpr int VanillaTrees[] = { 34, 41, 44, 47 };
constexpr int VanillaBigRocks[] = { 61, 62 };
constexpr int VanillaRockMass = 63; // all four pieces solid: the forest's edge

template <size_t N>
int Pick(const int (&from)[N], uint32_t h)
{
	return from[h % N];
}

/** @brief Tristram's tree-leaf overlays for the pieces that carry them (town.cpp InitTownPieces). */
void VanillaLeaves()
{
	constexpr std::pair<uint16_t, int8_t> Leaves[] = { { 359, 1 }, { 357, 2 }, { 128, 6 }, { 129, 7 }, { 127, 8 }, { 116, 9 }, { 156, 10 },
		{ 157, 11 }, { 155, 12 }, { 161, 13 }, { 159, 14 }, { 213, 15 }, { 211, 16 }, { 216, 17 }, { 215, 18 } };
	for (int y = 0; y < MAXDUNY; y++) {
		for (int x = 0; x < MAXDUNX; x++) {
			for (const auto &[piece, special] : Leaves) {
				if (dPiece[x][y] == piece) {
					dSpecial[x][y] = special;
					break;
				}
			}
		}
	}
}

/**
 * @brief The floor from Tristram's tiles: each megatile of the 40 x 40 dungeon from the woods' own map of it - the road in
 * dirt edged with worn grass, the forest's edge in the rock mass, groves in Tristram's trees, bushes and rocks, open ground in
 * grass with a few bushes and stones - and the rock mass all round outside.
 */
void BuildVanilla(uint32_t seed, bool paintedTrees)
{
	for (int y = 0; y < MAXDUNY; y += 2) {
		for (int x = 0; x < MAXDUNX; x += 2) {
			if (x < Lo || y < Lo || x > Hi || y > Hi) {
				FillMegatile(x, y, VanillaRockMass);
				if (paintedTrees && pMegaTiles != nullptr) {
					// The Mixed look: the forest beyond the map stands on grass, as inside - its trees are drawn one in four
					// (TreeHidden), and between them the rock mass showed as dark blue stone (preview, 2026-10-14). Still solid.
					const MegaTile grass = pMegaTiles[Pick(VanillaGrass, Hash(static_cast<uint32_t>(x), static_cast<uint32_t>(y), seed)) - 1];
					WoodsLookPiece[x][y] = SDL_SwapLE16(grass.micro1);
					WoodsLookPiece[x + 1][y] = SDL_SwapLE16(grass.micro2);
					WoodsLookPiece[x][y + 1] = SDL_SwapLE16(grass.micro3);
					WoodsLookPiece[x + 1][y + 1] = SDL_SwapLE16(grass.micro4);
				}
			}
		}
	}
	const auto cellHasPath = [](int mx, int my) {
		if (mx < 0 || my < 0 || mx >= WoodsCells || my >= WoodsCells)
			return false;
		for (int t = 0; t < 4; t++) {
			const int x = Lo + 2 * mx + (t & 1);
			const int y = Lo + 2 * my + (t >> 1);
			if (Tree[x][y] == NoTree && Ground[x][y] == Path)
				return true;
		}
		return false;
	};
	for (int my = 0; my < WoodsCells; my++) {
		for (int mx = 0; mx < WoodsCells; mx++) {
			int trees = 0;
			int dry = 0;
			for (int t = 0; t < 4; t++) {
				const int x = Lo + 2 * mx + (t & 1);
				const int y = Lo + 2 * my + (t >> 1);
				trees += Tree[x][y] != NoTree ? 1 : 0;
				dry += Ground[x][y] == GrassDry ? 1 : 0;
			}
			const uint32_t h = Hash(static_cast<uint32_t>(mx), static_cast<uint32_t>(my), seed ^ 0x7A11U);
			const int edge = std::min({ mx, my, WoodsCells - 1 - mx, WoodsCells - 1 - my });
			const float u = Unit(h);
			int mega;
			if (paintedTrees) {
				// The Mixed look: Tristram's grass only, worn by the road and in the dry patches. The road itself is the painted
				// dirt, drawn over it (DrawWoodsMixedGround); the trees and the ground details are the painted ones.
				const bool byPath = cellHasPath(mx, my) || cellHasPath(mx + 1, my) || cellHasPath(mx - 1, my) || cellHasPath(mx, my + 1) || cellHasPath(mx, my - 1);
				mega = byPath || dry >= 2 ? Pick(VanillaWornGrass, h >> 8) : Pick(VanillaGrass, h >> 8);
			} else if (cellHasPath(mx, my)) {
				mega = Pick(VanillaDirt, h >> 8);
			} else if (trees >= 3) {
				if (edge <= 1)
					mega = u < 0.85F ? VanillaRockMass : Pick(VanillaBigRocks, h >> 8);
				else
					mega = u < 0.55F ? Pick(VanillaTrees, h >> 8) : u < 0.75F ? Pick(VanillaBushes, h >> 8) : u < 0.90F ? Pick(VanillaBigRocks, h >> 8) : VanillaRockMass;
			} else if (trees >= 1) {
				mega = u < 0.55F ? Pick(VanillaTrees, h >> 8) : Pick(VanillaBushes, h >> 8);
			} else {
				const bool byPath = cellHasPath(mx + 1, my) || cellHasPath(mx - 1, my) || cellHasPath(mx, my + 1) || cellHasPath(mx, my - 1);
				mega = byPath || dry >= 2 ? Pick(VanillaWornGrass, h >> 8) : Pick(VanillaGrass, h >> 8);
				const float scatter = Unit(h >> 4);
				if (scatter < 0.035F)
					mega = Pick(VanillaSmallRocks, h >> 12);
				else if (scatter < 0.085F)
					mega = Pick(VanillaBushes, h >> 12);
			}
			// The minimap marks the forest as Tristram's rock mass, whichever way it is drawn.
			MapCell[mx][my] = static_cast<uint8_t>(std::min(paintedTrees && trees >= 3 ? VanillaRockMass : mega, 255));
			FillMegatile(Lo + 2 * mx, Lo + 2 * my, mega);
			if (paintedTrees) {
				// A painted tree's tile blocks as a trunk does, and is drawn as the grass it stands on.
				for (int t = 0; t < 4; t++) {
					const int x = Lo + 2 * mx + (t & 1);
					const int y = Lo + 2 * my + (t >> 1);
					if (Tree[x][y] == NoTree)
						continue;
					WoodsLookPiece[x][y] = dPiece[x][y];
					dPiece[x][y] = SolidPiece;
				}
			}
		}
	}
	if (!paintedTrees)
		VanillaLeaves();
}

} // namespace

bool WoodsPaintedArtChosen()
{
	return *sgOptions.Oracool.woodsPaintedArt;
}

void CreateWoods(uint32_t seed, lvl_entry /*entry*/)
{
	Desert = setlevel && IsDesertLevel(setlvlnum);
	Frost = setlevel && IsFrostLevel(setlvlnum);
	GenFloor = FloorIndex(setlvlnum);
	// The tile data before the generator: it picks its solid piece from it (LoadGameLevel loads it after LoadSetMap).
	LoadLevelSOLData();
	FindSolidPiece();
	Generate(seed);
	// The desert has no Tristram look: its floors are always the painted ones.
	BuiltVanilla = !OwnLook() && !WoodsPaintedArtChosen();
	BuiltMixed = BuiltVanilla && *sgOptions.Oracool.woodsPaintedTrees;
	std::memset(WoodsLookPiece, 0, sizeof(WoodsLookPiece));
	if (!BuiltMixed && BuiltVanilla) {
		// Tristram's trees: the painted trees' map is not used for drawing, so it holds nothing.
		std::memset(Scatter, 0, sizeof(Scatter));
	}
	if (BuiltVanilla) {
		BuildVanilla(seed, BuiltMixed);
		if (BuiltMixed) {
			// The painted ground details only where Tristram's ground is open, and half as many: they are a second voice here.
			for (int y = 0; y < MAXDUNY; y++)
				for (int x = 0; x < MAXDUNX; x++)
					if (Scatter[x][y] != 0 && (TileHasAny(dPiece[x][y], TileProperties::Solid) || (Hash(static_cast<uint32_t>(x), static_cast<uint32_t>(y), seed) & 1) != 0))
						Scatter[x][y] = 0;
		}
		std::memset(dLight, 3, sizeof(dLight));
		ViewPosition = Arrival;
		return;
	}
	for (int y = 0; y < MAXDUNY; y += 2) {
		for (int x = 0; x < MAXDUNX; x += 2)
			FillMegatile(x, y, 1 + static_cast<int>(Hash(static_cast<uint32_t>(x), static_cast<uint32_t>(y), seed) % GrassMegatiles));
	}
	for (int y = 0; y < MAXDUNY; y++) {
		for (int x = 0; x < MAXDUNX; x++) {
			// Outside the box too, a tree or none: the road ends' clearings reach the map's edge since the 100 x 100 floors, and
			// the hero walked out past the way's strip there (audit round 1).
			if (Tree[x][y] != NoTree || !Inside({ x, y }))
				dPiece[x][y] = SolidPiece;
		}
	}
	// The minimap's map (automap.cpp reads MapCell through WoodsAutomapCell): each megatile 1 open ground, 2 the forest's edge, 3 road, 4 deep
	// forest. Before this the woods kept Tristram's dungeon[][] and the minimap drew the town (the play test, 2026-10-13).
	for (int my = 0; my < WoodsCells; my++) {
		for (int mx = 0; mx < WoodsCells; mx++) {
			int trees = 0;
			bool road = false;
			for (int t = 0; t < 4; t++) {
				const int x = Lo + 2 * mx + (t & 1);
				const int y = Lo + 2 * my + (t >> 1);
				trees += Tree[x][y] != NoTree ? 1 : 0;
				road = road || (Tree[x][y] == NoTree && Ground[x][y] == Path);
			}
			uint8_t kind = trees >= 3 ? 4 : road ? 3 : 1;
			if (kind == 4) {
				// The edge: deep forest beside open ground.
				for (int d = 0; d < 4 && kind == 4; d++) {
					const int nx = mx + (d == 0 ? 1 : d == 1 ? -1 : 0);
					const int ny = my + (d == 2 ? 1 : d == 3 ? -1 : 0);
					if (nx < 0 || ny < 0 || nx >= WoodsCells || ny >= WoodsCells)
						continue;
					int open = 0;
					for (int t = 0; t < 4; t++)
						open += Tree[Lo + 2 * nx + (t & 1)][Lo + 2 * ny + (t >> 1)] == NoTree ? 1 : 0;
					if (open >= 2)
						kind = 2;
				}
			}
			MapCell[mx][my] = kind;
		}
	}
	// Outdoors under the sky: the ground is lit all round, not dark beyond the hero's torch (Tristram's light is 0). Underground,
	// the Frost Caves keep a dungeon's dark: the hero's torch and their own lights (BuildWoodsLevel) show the way.
	if (!Frost)
		std::memset(dLight, 3, sizeof(dLight));
	ViewPosition = Arrival;
}


// ---------------------------------------------------------------- the world along the roads

/** @brief objects.cpp's: loads object sheet @p ofile for this level if it is not loaded yet. */
void EnsureObjectGraphicsLoaded(object_graphic_id ofile, uint16_t animWidth);

namespace {

/**
 * @brief What stands along the woods' roads (user, 2026-10-14: "we need to introduce health shrines, mana shrines, lanterns along the
 * roads, and other world enriching assets in the woods act", then "go with your first and second batches of suggestion. no new
 * assets for now"): vanilla objects - the Blood Fountain, the Purifying Spring, a goat shrine or a cauldron, sarcophagi, chests,
 * barrels, the headless dead, the tortured, a skull pile, the burning cross - and what the game draws itself: will-o'-wisps
 * that lead off the road to a chest, a fairy ring, the cottages' lamps by night. Planned from the floor's own generation on
 * every build, so a revisit finds its wisps and its ring where its saved objects stand; the objects are added once, fresh.
 */
struct WorldPiece {
	_object_id type;
	Point at;
};
std::vector<WorldPiece> WorldPieces;
struct Wisp {
	Point road;
	Point chest;
	float phase;
};
std::array<Wisp, 2> Wisps;
int WispCount = 0;
bool HasFairyRing = false;
/** @brief The candle stands by the cottages' doors: lit by night (WoodsNightLamps). */
std::vector<Point> CandleStands;
bool Taken[MAXDUNX][MAXDUNY];
bool InFairyRingBefore = false;
uint32_t FairyRingTicks = 0;

bool InHouseBounds(Point p, int margin)
{
	for (int i = 0; i < HouseCount; i++) {
		const House &house = Houses[i];
		if (p.x >= Lo + 2 * house.x0 - margin && p.x <= Lo + 2 * house.x1 + 1 + margin && p.y >= Lo + 2 * house.y0 - margin
		    && p.y <= Lo + 2 * house.y1 + 1 + margin)
			return true;
	}
	return false;
}

/** @brief A tile an object may stand on: open ground off the road, the river, the mouth and the houses, not in anything's way. */
bool WorldLoose(Point p)
{
	if (!InDungeonBounds(p) || !Inside(p) || Taken[p.x][p.y])
		return false;
	if (TileHasAny(dPiece[p.x][p.y], TileProperties::Solid) || Tree[p.x][p.y] != NoTree || InRiver(p) || InMouth(p))
		return false;
	return true;
}

/** @brief And with room round it: the eight tiles about it walkable, the roads' ends, the waypoint and the mouth kept clear. */
bool WorldOpen(Point p)
{
	if (!WorldLoose(p) || Ground[p.x][p.y] == Path || InHouseBounds(p, 1))
		return false;
	for (const Point keep : { Arrival, WayOn, Arrival + Displacement { -2, -2 } }) {
		if (p.WalkingDistance(keep) < 4)
			return false;
	}
	if (MouthOnFloor && p.WalkingDistance(MouthEntrance) < 5)
		return false;
	if (HasFairyRing && p.WalkingDistance(GladeCentre) < 5)
		return false;
	// Nor on a light shaft's snow, nor in the Throne of Winter's hall (audit, 2026-10-17).
	if (std::any_of(LightShafts.begin(), LightShafts.end(), [&](Point c) { return c.WalkingDistance(p) <= 2; }))
		return false;
	if (ArenaCentre != Point {} && p.WalkingDistance(ArenaCentre) <= 6)
		return false; // the Throne of Winter's hall, the Sun-Eater's arena
	for (int dy = -1; dy <= 1; dy++) {
		for (int dx = -1; dx <= 1; dx++) {
			const Point q = p + Displacement { dx, dy };
			if (!InDungeonBounds(q) || TileHasAny(dPiece[q.x][q.y], TileProperties::Solid))
				return false;
		}
	}
	return true;
}

void Take(Point p, int radius)
{
	for (int dy = -radius; dy <= radius; dy++)
		for (int dx = -radius; dx <= radius; dx++)
			if (InDungeonBounds(p + Displacement { dx, dy }))
				Taken[p.x + dx][p.y + dy] = true;
}

/** @brief The first tile @p minD..@p maxD from @p from (the ring of each, from a place @p h picks) that @p ok takes. */
template <typename Ok>
std::optional<Point> NearOff(Point from, int minD, int maxD, uint32_t h, Ok ok)
{
	for (int d = minD; d <= maxD; d++) {
		std::vector<Point> ring;
		for (int dy = -d; dy <= d; dy++)
			for (int dx = -d; dx <= d; dx++)
				if (std::max(std::abs(dx), std::abs(dy)) == d)
					ring.push_back(from + Displacement { dx, dy });
		const size_t start = h % ring.size();
		for (size_t k = 0; k < ring.size(); k++) {
			const Point p = ring[(start + k) % ring.size()];
			if (ok(p))
				return p;
		}
	}
	return std::nullopt;
}

void Put(_object_id type, std::optional<Point> at, int spacing = 2)
{
	if (!at)
		return;
	WorldPieces.push_back({ type, *at });
	Take(*at, spacing);
}

void PlanWoodsWorld(uint32_t seed, int floor)
{
	WorldPieces.clear();
	CandleStands.clear();
	WispCount = 0;
	std::memset(Taken, 0, sizeof(Taken));
	// The road, from the arrival to the way on and the glade's path: its tiles by their distance from the arrival.
	std::vector<Point> road;
	for (int y = Lo; y <= Hi; y++)
		for (int x = Lo; x <= Hi; x++)
			if (Ground[x][y] == Path && Tree[x][y] == NoTree && !InRiver({ x, y }))
				road.push_back({ x, y });
	std::sort(road.begin(), road.end(), [](Point a, Point b) {
		const int da = a.WalkingDistance(Arrival), db = b.WalkingDistance(Arrival);
		return da != db ? da < db : (a.y != b.y ? a.y < b.y : a.x < b.x);
	});
	const auto along = [&](int percent) { return road[std::min(road.size() - 1, road.size() * static_cast<size_t>(percent) / 100)]; };
	const auto open = [](Point p) { return WorldOpen(p); };
	// The fairy ring in the glade's middle, where nothing else stands (the woods' alone).
	HasFairyRing = !OwnLook() && WorldLoose(GladeCentre) && !TileHasAny(dPiece[GladeCentre.x][GladeCentre.y], TileProperties::Solid);
	if (HasFairyRing)
		Take(GladeCentre, 3);
	if (road.empty())
		return;
	// The health shrine by the road a third of the way along; the mana shrine by the river, or two thirds along.
	Put(OBJ_BLOODFTN, NearOff(along(35), 2, 5, Hash(seed, 1, 0xF0), open), 3);
	std::optional<Point> spring;
	{
		std::vector<Point> bank;
		for (int y = Lo; y <= Hi; y++)
			for (int x = Lo; x <= Hi; x++) {
				if (!InRiver({ x, y }))
					continue;
				for (const Displacement d : { Displacement { 2, 0 }, Displacement { -2, 0 }, Displacement { 0, 2 }, Displacement { 0, -2 } })
					bank.push_back(Point { x, y } + d);
			}
		// The desert's spring by its oasis: round the pool, past its mud.
		if (OasisRadius > 0.0F) {
			for (int k = 0; k < 16; k++) {
				const float a = 6.2831853F * static_cast<float>(k) / 16.0F;
				bank.push_back(OasisCentre + Displacement { static_cast<int>(std::lround(std::cos(a) * 5.5F)), static_cast<int>(std::lround(std::sin(a) * 5.5F)) });
			}
		}
		const size_t start = bank.empty() ? 0 : Hash(seed, 2, 0xF1) % bank.size();
		for (size_t k = 0; k < bank.size() && !spring; k++)
			if (WorldOpen(bank[(start + k) % bank.size()]))
				spring = bank[(start + k) % bank.size()];
		if (!spring)
			spring = NearOff(along(65), 3, 6, Hash(seed, 3, 0xF2), open);
	}
	Put(OBJ_PURIFYINGFTN, spring, 3);
	// A shrine in the first pen - the goat shrine on even floors, the cauldron on odd - or near the glade.
	const _object_id shrine = floor % 2 == 0 ? OBJ_GOATSHRINE : OBJ_CAULDRON;
	if (PenCount > 0 && WorldLoose(Pens[0].centre))
		Put(shrine, Pens[0].centre);
	else
		Put(shrine, NearOff(GladeCentre, 5, 8, Hash(seed, 4, 0xF3), open));
	// The second pen a little graveyard below the first floor, a chest's otherwise; the others a chest each.
	const _object_id chest = floor <= 1 ? OBJ_CHEST1 : floor == 2 ? OBJ_CHEST2 : OBJ_CHEST3;
	// The desert has no pens: a chest in its ruins, one by its oasis.
	if (RuinsCentre != Point {})
		Put(chest, NearOff(RuinsCentre, 2, 4, Hash(seed, 15, 0xF4), open));
	if (OasisRadius > 0.0F)
		Put(chest, NearOff(OasisCentre, 5, 7, Hash(seed, 16, 0xF5), open));
	for (int i = 1; i < PenCount; i++) {
		const Point c = Pens[i].centre;
		if (i == 1 && floor >= 1) {
			// One, in the middle: a second one east of it stood in the fence's column, and in its gate on an east-gated pen
			// (audit, 2026-10-14). It is two tiles tall, so its tile and the one north of it are taken.
			Put(OBJ_SARC, WorldLoose(c) ? std::optional<Point>(c) : std::nullopt, 1);
		} else if (WorldLoose(c)) {
			Put(chest, c);
		}
	}
	// Each cottage: a candle stand either side of its door, a barrel near it.
	for (int i = 0; i < HouseCount; i++) {
		const House &house = Houses[i];
		const int doorX = Lo + 2 * house.x1 + 1;
		const int archY0 = Lo + 2 * house.doorY;
		for (const Point candle : { Point { doorX, archY0 - 1 }, Point { doorX, archY0 + 2 } }) {
			if (WorldLoose(candle)) {
				Put(OBJ_CANDLE2, candle, 0);
				CandleStands.push_back(candle);
			}
		}
		Put(Hash(seed, 5, static_cast<uint32_t>(i)) % 4 == 0 ? OBJ_BARRELEX : OBJ_BARREL, NearOff({ doorX, archY0 }, 3, 5, Hash(seed, 6, static_cast<uint32_t>(i)), open));
	}
	// Barrels by the road, one in four of them the kind that bursts; the desert's are clay urns (Hellfire's, where it has them).
	const bool urns = Desert && gbIsHellfire;
	for (const int percent : { 20, 50, 80 }) {
		const bool bursts = Hash(seed, 7, static_cast<uint32_t>(percent)) % 4 == 0;
		Put(urns ? (bursts ? OBJ_URNEX : OBJ_URN) : (bursts ? OBJ_BARRELEX : OBJ_BARREL), NearOff(along(percent), 2, 3, Hash(seed, 8, static_cast<uint32_t>(percent)), open));
	}
	// The headless dead by the road, to be searched.
	for (const int percent : { 15, 75 })
		Put(OBJ_DECAP, NearOff(along(percent), 2, 3, Hash(seed, 9, static_cast<uint32_t>(percent)), open));
	// The grim ones at the gates: the tortured by the way on, a skull pile by the way back.
	const _object_id tortured[] = { OBJ_TNUDEM1, OBJ_TNUDEM2, OBJ_TNUDEM3, OBJ_TNUDEM4 };
	for (uint32_t k = 0; k < 2; k++)
		Put(tortured[Hash(seed, 10, k) % 4], NearOff(WayOn + Displacement { 5, 0 }, 2, 5, Hash(seed, 11, k), open));
	Put(OBJ_SKPILE, NearOff(Arrival + Displacement { -5, 0 }, 2, 5, Hash(seed, 12, 0), open));
	// And on the last floor, at the end of the path, the burning cross.
	// Its burning tile (the one north of it, UpdateBurningCrossDamage) off the road, where the hero walks.
	if (floor == WoodsFloors - 1) {
		const auto crossOk = [](Point p) {
			const Point burn = p + Displacement { 0, -1 };
			return WorldOpen(p) && InDungeonBounds(burn) && Ground[burn.x][burn.y] != Path;
		};
		std::optional<Point> cross = NearOff(WayOn + Displacement { 3, 0 }, 2, 6, Hash(seed, 13, 0), crossOk);
		if (!cross)
			cross = NearOff(WayOn + Displacement { 3, 0 }, 2, 12, Hash(seed, 13, 1), crossOk); // the arena took the near room (audit round 3)
		Put(OBJ_BCROSS, cross, 2);
	}
	// Two will-o'-wisps, each over a chest in a pocket of the forest six to ten tiles off the road (the woods' alone).
	for (const int percent : { 30, 70 }) {
		if (OwnLook() || WispCount >= static_cast<int>(Wisps.size()))
			break;
		const Point from = along(percent);
		const uint32_t h = Hash(seed, 14, static_cast<uint32_t>(percent));
		std::optional<Point> pocket = NearOff(from, 6, 10, h, [](Point p) { return WorldLoose(p) && Ground[p.x][p.y] == Forest && !InHouseBounds(p, 2); });
		if (!pocket)
			pocket = NearOff(from, 6, 10, h, open);
		if (!pocket)
			continue;
		Put(OBJ_CHEST2, pocket, 2);
		Wisps[WispCount++] = { from, *pocket, static_cast<float>(h % 1000) / 1000.0F };
	}
}

void EnsureWoodsWorldGraphics()
{
	for (const _object_id type : { OBJ_BLOODFTN, OBJ_PURIFYINGFTN, OBJ_GOATSHRINE, OBJ_CAULDRON, OBJ_SARC, OBJ_CHEST1, OBJ_CHEST2, OBJ_CHEST3,
	         OBJ_CANDLE2, OBJ_BARREL, OBJ_BARRELEX, OBJ_DECAP, OBJ_TNUDEM1, OBJ_SKPILE, OBJ_BCROSS })
		EnsureObjectGraphicsLoaded(AllObjects[type].ofindex, AllObjects[type].animWidth);
	if (Desert && gbIsHellfire) {
		for (const _object_id type : { OBJ_URN, OBJ_URNEX })
			EnsureObjectGraphicsLoaded(AllObjects[type].ofindex, AllObjects[type].animWidth);
	}
}

void PlaceWoodsWorldObjects()
{
	for (const WorldPiece &piece : WorldPieces) {
		if (dObject[piece.at.x][piece.at.y] != 0 || TileHasAny(dPiece[piece.at.x][piece.at.y], TileProperties::Solid))
			continue;
		AddObject(piece.type, piece.at);
	}
}

/** @brief A wisp's place now, in tiles: drifting between the road and its chest and back, swaying as it goes. */
void WispAt(const Wisp &wisp, float &x, float &y)
{
	const float t = static_cast<float>(SDL_GetTicks() % 3600000U) / 1000.0F;
	const float u = 0.5F - 0.5F * std::cos(6.2831853F * (t / 18.0F + wisp.phase));
	const float dx = static_cast<float>(wisp.chest.x - wisp.road.x);
	const float dy = static_cast<float>(wisp.chest.y - wisp.road.y);
	const float len = std::max(1.0F, std::sqrt(dx * dx + dy * dy));
	const float sway = 0.45F * std::sin(t * 1.7F + wisp.phase * 6.0F);
	x = static_cast<float>(wisp.road.x) + dx * u - dy / len * sway;
	y = static_cast<float>(wisp.road.y) + dy * u + dx / len * sway;
}

/** @brief The wisp's chest still shut: once it is opened, its wisp is gone. */
bool WispLives(const Wisp &wisp)
{
	const int id = dObject[wisp.chest.x][wisp.chest.y];
	if (id == 0)
		return false;
	const Object &object = Objects[std::abs(id) - 1];
	return object._otype == OBJ_CHEST2 && object._oSelFlag != 0;
}

/** @brief The light on the way out under the cursor; false when there is none (defined with the ways out, below). */
bool HoveredExitLamp(WoodsLamp &lamp);

} // namespace

const std::vector<WoodsLamp> &WoodsNightLamps()
{
	static std::vector<WoodsLamp> lamps;
	lamps.clear();
	if (leveltype != DTYPE_WOODS || !setlevel || !IsOutdoorActLevel(setlvlnum))
		return lamps;
	// The way out under the cursor, lit (RTM dev note, 2026-10-20).
	if (WoodsLamp lamp {}; HoveredExitLamp(lamp))
		lamps.push_back(lamp);
	// The cottages' lamps behind their doors, and their candle stands outside.
	for (int i = 0; i < HouseCount; i++) {
		const House &house = Houses[i];
		lamps.push_back({ static_cast<float>(Lo + 2 * house.x1 - 1), static_cast<float>(Lo + 2 * house.doorY + 1), 5.0F, 0.9F, 0xFFB464U });
	}
	for (const Point candle : CandleStands) {
		const int id = dObject[candle.x][candle.y];
		if (id == 0 || Objects[std::abs(id) - 1]._otype != OBJ_CANDLE2)
			continue; // the plan and the saved floor apart (the woods' look changed between visits)
		lamps.push_back({ static_cast<float>(candle.x), static_cast<float>(candle.y), 3.5F, 0.8F, 0xFFC878U });
	}
	for (const Point brazier : Braziers)
		if (Tree[brazier.x][brazier.y] == Brazier)
			lamps.push_back({ static_cast<float>(brazier.x), static_cast<float>(brazier.y), 5.0F, 0.95F, 0xFF9A48U });
	for (const Point crystal : PaleCrystals)
		if (Tree[crystal.x][crystal.y] == CrystalPale)
			lamps.push_back({ static_cast<float>(crystal.x), static_cast<float>(crystal.y), 3.0F, 0.45F, 0xA8E8C8U });
	for (const Point lantern : CampLanterns)
		if (Tree[lantern.x][lantern.y] == LanternPole)
			lamps.push_back({ static_cast<float>(lantern.x), static_cast<float>(lantern.y), 4.0F, 0.85F, 0xFFB868U });
	for (int i = 0; i < WispCount; i++) {
		if (!WispLives(Wisps[i]))
			continue;
		float x, y;
		WispAt(Wisps[i], x, y);
		lamps.push_back({ x, y, 3.0F, 0.7F, 0xA0D2FFU });
	}
	if (HasFairyRing)
		lamps.push_back({ static_cast<float>(GladeCentre.x), static_cast<float>(GladeCentre.y), 3.5F, 0.6F, 0x78F0C8U });
	return lamps;
}

void DrawWoodsGlows(const Surface &out, Point firstTile, Point firstTileBuffer)
{
	if (leveltype != DTYPE_WOODS || !setlevel || !IsOutdoorActLevel(setlvlnum) || out.isIndexed() || out.w() <= 0 || out.h() <= 0)
		return;
	std::vector<Splat> *splats = SplatCollection();
	const auto add = [&](int px, int py, int r, int g, int b) {
		if (px < 0 || py < 0 || px >= out.w() || py >= out.h() || (r | g | b) <= 0)
			return;
		r = std::min(r, 255);
		g = std::min(g, 255);
		b = std::min(b, 255);
		if (splats != nullptr) {
			splats->push_back({ static_cast<float>(px), static_cast<float>(py), static_cast<float>(r) / 255.0F, static_cast<float>(g) / 255.0F, static_cast<float>(b) / 255.0F, 0.0F });
			return;
		}
		uint32_t &c = *out.at<uint32_t>(px, py);
		const int cr = std::min(255, static_cast<int>((c >> 16) & 0xFF) + r);
		const int cg = std::min(255, static_cast<int>((c >> 8) & 0xFF) + g);
		const int cb = std::min(255, static_cast<int>(c & 0xFF) + b);
		c = (c & 0xFF000000U) | (static_cast<uint32_t>(cr) << 16) | (static_cast<uint32_t>(cg) << 8) | static_cast<uint32_t>(cb);
	};
	const auto glow = [&](float cx, float cy, int radius, uint32_t rgb, float k) {
		if (cx < -radius || cy < -radius || cx > out.w() + radius || cy > out.h() + radius || k <= 0.01F)
			return;
		const int x0 = static_cast<int>(cx);
		const int y0 = static_cast<int>(cy);
		for (int dy = -radius; dy <= radius; dy++) {
			for (int dx = -radius; dx <= radius; dx++) {
				const float d = std::sqrt(static_cast<float>(dx * dx + dy * dy)) / static_cast<float>(radius);
				if (d >= 1.0F)
					continue;
				const float f = (1.0F - d) * (1.0F - d) * k;
				add(x0 + dx, y0 + dy, static_cast<int>(static_cast<float>((rgb >> 16) & 0xFF) * f), static_cast<int>(static_cast<float>((rgb >> 8) & 0xFF) * f),
				    static_cast<int>(static_cast<float>(rgb & 0xFF) * f));
			}
		}
	};
	const auto screen = [&](float tx, float ty, float &sx, float &sy) {
		const float ux = tx - static_cast<float>(firstTile.x);
		const float uy = ty - static_cast<float>(firstTile.y);
		sx = static_cast<float>(firstTileBuffer.x) + 32.0F + (ux - uy) * 32.0F;
		sy = static_cast<float>(firstTileBuffer.y) - 15.0F + (ux + uy) * 16.0F;
	};
	const float t = static_cast<float>(SDL_GetTicks() % 3600000U) / 1000.0F;
	// Brighter by night; by day a faint shimmer.
	const float night = TownIsDark() ? 1.0F : 0.4F;
	for (int i = 0; i < WispCount; i++) {
		if (!WispLives(Wisps[i]))
			continue;
		float x, y, sx, sy;
		WispAt(Wisps[i], x, y);
		screen(x, y, sx, sy);
		sy -= 34.0F + 6.0F * std::sin(t * 2.3F + Wisps[i].phase * 9.0F); // it floats at a man's chest
		const float flicker = 0.85F + 0.15F * std::sin(t * 11.0F + Wisps[i].phase * 4.0F);
		glow(sx, sy, 18, 0x6EA8FFU, 0.55F * night * flicker);
		glow(sx, sy, 5, 0xE6F4FFU, 1.1F * night * flicker);
	}
	// The Frost Caves' crystals (RfA-78, 81 paint them unlit): a cold blue shine in their upper half, breathing slowly, each on its own
	// phase.
	if (Frost) {
		for (const Point crystal : FrostLights) {
			const uint8_t kind = Tree[crystal.x][crystal.y];
			if (kind != CrystalCluster && kind != CrystalsSmall && kind != CrystalSpire)
				continue;
			const Picture &pic = Art().trees[kind];
			if (pic.px.empty())
				continue;
			float sx, sy;
			screen(static_cast<float>(crystal.x), static_cast<float>(crystal.y), sx, sy);
			sx += static_cast<float>(TreeJitterX[crystal.x][crystal.y]);
			sy += static_cast<float>(TreeJitterY[crystal.x][crystal.y]) - static_cast<float>(pic.footY) * (kind == CrystalSpire ? 0.60F : 0.55F);
			const float pulse = 0.70F + 0.30F * std::sin(t * 0.6F + static_cast<float>(crystal.x * 3 + crystal.y));
			const int reach = kind == CrystalsSmall ? 14 : 26;
			glow(sx, sy, reach, 0x6EA8F0U, 0.30F * pulse);
			glow(sx, sy, reach / 3, 0xCFE6FFU, 0.45F * pulse);
		}
	}
	// The Ash Dunes' boulders: embers deep in their cracks, a dull red breathing slowly (RfA-72 paints them dark).
	for (const Point rock : EmberRocks) {
		if (Tree[rock.x][rock.y] != BoulderAsh)
			continue;
		float sx, sy;
		screen(static_cast<float>(rock.x), static_cast<float>(rock.y), sx, sy);
		sx += static_cast<float>(TreeJitterX[rock.x][rock.y]);
		sy += static_cast<float>(TreeJitterY[rock.x][rock.y]);
		const float seed = static_cast<float>(rock.x * 13 + rock.y * 7);
		const float breathe = 0.55F + 0.45F * std::sin(t * 0.9F + seed);
		const float night = TownIsDark() ? 1.0F : 0.55F;
		glow(sx - 6.0F, sy - 18.0F, 7, 0xFF4A14U, 0.45F * breathe * night);
		glow(sx + 7.0F, sy - 12.0F, 5, 0xFF6A20U, 0.40F * (1.0F - 0.6F * breathe) * night);
	}
	// The camps' lanterns by night (RfA-73 paints them cold): a small flame behind the glass, flickering; out by day.
	if (TownIsDark()) {
		for (const Point lantern : CampLanterns) {
			if (Tree[lantern.x][lantern.y] != LanternPole)
				continue;
			const Picture &pic = Art().trees[LanternPole];
			float sx, sy;
			screen(static_cast<float>(lantern.x), static_cast<float>(lantern.y), sx, sy);
			// The lantern hangs left of the pole, a little below its top (the painting: 64 x 192, the glass at about (18, 60)).
			sx += static_cast<float>(TreeJitterX[lantern.x][lantern.y]) - static_cast<float>(pic.footX) * 0.45F;
			sy += static_cast<float>(TreeJitterY[lantern.x][lantern.y]) - static_cast<float>(pic.footY) * 0.66F;
			const float flicker = 0.82F + 0.12F * std::sin(t * 9.0F + static_cast<float>(lantern.x)) + 0.06F * std::sin(t * 23.0F + static_cast<float>(lantern.y));
			glow(sx, sy, 16, 0xFFA040U, 0.40F * flicker);
			glow(sx, sy, 4, 0xFFE0A0U, 1.00F * flicker);
		}
	}
	// The Glass Flats' pale crystals: a faint cold glow, pulsing (RfA-72b paints them unlit).
	for (const Point crystal : PaleCrystals) {
		if (Tree[crystal.x][crystal.y] != CrystalPale)
			continue;
		const Picture &pic = Art().trees[CrystalPale];
		float sx, sy;
		screen(static_cast<float>(crystal.x), static_cast<float>(crystal.y), sx, sy);
		sx += static_cast<float>(TreeJitterX[crystal.x][crystal.y]);
		sy += static_cast<float>(TreeJitterY[crystal.x][crystal.y]) - static_cast<float>(pic.footY) * 0.55F;
		const float pulse = 0.65F + 0.35F * std::sin(t * 0.7F + static_cast<float>(crystal.x + crystal.y));
		glow(sx, sy, 20, 0xB8F0D0U, 0.22F * pulse);
		glow(sx, sy, 6, 0xE8FFF0U, 0.35F * pulse);
	}
	// The Tomb's braziers (RfA-70 paints them cold): a fire in each bowl, flickering, with tongues rising off it.
	for (const Point brazier : Braziers) {
		if (Tree[brazier.x][brazier.y] != Brazier)
			continue;
		const Picture &pic = Art().trees[Brazier];
		if (pic.px.empty())
			continue;
		float sx, sy;
		screen(static_cast<float>(brazier.x), static_cast<float>(brazier.y), sx, sy);
		sx += static_cast<float>(TreeJitterX[brazier.x][brazier.y]);
		sy += static_cast<float>(TreeJitterY[brazier.x][brazier.y]) - static_cast<float>(pic.footY) * 0.86F; // the bowl's rim
		const float seed = static_cast<float>(brazier.x * 7 + brazier.y * 13);
		const float flicker = 0.80F + 0.12F * std::sin(t * 13.0F + seed) + 0.08F * std::sin(t * 29.0F + seed * 1.7F);
		const float fire = TownIsDark() ? 1.0F : 0.7F;
		glow(sx, sy - 4.0F, 22, 0xFF7A28U, 0.42F * fire * flicker);
		glow(sx, sy - 3.0F, 7, 0xFFC060U, 0.95F * fire * flicker);
		for (int k = 0; k < 3; k++) {
			const float life = std::fmod(t * 1.6F + static_cast<float>(k) / 3.0F + seed * 0.01F, 1.0F);
			const float drift = std::sin(t * 5.0F + static_cast<float>(k) * 2.1F + seed) * 2.5F;
			glow(sx + static_cast<float>(k - 1) * 3.0F + drift, sy - 5.0F - life * 12.0F, 3, 0xFFD890U, 0.8F * fire * (1.0F - life));
		}
	}
	if (HasFairyRing) {
		for (int k = 0; k < 11; k++) {
			const float a = 6.2831853F * static_cast<float>(k) / 11.0F;
			float sx, sy;
			screen(static_cast<float>(GladeCentre.x) + 2.4F * std::cos(a), static_cast<float>(GladeCentre.y) + 2.4F * std::sin(a), sx, sy);
			const float pulse = 0.55F + 0.45F * std::sin(t * 1.3F + static_cast<float>(k) * 1.7F);
			glow(sx, sy - 3.0F, 9, 0x50E6B4U, 0.45F * night * pulse);
			glow(sx, sy - 3.0F, 3, 0xD2FFF0U, 0.9F * night * pulse);
		}
	}
}

void DrawDesertSky(const Surface &out, Point firstTile, Point firstTileBuffer)
{
	if (!OnDesertFloor() || !Desert || out.isIndexed() || out.w() <= 0 || out.h() <= 0)
		return;
	const int w = out.w();
	const int h = out.h();
	const float t = static_cast<float>(SDL_GetTicks() % 3600000U) / 1000.0F;
	const auto screen = [&](float tx, float ty, float &sx, float &sy) {
		const float ux = tx - static_cast<float>(firstTile.x);
		const float uy = ty - static_cast<float>(firstTile.y);
		sx = static_cast<float>(firstTileBuffer.x) + 32.0F + (ux - uy) * 32.0F;
		sy = static_cast<float>(firstTileBuffer.y) - 15.0F + (ux + uy) * 16.0F;
	};
	const auto blend = [&](int px, int py, int r, int g, int b, int a) {
		if (px < 0 || py < 0 || px >= w || py >= h || a <= 0)
			return;
		uint32_t &c = *out.at<uint32_t>(px, py);
		a = std::min(a, 256);
		const auto ch = [a](uint32_t under, int over) { return static_cast<uint32_t>((static_cast<int>(under) * (256 - a) + over * a) >> 8); };
		c = (c & 0xFF000000U) | (ch((c >> 16) & 0xFF, r) << 16) | (ch((c >> 8) & 0xFF, g) << 8) | ch(c & 0xFF, b);
	};
	float storm = 0.0F;
	const bool stormy = CurrentWeather(&storm) == Weather::Sandstorm && storm > 0.2F && *sgOptions.Oracool.weather;
	const bool day = !TownIsDark();

	// The Ash Dunes' vents (RfA-72): thin grey smoke rising and spreading, each puff fading as it climbs; leaning with the wind in a
	// storm.
	for (const Point vent : SmokeVents) {
		float sx, sy;
		screen(static_cast<float>(vent.x), static_cast<float>(vent.y), sx, sy);
		if (sx < -40.0F || sx > static_cast<float>(w) + 40.0F || sy < -80.0F || sy > static_cast<float>(h) + 20.0F)
			continue;
		for (uint32_t k = 0; k < 6; k++) {
			const float life = std::fmod(t * 0.32F + static_cast<float>(k) / 6.0F + Unit(Hash(static_cast<uint32_t>(vent.x), static_cast<uint32_t>(vent.y), k)) * 0.2F, 1.0F);
			const float radius = 2.5F + life * 7.0F;
			const float cx = sx + std::sin(t * 0.8F + static_cast<float>(k) * 1.7F) * 3.0F * life + (stormy ? -life * 22.0F : 0.0F);
			const float cy = sy - 6.0F - life * 46.0F;
			const int alpha = static_cast<int>(70.0F * (1.0F - life));
			const int ri = static_cast<int>(radius);
			for (int dy = -ri; dy <= ri; dy++)
				for (int dx = -ri; dx <= ri; dx++) {
					const float d = std::sqrt(static_cast<float>(dx * dx + dy * dy)) / radius;
					if (d < 1.0F)
						blend(static_cast<int>(cx) + dx, static_cast<int>(cy) + dy, 120, 116, 110, static_cast<int>(static_cast<float>(alpha) * (1.0F - d)));
				}
		}
	}

	// The sand hole: grains sliding down its inner wall into the dark, each on its own path round the funnel - the painted sinkhole's
	// narrower one (RfA-71b); none down the painted stairway.
	const Picture *painted = DesertEntrancePicture();
	const bool stairway = painted != nullptr && painted == &Art().entrances[1];
	const float funnel = painted != nullptr ? 0.62F : 1.0F;
	if (MouthOnFloor && !stairway) {
		for (uint32_t k = 0; k < 18; k++) {
			const float phase = Unit(Hash(k, 0x5A9DU, 1));
			const float a = 6.2831853F * Unit(Hash(k, 0x5A9DU, 2)) + t * 0.15F;
			const float p = std::fmod(t * (0.22F + 0.18F * Unit(Hash(k, 0x5A9DU, 3))) + phase, 1.0F);
			const float radius = SandHoleRadius * funnel * (0.92F - 0.52F * p);
			float sx, sy;
			screen(static_cast<float>(MouthEntrance.x) + std::cos(a) * radius, static_cast<float>(MouthEntrance.y) + std::sin(a) * radius, sx, sy);
			const int alpha = static_cast<int>(200.0F * (1.0F - p) * (1.0F - p));
			const int shade = static_cast<int>(196.0F - 90.0F * p);
			blend(static_cast<int>(sx), static_cast<int>(sy), shade, shade * 88 / 100, shade * 68 / 100, alpha);
			if (k % 3 == 0)
				blend(static_cast<int>(sx) + 1, static_cast<int>(sy), shade, shade * 88 / 100, shade * 68 / 100, alpha / 2);
		}
	}

	// The vultures' shadows by day: three birds circling high over the floor's middle, their shadows soft dark wings on the sand
	// (none in a storm: the sun is gone).
	if (day && !stormy) {
		for (uint32_t k = 0; k < 3; k++) {
			const float radius = 7.0F + 3.0F * static_cast<float>(k);
			const float speed = (0.05F + 0.015F * static_cast<float>(k)) * ((k & 1) != 0 ? -1.0F : 1.0F);
			const float a = t * speed * 6.2831853F + static_cast<float>(k) * 2.1F;
			const float cx = static_cast<float>(GladeCentre.x) + std::cos(a) * radius;
			const float cy = static_cast<float>(GladeCentre.y) + std::sin(a) * radius;
			float sx, sy;
			screen(cx, cy, sx, sy);
			if (sx < -60 || sy < -40 || sx > w + 60 || sy > h + 40)
				continue;
			// The bird's heading on the screen, and a slow flap.
			const float hx = -std::sin(a) * speed, hy = std::cos(a) * speed;
			const float dirX = (hx - hy) * 32.0F, dirY = (hx + hy) * 16.0F;
			const float len = std::max(0.001F, std::sqrt(dirX * dirX + dirY * dirY));
			const float fx = dirX / len, fy = dirY / len; // forward
			const float flap = 0.75F + 0.25F * std::sin(t * 2.6F + static_cast<float>(k));
			for (int dy = -16; dy <= 16; dy++) {
				for (int dx = -26; dx <= 26; dx++) {
					// Into the bird's frame: along its heading and across it.
					const float along = static_cast<float>(dx) * fx + static_cast<float>(dy) * fy;
					const float across = -static_cast<float>(dx) * fy + static_cast<float>(dy) * fx;
					const float span = std::abs(across);
					if (span > 24.0F * flap)
						continue;
					// The wings sweep back toward their tips; the body a short line along the middle.
					const float sweep = -span * 0.35F;
					const float wing = std::abs(along - sweep) - (5.0F - span * 0.16F);
					const float body = span < 2.5F ? std::abs(along) - 7.0F : 1e9F;
					const float inside = -std::min(wing, body);
					if (inside <= -1.5F)
						continue;
					const int alpha = static_cast<int>(std::clamp((inside + 1.5F) / 3.0F, 0.0F, 1.0F) * 70.0F);
					blend(static_cast<int>(sx) + dx, static_cast<int>(sy) + dy, 20, 16, 12, alpha);
				}
			}
		}
	}

	// The dust devils: two whirls of sand wandering the open floor by day, each a column of grains spiralling up and widening, a
	// ring of dust at its foot (in a storm the wind tears them apart).
	if (day && !stormy) {
		for (uint32_t k = 0; k < 2; k++) {
			const float s = static_cast<float>(k) * 3.7F;
			const float cx = static_cast<float>(GladeCentre.x) + 14.0F * std::sin(t * 0.021F + s) + 6.0F * std::sin(t * 0.053F + s * 2.0F);
			const float cy = static_cast<float>(GladeCentre.y) + 12.0F * std::cos(t * 0.017F + s * 1.3F) + 5.0F * std::cos(t * 0.047F + s);
			const int tx = static_cast<int>(cx), ty = static_cast<int>(cy);
			if (!InDungeonBounds({ tx, ty }) || !Inside({ tx, ty }) || Tree[tx][ty] != NoTree)
				continue; // it dies against a rock and comes back elsewhere
			float sx, sy;
			screen(cx, cy, sx, sy);
			if (sx < -60 || sy < -120 || sx > w + 60 || sy > h + 30)
				continue;
			for (int g = 0; g < 110; g++) {
				const uint32_t hh = Hash(static_cast<uint32_t>(g), k, 0xD05DU);
				const float height = Unit(hh) * 78.0F; // up the column
				const float spin = t * (5.5F - height * 0.03F) + Unit(hh >> 5) * 6.2831853F;
				const float radius = 3.0F + height * 0.22F + 2.0F * std::sin(t * 1.3F + static_cast<float>(g));
				const int px = static_cast<int>(sx + std::cos(spin) * radius + std::sin(t * 0.9F + height * 0.05F) * height * 0.08F);
				const int py = static_cast<int>(sy - height + std::sin(spin) * radius * 0.35F);
				const int alpha = static_cast<int>((1.0F - height / 90.0F) * 120.0F * (std::sin(spin) > 0 ? 1.0F : 0.6F));
				blend(px, py, 206, 182, 138, alpha);
			}
			for (int g = 0; g < 26; g++) {
				const float a = 6.2831853F * static_cast<float>(g) / 26.0F + t * 2.0F;
				blend(static_cast<int>(sx + std::cos(a) * 9.0F), static_cast<int>(sy + std::sin(a) * 3.5F), 190, 166, 124, 60);
			}
		}
	}
}

void TickWoods()
{
	if (leveltype != DTYPE_WOODS || !setlevel || !IsOutdoorActLevel(setlvlnum) || MyPlayer == nullptr || !HasFairyRing)
		return;
	Player &player = *MyPlayer;
	// In at two tiles, out past three: along its edge the message came at every step (audit, 2026-10-14).
	const int distance = player.position.tile.WalkingDistance(GladeCentre);
	const bool in = player._pHitPoints > 0 && (distance <= 2 || (InFairyRingBefore && distance <= 3));
	if (in && !InFairyRingBefore && player._pMaxMana > 0 && !UsesRage(player))
		EventPlrMsg("The fairy ring hums. Your mana flows faster here.", UiFlags::ColorWhite);
	InFairyRingBefore = in;
	if (!in || ++FairyRingTicks % 20 != 0)
		return;
	// Three percent of the hero's mana a second while he stands in it.
	const int add = player._pMaxMana * 3 / 100;
	if (add <= 0 || player._pMana >= player._pMaxMana)
		return;
	player._pMana = std::min(player._pMana + add, player._pMaxMana);
	player._pManaBase = std::min(player._pManaBase + add, player._pMaxManaBase);
	RedrawComponent(PanelDrawComponent::Mana);
}

void BuildWoodsLevel(bool fresh)
{
	const OrclArea area = AreaOfLevel(setlvlnum);
	const int a = AreaIndex(area);
	const int floor = FloorIndex(setlvlnum);
	uint32_t &seed = Game.seed[a][floor];
	if (seed == 0)
		seed = (static_cast<uint32_t>(AdvanceRndSeed()) ^ (SDL_GetTicks() * 2654435761U)) | 1U;
	if (Game.rung[a][floor] == 0) {
		// D3: the floor's depth is fixed the first time it is entered in a game - by the hero who enters it.
		const int heroLevel = MyPlayer != nullptr ? MyPlayer->_pLevel : 1;
		Game.rung[a][floor] = static_cast<uint8_t>(std::clamp(std::clamp(heroLevel / 3, 1, RungsPerDifficulty) + floor, 1, RungsPerDifficulty));
	}
	MouthFloor = WoodsCaveOnFloor(floor, area) != SL_NONE ? floor : -1;
	ForgetWoodsMixedFloor(); // the last floor's ground pictures
	CreateDungeon(seed, ENTRY_SETLVL);
	MouthFloor = -1;
	if (MouthOnFloor) {
		for (int y = MouthOrigin.y; y < MouthOrigin.y + MouthHeight; y++)
			for (int x = MouthOrigin.x; x < MouthOrigin.x + MouthWidth; x++)
				Scatter[x][y] = 0;
	}
	StampRiver();
	StampCaveMouth();
	LayFences();
	LayHouses();
	ViewPosition = EnterAtWayOn ? Point { WayOn.x + 2, WayOn.y } : Arrival;
	EnterAtWayOn = false;
	InitNoTriggers();
	LoadRndLvlPal(leveltype);
	LoadLevelSOLData();
	if (EnterAtCaveMouth && MouthOnFloor) {
		// Out of the cave: open ground in front of the mouth, never its entrance (that would send the hero straight back).
		Point landing = OpenFloorNear(MouthEntrance + Displacement { 4, 3 });
		if (landing.WalkingDistance(MouthEntrance) < 2)
			landing = MouthEntrance + Displacement { 4, 3 };
		ViewPosition = landing;
	}
	EnterAtCaveMouth = false;
	// No pack on either road's end: the hero arrives at one, and may come back by the other.
	for (const Point end : { Arrival, WayOn, MouthOnFloor ? MouthEntrance : Arrival }) {
		for (int dy = -5; dy <= 5; dy++) {
			for (int dx = -5; dx <= 5; dx++) {
				const Point tile = end + Displacement { dx, dy };
				if (InDungeonBounds(tile))
					dFlags[tile.x][tile.y] |= DungeonFlag::Populated;
			}
		}
	}
	// The floor's waypoint (D9): in the arrival's clearing, two tiles up the screen from where the hero lands. A revisit's comes
	// back with the level's save; its graphic is loaded on every build, or a restored sigil draws from nothing.
	const Point sigil = Arrival + Displacement { -2, -2 };
	Scatter[sigil.x][sigil.y] = 0;
	if (fresh)
		AddGeneratedLevelWaypointSigil(sigil, (area == OrclArea::Frost ? FirstFrostWaypoint : area == OrclArea::Desert ? FirstDesertWaypoint : FirstWoodsWaypoint) + floor);
	else
		EnsureWaypointGraphicsLoaded();
	// What stands along the roads (PlanWoodsWorld): planned on every build, its objects added on the first.
	PlanWoodsWorld(seed, floor);
	EnsureWoodsWorldGraphics();
	if (fresh)
		PlaceWoodsWorldObjects();
	// The Frost Caves' crystals light the dark round them, as a dungeon's lights do (2026-10-17).
	if (area == OrclArea::Frost) {
		for (const auto &[at, radius] : FrostCrystalLightSpots())
			AddLight(at, radius);
		for (const Point at : LightShafts)
			AddLight(at, 6); // the grey daylight falling through the roof
	}
	if (!HeadlessMode) {
		LoadArt();
		BuildEdgeNoise();
	}
}

namespace {

/**
 * @brief The road out of Tristram: at the end of the cleared path below Pepin's house, on the map's south-west edge (user,
 * 2026-10-13: "Going west of Pepin takes us to the four grass/woods levels"; then, with a screenshot, "i want the entrance to
 * the woods to be where the green circle is"). Standing on it loads woods floor 1.
 */
constexpr int GateX0 = 56;
constexpr int GateX1 = 59;
constexpr int GateY0 = 94;
constexpr int GateY1 = 95;

bool OnTownGate(Point tile)
{
	return tile.x >= GateX0 && tile.x <= GateX1 && tile.y >= GateY0 && tile.y <= GateY1;
}

/**
 * @brief The road out to the desert: through a gap in Tristram's north wall by the church (user, 2026-10-21, a screenshot: "make here
 * the entrance to desert. the yellowish zone cover with sand textures"; then "just clear a part of the wall so i can get to the sand
 * area"), the wall's megatile at x 36-37 cleared (levels/town.cpp), into the walled strip behind it (x 35-41, y 0-9) laid with sand.
 * Its far end, rows 1-2, loads desert floor 1. Until then the open top row east of the cows' pen.
 */
constexpr int DoorwayX0 = 36; // the gap in the wall, for the sand's tongue through it
constexpr int DoorwayX1 = 37;
constexpr int DesertGateX0 = 35;
constexpr int DesertGateX1 = 41;
constexpr int DesertGateY0 = 1; // the strip's far end
constexpr int DesertGateY1 = 2;

bool OnDesertGate(Point tile)
{
	return tile.x >= DesertGateX0 && tile.x <= DesertGateX1 && tile.y >= DesertGateY0 && tile.y <= DesertGateY1;
}

/**
 * @brief The way down into the Frost Caves: on Tristram's east edge past the cows' pen fence (user, 2026-10-16, candidate A: "East
 * edge"), the open edge rows there (the walk map, 2026-10-17: x 83-84 open at y 27-33). Standing on it loads the Rime Gate. The
 * painted cave mouth (RfA-79's mouth_tristram) stands here when the town draws it.
 */
constexpr int FrostGateX0 = 83;
constexpr int FrostGateX1 = 84;
constexpr int FrostGateY0 = 29;
constexpr int FrostGateY1 = 32;

bool OnFrostGate(Point tile)
{
	return tile.x >= FrostGateX0 && tile.x <= FrostGateX1 && tile.y >= FrostGateY0 && tile.y <= FrostGateY1;
}

/** @brief Whether the hero last stood in the woods' way-on strip, so its message shows once per visit to it. */
bool AtWayOnBefore = false;

/** @brief A way out where the hero stands: its glow's centre and reach, whether it is open, the tile a click walks to, where it goes. */
struct ExitMark {
	float x, y, radius;
	bool open;
	Point entrance;
	_setlevels to; // SL_NONE: back to Tristram, or nowhere (the last floor's way on)
	bool cave;
};

/** @brief "The Salt Road" as "the Salt Road", to follow "To". */
std::string AfterTo(const char *name)
{
	std::string text = name;
	if (text.rfind("The ", 0) == 0)
		text[0] = 't';
	return text;
}

/** @brief The ways out of the floor the hero stands on: Tristram's three gates, or a woods floor's road back, way on and cave mouth. */
int ExitMarksHere(ExitMark (&marks)[3])
{
	int count = 0;
	if (leveltype == DTYPE_TOWN) {
		marks[count++] = { (GateX0 + GateX1) * 0.5F, (GateY0 + GateY1) * 0.5F, 2.6F, true, { (GateX0 + GateX1) / 2, GateY0 }, SL_ORCL_WOODS_1, false };
		marks[count++] = { (DesertGateX0 + DesertGateX1) * 0.5F, (DesertGateY0 + DesertGateY1) * 0.5F, 2.6F, true, { (DesertGateX0 + DesertGateX1) / 2, DesertGateY1 }, SL_ORCL_DESERT_1, false };
		marks[count++] = { (FrostGateX0 + FrostGateX1) * 0.5F, (FrostGateY0 + FrostGateY1) * 0.5F, 2.6F, true, { FrostGateX0, (FrostGateY0 + FrostGateY1) / 2 }, SL_ORCL_FROST_1, false };
	} else if (leveltype == DTYPE_WOODS && setlevel && IsOutdoorActLevel(setlvlnum)) {
		const OrclArea area = AreaOfLevel(setlvlnum);
		const int floor = FloorIndex(setlvlnum);
		const bool lastFloor = floor == WoodsFloors - 1;
		// The road back, the way on to the next floor (amber on the last, where it ends), the cave's entrance.
		marks[count++] = { static_cast<float>(Hi) - 0.5F, static_cast<float>(Arrival.y), 2.4F, true, { Hi - 1, Arrival.y },
			floor == 0 ? SL_NONE : OutdoorFloorLevel(area, floor - 1), false };
		marks[count++] = { static_cast<float>(Lo) + 0.5F, static_cast<float>(WayOn.y), 2.4F, !lastFloor, { Lo + 1, WayOn.y },
			lastFloor ? SL_NONE : OutdoorFloorLevel(area, floor + 1), false };
		if (MouthOnFloor)
			marks[count++] = { static_cast<float>(MouthEntrance.x), static_cast<float>(MouthEntrance.y), 1.4F, true, MouthEntrance,
				WoodsCaveOnFloor(floor, area), true };
	}
	return count;
}

/** @brief The way out the cursor is on (RTM dev note, 2026-10-20: "when i hover over entrance to another area light it up"), or -1. */
int HoveredExit = -1;

bool HoveredExitLamp(WoodsLamp &lamp)
{
	ExitMark marks[3];
	if (HoveredExit < 0 || HoveredExit >= ExitMarksHere(marks))
		return false;
	const ExitMark &mark = marks[HoveredExit];
	lamp = { mark.x, mark.y, 4.5F, 1.0F, mark.open ? 0xA8FF9CU : 0xFFC060U };
	return true;
}

} // namespace

void ClearWoodsExitHover()
{
	HoveredExit = -1;
}

bool WoodsExitHover(Point tile, Point &entrance, std::string &text)
{
	ExitMark marks[3];
	const int count = ExitMarksHere(marks);
	for (int m = 0; m < count; m++) {
		const float dx = static_cast<float>(tile.x) - marks[m].x, dy = static_cast<float>(tile.y) - marks[m].y;
		if (dx * dx + dy * dy > marks[m].radius * marks[m].radius)
			continue;
		HoveredExit = m;
		entrance = marks[m].entrance;
		if (marks[m].cave)
			text = std::string("Down to ") + WoodsCaveName(marks[m].to);
		else if (marks[m].to != SL_NONE)
			text = "To " + AfterTo(WoodsFloorName(marks[m].to));
		else if (marks[m].open)
			text = "To Tristram";
		else
			text = "The way ends here";
		return true;
	}
	return false;
}

std::vector<Point> WoodsExitMarkers()
{
	std::vector<Point> markers;
	if (setlevel && IsActCaveLevel(setlvlnum)) {
		for (int j = 0; j < numtrigs; j++)
			markers.push_back(trigs[j].position);
		return markers;
	}
	ExitMark marks[3];
	const int count = ExitMarksHere(marks);
	for (int m = 0; m < count; m++) {
		if (marks[m].open)
			markers.push_back(marks[m].entrance);
	}
	return markers;
}

Point WoodsTownReturnTile()
{
	if (IsDesertLevel(setlvlnum))
		return { (DesertGateX0 + DesertGateX1) / 2, DesertGateY1 + 3 }; // on the sand, three rows in from the strip's far end
	if (IsFrostLevel(setlvlnum))
		return { FrostGateX0 - 2, (FrostGateY0 + FrostGateY1) / 2 }; // two tiles in from the Frost Caves' gate
	return { 57, GateY0 - 2 }; // on the cleared path, two rows above the entrance
}

bool CheckWoodsEdges()
{
	if (MyPlayer == nullptr)
		return false;
	if (setlevel && IsActCaveLevel(setlvlnum))
		return MyPlayer->_pmode == PM_STAND && !MyPlayer->_pLvlChanging && CheckWoodsCaveExits();
	Player &player = *MyPlayer;
	if (player._pmode != PM_STAND || player._pLvlChanging)
		return false;
	const Point tile = player.position.tile;
	if (leveltype == DTYPE_TOWN) {
		const bool woods = OnTownGate(tile);
		const bool frost = OnFrostGate(tile);
		if (!woods && !frost && !OnDesertGate(tile))
			return false;
		setlvltype = DTYPE_WOODS; // BEFORE StartNewLvl: the level loads its tileset from this (the desert's and the Frost Caves' too)
		StartNewLvl(player, WM_DIABSETLVL, woods ? SL_ORCL_WOODS_1 : frost ? SL_ORCL_FROST_1 : SL_ORCL_DESERT_1);
		return true;
	}
	if (leveltype != DTYPE_WOODS || !setlevel || !IsOutdoorActLevel(setlvlnum))
		return false;
	const OrclArea area = AreaOfLevel(setlvlnum);
	const int floor = FloorIndex(setlvlnum);
	if (MouthOnFloor && tile == MouthEntrance) {
		EnterWoodsCaveFromMouth(floor, area);
		return true;
	}
	// The road back: the last tiles of the arrival road, at the map's edge - to Tristram from the first floor, to the way on
	// of the floor before from the others (milestone 2: four linked floors).
	if (tile.x >= Hi - 1 && std::abs(tile.y - Arrival.y) <= 1) {
		if (floor == 0) {
			StartNewLvl(player, WM_DIABRTNLVL, GetMapReturnLevel());
		} else {
			setlvltype = DTYPE_WOODS;
			EnterAtWayOn = true;
			StartNewLvl(player, WM_DIABSETLVL, static_cast<int>(OutdoorFloorLevel(area, floor - 1)));
		}
		return true;
	}
	const bool atWayOn = tile.x <= Lo + 1 && std::abs(tile.y - WayOn.y) <= 1;
	if (atWayOn && floor + 1 < WoodsFloors) {
		setlvltype = DTYPE_WOODS;
		EnterAtWayOn = false;
		StartNewLvl(player, WM_DIABSETLVL, static_cast<int>(OutdoorFloorLevel(area, floor + 1)));
		return true;
	}
	if (atWayOn && !AtWayOnBefore)
		EventPlrMsg(area == OrclArea::Frost ? "The way ends here, before the Throne of Winter."
		            : area == OrclArea::Desert ? "The salt road ends here, at the Sun-Eater's tomb." : "The path ends here, at the heart of the woods.",
		    UiFlags::ColorWhite);
	AtWayOnBefore = atWayOn;
	return false;
}

namespace {

/** @brief A house's footprint in world tile corners: west and north walls' lines to the east and south walls'. */
void HouseCorners(const House &house, int &wx0, int &wy0, int &wx1, int &wy1)
{
	wx0 = Lo + 2 * house.x0;
	wy0 = Lo + 2 * house.y0;
	wx1 = Lo + 2 * house.x1;
	wy1 = Lo + 2 * house.y1;
}

bool InsideHouse(const House &house, Point tile)
{
	int wx0, wy0, wx1, wy1;
	HouseCorners(house, wx0, wy0, wx1, wy1);
	return tile.x > wx0 && tile.x < wx1 && tile.y > wy0 && tile.y < wy1;
}

} // namespace

void UpdateWoodsShells()
{
	for (int i = 0; i < MaxHouses; i++)
		WoodsShellState[i] = 2;
	if (leveltype != DTYPE_WOODS || !setlevel || MyPlayer == nullptr || !MyPlayer->isOnActiveLevel())
		return;
	const Point hero = MyPlayer->position.tile;
	for (int i = 0; i < HouseCount; i++) {
		const House &house = Houses[i];
		int wx0, wy0, wx1, wy1;
		HouseCorners(house, wx0, wy0, wx1, wy1);
		if (InsideHouse(house, hero))
			WoodsShellState[i] = 0; // the cottage gone: the room under it
		else if (hero.x >= wx0 - 6 && hero.x <= wx1 + 1 && hero.y >= wy0 - 6 && hero.y <= wy1 + 1 && (hero.x <= wx0 || hero.y <= wy0))
			WoodsShellState[i] = 1; // behind it: see-through, as Tristram's own walls are
	}
}

bool WoodsShellCovers(Point tile)
{
	if (leveltype != DTYPE_WOODS || WoodsShellAt(tile) == 0)
		return false;
	const int owner = WoodsShellOwner[tile.x][tile.y];
	return owner >= 0 && owner < HouseCount && WoodsShellState[owner] == 2 && InsideHouse(Houses[owner], tile);
}

void FinishWoodsLevel(bool fresh)
{
	if (!fresh || !setlevel || !IsWoodsLevel(setlvlnum))
		return;
	// What each house holds on the first visit: three things on its floor, one of them a good one.
	for (int i = 0; i < HouseCount; i++) {
		const House &house = Houses[i];
		for (int k = 0; k < 3; k++) {
			const Point at { Lo + 2 * house.x0 + 2 + static_cast<int>(Hash(static_cast<uint32_t>(i), static_cast<uint32_t>(k), 0x100DU) % static_cast<uint32_t>(2 * (house.x1 - house.x0) - 3)),
				Lo + 2 * house.y0 + 2 + static_cast<int>(Hash(static_cast<uint32_t>(k), static_cast<uint32_t>(i), 0x100EU) % static_cast<uint32_t>(2 * (house.y1 - house.y0) - 3)) };
			if (!IsTileSolid(at))
				CreateRndItem(at, /*onlygood=*/k == 0, /*sendmsg=*/false, /*delta=*/false);
		}
	}
}

int WoodsHouseCountForTest()
{
	return HouseCount;
}

Point WoodsHouseDoorForTest(int index)
{
	if (index < 0 || index >= HouseCount)
		return { 0, 0 };
	const House &house = Houses[index];
	return { Lo + 2 * house.x1 + 1, Lo + 2 * house.doorY + 1 }; // outside the east arch
}

Point WoodsHouseInsideForTest(int index)
{
	if (index < 0 || index >= HouseCount)
		return { 0, 0 };
	const House &house = Houses[index];
	return { Lo + 2 * house.x1 - 1, Lo + 2 * house.doorY + 1 }; // inside it
}

int WoodsFloorRung(int floorIndex, OrclArea area)
{
	return Game.rung[AreaIndex(area)][std::clamp(floorIndex, 0, WoodsFloors - 1)];
}

bool DesertBuilt()
{
	return Desert;
}

bool FrostBuilt()
{
	return Frost;
}

std::vector<Point> FrostLightShaftsForTest()
{
	return LightShafts;
}


int FrostMissileFor(int missile)
{
	if (!setlevel || (!IsFrostLevel(setlvlnum) && !IsFrostRiverLevel(setlvlnum)))
		return missile;
	// The cold projectiles that roll a monster's own damage when a monster fires them (AddFirebolt, RollColdDamage); the frost arrow
	// does not (ProcessRogueArrow deals 1 from a monster), so an archer's arrow becomes an ice bolt.
	// Arrows stay arrows (audit, 2026-10-17): as Ice Bolts every archer's hit chilled the hero to half speed, cold resistance turned
	// off his block outside Hellfire, and the Rogue's arrow passives stopped working against them.
	switch (static_cast<MissileID>(missile)) {
	case MissileID::Firebolt:
	case MissileID::OrangeFlare:
	case MissileID::YellowFlare:
	case MissileID::BlueFlare:
	case MissileID::RedFlare:
	case MissileID::BlueFlare2:
		return static_cast<int>(MissileID::IceBolt);
	case MissileID::Acid:
	case MissileID::BloodStar:
	case MissileID::Fireball:
	case MissileID::MagmaBall:
		return static_cast<int>(MissileID::IceBlast);
	case MissileID::DiabloApocalypse:
	case MissileID::ThinLightningControl:
		return static_cast<int>(MissileID::GlacialSpike);
	default:
		return missile;
	}
}

const uint32_t *FrostLookTable(FrostLook look, const uint32_t *lit, std::array<uint32_t, 256> &out)
{
	// The Designer's five looks (scratchpad frost_looks.js, the previews the user picks from), per lit colour.
	const auto clamp8 = [](float v) { return static_cast<uint32_t>(std::clamp(v, 0.0F, 255.0F)); };
	for (size_t i = 0; i < out.size(); i++) {
		const uint32_t c = lit[i];
		float r = static_cast<float>((c >> 16) & 0xFF), g = static_cast<float>((c >> 8) & 0xFF), b = static_cast<float>(c & 0xFF);
		const float y = 0.299F * r + 0.587F * g + 0.114F * b;
		const float l = y / 255.0F;
		switch (look) {
		case FrostLook::Rime:
			r += (y - r) * 0.75F; g += (y - g) * 0.75F; b += (y - b) * 0.75F;
			r = r * 0.82F + 8.0F; g = g * 0.94F + 12.0F; b = b * 1.18F + 24.0F;
			break;
		case FrostLook::Glacier: {
			// Hue 205, saturation 0.42, the colour's own lightness a little lifted.
			const float L = std::min(1.0F, l * 1.06F), S = 0.42F;
			const float ch = (1.0F - std::abs(2.0F * L - 1.0F)) * S, m = L - ch / 2.0F;
			const float x = ch * (1.0F - std::abs(std::fmod(205.0F / 60.0F, 2.0F) - 1.0F));
			r = m * 255.0F; g = (x + m) * 255.0F; b = (ch + m) * 255.0F;
			break;
		}
		case FrostLook::Hoarfrost: {
			const float lift = std::min(1.0F, y / 48.0F);
			const float k = 0.62F * y + 70.0F * lift;
			r = k * 0.90F; g = k * 0.97F; b = k * 1.06F + 6.0F * lift;
			break;
		}
		case FrostLook::Frostbite: {
			r += (y - r) * 0.55F; g += (y - g) * 0.55F; b += (y - b) * 0.55F;
			r *= 0.90F; b = b * 1.10F + 6.0F;
			const float sh = std::max(0.0F, (0.45F - l) / 0.45F), hi = std::max(0.0F, (l - 0.45F) / 0.55F), shadow = std::min(1.0F, y / 24.0F);
			r = r * (1 - 0.75F * sh) + 16 * 0.75F * sh * shadow; g = g * (1 - 0.75F * sh) + 30 * 0.75F * sh * shadow; b = b * (1 - 0.75F * sh) + 70 * 0.75F * sh * shadow;
			r = r * (1 - 0.8F * hi) + 214 * 0.8F * hi; g = g * (1 - 0.8F * hi) + 234 * 0.8F * hi; b = b * (1 - 0.8F * hi) + 255 * 0.8F * hi;
			break;
		}
		case FrostLook::BlackIce: {
			const float d = std::pow(l, 1.35F) * 255.0F * 0.82F, hi = std::max(0.0F, (l - 0.62F) / 0.38F);
			r = d * 0.50F + 110.0F * hi; g = d * 0.72F + 170.0F * hi; b = d * 1.05F + 200.0F * hi + 6.0F;
			break;
		}
		case FrostLook::None:
			break;
		}
		out[i] = (clamp8(r) << 16) | (clamp8(g) << 8) | clamp8(b);
	}
	out[0] = lit[0]; // the sprite's transparent index stays as it is
	return out.data();
}

int WoodsKindAtForTest(Point tile)
{
	return InDungeonBounds(tile) ? Tree[tile.x][tile.y] : 0;
}

int FrostLightCountForTest()
{
	return static_cast<int>(FrostCrystalLightSpots().size());
}

std::vector<std::pair<Point, int>> FrostCrystalLightSpots()
{
	// From an open tile beside each crystal (audit round 2: a crystal stands on a solid tile, and the light pass skips a light in a
	// wall, so none of them lit anything), one light for crystals standing together, at most twenty a floor (the light pool is the
	// spells' too).
	// The big crystals first, then the small, each kind from a spread start (audit round 3: in the floor's row order the twenty went
	// to the top rows and left the bottom of the cavern dark); a group lit at its biggest crystal's radius.
	// Each kind shuffled (a seeded Fisher-Yates: a fixed stride walked the row order again for many counts, audit round 4), the
	// same on every build of the floor.
	std::vector<Point> order;
	const size_t n = FrostLights.size();
	for (int pass = 0; pass < 2; pass++) {
		std::vector<Point> kind;
		for (const Point at : FrostLights)
			if ((Tree[at.x][at.y] == CrystalsSmall) == (pass == 1))
				kind.push_back(at);
		for (size_t i = kind.size(); i > 1; i--) {
			const size_t j = Hash(static_cast<uint32_t>(n), 0x11C7U + static_cast<uint32_t>(pass), static_cast<uint32_t>(i)) % i;
			std::swap(kind[i - 1], kind[j]);
		}
		order.insert(order.end(), kind.begin(), kind.end());
	}
	std::vector<std::pair<Point, int>> spots;
	std::vector<Point> lit;
	for (const Point at : order) {
		if (lit.size() >= 20)
			break;
		if (std::any_of(lit.begin(), lit.end(), [&](Point l) { return l.WalkingDistance(at) < 4; }))
			continue;
		for (const Displacement d : { Displacement { 1, 1 }, Displacement { 1, 0 }, Displacement { 0, 1 }, Displacement { -1, 1 }, Displacement { 1, -1 },
		         Displacement { -1, 0 }, Displacement { 0, -1 }, Displacement { -1, -1 } }) {
			const Point q = at + d;
			if (InDungeonBounds(q) && Tree[q.x][q.y] == NoTree && !TileHasAny(dPiece[q.x][q.y], TileProperties::Solid)) {
				spots.emplace_back(q, Tree[at.x][at.y] == CrystalsSmall ? 4 : 7);
				lit.push_back(at);
				break;
			}
		}
	}
	return spots;
}

void EnterWoodsAtCaveMouthNext()
{
	EnterAtCaveMouth = true;
}

namespace {

/**
 * @brief A town piece with nothing above its floor: the ground round a Tristram cottage. Its stone-dark diamond sat on the painted
 * grass with straight edges (user, 2026-10-14: "look for sharp borders between assets and terain"); the woods' own ground is
 * drawn there instead. Asked of the town's micros once a piece.
 */
bool ShellPieceIsFlat(uint16_t piece)
{
	static std::array<int8_t, MAXTILES> known {}; // 0 not asked, 1 flat, 2 not
	if (piece >= known.size())
		return false;
	if (known[piece] == 0) {
		bool flat = true;
		for (size_t i = 2; i < std::size(DPieceMicros[piece].mt) && flat; i++)
			flat = !LevelCelBlock { DPieceMicros[piece].mt[i] }.hasValue();
		known[piece] = flat ? 1 : 2;
	}
	return known[piece] == 1;
}

} // namespace

bool WoodsTileUsesCels(Point tile)
{
	if (leveltype != DTYPE_WOODS || OwnLook())
		return false;
	if (InMouthStamp(tile) || InRiver(tile))
		return true;
	const uint16_t shell = WoodsShellAt(tile);
	return shell != 0 && !ShellPieceIsFlat(shell);
}

bool WoodsCaveMouthHover(Point tile, Point &entrance, const char *&text)
{
	if (leveltype != DTYPE_WOODS || !setlevel || !InMouth(tile))
		return false;
	static std::string hover;
	hover = std::string("Down to ") + WoodsCaveName(WoodsCaveOnFloor(FloorIndex(setlvlnum), AreaOfLevel(setlvlnum)));
	entrance = MouthEntrance;
	text = hover.c_str();
	return true;
}

int WoodsBandFloor()
{
	if (setlevel && IsActCaveLevel(setlvlnum))
		return WoodsCaveRung();
	if (!setlevel || !IsOutdoorActLevel(setlvlnum))
		return 1;
	const int rung = Game.rung[AreaIndex(AreaOfLevel(setlvlnum))][FloorIndex(setlvlnum)];
	return rung == 0 ? 1 : rung;
}

namespace {

/**
 * @brief The user's Mob Roster (Woods Levels Designer, 2026-10-14: "If i tick a mob, you can use it on Woods Act"): the 40 kinds
 * ticked there that are not bosses. Only these walk the woods and their caves.
 */
constexpr _monster_id WoodsKinds[] = {
	MT_NZOMBIE,
	MT_RFALLSP, MT_DFALLSP, MT_YFALLSP, MT_BFALLSP,
	MT_WSKELAX, MT_TSKELAX, MT_RSKELAX, MT_XSKELAX,
	MT_RFALLSD, MT_DFALLSD, MT_YFALLSD, MT_BFALLSD,
	MT_NSCAV, MT_BSCAV, MT_WSCAV, MT_YSCAV,
	MT_WSKELBW, MT_TSKELBW, MT_RSKELBW, MT_XSKELBW,
	MT_WSKELSD, MT_TSKELSD, MT_RSKELSD, MT_XSKELSD,
	MT_LRDSAYTR,
	MT_NGOATMC, MT_BGOATMC, MT_RGOATMC, MT_GGOATMC,
	MT_NGOATBW, MT_BGOATBW, MT_RGOATBW, MT_GGOATBW,
	MT_FAT, MT_TOAD,
	MT_HORNED, MT_MUDRUN, MT_FROSTC, MT_OBLORD,
	MT_SUCCUBUS,
};

/**
 * @brief The desert's Mob Roster (the Orcl Act Level Designer's desert roster, saved by the user 2026-10-16 11:54): the 28 kinds ticked
 * there that are not bosses. Only these walk the desert's floors and its caves.
 */
constexpr _monster_id DesertKinds[] = {
	MT_RFALLSP, MT_RFALLSD,
	MT_WSKELAX, MT_TSKELAX, MT_WSKELBW, MT_TSKELBW, MT_WSKELSD, MT_TSKELSD,
	MT_NSCAV, MT_YSCAV,
	MT_UNSEEN, MT_GLOOM,
	MT_BGOATMC, MT_BGOATBW,
	MT_TOAD, MT_YMAGMA,
	MT_HORNED, MT_MUDRUN,
	MT_GARGOYLE, MT_BSNAKE,
	MT_NBLACK, MT_UNRAV, MT_SUCCUBUS,
	MT_STINGER, MT_GRAVEDIG, MT_TOMBRAT, MT_SKLWING, MT_BICLOPS,
};

/**
 * @brief The Frost Caves' Mob Roster, each kind with its look (the Orcl Act Level Designer's Frost Caves roster): until the user saves
 * his picks there, the woods' kinds - their ice-dwellers among them - all in Rime, the game's own frozen look.
 */
struct FrostMobRow {
	_monster_id kind;
	FrostLook look;
};
constexpr FrostMobRow FrostKinds[] = {
	{ MT_NZOMBIE, FrostLook::Rime },
	{ MT_WSKELAX, FrostLook::Rime }, { MT_TSKELAX, FrostLook::Rime }, { MT_RSKELAX, FrostLook::Rime }, { MT_XSKELAX, FrostLook::Rime },
	{ MT_NSCAV, FrostLook::Rime }, { MT_BSCAV, FrostLook::Rime }, { MT_WSCAV, FrostLook::Rime }, { MT_YSCAV, FrostLook::Rime },
	{ MT_WSKELBW, FrostLook::Rime }, { MT_TSKELBW, FrostLook::Rime }, { MT_RSKELBW, FrostLook::Rime }, { MT_XSKELBW, FrostLook::Rime },
	{ MT_WSKELSD, FrostLook::Rime }, { MT_TSKELSD, FrostLook::Rime }, { MT_RSKELSD, FrostLook::Rime }, { MT_XSKELSD, FrostLook::Rime },
	{ MT_NGOATMC, FrostLook::Rime }, { MT_BGOATMC, FrostLook::Rime }, { MT_RGOATMC, FrostLook::Rime }, { MT_GGOATMC, FrostLook::Rime },
	{ MT_NGOATBW, FrostLook::Rime }, { MT_BGOATBW, FrostLook::Rime }, { MT_RGOATBW, FrostLook::Rime }, { MT_GGOATBW, FrostLook::Rime },
	{ MT_FAT, FrostLook::Rime }, { MT_TOAD, FrostLook::Rime },
	{ MT_HORNED, FrostLook::Rime }, { MT_MUDRUN, FrostLook::Rime }, { MT_FROSTC, FrostLook::Rime }, { MT_OBLORD, FrostLook::Rime },
	{ MT_SUCCUBUS, FrostLook::Rime },
};
/** @brief The Frost Caves' kinds alone, in FrostKinds' order (AreaKinds' list). */
const std::array<_monster_id, std::size(FrostKinds)> &FrostKindList()
{
	static const std::array<_monster_id, std::size(FrostKinds)> list = [] {
		std::array<_monster_id, std::size(FrostKinds)> out {};
		for (size_t i = 0; i < out.size(); i++)
			out[i] = FrostKinds[i].kind;
		return out;
	}();
	return list;
}

/** @brief The Mob Roster of the area the current level belongs to (its floors and its caves). */
std::pair<const _monster_id *, size_t> AreaKinds(OrclArea area)
{
	if (area == OrclArea::Frost)
		return { FrostKindList().data(), FrostKindList().size() };
	if (area == OrclArea::Desert)
		return { DesertKinds, std::size(DesertKinds) };
	return { WoodsKinds, std::size(WoodsKinds) };
}

struct WoodsBossRow {
	_monster_id kind;
	unsigned sizePercent;
	/**
	 * A rift roster row this boss wears (user, 2026-10-18: "Use these bosses also as bosses in orcl act areas"): its name, colour,
	 * size, trick, escort; nullptr for the four of old. Then the area's own colour over the row's (Tint::None keeps the row's) and
	 * the area's own trick (-1 keeps it), with what that trick raises (MT_INVALID keeps the row's).
	 */
	const char *roster = nullptr;
	Tint tint = Tint::None;
	uint32_t rgb = 0;
	int signature = -1;
	_monster_id summon = MT_INVALID;
};
constexpr int OwnTrick(BossSignature signature) { return static_cast<int>(signature); }
/**
 * @brief The bosses of the Mob Roster's notes ("use as boss. scaled up to be diablo size"), each with the size that makes him
 * Diablo's height - the designer's sprite heights: Diablo 151 px, the King 118, the Butcher 69, the Reaper 86. The fifth note,
 * oldboned, is on a kind left unticked (and Hellfire's unused one, with no sounds of its own), so it is not among them.
 */
constexpr WoodsBossRow WoodsBosses[] = {
	{ MT_SKING, 128 },
	{ MT_CLEAVER, 219 },
	{ MT_REAPER, 176 },
	{ MT_DIABLO, 100 },
	// Diablo II's first act and III's, the Lord of Hatred's jungle and Lilith (v1.13.090), as the rift roster has them.
	{ MT_HLSPWN, 0, "Andariel, Maiden of Anguish" },
	{ MT_SUCCUBUS, 0, "The Countess" },
	{ MT_SNOWWICH, 0, "Maghda, the Moth Witch" },
	{ MT_GZOMBIE, 0, "Pindleskin" },
	{ MT_ADVOCATE, 0, "Mephisto, Lord of Hatred" },
	{ MT_SOLBRNR, 0, "Lilith, Queen of the Succubi" },
	{ MT_DIABLO, 0, "Diablo, Lord of Terror" },
};

/**
 * @brief The desert's bosses, the woods' rule (user, 2026-10-16: "check which were used as bosses in woods and apply same rule here"):
 * of the kinds ticked on the desert's roster, the ones the woods use as bosses - the Skeleton King, the Butcher and the Reaper (Diablo
 * is not ticked there) - each drawn at Diablo's height as in the woods.
 */
constexpr WoodsBossRow DesertBosses[] = {
	{ MT_SKING, 128 },
	{ MT_CLEAVER, 219 },
	{ MT_REAPER, 176 },
	// Diablo II's and III's second acts (v1.13.090), in the desert's colours (user, 2026-10-18: "Desert ones to be desert
	// tinted"): sand, ochre, the sun's gold.
	{ MT_XSKELAX, 0, "Radament", Tint::Hue, 0xC8A060 },
	{ MT_COUNSLR, 0, "The Summoner", Tint::Hue, 0xD8B070 },
	{ MT_MAGISTR, 0, "Zoltun Kulle", Tint::Glint, 0xE8C060 },
	{ MT_SUCCUBUS, 0, "Adria, the Witch", Tint::Hue, 0xB0804A },
};

/**
 * @brief The Frost Caves' bosses (v1.13.090; until now the woods' list): Diablo II's fifth act and Diablo III's Arreat, and the
 * Lord of Terror himself, all frost (user, 2026-10-18: "Frost bosses to be frost themed - tint and skills"): ice colours, a cold
 * trick each, their rings ice (FireRing) and their shots ice (FrostMissileFor) down here.
 */
constexpr WoodsBossRow FrostBosses[] = {
	{ MT_DIABLO, 0, "Diablo, Lord of Terror", Tint::Glint, 0x9AD8FF, OwnTrick(BossSignature::FrostNovaRaise) },
	{ MT_CABALIST, 0, "Nihlathak", Tint::Glint, 0xB8E8FF, OwnTrick(BossSignature::FrostNovaRaise) },
	{ MT_GUARD, 0, "Lister the Tormentor", Tint::Hue, 0x8AAAD8, OwnTrick(BossSignature::ChillOnHit) },
	{ MT_DEATHW, 0, "Izual, the Fallen Angel", Tint::Glint, 0x8AB4FF, OwnTrick(BossSignature::ChillOnHit) },
	{ MT_FAT, 0, "Ghom, Lord of Gluttony", Tint::Hue, 0x7AA8C8, OwnTrick(BossSignature::FrostNovaRaise) },
	{ MT_MEGA, 0, "Rakanoth, Lord of Despair", Tint::Hue, 0x5A7AB8, OwnTrick(BossSignature::BlinkBoltRing) },
	{ MT_UNSEEN, 0, "Iskatu, the Shadow of Terror", Tint::Hue, 0x6A7AAA, OwnTrick(BossSignature::Split) },
	{ MT_BALROG, 0, "Azmodan, Lord of Sin", Tint::Hue, 0x9CC8F0, OwnTrick(BossSignature::FrostNovaRaise) },
};

/** @brief The bosses @p area's last floor may roll. */
std::pair<const WoodsBossRow *, size_t> AreaBosses(OrclArea area)
{
	if (area == OrclArea::Desert)
		return { DesertBosses, std::size(DesertBosses) };
	if (area == OrclArea::Frost)
		return { FrostBosses, std::size(FrostBosses) };
	return { WoodsBosses, std::size(WoodsBosses) };
}

bool WoodsKindAvailable(const MonsterData &data)
{
	if (data.availability == MonsterAvailability::Never)
		return false;
	// Hellfire's own (the Stingers, the Lashworm, the Tomb Rats ...) only in a Hellfire game: Diablo has not their sprites.
	if (!gbIsHellfire && &data >= &MonstersData[MT_HELLBOAR])
		return false;
	return !(gbIsSpawn && data.availability == MonsterAvailability::Retail);
}

/**
 * @brief A kind's own floors on the ladder (1-16): the Hive's and the Crypt's eight below their number (LadderFloorOf), as their
 * monsters stand in a rift; Diablo's row says 26 to keep him out of the rosters, and his floor is 16 (audit round 1: the boss
 * scaled from 26 was a flat quarter on most rungs, and the Satyr Lord - 21-22 - came only on rung 16).
 */
int LadderMin(const MonsterData &data)
{
	return &data == &MonstersData[MT_DIABLO] ? 16 : LadderFloorOf(data.minDunLvl);
}
int LadderMax(const MonsterData &data)
{
	return &data == &MonstersData[MT_DIABLO] ? 16 : LadderFloorOf(data.maxDunLvl);
}

/** @brief How many floors @p data's own band stands from @p floor: 0 inside it. */
int BandGap(const MonsterData &data, int floor)
{
	if (floor < LadderMin(data))
		return LadderMin(data) - floor;
	if (floor > LadderMax(data))
		return floor - LadderMax(data);
	return 0;
}

/** @brief 100 * 1.12^@p gap, in whole percent - a floor's worth of life and damage per step. */
int FloorStepPercent(int gap)
{
	return static_cast<int>(std::lround(100.0 * std::pow(1.12, static_cast<double>(gap))));
}

} // namespace

FrostLook FrostLookFor(int kind)
{
	if (!setlevel || (!IsFrostLevel(setlvlnum) && !IsFrostRiverLevel(setlvlnum)))
		return FrostLook::None;
	for (const FrostMobRow &row : FrostKinds)
		if (row.kind == kind)
			return row.look;
	// A kind off the roster (a boss, a rift's guest): the game's own frozen look, so nothing in the Frost Caves is warm.
	return FrostLook::Rime;
}

bool WoodsAcceptsMonster(const MonsterData &data)
{
	if (!WoodsKindAvailable(data))
		return false;
	const auto [kindsFirst, kindsCount] = AreaKinds(AreaOfLevel(setlvlnum));
	const _monster_id *const kindsLast = kindsFirst + kindsCount;
	bool listed = false;
	for (const _monster_id *kind = kindsFirst; kind != kindsLast; kind++)
		listed = listed || &data == &MonstersData[*kind];
	if (!listed)
		return false;
	// The nearest gap that gives eight kinds to choose from (dev note, 2026-10-14: "mix and diversify mobs on the levels. they
	// are now all the same" - four of one rung were often four goatmen): ScaleWoodsMonster brings each to the rung.
	const int floor = WoodsBandFloor();
	int reach = 0;
	for (;; reach++) {
		int kinds = 0;
		for (const _monster_id *kind = kindsFirst; kind != kindsLast; kind++)
			kinds += WoodsKindAvailable(MonstersData[*kind]) && BandGap(MonstersData[*kind], floor) <= reach ? 1 : 0;
		if (kinds >= 8 || reach >= 32)
			break;
	}
	return BandGap(data, floor) <= reach;
}

namespace {

/** @brief The row the area being built or stood in has drawn for its boss, or nullptr. */
const WoodsBossRow *PickedWoodsBoss()
{
	// Its last floor's seed (each area its own pick, each new game anew).
	const uint32_t seed = Game.seed[AreaIndex(AreaOfLevel(setlvlnum))][WoodsFloors - 1];
	if (seed == 0)
		return nullptr;
	// The seed's high bits: its low ones also build the floor, and a pick tied to them would tie the boss to the map. Only the
	// kinds this game has the sprites of (the shareware has no Reaper and no Diablo): the next one on where the pick is not.
	const auto [bosses, count] = AreaBosses(AreaOfLevel(setlvlnum));
	const size_t first = (seed >> 20) % count;
	for (size_t k = 0; k < count; k++) {
		const WoodsBossRow &row = bosses[(first + k) % count];
		const MonsterData &data = MonstersData[row.kind];
		if (!(gbIsSpawn && data.availability == MonsterAvailability::Retail) && (gbIsHellfire || row.kind < MT_HELLBOAR))
			return &row;
	}
	return nullptr;
}

} // namespace

int WoodsBossKind()
{
	const WoodsBossRow *row = PickedWoodsBoss();
	return row != nullptr ? row->kind : -1;
}

namespace {

/** @brief @p row's roster row in its area's colours and trick, into @p def; false for one of the four of old or a missing name. */
bool RosterDefFor(const WoodsBossRow &row, RiftBossDef &def)
{
	if (row.roster == nullptr)
		return false;
	const std::optional<size_t> at = FindRiftBoss(row.roster);
	if (!at)
		return false;
	def = RiftBossAt(*at);
	if (row.tint != Tint::None) {
		def.tint = row.tint;
		def.rgb = row.rgb;
	}
	if (row.signature >= 0)
		def.signature = static_cast<BossSignature>(row.signature);
	if (row.summon != MT_INVALID)
		def.summon = row.summon;
	return true;
}

} // namespace

const RiftBossDef *WoodsBossRosterDef()
{
	const WoodsBossRow *row = PickedWoodsBoss();
	static RiftBossDef def;
	if (row == nullptr || !RosterDefFor(*row, def))
		return nullptr;
	return &def;
}

size_t AreaBossCountForTest(OrclArea area)
{
	return AreaBosses(area).second;
}

bool AreaBossForTest(OrclArea area, size_t index, int &kind, RiftBossDef &def)
{
	const auto [bosses, count] = AreaBosses(area);
	if (index >= count)
		return false;
	kind = bosses[index].kind;
	return RosterDefFor(bosses[index], def);
}

bool IsWoodsBoss(const Monster &monster)
{
	if (!setlevel || !IsActBossLevel(setlvlnum) || monster.isPlayerMinion())
		return false;
	const WoodsBossRow *row = PickedWoodsBoss();
	if (row == nullptr)
		return false;
	// A roster boss is known by his slot (his body may walk the floor as an ordinary kind too: the Succubi, the Fat Overlord).
	if (row->roster != nullptr)
		return ActBossFor(monster) != nullptr;
	return monster.type().type == row->kind;
}

int WoodsFamilyOf(int kind)
{
	// The sprite's folder, by its first three letters: "falspear" and "falsword" are both the Fallen, "skelaxe" and "skelbow"
	// both skeletons, "goatmace" and "goatbow" both goatmen.
	const char *path = MonstersData[kind].assetsSuffix;
	if (path == nullptr)
		return kind;
	int family = 0;
	for (int i = 0; i < 3 && path[i] != '\0'; i++)
		family = family * 256 + static_cast<unsigned char>(path[i]);
	return family;
}

unsigned WoodsBossSizePercent(const Monster &monster)
{
	if (!IsWoodsBoss(monster))
		return 0;
	// A roster boss's size is his row's (RiftBossSizePercent answers it first).
	const WoodsBossRow *row = PickedWoodsBoss();
	return row != nullptr && row->roster == nullptr ? row->sizePercent : 0;
}

std::optional<Point> WoodsBossSpot()
{
	// The open tile nearest six steps in from the way on: inside the fence ring (its cells' tiles and the box's edge outside it
	// are off - audit round 1: the scan began at the map's edge, west of the fence), off the river, the cave mouth and the
	// houses. He guards the way out of the woods.
	const int fenceIn = Lo + 2 * (RingLo + 1);    // the first tile inside the ring
	const int fenceOut = Lo + 2 * RingHi - 1;     // the last
	// In the desert's Tomb, before the Sun-Eater's altar (RfA-74), facing it from the arena's open front.
	// The Throne of Winter's boss before the throne, between the rows (RfA-82).
	const Point aim = Desert && ArenaCentre != Point {} ? ArenaCentre + Displacement { 2, 2 }
	    : Frost && ArenaCentre != Point {}          ? ArenaCentre + Displacement { -1, 0 }
	                                                : WayOn + Displacement { 6, 0 };
	for (int radius = 0; radius <= 40; radius++) {
		for (int dx = -radius; dx <= radius; dx++) {
			for (int dy = -radius; dy <= radius; dy++) {
				if (std::max(std::abs(dx), std::abs(dy)) != radius)
					continue;
				const Point p = aim + Displacement { dx, dy };
				if (p.x < fenceIn || p.y < fenceIn || p.x > fenceOut || p.y > fenceOut || IsTileSolid(p) || dMonster[p.x][p.y] != 0
				    || dPlayer[p.x][p.y] != 0 || dObject[p.x][p.y] != 0 || InRiver(p) || InMouth(p))
					continue;
				bool inHouse = false;
				for (int i = 0; i < HouseCount && !inHouse; i++)
					inHouse = InsideHouse(Houses[i], p) || WoodsShellAt(p) != 0;
				if (!inHouse)
					return p;
			}
		}
	}
	return std::nullopt;
}

int WoodsBossDamagePercent(const Monster &monster)
{
	if (!IsWoodsBoss(monster))
		return 100;
	return std::max(25, FloorStepPercent(WoodsBandFloor() - LadderMax(monster.data()))) * 5 / 4;
}

namespace {

/** @brief A kind brought to @p rung: up for one whose floors end above it, down (not below half) for one whose floors start below. */
void ScaleKindToRung(Monster &monster, int rung)
{
	const MonsterData &data = monster.data();
	// A kind's own floors are on the ladder's sixteen rungs: Na-Krul's 31 sits at 23 there, so every rung would have halved him
	// (audit, 2026-10-17).
	const int lo = std::min(LadderMin(data), RungsPerDifficulty);
	const int hi = std::min(LadderMax(data), RungsPerDifficulty);
	const int gap = rung > hi ? rung - hi : rung < lo ? rung - lo : 0;
	if (gap == 0)
		return;
	const int pct = std::max(50, FloorStepPercent(gap));
	monster.maxHitPoints = std::max(64, static_cast<int>(static_cast<int64_t>(monster.maxHitPoints) * pct / 100));
	monster.hitPoints = monster.maxHitPoints;
	ScaleMonsterDamage(monster, pct, pct);
	if (pct > 100)
		monster.armorClass = static_cast<uint8_t>(std::min(255, monster.armorClass + (pct - 100) / 6));
}

} // namespace

void ScaleHellfireFloorMonsters()
{
	if (setlevel || currlevel < 17 || currlevel > 24)
		return;
	const int rung = HellfireFloorRung(currlevel);
	if (rung == 0)
		return;
	for (size_t i = 0; i < ActiveMonsterCount; i++) {
		Monster &monster = Monsters[ActiveMonsters[i]];
		if (monster.isPlayerMinion() || monster.type().type == MT_GOLEM)
			continue;
		ScaleKindToRung(monster, rung);
	}
}

void ScaleWoodsMonster(Monster &monster)
{
	// A Hive or Crypt floor's raised dead, clones and spawn come to its rung as its own monsters did (audit, 2026-10-17: only the
	// floor's first population was scaled).
	if (!setlevel && currlevel >= 17 && currlevel <= 24) {
		if (const int rung = HellfireFloorRung(currlevel); rung != 0 && !monster.isPlayerMinion() && monster.type().type != MT_GOLEM)
			ScaleKindToRung(monster, rung);
		return;
	}
	if (!setlevel || !IsOrclActLevel(setlvlnum) || monster.isPlayerMinion() || monster.type().type == MT_GOLEM)
		return;
	const int floor = WoodsBandFloor();
	const bool boss = IsWoodsBoss(monster);
	const MonsterData &data = monster.data();
	int pct;
	int lifePct;
	int damagePct;
	if (boss) {
		// Set to the rung from his own floors, down as well as up (Diablo is a floor-16 monster, the Butcher a floor-one), not
		// below a quarter; then a boss's share on top.
		const int gap = floor - LadderMax(data);
		pct = std::max(25, FloorStepPercent(gap));
		lifePct = pct * 3;
		damagePct = pct * 5 / 4;
	} else {
		// Up for a kind whose floors end above the rung, down (not below half) for one whose floors start below it: the roster
		// reaches either way for its eight kinds.
		const int gap = floor > LadderMax(data) ? floor - LadderMax(data) : floor < LadderMin(data) ? floor - LadderMin(data) : 0;
		if (gap == 0)
			return;
		pct = std::max(50, FloorStepPercent(gap));
		lifePct = pct;
		damagePct = pct;
	}
	const auto scale = [](int value, int percent) { return static_cast<int>(static_cast<int64_t>(value) * percent / 100); };
	monster.maxHitPoints = std::max(64, scale(monster.maxHitPoints, lifePct));
	monster.hitPoints = monster.maxHitPoints;
	ScaleMonsterDamage(monster, damagePct, damagePct);
	if (pct > 100)
		monster.armorClass = static_cast<uint8_t>(std::min(255, monster.armorClass + (pct - 100) / 6));
}

bool IsChampionKind(int type)
{
	for (size_t u = 0; UniqueMonstersData[u].mtype != -1; u++) {
		const UniqueMonsterData &row = UniqueMonstersData[u];
		if (row.mtype == type && row.mtalkmsg == TEXT_NONE)
			return true;
	}
	return false;
}

uint32_t WoodsRosterSeed()
{
	if (setlevel && IsActCaveLevel(setlvlnum))
		return WoodsCaveRosterSeed();
	return Game.seed[AreaIndex(AreaOfLevel(setlvlnum))][FloorIndex(setlvlnum)] ^ 0x0AC7U;
}

Point WoodsEntryTile()
{
	return ViewPosition;
}

const char *WoodsFloorName(_setlevels level)
{
	constexpr const char *Names[OrclAreaCount][WoodsFloors] = {
		{ "The Greywood Verge", "Thornmoor", "The Hanging Wood", "Witchbark Hollow" },
		{ "The Salt Road", "The Ash Dunes", "The Glass Flats", "Tomb of the Sun-Eater" },
		{ "The Rime Gate", "The Glacial Hollows", "The Frostbound Deep", "The Throne of Winter" },
	};
	return Names[AreaIndex(AreaOfLevel(level))][FloorIndex(level)];
}

bool WoodsSpritesOverTiles(const Surface &out)
{
	return leveltype == DTYPE_WOODS && BuiltMixed && Art().loaded && !out.isIndexed();
}

bool WoodsArtActive(const Surface &out)
{
	return leveltype == DTYPE_WOODS && !BuiltVanilla && Art().loaded && !out.isIndexed();
}

namespace {

/** @brief @p c scaled by @p k (0..2), each channel clamped. */
uint32_t ScaleRgb(uint32_t c, float kr, float kg, float kb)
{
	const auto ch = [](uint32_t v, float k) { return static_cast<uint32_t>(std::clamp(static_cast<float>(v) * k, 0.0F, 255.0F)); };
	return (ch((c >> 16) & 0xFF, kr) << 16) | (ch((c >> 8) & 0xFF, kg) << 8) | ch(c & 0xFF, kb);
}

uint32_t MixRgb(uint32_t c, float r, float g, float b, float a)
{
	a = std::clamp(a, 0.0F, 1.0F);
	const auto ch = [a](uint32_t v, float to) { return static_cast<uint32_t>(std::clamp(static_cast<float>(v) + (to - static_cast<float>(v)) * a, 0.0F, 255.0F)); };
	return (ch((c >> 16) & 0xFF, r) << 16) | (ch((c >> 8) & 0xFF, g) << 8) | ch(c & 0xFF, b);
}

/**
 * @brief What the desert's ground holds that no texture paints, drawn into it at the pixel's tile-space place (@p u, @p v) over its
 * ground @p c (user, 2026-10-16: "Try to draw an in-game Sand Hole which takes player to Desert Caves"):
 * - the oasis pool: dark still water, deeper to its middle, the grey sky on its far side, a few glints, its mud wet round it;
 * - the sand hole: a funnel of the floor's own sand slumping into a black pit - its inner wall lit where it faces the top-left sun
 *   and shadowed where it turns away, streaked where the sand slides, a raised lip, and the sand round it cracked and sunken.
 */
uint32_t DesertGroundFeature(uint32_t c, float u, float v, int wx, int wy)
{
	if (Frost) {
		// Inside the rock, black: the depth blended between the four tiles round the pixel, so the dark comes on softly behind the
		// wall's face and is whole two tiles in.
		const int i0 = std::clamp(static_cast<int>(std::floor(u)), 0, MAXDUNX - 2);
		const int j0 = std::clamp(static_cast<int>(std::floor(v)), 0, MAXDUNY - 2);
		const float fx = std::clamp(u - static_cast<float>(i0), 0.0F, 1.0F), fy = std::clamp(v - static_cast<float>(j0), 0.0F, 1.0F);
		const auto depth = [](int x, int y) { return std::min<float>(static_cast<float>(FrostRockDepth[x][y]), 3.0F); };
		const float top = depth(i0, j0) + (depth(i0 + 1, j0) - depth(i0, j0)) * fx;
		const float bottom = depth(i0, j0 + 1) + (depth(i0 + 1, j0 + 1) - depth(i0, j0 + 1)) * fx;
		const float d = top + (bottom - top) * fy;
		const float dark = std::clamp((d - 0.7F) / 1.3F, 0.0F, 1.0F);
		if (dark > 0.0F) {
			const float k = 1.0F - 0.94F * dark;
			c = ScaleRgb(c, k * 0.92F, k * 0.96F, k);
		}
	}
	// The sun's direction in tile space: toward the screen's top left, (-1, -0.4) normalised.
	constexpr float Lu = -0.928F, Lv = -0.371F;
	if (OasisRadius > 0.0F) {
		const float du = u - static_cast<float>(OasisCentre.x), dv = v - static_cast<float>(OasisCentre.y);
		// Most of the ground is nowhere near the pool: no square root there.
		const float reach = OasisRadius + 0.7F;
		const float d2 = du * du + dv * dv;
		const float d = d2 < reach * reach ? std::sqrt(d2) : reach;
		if (d < OasisRadius + 0.7F) {
			if (d >= OasisRadius) {
				// The wet edge: the mud darker and a little sheen, fading out over most of a tile.
				const float wet = 1.0F - (d - OasisRadius) / 0.7F;
				c = ScaleRgb(c, 1.0F - 0.38F * wet * wet, 1.0F - 0.36F * wet * wet, 1.0F - 0.30F * wet * wet);
			} else {
				const float depth = d / OasisRadius; // 0 the middle, 1 the shore
				float r = 16.0F + 18.0F * depth, g = 24.0F + 20.0F * depth, b = 27.0F + 18.0F * depth;
				// The sky in it on its far side (up the screen: u + v smaller), grey-blue, brighter toward the far shore.
				const float far = std::clamp(-(du + dv) / (2.0F * OasisRadius) + 0.15F, 0.0F, 1.0F);
				r += 34.0F * far * far;
				g += 42.0F * far * far;
				b += 48.0F * far * far;
				// Still water's few glints: thin world-pinned lines, broken.
				const float line = std::sin(static_cast<float>(wy) * 0.85F + std::sin(static_cast<float>(wx) * 0.045F) * 5.0F);
				if (line > 0.94F && ((wx / 7 + wy / 3) & 3) != 0) {
					r += 20.0F;
					g += 24.0F;
					b += 26.0F;
				}
				const float shore = std::clamp((depth - 0.86F) / 0.14F, 0.0F, 1.0F);
				const uint32_t water = (static_cast<uint32_t>(std::min(r, 255.0F)) << 16) | (static_cast<uint32_t>(std::min(g, 255.0F)) << 8) | static_cast<uint32_t>(std::min(b, 255.0F));
				c = MixRgb(water, static_cast<float>((c >> 16) & 0xFF) * 0.55F, static_cast<float>((c >> 8) & 0xFF) * 0.55F, static_cast<float>(c & 0xFF) * 0.55F, shore);
			}
		}
	}
	// The Glass Flats' craters (RfA-72b) and the Frost Caves' light shafts (RfA-79b): laid over the ground as the painted ways down
	// are, each by its soft alpha.
	for (int set = 0; set < 2; set++) {
		const std::vector<Point> &spots = set == 0 ? Craters : LightShafts;
		const Picture &crater = set == 0 ? Art().crater : Art().shaft;
		if (spots.empty() || crater.px.empty())
			continue;
		for (const Point at : spots) {
			const float du = u - static_cast<float>(at.x), dv = v - static_cast<float>(at.y);
			if (std::abs(du) > 3.0F || std::abs(dv) > 3.0F)
				continue;
			const int px = crater.footX + static_cast<int>(std::lround((du - dv) * 32.0F));
			const int py = crater.footY + static_cast<int>(std::lround((du + dv) * 16.0F));
			if (px < 0 || py < 0 || px >= crater.w || py >= crater.h)
				continue;
			const uint32_t d = crater.px[static_cast<size_t>(py) * crater.w + px];
			const uint32_t a = d >> 24;
			if (a == 0)
				continue;
			const auto mix = [a](uint32_t q, uint32_t r, int sh) { return ((((q >> sh) & 0xFF) * (255 - a) + ((r >> sh) & 0xFF) * a) / 255) & 0xFF; };
			c = (mix(c, d, 16) << 16) | (mix(c, d, 8) << 8) | mix(c, d, 0);
		}
	}
	if (const Picture *entrance = DesertEntrancePicture(); entrance != nullptr) {
		// The painted way down, its middle on the entrance tile's centre, laid over the sand by its soft alpha.
		const float du = u - static_cast<float>(MouthEntrance.x), dv = v - static_cast<float>(MouthEntrance.y);
		const int px = entrance->footX + static_cast<int>(std::lround((du - dv) * 32.0F));
		const int py = entrance->footY + static_cast<int>(std::lround((du + dv) * 16.0F));
		if (px < 0 || py < 0 || px >= entrance->w || py >= entrance->h)
			return c;
		const uint32_t d = entrance->px[static_cast<size_t>(py) * entrance->w + px];
		const uint32_t a = d >> 24;
		if (a == 0)
			return c;
		const auto mix = [a](uint32_t p, uint32_t q, int sh) { return ((((p >> sh) & 0xFF) * (255 - a) + ((q >> sh) & 0xFF) * a) / 255) & 0xFF; };
		return (mix(c, d, 16) << 16) | (mix(c, d, 8) << 8) | mix(c, d, 0);
	}
	if (MouthOnFloor) {
		const float du = u - static_cast<float>(MouthEntrance.x), dv = v - static_cast<float>(MouthEntrance.y);
		// The ragged rim is never less than 0.78 of the radius: past this nothing of the hole is drawn (audit, 2026-10-16).
		constexpr float Reach = SandHoleRadius * 1.9F / 0.78F;
		if (du * du + dv * dv >= Reach * Reach)
			return c;
		const float angle = std::atan2(dv, du);
		// A collapse, not a well: its rim wanders in and out round the hole (the first preview: a perfect ellipse read as a manhole).
		const float ragged = 1.0F + 0.11F * std::sin(angle * 3.0F + 1.3F) + 0.07F * std::sin(angle * 5.0F + 0.4F) + 0.04F * std::sin(angle * 11.0F + 2.1F);
		const float d = std::sqrt(du * du + dv * dv) / ragged;
		constexpr float R = SandHoleRadius;
		if (d < R * 1.9F) {
			const float nu = d > 0.001F ? du / (d * ragged) : 0.0F, nv = d > 0.001F ? dv / (d * ragged) : 0.0F;
			// Where the sand slides: thin radial streaks that wander.
			const float streak = 0.5F + 0.5F * std::sin(angle * 23.0F + std::sin(angle * 5.0F + d * 3.0F) * 2.2F + d * 4.0F);
			if (d < R * 0.42F) {
				// The pit: the dark of a hole in the ground, a little lighter at its edge.
				const float t = d / (R * 0.42F);
				const float k = 0.05F + 0.13F * t * t;
				c = ScaleRgb(c, k * 0.95F, k * 0.85F, k * 0.75F);
			} else if (d < R * 0.92F) {
				// The funnel's inner wall: lit where it faces the sun (its far side from the sun), shadowed where it faces away,
				// darker the deeper it goes, the slides' streaks over it.
				const float t = (d - R * 0.42F) / (R * 0.50F); // 0 at the pit, 1 at the lip
				const float lit = std::clamp(-(nu * Lu + nv * Lv), -1.0F, 1.0F);
				float k = 0.22F + 0.62F * t;
				k *= 0.80F + 0.32F * lit;
				k *= 0.86F + 0.18F * streak;
				c = ScaleRgb(c, k * 1.02F, k, k * 0.94F);
			} else if (d < R * 1.06F) {
				// The lip: the edge where the sand breaks over, a little raised, catching the sun on its top-left side - eased in and
				// out so it is a crest and not a drawn ring.
				const float lit = std::clamp(nu * Lu + nv * Lv, -1.0F, 1.0F);
				const float m = std::sin((d - R * 0.92F) / (R * 0.14F) * 3.1415927F);
				const float k = 1.0F + (0.03F + 0.09F * lit) * m;
				c = ScaleRgb(c, k, k, k * 0.98F);
			} else {
				// The sand round it: sunken toward the hole, sliding into it in streaks, cracked where it pulled away; fading out.
				const float t = (d - R * 1.06F) / (R * 0.84F); // 0 at the lip, 1 outside
				const float sink = (1.0F - t) * (1.0F - t);
				float k = 1.0F - 0.14F * sink - 0.08F * sink * streak;
				const float crack = std::abs(std::sin(angle * 7.0F + std::sin(d * 2.3F + angle * 3.0F) * 1.6F));
				if (crack < 0.06F * (1.0F - t) * (1.0F - t))
					k *= 0.66F;
				c = ScaleRgb(c, k, k, k * 0.99F);
			}
		}
	}
	return c;
}

/** @brief DrawWoodsGroundTile's body: the materials of @p tile and its neighbours, the texture pinned where @p world lies. */
void DrawGroundAt(const Surface &out, Point tile, Point world, Point target, int light, bool fullBright)
{
	const int brightness = (fullBright && light < LightsMax) || light < 0 ? 256 : BrightnessFor(light);
	// The tile's diamond: 31 rows ending on the tile's bottom line, as dun_render draws a floor's two triangles.
	const int originX = (world.x - world.y) * 32; // where the tile lies in the world, in pixels
	const int originY = (world.x + world.y) * 16;
	const int centreX = target.x + 32;
	const int centreY = target.y - 15;
	for (int k = 0; k < 31; k++) {
		const int y = target.y - k;
		if (y < 0 || y >= out.h())
			continue;
		const int inset = k < 16 ? 30 - 2 * k : 2 * (k - 15);
		const int x0 = std::max(target.x + inset, 0);
		const int x1 = std::min(target.x + 63 - inset, out.w() - 1);
		if (x0 > x1)
			continue;
		uint32_t *row = out.at<uint32_t>(0, y);
		const int wy = originY + (y - target.y) + (1 << 20);
		const int dy = y - centreY;
		for (int x = x0; x <= x1; x++) {
			const int wx = originX + (x - target.x) + (1 << 20);
			// Which materials this pixel takes, and how much of each (user, 2026-10-14: "identify sharp straight lines. they
			// indicate nonhumogenous stitching"): each material's share of the four tile centres round the pixel, bilinear in
			// tile space, so a patch of dry grass is a rounded blob and not a staircase of diamonds; nudged by the world-pinned
			// noise, so its edge wanders; and where two shares come close the two textures are mixed, not cut.
			const auto &n = EdgeNoise[static_cast<size_t>((wy >> 3) & (NoiseSize - 1)) * NoiseSize + ((wx >> 3) & (NoiseSize - 1))];
			const auto &n2 = EdgeNoise[static_cast<size_t>((wy >> 5) & (NoiseSize - 1)) * NoiseSize + ((wx >> 5) & (NoiseSize - 1))];
			const int dx = x - centreX;
			// The pixel in tile space: the tile's centre plus its offset in 1/64 tile.
			const float u = static_cast<float>(tile.x) + static_cast<float>(dx + 2 * dy) / 64.0F;
			const float v = static_cast<float>(tile.y) + static_cast<float>(2 * dy - dx) / 64.0F;
			const int i0 = static_cast<int>(std::floor(u));
			const int j0 = static_cast<int>(std::floor(v));
			const float fx = u - static_cast<float>(i0);
			const float fy = v - static_cast<float>(j0);
			uint8_t mats[4];
			float weights[4];
			int count = 0;
			for (int c = 0; c < 4; c++) {
				const int ti = std::clamp(i0 + (c & 1), 0, MAXDUNX - 1);
				const int tj = std::clamp(j0 + (c >> 1), 0, MAXDUNY - 1);
				const float w = ((c & 1) != 0 ? fx : 1.0F - fx) * ((c >> 1) != 0 ? fy : 1.0F - fy);
				const uint8_t m = Ground[ti][tj];
				int k2 = 0;
				while (k2 < count && mats[k2] != m)
					k2++;
				if (k2 == count) {
					mats[count] = m;
					weights[count++] = 0.0F;
				}
				weights[k2] += w;
			}
			// The noise: each material's share pushed up or down by its own channel, fine and coarse.
			for (int k2 = 0; k2 < count; k2++) {
				const int fine = (mats[k2] & 1) != 0 ? n[0] : n[1];
				const int coarse = (mats[k2] & 1) != 0 ? n2[1] : n2[0];
				// The desert's twice as far: its grounds lie in big soft patches, not the woods' fine weave of grasses.
				weights[k2] += (static_cast<float>(fine) * 0.0030F + static_cast<float>(coarse) * 0.0035F) * (mats[k2] == Path ? 0.6F : 1.0F) * (OwnLook() ? 1.9F : 1.0F);
			}
			int best = 0, second = -1;
			for (int k2 = 1; k2 < count; k2++) {
				if (weights[k2] > weights[best]) {
					second = best;
					best = k2;
				} else if (second < 0 || weights[k2] > weights[second]) {
					second = k2;
				}
			}
			const Picture &tex = Art().ground[mats[best]];
			uint32_t c = tex.px[static_cast<size_t>(wy % tex.h) * tex.w + (wx % tex.w)];
			{
				// The same texture again from far across it, mixed in by the coarse noise (the seam survey, 2026-10-14: the
				// litter's twigs and the grass's tufts fell on the same place in every tile, a lattice over a whole meadow).
				const int t = std::clamp(128 + n2[0] * 3, 0, 256);
				if (t > 8) {
					const uint32_t o = tex.px[static_cast<size_t>((wy + 357) % tex.h) * tex.w + ((wx + 211) % tex.w)];
					const auto mix = [t](uint32_t p, uint32_t q, int sh) { return ((((p >> sh) & 0xFF) * (256 - t) + ((q >> sh) & 0xFF) * t) >> 8) & 0xFF; };
					c = (mix(c, o, 16) << 16) | (mix(c, o, 8) << 8) | mix(c, o, 0);
				}
			}
			if (second >= 0) {
				// Mixed within a narrow band of the two shares, the second fading out as the first leads.
				const float lead = weights[best] - weights[second];
				const float a = std::clamp(0.5F - lead * 2.2F, 0.0F, 0.5F);
				if (a > 0.0F) {
					const Picture &other = Art().ground[mats[second]];
					const uint32_t o = other.px[static_cast<size_t>(wy % other.h) * other.w + (wx % other.w)];
					const int ai = static_cast<int>(a * 256.0F);
					const auto mix = [ai](uint32_t p, uint32_t q, int sh) { return ((((p >> sh) & 0xFF) * (256 - ai) + ((q >> sh) & 0xFF) * ai) >> 8) & 0xFF; };
					c = (mix(c, o, 16) << 16) | (mix(c, o, 8) << 8) | mix(c, o, 0);
				}
			}
			if (OwnLook())
				c = DesertGroundFeature(c, u, v, wx, wy);
			row[x] = brightness >= 256 ? (c & 0xFFFFFF) | 0x010101 : Shade(c, brightness);
		}
	}
}

} // namespace


namespace {

/** @brief One floor tile's finished picture: the 31 rows of the diamond, 64 wide, bottom row first; 0 off the diamond. */
struct FloorPicture {
	std::array<uint32_t, 64 * 31> px {};
};

/**
 * @brief The Mixed look's floor, made once and kept (the user's frame-rate complaint of 2026-10-14; DISABLED_WoodsFrameBenchmark
 * timed the per-frame grade and road at 72 ms and Tristram's floor cels under them at 13 ms, a Debug 1920 x 1080 frame): a tile's
 * ground never changes, so each is drawn whole - its cels at full light, the grade, the road - the first time it is seen, then
 * copied with its light. Grass far from the road by its piece (one picture serves every tile of that piece); a tile within two of
 * the road by itself, since the road's blend is its own. Rebuilt for a new floor and when the palette changes.
 */
std::unordered_map<uint16_t, std::unique_ptr<FloorPicture>> FloorByPiece;
std::unordered_map<uint32_t, std::unique_ptr<FloorPicture>> FloorByTile;
uint32_t FloorCachePalette = 0;
/** @brief Each tile's picture once it has been looked up (null: not yet, or the tile is not drawn from the cache). */
const FloorPicture *FloorOfTile[MAXDUNX][MAXDUNY];
/** @brief The tile was looked up and is drawn as it is (the river, the mouth, a cottage, a borrowed piece). */
bool FloorNotCached[MAXDUNX][MAXDUNY];
/**
 * @brief The Painted Art look's ground, a picture per tile once it is seen (user, 2026-10-14: the painted look for real, "so i can
 * test it in real life what we achieved with the size calibration"; DISABLED_WoodsFrameBenchmark timed its per-pixel blend at
 * 60 ms a Debug frame). At most PaintedCacheCap of them (8 KB each); past it the cache starts again.
 */
std::unique_ptr<FloorPicture> PaintedOfTile[MAXDUNX][MAXDUNY];
size_t PaintedCount = 0;
/**
 * @brief No cap: room for every tile of the map, about 100 MB at the most (audit round 3: a cap of 6000 under a 4K screen's ~8000
 * tiles cleared and rebuilt each frame). Freed with the floor, and when the hero leaves the woods (LoadGameLevel).
 */

bool BlitFloorPicture(const Surface &out, const FloorPicture &pic, Point target, int light, bool fullBright);

bool TileNearRoad(Point tile)
{
	if (InRiver(tile))
		return false;
	for (int dy = -2; dy <= 2; dy++)
		for (int dx = -2; dx <= 2; dx++) {
			const Point p = tile + Displacement { dx, dy };
			if (InDungeonBounds(p) && Ground[p.x][p.y] == Path && Tree[p.x][p.y] == NoTree)
				return true;
		}
	return false;
}

} // namespace

void ForgetWoodsMixedFloor()
{
	for (auto &column : PaintedOfTile)
		for (auto &picture : column)
			picture.reset();
	PaintedCount = 0;
	FloorByPiece.clear();
	FloorByTile.clear();
	std::memset(FloorOfTile, 0, sizeof(FloorOfTile));
	std::memset(FloorNotCached, 0, sizeof(FloorNotCached));
}

bool DrawWoodsMixedFloorCached(const Surface &out, Point tile, Point target, int light, bool fullBright)
{
	if (leveltype != DTYPE_WOODS || !BuiltMixed || out.isIndexed() || Art().ground[Path].px.empty() || pDungeonCels == nullptr)
		return false;
	if (FloorCachePalette != PaletteRgbGeneration) {
		ForgetWoodsMixedFloor();
		FloorCachePalette = PaletteRgbGeneration;
	}
	// A cottage comes and goes as the hero enters it: asked every frame. The rest is looked up once a floor.
	if (WoodsShellAt(tile) != 0 || FloorNotCached[tile.x][tile.y])
		return false;
	if (const FloorPicture *known = FloorOfTile[tile.x][tile.y]; known != nullptr)
		return BlitFloorPicture(out, *known, target, light, fullBright);
	if (InMouth(tile) || InRiver(tile) || IsBorrowedPiece(WoodsDrawnPiece(tile))) {
		FloorNotCached[tile.x][tile.y] = true; // drawn as they are: the water's colours cycle, the rest is not plain ground
		return false;
	}
	const uint16_t piece = WoodsDrawnPiece(tile);
	const bool own = TileNearRoad(tile);
	std::unique_ptr<FloorPicture> &slot = own ? FloorByTile[static_cast<uint32_t>(tile.y) * MAXDUNX + static_cast<uint32_t>(tile.x)] : FloorByPiece[piece];
	if (slot == nullptr) {
		// Drawn once: the piece's two floor micros at full light, then the grade and the road, into a tile-sized picture.
		OwnedSurface canvas = OwnedSurface::Rgb(64, 32);
		const Surface surface = canvas;
		for (int y = 0; y < 32; y++)
			std::fill_n(surface.at<uint32_t>(0, y), 64, 0U);
		const uint8_t *full = LightTables[0].data();
		for (int side = 0; side < 2; side++) {
			const LevelCelBlock block { DPieceMicros[piece].mt[side] };
			if (block.hasValue())
				RenderTile(surface, { side * 32, 31 }, block, MaskType::Solid, full);
		}
		DrawWoodsMixedGround(surface, tile, { 0, 31 }, -1, true);
		slot = std::make_unique<FloorPicture>();
		for (int k = 0; k < 31; k++) {
			const int inset = k < 16 ? 30 - 2 * k : 2 * (k - 15);
			const uint32_t *row = surface.at<uint32_t>(0, 31 - k);
			for (int x = inset; x <= 63 - inset; x++)
				slot->px[static_cast<size_t>(k) * 64 + x] = (row[x] & 0xFFFFFFU) | 0x01000000U; // the top byte marks the diamond
		}
	}
	FloorOfTile[tile.x][tile.y] = slot.get();
	return BlitFloorPicture(out, *slot, target, light, fullBright);
}

namespace {

bool BlitFloorPicture(const Surface &out, const FloorPicture &pic, Point target, int light, bool fullBright)
{
	const int brightness = (fullBright && light < LightsMax) ? 256 : light < 0 ? 256 : BrightnessFor(light);
	for (int k = 0; k < 31; k++) {
		const int y = target.y - k;
		if (y < 0 || y >= out.h())
			continue;
		const int inset = k < 16 ? 30 - 2 * k : 2 * (k - 15);
		const int x0 = std::max(inset, -target.x);
		const int x1 = std::min(63 - inset, out.w() - 1 - target.x);
		uint32_t *row = out.at<uint32_t>(0, y) + target.x;
		const uint32_t *src = &pic.px[static_cast<size_t>(k) * 64];
		if (brightness >= 256) {
			for (int x = x0; x <= x1; x++)
				row[x] = src[x] & 0xFFFFFFU;
		} else {
			// Red and blue in one multiply, green in another (the per-pixel Shade call was most of the copy in a Debug build).
			const uint32_t b = static_cast<uint32_t>(brightness);
			for (int x = x0; x <= x1; x++) {
				const uint32_t p = src[x];
				row[x] = ((((p & 0xFF00FFU) * b) >> 8) & 0xFF00FFU) | ((((p & 0x00FF00U) * b) >> 8) & 0x00FF00U) | 0x010101U;
			}
		}
	}
	return true;
}

} // namespace

void DrawWoodsGroundTile(const Surface &out, Point tile, Point target, int light, bool fullBright)
{
	if (out.isIndexed() || !InDungeonBounds(tile)) {
		DrawGroundAt(out, tile, tile, target, light, fullBright);
		return;
	}
	std::unique_ptr<FloorPicture> &slot = PaintedOfTile[tile.x][tile.y];
	if (slot == nullptr) {
		// Drawn once at full light into a tile-sized picture, then copied with the light every frame.
		OwnedSurface canvas = OwnedSurface::Rgb(64, 32);
		const Surface surface = canvas;
		for (int y = 0; y < 32; y++)
			std::fill_n(surface.at<uint32_t>(0, y), 64, 0U);
		DrawGroundAt(surface, tile, tile, { 0, 31 }, -1, true);
		auto picture = std::make_unique<FloorPicture>();
		for (int k = 0; k < 31; k++) {
			const int inset = k < 16 ? 30 - 2 * k : 2 * (k - 15);
			const uint32_t *row = surface.at<uint32_t>(0, 31 - k);
			for (int x = inset; x <= 63 - inset; x++)
				picture->px[static_cast<size_t>(k) * 64 + x] = (row[x] & 0xFFFFFFU) | 0x01000000U;
		}
		PaintedOfTile[tile.x][tile.y] = std::move(picture);
		PaintedCount++;
	}
	BlitFloorPicture(out, *PaintedOfTile[tile.x][tile.y], target, light, fullBright);
}

void RepaintWoodsCelGrass(const Surface &out, Point tile, Point target, int light, bool fullBright)
{
	// The river's banks and the cave mouth's rock are Tristram's pieces, and their grass is Tristram's: a hard line where it meets
	// the painted woods (user, 2026-10-14: "river to grass. underground entrance to grass."). Each of their floor pixels is
	// read as it was drawn - green or dry-gold where it is grass, grey where rock, blue-green where water - and the grass taken
	// over by the woods' own ground at the same light, the more the greener; the water, the stones and the earth are kept.
	if (leveltype != DTYPE_WOODS || out.isIndexed() || !WoodsArtActive(out) || !InDungeonBounds(tile) || !(InRiver(tile) || InMouthStamp(tile)))
		return;
	std::unique_ptr<FloorPicture> &slot = PaintedOfTile[tile.x][tile.y];
	if (slot == nullptr) {
		OwnedSurface canvas = OwnedSurface::Rgb(64, 32);
		const Surface surface = canvas;
		for (int y = 0; y < 32; y++)
			std::fill_n(surface.at<uint32_t>(0, y), 64, 0U);
		DrawGroundAt(surface, tile, tile, { 0, 31 }, -1, true);
		auto picture = std::make_unique<FloorPicture>();
		for (int k = 0; k < 31; k++) {
			const int inset = k < 16 ? 30 - 2 * k : 2 * (k - 15);
			const uint32_t *row = surface.at<uint32_t>(0, 31 - k);
			for (int x = inset; x <= 63 - inset; x++)
				picture->px[static_cast<size_t>(k) * 64 + x] = (row[x] & 0xFFFFFFU) | 0x01000000U;
		}
		slot = std::move(picture);
		PaintedCount++;
	}
	const FloorPicture &pic = *slot;
	const int brightness = (fullBright && light < LightsMax) ? 256 : light < 0 ? 256 : BrightnessFor(light);
	// The cave mouth's block is a square of Tristram's dark earth: over its outer two tiles the woods' ground takes it over
	// whatever its colour, all of it at the block's edge, none two tiles in - its rocks stand in the middle.
	const bool mouth = InMouthStamp(tile);
	// The river's banks are Tristram's dry earth, flat and grey (the seam survey, 2026-10-14): within three quarters of a tile
	// of the woods' ground beside the river the woods' ground takes it over, the more the nearer.
	const bool river = InRiver(tile);
	Point dry[8];
	int dryCount = 0;
	if (river) {
		for (int ny = -1; ny <= 1; ny++)
			for (int nx = -1; nx <= 1; nx++) {
				const Point p = tile + Displacement { nx, ny };
				if ((nx != 0 || ny != 0) && InDungeonBounds(p) && !InRiver(p))
					dry[dryCount++] = p;
			}
	}
	const float mouthLo = static_cast<float>(MouthOrigin.x) - 0.5F, mouthHiX = static_cast<float>(MouthOrigin.x + MouthWidth) - 0.5F;
	const float mouthLoY = static_cast<float>(MouthOrigin.y) - 0.5F, mouthHiY = static_cast<float>(MouthOrigin.y + MouthHeight) - 0.5F;
	for (int k = 0; k < 31; k++) {
		const int y = target.y - k;
		if (y < 0 || y >= out.h())
			continue;
		const int inset = k < 16 ? 30 - 2 * k : 2 * (k - 15);
		const int x0 = std::max(inset, -target.x);
		const int x1 = std::min(63 - inset, out.w() - 1 - target.x);
		uint32_t *row = out.at<uint32_t>(0, y) + target.x;
		const uint32_t *src = &pic.px[static_cast<size_t>(k) * 64];
		const int dy = 15 - k; // the row's offset from the tile's centre line, down the screen
		for (int x = x0; x <= x1; x++) {
			float edge = 0.0F;
			const int dx = x - 32;
			const float u = static_cast<float>(tile.x) + static_cast<float>(dx + 2 * dy) / 64.0F;
			const float v = static_cast<float>(tile.y) + static_cast<float>(2 * dy - dx) / 64.0F;
			if (mouth) {
				const float inside = std::min({ u - mouthLo, mouthHiX - u, v - mouthLoY, mouthHiY - v });
				edge = std::clamp(1.0F - inside / 2.0F, 0.0F, 1.0F);
			}
			for (int d = 0; d < dryCount; d++) {
				const float ex = std::max(0.0F, std::abs(u - static_cast<float>(dry[d].x)) - 0.5F);
				const float ey = std::max(0.0F, std::abs(v - static_cast<float>(dry[d].y)) - 0.5F);
				edge = std::max(edge, std::clamp(1.0F - std::sqrt(ex * ex + ey * ey) / 0.75F, 0.0F, 1.0F));
			}
			const uint32_t c = row[x];
			const int r = static_cast<int>((c >> 16) & 0xFF), g = static_cast<int>((c >> 8) & 0xFF), b = static_cast<int>(c & 0xFF);
			const int hi = std::max({ r, g, b });
			if (hi < 10 && edge <= 0.0F)
				continue;
			// Green over the other two, or a dry grass's gold (red and green together well over blue).
			const float green = hi > 0 ? static_cast<float>(g - std::max(r, b)) / static_cast<float>(hi) : 0.0F;
			const float gold = hi > 0 && g * 10 >= r * 8 ? static_cast<float>(std::min(r, g) - b) / static_cast<float>(hi) - 0.22F : -1.0F;
			float w = std::max({ (green - 0.03F) / 0.12F, gold / 0.18F, edge });
			// Round the hole, its dull dark earth too (the hole's own tile keeps its dark).
			if (mouth && tile != MouthEntrance) {
				const float dark = std::clamp((90.0F - static_cast<float>(hi)) / 40.0F, 0.0F, 1.0F);
				const float notBlue = std::clamp(static_cast<float>(std::max(r, g) + 6 - b) / 12.0F, 0.0F, 1.0F);
				w = std::max(w, dark * notBlue);
			}
			if (w <= 0.0F)
				continue;
			w = std::min(w, 1.0F);
			w = w * w * (3.0F - 2.0F * w);
			const uint32_t p = src[x];
			const uint32_t lit = brightness >= 256 ? p & 0xFFFFFFU : Shade(p, brightness);
			const int a = static_cast<int>(w * 256.0F);
			const auto mix = [a](int under, int over) { return (under * (256 - a) + over * a) >> 8; };
			row[x] = (c & 0xFF000000U) | (static_cast<uint32_t>(mix(r, static_cast<int>((lit >> 16) & 0xFF))) << 16)
			    | (static_cast<uint32_t>(mix(g, static_cast<int>((lit >> 8) & 0xFF))) << 8) | static_cast<uint32_t>(mix(b, static_cast<int>(lit & 0xFF)));
		}
	}
}

void RepaintWoodsCelGround(const Surface &out, Point tile, Point target, int light)
{
	if (leveltype != DTYPE_WOODS || out.isIndexed() || !WoodsArtActive(out) || !InDungeonBounds(tile) || !InMouthStamp(tile)
	    || !TileHasAny(dPiece[tile.x][tile.y], TileProperties::Solid))
		return;
	std::unique_ptr<FloorPicture> &slot = PaintedOfTile[tile.x][tile.y];
	if (slot == nullptr) {
		OwnedSurface canvas = OwnedSurface::Rgb(64, 32);
		const Surface surface = canvas;
		for (int y = 0; y < 32; y++)
			std::fill_n(surface.at<uint32_t>(0, y), 64, 0U);
		DrawGroundAt(surface, tile, tile, { 0, 31 }, -1, true);
		auto picture = std::make_unique<FloorPicture>();
		for (int k = 0; k < 31; k++) {
			const int inset = k < 16 ? 30 - 2 * k : 2 * (k - 15);
			const uint32_t *row = surface.at<uint32_t>(0, 31 - k);
			for (int x = inset; x <= 63 - inset; x++)
				picture->px[static_cast<size_t>(k) * 64 + x] = (row[x] & 0xFFFFFFU) | 0x01000000U;
		}
		slot = std::move(picture);
		PaintedCount++;
	}
	const FloorPicture &pic = *slot;
	const int brightness = light < 0 ? 256 : BrightnessFor(light);
	for (int k = 0; k < 31; k++) {
		const int y = target.y - k;
		if (y < 0 || y >= out.h())
			continue;
		const int inset = k < 16 ? 30 - 2 * k : 2 * (k - 15);
		const int x0 = std::max(inset, -target.x);
		const int x1 = std::min(63 - inset, out.w() - 1 - target.x);
		uint32_t *row = out.at<uint32_t>(0, y) + target.x;
		const uint32_t *src = &pic.px[static_cast<size_t>(k) * 64];
		for (int x = x0; x <= x1; x++) {
			const uint32_t c = row[x];
			const int r = static_cast<int>((c >> 16) & 0xFF), g = static_cast<int>((c >> 8) & 0xFF), b = static_cast<int>(c & 0xFF);
			// Earth: dull and dark, its blue no stronger than its red and green (the town's rocks are blue-grey).
			const int hi = std::max({ r, g, b });
			const float dark = std::clamp((90.0F - static_cast<float>(hi)) / 40.0F, 0.0F, 1.0F);
			const float notBlue = std::clamp(static_cast<float>(std::max(r, g) + 6 - b) / 12.0F, 0.0F, 1.0F);
			const float w = dark * notBlue;
			if (w <= 0.02F)
				continue;
			const uint32_t p = src[x];
			const uint32_t lit = brightness >= 256 ? p & 0xFFFFFFU : Shade(p, brightness);
			const int a = static_cast<int>(w * 256.0F);
			const auto mix = [a](int under, int over) { return (under * (256 - a) + over * a) >> 8; };
			row[x] = (c & 0xFF000000U) | (static_cast<uint32_t>(mix(r, static_cast<int>((lit >> 16) & 0xFF))) << 16)
			    | (static_cast<uint32_t>(mix(g, static_cast<int>((lit >> 8) & 0xFF))) << 8) | static_cast<uint32_t>(mix(b, static_cast<int>(lit & 0xFF)));
		}
	}
}

void DrawWoodsMixedGround(const Surface &out, Point tile, Point target, int light, bool fullBright)
{
	if (leveltype != DTYPE_WOODS || !BuiltMixed || out.isIndexed() || Art().ground[Path].px.empty())
		return;
	const int brightness = (fullBright && light < LightsMax) || light < 0 ? 256 : BrightnessFor(light);
	const Picture &road = Art().ground[Path];
	const int originX = (tile.x - tile.y) * 32;
	const int originY = (tile.x + tile.y) * 16;
	const int centreX = target.x + 32;
	const int centreY = target.y - 15;
	// Only a tile within two of the road carries any of it; the rest is graded and left.
	const auto onRoad = [](int x, int y) {
		x = std::clamp(x, 0, MAXDUNX - 1);
		y = std::clamp(y, 0, MAXDUNY - 1);
		return Ground[x][y] == Path && Tree[x][y] == NoTree ? 1 : 0;
	};
	bool nearRoad = false;
	for (int dy = -2; dy <= 2 && !nearRoad; dy++)
		for (int dx = -2; dx <= 2 && !nearRoad; dx++) {
			const Point p = tile + Displacement { dx, dy };
			nearRoad = InDungeonBounds(p) && Ground[p.x][p.y] == Path && Tree[p.x][p.y] == NoTree;
		}
	if (InRiver(tile))
		nearRoad = false; // the bridge's planks and the water, not the painted road
	for (int k = 0; k < 31; k++) {
		const int y = target.y - k;
		if (y < 0 || y >= out.h())
			continue;
		const int inset = k < 16 ? 30 - 2 * k : 2 * (k - 15);
		const int x0 = std::max(target.x + inset, 0);
		const int x1 = std::min(target.x + 63 - inset, out.w() - 1);
		uint32_t *row = out.at<uint32_t>(0, y);
		const int wy = originY + (y - target.y) + (1 << 20);
		const int dy = y - centreY;
		for (int x = x0; x <= x1; x++) {
			const uint32_t c = row[x];
			// Tristram's grey-green grass toward the painted trees' olive (the user's frame, graded three ways: this one read
			// as the same woods): red x0.85, green x1.08, blue x0.62.
			uint32_t r = std::max<uint32_t>(((c >> 16) & 0xFF) * 218 >> 8, 1);
			uint32_t g = std::max<uint32_t>(std::min<uint32_t>(255, ((c >> 8) & 0xFF) * 276 >> 8), 1);
			uint32_t b = std::max<uint32_t>((c & 0xFF) * 159 >> 8, 1);
			if (nearRoad) {
				// How much road this pixel is (user, 2026-10-13: "a fading effect where road tiles spill into grass, not sharp
				// edges"): the road tiles' share blended across the tile centres, nudged by the world-pinned noise so the edge
				// frays, then eased over the middle half - about a tile of fade.
				const int wx = originX + (x - target.x) + (1 << 20);
				const auto &n = EdgeNoise[static_cast<size_t>((wy >> 3) & (NoiseSize - 1)) * NoiseSize + ((wx >> 3) & (NoiseSize - 1))];
				const int dx = x - centreX;
				const int u = tile.x * 64 + dx + 2 * dy + 64 * 4; // in 1/64 tile, tile centres on multiples of 64
				const int v = tile.y * 64 + 2 * dy - dx + 64 * 4;
				const int ui = u / 64 - 4;
				const int vi = v / 64 - 4;
				const int fu = u & 63;
				const int fv = v & 63;
				int share = (onRoad(ui, vi) * (64 - fu) + onRoad(ui + 1, vi) * fu) * (64 - fv)
				    + (onRoad(ui, vi + 1) * (64 - fu) + onRoad(ui + 1, vi + 1) * fu) * fv; // 0..4096
				share += (n[0] + n[1]) * 8;
				int a = std::clamp((share - 1024) * 256 / 2048, 0, 256);
				a = a * a * (768 - 2 * a) >> 16; // smoothstep, 0..256
				if (a > 0) {
					uint32_t t = road.px[static_cast<size_t>(wy % road.h) * road.w + (wx % road.w)];
					t = brightness >= 256 ? t : Shade(t, brightness);
					r = (((t >> 16) & 0xFF) * a + r * (256 - a)) >> 8;
					g = (((t >> 8) & 0xFF) * a + g * (256 - a)) >> 8;
					b = ((t & 0xFF) * a + b * (256 - a)) >> 8;
				}
			}
			row[x] = (std::max<uint32_t>(r, 1) << 16) | (std::max<uint32_t>(g, 1) << 8) | std::max<uint32_t>(b, 1);
		}
	}
}

namespace {

/**
 * @brief Where a tree in front of the hero must let him show (user's play test, 2026-10-13: the canopies hid the hero):
 * the hero's chest on screen and the circle round it the tree is drawn see-through in. Set per tree by
 * DrawWoodsTileSprite; radius 0 for none.
 */
struct SeeThrough {
	int x = 0;
	int y = 0;
	int radius = 0;
};

void DrawPicture(const Surface &out, const Picture &pic, int left, int top, int srcX, int srcY, int w, int h, int brightness, SeeThrough hole = {})
{
	if (brightness <= 0)
		return;
	const int r2 = hole.radius * hole.radius;
	const int y0 = std::max(0, -top);
	const int y1 = std::min(h, out.h() - top);
	const bool wholeRows = srcX == 0 && w == pic.w && !pic.spanFirst.empty();
	for (int y = y0; y < y1; y++) {
		const int sy = top + y;
		uint32_t *row = out.at<uint32_t>(0, sy);
		const uint32_t *src = &pic.px[static_cast<size_t>(srcY + y) * pic.w + srcX];
		// Clipped to the screen, and to the row's opaque span when the whole picture is drawn.
		int x0 = std::max(0, -left);
		int x1 = std::min(w, out.w() - left);
		if (wholeRows) {
			x0 = std::max<int>(x0, pic.spanFirst[srcY + y]);
			x1 = std::min<int>(x1, pic.spanLast[srcY + y]);
		}
		for (int x = x0; x < x1; x++) {
			const int sx = left + x;
			const uint32_t c = src[x];
			if ((c & 0xFF000000U) == 0)
				continue;
			const uint32_t lit = brightness >= 256 ? (c & 0xFFFFFF) | 0x010101 : Shade(c, brightness);
			if (r2 > 0) {
				const int ddx = sx - hole.x;
				const int ddy = (sy - hole.y) * 3 / 2; // a little taller than wide, as a figure is
				const int d2 = ddx * ddx + ddy * ddy;
				if (d2 < r2) {
					// A quarter of the tree at the centre, all of it at the circle's edge.
					const int keep = 64 + 192 * d2 / r2;
					const uint32_t bg = row[sx];
					const auto mix = [&](int shift) { return ((((lit >> shift) & 0xFF) * keep + ((bg >> shift) & 0xFF) * (256 - keep)) >> 8) & 0xFF; };
					row[sx] = (static_cast<uint32_t>(mix(16)) << 16) | (static_cast<uint32_t>(mix(8)) << 8) | static_cast<uint32_t>(mix(0));
					continue;
				}
			}
			row[sx] = lit;
		}
	}
}

/**
 * @brief A tree that sways (user, 2026-10-14: "animate a bit the trees like we did in tristram"): @p pic drawn as DrawPicture
 * draws it, each row shifted by @p wave times its share of the sway - nothing at the foot (@p footRow, the picture's own row),
 * full from 48 px up, eased in as the town's foliage is (scene_fx.cpp ApplyFoliageSway). A shift between whole pixels blends the
 * two source pixels it falls between, so the crown moves smoothly rather than a pixel at a time.
 */
void DrawPictureSwaying(const Surface &out, const Picture &pic, int left, int top, int brightness, SeeThrough hole, float wave, int footRow)
{
	if (brightness <= 0)
		return;
	const int r2 = hole.radius * hole.radius;
	const int y0 = std::max(0, -top);
	const int y1 = std::min(pic.h, out.h() - top);
	const int w = pic.w;
	for (int y = y0; y < y1; y++) {
		const int sy = top + y;
		uint32_t *row = out.at<uint32_t>(0, sy);
		const uint32_t *src = &pic.px[static_cast<size_t>(y) * w];
		const float up = std::min(1.0F, static_cast<float>(std::max(0, footRow - y)) / 48.0F);
		const float shift = wave * std::pow(up, 1.3F);
		const int whole = static_cast<int>(std::floor(shift));
		const int part = static_cast<int>((shift - static_cast<float>(whole)) * 256.0F); // the share of the pixel to the left
		const int first = pic.spanFirst.empty() ? 0 : pic.spanFirst[y];
		const int last = pic.spanLast.empty() ? w : pic.spanLast[y];
		const int x0 = std::max(first + whole, -left);
		const int x1 = std::min(last + whole + 1, out.w() - left);
		for (int x = x0; x < x1; x++) {
			const int i1 = x - whole;     // the pixel the shift's whole part lands here
			const int i0 = i1 - 1;        // and the one to its left, which the fraction brings in
			const uint32_t c1 = i1 >= 0 && i1 < w ? src[i1] : 0;
			const uint32_t c0 = i0 >= 0 && i0 < w ? src[i0] : 0;
			const bool o1 = (c1 & 0xFF000000U) != 0;
			const bool o0 = (c0 & 0xFF000000U) != 0;
			uint32_t c;
			if (o1 && o0) {
				const auto blend = [&](int sh) { return ((((c1 >> sh) & 0xFF) * (256 - part) + ((c0 >> sh) & 0xFF) * part) >> 8) & 0xFF; };
				c = 0xFF000000U | (static_cast<uint32_t>(blend(16)) << 16) | (static_cast<uint32_t>(blend(8)) << 8) | static_cast<uint32_t>(blend(0));
			} else if (part < 128 ? o1 : o0) {
				c = part < 128 ? c1 : c0; // an edge: the nearer of the two, or nothing
			} else {
				continue;
			}
			const int sx = left + x;
			const uint32_t lit = brightness >= 256 ? (c & 0xFFFFFF) | 0x010101 : Shade(c, brightness);
			if (r2 > 0) {
				const int ddx = sx - hole.x;
				const int ddy = (sy - hole.y) * 3 / 2;
				const int d2 = ddx * ddx + ddy * ddy;
				if (d2 < r2) {
					const int keep = 64 + 192 * d2 / r2;
					const uint32_t bg = row[sx];
					const auto mix = [&](int sh) { return ((((lit >> sh) & 0xFF) * keep + ((bg >> sh) & 0xFF) * (256 - keep)) >> 8) & 0xFF; };
					row[sx] = (static_cast<uint32_t>(mix(16)) << 16) | (static_cast<uint32_t>(mix(8)) << 8) | static_cast<uint32_t>(mix(0));
					continue;
				}
			}
			row[sx] = lit;
		}
	}
}

} // namespace

namespace {
/** @brief A mirrored tree past the map's edge is being drawn: it is never in front of the hero, so it opens no see-through. */
bool DrawingBeyondMap = false;

/**
 * @brief The mirror of a tile past the edge, moved a band (six tiles) along the edge where it lies by a road's end: the band's
 * tiles beside the box's road strip blended its dirt, and the mirror repeated it every six tiles out (audit round 2).
 */
Point BeyondMapMirror(Point tile)
{
	const Point m = WoodsMirrorBeyondMap(tile);
	if (!TileNearRoad(m))
		return m;
	for (const Displacement d : { Displacement { 0, Lo }, Displacement { 0, -Lo }, Displacement { Lo, 0 }, Displacement { -Lo, 0 },
	         Displacement { 0, 2 * Lo }, Displacement { 0, -2 * Lo }, Displacement { 2 * Lo, 0 }, Displacement { -2 * Lo, 0 } }) {
		const Point n = m + d;
		if (InDungeonBounds(n) && !Inside(n) && !TileNearRoad(n))
			return n;
	}
	return m;
}
} // namespace

bool DrawWoodsBeyondMapFloor(const Surface &out, Point tile, Point target, bool fullBright)
{
	if (leveltype != DTYPE_WOODS || InDungeonBounds(tile))
		return false;
	const Point m = BeyondMapMirror(tile);
	const int light = ExplorationMemoryLight(m, dLight[m.x][m.y]);
	if (WoodsArtActive(out) && !WoodsTileUsesCels(m)) {
		DrawGroundAt(out, m, tile, target, light, fullBright); // the texture pinned to where the tile is, not its mirror
		return true;
	}
	return DrawWoodsMixedFloorCached(out, m, target, light, fullBright);
}

bool WoodsBeyondMapUsesCels(const Surface &out)
{
	return leveltype == DTYPE_WOODS && !WoodsArtActive(out) && !(BuiltMixed && !out.isIndexed());
}

void DrawWoodsBeyondMapTree(const Surface &out, Point tile, Point target)
{
	// The painted trees only where the look draws them (the Painted Art, or the Mixed look's trees over Tristram's cels) and
	// on a true-colour surface: DrawPicture writes 32-bit pixels (audit round 1). The pure Tristram look draws the mirror's
	// own cels instead (WoodsBeyondMapUsesCels).
	if (leveltype != DTYPE_WOODS || InDungeonBounds(tile) || out.isIndexed() || !(WoodsArtActive(out) || BuiltMixed))
		return;
	const Point m = BeyondMapMirror(tile);
	DrawingBeyondMap = true;
	DrawWoodsTileSprite(out, m, target, ExplorationMemoryLight(m, dLight[m.x][m.y]));
	DrawingBeyondMap = false;
}

void DrawWoodsTileSprite(const Surface &out, Point tile, Point target, int light)
{
	if (!InDungeonBounds(tile))
		return;
	const int brightness = BrightnessFor(light);
	const int footX = target.x + 32;
	const int footY = target.y - 15;
	if (const uint8_t kind = Tree[tile.x][tile.y]; kind != NoTree) {
		if (TreeHidden[tile.x][tile.y])
			return; // deep in the forest, behind the trees in front
		const Picture &pic = Art().trees[kind];
		if (pic.px.empty())
			return;
		const int left = footX + TreeJitterX[tile.x][tile.y] - pic.footX;
		const int top = footY + TreeJitterY[tile.x][tile.y] - pic.footY;
		SeeThrough hole;
		// In front of the hero (drawn after his row) and over him on screen: the circle round his chest is see-through.
		if (MyPlayer != nullptr && MyPlayer->isOnActiveLevel() && !DrawingBeyondMap) {
			const Point hero = MyPlayer->position.tile;
			if (hero.x + hero.y < tile.x + tile.y) {
				const int heroX = footX + ((hero.x - tile.x) - (hero.y - tile.y)) * 32;
				const int heroY = footY + ((hero.x - tile.x) + (hero.y - tile.y)) * 16 - 36;
				if (heroX > left - 40 && heroX < left + pic.w + 40 && heroY > top - 40 && heroY < top + pic.h + 40)
					hole = { heroX, heroY, 70 };
			}
		}
		// The sway (Living Town, as Tristram's trees): the trees of the playable woods, each on its own phase from where it
		// stands; the forest beyond the fence stands still - it is the costly part of the picture and no one walks there. The
		// desert's rocks and bones never move; its palms, acacias and thorn do.
		if (*sgOptions.Oracool.livingTown && TreeFiles[kind].sway && Inside(tile) && !out.isIndexed()) {
			const float t = static_cast<float>(SDL_GetTicks() % 3600000U) / 1000.0F;
			const float phase = static_cast<float>((tile.x - tile.y) * 32) * 0.012F + static_cast<float>(TreeJitterX[tile.x][tile.y]) * 0.21F;
			// A taller tree sways a little further: 1.2 px at the crown of a small one, up to 2 px for the tallest.
			const float reach = std::min(2.0F, 1.2F + static_cast<float>(pic.h) / 300.0F);
			const float wave = reach * (std::sin(t * 1.1F + phase) + 0.3F * std::sin(t * 2.7F + phase * 2.0F)) / 1.3F;
			DrawPictureSwaying(out, pic, left, top, brightness, hole, wave, pic.footY);
		} else {
			DrawPicture(out, pic, left, top, 0, 0, pic.w, pic.h, brightness, hole);
		}
		return;
	}
	if (const uint8_t cell = Scatter[tile.x][tile.y]; cell != 0 && cell <= Art().scatter.size() && !Art().scatter[cell - 1].px.empty()) {
		const Picture &pic = Art().scatter[cell - 1];
		DrawPicture(out, pic, footX - pic.footX, footY - pic.footY, 0, 0, pic.w, pic.h, brightness);
	}
}

void DrawTownFrostMouth(const Surface &out, Point tile, Point target, int light)
{
	// Behind the gate's middle, out past the walk's edge, so the hero walking onto the gate stands in front of it.
	constexpr Point MouthTile { FrostGateX1 + 2, (FrostGateY0 + FrostGateY1) / 2 };
	// The hill's mouth is gone (RTM dev note, 2026-10-21: "the frozen stream entrance - use this asset in tristram to go down to frost
	// area"): the Frozen Stream's hole with its ladder lies in the gate's ground instead (DrawTownFrostHole).
	constexpr bool HillMouth = false;
	if (!HillMouth || leveltype != DTYPE_TOWN || out.isIndexed() || tile != MouthTile)
		return;
	// At half the painting's size: the hero, 150 px painted, at the town's own height.
	static Picture mouth;
	static bool tried = false;
	if (!tried) {
		tried = true;
		constexpr TreeFile File { "levels\\frost\\mouth_tristram.png", 224, 436, 50, false, OrclArea::Frost };
		LoadStanding(File, mouth);
	}
	if (mouth.px.empty())
		return;
	DrawPicture(out, mouth, target.x + 32 - mouth.footX, target.y - 15 - mouth.footY, 0, 0, mouth.w, mouth.h, BrightnessFor(light));
}

namespace {

/**
 * @brief One area object's two paintings (before, after) and where each stands. A waypoint has one painting, lit by a glow of
 * @p glow's colour once it is (its after state); every other object at half the paintings' size, as the frost mouth (the briefs
 * paint a hero 150 px tall), the waypoint at the width of the game's own pad (144 of its 192).
 */
struct AreaObjectFile {
	const char *before;
	const char *after;
	int footX;
	int footY;
	int percent = 50;
	uint32_t glow = 0;
};
constexpr AreaObjectFile FrostBarrel { "levels\\frost\\objects\\barrel_frozen.png", "levels\\frost\\objects\\barrel_frozen_broken.png", 48, 116 };
constexpr AreaObjectFile FrostChest { "levels\\frost\\objects\\chest_ice.png", "levels\\frost\\objects\\chest_ice_open.png", 64, 100 };
constexpr AreaObjectFile FrostShrine { "levels\\frost\\objects\\shrine_ice.png", "levels\\frost\\objects\\shrine_ice_spent.png", 64, 180 };
constexpr AreaObjectFile FrostTorch { "levels\\frost\\objects\\torch_stand.png", "levels\\frost\\objects\\torch_stand_out.png", 32, 164 };
constexpr AreaObjectFile FrostCorpse { "levels\\frost\\objects\\corpse_snow.png", "levels\\frost\\objects\\corpse_snow_searched.png", 80, 84 };
constexpr AreaObjectFile DesertSarcophagus { "levels\\desert\\objects\\sarcophagus.png", "levels\\desert\\objects\\sarcophagus_open.png", 80, 100 };
constexpr AreaObjectFile DesertShrine { "levels\\desert\\objects\\shrine_sun.png", "levels\\desert\\objects\\shrine_sun_spent.png", 64, 180 };
constexpr AreaObjectFile DesertBrazier { "levels\\desert\\objects\\brazier_tripod.png", "levels\\desert\\objects\\brazier_tripod_out.png", 40, 148 };
constexpr AreaObjectFile DesertCorpse { "levels\\desert\\objects\\corpse_bier.png", "levels\\desert\\objects\\corpse_bier_searched.png", 96, 100 };
// The Still River's captive (RfA-85): the robed figure in its ice, the ice shattered and the robe empty once opened.
constexpr AreaObjectFile FrostCaptive { "levels\\frost\\objects\\captive_ice.png", "levels\\frost\\objects\\captive_ice_broken.png", 80, 276 };
// The waypoints (RfA-79, RfA-84): a frost pad lit pale ice-blue, a sun pad lit gold.
constexpr AreaObjectFile FrostWaypoint { "levels\\frost\\waypoint_frost.png", "levels\\frost\\waypoint_frost.png", 96, 100, 75, 0x8CC8FFU };
constexpr AreaObjectFile DesertWaypoint { "levels\\desert\\objects\\waypoint_sun.png", "levels\\desert\\objects\\waypoint_sun.png", 96, 100, 75, 0xFFC860U };

/**
 * @brief The painting @p type wears in @p area: the Frost Caves have barrels (urns too), chests, shrines, candle stands and corpses
 * (a sarcophagus there is a corpse in the snow); the desert sarcophagi, shrines, candle stands and corpses - its barrels and chests
 * keep the game's own. The candle stand is always lit (the game's light on it), so it never shows its burnt-out picture.
 */
const AreaObjectFile *AreaObjectFor(OrclArea area, int type)
{
	if (type == CaptiveObjectType)
		return area == OrclArea::Frost ? &FrostCaptive : nullptr;
	switch (static_cast<_object_id>(type)) {
	case OBJ_BARREL:
	case OBJ_BARRELEX:
	case OBJ_URN:
	case OBJ_URNEX:
		return area == OrclArea::Frost ? &FrostBarrel : nullptr;
	case OBJ_CHEST1:
	case OBJ_CHEST2:
	case OBJ_CHEST3:
	case OBJ_TCHEST1: // a trapped one too, or the vanilla chest among the ice ones gives the trap away (audit)
	case OBJ_TCHEST2:
	case OBJ_TCHEST3:
		return area == OrclArea::Frost ? &FrostChest : nullptr;
	case OBJ_GOATSHRINE:
	case OBJ_CAULDRON:
	case OBJ_SHRINEL:
	case OBJ_SHRINER:
		return area == OrclArea::Frost ? &FrostShrine : area == OrclArea::Desert ? &DesertShrine : nullptr;
	case OBJ_CANDLE2:
		return area == OrclArea::Frost ? &FrostTorch : area == OrclArea::Desert ? &DesertBrazier : nullptr;
	case OBJ_DECAP:
		return area == OrclArea::Frost ? &FrostCorpse : area == OrclArea::Desert ? &DesertCorpse : nullptr;
	case OBJ_SARC:
		return area == OrclArea::Frost ? &FrostCorpse : area == OrclArea::Desert ? &DesertSarcophagus : nullptr;
	case OBJ_WAYPOINT:
		return area == OrclArea::Frost ? &FrostWaypoint : area == OrclArea::Desert ? &DesertWaypoint : nullptr;
	default:
		return nullptr;
	}
}

/** @brief The area whose objects the current level wears, if it has any (the Frost Caves and the desert, floors and caves). */
std::optional<OrclArea> ObjectArea()
{
	if (!setlevel)
		return std::nullopt;
	const OrclArea area = AreaOfLevel(setlvlnum);
	if (area == OrclArea::Woods)
		return std::nullopt;
	if (area == OrclArea::Desert && !IsDesertLevel(setlvlnum) && !IsDesertCaveLevel(setlvlnum))
		return std::nullopt;
	return area;
}

const char *AreaObjectPath(int type, bool used)
{
	const std::optional<OrclArea> area = ObjectArea();
	if (!area)
		return nullptr;
	const AreaObjectFile *file = AreaObjectFor(*area, type);
	if (file == nullptr)
		return nullptr;
	return used && type != OBJ_CANDLE2 ? file->after : file->before; // a waypoint's two are the one painting
}

} // namespace

bool DrawsAreaObject(int objectType)
{
	return AreaObjectPath(objectType, false) != nullptr;
}

bool IsStillRiverCaptive(int type, int var3)
{
	return setlevel && setlvlnum == SL_ORCL_FROST_RIVER_4 && type == OBJ_CHEST3 && var3 == CaptiveMark;
}

const char *AreaObjectPictureForTest(int objectType, bool used)
{
	return AreaObjectPath(objectType, used);
}

bool DrawAreaObject(const Surface &out, int objectType, bool used, Point foot, bool hovered, int light)
{
	if (out.isIndexed())
		return false;
	const std::optional<OrclArea> area = ObjectArea();
	if (!area)
		return false;
	const AreaObjectFile *file = AreaObjectFor(*area, objectType);
	if (file == nullptr)
		return false;
	const bool after = used && objectType != OBJ_CANDLE2;
	// Loaded once each, kept for the game (a handful of small pictures); a painting that fails to load leaves the vanilla sprite.
	static std::unordered_map<const char *, Picture> pictures; // the paths are constants: no string built a frame (audit)
	const char *path = after ? file->after : file->before;
	auto [it, added] = pictures.try_emplace(path);
	if (added) {
		const TreeFile asTree { path, file->footX, file->footY, file->percent, false, *area };
		LoadStanding(asTree, it->second);
	}
	const Picture &pic = it->second;
	if (pic.px.empty())
		return false;
	const int left = foot.x - pic.footX;
	const int top = foot.y - pic.footY;
	if (hovered) {
		// The game's own hover outline (palette 194, as ClxDrawOutlineSkipColorZero draws it): every clear pixel beside an opaque one.
		const uint32_t outline = (LitPaletteTable(0)[194] & 0xFFFFFF) | 0x010101;
		const auto opaque = [&](int x, int y) {
			return x >= 0 && y >= 0 && x < pic.w && y < pic.h && (pic.px[static_cast<size_t>(y) * pic.w + x] & 0xFF000000U) != 0;
		};
		for (int y = -1; y <= pic.h; y++) {
			const int sy = top + y;
			if (sy < 0 || sy >= out.h())
				continue;
			uint32_t *row = out.at<uint32_t>(0, sy);
			for (int x = -1; x <= pic.w; x++) {
				const int sx = left + x;
				if (sx < 0 || sx >= out.w() || opaque(x, y))
					continue;
				if (opaque(x - 1, y) || opaque(x + 1, y) || opaque(x, y - 1) || opaque(x, y + 1))
					row[sx] = outline;
			}
		}
	}
	DrawPicture(out, pic, left, top, 0, 0, pic.w, pic.h, BrightnessFor(light));
	if (after && file->glow != 0) {
		// A lit waypoint: a soft glow over the pad, brightest at its middle and gone by its rim, added to the painting (the game's
		// own pad glows in its lit frame; the paintings are unlit by brief).
		const float rx = pic.w * 0.42F, ry = rx * 0.5F;
		const int cy = foot.y - static_cast<int>(ry * 0.6F);
		const float gr = static_cast<float>((file->glow >> 16) & 0xFF), gg = static_cast<float>((file->glow >> 8) & 0xFF), gb = static_cast<float>(file->glow & 0xFF);
		const float strength = 0.55F * static_cast<float>(BrightnessFor(light)) / 256.0F;
		for (int y = static_cast<int>(-ry); y <= static_cast<int>(ry); y++) {
			const int sy = cy + y;
			if (sy < 0 || sy >= out.h())
				continue;
			uint32_t *row = out.at<uint32_t>(0, sy);
			for (int x = static_cast<int>(-rx); x <= static_cast<int>(rx); x++) {
				const int sx = foot.x + x;
				if (sx < 0 || sx >= out.w())
					continue;
				const float d = (x * x) / (rx * rx) + (y * y) / (ry * ry);
				if (d >= 1.0F)
					continue;
				const float k = strength * (1.0F - d) * (1.0F - d);
				const uint32_t c = row[sx];
				const auto add = [&](int shift, float g) { return static_cast<uint32_t>(std::min(255.0F, static_cast<float>((c >> shift) & 0xFF) + g * k)); };
				row[sx] = (add(16, gr) << 16) | (add(8, gg) << 8) | add(0, gb);
			}
		}
	}
	return true;
}

namespace {

/**
 * @brief Tristram's way down into the Frost Caves: the Frozen Stream's own entrance (RfA-79's hole with its rope ladder), laid into the
 * ground of the east edge's gate by its soft alpha, as the Frost Caves lay it into theirs - the part of it over @p tile's diamond.
 */
void DrawTownFrostHole(const Surface &out, Point tile, Point target)
{
	static Picture hole;
	static bool tried = false;
	if (!tried) {
		tried = true;
		LoadDecal(FrostEntranceFiles[0], hole, EntrancePercent, { 0.0, 0.0, 0.0 }); // no ground colour to match: its own snow
	}
	if (hole.px.empty())
		return;
	// On the gate's west column, so its fade stays on the open edge tiles (the column past the gate is rock, never floored).
	const float cx = static_cast<float>(FrostGateX0), cy = (FrostGateY0 + FrostGateY1) * 0.5F;
	if (std::abs(static_cast<float>(tile.x) - cx) > 3.0F || std::abs(static_cast<float>(tile.y) - cy) > 3.0F)
		return;
	for (int k = 0; k < 31; k++) {
		const int y = target.y - k;
		if (y < 0 || y >= out.h())
			continue;
		const int inset = k < 16 ? 30 - 2 * k : 2 * (k - 15);
		const int x0 = std::max(target.x + inset, 0);
		const int x1 = std::min(target.x + 63 - inset, out.w() - 1);
		uint32_t *row = out.at<uint32_t>(0, y);
		const int dy = 15 - k;
		for (int x = x0; x <= x1; x++) {
			const int dx = x - (target.x + 32);
			const float du = static_cast<float>(tile.x) + static_cast<float>(dx + 2 * dy) / 64.0F - cx;
			const float dv = static_cast<float>(tile.y) + static_cast<float>(2 * dy - dx) / 64.0F - cy;
			const int px = hole.footX + static_cast<int>(std::lround((du - dv) * 32.0F));
			const int py = hole.footY + static_cast<int>(std::lround((du + dv) * 16.0F));
			if (px < 0 || py < 0 || px >= hole.w || py >= hole.h)
				continue;
			const uint32_t d = hole.px[static_cast<size_t>(py) * hole.w + px];
			const uint32_t a = d >> 24;
			if (a == 0)
				continue;
			const uint32_t c = row[x];
			const auto mix = [a](uint32_t p, uint32_t q, int sh) { return ((((p >> sh) & 0xFF) * (255 - a) + ((q >> sh) & 0xFF) * a) / 255) & 0xFF; };
			row[x] = (std::max<uint32_t>(mix(c, d, 16), 1) << 16) | (std::max<uint32_t>(mix(c, d, 8), 1) << 8) | std::max<uint32_t>(mix(c, d, 0), 1);
		}
	}
}


/**
 * @brief The sand of the walled strip behind the gap in Tristram's north wall (x 35-41, y 0-9) and a tongue of it through the gap, over
 * @p tile's diamond: the desert's own sand_a, pinned to the world, darkened as the grass under it is, its edge soft.
 */
void DrawTownDesertSand(const Surface &out, Point tile, Point target)
{
	constexpr float X0 = 34.5F, X1 = 41.5F, Y0 = -1.0F, Y1 = 9.5F; // the strip, wall to wall
	if (tile.x < 33 || tile.x > 43 || tile.y > 12)
		return;
	static Picture sand;
	static bool tried = false;
	if (!tried) {
		tried = true;
		LoadScaled("levels\\desert\\sand_a.png", sand, false, 14); // the desert floors' own scale (GroundFiles)
	}
	if (sand.px.empty())
		return;
	for (int k = 0; k < 31; k++) {
		const int y = target.y - k;
		if (y < 0 || y >= out.h())
			continue;
		const int inset = k < 16 ? 30 - 2 * k : 2 * (k - 15);
		const int x0 = std::max(target.x + inset, 0);
		const int x1 = std::min(target.x + 63 - inset, out.w() - 1);
		uint32_t *row = out.at<uint32_t>(0, y);
		const int dy = 15 - k;
		for (int x = x0; x <= x1; x++) {
			const int dx = x - (target.x + 32);
			const float u = static_cast<float>(tile.x) + static_cast<float>(dx + 2 * dy) / 64.0F;
			const float v = static_cast<float>(tile.y) + static_cast<float>(2 * dy - dx) / 64.0F;
			// Inside the strip: full; the tongue through the doorway, fading two tiles into the town.
			float cover = 0.0F;
			if (u > X0 && u < X1 && v > Y0 && v < Y1)
				cover = std::clamp(std::min({ u - X0, X1 - u, Y1 - v + 0.9F }) / 0.6F, 0.0F, 1.0F);
			const float doorU = (DoorwayX0 + DoorwayX1) * 0.5F;
			if (std::abs(u - doorU) < 1.6F && v >= Y1 - 0.5F && v < 12.5F) {
				const float side = std::clamp((1.6F - std::abs(u - doorU)) / 0.6F, 0.0F, 1.0F);
				const float along = std::clamp((12.5F - v) / 2.0F, 0.0F, 1.0F);
				cover = std::max(cover, side * along);
			}
			if (cover <= 0.0F)
				continue;
			// The world pixel, so the sand does not swim with the camera.
			const int wx = (tile.x - tile.y) * 32 + dx;
			const int wy = (tile.x + tile.y) * 16 + dy;
			const int sx = ((wx % sand.w) + sand.w) % sand.w;
			const int sy = ((wy % sand.h) + sand.h) % sand.h;
			const uint32_t s = sand.px[static_cast<size_t>(sy) * sand.w + sx];
			const uint32_t c = row[x];
			const float r = static_cast<float>((c >> 16) & 0xFF), g = static_cast<float>((c >> 8) & 0xFF), b = static_cast<float>(c & 0xFF);
			// As dark as the grass it covers: the town's day and night, the hero's light.
			const float lit = std::clamp((0.299F * r + 0.587F * g + 0.114F * b) / 70.0F, 0.25F, 1.5F);
			const auto ch = [&](int sh, float under) {
				const float top = static_cast<float>((s >> sh) & 0xFF) * lit;
				return static_cast<uint32_t>(std::clamp(under + (top - under) * cover, 1.0F, 255.0F));
			};
			row[x] = (ch(16, r) << 16) | (ch(8, g) << 8) | ch(0, b);
		}
	}
}

} // namespace

void DrawWoodsGateTint(const Surface &out, Point tile, Point target)
{
	if (out.isIndexed())
		return;
	if (leveltype == DTYPE_TOWN) {
		DrawTownDesertSand(out, tile, target); // the strip behind the desert's doorway
		DrawTownFrostHole(out, tile, target); // under the gate's glow
	}
	// The marks: a soft round glow over each way out, centred on its strip, fading to nothing by its radius (the seam survey,
	// 2026-10-14: the tinted strips were flat blocks of tiles with straight edges). Each tile nearest one takes its share.
	ExitMark marks[3];
	const int markCount = ExitMarksHere(marks);
	int nearest = -1;
	for (int m = 0; m < markCount; m++) {
		const float ddx = static_cast<float>(tile.x) - marks[m].x, ddy = static_cast<float>(tile.y) - marks[m].y;
		if (std::sqrt(ddx * ddx + ddy * ddy) < marks[m].radius + 1.0F)
			nearest = m;
	}
	if (nearest < 0)
		return;
	const ExitMark mark = marks[nearest];
	const bool open = mark.open;
	const bool hovered = nearest == HoveredExit;
	const bool frostHole = leveltype == DTYPE_TOWN && marks[nearest].to == SL_ORCL_FROST_1;
	// Green for a road you can take, amber for one not open yet; a slow pulse, a little stronger toward the tile's centre.
	const int tr = open ? 120 : 236;
	const int tg = open ? 255 : 170;
	const int tb = open ? 110 : 60;
	const float pulse = 0.5F + 0.5F * std::sin(static_cast<float>(SDL_GetTicks() % 100000U) * 0.004F);
	// Lit up while the cursor is on it (RTM dev note, 2026-10-20): steady and twice as strong.
	const int base = hovered ? 210 : static_cast<int>((open ? 70.0F : 55.0F) + 50.0F * pulse); // of 256
	for (int k = 0; k < 31; k++) {
		const int y = target.y - k;
		if (y < 0 || y >= out.h())
			continue;
		const int inset = k < 16 ? 30 - 2 * k : 2 * (k - 15);
		const int x0 = std::max(target.x + inset, 0);
		const int x1 = std::min(target.x + 63 - inset, out.w() - 1);
		uint32_t *row = out.at<uint32_t>(0, y);
		const int dy = 15 - k;
		for (int x = x0; x <= x1; x++) {
			const int dx = x - (target.x + 32);
			const float u = static_cast<float>(tile.x) + static_cast<float>(dx + 2 * dy) / 64.0F - mark.x;
			const float v = static_cast<float>(tile.y) + static_cast<float>(2 * dy - dx) / 64.0F - mark.y;
			const float fall = 1.0F - std::sqrt(u * u + v * v) / mark.radius;
			if (fall <= 0.0F)
				continue;
			int a = static_cast<int>(static_cast<float>(base) * fall * fall * (3.0F - 2.0F * fall));
			const uint32_t c = row[x];
			const int r = static_cast<int>((c >> 16) & 0xFF);
			const int g = static_cast<int>((c >> 8) & 0xFF);
			const int b = static_cast<int>(c & 0xFF);
			// Tristram's frost hole (2026-10-21): the glow on its snowy rim, its dark pit left dark.
			if (frostHole)
				a = a * std::min(255, (r + g + b) / 2) / 255;
			// Mixed toward the tint and brightened, so it reads on dark grass and on bright paving alike.
			const int nr = std::min(255, r + ((tr - r) * a >> 8) + (a >> 3));
			const int ng = std::min(255, g + ((tg - g) * a >> 8) + (a >> 3));
			const int nb = std::min(255, b + ((tb - b) * a >> 8) + (a >> 3));
			row[x] = static_cast<uint32_t>((std::max(nr, 1) << 16) | (std::max(ng, 1) << 8) | std::max(nb, 1));
		}
	}
}

uint8_t WoodsGroundAt(Point tile)
{
	return InDungeonBounds(tile) ? Ground[tile.x][tile.y] : Forest;
}

bool WoodsTreeAt(Point tile)
{
	return !InDungeonBounds(tile) || Tree[tile.x][tile.y] != NoTree;
}

Point WoodsArrival()
{
	return Arrival;
}

Point WoodsWayOn()
{
	return WayOn;
}

uint8_t WoodsAutomapCell(Point cell)
{
	if (cell.x < 0 || cell.y < 0 || cell.x >= WoodsCells || cell.y >= WoodsCells)
		return 0;
	return MapCell[cell.x][cell.y];
}

Point WoodsMirrorBeyondMap(Point tile)
{
	// Into the forest band between the map's edge and the box (tiles 0..Lo-1, and the last Lo), shifted by whole bands - Lo is
	// six, a multiple of both TreeHidden lattices (2 x 2 and 3 x 3), so the hidden and drawn trees keep their rhythm and no
	// tile repeats its neighbour (a reflection twinned the edge row, audit round 1).
	const auto fold = [](int v, int size) {
		if (v < 0)
			return ((v % Lo) + Lo) % Lo;
		if (v >= size)
			return size - Lo + (v - size) % Lo;
		return v;
	};
	return { fold(tile.x, MAXDUNX), fold(tile.y, MAXDUNY) };
}

bool WoodsStampForTest(int which, std::vector<uint16_t> &pieces, int &w, int &h)
{
	pieces.clear();
	if (which == 0) {
		w = 8;
		h = 6;
		for (const auto &row : CottageShell)
			pieces.insert(pieces.end(), std::begin(row), std::end(row));
		return true;
	}
	if (which == 1) {
		w = MouthWidth;
		h = MouthHeight;
		for (const auto &row : MouthStamp)
			pieces.insert(pieces.end(), std::begin(row), std::end(row));
		return true;
	}
	if (which >= 2 && which <= 7) {
		w = 2;
		h = 2;
		pieces.assign(std::begin(RiverPieces[which - 1]), std::end(RiverPieces[which - 1]));
		return true;
	}
	return false;
}

void SetWoodsRiverForTest(int mode)
{
	RiverForced = mode;
}

bool WoodsRiverAtForTest(Point tile)
{
	return InRiver(tile);
}

void SetWoodsCaveMouthFloorForTest(int floorIndex)
{
	MouthFloor = floorIndex;
}

Point StampWoodsCaveMouthForTest()
{
	MouthFloor = -1;
	StampRiver();
	StampCaveMouth();
	LayFences();
	LayHouses();
	return MouthOnFloor ? MouthEntrance : Point { 0, 0 };
}

void SetWoodsRungForTest(int floorIndex, int rung, OrclArea area)
{
	Game.rung[AreaIndex(area)][std::clamp(floorIndex, 0, WoodsFloors - 1)] = static_cast<uint8_t>(rung);
}

WoodsWorldPlanForTest PlanWoodsWorldForTest(uint32_t seed, int floor)
{
	PlanWoodsWorld(seed, floor);
	WoodsWorldPlanForTest plan;
	for (const WorldPiece &piece : WorldPieces)
		plan.objects.emplace_back(static_cast<int>(piece.type), piece.at);
	plan.wisps = WispCount;
	plan.fairyRing = HasFairyRing;
	plan.fairyRingAt = GladeCentre;
	return plan;
}

void SetWoodsSeedForTest(int floorIndex, uint32_t seed, OrclArea area)
{
	Game.seed[AreaIndex(area)][std::clamp(floorIndex, 0, WoodsFloors - 1)] = seed;
}

Point DesertOasisForTest()
{
	return OasisRadius > 0.0F ? OasisCentre : Point { 0, 0 };
}

Point DesertArenaForTest()
{
	return ArenaCentre;
}

int DesertCampsForTest(Point *first)
{
	if (first != nullptr)
		*first = CampLanterns.empty() ? Point { 0, 0 } : CampLanterns.front();
	return static_cast<int>(CampLanterns.size());
}

Point DesertCraterForTest()
{
	return Craters.empty() ? Point { 0, 0 } : Craters.front();
}

Point DesertRuinsForTest(int *braziers)
{
	if (braziers != nullptr)
		*braziers = static_cast<int>(std::count_if(Braziers.begin(), Braziers.end(), [](Point p) { return Tree[p.x][p.y] == Brazier; }));
	return RuinsCentre;
}

std::vector<int> AreaRosterForTest(OrclArea area)
{
	const auto [first, count] = AreaKinds(area);
	std::vector<int> kinds;
	for (size_t i = 0; i < count; i++)
		kinds.push_back(first[i]);
	return kinds;
}

void LoadWoodsArtForTest()
{
	Art().tried = false;
	LoadArt();
	BuildEdgeNoise();
}

} // namespace devilution::oracool
