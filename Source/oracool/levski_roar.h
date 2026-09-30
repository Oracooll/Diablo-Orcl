/**
 * @file oracool/levski_roar.h
 *
 * Oracool: Levski's Roar - the crafting monument in town, and the window it opens.
 *
 * Diablo III's Kanai's Cube, with Diablo I's furniture. The transmute area is a 3x4 grid - one row
 * deeper than Kanai's own 3x3, at the user's request, and the same twelve slots Diablo II's
 * Horadric Cube used. The Cube's three legendary-power slots below the grid are absent: legendary
 * powers are not built, and an empty row of slots promising a system that does not exist is worse
 * than no row at all.
 *
 * The monument has its own art as of 2026-08-20: objects\orclroar.cel, three town tiles wide, built
 * by tools/MonumentCel.cs from the user's painting. It is still an OBJ_STAND - the type is only the
 * carrier now, and the sprite is swapped onto the instance (ApplyLevskiRoarGraphics in objects.cpp)
 * so the Caves' actual rock stands are untouched. Before that it wore rockstan.cel outright, as an
 * openly-labelled placeholder.
 *
 * The window has its own art too: the painted skin ui\levski_bg.png, with the levski_<stem>_hover
 * and _pressed plates laid over it for state (see levski_roar_skin.h). Before that it wore the
 * ordinary ornate border on a flat ground, which is now only the fallback when the skin is missing.
 *
 * Items placed in the grid are NEVER persisted: closing the window returns them to the backpack.
 * That is what keeps a crafting station out of the save format entirely - there is no state to
 * write, so there is no format to break and nothing to recover if the game exits with the window
 * open.
 */
#pragma once

#include "engine/point.hpp"
#include "engine/rectangle.hpp"
#include "engine/surface.hpp"
#include "oracool/crafting.h" // TransmuteHost
#include "oracool/levski_roar_skin.h"

namespace devilution {
struct Player;
struct Item;
struct Object;
} // namespace devilution

