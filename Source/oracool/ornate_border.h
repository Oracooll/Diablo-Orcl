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
#include "engine/clx_sprite.hpp"
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
constexpr int PanelTitleTop = 28; // 28 since 2026-09-05: the user's new canvas's top bezel ends at y=24, and the title sits 3px below it ("move all canvas titles 3 pixels below edge bezel"); was 18
// 18, not 8 - user request (2026-08-18): "titles of UI windows to move 10px down". One number moves
// all six windows and the Abilities window's page arrows with them, since GetArrowRect centres on
// this same band. The limestone panel's own top framing is what the extra ten pixels clear.
constexpr int PanelTitleHeight = 38;

/**
 * @brief Top edge for a limestone side panel @p windowHeight tall, per the docking preference.
 *
 * A game-wide rule (user, 2026-08-27: "All limestone windows to be docked flush with the bottom of
 * the screen instead of the top"), so it lives in one place and the panels ask rather than each
 * choosing an anchor of its own - which is how they came to disagree: one sat a third of the way
 * down, one aligned with the mini-map, and one was pinned at y=0.
 *
 * BOTTOM or MIDDLE, and it is a setting rather than a decision because the right answer depends on
 * the screen. The panels are 720 tall, so at 16:9 they fill it and the question does not arise; on a
 * 3:2 laptop there is real space left over, and the same rule that reads as "sitting on the HUD"
 * there reads as "stranded at the bottom" (user, 2026-08-27: "i am now playing on surface laptop
 * with its 3:2 aspect ratio and the bottom dock doesnt sit nice with me").
 *
 * Bottom has a genuine argument behind it: the panel's grid is laid out to end exactly where the
 * mana orb begins, and the orbs are bottom-anchored, so bottom-docking is the only anchor that keeps
 * that alignment on any screen. Middle breaks it and looks better while doing so. That is a taste
 * call on a screen this code cannot see, which is why it is in the INI and not in this file.
 *
 * Clamped at zero either way, so a panel taller than the screen is cut off at the BOTTOM rather than
 * having its title band pushed off the top where the close button lives.
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
// Since the fifth HUD (2026-09-05) the orb CRADLES are taller than 96 and rise above this line -
// which the stash (17 saved rows) and both Abilities pages cannot give up. scrollrt.cpp clips the
// orbs to this line while a side panel is open. At the first scale (0.288, 109 tall) only the
// arches' tips crossed it; at the +10% the user asked for the same night (0.3168, 121 tall) the
// spheres' crowns cross it by 12px too, so with a panel open the orb beside it is cut flat across
// the top for as long as the panel is up. The user's call: a bigger HUD over a whole crown. This
// stays at the 96 the old orbs were, and is now the clip line as well as the content line.
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
 * @brief The legacy text box: the sunken gold-pinstriped field the original game's gold-amount box
 * is drawn in (user, 2026-09-04, with a screenshot of it: "this is the legacy textbox border i was
 * talking about").
 *
 * NOT the same thing as DrawOrnateBorder above, which is textbox_frame00's OUTER bevel - the wide
 * stone-and-gold frame of the big dialog. What the user pointed at is the frame's INNER ring, the
 * one that actually touches the black field: a three-pixel gold pinstripe, dark-bright-dark, lit
 * from above so the top run is a shade brighter than the bottom. Sampled from
 * golddrop_frame00 (the same ring, y 22..24 and 111..113), and every index is in the shared upper
 * half of the palette, so it is the same gold on every tileset:
 *
 *   204  (57,49,29)   the dark rails either side of the stripe
 *   194  (221,196,126) the bright stripe, top and left
 *   195  (204,183,117) the bright stripe, bottom and right
 *   223  (15,5,0)     the field
 *
 * The field is filled too, because the box IS the black inside it - a pinstripe alone on a stone
 * panel reads as a line, not a control. @p fill lets a caller light the field: a hovered button, a
 * pressed flash.
 *
 * Draws INSIDE @p rect, like DrawOrnateBorder, so a button keeps its hit rect and its face shrinks
 * by LegacyTextBoxBevel on every side.
 */
