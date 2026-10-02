/**
 * @file oracool/hud_menu.h
 *
 * Oracool: user request - Phase 1 of the 960x720 HUD overhaul. The old 8 panel buttons (Character,
 * Quests, Automap, Game Menu, Inventory, Spellbook, Chat, Friendly-fire) are gone; this is their
 * single replacement, opened by clicking the new belt's Menu slot (see hud_layout.h's
 * BeltMenuSlotIndex). Two mini-map controls (previously keybind-only) are folded in alongside them,
 * per the approved HUD design.
 *
 * Deliberately NOT built on gmenu.cpp's pause-menu singleton (sgpCurrentMenu) - ~10 call sites
 * across the codebase (nthread.cpp, diablo.cpp, controls/plrctrls.cpp, oracool/auto_save.cpp,
 * qol/itemlabels.cpp) treat gmenu_is_active() as "gameplay is frozen," which this popup must not
 * do. Pattern-matched instead on oracool/waypoint_menu.h/.cpp: its own open/close bool, its own
 * draw + click-dispatch functions, no shared state with any other panel.
 *
 * This file also owns the belt's two special (non-item) slots - the Menu button itself and the
 * permanent Town Portal button - since both are simple, self-contained "draw the button, handle a
 * click" pairs with no other public surface. The drawing is hud_art.cpp's: each wears its glyph
 * sheet (ui\belt_glyphs_menu.png, ui\belt_glyphs_tp.png), falling back to "M" / "TP" text without it.
 */
#pragma once

#include "engine/point.hpp"
#include "engine/rectangle.hpp"
#include "engine/surface.hpp"
#include "utils/stdcompat/string_view.hpp"

namespace devilution::oracool {

bool IsHudMenuOpen();
void CloseHudMenu();

/** @brief Draws the icon row. Only called while it's open (see scrollrt.cpp's DrawAndBlit). */
void DrawHudMenu(const Surface &out);

/** @brief Index of the menu icon under `mousePosition`, or -1. Returns -1 while the row is closed. */
int HitTestHudMenuIcon(Point mousePosition);

/** @brief An entry's name, for the hover tooltip. Empty for an out-of-range index. */
string_view GetHudMenuEntryLabel(int index);

/** @brief Whether `mousePosition` falls on the open icon row - so clicks there count as UI rather
 * than falling through to the world. See diablo.cpp's LeftMouseDown. */
bool IsPointOverHudMenu(Point mousePosition);

/** @brief The menu window, for tests and for anything that must not draw over it. Empty when closed is NOT implied - it is the geometry regardless. */
Rectangle GetHudMenuWindowRect();
/** @brief Cell @p index of the window: a 37x38 plate. */
Rectangle GetHudMenuCellRect(int index);

/**
 * @brief Handles a left-click while the menu is open. Dispatches to whichever of the 10 entries
 * was clicked (a straight relocation of control.cpp's old CheckBtnUp switch bodies, plus the 2
 * mini-map entries - no new gameplay logic), or closes the menu if the click landed outside it.
 * Only called when the menu is already open - see diablo.cpp's LeftMouseDown.
 */
void CheckHudMenuClick(Point mousePosition);

/**
 * @brief Checks whether `mousePosition` hit the plate's Menu cell and, if so, toggles the popup.
 * Returns true if the click was handled (caller should not fall through to normal belt/item
 * handling). See diablo.cpp's LeftMouseDown. The cell's visual (frame + "Menu" label) is baked
 * into the plate art itself (assets/ui/middle_hud.png, see oracool/hud_art.h) - nothing to draw.
 */
bool CheckHudMenuSlotClick(Point mousePosition);

/**
 * @brief Checks whether `mousePosition` hit the plate's Town Portal cell and, if so, casts it.
 * Returns true if the click was handled. The cast itself is a no-op in multiplayer - the
 * underlying always-memorized, zero-mana Town Portal behavior this button relies on is
 * single-player only. Like the Menu cell, its visual is baked into the plate art.
 */
bool CheckTownPortalBeltSlotClick(Point mousePosition);

/**
 * @brief Handles a click on the belt's Walk/Run toggle (the seventh cell). True if it was consumed.
 *
 * The same thing the R key does, on the belt - see oracool/run_toggle.h.
 */
bool CheckRunToggleBeltSlotClick(Point mousePosition);

/**
 * @brief LeftMouseUp for the menu's icons and the belt's Menu, Portal and Run cells. The four Check* functions above
 * only PRESS (the plate sinks, the cell shows its pressed picture); the pressed control runs here, and only when the
 * release lands back inside it - the press/release default (round 75 audit). The window's X is the shared one's
 * (ReleaseWindowCloseButton). Safe when nothing is pressed.
 */
void ReleaseHudMenuButtons();

/**
 * @brief Draws click feedback over the plate's two button cells, since neither has any art state
 * of its own to react with (their frames and icons are baked into the plate image).
 *
 * The Menu cell stays lit for as long as its popup is open, so the button reads as toggled rather
 * than just blinking. Both cells also flash briefly the moment they are clicked - which for the
 * Town Portal cell is the only feedback available, the cast itself being instantaneous.
 *
 * Call after DrawInvBelt so it lands on top of the plate (see scrollrt.cpp).
 */
void DrawBeltButtonFeedback(const Surface &out);

/**
 * @brief Opens a Town Portal at the player's own feet, bypassing the normal cursor-target dispatch
 * (CheckPlrSpell/LeftMouseCmd) since a fixed belt button has no cursor world-tile to target.
 * Reuses the same CMD_SPELLXY wire command the normal cast path uses, so player.cpp's existing
 * handler needs no changes. No-op in town (nothing to portal from) or multiplayer.
 *
 * @return whether the cast was sent - false for those two no-ops, so the belt cell can give its
 * refusal a click (the cast itself sounds as the spell).
 */
bool CastTownPortalAtFeet();

} // namespace devilution::oracool
