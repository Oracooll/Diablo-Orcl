#pragma once
/**
 * @file oracool/level_preload.h
 *
 * Oracool: the heavy art and sound a floor will ask for, got ready while the loading screen is still up rather than in
 * the tick it is first needed (user, 2026-10-10, after DamnationX: "never stall a frame"). One call at the end of
 * LoadGameLevel, one pump a game tick:
 *
 *  - the floor's sized monsters (Runt / Giant / Colossal, rift bosses): every scaled sheet queued nearest monster
 *    first on a worker thread, the unscaled sprites standing in until each arrives (oracool/monster_scale.h);
 *  - the hero's skill cues, for every skill with a point in it (oracool/skill_sounds.h);
 *  - the monster types the hero's minions will need on this floor - the army waiting to re-form, and the bodies his
 *    Raise skills would stand up now - so AddMinionBody finds them loaded (monster.h, PreloadMinionType);
 *  - the companions' dungeon sheets, for their return a quarter-second after arrival (oracool/companion.h).
 *
 * The PNG missile sheets are the fifth piece and need no call here: misdat.cpp keeps them between levels.
 */

#include <cstddef>

namespace devilution::oracool {

/** @brief How many free monster type slots a minion preload leaves for whatever the floor still needs. */
constexpr size_t MinionPreloadKeepFree = 1;

/** @brief End of LoadGameLevel, the hero on his tile: everything above. */
void PreloadForLevel();

/** @brief Once a game tick, main thread: delivers what the background work has finished. */
void PumpLevelPreload();

/** @brief Leaving the game: drops what was prepared for a level nobody will play. */
void ForgetLevelPreloads();

} // namespace devilution::oracool
