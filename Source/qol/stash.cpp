#include "qol/stash.h"

#include <cstdint>
#include <utility>

#include <fmt/format.h>

#include "DiabloUI/text_input.hpp"
#include "control.h"
#include "controls/plrctrls.h"
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
#include "oracool/inventory_layout.h" // CellPx / GridOrigin - the grid this one must match
#include "oracool/ornate_border.h"
#include "oracool/salvage.h"
#include "oracool/socket_overlay.h"
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

// Oracool V1: the shared theme and geometry - the same 340x720 window, title band and separator as
// the inventory, character sheet, quest log, waypoint list and Abilities window.
//   18..56     title band, "STASH" (oracool::PanelTitleTop/Height)
//   61..87     page label, "Page N / M"
//   92..118    nav row: << <          > >>
//   123..149   gold row: SORT on the left, the total on the right
//   155..161   the grid's carved frame
//   161..654   the item grid
//   654..720   clear - below y=660 the central HUD begins
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
constexpr int StashPageLabelY = oracool::PanelTitleTop + oracool::PanelTitleHeight + StashControlGap;
constexpr int StashPageRowY = StashPageLabelY + StashControlLineHeight + StashControlGap;
constexpr int StashPageRowHeight = StashControlLineHeight;
// 46x32, not 40x26: the legacy bevel draws INSIDE its rect (user, 2026-09-04: "apply legacy text box
// borders for stash nav buttons"), so the button grew by the bevel on every side and the face the
// glyph sits on is the same 40x26 it was. The 3px it overhangs the row either way is inside the
// 5px StashControlGap.
// 28x18 faces since 2026-09-05 (user: "reduce size of NAV buttons"), with the 12px glyphs; were 40x26 under the 24px face.
constexpr Size ButtonSize { 28 + 2 * oracool::LegacyTextBoxBevel, 18 + 2 * oracool::LegacyTextBoxBevel };
/** The buttons fill their row now, so this is the row's own y. */
constexpr int StashButtonY = StashPageRowY + (StashPageRowHeight - ButtonSize.height) / 2;

constexpr int StashGoldRowY = StashPageRowY + StashPageRowHeight + StashControlGap;
constexpr int StashGoldRowHeight = StashControlLineHeight;

/**
 * @brief Contains mappings for the four page-navigation buttons.
 *
 * Index 2 used to be a fifth button here (originally Withdraw Gold, then Sort). It is drawn as a
 * text button now, like the character sheet's RESET and the inventory's sort tab, so it no longer
 * needs a slot in this table - but the ORDER of the remaining four still matters, because
 * StashButtonPressed indexes both this and the nav-button art.
 */
constexpr Rectangle StashButtonRect[] = {
	// RE-SPACED for the 40px button (user, 2026-09-03: "rearrange them accordingly afterwards"). The
	// old xs were the 2026-08-16 positions - each pair moved two grid columns inward to open the
	// middle of the row - and at 27px wide they had a 5px gap between the members of a pair. At 40
	// they would have overlapped, which is the whole of the rearranging.
	//
	// Anchored to the GRID rather than to literals now: the outer pair sits flush with the grid's
	// left and right edges, the inner pair 4px in from it. That keeps the row aligned with the
	// columns beneath it the way SORT and the gold total already are, and it stays symmetric about
	// the panel's centre by construction rather than by two numbers happening to mirror.
	// clang-format off
	{ { StashColumnX(1),                            StashButtonY }, ButtonSize }, // 10 left
	{ { StashColumnX(1) + ButtonSize.width + 4,     StashButtonY }, ButtonSize }, // 1 left
	{ { StashColumnX(11) - 2 * ButtonSize.width - 4, StashButtonY }, ButtonSize }, // 1 right
	{ { StashColumnX(11) - ButtonSize.width,        StashButtonY }, ButtonSize }  // 10 right
	// clang-format on
};
static_assert(StashButtonRect[1].position.x + ButtonSize.width < StashButtonRect[2].position.x,
    "the two nav-button pairs now meet in the middle of the row - narrow ButtonSize");
