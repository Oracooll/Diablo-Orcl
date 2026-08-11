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

#include "engine/point.hpp"
#include "engine/surface.hpp"

namespace devilution::oracool {

bool IsWaypointMenuOpen();

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

/** @brief Draws the menu. Only called while the menu is open (see scrollrt.cpp's DrawAndBlit).
 * Checks the walk-away distance first (see OpenWaypointMenu's doc comment) and closes the menu
 * instead of drawing if the player has moved too far from the sigil that opened it. */
void DrawWaypointMenu(const Surface &out);

/**
 * @brief Handles a left-click while the menu is open. Selecting an unlocked entry closes the
 * menu and warps the player there (a no-op if that's already where they are); clicking a locked
 * entry or empty panel space does nothing. Only called when the click already landed inside the
 * left panel area - see diablo.cpp's LeftMouseDown.
 */
void CheckWaypointMenuClick(Point mousePosition);

/**
 * @brief Whether waypoint list entry/dungeon level `index` (0 = Tristram, 1-16 = that dungeon
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

} // namespace devilution::oracool
