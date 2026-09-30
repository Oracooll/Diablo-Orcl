#include "qol/stash.h"

#include <cstdint>
#include <limits>
#include <utility>

#include <fmt/format.h>

#include "DiabloUI/text_input.hpp"
#include "control.h"
#include "controls/plrctrls.h"
#include "diablo.h" // CloseOtherShopSurfaces - one shop surface at a time
#include "error.h" // InitDiabloMsg - a sort that cannot fit says so
#include "cursor.h"
#include "engine/clx_sprite.hpp"
#include "engine/load_clx.hpp"
#include "engine/points_in_rectangle_range.hpp"
#include "engine/rectangle.hpp"
#include "engine/palette.h"
#include "engine/render/clx_render.hpp"
#include "engine/render/primitive_render.hpp"
#include "engine/render/text_render.hpp"
#include "engine/size.hpp"
#include "hwcursor.hpp"
#include "minitext.h"
#include "oracool/auto_save.h"
#include "oracool/hud_art.h"
#include "oracool/gems.h"
#include "oracool/levski_roar.h" // the Cube keeps the slot when it refuses to close
#include "oracool/workshop.h" // and so does a bench
#include "oracool/inventory_layout.h" // CellPx / GridOrigin - the grid this one must match
#include "oracool/item_sets.h"        // which set a piece belongs to, for the set page's grouping
#include "oracool/ornate_border.h"
#include "oracool/salvage.h"
#include "oracool/socket_overlay.h"
#include "oracool/runewords.h"
#include "oracool/event_log.h"
#include "loadsave.h" // StashFileRefused - an unreadable stash stays shut
#include "oracool/named_encounters.h"
#include "oracool/rift.h"
#include "oracool/signets.h"
#include "oracool/skill_sounds.h" // PlayUiEventSound - the map's unsealing
#include "oracool/ui_sound.h"
#include "oracool/window_close.h" // the withdraw box's red X (2026-09-24)
#include "inv.h" // AddShopServiceFeeLine
#include "stores.h"
#include "utils/format_int.hpp"
#include "utils/language.h"
#include "utils/stdcompat/algorithm.hpp"
#include "utils/stdcompat/optional.hpp"
#include "utils/str_cat.hpp"
#include "utils/utf8.hpp"

