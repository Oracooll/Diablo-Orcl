/**
 * @file stairless.cpp
 *
 * Oracool: a generated level without stairs - see stairless.h.
 */
#include "oracool/stairless.h"

#include <cstdint>
#include <cstdlib>
#include <cstring>

#include "engine/path.h"
#include "levels/gendung.h"

namespace devilution::oracool {

bool GeneratingStairlessLevel = false;

namespace {

/** dungeon[][] as it stood right before the stairs went in. */
uint8_t FloorUnderStairs[DMAXX][DMAXY];

/** How far OpenFloorNear looks: a rift's landing is at most a room away from open floor. */
constexpr int OpenFloorReach = 12;

bool IsOpenAllRound(Point tile)
{
	for (int dy = -1; dy <= 1; dy++) {
		for (int dx = -1; dx <= 1; dx++) {
			if (!IsTileNotSolid(tile + Displacement { dx, dy }))
				return false;
		}
	}
	return true;
}

} // namespace

void RememberFloorUnderStairs()
{
	if (!GeneratingStairlessLevel)
		return;
	memcpy(FloorUnderStairs, dungeon, sizeof(FloorUnderStairs));
}

bool TakeStairsBackOut()
{
	if (!GeneratingStairlessLevel)
		return false;
	// Only the stairs ran since the snapshot, so every difference is a stairs megatile. Protected
	// keeps the lamps, stalagmites and other minisets off it, as the stairs themselves did - the
	// landing stays as clear as a normal floor's.
	for (int j = 0; j < DMAXY; j++) {
		for (int i = 0; i < DMAXX; i++) {
			if (dungeon[i][j] == FloorUnderStairs[i][j])
				continue;
			dungeon[i][j] = FloorUnderStairs[i][j];
			Protected.set(i, j);
		}
	}
	return true;
}

Point OpenFloorNear(Point start)
{
	// Ring by ring outwards, so the first hit is one of the nearest. A theme room's wall or a
	// decoration can still fall on the landing the stairs used to keep clear; this steps off it.
	for (int r = 0; r <= OpenFloorReach; r++) {
		for (int dy = -r; dy <= r; dy++) {
			for (int dx = -r; dx <= r; dx++) {
				if (std::abs(dx) != r && std::abs(dy) != r)
					continue;
				const Point tile = start + Displacement { dx, dy };
				if (InDungeonBounds(tile) && IsOpenAllRound(tile))
					return tile;
			}
		}
	}
	return start;
}

} // namespace devilution::oracool
