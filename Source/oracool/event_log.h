/**
 * @file event_log.h
 *
 * Oracool: a collapsible, timestamped log of noteworthy session events (game saves, boss kills,
 * special item drops, character deaths) - a Diablo 3-style "message log" window, toggled by a
 * small always-visible button in the top-left corner. Session-only: not persisted to disk or
 * cleared on level/game transitions - it simply accumulates for as long as the client stays open,
 * capped at a fixed capacity (oldest entries drop off first).
 */
#pragma once

#include <string>

#include "DiabloUI/ui_flags.hpp"
#include "engine/point.hpp"
#include "engine/rectangle.hpp"
#include "engine/surface.hpp"

namespace devilution::oracool {

/**
 * @brief Appends a new entry stamped with the current wall-clock time. Oldest entries are
 * dropped once the log exceeds its capacity. Safe to call even when the log window is closed -
 * entries still accumulate, just aren't drawn until it's opened. Entries are gold by default;
 * pass a different color (e.g. UiFlags::ColorRed, used for shrine stat effects) to stand out.
 */
void LogEvent(std::string message, UiFlags color = UiFlags::ColorGold);

/** @brief Opens or closes the event log window. Resets scroll position back to the newest entries. */
void ToggleEventLog();

/** @brief True when the Event Log option is on and the window is currently open. */
bool IsEventLogOpen();

/**
 * @brief True while the corner HUD beside the mini-map is drawn: no full map, no right panel, no chat, no
 * runeword book. The log, the clock and the companion and minion headers draw only then, so their click
 * rects must answer only then - a hidden log swallowed clicks and the wheel (round 3 audit, v1.12.228).
 */
bool IsCornerHudShown();

/**
 * @brief Empties the log and closes it, for game teardown.
 *
 * The entries live in a file-local deque, so they outlive a GAME rather than the process. Without
 * this the next character started in the same session opened the log onto the previous one's
 * history (audit, 2026-08-30).
 */
void ClearEventLogForNewGame();

/**
 * @brief How many entries the log currently holds.
 *
 * Exported so the teardown above can be tested on what it actually does rather than on the window
 * flag, which would go on passing if the entries survived - the exact shape of test this audit was
 * looking for.
 */
size_t EventLogEntryCount();

/**
 * @brief The window's screen rect, empty when closed - for click-through rejection.
 *
 * The log fills the whole column under the mini-map, and until 2026-08-30 it was in none of the
 * rejection lists: a click on it walked the character and a hover through it highlighted whatever
 * stood behind.
 */
Rectangle GetEventLogWindowRect();

/** @brief Scrolls toward the newest entries. No-op if already at the top. Mouse-wheel-up. */
void ScrollEventLogUp();

/** @brief Scrolls toward older entries. No-op if already at the oldest visible page. Mouse-wheel-down. */
void ScrollEventLogDown();

/** @brief Draws the event log window if currently open. Call once per frame during gameplay.
 * Oracool: the standalone "LOG" button that used to toggle this is gone (2026-08-11) - the log is
 * opened from the belt's Menu popup now, see oracool/hud_menu.cpp's entry list. */
void DrawEventLogWindow(const Surface &out);

/**
 * @brief Records the name of whatever is about to (or might) damage the player, so that if this
 * hit turns out to be fatal, LogPlayerDeath can attribute it correctly. Call right before a
 * damage-applying call whenever the attacker is known (a monster, a trap). Safe to call on
 * non-fatal hits too - it's only ever read (and cleared) by LogPlayerDeath, so a hit that doesn't
 * kill just gets silently overwritten by whatever damages the player next.
 */
void NotePendingDeathSource(std::string source);
/** @brief Forgets the pending source: the blow it named did not kill (ApplyPlrDamage). */
void ClearPendingDeathSource();

/**
 * @brief Logs a player death, attributing it to whatever NotePendingDeathSource last recorded, or
 * to fallbackReason if nothing was recorded (e.g. a death path that doesn't go through a hit
 * function that calls NotePendingDeathSource). Clears the pending source afterwards. Call once
 * per actual death, not on the vanilla town-revival safety net.
 */
void LogPlayerDeath(const std::string &fallbackReason);

} // namespace devilution::oracool
