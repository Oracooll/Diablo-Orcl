/**
 * @file oracool/waypoint_menu.h
 *
 * Oracool: user request - the waypoint sigil's travel list. Visually styled after the Quest Log
 * (same parchment panel art, same left panel slot) but deliberately NOT wired into the Quest Log's
 * own state machine - it's an independent, self-contained panel with its own open/close state and
 * a minimal interaction surface (mouse click to select/close, ESC to close via IsLeftPanelOpen()/
 * ClosePanels()). It does not get the Quest Log's keyboard arrow-key navigation, gamepad, or touch
 * support - those are wired into a dozen+ call sites across the codebase and out of scope for this
 * first pass. Single-player only, matching every other Oracool town-object feature.
 */
#pragma once

#include <cstdint>

#include "engine/point.hpp"
#include "engine/surface.hpp"

namespace devilution::oracool {

bool IsWaypointMenuOpen();

/**
 * @brief The three ACTS the list is split into (user, 2026-09-20), each behind one of the abilities
 * window's 56x56 buttons under the title: Diablo's sixteen floors, Hellfire's Nest and Crypt, and
 * the Orcl areas to come. Tristram heads all three. The enum's order is the buttons' order.
 */
enum class WaypointAct : uint8_t {
	Diablo,
	Hellfire,
	Orcl,
};

/** @brief The act whose list is showing. Persists across opens; OpenWaypointMenu picks the level's act in a dungeon. */
WaypointAct ActiveWaypointAct();
/** @brief Shows @p act's list from the top. What a click on an Act button does. */
void SelectWaypointAct(WaypointAct act);
/** @brief Rows in @p act's list: 17 / 9 / 1 (Tristram alone) - the Hellfire act is 1 too outside a Hellfire game. */
size_t WaypointActRowCount(WaypointAct act);
/** @brief The dungeon level at row @p row of @p act's list, or -1 past its end. */
int WaypointActLevelAt(WaypointAct act, size_t row);
/** @brief The act a dungeon level belongs to: 17-24 are Hellfire's, everything else Diablo's. */
WaypointAct WaypointActOfLevel(int level);

/**
 * @brief Lets go of a pressed Act button. Called from diablo.cpp's LeftMouseUp, like every other
 * button that sinks while the mouse is held (user, 2026-09-20: "When i click on an Act button move the
 * 88x68px button+text 2px down and 2px left. Keep it there until mouse button is released. Dont move
 * the hover shadow with it."). Safe to call when nothing is pressed.
 */
void ReleaseWaypointActButton();

/**
 * @brief Opens the travel list. Currently only the Tristram entry is selectable - no other
 * waypoint has anywhere to travel to yet.
 * @param sigilPosition The tile of the waypoint object that opened this menu. Diablo's own NPC
 * dialogs don't auto-close on distance - they simply block movement while open - but this menu
 * doesn't block movement, so without a walk-away check it would stay open and usable no matter
 * how far the player wanders. Remembered here so DrawWaypointMenu() can close it once the player
 * strays more than 1 tile away, matching the adjacency radius every other object interaction in
 * this engine already uses (see IsPlayerAdjacentToObject in player.cpp).
 */
void OpenWaypointMenu(Point sigilPosition);

/** @brief Closes the travel list. Safe to call even when already closed. */
void CloseWaypointMenu();

/**
 * @brief Scrolls the travel list by one row. Mouse-wheel, dispatched from diablo.cpp alongside the
 * event log and the other scrolling windows.
 *
 * The list is 25 rows of 45px against a 595px viewport, so roughly half of it is off-screen at any
 * time and scrolling is the only way to reach the Crypt. Both are no-ops at their respective ends,
 * and both re-clamp first, since the row count depends on whether this is a Hellfire game.
 */
void ScrollWaypointMenuUp();
void ScrollWaypointMenuDown();

/** @brief Draws the menu. Only called while the menu is open (see scrollrt.cpp's DrawAndBlit).
 * Checks the walk-away distance first (see OpenWaypointMenu's doc comment) and closes the menu
 * instead of drawing if the player has moved too far from the sigil that opened it. */
void DrawWaypointMenu(const Surface &out);

/**
 * @brief Screen rect of the waypoint list - 340x720, flush to the top-left corner.
 *
 * Exported so control.cpp's GetLeftPanelContentRect() can route and absorb clicks over the whole
 * window. Routing used to go through GetLeftPanel()'s 320x352, which left entries 8-16 - every row
 * below y=352 - unclickable, with the click falling through and walking the player instead.
 */
Rectangle GetWaypointMenuRect();

/**
 * @brief Handles a left-click while the menu is open. Selecting an unlocked entry closes the
 * menu and warps the player there (a no-op if that's already where they are); clicking a locked
 * entry or empty panel space does nothing. Only called when the click already landed inside the
 * menu's own rect - see diablo.cpp's LeftMouseDown and GetLeftPanelContentRect().
 */
void CheckWaypointMenuClick(Point mousePosition);

/**
 * @brief Whether waypoint list entry/dungeon level `index` (0 = Tristram, 1-24 = that dungeon
 * level, matching currlevel numbering) has been unlocked on the current difficulty. Index 0 is
 * always unlocked. Persisted per character per difficulty in Player::_pWaypointUnlocked (see its
 * doc comment in player.h) - unlocking on Normal doesn't unlock the same waypoint on Nightmare/
 * Hell/Torment. Reads MyPlayer and sgGameInitInfo.nDifficulty; only meaningful once a game is
 * loaded.
 */
bool IsWaypointUnlocked(int index);

/**
 * @brief Marks waypoint list entry/dungeon level `index` unlocked on the current difficulty
 * (Player::_pWaypointUnlocked). Safe to call repeatedly; a no-op for index 0 (Tristram), which is
 * always unlocked regardless of stored state.
 */
void UnlockWaypoint(int index);

/**
 * @brief Returns whether the player should spawn at the destination's waypoint sigil instead of
 * the level's normal entry point, clearing the flag either way. Called once by
 * AddWaypointSigilObject() (objects.cpp) right after it places the sigil for the level being
 * entered - that's the only point that knows the sigil's actual position, since a dungeon level's
 * sigil is placed at a fresh random spot every visit (town's is fixed). This has to be a
 * "reposition after the fact" flag rather than setting ViewPosition directly from
 * CheckWaypointMenuClick(), because by the time AddWaypointSigilObject() runs during level load,
 * InitPlayer() has already consumed the level's original ViewPosition to place the player -
 * setting ViewPosition alone at that point would only move the camera, not the player.
 */
bool ConsumeWaypointSpawnRequest();

/**
 * @brief Clears the menu and any unconsumed spawn request, for game teardown.
 *
 * The open flag, the scroll offset and the spawn request are file-local statics, so they outlive a
 * GAME - they live as long as the process. The request is normally set and consumed inside one level
 * transition, but nothing guarantees the transition finishes: quit between the warp and the
 * destination's sigil being placed and the flag is still true when the NEXT character loads their
 * first level, which would teleport them onto that level's waypoint.
 *
 * The same shape as ResetLevskiRoarForNewGame and ClearEventLogForNewGame (audit, 2026-08-30); this
 * is the sweep for their siblings.
 */
void ResetWaypointMenuForNewGame();

/**
 * @brief Raises the spawn request without warping anywhere. For tests only.
 *
 * The real setter sits inside the menu's click handler, one line before a StartNewLvl that a
 * headless test cannot survive. Without this hook the teardown test could only assert that a flag
 * nothing set is still unset, which proves nothing at all.
 */
void SetWaypointSpawnRequestForTest();

} // namespace devilution::oracool
