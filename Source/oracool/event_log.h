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

#include "engine/point.hpp"
#include "engine/surface.hpp"

namespace devilution::oracool {

/**
 * @brief Appends a new entry stamped with the current wall-clock time. Oldest entries are
 * dropped once the log exceeds its capacity. Safe to call even when the log window is closed -
 * entries still accumulate, just aren't drawn until it's opened.
 */
void LogEvent(std::string message);

/** @brief Opens or closes the event log window. Resets scroll position back to the newest entries. */
void ToggleEventLog();

/** @brief True when the Event Log option is on and the window is currently open. */
bool IsEventLogOpen();

/** @brief Scrolls toward the newest entries. No-op if already at the top. Mouse-wheel-up. */
void ScrollEventLogUp();

/** @brief Scrolls toward older entries. No-op if already at the oldest visible page. Mouse-wheel-down. */
void ScrollEventLogDown();

/** @brief Draws the always-visible toggle button. Call once per frame during gameplay. */
void DrawEventLogButton(const Surface &out);

/** @brief Draws the event log window if currently open. Call once per frame, after DrawEventLogButton. */
void DrawEventLogWindow(const Surface &out);

/**
 * @brief Hit-tests the toggle button against a click position; toggles the window and returns
 * true if the button was hit, false otherwise (caller should let the click fall through).
 */
bool CheckEventLogButtonClick(Point mousePosition);

/**
 * @brief Records the name of whatever is about to (or might) damage the player, so that if this
 * hit turns out to be fatal, LogPlayerDeath can attribute it correctly. Call right before a
 * damage-applying call whenever the attacker is known (a monster, a trap). Safe to call on
 * non-fatal hits too - it's only ever read (and cleared) by LogPlayerDeath, so a hit that doesn't
 * kill just gets silently overwritten by whatever damages the player next.
 */
void NotePendingDeathSource(std::string source);

/**
 * @brief Logs a player death, attributing it to whatever NotePendingDeathSource last recorded, or
 * to fallbackReason if nothing was recorded (e.g. a death path that doesn't go through a hit
 * function that calls NotePendingDeathSource). Clears the pending source afterwards. Call once
 * per actual death, not on the vanilla town-revival safety net.
 */
void LogPlayerDeath(const std::string &fallbackReason);

} // namespace devilution::oracool