constexpr int StashNavButtonCount = 4;
/** @brief Drawn as text, in the same order as StashButtonRect. */
constexpr const char *StashNavLabel[StashNavButtonCount] = { "<<", "<", ">", ">>" };

/** @brief Shared with the inventory grid - see oracool::ThemeGridLineColor for the reasoning. */
constexpr uint8_t StashGridLineColor = oracool::ThemeGridLineColor;


/**
 * @brief The page number, centred over the grid on its own line above the nav buttons.
 *
 * User request (2026-08-16): "Move page 1/100 one row up (around 28px)" - the label took its own
 * line and the buttons stayed below it. StashPageLabelY now leads the stack rather than being
 * derived backwards from the button row; see the stack comment above.
 *
 * Spans the GRID's full width since 2026-09-03, where it used to be 156px starting at 92. It is
 * centred text, so the width only decides where it can overflow to - and at FontSize24 "Page 100 /
 * 100" no longer fits in 156.
 */
constexpr Rectangle StashPageLabelRect { { StashColumnX(1), StashPageLabelY },
	{ StashColumnX(11) - StashColumnX(1), StashPageRowHeight } };

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
constexpr Rectangle GoldDisplayRect { { StashColumnX(11) - 7 * StashCellPx, StashGoldRowY },
	{ 7 * StashCellPx, StashGoldRowHeight } };
bool GoldDisplayPressed = false;

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
constexpr Rectangle StashSortButtonRect { { StashColumnX(1), StashGoldRowY },
	{ 3 * StashCellPx, StashGoldRowHeight } };

static_assert(StashSortButtonRect.position.x + StashSortButtonRect.size.width <= GoldDisplayRect.position.x,
    "The SORT button now runs into the gold readout");
static_assert(GoldDisplayRect.position.x + GoldDisplayRect.size.width <= StashColumnX(11),
    "The gold readout runs past the last grid column");

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
static_assert(StashGridTop - oracool::GridFrameWidth >= StashGoldRowY + StashGoldRowHeight,
    "the stash grid's carved frame now starts inside the gold readout above it");
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

