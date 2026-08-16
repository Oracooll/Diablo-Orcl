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
#include "oracool/inventory_layout.h" // CellPx / GridOrigin - the grid this one must match
#include "oracool/ornate_border.h"
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
//   0..24      top margin
//   24..74     label band, "STASH"
//   74..77     separator rule
//   77..101    gap below the rule
//   101..127   page row: << < page N > >>
//   131..153   gold row: the total on the left, SORT on the right
//   161..654   the item grid
//   654..720   clear - below y=660 the central HUD begins
constexpr Size StashPanelSize { 340, 720 };
constexpr int StashMargin = 24;
constexpr int StashLabelHeight = 50;
// The title band is oracool::PanelTitleTop / PanelTitleHeight - shared by all five side panels so
// two open side by side line up. See oracool/ornate_border.h.
constexpr int StashContentTop = StashMargin + StashLabelHeight + oracool::OrnateBorderWidth + StashMargin;

constexpr Size ButtonSize { 27, 16 };
constexpr int StashPageRowY = StashContentTop;
constexpr int StashPageRowHeight = 26;
/** Buttons are shorter than their row, so they sit centred in it. */
constexpr int StashButtonY = StashPageRowY + (StashPageRowHeight - ButtonSize.height) / 2;

constexpr int StashGoldRowY = StashPageRowY + StashPageRowHeight + 4;
constexpr int StashGoldRowHeight = 22;

/**
 * @brief Contains mappings for the four page-navigation buttons.
 *
 * Index 2 used to be a fifth button here (originally Withdraw Gold, then Sort). It is drawn as a
 * text button now, like the character sheet's RESET and the inventory's sort tab, so it no longer
 * needs a slot in this table - but the ORDER of the remaining four still matters, because
 * StashButtonPressed indexes both this and the nav-button art.
 */
constexpr Rectangle StashButtonRect[] = {
	// clang-format off
	{ {  25, StashButtonY }, ButtonSize }, // 10 left
	{ {  57, StashButtonY }, ButtonSize }, // 1 left
	{ { 256, StashButtonY }, ButtonSize }, // 1 right
	{ { 288, StashButtonY }, ButtonSize }  // 10 right
	// clang-format on
};
constexpr int StashNavButtonCount = 4;
/** @brief Drawn as text, in the same order as StashButtonRect. */
constexpr const char *StashNavLabel[StashNavButtonCount] = { "<<", "<", ">", ">>" };

/** @brief Shared with the inventory grid - see oracool::ThemeGridLineColor for the reasoning. */
constexpr uint8_t StashGridLineColor = oracool::ThemeGridLineColor;

/** @brief The page number, between the two pairs of navigation buttons. */
constexpr Rectangle StashPageLabelRect { { 92, StashPageRowY }, { 156, StashPageRowHeight } };

/**
 * @brief Oracool: user request - the gold total's on-screen area is the click target for
 * withdrawing gold, which is what freed the old button slot up to become Sort.
 */
constexpr Rectangle GoldDisplayRect { { 25, StashGoldRowY }, { 180, StashGoldRowHeight } };
bool GoldDisplayPressed = false;

/** @brief Drawn as a word rather than art, matching RESET on the character sheet. */
constexpr Rectangle StashSortButtonRect { { 258, StashGoldRowY }, { 57, StashGoldRowHeight } };

constexpr Size StashGridSize { StashGridColumns, StashGridRows };
constexpr PointsInRectangleRange<int> StashGridRange { { { 0, 0 }, StashGridSize } };

/**
 * @brief Cell pitch. The same 28 the inventory grid uses (oracool::CellPx) - user request, 2026-08-16.
 *
 * This was INV_SLOT_SIZE_PX + 1: a 28px slot plus a dedicated pixel for its dividing rule, so no
 * slot lost any of its 28. The comment claimed that matched the inventory grid; it did not. The
 * inventory draws the same 1px ThemeGridLineColor rule at `c * CellPx - 1` and simply lets the item
 * sprite overdraw it, which costs the boundary pixel of one cell and keeps the pitch at 28.
 *
 * Matching the pitch is what makes the two grids line up: StashGridLeft now lands on 30, the same
 * x as the inventory's GridOrigin, and a 2x3 item spans the same 56x84 in both - which is also the
 * span the item backing paints, so backings sit flush in the stash instead of 1px inside the cell.
 *
 * Costs 10px of width and 16px of height, which the panel has: see the health-orb asserts below.
 */
