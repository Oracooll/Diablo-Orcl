/**
 * @file oracool/woods.h
 *
 * The Orcl Act's woods (user, 2026-10-13: "introduce 2-4 areas with 4 levels each ... open world grass and woods area as in
 * early Act 1 in Diablo 2 ... Going west of Pepin takes us to the four grass/woods levels"). The plan and its decisions are
 * the "Road to the Orcl Act" page.
 *
 * Four floors, each a SET LEVEL generated from a seed the way the rifts are (decision D1): SL_ORCL_WOODS_1..4, level type
 * DTYPE_WOODS. A woods floor is a dungeon in every rule - monsters, light, exploration - on the town tileset's data (its
 * pieces give the walking and solidity), but nothing of the tileset is drawn: the ground is the woods' true-colour
 * textures (ChatGPT, RfA-62/62b/62c), laid SCREEN-ALIGNED as painted and pinned to the world, and the trees are
 * true-colour sprites (RfA-63) sorted with the figures. A new layout every new game, the same one for the rest of that
 * game (D4).
 *
 * Milestone 1 (this file's first version): floor 1, The Greywood Verge - the road out of Tristram west of Pepin, the
 * generated woods, the way back. No monsters yet; the way on to floor 2 is not open yet.
 *
 * The desert (user, 2026-10-16: "design the 100x100 levels of Desert Area. Implement whatever assets we now have into them"): a
 * second AREA on the same generator - four more floors, SL_ORCL_DESERT_1..4, also DTYPE_WOODS (the act's outdoor level type, so
 * the sky, the day, the light and the play area are the woods'), out of Tristram east of the cows. Its own ground (RfA-65, 68),
 * its rocks, palms and bones where the woods have trees (RfA-66, 67), an oasis, buried ruins, a sand hole down to its caves, its
 * own Mob Roster and weather; no fences, cottages or river. A floor's "area" is the woods' (0) or the desert's (1).
 */
#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

#include "engine/point.hpp"
#include "levels/gendung.h"

namespace devilution {
struct Surface;
struct Player;
struct MonsterData;
struct Monster;
} // namespace devilution