std::optional<Point> FindTargetSlotUnderItemCursor(Point cursorPosition, Size itemSize)
{
	for (auto point : StashGridRange) {
		Rectangle cell {
			GetStashSlotCoord(point),
			InventorySlotSizeInPixels + 1
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
			InventorySlotSizeInPixels + 1
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
		holdItem._iStatFlag = player.CanUseItem(holdItem);
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
	if (itemId == StashStruct::EmptyCell) {
		return;
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
	if (&player == MyPlayer)
		oracool::ScheduleAutoSaveForStashChange();
}

int StashButtonPressed = -1;
/** @brief Set while SORT is held, so it can be drawn pressed - it is text, not art. */
bool StashSortPressed = false;

void CheckStashButtonRelease(Point mousePosition)
{
	if (GoldDisplayPressed) {
		Rectangle goldRect = GoldDisplayRect;
		goldRect.position = GetPanelPosition(UiPanels::Stash, goldRect.position);
		if (goldRect.contains(mousePosition))
			StartGoldWithdraw();
		GoldDisplayPressed = false;
	}

	if (StashSortPressed) {
		Rectangle sortRect = StashSortButtonRect;
		sortRect.position = GetPanelPosition(UiPanels::Stash, sortRect.position);
		if (sortRect.contains(mousePosition)) {
			// Oracool: user request - was withdraw gold (moved to clicking the gold total itself,
			// see GoldDisplayRect); this control is Sort. IS_ISHIEL is the sound normally played
			// when placing a shield into its equip slot, per the user's request.
			SortStash(*MyPlayer);
			PlaySFX(IS_ISHIEL);
		}
		StashSortPressed = false;
	}

	if (StashButtonPressed == -1)
		return;

	Rectangle stashButton = StashButtonRect[StashButtonPressed];
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
	Rectangle goldRect = GoldDisplayRect;
	goldRect.position = GetPanelPosition(UiPanels::Stash, goldRect.position);
	if (goldRect.contains(mousePosition)) {
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
		Rectangle stashButton = StashButtonRect[i];
		stashButton.position = GetPanelPosition(UiPanels::Stash, stashButton.position);
		if (stashButton.contains(mousePosition)) {
			StashButtonPressed = i;
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
	if (oracool::HasSidePanelArt()) {
		oracool::DrawSidePanelArt(out, panel.position);
	} else {
		oracool::DrawThemedFill(out, panel);
		oracool::DrawOrnateBorder(out, panel);
	}

	// User request (2026-08-16): the title sits 12px from the panel's top edge, and the gold rule
	// that used to run under it is gone - the painted background brings its own header framing, so
	// the separator was a second line drawn across the first.
	//
	// The band is exactly one line tall so VerticalCenter cannot drift it: 12 means 12.
	const Rectangle labelArea { { panel.position.x + StashMargin, panel.position.y + oracool::PanelTitleTop },
		{ panel.size.width - 2 * StashMargin, oracool::PanelTitleHeight } };
	oracool::DrawOutlinedString(out, _("STASH"), labelArea,
	    UiFlags::ColorWhitegold | UiFlags::FontSize30 | UiFlags::AlignCenter | UiFlags::VerticalCenter);

	// Bug fix: the four page arrows were INVISIBLE until pressed. data\stashnavbtns.clx only holds
	// each button's pressed frame - the unpressed state was painted into data\stash.clx, which the
	// theme replaced, so nothing drew them at rest. They are text now, like SORT beside them and
	// RESET on the character sheet, which also drops the last dependency on that CEL.
	for (int i = 0; i < StashNavButtonCount; i++) {
		const Rectangle rect { GetPanelPosition(UiPanels::Stash, StashButtonRect[i].position), StashButtonRect[i].size };
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
		DrawString(out, StashNavLabel[i], rect,
		    { UiFlags::AlignCenter | UiFlags::VerticalCenter | UiFlags::FontSize12 // 12 with the smaller buttons (user, 2026-09-05: "reduce size of NAV buttons")
		        | (StashButtonPressed == i ? UiFlags::ColorWhite : UiFlags::ColorGold)
		        | UiFlags::Shadowed });
	}

	// One bevelled recess around the whole grid, the way the inventory frames its own.
	const Rectangle gridRect { GetPanelPosition(UiPanels::Stash, { StashGridLeft, StashGridTop }),
		{ StashGridWidth, StashGridRows * StashCellPx } };
	oracool::DrawThemedFill(out, gridRect, 2);
	// Outside the cells, matching the inventory. The stash needed no repositioning for either frame:
	// its grid already had margin on every side, and it absorbed the carved bezel's extra three
	// pixels without losing a row - see the StashGridBottom assert.
	if (oracool::HasGridBezel(gridRect.size)) {
		oracool::DrawDropShadow(out, gridRect, oracool::GridBezelInset); // the slot shadow (2026-09-05)
		oracool::DrawGridBezel(out, gridRect);
	} else {
		oracool::DrawOrnateBorderOutside(out, gridRect);
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
	DrawString(out, fmt::format(fmt::runtime(_("Page {:d} / {:d}")), Stash.GetPage() + 1, CountStashPages),
	    { position + Displacement { StashPageLabelRect.position.x, StashPageLabelRect.position.y }, StashPageLabelRect.size },
	    { UiFlags::AlignCenter | UiFlags::VerticalCenter | UiFlags::ColorGold | UiFlags::FontSize24 | UiFlags::Shadowed });

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
	DrawString(out, StrCat(_("GOLD: "), FormatInteger(Stash.gold)),
	    { position + Displacement { GoldDisplayRect.position.x, GoldDisplayRect.position.y }, GoldDisplayRect.size },
	    // The 12px face for both (user, 2026-09-05: "reduce font size in stash of SORT, GOLD").
	    { UiFlags::ColorWhitegold | UiFlags::AlignRight | UiFlags::VerticalCenter | UiFlags::FontSize12 | UiFlags::Shadowed });

	// Left-aligned for the mirror reason: the word starts on column 1's left edge.
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
			InventorySlotSizeInPixels + 1
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

	if (item->_iMiscId == IMISC_BOOK)
		PlaySFX(IS_RBOOK);
	else
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
	// Iterate through stashGrid and remove every reference to item
	for (auto &row : Stash.GetCurrentGrid()) {
		for (StashStruct::StashCell &itemId : row) {
			if (itemId - 1 == iv) {
				itemId = 0;
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
}

void StashStruct::NextPage(unsigned offset)
{
	if (page <= LastStashPage) {
		page += std::min(offset, LastStashPage - page);
	} else {
		page = LastStashPage;
	}
	dirty = true;
}

void StashStruct::PreviousPage(unsigned offset)
{
	if (page <= LastStashPage) {
		page -= std::min(offset, page);
	} else {
		page = LastStashPage;
	}
	dirty = true;
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

	const Point start = GetPanelPosition(UiPanels::Stash, { 67, 128 });
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

void DrawGoldWithdraw(const Surface &out)
{
	if (!IsWithdrawGoldOpen) {
		return;
	}

	const string_view amountText = GoldWithdrawText;
	const TextInputCursorState &cursor = GoldWithdrawCursor;

	const int dialogX = 30;

	ClxDraw(out, GetPanelPosition(UiPanels::Stash, { dialogX, 178 }), (*pGBoxBuff)[0]);

	// Pre-wrap the string at spaces, otherwise DrawString would hard wrap in the middle of words
	const std::string wrapped = WordWrapString(_("How many gold pieces do you want to withdraw?"), 200);

	// The split gold dialog is roughly 4 lines high, but we need at least one line for the player to input an amount.
	// Using a clipping region 50 units high (approx 3 lines with a lineheight of 17) to ensure there is enough room left
	//  for the text entered by the player.
	DrawString(out, wrapped, { GetPanelPosition(UiPanels::Stash, { dialogX + 31, 75 }), { 200, 50 } },
	    { UiFlags::ColorWhitegold | UiFlags::AlignCenter, 1, 17 });

	// Even a ten digit amount of gold only takes up about half a line. There's no need to wrap or clip text here so we
	// use the Point form of DrawString.
	DrawString(out, amountText, GetPanelPosition(UiPanels::Stash, { dialogX + 37, 128 }),
	    TextRenderOptions {
	        /*flags=*/UiFlags::ColorWhite | UiFlags::PentaCursor,
	        /*spacing=*/1,
	        /*lineHeight=*/-1,
	        /*cursorPosition=*/static_cast<int>(cursor.position),
	        /*highlightRange=*/ { static_cast<int>(cursor.selection.begin), static_cast<int>(cursor.selection.end) },
	    });
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

bool AutoPlaceItemInStash(Player &player, const Item &item, bool persistItem)
{
	if (!IsItemAllowedInStash(item))
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
void PlaceMaterialAt(unsigned page, Point cell, const Item &item)
{
	Stash.stashList.emplace_back(item);
	const auto index = static_cast<uint16_t>(Stash.stashList.size() - 1);
	Stash.stashList[index].position = cell;
	AddItemToStashGrid(page, cell, index, { 1, 1 });
}

/** @brief The first page with nothing on it, searching upward from 0. */
unsigned FirstEmptyStashPage()
{
	for (unsigned page = 0; page < CountStashPages; page++) {
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

} // namespace

void SortStash(Player &player)
{
	struct SortEntry {
		Item item;
		int categoryRank;
		int value;
	};
	std::vector<SortEntry> entries;
	std::vector<Item> materials;
	std::vector<Item> consumables; // potions, elixirs, scrolls - their own page on SORT (2026-09-05)
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
		if (IsOracoolRuneIdx(item.IDidx) || IsOracoolGemIdx(item.IDidx) || IsOracoolSalvageIdx(item.IDidx)
		    || IsOracoolJewelIdx(item.IDidx)) {
			materials.push_back(item);
			continue;
		}
		if (item.isStackableConsumable()) {
			consumables.push_back(item);
			continue;
		}
		entries.push_back({ item, StashSortCategoryRank(item), GetItemSellValue(item) });
	}

	std::stable_sort(entries.begin(), entries.end(), [](const SortEntry &a, const SortEntry &b) {
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

	for (const SortEntry &entry : entries)
		AutoPlaceItemInStash(player, entry.item, true);

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

	unsigned materialsPage = CountStashPages; // the page the materials took, for the consumables to share
	if (!materials.empty()) {
		const unsigned page = FirstEmptyStashPage();
		materialsPage = page;
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

			// The seven salvage materials get a row of their own between the two blocks (user,
			// 2026-08-20: "land them in a row of their own sorted left to right from white to
			// darkgey somewhere inbetween runes and bems").
			//
			// Row 6 rather than a computed midpoint: it leaves rows 4-5 free directly under the
			// runes, which is where rune overflow lands, so the commonest overflow case never has
			// to step over the material row to find space.
			//
			// Left to right in enum order, which IS white to dark grey - the generator emits the
			// seven in the order the user listed both the colours and the salvage buttons, so
			// "IDidx - the first material" is the column, and no second ordering table can drift
			// from the first.
			constexpr int SalvageRow = 6;
			static_assert(SalvageRow > RuneRows && SalvageRow < GemTopRow,
			    "the salvage row must sit between the rune and gem blocks");
			static_assert(oracool::SalvageTierCount <= StashGridColumns,
			    "the seven materials must fit across one row");

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
			static_assert(JewelTopRow > SalvageRow, "the jewel block would collide with the salvage row");
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
				} else if (IsOracoolSalvageIdx(item.IDidx)) {
					cell = { item.IDidx - IDI_ORACOOL_SALVAGE_WHITE_SCALES, SalvageRow };
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
				// The band is full too - an extreme case, but losing the item is not an option.
				if (!placed)
					AutoPlaceItemInStash(player, item, true);
			}
		}
	}

	Stash.dirty = true;
	if (&player == MyPlayer)
		oracool::ScheduleAutoSaveForStashChange();

	// THE CONSUMABLES on the MATERIALS page (user, 2026-09-05: "find a unallocated slot on the
	// runes/gems dedicated tab and we keep all of them there"). The materials' layout allots its
	// blocks - runes, salvage, jewels, gems - and leaves cells free between and beside them; the
	// consumables take those free cells in row order, kind by kind - potions and elixirs in the
	// belt's own order, then scrolls by spell, then the runes and oils - one cell per stack.
	// Already merged to stacks of 99 above, so a kind takes as few cells as it can. If there is no
	// materials page (nothing to sort there) they take the next empty page; what the page cannot
	// hold falls to the ordinary first-fit placement.
	if (!consumables.empty()) {
		const auto kindKey = [](const Item &item) {
			// Potions and elixirs by misc id (heal, full heal, mana, full mana, rejuvenation...),
			// scrolls after them by spell - so the belt's order reads across the page.
			if (item.isScroll())
				return 1000 + static_cast<int>(item._iSpell);
			return static_cast<int>(item._iMiscId);
		};
		std::stable_sort(consumables.begin(), consumables.end(), [&kindKey](const Item &a, const Item &b) {
			return kindKey(a) < kindKey(b);
		});
		const unsigned page = materialsPage < CountStashPages ? materialsPage : FirstEmptyStashPage();
		if (page >= CountStashPages) {
			for (const Item &item : consumables)
				AutoPlaceItemInStash(player, item, true);
		} else {
			// Free cells in row order. The page's grid may not exist yet when nothing was placed
			// on it; then every cell is free.
			const auto cellFree = [page](int x, int y) {
				return page >= Stash.stashGrids.size() || Stash.stashGrids[page][x][y] == 0;
			};
			int cell = 0;
			for (const Item &item : consumables) {
				while (cell < StashGridColumns * StashGridRows && !cellFree(cell % StashGridColumns, cell / StashGridColumns))
					cell++;
				if (cell >= StashGridColumns * StashGridRows) {
					AutoPlaceItemInStash(player, item, true);
					continue;
				}
				PlaceMaterialAt(page, { cell % StashGridColumns, cell / StashGridColumns }, item);
				cell++;
			}
		}
	}
}

} // namespace devilution