constexpr int StashCellPx = INV_SLOT_SIZE_PX;
constexpr int StashGridWidth = StashGridColumns * StashCellPx;
constexpr int StashGridTop = StashGoldRowY + StashGoldRowHeight + 8;
/** @brief Centred across the panel. */
constexpr int StashGridLeft = (StashPanelSize.width - StashGridWidth) / 2;
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
// The FRAME is what has to clear the orb, not the cells - the bevel is drawn outside the grid now
// (DrawOrnateBorderOutside), so it reaches OrnateBorderWidth past the last row.
static_assert(StashGridBottom - 1 + oracool::OrnateBorderWidth <= StashHealthOrbTop,
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
		return 6; // Others
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
	// Flush to the top-left corner, like the character sheet, quest log and waypoint list - the
	// stash shares the left-hand slot with them and opens with the inventory on the right.
	return { { 0, 0 }, StashPanelSize };
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
		DrawString(out, StashNavLabel[i], rect,
		    { UiFlags::AlignCenter | UiFlags::VerticalCenter
		        | (StashButtonPressed == i ? UiFlags::ColorWhite : UiFlags::ColorGold) });
	}

	// One bevelled recess around the whole grid, the way the inventory frames its own.
	const Rectangle gridRect { GetPanelPosition(UiPanels::Stash, { StashGridLeft, StashGridTop }),
		{ StashGridWidth, StashGridRows * StashCellPx } };
	oracool::DrawThemedFill(out, gridRect, 2);
	// Outside the cells, matching the inventory - see DrawOrnateBorderOutside. The stash needed no
	// repositioning to afford it: its grid already had margin on every side.
	oracool::DrawOrnateBorderOutside(out, gridRect);

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

		if (pcursstashitem == itemId) {
			uint8_t color = GetOutlineColor(item, true);
			ClxDrawOutline(out, color, position, sprite);
		}

		DrawItem(item, out, position, sprite);
	}

	const Point position = GetPanelPosition(UiPanels::Stash);
	constexpr UiFlags Style = UiFlags::VerticalCenter | UiFlags::ColorWhite;

	DrawString(out, fmt::format(fmt::runtime(_("Page {:d} / {:d}")), Stash.GetPage() + 1, CountStashPages),
	    { position + Displacement { StashPageLabelRect.position.x, StashPageLabelRect.position.y }, StashPageLabelRect.size },
	    { UiFlags::AlignCenter | Style });

	// Gold in the theme's own gold, matching the inventory's readout, rather than the plain white
	// the vanilla panel used.
	DrawString(out, StrCat(_("GOLD: "), FormatInteger(Stash.gold)),
	    { position + Displacement { GoldDisplayRect.position.x, GoldDisplayRect.position.y }, GoldDisplayRect.size },
	    { UiFlags::ColorWhitegold | UiFlags::VerticalCenter });

	DrawString(out, _("SORT"),
	    { position + Displacement { StashSortButtonRect.position.x, StashSortButtonRect.position.y }, StashSortButtonRect.size },
	    { UiFlags::AlignCenter | UiFlags::VerticalCenter | (StashSortPressed ? UiFlags::ColorWhite : UiFlags::ColorGold) });
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
	// Oracool: user request - "Sort" tooltip over the Sort control (StashSortButtonRect,
	// formerly Withdraw Gold).
	Rectangle sortButtonRect = StashButtonRect[2];
	sortButtonRect.position = GetPanelPosition(UiPanels::Stash, sortButtonRect.position);
	if (sortButtonRect.contains(mousePosition)) {
		InfoColor = UiFlags::ColorWhite;
		InfoString = _("Sort");
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
	    /*max=*/std::min(RoomForGold(), Stash.gold),
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

	Size itemSize = GetInventorySize(item);

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
				Stash.stashList.push_back(item);
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

void SortStash(Player &player)
{
	struct SortEntry {
		Item item;
		int categoryRank;
		int value;
	};
	std::vector<SortEntry> entries;
	entries.reserve(Stash.stashList.size());
	for (const Item &item : Stash.stashList)
		entries.push_back({ item, StashSortCategoryRank(item), GetItemSellValue(item) });

	std::stable_sort(entries.begin(), entries.end(), [](const SortEntry &a, const SortEntry &b) {
		if (a.categoryRank != b.categoryRank)
			return a.categoryRank < b.categoryRank;
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

	Stash.dirty = true;
	if (&player == MyPlayer)
		oracool::ScheduleAutoSaveForStashChange();
}

} // namespace devilution
