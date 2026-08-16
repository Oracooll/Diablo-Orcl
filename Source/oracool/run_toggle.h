/**
 * @file oracool/run_toggle.h
 *
 * Oracool: Megaplan Phase 2.5 - the run toggle.
 *
 * Running IS "Run In Town", everywhere: the same double-speed frame skip StartWalkAnimation has
 * always applied when bRunInTown says so (and that Furious Charge's dash borrowed) - one proven
 * mechanism, third consumer. The toggle is session state on the R key, default off, single-player
 * only like every other Oracool combat-adjacent change. No stamina bar: the megaplan lists
 * stamina as optional and D1's danger comes from what you walk into, not from a second resource.
 */
#pragma once

namespace devilution::oracool {

/** @brief Whether the run toggle is on - consulted by StartWalkAnimation beside bRunInTown. */
bool IsRunEnabled();

/** @brief Flips the toggle and says so in the event log. Bound to R (settable in keymapping). */
void ToggleRun();

} // namespace devilution::oracool
