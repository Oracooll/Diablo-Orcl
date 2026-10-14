/**
 * @file oracool/borrowed_tiles.h
 *
 * Tiles from every vanilla tileset on one level (user, 2026-10-14: "i dont want us to limit ourselves to one tileset. i want to
 * be able to use all vanilla tilesets in orcl act mixed anyway i like"). A level keeps its own tileset in dPiece's ids below
 * BorrowedPieceBase; a piece of any other set gets an id above it, from BorrowPiece. Such an id walks, blocks light and
 * missiles by its own set's .sol (copied into SOLData), and draws as a true-colour picture rendered once from its own set's cels
 * in its own set's palette - lit by the level's light, and see-through round the hero when it stands in front of him.
 *
 * Everything is read at run time from the player's own game files; nothing of Blizzard's is shipped.
 */
#pragma once

#include <cstdint>

#include "engine/point.hpp"
#include "levels/gendung.h"

namespace devilution {
struct Surface;
} // namespace devilution

namespace devilution::oracool {

/** @brief The first borrowed id: past the largest native tileset's pieces. */
constexpr uint16_t BorrowedPieceBase = 1379;
/** @brief How many borrowed pieces a session can hold (MAXTILES is BorrowedPieceBase plus this). */
constexpr int BorrowedPieceCapacity = MAXTILES - BorrowedPieceBase;

inline bool IsBorrowedPiece(uint16_t piece)
{
	return piece >= BorrowedPieceBase;
}

/**
 * @brief The dPiece id for piece @p piece (as dPiece holds it, 0-based) of tileset @p set: its native id when @p set is the
 * level's own, a borrowed id otherwise (rendered on the first call, kept for the session). 0 when the set's files are missing
 * or the capacity is spent.
 */
uint16_t BorrowPiece(dungeon_type set, uint16_t piece);

/**
 * @brief The 2 x 2 tiles at (@p x, @p y) made megatile @p megatile (1-based, as FillTile takes it) of tileset @p set.
 * @return false when the set or the megatile is not there; the tiles are then left as they were
 */
bool BorrowMegatile(dungeon_type set, int megatile, int x, int y);

/** @brief The borrowed pieces' tile properties back into SOLData: after every LoadLevelSOLData. */
void ApplyBorrowedSol();

/** @brief The four native pieces of megatile @p megatile (1-based) of @p set - top, right, left, bottom. */
bool BorrowedMegatilePieces(dungeon_type set, int megatile, uint16_t (&pieces)[4]);

/** @brief How many megatiles tileset @p set has (0 when its files are missing). */
int BorrowedMegatileCount(dungeon_type set);

/**
 * @brief A borrowed piece's floor (its two bottom micros) at @p target, the tile's bottom-left as DrawFloor takes it. Only the
 * triangles for a solid piece, as DrawWallFloorTriangles draws them. @p light < 0 is full brightness.
 */
void DrawBorrowedFloor(const Surface &out, uint16_t piece, Point target, int light);

/**
 * @brief A borrowed piece's upright part (everything above its floor, and the bottom micros' wall halves) at @p target, as
 * DrawCell draws a piece, for tile @p tile - see-through round the hero when it stands in front of him.
 * @param withFloorTriangles false when the floor pass already drew them (FloorSmoothedThisFrame)
 */
void DrawBorrowedCell(const Surface &out, Point tile, uint16_t piece, Point target, int light, bool withFloorTriangles);

/** @brief Test hook: the borrowed piece's set and native piece, or false for a native id. */
bool BorrowedPieceSourceForTest(uint16_t piece, dungeon_type &set, uint16_t &native);
/** @brief Test hook: how many opaque pixels the piece's floor and upright pictures hold. */
void BorrowedPiecePixelsForTest(uint16_t piece, int &floorPixels, int &cellPixels);

} // namespace devilution::oracool