namespace devilution::oracool {

/** @brief The Orcl Act's areas under the sky: each has four floors, numbered 0-3 within it. */
enum class OrclArea : uint8_t {
	Woods,
	Desert,
	Frost, // the Frost Caves (2026-10-17): underground, on the same generator
};

/** @brief The area @p level belongs to: a floor's or a cave's (the woods for anything else). */
inline OrclArea AreaOfLevel(_setlevels level)
{
	if (IsFrostLevel(level) || IsFrostRiverLevel(level))
		return OrclArea::Frost;
	return IsDesertLevel(level) || IsDesertCaveLevel(level) ? OrclArea::Desert : OrclArea::Woods;
}

/** @brief Floor @p floor (0-3) of @p area as a set level. */
inline _setlevels OutdoorFloorLevel(OrclArea area, int floor)
{
	return static_cast<_setlevels>((area == OrclArea::Frost ? SL_FIRST_FROST : area == OrclArea::Desert ? SL_FIRST_DESERT : SL_FIRST_WOODS) + floor);
}

/** @brief The hero stands on a desert floor (not its caves): its weather, its dusk, its sky. */
inline bool OnDesertFloor()
{
	return leveltype == DTYPE_WOODS && setlevel && IsDesertLevel(setlvlnum);
}

/**
 * @brief The piece a woods tile is DRAWN with, where that differs from the one that decides its walking (dPiece): on the
 * Mixed look a painted tree's tile is solid, yet shows Tristram's grass under the tree. 0 where they are the same.
 */
extern DVL_API_FOR_TEST uint16_t WoodsLookPiece[MAXDUNX][MAXDUNY];

/**
 * @brief The houses' outsides (woods.cpp CottageShell): a Tristram cottage's town piece over each of a house's tiles, 0 for none;
 * the house it belongs to; and each house's state this frame - 0 gone (the hero is inside), 1 see-through (he is behind it),
 * 2 whole. UpdateWoodsShells sets the states, once a frame.
 */
extern DVL_API_FOR_TEST uint16_t WoodsShell[MAXDUNX][MAXDUNY];
extern DVL_API_FOR_TEST int8_t WoodsShellOwner[MAXDUNX][MAXDUNY];
extern DVL_API_FOR_TEST uint8_t WoodsShellState[2];

/** @brief The cottage's piece drawn on @p tile this frame, or 0. */
inline uint16_t WoodsShellAt(Point tile)
{
	if (leveltype != DTYPE_WOODS)
		return 0;
	const uint16_t shell = WoodsShell[tile.x][tile.y];
	const int owner = WoodsShellOwner[tile.x][tile.y];
	return shell != 0 && owner >= 0 && owner < 2 && WoodsShellState[owner] != 0 ? shell : 0;
}

/** @brief The cottage on @p tile is drawn see-through this frame. */
inline bool WoodsShellSeeThrough(Point tile)
{
	const int owner = WoodsShellOwner[tile.x][tile.y];
	return WoodsShellAt(tile) != 0 && WoodsShellState[owner] == 1;
}

/** @brief The piece the renderer draws for @p tile: a cottage's, else the woods' look piece where it has one, else dPiece. */
inline uint16_t WoodsDrawnPiece(Point tile)
{
	if (leveltype == DTYPE_WOODS) {
		if (const uint16_t shell = WoodsShellAt(tile); shell != 0)
			return shell;
		if (WoodsLookPiece[tile.x][tile.y] != 0)
			return WoodsLookPiece[tile.x][tile.y];
	}
	return dPiece[tile.x][tile.y];
}

/** @brief The cottages' states for this frame, from where the hero stands. Once a frame, before the floor is drawn. */
void UpdateWoodsShells();

/** @brief A whole cottage stands over @p tile, inside its house: what stands there is hidden under its roof. */
bool WoodsShellCovers(Point tile);

/** @brief The Mixed look is on for the floor last built: painted trees and ground details over Tristram's tiles. */
bool WoodsSpritesOverTiles(const Surface &out);

/** @brief Forgets the woods' layouts and their monsters' levels: a new game draws new woods (decisions D3, D4). */
void ResetWoodsForNewGame();
/**
 * @brief The Hive's or the Crypt's monsters brought to the floor's rung, fixed for this game from the hero's level
 * (HellfireFloorRung): each kind up or down by the gap between the rung and its own floors, as a woods floor's are. Fresh
 * generation only, after the theme rooms; a no-op off floors 17-24.
 */
void ScaleHellfireFloorMonsters();

/** @brief The woods generator, CreateDungeon's DTYPE_WOODS arm: the floor setlvlnum names, from @p seed. */
void CreateWoods(uint32_t seed, lvl_entry entry);

/** @brief LoadSetMap's arm for a woods floor: the floor from its seed, its tile data, its palette and its art. */
void BuildWoodsLevel(bool fresh);

/** @brief Where the hero stands in Tristram coming back out of the area setlvlnum names: on its road, clear of its gate. */
Point WoodsTownReturnTile();

/**
 * @brief The town gate and the woods' edges, CheckTriggers' Orcl arm: in Tristram, the road out west of Pepin loads woods
 * floor 1; on a woods floor, its arrival road leads back. True when a level change was started.
 */
bool CheckWoodsEdges();

/** @brief After a woods floor's items are made: on its first visit, what its houses hold. */
void FinishWoodsLevel(bool fresh);

/** @brief Test hooks: the floor last generated's houses, a house's arch and a tile inside it. */
int WoodsHouseCountForTest();
Point WoodsHouseDoorForTest(int index);
Point WoodsHouseInsideForTest(int index);

/** @brief Floor @p floorIndex of @p area's rung, 0 until it is first entered in this game. */
int WoodsFloorRung(int floorIndex, OrclArea area = OrclArea::Woods);

/** @brief The next woods floor built is entered from its cave: the hero stands by the cave's mouth. */
void EnterWoodsAtCaveMouthNext();

/** @brief The floor last built is a desert floor (its art, its generator; what the drawing calls draw). */
bool DesertBuilt();
/** @brief The floor last built is the Frost Caves' (2026-10-17). */
bool FrostBuilt();
/** @brief Where the Frost Caves floor last built's crystal lights go, and their radius: open tiles beside the crystals (BuildWoodsLevel). */
std::vector<std::pair<Point, int>> FrostCrystalLightSpots();
/** @brief The Frost Caves floor last built's light shafts (RfA-79b), for the tests. */
std::vector<Point> FrostLightShaftsForTest();
/**
 * @brief The Frost Caves' monster looks (the Orcl Act Level Designer's Frost Caves roster, 2026-10-16): each kind there recoloured,
 * never redrawn. 0 as it is; Rime the game's own frozen look; Glacier every colour ice-blue at its own lightness; Hoarfrost bleached
 * toward white; Frostbite its colours muted, its shadows deep blue and its highlights frost; Black ice dark glassy blue-black.
 */
enum class FrostLook : uint8_t {
	None,
	Rime,
	Glacier,
	Hoarfrost,
	Frostbite,
	BlackIce,
};
/**
 * @brief A monster's ranged attack @p missile as the Frost Caves have it (the Designer's Frost Caves roster, "we will make them shoot ice
 * shards, or diablo can produce ice spikes instead of appocalypse flames. acid spiters can spit ice shards"): on a Frost Caves floor
 * or frozen river, its ice version - firebolts and flares an ice bolt (arrows stay arrows); acid, blood stars, fireballs and magma ice blast shards;
 * Apocalypse and lightning glacial spikes. @p missile itself anywhere else, and for anything with no ice version.
 */
int FrostMissileFor(int missile);
/** @brief The look @p kind wears on a Frost Caves floor or frozen river; None anywhere else. */
FrostLook FrostLookFor(int kind);
/** @brief @p lit (256 lit palette colours) in @p look, into @p out; returns out. */
const uint32_t *FrostLookTable(FrostLook look, const uint32_t *lit, std::array<uint32_t, 256> &out);
/** @brief Test hook: the Frost Caves floor last built's lights (its crystals). */
int FrostLightCountForTest();
/** @brief Test hook: what stands on @p tile, as the generator's kind number (0 none). */
int WoodsKindAtForTest(Point tile);

/** @brief The cave mouth's tiles are Tristram's own cels, drawn by DrawCell even on the painted look. */
bool WoodsTileUsesCels(Point tile);

/**
 * @brief The Painted Art look: the Tristram grass of a river or cave-mouth piece just drawn on @p tile repainted with the woods'
 * own ground, by how green each pixel is - its water, rock and earth kept - so its edge meets the painted grass softly.
 */
void RepaintWoodsCelGrass(const Surface &out, Point tile, Point target, int light, bool fullBright);

/**
 * @brief And the dark earth of a cave-mouth rock's piece just drawn (its floor half, under the rock): repainted with the woods'
 * ground where it is a dull dark grey-brown, the rock's own blue kept - so the rocks stand in the grass, not on a slab.
 */
void RepaintWoodsCelGround(const Surface &out, Point tile, Point target, int light);

/**
 * @brief The hover over the cave mouth at @p tile: its entrance and the text ("Down to The Mossbound Den").
 * @return false off the mouth
 */
bool WoodsCaveMouthHover(Point tile, Point &entrance, const char *&text);

/**
 * @brief The rung (1-16 inside the difficulty's block) the current woods floor's monsters and loot stand on: the hero's level
 * by the Nephalem Rift's rule (one rung per three levels, the user's pick of 2026-09-24), one rung deeper per woods floor, and
 * fixed the first time the floor is entered in a game (D3). 1 off the woods.
 */
int WoodsBandFloor();

/**
 * @brief A monster type the current woods floor may draw: one of the user's Mob Roster (the Woods Levels Designer's tab 2,
 * 2026-10-14), of its rung - or, where fewer than four of them live that deep, the four nearest it (ScaleWoodsMonster makes
 * up the difference). Never a boss or a summon.
 */
bool WoodsAcceptsMonster(const MonsterData &data);

/**
 * @brief The kind the woods' last floor's boss is made of this game: one of the user's four (the Skeleton King, the Butcher,
 * the Reaper, Diablo - Mob Roster notes "use as boss"), picked from the floor's seed, so it is rolled anew each new game and
 * holds still for the rest of it. -1 before the floor has a seed.
 */
int WoodsBossKind();

/** @brief A monster kind's look, for the roster's mix: kinds of one sprite family (the Fallen, the skeletons, the goatmen) share it. */
int WoodsFamilyOf(int kind);

/** @brief @p monster is the woods' last floor's boss (its kind, on that floor). */
bool IsWoodsBoss(const Monster &monster);

/** @brief The size the woods boss is drawn at, so each stands as tall as Diablo (the user's notes); 0 when @p monster is not him. */
unsigned WoodsBossSizePercent(const Monster &monster);

/** @brief Where the woods boss waits: open ground inside the fence, a few steps in from the way on; none if the floor has none. */
std::optional<Point> WoodsBossSpot();

/** @brief The damage percent ScaleWoodsMonster gave the woods boss (100 for any other): his Apocalypse takes it too. */
int WoodsBossDamagePercent(const Monster &monster);

/** @brief An area's last floor, whose boss's own kinds (the King's skeletons) are loaded but never scattered. */
inline bool IsWoodsBossFloor()
{
	return setlevel && IsActBossLevel(setlvlnum);
}

/**
 * @brief A woods or cave monster brought up to the floor's rung: a kind whose own floors end above it gains 12% life and
 * damage per floor between (the Hell-depth rungs, where none of the user's kinds lives); the boss is set to the rung from his
 * own floors either way - up or down - then given three times the life and a quarter more damage. Does nothing elsewhere.
 */
void ScaleWoodsMonster(Monster &monster);

/** @brief Some champion (a non-speaking unique row) is made of monster kind @p type: a Dread boss can wear it. */
bool IsChampionKind(int type);

/** @brief The seed of the floor's monster roster: the same on every build, so a revisit's saved monsters keep their types. */
uint32_t WoodsRosterSeed();

/** @brief Where the hero stands on a woods floor he has just entered: the arrival road, or the way on coming back up. */
Point WoodsEntryTile();

/** @brief The woods floor's own name, for the floor banner and the map. */
const char *WoodsFloorName(_setlevels level);

/** @brief Whether the Woods Painted Art option asks for the painted woods; off, a woods floor is built from Tristram's tiles. */
bool WoodsPaintedArtChosen();

/** @brief True while the woods' true-colour ground and trees replace the tileset's drawing on @p out. */
bool WoodsArtActive(const Surface &out);

/**
 * @brief One woods tile's ground: the tile's diamond filled from the woods' textures, sampled where the tile lies in the
 * world, its material blended into its neighbours' along a noisy edge, lit by @p light (0 bright .. 15 black; -1 full brightness), or written
 * at full brightness when @p fullBright (the smooth floor pass darkens it afterwards).
 */
void DrawWoodsGroundTile(const Surface &out, Point tile, Point target, int light, bool fullBright);

/**
 * @brief The roads' markers (user, 2026-10-13: "colour somehow the ground which acts as an entrance ... brighter green
 * tint ... make it clear to me where i need to take my hero"): over the floor of a gate tile, a pulsing bright-green glow -
 * Tristram's road out to the woods, and the woods' road back - or amber on a way on that is not open yet. Nothing on any
 * other tile.
 */
void DrawWoodsGateTint(const Surface &out, Point tile, Point target);
/**
 * @brief Tristram's way down into the Frost Caves, painted (RfA-79's mouth_tristram): a cave mouth in a turf hill behind the east
 * edge's gate, drawn with the town's cells when the tile it stands on is drawn. Nothing on an indexed surface or off that tile.
 */
void DrawTownFrostMouth(const Surface &out, Point tile, Point target, int light);
/**
 * @brief The Frost Caves' and the desert's own objects (RfA-83, RfA-84, 2026-10-17): on their floors and caves, a barrel, chest,
 * shrine, candle stand, corpse or sarcophagus of type @p objectType is drawn as the area's painting - its after picture once @p used
 * (broken, opened, spent, searched) - standing on @p foot (the tile's middle on screen), outlined when @p hovered, at @p light.
 * False (nothing drawn) for a type the area has no painting of, anywhere else, or on an indexed target: the vanilla sprite then.
 */
bool DrawAreaObject(const Surface &out, int objectType, bool used, Point foot, bool hovered, int light); // a waypoint's used = lit
/**
 * @brief The Still River's captive (RfA-85, user 2026-10-17: "the Still River captive carries a treasure"): a large chest, marked by
 * CaptiveMark in its _oVar3, no quest. Drawn as the frozen robed figure (DrawAreaObject's CaptiveObjectType), shattered once opened.
 */
constexpr int CaptiveMark = 0x0CA97;
constexpr int CaptiveObjectType = -2;
/** @brief An object of @p type whose _oVar3 is @p var3 is the Still River's captive, here. */
bool IsStillRiverCaptive(int type, int var3);
/** @brief DrawAreaObject has a painting for @p objectType here (so the sprite is not drawn). */
bool DrawsAreaObject(int objectType);
/** @brief The painting DrawAreaObject would draw for @p objectType here (its path), or nullptr; for the tests. */
const char *AreaObjectPictureForTest(int objectType, bool used);

/**
 * @brief The Mixed look's ground, over the tile Tristram's cels just drew: the painted dirt road on and along the road, its
 * edge frayed by the same world-pinned noise as the painted look, and the rest of Tristram's grass graded toward the painted
 * trees' olive. Nothing on another look.
 */
void DrawWoodsMixedGround(const Surface &out, Point tile, Point target, int light, bool fullBright);

/**
 * @brief The Mixed look's floor tile from its cache - Tristram's cels, the grade and the road made once, copied with the light
 * each frame. False where the tile is drawn as it is (the river, the cave mouth, a cottage, a borrowed piece, another look).
 */
bool DrawWoodsMixedFloorCached(const Surface &out, Point tile, Point target, int light, bool fullBright);

/** @brief The Mixed floor's cached pictures dropped (a new floor). */
void ForgetWoodsMixedFloor();

/** @brief What stands on a woods tile, drawn in the figures' order: a tree, or a small thing on the ground. */
void DrawWoodsTileSprite(const Surface &out, Point tile, Point target, int light);

/**
 * @brief Test hook: the ground material of a woods tile (0 grass, 1 dark grass, 2 dry grass, 3 path, 4 forest floor; on the desert
 * 0 sand, 1 dune ripples, 2 cracked clay, 3 the salt road, 4 bedrock, 5 oasis grass, 6 mud, 7 reeds, 8 buried paving).
 */
uint8_t WoodsGroundAt(Point tile);
/** @brief Test hook: whether a woods tile holds a tree. */
bool WoodsTreeAt(Point tile);
/** @brief A light the woods hold by night: the cottages' lamps and candles, the wisps, the fairy ring (colour_light's night). */
struct WoodsLamp {
	float x, y, reach, strength;
	uint32_t rgb;
};
/** @brief This frame's woods lamps (empty off the woods). */
const std::vector<WoodsLamp> &WoodsNightLamps();
/** @brief The wisps and the fairy ring, drawn as light with the other lights (scrollrt's Beams pass; splats on the card). */
void DrawWoodsGlows(const Surface &out, Point firstTile, Point firstTileBuffer);
/** @brief Once a game tick on a woods floor: the fairy ring's mana. */
void TickWoods();
/**
 * @brief The desert's sky over the ground (user, 2026-10-16: "implement all the weather effects including your suggestions of sand
 * storm"): the dust devils wandering the sand, the vultures' shadows gliding over it by day, the grains trickling into the sand
 * hole. Drawn with the lights (scrollrt's Beams pass); nothing off a desert floor.
 */
void DrawDesertSky(const Surface &out, Point firstTile, Point firstTileBuffer);

/** @brief Test hook: the arrival and the way on of the floor last built. */
Point WoodsArrival();
Point WoodsWayOn();

/** @brief The minimap's type of woods megatile @p cell (0..49 each way; 0 off it): what dungeon[][] holds on other floors. */
uint8_t WoodsAutomapCell(Point cell);

/**
 * @brief The forest tile drawn for @p tile past the map's edge (the woods' box runs to six tiles of it since the 100 x 100
 * floors): the band between the edge and the box, mirrored. @p tile itself where it is on the map.
 */
Point WoodsMirrorBeyondMap(Point tile);

/** @brief The floor of @p tile past the woods' map edge, from its mirror (scrollrt's DrawFloor); false off the woods or on the map. */
bool DrawWoodsBeyondMapFloor(const Surface &out, Point tile, Point target, bool fullBright);
/** @brief And its tree (DrawTileContent). */
void DrawWoodsBeyondMapTree(const Surface &out, Point tile, Point target);
/** @brief The look draws the forest past the edge from the mirror's own cels (Tristram's tiles, nothing painted over them). */
bool WoodsBeyondMapUsesCels(const Surface &out);
/** @brief Test hook: loads the woods art as BuildWoodsLevel does outside headless mode. */
void LoadWoodsArtForTest();
/**
 * @brief Test hook (the Woods Levels Designer's exporter): stamp @p which as town pieces, rows of @p w - 0 the cottage, 1 the cave
 * mouth, 2..7 the river's parts (M1, M2, M3, M4, straight, bridge; one cell each). False past the last.
 */
bool WoodsStampForTest(int which, std::vector<uint16_t> &pieces, int &w, int &h);
/** @brief Test hook: -1 the seed decides whether a floor has a river, 0 none, 1 always. */
void SetWoodsRiverForTest(int mode);
/** @brief Test hook: the floor last generated has its river on @p tile. */
bool WoodsRiverAtForTest(Point tile);
/** @brief Test hook: the next floor generated has the cave mouth of woods floor @p floorIndex (-1: none). */
void SetWoodsCaveMouthFloorForTest(int floorIndex);
/** @brief Test hook: stamps the mouth into the floor last generated; its entrance, or {0,0} for none. */
Point StampWoodsCaveMouthForTest();
/** @brief Test hook: sets floor @p floorIndex's rung as its first visit would. */
void SetWoodsRungForTest(int floorIndex, int rung, OrclArea area = OrclArea::Woods);
/** @brief Test hook: PlanWoodsWorld for the floor last built - each planned object's kind and tile, the wisps, the fairy ring. */
struct WoodsWorldPlanForTest {
	std::vector<std::pair<int, Point>> objects;
	int wisps = 0;
	bool fairyRing = false;
	Point fairyRingAt {};
};
WoodsWorldPlanForTest PlanWoodsWorldForTest(uint32_t seed, int floor);
/** @brief Test hook: floor @p floorIndex's seed (0: not yet entered). */
void SetWoodsSeedForTest(int floorIndex, uint32_t seed, OrclArea area = OrclArea::Woods);
/** @brief Test hook: the floor last generated's oasis pool (its centre, or {0,0} for none) and its sand hole's entrance. */
Point DesertOasisForTest();
/** @brief Test hook: the floor last generated's buried ruins (their centre, or {0,0} for none) and its braziers' count (RfA-70). */
Point DesertRuinsForTest(int *braziers = nullptr);
/** @brief Test hook: the floor last generated's first scorched crater (RfA-72b), or {0,0} for none. */
Point DesertCraterForTest();
/** @brief Test hook: the floor last generated's nomad camps (RfA-73), counted by their lanterns, and the first one's lantern. */
int DesertCampsForTest(Point *first = nullptr);
/** @brief Test hook: the floor last generated's Sun-Eater arena (RfA-74), its altar, or {0,0} for none. */
Point DesertArenaForTest();
/** @brief Test hook: the kinds the area's Mob Roster lets walk its floors and caves. */
std::vector<int> AreaRosterForTest(OrclArea area);

} // namespace devilution::oracool