constexpr int LegacyTextBoxBevel = 3;
constexpr uint8_t LegacyTextBoxFill = 223;
constexpr uint8_t LegacyTextBoxHoverFill = 252; // (46,46,46) - the field lifts a shade under the cursor
void DrawLegacyTextBox(const Surface &out, Rectangle rect, uint8_t fill = LegacyTextBoxFill);

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
 * @brief The Abilities window's hover rectangle: DrawHoverOutline's gold at three pixels, casting a
 * two-pixel shadow (user, 2026-09-05: "make it 3px thick. and let it cast shadow 2px").
 *
 * Drawn twelve pixels OUTSIDE @p rect (the slot's own rect): past the six of carved bezel, the
 * three the slot's shadow is cast, and three of air - "the rectangle left border to be 3px to the
 * left from the cast shadow from the spell slots". Its shadow is the same ring two left and two
 * down as a half-transparent darkening, "like the skill slots", drawn first so the gold sits on it.
 *
 * VERTICALLY it clears less: nine, the bezel plus three. Rows and bands sit at a 74px pitch with
 * six of air between neighbouring frames, and the ring with its shadow must live in that air
 * (user, 2026-09-05: "it need to fit its upper border in the area between selected and upper and
 * lower spells. no overlapping with adjacent spells") - three of ring and two of shadow are five,
 * inside the six. Twelve there would have put the ring on the neighbours.
 *
 * @param clearanceX How far outside @p rect the ring sits, left and right. The slot default is
 * the twelve above.
 * @param clearanceY The same, above and below; nine for a slot. A row with no bezel and no shadow
 * - the waypoint list's - passes 0 for both and wears the ring on its rect.
 */
constexpr int HoverOutlineSlotClearanceX = 2 * 3 + 3 + 3;
constexpr int HoverOutlineSlotClearanceY = 2 * 3 + 3;
void DrawHoverOutlineHeavy(const Surface &out, Rectangle rect,
    int clearanceX = HoverOutlineSlotClearanceX, int clearanceY = HoverOutlineSlotClearanceY);

/**
 * @brief A drop shadow under a slot, cast at the angle the character sheet's text casts its own.
 *
 * Pilot (user, 2026-09-05: "cast same angle shadow as texts in hero stats window to all
 * skills/spells/auras slots in abilities windows as a pilot test"). The text shadow is vanilla's
 * own angle - left and down (text_render.cpp, UiFlags::Shadowed) - so the slot's is the same
 * angle at three pixels (the text's two vanished under the bezel): the slot's FULL footprint - @p rect
 * grown by @p bezelWidth on every side, since the grid bezel is painted outside the rect it is
 * given - shifted (-3, +3) and darkened, drawn BEFORE the slot so the slot covers all of it but
 * the strip down its left and along its bottom. One half-transparent pass ("reduce the shadow by
 * half", 2026-09-05); it was two.
 */
void DrawDropShadow(const Surface &out, Rectangle rect, int bezelWidth = 0, bool sunk = false);
/** @brief The HOVER shadow: DrawDropShadow twice as far (6px) and twice as dark. Under the slot, before the icon. */
void DrawHoverShadow(const Surface &out, Rectangle rect, int bezelWidth = 0, bool sunk = false);
/** @brief What a held button's shadow loses on every side (@p sunk above): the face has closed in on the ground. */
constexpr int SunkShadowInset = 1;

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
/**
 * @brief As above, but the panel hangs off @p avoid's edge - left of it, or right when the left has
 * no room - and never overlaps it (user, 2026-09-05: "to not overlap abilities window. to be
 * adjacent to it - 8px apart"). @p anchor still sets the vertical placement.
 */
void DrawHoverPanel(const Surface &out, string_view title, string_view text, Rectangle anchor, Rectangle avoid);
/** @brief Whether @p line is one of the block builders' headings ("Current Skill Level: 3", "Next Level"), drawn gold. */
bool IsHoverHeadingLine(string_view line);

/**
 * @brief Draws @p sprite scaled to fill @p target, keeping its aspect ratio and centred in it.
 *
 * A FRACTIONAL scaler (user, 2026-09-22). The one this joins - DrawSpriteScaled in levski_roar.cpp -
 * takes an integer, so a 28px item icon can be 28 or 56 and nothing between; asked to fill four
 * fifths of a 42px cell it could only overshoot or undershoot by a third. This takes the box instead
 * of a factor and works out the ratio itself.
 *
 * Nearest-neighbour, and driven from the DESTINATION rather than the source: a source-driven loop at
 * a fractional ratio leaves unwritten pixels wherever two destination pixels fall to one source one,
 * which reads as holes punched through the icon. Every destination pixel asking which source pixel
 * it came from cannot leave a gap.
 *
 * Index 0 is transparent, as everywhere else in this engine. @p trn remaps the indices when given,
 * for the greyed-out draw.
 */
void DrawSpriteToFit(const Surface &out, Rectangle target, ClxSprite sprite, const uint8_t *trn = nullptr);

} // namespace devilution::oracool
