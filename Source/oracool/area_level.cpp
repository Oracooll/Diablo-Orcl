#include "oracool/area_level.h"

#include <algorithm>

#include "multi.h"
#include "oracool/named_encounters.h"
#include "quests.h"
#include "utils/language.h"

namespace devilution::oracool {

namespace {

/** @brief Difficulty's place on the ladder: 0, 1, 2, 3. One 24-floor block each. */
int DifficultyBlock(_difficulty difficulty)
{
	switch (difficulty) {
	case DIFF_NORMAL:
		return 0;
	case DIFF_NIGHTMARE:
		return 1;
	case DIFF_HELL:
		return 2;
	case DIFF_TORMENT:
		return 3;
	}
	return 0;
}

/** @brief The area names, in floor order. */
const char *const AreaNames[AreaCount] = {
	N_("Cathedral"),
	N_("Catacombs"),
	N_("Caves"),
	N_("Hell"),
	N_("Nest"),
	N_("Crypt"),
};

} // namespace

int AreaLevel(int floor, _difficulty difficulty)
{
	const int clampedFloor = std::clamp(floor, 1, AreaFloorCount);
	return clampedFloor + AreaFloorCount * DifficultyBlock(difficulty);
}

int CurrentAreaLevel()
{
	// A quest set-level takes the level of the quest that owns it, run through the same ladder - so
	// the Skeleton King's lair is as deep as the floor it hangs off, on every difficulty. The
	// vanilla fall-through of "1" is kept for an unrecognised set-level: an unknown place should be
	// the LEAST rewarding thing on the ladder, never the most.
	int floor = currlevel;
	if (setlevel) {
		switch (setlvlnum) {
		case SL_SKELKING:
			floor = Quests[Q_SKELKING]._qlevel;
			break;
		case SL_BONECHAMB:
			floor = Quests[Q_SCHAMB]._qlevel;
			break;
		case SL_POISONWATER:
			floor = Quests[Q_PWATER]._qlevel;
			break;
		case SL_VILEBETRAYER:
			floor = Quests[Q_BETRAYER]._qlevel;
			break;
		default:
			// A named encounter's arena has no quest to take its depth from, so it carries its own
			// (oracool/named_encounters.cpp). Audit finding, 2026-08-26: all three fell through to
			// the `1` below, which is the correct answer for an UNKNOWN place and the wrong one for
			// an endgame fight - a Dread boss guarding a guaranteed unique charm was paying floor-1
			// loot and the HUD was reporting floor-1 depth.
			if (!NamedEncounterFloorForSetLevel(setlvlnum, floor))
				floor = 1;
			break;
		}
	}
	return AreaLevel(floor, sgGameInitInfo.nDifficulty);
}

int AreaIndexOfFloor(int floor)
{
	const int clampedFloor = std::clamp(floor, 1, AreaFloorCount);
	return (clampedFloor - 1) / FloorsPerArea;
}

const char *AreaNameOfFloor(int floor)
{
	return AreaNames[AreaIndexOfFloor(floor)];
}

} // namespace devilution::oracool
