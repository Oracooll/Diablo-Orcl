/**
 * @file oracool/shutdown_watchdog.h
 *
 * Oracool: user report (2026-08-31) - "debug version regularly leave diabloorcl.exe proces runing
 * after exit. it is common occurance."
 *
 * Confirmed from the outside on the same day: a leftover process had NO window, no window title,
 * three threads and 0.00 seconds of CPU across four seconds. That is a shutdown that stopped rather
 * than a game still running - a deadlock somewhere in teardown, in a build with no one watching.
 *
 * It is not merely untidy. DiabloDeinit closes the MPQ archives in the MIDDLE of its list, after the
 * sound and UI teardown, so a hang anywhere before that leaves oracool.mpq locked - and the next
 * build fails with "MoveFileExW ... error code 5" while packing it. That has cost three builds.
 *
 * ## Why a watchdog rather than a fix to the hang
 *
 * The honest reason: the blocking call has not been identified. It cannot be reproduced here (the
 * game is not run by this process) and the symptom appears after the window is gone, so there is
 * nothing on screen to read. Guessing at SDL_Quit or the audio teardown and "fixing" one of them
 * would be a change with no evidence behind it.
 *
 * What IS certain is that by the time teardown begins, everything the player owns is already on
 * disk - SaveOnExit runs before this, and the archives are read-only. Nothing after that point is
 * worth waiting on, so a process still alive seconds later is pure cost with nothing to lose by
 * ending it.
 *
 * So this does not pretend to cure the deadlock. It bounds it.
 */
#pragma once

#include <cstdint>

namespace devilution::oracool {

/**
 * @brief Starts a detached thread that force-ends this process if shutdown takes too long.
 *
 * Call once, immediately before teardown begins. A normal exit finishes long before the timeout and
 * the thread dies with the process, having done nothing.
 *
 * @param timeoutMs how long teardown is allowed to take. Generous on purpose: a healthy shutdown is
 *                  well under a second, so anything approaching this is already wrong.
 */
void ArmShutdownWatchdog(uint32_t timeoutMs = 5000);

} // namespace devilution::oracool