namespace devilution::oracool {

struct SpriteColours; // oracool/sprite_colours.h - the Cube's own colours

/** @brief The transmute grid, in cells. 3x4 - the Horadric Cube's twelve - until the second painted
 * skin (2026-09-04), which is an 8x10 well; the grid IS the painting's grid, so the size comes from
 * the generated skin header. Not saved: the grid empties with the game, like the cube did. */
constexpr int LevskiGridColumns = levski_skin::GridColumns;
constexpr int LevskiGridRows = levski_skin::GridRows;
constexpr int LevskiGridSlots = LevskiGridColumns * LevskiGridRows;

/**
 * @brief Whether @p count items would all fit in the grid AT ONCE, by footprint.
 *
 * The recipes rewrite the grid's item array with no idea of footprints, so "is there room?" cannot
 * be answered by counting free array slots - twelve slots is not twelve free cells once a 2x3
 * breastplate is in one of them. This runs the REAL placement (largest first, first fit) against a
 * scratch occupancy map and reports whether every item found a home.
 *
 * Lives here rather than in crafting.cpp because it must not drift from PlaceInGrid: the packing
 * that answers the question and the packing that happens afterwards are the same code.
 */
bool LevskiGridCanHold(const Item *items, int count);

/** @brief Places the monument in town. Called on fresh town generation, like the stash chest. */
void AddLevskiRoarObject();

/**
 * @brief Whether @p object is the town monument rather than a Caves rock stand.
 *
 * The same test the graphics swap and the operate path already make, given a name so the
 * hold-to-repeat guard in track.cpp does not become a fourth copy of it. Town has exactly one
 * OBJ_STAND and it is this one; the Caves' stands are on other levels.
 */
bool IsLevskiRoarObject(const Object &object);

/** @brief Whether the monument's window is open. */
bool IsLevskiRoarOpen();

/**
 * @brief Fills InfoString from the grid item under the cursor. True if it did.
 *
 * User, 2026-09-03: "when i moved my socketed ring in levski's grid hovering over it show no pop-up
 * of the item socketed in it [...] Make sure levski's grid works as stash or inv grid."
 *
 * Called from UpdateInfoString beside the shop grid's equivalent, for the same reason that one is
 * called there rather than from the draw: the panel text is rebuilt once per frame by that pass, and
 * a producer that runs anywhere else is either overwritten by it or overwrites it.
 */
bool SetLevskiHoverInfoString();
/**
 * @brief The grid item under the cursor, or nullptr - for the cursor tooltip, which gives it the
 * backpack's plate (user, 2026-09-05: "golden outline, transparent dark backing") and the
 * EQUIPPED ITEM comparison beside it.
 */
const Item *HoveredLevskiGridItem();
/** @brief Opens the window; closes it if already open. Called from the object's operate path. */
void ToggleLevskiRoar();
/** @brief Closes the window and returns everything in the grid to the backpack. */
void CloseLevskiRoar();
/**
 * @brief Levski's Cube (2026-09-20): opens the window on one host's recipe book. The Cube object opens
 * TransmuteHost::Cube (ToggleLevskiRoar does the same); Griswold's Forge tab and Ogden's and Gillian's
 * menu lines open theirs. Closes any open store first.
 */
void OpenLevskiWindowFor(TransmuteHost host);
TransmuteHost CurrentTransmuteHost();
/** @brief Per game tick: the Cube's twelve-frame idle loop and its open pose (a sheet of thirteen; the Roar's one frame is left alone). */
void ProcessLevskiCubeAnimation();

/**
 * @brief Gives the Cube in town its animated sheet (objects\levski_cube.png, 2026-10-01): the closed idle loop, the
 * opening (played backwards to close) and the opened idle loop, in true colour. Leaves the one-frame painting when the
 * sheet is missing.
 */
void ApplyLevskiCubeSheet(Object &cube);

/** @brief The Cube's own colours when @p object is the Cube wearing its animated sheet; null for everything else. */
const SpriteColours *LevskiCubeColoursFor(const Object &object);

/**
 * @brief While the Cube stands open, the window's grid in miniature on the Cube's own pink panel (user, 2026-10-01): what
 * is placed in the window shows there too, tiny. @p bottomLeft is where the object's sprite was drawn.
 */
void DrawLevskiCubeLiveGrid(const Surface &out, const Object &cube, Point bottomLeft);

/**
 * @brief Clears the window and its grid outright, for game teardown. Returns nothing to anyone.
 *
 * The grid and the open flag are file-local statics, so they outlive a GAME - they live as long as
 * the process. Leaving to the main menu closes no windows, and CloseLevskiRoar may refuse while the
 * pack is full, so without this the next character started in the same session found this window
 * already open holding the previous character's items (audit, 2026-08-30).
 */
void ResetLevskiRoarForNewGame();

/** @brief Whether the recipe book popup is showing. Toggled from the window's own button. */
bool IsLevskiRecipeBookOpen();
/** @brief Closes the recipe book popup alone (Escape takes it before the Cube). */
void CloseLevskiRecipeBook();

/** @brief The window's screen rect - used for click-through rejection like every other panel. */
Rectangle GetLevskiRoarRect();
/** @brief Whether @p position is over the Cube - its window or its side tabs, which sit outside the window. */
bool IsPointOverLevski(Point position);
/** @brief The recipe book's screen rect, empty when closed. */
Rectangle GetLevskiRecipeBookRect();

/**
 * @brief Scrolls the recipe book by @p notches. True if it consumed the wheel event.
 *
 * The book grew past the screen when the recipe list reached eighteen, so it is capped and scrolled
 * rather than sized to its content. False when the book is closed, so the wheel falls through to
 * whatever else wants it.
 */
bool HandleLevskiRecipeBookScroll(int notches);

/** @brief Draws the window and, over it, the recipe book. Call once per frame. */
void DrawLevskiRoar(const Surface &out);

/** @brief Routes a click. True when the click was consumed by the window. */
bool CheckLevskiRoarClick(Point mousePosition, bool isCtrlHeld);

/** @brief LeftMouseUp: the painted Cube UI's pressed button (TRANSMUTE / RECIPE BOOK) springs back (2026-09-20). */
void ReleaseLevskiButtons();
/** @brief Whether Griswold's Salvage page has armed the hammer to break ONE backpack item down (2026-09-21). */
bool IsSalvageItemCursorArmed();

/** @brief Takes that hammer back without salvaging anything - a click that landed on no item. */
void CancelSalvageItemCursor();

/**
 * @brief That hammer's click: salvages the item at @p index (@p tab -1 for the main backpack), writes the window's
 * message and disarms. False, with a line in the log, when the item cannot be broken down.
 */
bool UseSalvageItemCursor(Player &player, int tab, int index);

/**
 * @brief Copies @p item into the first grid slot its footprint fits. False if it does not fit.
 *
 * The inbound half of the ctrl+click gesture whose outbound half lives in CheckLevskiRoarClick.
 * Only the placement lives here: the hovered-cell resolution belongs to inv.cpp, which owns InvRect
 * and the active-tab helpers - and whose layout header cannot be included from this file without
 * its GridWidth/CellSize colliding with this window's own.
 *
 * Place BEFORE removing from the source container. A refusal must leave the item where it was.
 */
bool PlaceItemInLevskiGrid(const Item &item);

} // namespace devilution::oracool
