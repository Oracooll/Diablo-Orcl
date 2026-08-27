/**
 * @file oracool/ornate_border.h
 *
 * Oracool: user request - "there is a textbox_frame00 file in the game. looks nice. let's try using
 * it as a border for the minimap and the log."
 *
 * The art itself is a fixed 591x303 CEL, so it cannot frame a 306x175 mini-map or a log window
 * whose height depends on how many entries fit. What it actually IS, though, is a plain 3px bevel,
 * and that reproduces at any size: this module draws the same bevel procedurally, sampled pixel for
 * pixel from the original.
 *
 * Every index it uses comes from the shared upper half of the palette (128-255), which is identical
 * across town and all four dungeon tilesets - so the frame is the same colour everywhere, without
 * the level-specific recolouring the lower half would bring.
 */
#pragma once

#include <cstdint>

#include "DiabloUI/ui_flags.hpp"
#include "engine/palette.h"
#include "engine/rectangle.hpp"
#include "engine/surface.hpp"
#include "utils/stdcompat/string_view.hpp"

namespace devilution::oracool {

/** @brief Thickness of the bevel, in pixels. Content should inset by at least this much. */
constexpr int OrnateBorderWidth = 3;

/**
 * @brief Where a side panel's title sits, and how tall its band is. Shared by all five windows.
 *
 * User request (2026-08-16). The band is exactly one FontSize30 line so the string's VerticalCenter
 * has no slack to drift in - the title's top edge IS PanelTitleTop, in every window, which is what
 * makes them line up when two are open side by side.
 *
 * Lives here rather than in any one panel's header because inventory, stash, character, quests and
 * the waypoint list all need the same two numbers, and they were already sharing this module's
 * bevel and separator.
 */
constexpr int PanelTitleTop = 18;
// 18, not 8 - user request (2026-08-18): "titles of UI windows to move 10px down". One number moves
// all six windows and the Abilities window's page arrows with them, since GetArrowRect centres on
// this same band. The limestone panel's own top framing is what the extra ten pixels clear.
constexpr int PanelTitleHeight = 38;

/**
 * @brief Top edge for a limestone window @p windowHeight tall, docked flush to the screen's bottom.
 *
 * A game-wide rule (user, 2026-08-27: "All limestone windows to be docked flush with the bottom of
 * the screen instead of the top"), so it lives in one place and the windows ask rather than each
 * choosing an anchor of its own - which is how they came to disagree: one sat a third of the way
 * down, one aligned with the mini-map, and one was pinned at y=0.
 *
 * Bottom is the right edge to share because it is the one the HUD already owns. The side panels end
 * on it, the belt and orbs sit against it, and a floating window that lines up with them reads as
 * part of the same furniture instead of hovering over the world at its own height.
 *
 * Clamped at zero, so a window taller than the screen is cut off at the BOTTOM rather than having
 * its title band pushed off the top where the close button lives.
 */
int BottomDockedTop(int windowHeight);

/**
 * @brief Screen y below which an orb draws over a side panel. Content must end above this.
 *
 * NOT the panel's own height minus a margin, which is what the character sheet and quest log used
 * and why their last rows rendered underneath the health orb (user, 2026-08-16). The orbs are
 * anchored to the screen's bottom corners and sit OVER the panels, so a 720-tall window's usable
 * area stops where the orb starts, not where the window does.
 *
 * 720 - HealthOrbScreenSize.height. Hardcoded rather than derived because a static_assert cannot
 * call GetHealthOrbRect(); safe because every resolution this project targets is 720 tall. The
 * stash and the inventory grid already measured against this line - it just had no shared name.
 */
constexpr int SidePanelContentBottom = 720 - 96;

/**
 * @brief Frames @p rect from OUTSIDE it, so the bevel costs the content nothing.
 *
 * DrawOrnateBorder draws its three rings inside the rect it is given, which is right for a window
 * (the frame IS its edge) and wrong for a cell grid: there, the outer row and column each lost 3 of
 * their 28 pixels to it. User request (2026-08-16) - "I don't want it to take space from the boxes.
 * There is enough space outside of them to use."
 *
 * The caller must leave OrnateBorderWidth of clearance on every side; both item grids do.
 */
void DrawOrnateBorderOutside(const Surface &out, Rectangle rect);

/**
 * @brief The theme's gold for a thin drawn edge - a silhouette outline, a tooltip's border.
 *
 * One constant because the eye compares them: two edges of slightly different gold on screen at the
 * same time read as a mistake rather than as two elements. Both started life as separate literals
 * at +2, both were then asked to be dimmed, and keeping them in step by editing two numbers is a
 * promise that would be broken the first time only one of them was tuned.
 *
 * PAL16 ramps run LIGHT to DARK as the offset grows (see engine/palette.h), so +9 is a deep gold,
 * one step off the unique-item backing's own +10. An edge does not need to be bright to read - it
 * is drawn opaque against backgrounds that are half-transparent, and being solid is what separates
 * it.
 */
constexpr uint8_t ThemeEdgeColor = PAL16_YELLOW + 9;

/**
 * @brief The theme's 1px rule between item-grid cells - dark grey, in both the stash and inventory.
 *
 * One constant for the same reason as ThemeEdgeColor: the two grids are on screen together whenever
 * the stash is open, so a difference between them would read as a mistake.
 *
 * Grey rather than the gold bevel these grids used to divide their cells with. At 3px of gold per
 * boundary, a large grid becomes a gold mesh with items sitting inside it; one dark pixel separates
 * the cells and lets the items carry the colour. PAL16 ramps run light to dark as the offset grows
 * (engine/palette.h), so +11 is deep grey.
 */
constexpr uint8_t ThemeGridLineColor = PAL16_GRAY + 11;

/**
 * @brief Half the bevel's width, for centring a rule ON a boundary rather than starting it there.
 *
 * A rule drawn AT a cell boundary puts its whole width inside the cell to the right, which shifts
 * that cell's visible interior and leaves anything centred in its rect looking off by half the
 * rule. Subtract this from the boundary coordinate to make rect centre and visual centre agree.
 */
constexpr int OrnateBorderWidthHalf = OrnateBorderWidth / 2;

/**
 * @brief Draws textbox_frame00's bevel around (and just inside) @p rect.
 *
 * Replaces the 1px dashed placeholder the mini-map and event log shared. Clipped, so a rect that
 * runs off the surface is safe.
 */
void DrawOrnateBorder(const Surface &out, Rectangle rect);

/**
 * @brief Draws a horizontal rule OrnateBorderWidth tall, in the bevel's own colours.
 *
 * For dividing a panel into sections - a title band from a list, say - so the divider belongs to
 * the same frame instead of looking like a line drawn on top of it.
 */
void DrawOrnateSeparator(const Surface &out, Point from, int width);

/** @brief DrawOrnateSeparator's vertical twin - a rule OrnateBorderWidth wide, @p height tall. */
void DrawOrnateSeparatorVertical(const Surface &out, Point from, int height);

/**
 * @brief Draws @p text with a hard black outline on all four sides - the panel-title treatment.
 *
 * The text renderer has no outline or shadow flag; control.cpp's DrawFlaskValues sets the
 * precedent with a single black draw offset up-left. A title over a half-transparent fill needs it
 * on all sides, since the fill takes the colour of whatever is behind the window.
 *
 * @p style is passed through verbatim, so the caller owns the colour, size and alignment.
 */
void DrawOutlinedString(const Surface &out, string_view text, Rectangle area, UiFlags style);

/**
 * @brief The theme's window fill: a half-transparent darkening of whatever is behind @p rect.
 *
 * @p passes controls how opaque it lands. The renderer is 8-bit palettized with no alpha, so
 * translucency means blending through paletteTransparencyLookup, and that table only gives one
 * strength - half. More opacity therefore means blending repeatedly: one pass is 1/2 toward black,
 * two is 3/4, three is 7/8. Used to sit item slots and the grid more solidly than the panel they
 * are on, so they read as recesses cut into it.
 */
void DrawThemedFill(const Surface &out, Rectangle rect, int passes = 1);

/**
 * @brief A one-pixel gold rectangle marking the row under the cursor.
 *
 * Oracool: user request (2026-08-15) - "a subtle golden rectangle around the item I am hovering
 * over. Subtle gold outline, like the outline of silhouettes." Deliberately an OUTLINE and not a
 * fill: these rows sit on a half-transparent panel over the dungeon, and a fill would either wash
 * the row out or fight the icon's own art.
 *
 * One pixel, mid-ramp gold, no corners softened. Drawn on the row's own rect, so a caller that
 * already knows which row the cursor is on needs nothing else.
 */
void DrawHoverOutline(const Surface &out, Rectangle rect);

/**
 * @brief The same one-pixel rectangle in a caller-chosen palette index.
 *
 * DrawHoverOutline is this with the frame's gold. Split out for the Abilities window's assignment
 * rings, which say which mouse button an ability is readied on and so need two more colours.
 * Clip-safe: it draws through the engine's line primitives, so a row half-scrolled out of a
 * subregion is cut rather than written past.
 */
void DrawColoredOutline(const Surface &out, Rectangle rect, uint8_t color);

/**
 * @brief A @p weight-pixel rectangle whose left and top edges are one colour and right and bottom
 * another.
 *
 * Oracool: user request (2026-08-15) - the Abilities window marks an ability readied on the left
 * mouse button in red and on the right in yellow, both on the icon's own edge and at the same
 * weight. One rect cannot be two colours, so an ability readied on BOTH buttons splits it: "RED take
 * left and top edges, YELLOW take right and bottom edges."
 *
 * Pass the same colour twice for a plain single-colour ring - which is what a one-button assignment
 * draws, so the marker is the same square either way.
 */
void DrawSplitOutline(const Surface &out, Rectangle rect, uint8_t leftTopColor, uint8_t rightBottomColor, int weight);

/**
 * @brief A floating panel of wrapped text, placed beside @p anchor and kept on screen.
 *
 * The hover description window. Sizes itself to the wrapped text rather than to a fixed box, so a
 * one-line spell and a three-line one both look deliberate, and flips to the other side of the
 * anchor rather than running off the edge.
 */
void DrawHoverPanel(const Surface &out, string_view title, string_view text, Rectangle anchor);

} // namespace devilution::oracool