namespace devilution {

bool IsStashOpen;
StashStruct Stash;
bool IsWithdrawGoldOpen;

namespace {

constexpr unsigned CountStashPages = 100;
constexpr unsigned LastStashPage = CountStashPages - 1;

char GoldWithdrawText[21];
TextInputCursorState GoldWithdrawCursor;
std::optional<NumberInputState> GoldWithdrawInputState;

// Oracool V1: the shared 340x720 window, now with everything placed from the canvas's painted grid
// frame rather than from a title band that no longer exists (user, 2026-09-21).
//   0..138     stone - the title band is EMPTY since "remove the title of stash"
//   139..154   the one control row: << <  Page N / M  > >>, four pixels above the frame
//   159..628   the painted grid frame, with the 10x16 grid inside it at 170..618
//   633..660   the gold pile (ui\shop_gold_icon.png), Griswold's own
//   660..676   the gold total, bare, under the pile
//   676..692   SORT, under the total (user: "move sort button under the gold counter")
constexpr Size StashPanelSize { 340, 720 };
constexpr int StashMargin = 24;
constexpr int StashLabelHeight = 50;
// The title band is oracool::PanelTitleTop / PanelTitleHeight - shared by all five side panels so
// two open side by side line up. See oracool/ornate_border.h.
constexpr int StashContentTop = StashMargin + StashLabelHeight + oracool::OrnateBorderWidth + StashMargin;

/**
 * @brief Cell pitch, and where the grid starts. Declared HERE, above the controls, because the
 * controls are placed from the grid (StashColumnX) rather than the other way round.
 *
 * The same 28 the inventory grid uses (oracool::CellPx) - user request, 2026-08-16. This was
 * INV_SLOT_SIZE_PX + 1: a 28px slot plus a dedicated pixel for its dividing rule, so no slot lost
 * any of its 28. The comment claimed that matched the inventory grid; it did not. The inventory
 * draws the same 1px ThemeGridLineColor rule at `c * CellPx - 1` and simply lets the item sprite
 * overdraw it, which costs the boundary pixel of one cell and keeps the pitch at 28.
 *
 * Matching the pitch is what makes the two grids line up: StashGridLeft lands on 30, the same x as
 * the inventory's GridOrigin, and a 2x3 item spans the same 56x84 in both - which is also the span
 * the item backing paints, so backings sit flush in the stash instead of 1px inside the cell.
 */
constexpr int StashCellPx = INV_SLOT_SIZE_PX;
constexpr int StashGridWidth = StashGridColumns * StashCellPx;
/** @brief Centred across the panel. */
constexpr int StashGridLeft = (StashPanelSize.width - StashGridWidth) / 2;

/**
 * @brief The x of stash grid column @p column, counting from 1 the way the user does.
 *
 * The controls above the grid are placed by COLUMN rather than by pixel (user request, 2026-08-16:
 * "move SORT above grid columns 8-9", "Move GOLD above grid column 2"). Saying it in columns is what
 * keeps each control aligned with the cells beneath it; a pixel literal drifts the moment the grid's
 * width or its centring changes.
 *
 * Declared here, above the control rects, which is why StashCellPx and StashGridLeft moved up with
 * it - the controls are positioned FROM the grid, so the grid's geometry has to be known first.
 */
constexpr int StashColumnX(int column)
{
	return StashGridLeft + (column - 1) * StashCellPx;
}

/**
 * @brief The face every control above the grid now uses, and the height one line of it needs.
 *
 * User, 2026-09-03: "increase size of sort, gold and page indicator font in stash. also increase
 * size of browsing buttons." These four controls sit on open stone with the grid below them, and at
 * GameFont12 they read as captions rather than as the window's controls.
 *
 * 24 rather than the 22px FontSizeDialog, and that is not a taste call: the 22px face is a separate
 * asset whose ink does not sit in the range the colour .trn tables remap, so EVERY colour flag is a
 * no-op on it - the waypoint list found that out from a screenshot on 2026-08-30. SORT goes white
 * when clicked and the gold total is whitegold, so a face that cannot take a colour is not a
 * candidate here.
 */
constexpr int StashControlLineHeight = 26; // FontSize24's line height

/**
 * @brief One gap, used everywhere in the stack above the grid.
 *
 * User, 2026-09-03: "leave an equal gap between stash title, page indicator, buttons, sort/gold
 * row." So it is a single constant that every band is placed by, rather than four numbers that
 * happen to agree today: the request is about the RHYTHM, and a rhythm written as separate literals
 * is one edit away from not being one.
 *
 * Five, and the arithmetic is fixed at both ends. The title band ends at PanelTitleTop +
 * PanelTitleHeight (56) and the grid's carved frame begins six pixels above StashGridTop (155), so
 * the stack has 99 pixels for three 26px bands - 21 to spend. Three gaps of five leaves six above
 * the grid, which reads as the section break it is rather than as a fourth gap that got the
 * remainder.
 */
constexpr int StashControlGap = 5;

/**
 * @brief The three stacked bands above the grid, each placed from the one above it.
 *
 * Measured from the TITLE now rather than from StashContentTop. StashContentTop is margin + label +
 * border + margin, an expression whose value happened to be the old first row - it says nothing
 * about where the title actually ends, and the equal-gap rule is a statement about exactly that.
 */
/**
 * @brief The painted canvas's grid frame, panel-relative: its ornate band runs y 159..628.
 *
 * Measured on ui\stash_canvas.png (and the same on the other framed canvases). Everything the user
 * placed "4px above the grid frame" or under it is derived from this rather than from a literal, so
 * a recut canvas that moves the band moves the furniture with it.
 */
constexpr int StashFrameTop = 159;
constexpr int StashFrameBottom = 628;
/** @brief The air the user asked for between the controls and the frame, above and below. */
constexpr int StashFrameClearance = 4;

/**
 * The page counter, the four nav buttons and nothing else, on ONE line four pixels above the frame
 * (user, 2026-09-21: "move the page navigation buttons and page counter to 4px above the grid
 * frame"). It was three stacked bands under the title; the title keeps its place and the rest of
 * that stack is gone.
 *
 * The row's last pixel is StashFrameTop - StashFrameClearance - 1 = 154, so a 16px row starts at 139.
 */
constexpr int StashControlRowHeight = 16; // "if the font is 16px high then make the nav buttons also 16px high"
constexpr int StashPageRowY = StashFrameTop - StashFrameClearance - StashControlRowHeight;
constexpr int StashPageRowHeight = StashControlRowHeight;
constexpr int StashPageLabelY = StashPageRowY; // the counter shares the line with the buttons now
// 46x32, not 40x26: the legacy bevel draws INSIDE its rect (user, 2026-09-04: "apply legacy text box
// borders for stash nav buttons"), so the button grew by the bevel on every side and the face the
// glyph sits on is the same 40x26 it was. The 3px it overhangs the row either way is inside the
// 5px StashControlGap.
// 28x18 faces since 2026-09-05 (user: "reduce size of NAV buttons"), with the 12px glyphs; were 40x26 under the 24px face.
// SIXTEEN tall, the width unchanged (user, 2026-09-21: "if the font is 16px high then make the nav
// buttons also 16px high. keep their width"). The legacy box's bevel draws INSIDE the rect, so the
// face this leaves is 16 - 2*3 = 10; the arrow glyphs are short and sit inside it, and matching the
// font's own height is what was asked for.
constexpr Size ButtonSize { 28 + 2 * oracool::LegacyTextBoxBevel, StashControlRowHeight };
/** The buttons fill their row now, so this is the row's own y. */
constexpr int StashButtonY = StashPageRowY + (StashPageRowHeight - ButtonSize.height) / 2;

/**
 * The gold and SORT sit UNDER the grid now, four pixels below the painted frame's foot.
 *
 * The gold is Griswold's pair - a pile and a number beneath it, no "GOLD:" label (user, 2026-09-21:
 * "remove the current gold counter and place a new one, the same as in Griswold stores. Icon +
 * counter under the grid"), at his own x so the two windows read alike.
 */
constexpr int StashUnderGridY = StashFrameBottom + 1 + StashFrameClearance;
constexpr int StashGoldRowHeight = StashControlRowHeight;
/** @brief The same pile Griswold's page draws, so the two windows count gold in one visual language. */
constexpr const char *StashGoldIconAsset = "ui\\shop_gold_icon.png";
constexpr Point StashGoldIconAt { 27, StashUnderGridY };
constexpr int StashGoldIconHeight = 28; // ui\shop_gold_icon.png, the same file Griswold draws
constexpr int StashGoldCountY = StashUnderGridY + StashGoldIconHeight - 1;

/**
 * @brief Contains mappings for the four page-navigation buttons.
 *
 * Index 2 used to be a fifth button here (originally Withdraw Gold, then Sort). It is drawn as a
 * text button now, like the character sheet's RESET and the inventory's sort tab, so it no longer
 * needs a slot in this table - but the ORDER of the remaining four still matters, because
 * StashButtonPressed indexes both this and the nav-button art.
 */
/**
 * @brief The four nav buttons, placed from the page counter's REAL drawn width.
 *
 * A function rather than the constexpr table this replaces, because the user asked for the buttons
 * to sit "6px horizontally away" from the counter and the counter is centred text whose width is the
 * font's business, not a literal's: "Page 1 / 2" and "Page 100 / 100" are not the same size. Measured
 * once per call with GetLineWidth against the string actually being drawn, so the gap is six pixels
 * whatever the page number does.
 *
 * Index order is unchanged and still matters - StashButtonPressed indexes this and the labels.
 */
Rectangle StashNavButtonRectAt(int index);

constexpr int StashNavButtonCount = 4;
/** @brief Drawn as text, in the same order StashNavButtonRectAt uses. */
constexpr const char *StashNavLabel[StashNavButtonCount] = { "<<", "<", ">", ">>" };

/**
 * @brief The face the row above the grid uses.
 *
 * The user asked for "font16" (2026-09-21) and THIS ENGINE HAS NO 16px FACE: the tables are 8, 9,
 * 10, 11, 12, 22 (FontSizeDialog), 24, 30, 42 and 46. Twelve is the nearest real one below the 24
 * this replaces - 22 is nearer in number and unusable here, because its ink does not sit in the
 * range the colour .trn tables remap, so the ColorGold this string is drawn in would be a no-op on
 * it. That was learned from a waypoint-list screenshot on 2026-08-30 and is why 24 was chosen over
 * 22 in the first place.
 */
constexpr UiFlags StashControlFont = UiFlags::FontSize12;
constexpr GameFontTables StashControlFontTable = GameFont12;

/** @brief The counter's text. One source for the string, the rect it needs and the buttons beside it. */
std::string StashPageLabelText()
{
	return fmt::format(fmt::runtime(_("Page {:d} / {:d}")), Stash.GetPage() + 1, CountStashPages);
}

/** @brief The counter's own rect: its MEASURED width, centred over the grid (user, 2026-09-21). */
Rectangle StashPageLabelRectNow()
{
	const int width = GetLineWidth(StashPageLabelText(), StashControlFontTable);
	const int centre = StashGridLeft + StashGridWidth / 2;
	return { { centre - width / 2, StashPageLabelY }, { width, StashPageRowHeight } };
}

Rectangle StashNavButtonRectAt(int index)
{
	// Six from the counter (user, 2026-09-21: "place the nav buttons close to the page counter - 6px
	// horizontally away"); four between the members of a pair, which is the gap they already had.
	//
	// Measured rather than tabulated, because "6px from the counter" cannot be a constant: "Page 1 /
	// 2" and "Page 100 / 100" are different widths, and a fixed table would honour the gap on one of
	// them and nothing else.
	constexpr int CounterGap = 6;
	constexpr int PairGap = 4;
	const Rectangle label = StashPageLabelRectNow();
	const int w = ButtonSize.width;
	const int leftOfLabel = label.position.x - CounterGap;
	const int rightOfLabel = label.position.x + label.size.width + CounterGap;
	switch (index) {
	case 0: return { { leftOfLabel - 2 * w - PairGap, StashButtonY }, ButtonSize }; // "<<"
	case 1: return { { leftOfLabel - w, StashButtonY }, ButtonSize };               // "<"
	case 2: return { { rightOfLabel, StashButtonY }, ButtonSize };                  // ">"
	default: return { { rightOfLabel + w + PairGap, StashButtonY }, ButtonSize };   // ">>"
	}
}

/** @brief Shared with the inventory grid - see oracool::ThemeGridLineColor for the reasoning. */
constexpr uint8_t StashGridLineColor = oracool::ThemeGridLineColor;



/**
 * @brief Oracool: user request - the gold total's on-screen area is the click target for
 * withdrawing gold, which is what freed the old button slot up to become Sort.
 *
 * Columns 2-7 (user request, 2026-08-16: "Move GOLD above grid column 2 - move it one column to the
 * right"). Six columns wide rather than the old 180px: that is exactly the space between it and
 * SORT, so the two can no longer collide however long the gold total gets.
 */
// SEVEN columns since 2026-09-03, not six: the readout doubled in font size that day, and "GOLD:"
// plus eight digits at FontSize24 no longer fits in 168px. It still ENDS on column 10's right edge -
// that is the part the user asked for and the part that must not move - so the extra column is taken
// from the empty middle of the row, and SORT widens to meet it. The assert below is what says the
// two still cannot collide.
// Under the grid, beneath the pile, left-aligned with it - Griswold's arrangement exactly. The rect
// is still the click target for withdrawing gold, so it covers the number rather than the icon.
constexpr Rectangle GoldDisplayRect { { 25, StashGoldCountY },
	{ 140, StashGoldRowHeight } };
/**
 * @brief The pile itself, a button too (user, 2026-09-23 dev note: "in stash make gold icon clickable
 * and sinkable and make it initiat[e] the draw gold window"). Its own square rather than a taller
 * GoldDisplayRect, so the number keeps its target and the pile adds one; either press sinks the pile.
 */
constexpr Rectangle GoldIconRect { StashGoldIconAt, { StashGoldIconHeight, StashGoldIconHeight } };
/** @brief How far a pressed pile sinks - the game's press, 2px down-left (feedback: button press). */
constexpr Displacement GoldIconPressSink { -2, 2 };
bool GoldDisplayPressed = false;
/**
 * @brief How far the withdraw box moves from vanilla's spot, so its FOOT is the grid frame's foot
 * (user, 2026-09-23 dev note: "render the draw gold window flush with bottom border of grid").
 * Vanilla draws the box's bottom row at y 178; the painted frame's band ends at StashFrameBottom.
 * Everything the box carries - the question, the typed amount, the IME rect - moves by the same step.
 */
constexpr Displacement GoldWithdrawDrop { 0, StashFrameBottom - 178 };

bool GoldButtonContains(Point mousePosition)
{
	for (const Rectangle &local : { GoldDisplayRect, GoldIconRect }) {
		if (Rectangle { GetPanelPosition(UiPanels::Stash, local.position), local.size }.contains(mousePosition))
			return true;
	}
	return false;
}

/**
 * @brief Drawn as a word rather than art, matching RESET on the character sheet.
 *
 * SORT sits flush with the LEFT edge of column 1 and the gold total flush with the RIGHT edge of
 * column 10 (user request, 2026-08-20: "to keep consistancy move sort button in stash flush with
 * left border of grid column 1 and move gold counter flush with right border of grid column 10").
 *
 * The consistency is with the INVENTORY's own header, which already reads SORT-left / gold-right.
 * They were the other way round here, which is why the two panels never looked like siblings even
 * though both rows carried the same two controls. This swaps the sides rather than nudging pixels:
 * StashColumnX(11) is the right edge of column 10, so the gold rect ENDS there by construction and
 * cannot drift if the cell size or grid origin changes.
 */
// Three columns since 2026-09-03, for the same reason the gold rect took a seventh: at FontSize24
// the word fills 2 columns to the pixel, and a click target that ends exactly where its own glyphs
// do is one the player misses from either side.
// UNDER the gold counter now (user, 2026-09-21: "move sort button under the gold counter"), not
// beside it at the other end of the row. It was flush right, the mirror of the inventory's
// SORT-left/gold-right header; the user wants the two stacked instead, so SORT takes the gold's own
// x and the line below it and the pair reads as one block under the grid's left corner.
//
// Left-aligned to match, and the same three columns wide - the width is the click target, and a
// target that ends exactly where its own glyphs do is one the player misses from either side.
constexpr int StashSortRowY = StashGoldCountY + StashGoldRowHeight;
constexpr Rectangle StashSortButtonRect { { GoldDisplayRect.position.x, StashSortRowY },
	{ 3 * StashCellPx, StashGoldRowHeight } };

// Stacked, so the test that mattered is the vertical one: the two rects must not overlap, and the
// pair must still finish inside the panel rather than running off its foot.
static_assert(GoldDisplayRect.position.y + GoldDisplayRect.size.height <= StashSortButtonRect.position.y,
    "SORT now overlaps the gold readout it sits under");
static_assert(StashSortButtonRect.position.y + StashSortButtonRect.size.height <= StashPanelSize.height,
    "SORT now runs off the bottom of the stash panel");
static_assert(StashSortButtonRect.position.x + StashSortButtonRect.size.width <= StashColumnX(11),
    "SORT runs past the last grid column");

constexpr Size StashGridSize { StashGridColumns, StashGridRows };
constexpr PointsInRectangleRange<int> StashGridRange { { { 0, 0 }, StashGridSize } };

// StashCellPx, StashGridWidth and StashGridLeft moved up above the control rects on 2026-08-16 -
// SORT, the gold readout and the page label are now placed by grid COLUMN (StashColumnX), so the
// grid's horizontal geometry has to be known before them. The vertical half stays here, where it
// still reads in order after the rows it measures from.
// Pinned so the stash grid's LAST ROW ends on the same line as the inventory grid's (user request,
// 2026-08-18). Derived from the inventory rather than written as a number, so the two cannot drift:
// whatever moves oracool::GridBottom moves this with it. The gold row above keeps its own spacing,
// which simply has more air under it now.
constexpr int StashGridTop = oracool::GridBottom - StashGridRows * StashCellPx;
// Against the grid's FRAME, not its first cell (2026-09-03). The carved bezel is drawn OUTSIDE the
// cells and reaches GridFrameWidth above StashGridTop, so the old form passed while the gold row sat
// on the stone frame above the grid - which is exactly where the previous version's row ended up,
// flush at 161, with the bezel occupying its last six pixels.
// The control row above the grid must clear the painted frame by the clearance the user asked for.
static_assert(StashPageRowY + StashPageRowHeight + StashFrameClearance <= StashFrameTop,
    "the stash's control row no longer clears the painted grid frame above it");
constexpr int StashGridBottom = StashGridTop + StashGridRows * StashCellPx;

/**
 * @brief The health orb's top edge - what the grid's height is actually limited by.
 *
 * Not the 660 that once stood here, which was the central HUD PLATE's top. The plate is not the
 * thing the stash meets: the health orb sits to the plate's left (oracool::GetHealthOrbRect), is
 * 96px tall against the plate's shorter body, and therefore reaches higher - and the stash panel is
 * at the screen's top-left, so the orb is its actual neighbour. The assert passed while the user was
 * looking at a row drawn underneath a sphere.
 *
 * Now the shared oracool::SidePanelContentBottom: the character sheet and quest log turned out to
 * have exactly the same neighbour and exactly the same bug (user, 2026-08-16), so the line they all
 * measure against belongs in one place rather than three.
 */
constexpr int StashHealthOrbTop = oracool::SidePanelContentBottom;

// -1 because StashGridBottom is the row AFTER the last one: the grid's final pixel is
// StashGridBottom - 1, and it is allowed to sit on the orb's first row. That single shared line is
// the grid's bottom rule meeting the very top of the sphere, which is a few pixels wide there.
// The FRAME is what has to clear the orb, not the cells - it is drawn outside the grid, so it
// reaches GridFrameWidth past the last row. That is the carved bezel's 6 since the 2026-08-18 MPQ
// sweep, twice the procedural bevel it replaced; the stash absorbed the extra three pixels without
// losing a row, which the second assert below is what proves.
static_assert(StashGridBottom - 1 + oracool::GridFrameWidth <= StashHealthOrbTop,
    "Stash grid or its frame now runs under the health orb - lower StashGridRows");
static_assert(StashGridBottom - 1 + StashCellPx > StashHealthOrbTop, "Another stash row would still fit - raise StashGridRows");
static_assert(StashGridLeft >= StashMargin, "Stash grid is wider than the panel's margins allow");

// The two grids must agree, and the only reason they did not for so long is that nothing said so
// out loud - the pitch comment claimed a match that was never checked (user, 2026-08-16: "why is
// stash 29x29px grid instead of 28x28px"). Both windows are 340 wide with a 10-column grid, so
// matching the pitch necessarily matches the left edge too; asserting both says which is the cause.
static_assert(StashCellPx == oracool::CellPx,
    "Stash and inventory grids no longer share a cell pitch - items would span different widths in each");
static_assert(StashGridLeft == oracool::GridOrigin.x,
    "Stash and inventory grids no longer start at the same x - their columns would not line up");


/**
 * @param page The stash page index.
 * @param position Position to add the item to.
 * @param stashListIndex The item's StashList index
 * @param itemSize Size of item
 */
void AddItemToStashGrid(unsigned page, Point position, uint16_t stashListIndex, Size itemSize)
{
	for (Point point : PointsInRectangle(Rectangle { position, itemSize })) {
		Stash.stashGrids[page][point.x][point.y] = stashListIndex + 1;
	}
}

/**
 * @brief Oracool: category ordering for SortStash - Weapons, Armor, Helms, Shields, worn
 * accessories, Jewelry, then everything else (potions, scrolls, books, oils, misc items, etc).
 */
int StashSortCategoryRank(const Item &item)
{
	switch (item._itype) {
	case ItemType::Sword:
	case ItemType::Axe:
	case ItemType::Bow:
	case ItemType::Mace:
	case ItemType::Staff:
		return 0; // Weapons
	case ItemType::LightArmor:
	case ItemType::MediumArmor:
	case ItemType::HeavyArmor:
		return 1; // Armor
	case ItemType::Helm:
		return 2; // Helms
	case ItemType::Shield:
		return 3; // Shields
	// Oracool bug fix (2026-08-16): user report - the new items "dont get sorted properly". The six
	// worn types postdate this switch, so every pauldron and greave fell through to Others and
	// sorted in among the potions and scrolls. They are armor the player wears; they sort with the
	// other worn gear, in their own band after shields.
	case ItemType::Shoulders:
	case ItemType::Bracers:
	case ItemType::Gloves:
	case ItemType::Belt:
	case ItemType::Legs:
	case ItemType::Boots:
		return 4; // Worn accessories (the six Oracool slots)
	case ItemType::Ring:
	case ItemType::Amulet:
		return 5; // Jewelry
	default:
		// Books ahead of the rest of the miscellany (user, 2026-09-07: "books get weirdly placed in the
		// stash after SORT"): they were Others with everything else, sorted by value among 1x1 trinkets,
		// and the first-fit scan then dropped each book into whatever gap the trinkets before it had
		// left. A band of their own, and SortStash's size-first order within a band, keeps them together.
		if (item._iMiscId == IMISC_BOOK)
			return 6; // Books
		return 7; // Others
	}
}

/**
 * @brief Oracool: which PAGE an item sorts onto - one per quality tier (user, 2026-09-12: "when
 * sorting the stash sort different tiers of items (basic, magic, rare, etc...) in different tabs").
 *
 * The order is the ladder the player already reads in the item's own name colour, weakest first, so
 * the page number climbs with the quality: white, blue, yellow, green, gold, orange. Deliberately
 * the same classification `Item::getTextColor` makes rather than a second opinion on it - if the
 * name is green the piece is on the Set page, and there is no way for the two to disagree.
 *
 * A tier with nothing in it takes no page: pages are handed out as the tiers are placed, so a stash
 * holding only plain and rare gear uses pages 0 and 1, not 0 and 2. That matters because the
 * material and consumable pages are "the first empty page" and would otherwise sit past a gap.
 */
enum class StashSortTier {
	Plain = 0,
	Magic = 1,
	Rare = 2,
	Set = 3,
	Unique = 4,
	Primal = 5,
	/** An assembled runeword, whatever its base's quality (user, 2026-09-13: "put assembled runewords in separate stash tab"). */
	Runeword = 6,
	LAST = Runeword,
};

StashSortTier StashSortTierOf(const Item &item)
{
	// A finished runeword before any quality: its base is usually a plain or magic piece, and the
	// point of the page is that the words are found together, not among the bases they were made on.
	if (oracool::GetActiveRuneword(item) != nullptr)
		return StashSortTier::Runeword;
	// The fork's tier field first: it is the more specific answer, and Set and Primal exist ONLY
	// here. A set piece is marked ITEM_QUALITY_UNIQUE by MakeSetItem, so reading _iMagical first
	// would put every green item on the gold page - the same trap getTextColor documents.
	switch (item._iOracoolTier) {
	case OracoolItemTier::Rare:
		return StashSortTier::Rare;
	case OracoolItemTier::Set:
		return StashSortTier::Set;
	case OracoolItemTier::Primal:
		return StashSortTier::Primal;
	case OracoolItemTier::BuffedUnique:
		return StashSortTier::Unique;
	case OracoolItemTier::None:
		break;
	}
	switch (item._iMagical) {
	case ITEM_QUALITY_MAGIC:
		return StashSortTier::Magic;
	case ITEM_QUALITY_UNIQUE:
		return StashSortTier::Unique; // a vanilla unique, which wears the same gold as a buffed one
	default:
		return StashSortTier::Plain;
	}
}

/**
 * @brief Oracool: a key that puts pieces of ONE set next to each other (user, 2026-09-12: "try
 * sorting set items of same set close to each other").
 *
 * `set index * 100 + piece index`, so a set's pieces are contiguous AND in the order the set data
 * declares them - helm, torso, and so on down the body - which is the order the set's own tooltip
 * lists them in. Both indices come from pointer arithmetic into the two static tables, because the
 * identity of a set piece is its position in those tables; there is no id field on Item to read.
 *
 * A piece whose icon matches no definition sorts last rather than being dropped. That should be
 * unreachable - a Set tier is only ever applied by MakeSetItem - but sorting is not the place to
 * find out, and a stray green item is better misplaced than lost.
 */
int StashSortSetKey(const Item &item)
{
	const oracool::SetItemDefinition *piece = oracool::FindSetItemByCursor(item._iCurs);
	if (piece == nullptr)
		return std::numeric_limits<int>::max();
	const oracool::ItemSetDefinition *set = oracool::FindItemSetOwning(piece->id);
	if (set == nullptr)
		return std::numeric_limits<int>::max();
	const int setIndex = static_cast<int>(set - oracool::ItemSets);
	const int pieceIndex = static_cast<int>(piece - oracool::ItemSetItems) - set->firstItem;
	return setIndex * 100 + pieceIndex;
}

std::optional<Point> FindTargetSlotUnderItemCursor(Point cursorPosition, Size itemSize)
{
	for (auto point : StashGridRange) {
		Rectangle cell {
			GetStashSlotCoord(point),
			StashCellPx // the grid's own 28px pitch; 29 gave each cell's first column and row to its neighbour (round 13)
		};

		if (cell.contains(cursorPosition)) {
			// When trying to paste into the stash we need to determine the top left cell of the nearest area that could fit the item, not the slot under the center/hot pixel.
			if (itemSize.height <= 1 && itemSize.width <= 1) {
				// top left cell of a 1x1 item is the same cell as the hot pixel, no work to do
				return point;
			}
			// Otherwise work out how far the central cell is from the top-left cell
			Displacement hotPixelCellOffset = { (itemSize.width - 1) / 2, (itemSize.height - 1) / 2 };
			// For even dimension items we need to work out if the cursor is in the left/right (or top/bottom) half of the central cell and adjust the offset so the item lands in the area most covered by the cursor.
			if (itemSize.width % 2 == 0 && cell.contains(cursorPosition + Displacement { INV_SLOT_HALF_SIZE_PX, 0 })) {
				// hot pixel was in the left half of the cell, so we want to increase the offset to preference the column to the left
				hotPixelCellOffset.deltaX++;
			}
			if (itemSize.height % 2 == 0 && cell.contains(cursorPosition + Displacement { 0, INV_SLOT_HALF_SIZE_PX })) {
				// hot pixel was in the top half of the cell, so we want to increase the offset to preference the row above
				hotPixelCellOffset.deltaY++;
			}
			// Then work out the top left cell of the nearest area that could fit this item (as pasting on the edge of the stash would otherwise put it out of bounds)
			point.y = clamp(point.y - hotPixelCellOffset.deltaY, 0, StashGridSize.height - itemSize.height);
			point.x = clamp(point.x - hotPixelCellOffset.deltaX, 0, StashGridSize.width - itemSize.width);
			return point;
		}
	}

	return {};
}

bool IsItemAllowedInStash(const Item &item)
{
	return item._iMiscId != IMISC_ARENAPOT;
}

void CheckStashPaste(Point cursorPosition)
{
	Player &player = *MyPlayer;

	if (!IsItemAllowedInStash(player.HoldItem))
		return;

	if (player.HoldItem._itype == ItemType::Gold) {
		if (Stash.gold > std::numeric_limits<int>::max() - player.HoldItem._ivalue)
			return;
		Stash.gold += player.HoldItem._ivalue;
		player.HoldItem.clear();
		PlaySFX(IS_GOLD);
		Stash.dirty = true;
		NewCursor(CURSOR_HAND);
		oracool::ScheduleAutoSaveForStashChange();
		return;
	}

	const Size itemSize = GetInventorySize(player.HoldItem);

	std::optional<Point> targetSlot = FindTargetSlotUnderItemCursor(cursorPosition, itemSize);
	if (!targetSlot)
		return;

	Point firstSlot = *targetSlot;

	// Check that no more than 1 item is replaced by the move
	StashStruct::StashCell stashIndex = StashStruct::EmptyCell;
	for (Point point : PointsInRectangle(Rectangle { firstSlot, itemSize })) {
		StashStruct::StashCell iv = Stash.GetItemIdAtPosition(point);
		if (iv == StashStruct::EmptyCell || stashIndex == iv)
			continue;
		if (stashIndex == StashStruct::EmptyCell) {
			stashIndex = iv; // Found first item
			continue;
		}
		return; // Found a second item
	}

	PlaySFX(ItemInvSnds[GetItemDropAnimIndex(player.HoldItem._iCurs)]);

	// MERGE before swap (user, 2026-09-05: "too many items in the stash dont seem to stack"): a
	// potion dropped on a potion of its kind joins the stack, up to 99, exactly as the backpack
	// and the belt do. The stash used to swap the two, which is how a page filled with singles.
	if (stashIndex != StashStruct::EmptyCell && player.HoldItem.canStackWith(Stash.stashList[stashIndex])) {
		Item &target = Stash.stashList[stashIndex];
		const int room = Item::MaxStackCount - target.stackCount();
		const int moved = std::min(room, player.HoldItem.stackCount());
		if (moved > 0) {
			target.setStackCount(target.stackCount() + moved);
			const int remainder = player.HoldItem.stackCount() - moved;
			if (remainder <= 0)
				player.HoldItem.clear();
			else
				player.HoldItem.setStackCount(remainder);
			Stash.dirty = true;
			oracool::ScheduleAutoSaveForStashChange();
			NewCursor(player.HoldItem);
			return;
		}
	}

	// Need to set the item anchor position to the bottom left so drawing code functions correctly.
	player.HoldItem.position = firstSlot + Displacement { 0, itemSize.height - 1 };

	if (stashIndex == StashStruct::EmptyCell) {
		Stash.stashList.emplace_back(player.HoldItem.pop());
		// stashList will have at most 10 000 items, up to 65 535 are supported with uint16_t indexes
		stashIndex = static_cast<uint16_t>(Stash.stashList.size() - 1);
	} else {
		// swap the held item and whatever was in the stash at this position
		std::swap(Stash.stashList[stashIndex], player.HoldItem);
		// then clear the space occupied by the old item
		for (auto &row : Stash.GetCurrentGrid()) {
			for (auto &itemId : row) {
				if (itemId - 1 == stashIndex)
					itemId = 0;
			}
		}
	}

	// Finally mark the area now occupied by the pasted item in the current page/grid.
	AddItemToStashGrid(Stash.GetPage(), firstSlot, stashIndex, itemSize);

	Stash.dirty = true;
	oracool::ScheduleAutoSaveForStashChange();

	NewCursor(player.HoldItem);
}

void CheckStashCut(Point cursorPosition, bool automaticMove)
{
	Player &player = *MyPlayer;

	if (IsWithdrawGoldOpen) {
		IsWithdrawGoldOpen = false;
	}

	Point slot = InvalidStashPoint;

	for (auto point : StashGridRange) {
		Rectangle cell {
			GetStashSlotCoord(point),
			StashCellPx // the grid's own 28px pitch; 29 gave each cell's first column and row to its neighbour (round 13)
		};

		// check which inventory rectangle the mouse is in, if any
		if (cell.contains(cursorPosition)) {
			slot = point;
			break;
		}
	}

	if (slot == InvalidStashPoint) {
		return;
	}

	Item &holdItem = player.HoldItem;
	holdItem.clear();

	bool automaticallyMoved = false;
	bool automaticallyEquipped = false;

	StashStruct::StashCell iv = Stash.GetItemIdAtPosition(slot);
	if (iv != StashStruct::EmptyCell) {
		holdItem = Stash.stashList[iv];
		if (automaticMove) {
			if (CanBePlacedOnBelt(holdItem)) {
				automaticallyMoved = AutoPlaceItemInBelt(player, holdItem, true);
			} else {
				automaticallyMoved = automaticallyEquipped = AutoEquip(player, holdItem);
			}
		}

		if (!automaticMove || automaticallyMoved) {
			Stash.RemoveStashItem(iv);
			oracool::ScheduleAutoSaveForStashChange();
		}
	}

	if (!holdItem.isEmpty()) {
		CalcPlrInv(player, true);
		holdItem.updateRequiredStatsCacheForPlayer(player); // with the book rule, as the pack's items (round 28 audit)
		if (automaticallyEquipped) {
			PlaySFX(ItemInvSnds[GetItemDropAnimIndex(holdItem._iCurs)]);
		} else if (!automaticMove || automaticallyMoved) {
			PlaySFX(IS_IGRAB);
		}

		if (automaticMove) {
			if (!automaticallyMoved) {
				if (CanBePlacedOnBelt(holdItem)) {
					player.SaySpecific(HeroSpeech::IHaveNoRoom);
				} else {
					player.SaySpecific(HeroSpeech::ICantDoThat);
				}
			}

			holdItem.clear();
		} else {
			NewCursor(holdItem);
		}
	}
}

} // namespace

int WithdrawGold(Player &player, int amount)
{
	const int unplacedGold = AddGoldToInventory(player, amount);
	const int transferredGold = amount - unplacedGold;
	if (transferredGold == 0)
		return 0;

	Stash.gold -= transferredGold;
	player._pGold = CalculateGold(player);
	Stash.dirty = true;
	if (&player == MyPlayer)
		oracool::ScheduleAutoSaveForStashChange();
	return transferredGold;
}

Rectangle GetStashPanelRect()
{
	// Flush to the BOTTOM-left, like the character sheet, quest log and waypoint list - the stash
	// shares the left-hand slot with them and opens with the inventory on the right, so all four had
	// to move together (user, 2026-08-27).
	return { { 0, oracool::BottomDockedTop(StashPanelSize.height) }, StashPanelSize };
}

Point GetStashSlotCoord(Point slot)
{
	return GetPanelPosition(UiPanels::Stash, slot * StashCellPx + Displacement { StashGridLeft, StashGridTop });
}

void FreeStashGFX()
{
	// Nothing left to free: data\stash.clx went with the panel it drew, and data\stashnavbtns.clx
	// with the arrows, which are text now. Kept as a function because InitStash's counterpart is
	// called from the shutdown path.
}

void InitStash()
{
}

void OpenStash()
{
	// The reported bug (user, 2026-08-31): with the character sheet open, this set the flag and
	// nothing else, so GetLeftPanelContent kept answering Character and the stash was open,
	// invisible, and unclickable until the sheet was closed.
	// The counters, the artisans' benches and the Cube share this slot, and only one of them may have
	// it (2026-09-22). Before the flag, so a refused close leaves the other window up rather than
	// being drawn over.
	CloseOtherShopSurfaces();
	// A bench or the Cube that refused to close (a full pack) keeps the slot: the stash opened under it, invisible and
	// dead (round 10 audit, v1.12.235) - the workshop and the Cube back out the same way.
	if (oracool::IsWorkshopOpen() || oracool::IsLevskiRoarOpen())
		return;
	// Locked while its file could not be read: anything put in would never be saved (round 15 audit, v1.12.240).
	if (StashFileRefused) {
		oracool::LogEvent("The stash could not be read this game, so it stays shut - nothing put in it would be saved.", UiFlags::ColorRed);
		return;
	}
	TakeLeftPanelSlot(LeftPanelContent::Stash);
	IsStashOpen = true;
	Stash.RefreshItemStatFlags();
	invflag = true;
	if (ControlMode != ControlTypes::KeyboardAndMouse) {
		if (pcurs == CURSOR_DISARM)
			NewCursor(CURSOR_HAND);
		FocusOnInventory();
	}
}

void TransferItemToInventory(Player &player, uint16_t itemId)
{
	if (itemId == StashStruct::EmptyCell || itemId >= Stash.stashList.size()) {
		return; // a stale hover index (round 10 audit)
	}

	Item &item = Stash.stashList[itemId];
	if (item.isEmpty()) {
		return;
	}

	if (!AutoPlaceItemInInventory(player, item, true)) {
		player.SaySpecific(HeroSpeech::IHaveNoRoom);
		return;
	}

	PlaySFX(ItemInvSnds[GetItemDropAnimIndex(item._iCurs)]);

	Stash.RemoveStashItem(itemId);
	CalcPlrInvKeepingLife(player); // a charm taken out counts at once (round 27 audit), life kept (round 28)
	if (&player == MyPlayer)
		oracool::ScheduleAutoSaveForStashChange();
}

int StashButtonPressed = -1;
/** @brief Set while SORT is held, so it can be drawn pressed - it is text, not art. */
bool StashSortPressed = false;

void CheckStashButtonRelease(Point mousePosition)
{
	// Only for an open stash: a press, Esc, then a release on the same spot opened the withdraw box with no stash behind
	// it (round 10 audit).
	if (!IsStashOpen) {
		GoldDisplayPressed = false;
		StashSortPressed = false;
		StashButtonPressed = -1;
		return;
	}
	if (GoldDisplayPressed) {
		if (GoldButtonContains(mousePosition)) {
			oracool::PlayUiMoveSound(); // Oracool: the gold total is a button since Sort took its place
			// A TOGGLE (user, 2026-09-24 dev note: "make clicking on the gold icon in stash to open/close
			// the gold draw window"): the press that opened the box closes it.
			if (IsWithdrawGoldOpen)
				CloseGoldWithdraw();
			else
				StartGoldWithdraw();
		}
		GoldDisplayPressed = false;
	}

	if (StashSortPressed) {
		Rectangle sortRect = StashSortButtonRect;
		sortRect.position = GetPanelPosition(UiPanels::Stash, sortRect.position);
		if (sortRect.contains(mousePosition)) {
			// Oracool: user request - was withdraw gold (moved to clicking the gold total itself,
			// see GoldDisplayRect); this control is Sort. IS_ISHIEL is the sound normally played
			// when placing a shield into its equip slot, per the user's request.
			if (SortStash(*MyPlayer))
				PlaySFX(IS_ISHIEL);
			else
				InitDiabloMsg(_("The stash is too full to sort"));
		}
		StashSortPressed = false;
	}

	if (StashButtonPressed == -1)
		return;

	Rectangle stashButton = StashNavButtonRectAt(StashButtonPressed);
	stashButton.position = GetPanelPosition(UiPanels::Stash, stashButton.position);
	if (stashButton.contains(mousePosition)) {
		// Four buttons now, not five - Sort left this table for a text control, so the indices
		// after it shifted down by one. Kept as a switch on the index rather than a table of
		// function pointers because the art is indexed the same way.
		switch (StashButtonPressed) {
		case 0:
			Stash.PreviousPage(10);
			break;
		case 1:
			Stash.PreviousPage();
			break;
		case 2:
			Stash.NextPage();
			break;
		case 3:
			Stash.NextPage(10);
			break;
		}
	}

	StashButtonPressed = -1;
}

void CheckStashButtonPress(Point mousePosition)
{
	if (!IsStashOpen)
		return; // the HUD branch asks on every click; a closed stash has no buttons (round 10 audit)
	if (GoldButtonContains(mousePosition)) {
		GoldDisplayPressed = true;
		StashButtonPressed = -1;
		return;
	}
	GoldDisplayPressed = false;

	Rectangle sortRect = StashSortButtonRect;
	sortRect.position = GetPanelPosition(UiPanels::Stash, sortRect.position);
	if (sortRect.contains(mousePosition)) {
		StashSortPressed = true;
		StashButtonPressed = -1;
		return;
	}
	StashSortPressed = false;

	for (int i = 0; i < StashNavButtonCount; i++) {
		Rectangle stashButton = StashNavButtonRectAt(i);
		stashButton.position = GetPanelPosition(UiPanels::Stash, stashButton.position);
		if (stashButton.contains(mousePosition)) {
			StashButtonPressed = i;
			// The vendors' tab tick at the press (user, 2026-09-25 dev note: "the sound played when hovering over
			// vendor tabs ... play it when hovering or clicking on stash nav buttons") - IS_TITLEMOV.
			oracool::PlayUiMoveSound();
			return;
		}
	}

	StashButtonPressed = -1;
}

void DrawStash(const Surface &out)
{
	// Oracool V1: the shared theme replaces data\stash.clx, exactly as it did for the other five
	// windows - half-transparent fill under the ornate bevel, outlined FontSize30 title, separator.
	//
	// Oracool (2026-08-16): and the theme is in turn replaced here by ui\stash_background.png, the
	// inventory background's pair, which carries its own arches and border. The procedural fill and
	// bevel stay as the fallback so the art is droppable rather than required.
	const Rectangle panel = GetStashPanelRect();
	const bool framedCanvas = oracool::HasStashCanvasArt() || oracool::HasSidePanelGridArt();
	if (framedCanvas) {
		// The stash's OWN storeroom when it is there (user, 2026-09-21), and the shared grid canvas
		// otherwise - both put their frame in the same place, so what follows is the same either way.
		if (oracool::HasStashCanvasArt())
			oracool::DrawStashCanvasArt(out, panel.position);
		else
			oracool::DrawSidePanelGridArt(out, panel.position);

		// NO cast shadow (user, 2026-09-21: "is there a shadow added to stash grid? if so - remove it.
		// it is overlapping the frame"). v1.12.122 drew a 2px black L down the left of the
		// grid-and-frame combo and along its foot, and it was measured against the WRONG frame: the
		// combo is the grid plus OrnateBorderWidth, which is the procedural bezel's footprint, while
		// the frame on this canvas is painted and runs y 159..628. The strips therefore landed inside
		// the painting rather than on the stone beside it.
		//
		// Not re-measured against the painted band, because there is nothing for it to fall on: that
		// band reaches x 26..313 in a 340-wide panel, so a shadow two pixels left of it has seven
		// pixels of stone and the panel's own edge bezel to share. The canvas paints its own relief.
	} else if (oracool::HasSidePanelArt()) {
		oracool::DrawSidePanelArt(out, panel.position);
	} else {
		oracool::DrawThemedFill(out, panel);
		oracool::DrawOrnateBorder(out, panel);
	}

	// No title (user, 2026-09-21: "remove the title of stash"). It stood in the shared PanelTitleTop
	// band, outlined at FontSize30; the storeroom canvas carries its own header now and the word
	// printed over it was a caption on a painting. The same removal Griswold's tabs had on
	// 2026-09-21 - one window naming itself while its neighbours do not is what makes it look
	// unfinished.
	//
	// Nothing moves with it: every control here is placed from the grid frame, not from the title,
	// so the band it vacated is simply stone now.

	// Bug fix: the four page arrows were INVISIBLE until pressed. data\stashnavbtns.clx only holds
	// each button's pressed frame - the unpressed state was painted into data\stash.clx, which the
	// theme replaced, so nothing drew them at rest. They are text now, like SORT beside them and
	// RESET on the character sheet, which also drops the last dependency on that CEL.
	// And the same tick as the pointer enters a page button, once per entry (2026-09-25), as the vendors' tabs do.
	{
		static int lastHoveredNav = -1;
		int hoveredNav = -1;
		for (int i = 0; i < StashNavButtonCount; i++) {
			const Rectangle navRect = StashNavButtonRectAt(i);
			if (Rectangle { GetPanelPosition(UiPanels::Stash, navRect.position), navRect.size }.contains(MousePosition))
				hoveredNav = i;
		}
		if (hoveredNav >= 0 && hoveredNav != lastHoveredNav)
			oracool::PlayUiMoveSound();
		lastHoveredNav = hoveredNav;
	}
	for (int i = 0; i < StashNavButtonCount; i++) {
		const Rectangle navRect = StashNavButtonRectAt(i);
		const Rectangle rect { GetPanelPosition(UiPanels::Stash, navRect.position), navRect.size };
		// A 2px gold box around each (user request, 2026-08-16 - 1px first, then "make the next/prev
		// buttons borders 2px thick"), so the four read as BUTTONS rather than as loose glyphs on the
		// background. Drawn before the label, so the text sits inside its own frame rather than under
		// it.
		//
		// DrawSplitOutline with one colour on both halves is the theme's only weighted outline;
		// DrawColoredOutline is 1px only. Passing the same colour twice collapses the split and
		// leaves a plain uniform border - see its own comment for why the two-colour form exists.
		// A dark face inside that frame (user, 2026-09-02: "plase dark backing in the tab browsing
		// buttons in the stash to see the arrows easier"). These four are the only text buttons in
		// the game drawn straight onto the panel's stone, and gold glyphs on mid-grey are the pairing
		// this window is weakest at - a border alone said "button" without making the arrow legible.
		//
		// BEFORE the outline, so the 2px gold frame stays the button's own edge rather than being
		// dimmed by its filling; and half-transparent rather than a flat colour, so it darkens
		// whatever stone is behind it and survives the next recut of the panel.
		//
		// TWICE when the cursor is on it (user, 2026-09-03: "add second dark backing when hovering
		// over them"). Two passes of the same half-transparent fill, which is a second application of
		// the same table rather than a second colour - so the hover state cannot drift away from the
		// resting state as the art changes, it is simply more of it. That also gives these buttons the
		// hover feedback they never had: pressed turned the glyph white, and nothing at all happened
		// on the way to pressing.
		// THE legacy text box (user, 2026-09-04, with a screenshot of the gold-amount box: "this is the
		// legacy textbox border i was talking about") - not the wide ornate bevel that stood here for
		// a day. The field is part of it, so the two half-transparent backings are gone with the gold
		// box they were painted under; the hover lift is the field's own second colour.
		oracool::DrawLegacyTextBox(out, rect,
		    rect.contains(MousePosition) ? oracool::LegacyTextBoxHoverFill : oracool::LegacyTextBoxFill);
		// NO Shadowed (user, 2026-09-21: "remove the shadows of the nav arrows"). The drop shadow was
		// added for the stone these four used to sit on; inside the legacy box's own dark field it
		// only thickened a 12px glyph in a 10px face, which is what made them look smudged rather
		// than lit. SORT and the gold total keep theirs - those two ARE on open stone.
		DrawString(out, StashNavLabel[i], rect,
		    { UiFlags::AlignCenter | UiFlags::VerticalCenter | StashControlFont // 16 with the 16px buttons (user, 2026-09-21)
		        | (StashButtonPressed == i ? UiFlags::ColorWhite : UiFlags::ColorGold) });
	}

	// One bevelled recess around the whole grid, the way the inventory frames its own.
	const Rectangle gridRect { GetPanelPosition(UiPanels::Stash, { StashGridLeft, StashGridTop }),
		{ StashGridWidth, StashGridRows * StashCellPx } };
	// ONE pass over the painted canvas, two over the procedural fallback (user, 2026-09-21: "make the
	// grid a bit transparent"). Each pass is the same half-transparent blend, so two of them leave
	// about a quarter of what is underneath and one leaves about half - the storeroom shows through
	// the cells now instead of being boarded over by its own inventory grid.
	//
	// Only where there IS a painting to show. With no canvas the fill is not covering art, it IS the
	// grid's face, and one pass there would be a paler grid on grey stone rather than a transparent
	// one - so the fallback keeps its two.
	oracool::DrawThemedFill(out, gridRect, framedCanvas ? 1 : 2);
	// Outside the cells, matching the inventory. The stash needed no repositioning for either frame:
	// its grid already had margin on every side, and it absorbed the carved bezel's extra three
	// pixels without losing a row - see the StashGridBottom assert.
	// Skipped entirely when the grid canvas is up: that canvas has the frame painted into it
	// (2026-09-21), and a second bezel inside the first is what drawing one anyway would give.
	if (!oracool::HasStashCanvasArt() && !oracool::HasSidePanelGridArt()) {
		if (oracool::HasGridBezel(gridRect.size)) {
			oracool::DrawDropShadow(out, gridRect, oracool::GridBezelInset); // the slot shadow (2026-09-05)
			oracool::DrawGridBezel(out, gridRect);
		} else {
			oracool::DrawOrnateBorderOutside(out, gridRect);
		}
	}

	// Oracool: user request - 1px cell rules, deliberately a DIFFERENT colour from the inventory
	// grid's. The inventory divides its cells with the theme's 3px gold bevel; the stash is a much
	// larger grid, and the same treatment at sixteen rows would read as a gold mesh rather than as
	// storage. Dark grey, one pixel, is enough to separate the cells and lets the items carry the
	// colour.
	//
	// Drawn on the last pixel of the preceding cell's span, exactly as the inventory grid draws its
	// own - same expression, same colour. At a 28 pitch that pixel belongs to a slot rather than to
	// the rule, so an occupied cell's item overdraws its own boundary; that is what the inventory
	// has always done, and matching it is the point (user request, 2026-08-16).
	// THE SLOT FACE, one per cell, before the rules (user, 2026-09-22: "apply this texture to all
	// 28x28px inv/stash grids game-wide"). Under the rules on purpose: the art carries its own bevel
	// and the rules are what the stash has always separated its cells with, so the two agree rather
	// than the texture painting over the grid it sits in.
	for (auto slot : StashGridRange) {
		oracool::DrawSlotBackground(out, { GetStashSlotCoord(slot), { StashCellPx, StashCellPx } });
	}

	for (int col = 1; col < StashGridColumns; col++) {
		const int x = gridRect.position.x + col * StashCellPx - 1;
		DrawVerticalLine(out, { x, gridRect.position.y }, gridRect.size.height, StashGridLineColor);
	}
	for (int row = 1; row < StashGridRows; row++) {
		const int y = gridRect.position.y + row * StashCellPx - 1;
		DrawHorizontalLine(out, { gridRect.position.x, y }, gridRect.size.width, StashGridLineColor);
	}

	constexpr Displacement offset { 0, INV_SLOT_SIZE_PX - 1 };

	for (auto slot : StashGridRange) {
		StashStruct::StashCell itemId = Stash.GetItemIdAtPosition(slot);
		if (itemId == StashStruct::EmptyCell) {
			continue; // No item in the given slot
		}
		Item &item = Stash.stashList[itemId];
		// One backing per ITEM, not per cell (user request, 2026-08-16) - see inv.cpp's matching
		// loop. `item.position != slot` is the same first-slot test the sprite loop below uses.
		if (item.position != slot)
			continue;
		const Size itemCells = GetInventorySize(item);
		InvDrawSlotBack(out, GetStashSlotCoord(slot) + offset,
		    { itemCells.width * InventorySlotSizeInPixels.width, itemCells.height * InventorySlotSizeInPixels.height },
		    item);
	}

	for (auto slot : StashGridRange) {
		StashStruct::StashCell itemId = Stash.GetItemIdAtPosition(slot);
		if (itemId == StashStruct::EmptyCell) {
			continue; // No item in the given slot
		}

		Item &item = Stash.stashList[itemId];
		if (item.position != slot) {
			continue; // Not the first slot of the item
		}

		int frame = item._iCurs + CURSOR_FIRSTITEM;

		const Point position = GetStashSlotCoord(item.position) + offset;
		const ClxSprite sprite = GetInvItemSprite(frame);

		const bool hovered = pcursstashitem == itemId;
		if (hovered) {
			uint8_t color = GetOutlineColor(item, true);
			ClxDrawOutline(out, color, position, sprite);
		}

		DrawItem(item, out, position, sprite);
		// Over the sprite, hover-only - the same overlay the backpack draws. See inv.cpp's copy.
		if (hovered)
			oracool::DrawSocketOverlay(out, item, position, GetInventorySize(item));
	}

	const Point position = GetPanelPosition(UiPanels::Stash);

	// Gold, not the row's white (user request, 2026-08-16: "use gold font") - so the page readout
	// reads as a heading over the nav buttons rather than as another value in the row.
	// FontSize16 (user, 2026-09-21: "reduce the page navigation font to font16") in the rect its own
	// width gives it, centred over the grid - the same rect the buttons were placed from, so the six
	// pixels either side are six pixels of the string rather than of a box around it.
	{
		const Rectangle label = StashPageLabelRectNow();
		DrawString(out, StashPageLabelText(),
		    { position + Displacement { label.position.x, label.position.y }, label.size },
		    { UiFlags::AlignCenter | UiFlags::VerticalCenter | UiFlags::ColorGold | StashControlFont | UiFlags::Shadowed });
	}

	// Gold in the theme's own gold, matching the inventory's readout, rather than the plain white
	// the vanilla panel used.
	// Right-aligned, so the total is FLUSH with column 10's right edge however long it gets - a
	// left-aligned string in a right-anchored box would leave a ragged gap that grows as the player
	// gets richer, which is the opposite of what "flush with the right border" asks for.
	// Shadowed, like the inventory's pair and the character sheet's every string (user, 2026-09-02:
	// "also apply this type of shadows in the stash SORT and GOLD"). The same two words in the same
	// two roles in the neighbouring window - if one of them casts a shadow they both must, or the
	// pairing the 2026-08-16 layout was built on comes apart on the one detail nobody would think to
	// check.
	// Griswold's pair, under the grid: the pile, with the number beneath it and no "GOLD:" in front
	// (user, 2026-09-21: "remove the current gold counter and place a new one, the same as in
	// Griswold stores. Icon + counter under the grid"). Left-aligned, because the pile is on the left
	// and a right-aligned number under a left-aligned icon is two controls, not one.
	if (oracool::GetLoosePngSize(StashGoldIconAsset).width > 0)
		oracool::DrawLoosePng(out, StashGoldIconAsset, position + Displacement { StashGoldIconAt.x, StashGoldIconAt.y }
		        + (GoldDisplayPressed ? GoldIconPressSink : Displacement { 0, 0 }));
	DrawString(out, FormatInteger(Stash.gold),
	    { position + Displacement { GoldDisplayRect.position.x, GoldDisplayRect.position.y }, GoldDisplayRect.size },
	    { UiFlags::ColorWhitegold | UiFlags::VerticalCenter | UiFlags::FontSize12 | UiFlags::Shadowed });

	// Left-aligned now that it sits UNDER the total rather than opposite it: the word starts on the
	// same x the number starts on, so the two stack as one block instead of reading as a row.
	DrawString(out, _("SORT"),
	    { position + Displacement { StashSortButtonRect.position.x, StashSortButtonRect.position.y }, StashSortButtonRect.size },
	    { UiFlags::VerticalCenter | (StashSortPressed ? UiFlags::ColorWhite : UiFlags::ColorGold) | UiFlags::FontSize12
	        | UiFlags::Shadowed });
}

void CheckStashItem(Point mousePosition, bool isShiftHeld, bool isCtrlHeld)
{
	if (!MyPlayer->HoldItem.isEmpty()) {
		CheckStashPaste(mousePosition);
	} else if (isCtrlHeld) {
		TransferItemToInventory(*MyPlayer, pcursstashitem);
	} else {
		CheckStashCut(mousePosition, isShiftHeld);
	}
}

uint16_t CheckStashHLight(Point mousePosition)
{
	// Oracool: user request - "Sort" tooltip over the Sort control.
	//
	// StashSortButtonRect, and it was StashButtonRect[2] until 2026-09-03 - the ">" nav button. That
	// index was the Sort control's slot in the button table before Sort became a text button, and the
	// tooltip kept pointing at the slot rather than following the control out of the table. So
	// hovering "next page" said "Sort" and hovering SORT said nothing. Found while widening these
	// buttons, which is the change that would have made a wrong tooltip twice as easy to hit.
	Rectangle sortButtonRect = StashSortButtonRect;
	sortButtonRect.position = GetPanelPosition(UiPanels::Stash, sortButtonRect.position);
	if (sortButtonRect.contains(mousePosition)) {
		InfoColor = UiFlags::ColorWhite;
		SetPanelString(_("Sort"), UiFlags::ColorWhite); // not a bare assignment - see CheckInvHLight
		return StashStruct::EmptyCell;
	}

	Point slot = InvalidStashPoint;
	for (auto point : StashGridRange) {
		Rectangle cell {
			GetStashSlotCoord(point),
			StashCellPx // the grid's own 28px pitch; 29 gave each cell's first column and row to its neighbour (round 13)
		};

		if (cell.contains(mousePosition)) {
			slot = point;
			break;
		}
	}

	if (slot == InvalidStashPoint)
		return -1;

	InfoColor = UiFlags::ColorWhite;

	StashStruct::StashCell itemId = Stash.GetItemIdAtPosition(slot);
	if (itemId == StashStruct::EmptyCell) {
		return -1;
	}

	Item &item = Stash.stashList[itemId];
	if (item.isEmpty()) {
		return -1;
	}

	// Through SetPanelString - see the note at the matching call in inv.cpp's CheckInvHLight.
	SetPanelString(item.getName(), item.getTextColor());
	if (item._iIdentified) {
		PrintItemDetails(item);
	} else {
		PrintItemDur(item);
	}
	AddShopServiceFeeLine(item); // the hammer and Adria's recharge work here too (round 35 audit)

	return itemId;
}

bool UseStashItem(uint16_t c)
{
	if (MyPlayer->_pInvincible && MyPlayer->_pHitPoints == 0)
		return true;
	if (pcurs != CURSOR_HAND)
		return true;
	if (stextflag != TalkID::None)
		return true;

	// A hover index can be a click stale: RemoveStashItem moves the last item into a freed index (round 10 audit).
	if (c >= Stash.stashList.size())
		return true;
	Item *item = &Stash.stashList[c];

	constexpr int SpeechDelay = 10;
	if (item->IDidx == IDI_MUSHROOM) {
		MyPlayer->Say(HeroSpeech::NowThatsOneBigMushroom, SpeechDelay);
		return true;
	}
	if (item->IDidx == IDI_FUNGALTM) {
		PlaySFX(IS_IBOOK);
		MyPlayer->Say(HeroSpeech::ThatDidntDoAnything, SpeechDelay);
		return true;
	}

	if (!item->isUsable())
		return false;

	if (!MyPlayer->CanUseItem(*item)) {
		MyPlayer->Say(HeroSpeech::ICantUseThisYet);
		return true;
	}

	if (IsWithdrawGoldOpen) {
		IsWithdrawGoldOpen = false;
	}

	if (item->isScroll()) {
		return true;
	}

	if (item->_iMiscId > IMISC_RUNEFIRST && item->_iMiscId < IMISC_RUNELAST && leveltype == DTYPE_TOWN) {
		return true;
	}

	// The fork's three one-use items, whose effect lives in UseInvItem rather than UseItem (user, 2026-09-26
	// dev note: "make signet of learning unable to consume if 20/20 reached"). The backpack refused a signet at
	// the cap; this path went straight to UseItem and then removed the item - so a signet was eaten for nothing,
	// and a keystone or a sealed map was eaten without opening anything, UseItem having no case for either.
	if (item->_iMiscId == IMISC_ORACOOL_SIGNET && !oracool::CanConsumeSignet(*MyPlayer)) {
		MyPlayer->Say(HeroSpeech::ICantUseThisYet);
		oracool::LogEvent("Signet of Learning: this life has no room for another.");
		return true;
	}
	if (item->_iMiscId == IMISC_ORACOOL_KEYSTONE || item->_iMiscId == IMISC_ORACOOL_MAP) {
		bool opened = false;
		if (item->_iMiscId == IMISC_ORACOOL_KEYSTONE) {
			opened = oracool::UseGuardianKeystone(*MyPlayer, *item);
			if (!opened && (!MyPlayer->isOnLevel(0) || setlevel)) // in town the refusal logged its own reason
				oracool::LogEvent("A keystone only turns in town, at the Rift Monument.");
		} else {
			oracool::NamedEncounter encounter;
			if (!oracool::EncounterForMapItem(item->IDidx, encounter))
				return true;
			opened = oracool::EnterNamedEncounter(*MyPlayer, encounter);
			if (opened)
				oracool::PlayUiEventSound(oracool::UiEventSound::MapUnseal);
			else
				oracool::LogEvent("A sealed map only opens in town.");
		}
		if (!opened) {
			MyPlayer->Say(HeroSpeech::ICantUseThisYet);
			return true;
		}
		// A keystone stays until the first step through the portal (oracool::SpendPendingKeystone); a map is used now.
		if (item->_iMiscId == IMISC_ORACOOL_KEYSTONE)
			return true;
		Stash.RemoveStashItem(c);
		oracool::ScheduleAutoSaveForStashChange();
		return true;
	}

	// The backpack's book gate: read from the stash, a book above the hero's level (or at the ceiling) was used up and
	// taught nothing (round 10 audit, v1.12.235).
	if (RefuseUnreadableBook(*MyPlayer, *item))
		return true;

	if (item->_iMiscId == IMISC_BOOK)
		PlaySFX(IS_RBOOK);
	// A Signet of Learning speaks with its own sound from the stash too, as from the backpack (round 31 audit).
	else if (!(item->_iMiscId == IMISC_ORACOOL_SIGNET && oracool::PlayUiEventSound(oracool::UiEventSound::SignetUse)))
		PlaySFX(ItemInvSnds[GetItemDropAnimIndex(item->_iCurs)]);

	UseItem(MyPlayerId, item->_iMiscId, item->_iSpell, -1);

	if (Stash.stashList[c]._iMiscId == IMISC_MAPOFDOOM)
		return true;
	if (Stash.stashList[c]._iMiscId == IMISC_NOTE) {
		InitQTextMsg(TEXT_BOOK9);
		CloseInventory();
		return true;
	}

	// Oracool bug fix: user report - reading a stack of 4 Books of Flash from the Stash taught
	// only 1 spell level (UseItem above is correct - it only ever grants one level per use) but
	// consumed the entire stack, not just 1 unit, because this always removed the whole stash
	// slot unconditionally. Matches DecrementOrRemoveInvItem's inventory/belt behavior.
	Item &stashItem = Stash.stashList[c];
	if (stashItem.isStackableConsumable() && stashItem.stackCount() > 1) {
		stashItem.setStackCount(stashItem.stackCount() - 1);
		Stash.dirty = true;
	} else {
		Stash.RemoveStashItem(c);
	}
	oracool::ScheduleAutoSaveForStashChange();

	return true;
}

void StashStruct::RemoveStashItem(StashStruct::StashCell iv)
{
	// The hover index names a list slot the swap below may give to another item: forgotten until the next hover (round 10).
	pcursstashitem = StashStruct::EmptyCell;
	// Every page's references, not only the page on screen (audit, 2026-09-27): Ogden's boards and the rift's keystone
	// take items from any page, and the page left behind kept cells naming an index past the list - an out-of-range
	// read when drawn - or, after the swap below, an item on another page.
	for (auto &page : Stash.stashGrids) {
		for (auto &row : page.second) {
			for (StashStruct::StashCell &itemId : row) {
				if (itemId - 1 == iv) {
					itemId = 0;
				}
			}
		}
	}

	if (stashList.empty()) {
		return;
	}

	// If the item at the end of stash array isn't the one we removed, we need to swap its position in the array with the removed item
	StashStruct::StashCell lastItemIndex = static_cast<StashStruct::StashCell>(stashList.size() - 1);
	if (lastItemIndex != iv) {
		stashList[iv] = stashList[lastItemIndex];

		for (auto &pair : Stash.stashGrids) {
			auto &grid = pair.second;
			for (auto &row : grid) {
				for (StashStruct::StashCell &itemId : row) {
					if (itemId == lastItemIndex + 1) {
						itemId = iv + 1;
					}
				}
			}
		}
	}
	stashList.pop_back();
	Stash.dirty = true;
}

void StashStruct::SetPage(unsigned newPage)
{
	page = std::min(newPage, LastStashPage);
	dirty = true;
	pcursstashitem = StashStruct::EmptyCell; // the last page's hover (round 38 audit: a Ctrl+click moved it)
}

void StashStruct::NextPage(unsigned offset)
{
	if (page <= LastStashPage) {
		page += std::min(offset, LastStashPage - page);
	} else {
		page = LastStashPage;
	}
	dirty = true;
	pcursstashitem = StashStruct::EmptyCell; // round 38 audit
}

void StashStruct::PreviousPage(unsigned offset)
{
	if (page <= LastStashPage) {
		page -= std::min(offset, page);
	} else {
		page = LastStashPage;
	}
	dirty = true;
	pcursstashitem = StashStruct::EmptyCell; // round 38 audit
}

void StashStruct::RefreshItemStatFlags()
{
	for (auto &item : Stash.stashList) {
		item.updateRequiredStatsCacheForPlayer(*MyPlayer);
	}
}

void StartGoldWithdraw()
{
	CloseGoldDrop();

	if (talkflag)
		control_reset_talk();

	const Point start = GetPanelPosition(UiPanels::Stash, Point { 67, 128 } + GoldWithdrawDrop);
	SDL_Rect rect = MakeSdlRect(start.x, start.y, 180, 20);
	SDL_SetTextInputRect(&rect);

	IsWithdrawGoldOpen = true;
	GoldWithdrawText[0] = '\0';
	GoldWithdrawInputState.emplace(NumberInputState::Options {
	    /*textOptions*/ {
	        /*value=*/GoldWithdrawText,
	        /*cursor=*/&GoldWithdrawCursor,
	        /*maxLength=*/sizeof(GoldWithdrawText) - 1,
	    },
	    /*min=*/0,
	    // Compared in 64 bits and narrowed after: RoomForGold answers up to 7,000,000,000 on an
	    // empty backpack, which is wider than the int this prompt's cap is. Taking the min FIRST
	    // means the narrowing only ever happens to a value Stash.gold already bounded, so it cannot
	    // truncate.
	    /*max=*/static_cast<int>(std::min<int64_t>(RoomForGold(), Stash.gold)),
	});
	SDL_StartTextInput();
}

void WithdrawGoldKeyPress(SDL_Keycode vkey)
{
	Player &myPlayer = *MyPlayer;

	if (myPlayer._pHitPoints >> 6 <= 0) {
		CloseGoldWithdraw();
		return;
	}

	switch (vkey) {
	case SDLK_RETURN:
	case SDLK_KP_ENTER:
		if (const int value = GoldWithdrawInputState->value(); value != 0) {
			WithdrawGold(myPlayer, value);
			PlaySFX(IS_GOLD);
		}
		CloseGoldWithdraw();
		break;
	case SDLK_ESCAPE:
		CloseGoldWithdraw();
		break;
	default:
		break;
	}
}

/**
 * @brief The withdraw box's rect on screen: vanilla's gold-drop plate, dropped onto the grid frame's foot.
 * ClxDraw places the plate by its BOTTOM-left, (30, 178) before the drop, so its top is found from its height.
 */
Rectangle GoldWithdrawBoxRect()
{
	const ClxSprite plate = (*pGBoxBuff)[0];
	const int height = plate.height();
	return { GetPanelPosition(UiPanels::Stash, Point { 30, 178 - height + 1 } + GoldWithdrawDrop), { plate.width(), height } };
}

bool CheckGoldWithdrawPromptPress(Point mousePosition)
{
	if (!IsWithdrawGoldOpen || !pGBoxBuff)
		return false;
	// The red X in the box's top-right corner, where every window has it (user, 2026-09-24 dev note:
	// "put the X close button on same spot on the draw gold window").
	if (oracool::CheckWindowCloseButtonClick(GoldWithdrawBoxRect(), mousePosition)) {
		CloseGoldWithdraw();
		return true;
	}
	// And the pile, which closes what it opened - pressed here, acted on at the release.
	if (GoldButtonContains(mousePosition)) {
		GoldDisplayPressed = true;
		return true;
	}
	return false;
}

void DrawGoldWithdraw(const Surface &out)
{
	if (!IsWithdrawGoldOpen) {
		return;
	}

	const string_view amountText = GoldWithdrawText;
	const TextInputCursorState &cursor = GoldWithdrawCursor;

	const int dialogX = 30;

	ClxDraw(out, GetPanelPosition(UiPanels::Stash, Point { dialogX, 178 } + GoldWithdrawDrop), (*pGBoxBuff)[0]);

	// Pre-wrap the string at spaces, otherwise DrawString would hard wrap in the middle of words
	const std::string wrapped = WordWrapString(_("How many gold pieces do you want to withdraw?"), 200);

	// The split gold dialog is roughly 4 lines high, but we need at least one line for the player to input an amount.
	// Using a clipping region 50 units high (approx 3 lines with a lineheight of 17) to ensure there is enough room left
	//  for the text entered by the player.
	DrawString(out, wrapped, { GetPanelPosition(UiPanels::Stash, Point { dialogX + 31, 75 } + GoldWithdrawDrop), { 200, 50 } },
	    { UiFlags::ColorWhitegold | UiFlags::AlignCenter, 1, 17 });

	// Even a ten digit amount of gold only takes up about half a line. There's no need to wrap or clip text here so we
	// use the Point form of DrawString.
	DrawString(out, amountText, GetPanelPosition(UiPanels::Stash, Point { dialogX + 37, 128 } + GoldWithdrawDrop),
	    TextRenderOptions {
	        /*flags=*/UiFlags::ColorWhite | UiFlags::PentaCursor,
	        /*spacing=*/1,
	        /*lineHeight=*/-1,
	        /*cursorPosition=*/static_cast<int>(cursor.position),
	        /*highlightRange=*/ { static_cast<int>(cursor.selection.begin), static_cast<int>(cursor.selection.end) },
	    });
	oracool::DrawWindowCloseButton(out, GoldWithdrawBoxRect());
}

void CloseGoldWithdraw()
{
	if (!IsWithdrawGoldOpen)
		return;
	SDL_StopTextInput();
	IsWithdrawGoldOpen = false;
	GoldWithdrawInputState = std::nullopt;
}

bool HandleGoldWithdrawTextInputEvent(const SDL_Event &event)
{
	return HandleNumberInputEvent(event, *GoldWithdrawInputState);
}

void TakeOutgrownStashItems(std::vector<Item> &displaced)
{
	// A stash item fills every one of its cells, from its top-left, so a taller item reaches down.
	const auto takeOne = [&]() {
		for (auto &[page, grid] : Stash.stashGrids) {
			for (int x = 0; x < StashGridColumns; x++) {
				for (int y = 0; y < StashGridRows; y++) {
					const StashStruct::StashCell cell = grid[x][y];
					if (cell == 0 || cell > Stash.stashList.size())
						continue;
					if ((x > 0 && grid[x - 1][y] == cell) || (y > 0 && grid[x][y - 1] == cell))
						continue; // not the top-left cell
					const Size size = GetInventorySize(Stash.stashList[cell - 1]);
					bool fits = x + size.width <= StashGridColumns && y + size.height <= StashGridRows;
					for (int dx = 0; fits && dx < size.width; dx++) {
						for (int dy = 0; fits && dy < size.height; dy++) {
							// Every cell the item's own - an empty one inside the footprint means it grew (audit, 2026-09-29).
							const StashStruct::StashCell other = grid[x + dx][y + dy];
							fits = other == cell;
						}
					}
					if (fits)
						continue;
					displaced.push_back(Stash.stashList[cell - 1]);
					Stash.RemoveStashItem(static_cast<StashStruct::StashCell>(cell - 1));
					Stash.dirty = true;
					return true;
				}
			}
		}
		return false;
	};
	while (takeOne()) { }
}

bool AutoPlaceItemInStash(Player &player, const Item &item, bool persistItem)
{
	if (!IsItemAllowedInStash(item))
		return false;
	// A stash file this game could not read is left as it is and never written: what went in was lost (round 15 audit).
	if (StashFileRefused)
		return false;

	if (item._itype == ItemType::Gold) {
		if (Stash.gold > std::numeric_limits<int>::max() - item._ivalue)
			return false;
		if (persistItem) {
			Stash.gold += item._ivalue;
			Stash.dirty = true;
		}
		return true;
	}

	// All-or-nothing, like the backpack (external audit, 2026-09-06: INV-01): the merge below
	// wrote into partial stacks before the scan knew there was a cell for the rest, so a full
	// stash returned false with the stacks already topped up and the source still in the hand.
	if (persistItem && !AutoPlaceItemInStash(player, item, /*persistItem=*/false))
		return false;

	// MERGE first (user, 2026-09-05): a stackable kind joins stacks of itself that have room, on
	// any page, before a free cell is looked for - the backpack's rule. Only what does not fit in
	// an existing stack goes on to the scan. Without this every ctrl-click deposit took a cell.
	Item remaining = item;
	if (remaining.isStackableConsumable()) {
		for (Item &existing : Stash.stashList) {
			if (!existing.canStackWith(remaining))
				continue;
			const int room = Item::MaxStackCount - existing.stackCount();
			if (room <= 0)
				continue;
			const int moved = std::min(room, remaining.stackCount());
			if (persistItem) {
				existing.setStackCount(existing.stackCount() + moved);
				Stash.dirty = true;
			}
			const int left = remaining.stackCount() - moved;
			if (left <= 0)
				return true;
			remaining.setStackCount(left);
		}
	}

	Size itemSize = GetInventorySize(remaining);

	// Try to add the item to the current active page and if it's not possible move forward
	for (unsigned pageCounter = 0; pageCounter < CountStashPages; pageCounter++) {
		unsigned pageIndex = Stash.GetPage() + pageCounter;
		// Wrap around if needed
		if (pageIndex >= CountStashPages)
			pageIndex -= CountStashPages;
		// Search all possible position in stash grid.
		//
		// Bug (fixed 2026-08-16, user report: "sort function of stash isnt utilizing last 6 rows"):
		// both bounds were the literal 10 from the vanilla 10x10 page. The page grew to 10x16 when
		// the stash moved into the 340x720 theme (StashGridRows, stash.h), but this scan did not,
		// so rows 10..15 were unreachable for EVERY auto-placement - Gillian's deposit, shift-click
		// to stash, and the re-pack SortStash does. Only hand-dragging could reach them, which is
		// why sort appeared to be the culprit: it clears the page and re-places through here, so it
		// actively emptied the last six rows.
		//
		// Derived from StashGridSize now, so the next time the page is resized this follows.
		const Size scanArea { StashGridSize.width - (itemSize.width - 1), StashGridSize.height - (itemSize.height - 1) };
		for (auto stashPosition : PointsInRectangle(Rectangle { { 0, 0 }, scanArea })) {
			// Check that all needed slots are free
			bool isSpaceFree = true;
			for (auto itemPoint : PointsInRectangle(Rectangle { stashPosition, itemSize })) {
				uint16_t iv = Stash.stashGrids[pageIndex][itemPoint.x][itemPoint.y];
				if (iv != 0) {
					isSpaceFree = false;
					break;
				}
			}
			if (!isSpaceFree)
				continue;
			if (persistItem) {
				Stash.stashList.push_back(remaining); // what the stacks above did not absorb
				uint16_t stashIndex = static_cast<uint16_t>(Stash.stashList.size() - 1);
				Stash.stashList[stashIndex].position = stashPosition + Displacement { 0, itemSize.height - 1 };
				AddItemToStashGrid(pageIndex, stashPosition, stashIndex, itemSize);
				Stash.dirty = true;
			}
			return true;
		}
	}

	return false;
}

/**
 * @brief Oracool: puts one material on an exact cell of @p page, bypassing the first-fit scan.
 *
 * AutoPlaceItemInStash cannot be used for the material page: it finds the first hole that fits,
 * which is precisely what a fixed layout must not do. Every material is 1x1, so there is no
 * footprint to fit and the cell either holds it or the page is malformed.
 */
/**
 * @brief Puts @p item on @p page with its top-left cell at @p topLeft, claiming its whole footprint.
 *
 * The item's recorded position is its BOTTOM-left cell, as AutoPlaceItemInStash records it and as
 * the stash draws it (sprites anchor at their bottom-left). PlaceMaterialAt below recorded the top
 * cell and claimed one grid cell whatever the item's size, which was right for the 1x1 materials it
 * was written for and wrong for the 2x2 books the consumables layout later sent through it: the
 * book's sprite spilled over the cells above and beside it, and its unclaimed cells were handed to
 * the next stack (user, 2026-09-07: "overlapping adjacent books").
 */
void PlaceStashItemAt(unsigned page, Point topLeft, const Item &item, Size size)
{
	Stash.stashList.emplace_back(item);
	const auto index = static_cast<uint16_t>(Stash.stashList.size() - 1);
	Stash.stashList[index].position = topLeft + Displacement { 0, size.height - 1 };
	AddItemToStashGrid(page, topLeft, index, size);
}

void PlaceMaterialAt(unsigned page, Point cell, const Item &item)
{
	PlaceStashItemAt(page, cell, item, { 1, 1 });
}

/** @brief The first page with nothing on it at or after @p from. */
unsigned FirstEmptyStashPageFrom(unsigned from)
{
	for (unsigned page = from; page < CountStashPages; page++) {
		bool empty = true;
		for (const auto &column : Stash.stashGrids[page]) {
			for (const StashStruct::StashCell cell : column) {
				if (cell != 0) {
					empty = false;
					break;
				}
			}
			if (!empty)
				break;
		}
		if (empty)
			return page;
	}
	// Every page occupied. The caller falls back to the ordinary scan rather than overwriting
	// somebody's items, which is the only safe answer here.
	return CountStashPages;
}

/** @brief The first page with nothing on it, searching upward from 0. */
unsigned FirstEmptyStashPage()
{
	return FirstEmptyStashPageFrom(0);
}

namespace {

/**
 * @brief Folds every stack in @p items into the first stack of its kind that has room, up to 99;
 * the emptied stacks are removed. Every stackable kind - potions, scrolls, materials alike.
 */
void MergeStacks(std::vector<Item> &items)
{
	for (size_t i = 0; i < items.size(); i++) {
		if (items[i].isEmpty() || !items[i].isStackableConsumable())
			continue;
		for (size_t j = i + 1; j < items.size(); j++) {
			if (items[j].isEmpty() || !items[i].canStackWith(items[j]))
				continue;
			const int room = Item::MaxStackCount - items[i].stackCount();
			if (room <= 0)
				break;
			const int moved = std::min(room, items[j].stackCount());
			items[i].setStackCount(items[i].stackCount() + moved);
			if (moved == items[j].stackCount())
				items[j].clear();
			else
				items[j].setStackCount(items[j].stackCount() - moved);
		}
	}
	items.erase(std::remove_if(items.begin(), items.end(), [](const Item &item) { return item.isEmpty(); }), items.end());
}

/**
 * @brief What the stash holds, counted in units: one per item, a stack's count for a stack. SORT merges
 * stacks and re-seats every item, so the list changes shape, but this number must not.
 */
int StashUnitCount(const std::vector<Item> &items)
{
	int units = 0;
	for (const Item &item : items)
		units += item.isStackableConsumable() ? item.stackCount() : 1;
	return units;
}

} // namespace

bool SortStash(Player &player)
{
	// ALL OR NOTHING (external audit of v1.12.188, ITEM-01 - STASH-01 again from v1.11.102). The re-pack
	// below clears the stash and then seats every item again, and the one-page-per-tier layout packs less
	// tightly than the stash it replaces: a stash that is nearly full can come out with items that no
	// longer fit anywhere, and those were simply dropped. The stash as it was is kept here, and put back
	// whole if the sorted one holds fewer units than went in.
	const std::vector<Item> keptList = Stash.stashList;
	const std::map<unsigned, StashStruct::StashGrid> keptGrids = Stash.stashGrids;
	const unsigned keptPage = Stash.GetPage();
	const bool keptDirty = Stash.dirty;
	const int unitsBefore = StashUnitCount(Stash.stashList);

	struct SortEntry {
		Item item;
		StashSortTier tier;
		int setKey; // only meaningful on the Set tier
		int categoryRank;
		int value;
	};
	std::vector<SortEntry> entries;
	std::vector<Item> materials;
	std::vector<Item> consumables; // potions, elixirs, scrolls - their own page on SORT (2026-09-05)
	std::vector<Item> charms; // their own page, after the gear and before the consumables (2026-09-25)
	entries.reserve(Stash.stashList.size());

	// SORT merges EVERY stackable kind first (user, 2026-09-05: "make sure they will stack when i
	// hit sort"). It only merged the materials before, on their own page, so a stash full of single
	// potions sorted into a stash full of single potions.
	std::vector<Item> pool(Stash.stashList.begin(), Stash.stashList.end());
	MergeStacks(pool);

	// Oracool: user request (2026-08-20) - "Move and sort Runes and Gems in their own tab. The
	// first one unoccupied by items." Materials are pulled out of the ordinary sort entirely; what
	// is left packs as it always did, which is also what decides which page comes up empty.
	for (const Item &item : pool) {
		// The SOCKETABLE consumables - runes, gems, jewels - have the material page; every other
		// consumable, the salvage materials included, has the page after it (user, 2026-09-13: "divide
		// socketable and unsocketable consumables in different tabs, one after the other").
		if (IsOracoolRuneIdx(item.IDidx) || IsOracoolGemIdx(item.IDidx) || IsOracoolJewelIdx(item.IDidx)) {
			materials.push_back(item);
			continue;
		}
		// And every other thing that is not WORN (user, 2026-09-24 dev note: "when sorting stash first
		// pages go to items, then consumables follow"; asked, "consumables landed before gear"). The
		// one-use kinds that do not stack - Signets, Sealed Maps, Guardian Keystones - and the quest
		// pieces and ears were filed as gear: Misc items of the Plain tier, so they sorted onto the FIRST
		// page with the white gear, ahead of every magic, rare and unique page. A Misc item is not gear
		// unless it is a charm, which is worn from the backpack and keeps its tier's page.
		if (item.isStackableConsumable() || IsOracoolSalvageIdx(item.IDidx)
		    || (item._itype == ItemType::Misc && !IsOracoolCharmIdx(item.IDidx))) {
			consumables.push_back(item);
			continue;
		}
		// CHARMS on a page of their own (user, 2026-09-25 dev note: "charms still land on page 1 of stash moving
		// items to page 2. move charms in their own page after items, before consumables"). They were gear of
		// their quality tier, so a plain charm opened the plain page and pushed the gear after it along.
		if (IsOracoolCharmIdx(item.IDidx)) {
			charms.push_back(item);
			continue;
		}
		const StashSortTier tier = StashSortTierOf(item);
		entries.push_back({ item, tier,
		    tier == StashSortTier::Set ? StashSortSetKey(item) : 0,
		    StashSortCategoryRank(item), GetItemSellValue(item) });
	}

	std::stable_sort(entries.begin(), entries.end(), [](const SortEntry &a, const SortEntry &b) {
		// TIER FIRST, so each tier's run is contiguous and the placement loop below can hand it a
		// page of its own by watching for the tier changing.
		if (a.tier != b.tier)
			return a.tier < b.tier;
		// On the Set page the SET outranks the category: a set's helm, torso and boots sitting
		// together is the thing asked for, and sorting by category first would have scattered them
		// down the page among every other set's pieces of the same kind.
		if (a.tier == StashSortTier::Set && a.setKey != b.setKey)
			return a.setKey < b.setKey;
		if (a.categoryRank != b.categoryRank)
			return a.categoryRank < b.categoryRank;
		// Bigger footprints first within a band (2026-09-07): the first-fit scan packs a run of 2x2s
		// cleanly and the 1x1s fill the remainder, whereas value order interleaved them and left the
		// big ones scattered into gaps - which is how the books looked "weirdly placed".
		const Size sa = GetInventorySize(a.item);
		const Size sb = GetInventorySize(b.item);
		if (sa.height * sa.width != sb.height * sb.width)
			return sa.height * sa.width > sb.height * sb.width;
		if (sa.height != sb.height)
			return sa.height > sb.height;
		return a.value > b.value;
	});

	// Gold isn't a grid item (it's tracked separately via Stash.gold) and every remaining item
	// already passed IsItemAllowedInStash once to get here, so re-placing them all via
	// AutoPlaceItemInStash - the same first-fit-per-page scan used for every normal stash
	// deposit - is guaranteed to succeed and re-pack every page as tightly as that scan allows.
	Stash.stashList.clear();
	Stash.stashGrids.clear();
	Stash.SetPage(0);

	// ONE PAGE PER TIER. AutoPlaceItemInStash begins its first-fit scan on Stash.GetPage() and only
	// moves forward, so seating the page before a tier's run is the whole mechanism: the run fills
	// that page and spills onto the next if it is longer than a page, and the tier after it starts
	// on the first page still empty. Nothing here reserves pages in advance, which is what keeps an
	// absent tier from leaving a hole - and a hole would be read as "the first empty page" by the
	// material and consumable layouts further down.
	StashSortTier currentTier = entries.empty() ? StashSortTier::Plain : entries.front().tier;
	unsigned tierPage = 0;
	Stash.SetPage(tierPage);
	for (const SortEntry &entry : entries) {
		if (entry.tier != currentTier) {
			currentTier = entry.tier;
			tierPage = FirstEmptyStashPageFrom(tierPage);
			// Out of pages: let the ordinary wrapping scan finish the job. Tidiness is worth less
			// than every item still being in the stash.
			if (tierPage >= CountStashPages)
				tierPage = 0;
			Stash.SetPage(tierPage);
		}
		AutoPlaceItemInStash(player, entry.item, true);
	}

	// The charms' page: the first empty page after the gear, before the socketables and the consumables take
	// theirs - they ask for "the first empty page" below, which is now the one after this. Grouped by kind
	// (the base index), the better quality first within a kind; spills onto the next page like any run.
	if (!charms.empty()) {
		std::stable_sort(charms.begin(), charms.end(), [](const Item &a, const Item &b) {
			if (a.IDidx != b.IDidx)
				return a.IDidx < b.IDidx;
			return StashSortTierOf(a) > StashSortTierOf(b);
		});
		const unsigned charmPage = FirstEmptyStashPage();
		Stash.SetPage(charmPage < CountStashPages ? charmPage : 0);
		for (const Item &charm : charms)
			AutoPlaceItemInStash(player, charm, true);
	}

	// Lays @p items out in FAMILY blocks (familyOf), row by row, each item in the first free rectangle
	// its size below the previous family's lowest row, on the first empty page. When that page cannot
	// take the next item, the layout carries on at the top of the next EMPTY page - never on a page of
	// other kinds (user, 2026-09-13: "when a consumables stash tab is full move other consumables to
	// another separate tab of their own, dont place them in tabs with different types of items").
	// Only a stash with no empty page left falls back to the ordinary first-fit scan, because an item
	// in the wrong tab is still better than an item lost.
	const auto placeInBlocks = [&player](const std::vector<Item> &items, const auto &familyOf) {
		if (items.empty())
			return;
		unsigned page = FirstEmptyStashPage();
		if (page >= CountStashPages) {
			for (const Item &item : items)
				AutoPlaceItemInStash(player, item, true);
			return;
		}
		// The grid is a map, so asking for a page creates it; every cell of a fresh page reads free.
		const auto fits = [](unsigned onPage, Point topLeft, Size size) {
			if (topLeft.x + size.width > StashGridColumns || topLeft.y + size.height > StashGridRows)
				return false;
			const StashStruct::StashGrid &grid = Stash.stashGrids[onPage];
			for (Point p : PointsInRectangle(Rectangle { topLeft, size })) {
				if (grid[p.x][p.y] != 0)
					return false;
			}
			return true;
		};
		const auto placeFrom = [&fits](unsigned onPage, int top, const Item &item, int &bottom) {
			const Size size = GetInventorySize(item);
			for (int y = top; y < StashGridRows; y++) {
				for (int x = 0; x < StashGridColumns; x++) {
					if (!fits(onPage, { x, y }, size))
						continue;
					PlaceStashItemAt(onPage, { x, y }, item, size);
					bottom = std::max(bottom, y + size.height);
					return true;
				}
			}
			return false;
		};
		int familyTop = 0;    // the row the current family's scan starts on
		int familyBottom = 0; // one past the lowest row the current family has used
		int lastFamily = -1;
		for (const Item &item : items) {
			const int itemFamily = familyOf(item);
			if (itemFamily != lastFamily) {
				familyTop = familyBottom;
				lastFamily = itemFamily;
			}
			if (placeFrom(page, familyTop, item, familyBottom))
				continue;
			const unsigned next = FirstEmptyStashPageFrom(page + 1);
			if (next < CountStashPages) {
				page = next;
				familyTop = 0;
				familyBottom = 0;
				if (placeFrom(page, familyTop, item, familyBottom))
					continue;
			}
			AutoPlaceItemInStash(player, item, true); // no empty page left: anywhere it fits
		}
	};

	// ---------------------------------------------------------------------------------------
	// The material page
	// ---------------------------------------------------------------------------------------
	//
	// Runes across the TOP, El to Zod, left to right: three full rows of ten and a fourth of three,
	// which is exactly the 33 the ladder holds. Gems from the BOTTOM UP, one column per type, best
	// quality on the lowest row and worsening upward - so Perfects line the floor of the page.
	//
	// Both blocks are one box per kind, which only works because runes and gems already stack
	// (Item::isStackableConsumable, 2026-08-16). A kind that has overflowed past MaxStackCount into
	// a second box is placed in the gap between the two blocks rather than being allowed to shove
	// the grid out of alignment - the layout is the point, and a 34th rune box is not worth losing
	// it over.
	// MERGE PARTIAL STACKS FIRST (user report, 2026-08-20: "i've got two stacks of white scales
	// that dont want to stack").
	//
	// The stash has no merge on deposit - AutoPlaceItemInStash finds a free cell, it does not look
	// for a stack to join - so two half-stacks of the same material can sit in it indefinitely. The
	// layout below then makes that visible rather than merely wasteful: it allots ONE cell per kind,
	// so the second stack of White Scales cannot have the white column and gets pushed into the
	// overflow band above, which is exactly where the report's screenshot shows it.
	//
	// Merging here rather than at deposit time is deliberate: SORT is the one moment the player has
	// asked for the stash to be tidied, and it is the only place that already rebuilds every stack's
	// position from scratch. Capped at MaxStackCount, so a kind that genuinely overflows still ends
	// up with a full stack plus a remainder, and the overflow band still catches the remainder.
	for (size_t i = 0; i < materials.size(); i++) {
		if (materials[i].isEmpty())
			continue;
		for (size_t j = i + 1; j < materials.size(); j++) {
			if (materials[j].isEmpty() || !materials[i].canStackWith(materials[j]))
				continue;
			const int room = Item::MaxStackCount - materials[i].stackCount();
			if (room <= 0)
				break;
			const int moved = std::min(room, materials[j].stackCount());
			materials[i].setStackCount(materials[i].stackCount() + moved);
			if (moved == materials[j].stackCount())
				materials[j].clear();
			else
				materials[j].setStackCount(materials[j].stackCount() - moved);
		}
	}
	materials.erase(std::remove_if(materials.begin(), materials.end(),
	                    [](const Item &item) { return item.isEmpty(); }),
	    materials.end());

	std::vector<Item> materialSpill; // socketables the material page could not hold, for a page of their own
	if (!materials.empty()) {
		const unsigned page = FirstEmptyStashPage();
		if (page >= CountStashPages) {
			// No empty page. Fall back to the ordinary scan so nothing is lost.
			for (const Item &item : materials)
				AutoPlaceItemInStash(player, item, true);
		} else {
			constexpr int RuneRows = 4;
			// Gems occupy the bottom GemQualityCount rows; quality 0 (Chipped) is the highest of
			// them and Perfect lands on the last row of the grid.
			constexpr int GemTopRow = StashGridRows - static_cast<int>(oracool::GemQualityCount);
			static_assert(GemTopRow > RuneRows, "the rune and gem blocks would overlap");

			// The seven salvage materials had a row between the two blocks here (2026-08-20). They
			// cannot go in a socket, so since 2026-09-13 they sort with the unsocketable consumables
			// on the next page, still white to dark grey in one family block.

			// Jewels: three rows directly ABOVE the gems, one row per grade, one column per
			// family. Flawed on top and Radiant on the bottom, so the reading order down the block
			// is worst to best - the same direction the gem columns run, and the opposite of the
			// runes above, which run best-downward from El. Sitting them flush against the gem
			// block groups all three socket families at the foot of the page.
			//
			// Column and row both come out of the id, so this cannot drift from the enum: the
			// generator emits the fifteen grade-major, five families per grade.
			constexpr int JewelColumns = 5;
			constexpr int JewelRows = 3;
			constexpr int JewelTopRow = GemTopRow - JewelRows;
			static_assert(JewelTopRow > RuneRows, "the jewel block would collide with the rune block");
			static_assert(JewelColumns <= StashGridColumns, "a jewel grade must fit across one row");
			static_assert(JewelColumns * JewelRows == IDI_ORACOOL_JEWEL_WARDING_RADIANT - IDI_ORACOOL_JEWEL_FERVOR_FLAWED + 1,
			    "the jewel block is not the size of the jewel family");

			std::vector<Item> overflow;
			// One flag per cell, so a second stack of the same kind is detected rather than
			// silently overwriting the first - the failure that would make a Zod vanish.
			bool taken[StashGridColumns][StashGridRows] = {};

			for (const Item &item : materials) {
				Point cell { -1, -1 };
				if (IsOracoolRuneIdx(item.IDidx)) {
					for (size_t p = 0; p < oracool::RuneLadderSize(); p++) {
						if (oracool::RuneAtLadderPosition(p) != item.IDidx)
							continue;
						cell = { static_cast<int>(p % StashGridColumns), static_cast<int>(p / StashGridColumns) };
						break;
					}
				} else if (IsOracoolJewelIdx(item.IDidx)) {
					const int offset = item.IDidx - IDI_ORACOOL_JEWEL_FERVOR_FLAWED;
					cell = { offset % JewelColumns, JewelTopRow + offset / JewelColumns };
				} else {
					oracool::GemType type;
					oracool::GemQuality quality;
					if (oracool::GemTypeAndQuality(item.IDidx, type, quality))
						cell = { static_cast<int>(type), GemTopRow + static_cast<int>(quality) };
				}

				if (cell.x < 0 || taken[cell.x][cell.y]) {
					overflow.push_back(item);
					continue;
				}
				taken[cell.x][cell.y] = true;
				PlaceMaterialAt(page, cell, item);
			}

			// The band between the two blocks, filled left to right, top to bottom.
			int slot = 0;
			for (const Item &item : overflow) {
				bool placed = false;
				for (; slot < (GemTopRow - RuneRows) * StashGridColumns; slot++) {
					const Point cell { slot % StashGridColumns, RuneRows + slot / StashGridColumns };
					if (taken[cell.x][cell.y])
						continue;
					taken[cell.x][cell.y] = true;
					PlaceMaterialAt(page, cell, item);
					placed = true;
					slot++;
					break;
				}
				// The band is full too - an extreme case. What is left goes on a page of its own
				// after this one, never among other kinds (user, 2026-09-13).
				if (!placed)
					materialSpill.push_back(item);
			}
		}
	}
	placeInBlocks(materialSpill, [](const Item &) { return 0; });

	// THE UNSOCKETABLE CONSUMABLES on a page of their own, the one after the socketables (user,
	// 2026-09-13: "divide socketable and unsocketable consumables in different tabs, one after the
	// other. socketable consumables to be the first of the two"). They shared the material page's free
	// cells from 2026-09-05 until then. Family blocks as before - potions, elixirs, scrolls by spell,
	// books by spell, oils, the Hellfire trap runes, the salvage materials, anything else - and a page
	// that fills hands over to the next empty one (placeInBlocks). Already merged to stacks of 99.
	if (!consumables.empty()) {
		// FAMILIES, each starting on a fresh row (user, 2026-09-07: "still issues with books sorting
		// in stash" - the books, being one more misc id in the sequence, were sorted into the middle
		// of the potion run by their enum number and then wrapped across rows with everything else,
		// so a page read as potions, a book, more potions, a book). A family is a visual block now:
		// potions, elixirs, scrolls by spell, BOOKS by spell, oils, the Hellfire trap runes, and
		// anything else last. Within a family the order is the belt's (misc id) or the spell's.
		const auto family = [](const Item &item) {
			// Left to right in enum order, which IS white to dark grey (the generator emits the seven in
			// the order the user listed the colours and the salvage buttons).
			if (IsOracoolSalvageIdx(item.IDidx))
				return 6;
			if (item.isScroll())
				return 2;
			if (item._iMiscId == IMISC_BOOK)
				return 3;
			switch (item._iMiscId) {
			case IMISC_HEAL:
			case IMISC_FULLHEAL:
			case IMISC_MANA:
			case IMISC_FULLMANA:
			case IMISC_REJUV:
			case IMISC_FULLREJUV:
				return 0;
			case IMISC_ELIXSTR:
			case IMISC_ELIXMAG:
			case IMISC_ELIXDEX:
			case IMISC_ELIXVIT:
			case IMISC_SPECELIX:
				return 1;
			default:
				break;
			}
			if (item._iMiscId > IMISC_OILFIRST && item._iMiscId < IMISC_OILLAST)
				return 4;
			if (item._iMiscId > IMISC_RUNEFIRST && item._iMiscId < IMISC_RUNELAST)
				return 5;
			return 7;
		};
		// Wide enough that no id of one family reaches into the next: item indices run past a thousand.
		const auto kindKey = [&family](const Item &item) {
			const int f = family(item);
			if (f == 2 || f == 3)
				return f * 100000 + static_cast<int>(item._iSpell);
			if (f == 6)
				return f * 100000 + static_cast<int>(item.IDidx);
			return f * 100000 + static_cast<int>(item._iMiscId);
		};
		std::stable_sort(consumables.begin(), consumables.end(), [&kindKey](const Item &a, const Item &b) {
			return kindKey(a) < kindKey(b);
		});
		// Footprints, not cells (2026-09-07: "overlapping adjacent books and when second row of
		// adjacent family occurs"): each item takes the first free rectangle its size, and a family
		// starts under the whole of the one before it, books included. See placeInBlocks.
		placeInBlocks(consumables, family);
	}

	// Back to the first page. SortStash used to set it once, before placing anything, and leaving
	// it there was free; the per-tier seating above moves it as a side effect of placing, so the
	// player would otherwise be looking at whichever page the last tier happened to land on.
	if (StashUnitCount(Stash.stashList) != unitsBefore) {
		Stash.stashList = keptList;
		Stash.stashGrids = keptGrids;
		Stash.SetPage(keptPage);
		Stash.dirty = keptDirty;
		return false;
	}
	Stash.SetPage(0);
	Stash.dirty = true;
	if (&player == MyPlayer)
		oracool::ScheduleAutoSaveForStashChange();
	return true;
}

} // namespace devilution
