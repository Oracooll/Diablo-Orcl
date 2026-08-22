#include "oracool/shop_grid.h"

#include <algorithm>
#include <vector>

#include <fmt/format.h>

#include "control.h"
#include "cursor.h"
#include "engine/render/clx_render.hpp"
#include "engine/render/primitive_render.hpp"
#include "engine/render/text_render.hpp"
#include "inv.h"
#include "items.h"
#include "oracool/grid_bezel.h"
#include "oracool/hud_art.h"
#include "oracool/inventory_layout.h" // GridBottom - the line the stash's grid also ends on
#include "oracool/ornate_border.h"
#include "oracool/shop_tabs.h"
#include "utils/format_int.hpp"
#include "utils/language.h"
#include "utils/str_cat.hpp"

namespace devilution::oracool {

namespace {

constexpr Size ShopPanelSize { 340, 720 };

/** @brief Ten by sixteen at the inventory pitch - the stash's grid, to the pixel. */
constexpr int ShopGridColumns = 10;
constexpr int ShopGridRows = 16;
constexpr int ShopCellPx = INV_SLOT_SIZE_PX;
constexpr int ShopGridWidth = ShopGridColumns * ShopCellPx;
constexpr int ShopGridHeight = ShopGridRows * ShopCellPx;
constexpr int ShopGridLeft = (ShopPanelSize.width - ShopGridWidth) / 2;

/**
 * @brief The grid sits as low as the stash's does, and every control is above it (user request).
 *
 * `GridBottom` is the inventory's, and the stash derives its own grid top from the same constant
 * for the same reason: the three grids are the same surface at the same pitch, and a shop grid that
 * ended anywhere else would be visibly out of line with the inventory beside it. It also puts the
 * bezel's last pixel exactly on the HUD's content edge, so there is no gap under the grid to
 * explain away.
 */
constexpr int ShopGridTop = GridBottom - ShopGridRows * ShopCellPx;

/** @brief Tab strip: rows of at most three, under the title band. */
constexpr int ShopTabTop = 58;
constexpr int ShopTabHeight = 20;
constexpr int ShopTabRowGap = 2;
/**
 * @brief Three, not four.
 *
 * Four tabs across a 340px panel is 77px each, and "Supplies" and "Recharge" both overran that at
 * FontSize12 - the first screenshot of this panel showed "SUPPLIE". Three gives 102px, which is
 * wide enough for any label the tab sets contain with room to spare.
 */
constexpr int ShopTabsPerRow = 3;
constexpr int ShopTabRows = 3;
constexpr int ShopTabStripLeft = 16;
constexpr int ShopTabStripWidth = ShopPanelSize.width - 2 * ShopTabStripLeft;

/** @brief Bulk actions and the gold readout, stacked between the tabs and the grid. */
// The two gaps are 2, not a more comfortable 4: the grid's bottom is pinned and the title band is
// fixed, so everything between them shares one fixed budget. GridFrameWidth is 6 - the carved stone
// bezel's, not the procedural bevel's 3 - and reserving the smaller number is the mistake the
// assert below exists to catch.
constexpr int ShopActionTop = ShopTabTop + ShopTabRows * (ShopTabHeight + ShopTabRowGap) + 2;
constexpr int ShopActionHeight = 20;
constexpr int ShopGoldTop = ShopActionTop + ShopActionHeight + 2;
constexpr int ShopGoldHeight = 15;

static_assert(ShopGoldTop + ShopGoldHeight <= ShopGridTop - GridFrameWidth,
    "the controls above the shop grid no longer clear it - drop a tab row or shorten the stack");
static_assert(ShopGridLeft >= 0, "the shop grid is wider than the panel");

/** @brief Where one stock entry sits on the grid, and which stock entry it is. */
struct PlacedSlot {
	int stockIndex;
	Point cell;
	Size cells;
};

/**
 * @brief The keyboard cursor, as a position in the tab's stock order.
 *
 * Stock order, not a grid cell: the stock is what the transaction is indexed by, and a cursor that
 * lived on the grid would have to answer "what is selected" for the empty cells too.
 */
int ShopGridSel = 0;

/** @brief Row-major first-fit, the same shape the stash uses to auto-place a withdrawal. */
std::vector<PlacedSlot> PlaceStock(const std::vector<ShopSlot> &stock)
{
	std::vector<PlacedSlot> placed;
	placed.reserve(stock.size());
	bool taken[ShopGridRows][ShopGridColumns] = {};

	for (size_t i = 0; i < stock.size(); i++) {
		const Size cells = GetInventorySize(*stock[i].item);
		bool done = false;
		for (int row = 0; row + cells.height <= ShopGridRows && !done; row++) {
			for (int col = 0; col + cells.width <= ShopGridColumns && !done; col++) {
				bool free = true;
				for (int dy = 0; dy < cells.height && free; dy++) {
					for (int dx = 0; dx < cells.width && free; dx++)
						free = !taken[row + dy][col + dx];
				}
				if (!free)
					continue;
				for (int dy = 0; dy < cells.height; dy++) {
					for (int dx = 0; dx < cells.width; dx++)
						taken[row + dy][col + dx] = true;
				}
				placed.push_back({ static_cast<int>(i), { col, row }, cells });
				done = true;
			}
		}
		// An item that does not fit is simply not drawn. 160 cells against a 25-item stock means
		// this cannot happen today; it is a dropped item rather than a wrapped one on purpose,
		// because a wrapped item would be drawn on top of another one's cells.
	}
	return placed;
}

Point CellOrigin(Point cell)
{
	const Rectangle grid = GetShopGridRect();
	return { grid.position.x + cell.x * ShopCellPx, grid.position.y + cell.y * ShopCellPx };
}

/**
 * @brief Where ClxDraw wants an item sprite: the pixel just past its BOTTOM row.
 *
 * ClxDraw renders upward from the point it is given, so a three-cell-tall sword anchored on its top
 * row draws 56px above the grid. The first build of this panel did exactly that, and the stock
 * climbed out over the tab strip. The inventory has always anchored the same way - see inv.cpp's
 * `+ Displacement { 0, InventorySlotSizeInPixels.height }` - it just gets there differently, because
 * AddItemToInvGrid marks an item's BOTTOM-left cell as its first slot rather than its top-left. This
 * grid packs from the top-left, so the item's own height has to be added back here.
 */
Point SpriteAnchor(const PlacedSlot &slot)
{
	const Point origin = CellOrigin(slot.cell);
	return { origin.x, origin.y + slot.cells.height * ShopCellPx };
}

/** @brief Which placed slot @p position is over, or -1. */
int PlacedSlotAt(const std::vector<PlacedSlot> &placed, Point position)
{
	const Rectangle grid = GetShopGridRect();
	if (!grid.contains(position))
		return -1;
	const Point cell { (position.x - grid.position.x) / ShopCellPx, (position.y - grid.position.y) / ShopCellPx };
	for (size_t i = 0; i < placed.size(); i++) {
		const PlacedSlot &slot = placed[i];
		if (cell.x >= slot.cell.x && cell.x < slot.cell.x + slot.cells.width
		    && cell.y >= slot.cell.y && cell.y < slot.cell.y + slot.cells.height)
			return static_cast<int>(i);
	}
	return -1;
}

/** @brief The vendor's name for the panel title - the shop is one door, so the door is named. */
const char *ShopTitle(TalkID id)
{
	switch (id) {
	case TalkID::SmithBuy:
	case TalkID::SmithPremiumBuy:
	case TalkID::SmithUniqueBuy:
	case TalkID::SmithConsumables:
	case TalkID::SmithSell:
	case TalkID::SmithRepair:
	case TalkID::SmithRecharge:
		return N_("GRISWOLD");
	case TalkID::WitchBuy:
	case TalkID::WitchSell:
	case TalkID::WitchRecharge:
		return N_("ADRIA");
	case TalkID::HealerBuy:
		return N_("PEPIN");
	default:
		return "";
	}
}

/** @brief What the price on this tab is FOR. The player is not always buying. */
const char *ShopPriceLabel(TalkID id)
{
	switch (id) {
	case TalkID::SmithSell:
	case TalkID::WitchSell:
		return N_("You get");
	case TalkID::SmithRepair:
		return N_("Repair");
	case TalkID::SmithRecharge:
	case TalkID::WitchRecharge:
		return N_("Recharge");
	default:
		return N_("Price");
	}
}

Rectangle ShopTabRect(size_t index, size_t count)
{
	const Rectangle panel = GetShopPanelRect();
	const int perRow = std::min<int>(ShopTabsPerRow, static_cast<int>(count));
	const int row = static_cast<int>(index) / ShopTabsPerRow;
	const int col = static_cast<int>(index) % ShopTabsPerRow;
	// The last row is narrower than a full one, so it gets its tabs at the SAME width and centred
	// under the row above rather than stretched to the strip's edges - stretched tabs on the short
	// row read as a different kind of control.
	const int width = ShopTabStripWidth / perRow;
	const size_t rowStart = static_cast<size_t>(row) * ShopTabsPerRow;
	const int inThisRow = static_cast<int>(std::min(count - rowStart, static_cast<size_t>(ShopTabsPerRow)));
	const int rowLeft = panel.position.x + ShopTabStripLeft + (ShopTabStripWidth - inThisRow * width) / 2;
	return Rectangle { { rowLeft + col * width, panel.position.y + ShopTabTop + row * (ShopTabHeight + ShopTabRowGap) },
		{ width, ShopTabHeight } };
}

void DrawShopTabRow(const Surface &out)
{
	const std::vector<TalkID> tabs = ShopTabsFor(stextflag);
	for (size_t i = 0; i < tabs.size(); i++) {
		const Rectangle rect = ShopTabRect(i, tabs.size());
		const bool active = tabs[i] == stextflag;
		if (active) {
			DrawThemedFill(out, rect, 2);
		} else {
			DrawHalfTransparentRectTo(out, rect.position.x, rect.position.y, rect.size.width, rect.size.height);
		}
		DrawOrnateBorder(out, rect);
		DrawString(out, _(ShopTabName(tabs[i])), rect,
		    { (active ? UiFlags::ColorWhite : UiFlags::ColorWhitegold)
		        | UiFlags::FontSize12 | UiFlags::AlignCenter | UiFlags::VerticalCenter });
	}
}

/** @brief True if the click switched tabs, so the caller stops. */
bool CheckShopTabRowClick(Point position)
{
	const std::vector<TalkID> tabs = ShopTabsFor(stextflag);
	for (size_t i = 0; i < tabs.size(); i++) {
		if (!ShopTabRect(i, tabs.size()).contains(position))
			continue;
		// The tab you are already on absorbs the click rather than restarting the screen.
		if (tabs[i] != stextflag) {
			StartStore(tabs[i]);
			ResetShopGridSelection();
		}
		return true;
	}
	return false;
}

/** @brief Bulk-action buttons share one row above the grid, at most two of them. */
Rectangle ShopActionRect(size_t index, size_t count)
{
	const Rectangle panel = GetShopPanelRect();
	const int width = ShopTabStripWidth / static_cast<int>(std::max<size_t>(count, 1));
	return Rectangle { { panel.position.x + ShopTabStripLeft + static_cast<int>(index) * width,
	                       panel.position.y + ShopActionTop },
		{ width, ShopActionHeight } };
}

/** @brief The red X, top-right, same as every other Oracool window's. */
Rectangle ShopCloseRect()
{
	const Rectangle panel = GetShopPanelRect();
	return Rectangle { { panel.position.x + panel.size.width - 34, panel.position.y + 14 }, { 20, 20 } };
}

/**
 * @brief The bulk-action row and the gold readout, both above the grid.
 *
 * The hovered item's name and price used to live down here too. They are a cursor-following popup
 * now (SetShopHoverInfoString) - the player is already looking at the icon they are hovering, and a
 * readout at the far end of the panel made them look away from it to read it.
 */
void DrawShopControls(const Surface &out)
{
	const Rectangle panel = GetShopPanelRect();

	const std::vector<ShopAction> actions = GetShopActions(stextflag);
	for (size_t i = 0; i < actions.size(); i++) {
		const Rectangle rect = ShopActionRect(i, actions.size());
		DrawHalfTransparentRectTo(out, rect.position.x, rect.position.y, rect.size.width, rect.size.height);
		DrawOrnateBorder(out, rect);
		DrawString(out, _(actions[i].label), rect,
		    { UiFlags::ColorWhite | UiFlags::FontSize12 | UiFlags::AlignCenter | UiFlags::VerticalCenter });
	}

	const Rectangle goldLine { { panel.position.x + ShopTabStripLeft, panel.position.y + ShopGoldTop },
		{ ShopTabStripWidth, ShopGoldHeight } };
	DrawString(out, fmt::format(fmt::runtime(_("Your gold: {:s}")), FormatInteger(TotalPlayerGold())), goldLine,
	    { UiFlags::ColorWhitegold | UiFlags::FontSize12 | UiFlags::AlignCenter | UiFlags::VerticalCenter });
}

void DrawShopClose(const Surface &out)
{
	const Rectangle rect = ShopCloseRect();
	DrawOrnateBorder(out, rect);
	DrawString(out, "X", rect,
	    { UiFlags::ColorRed | UiFlags::FontSize24 | UiFlags::AlignCenter | UiFlags::VerticalCenter });
}

} // namespace

bool IsShopGridScreen(TalkID id)
{
	return IsShopTab(id);
}

Rectangle GetShopPanelRect()
{
	// The top-left slot, shared with the stash, character sheet and quest log. Nothing else is open
	// while a shop is, so the slot is free.
	return Rectangle { { 0, 0 }, ShopPanelSize };
}

Rectangle GetShopGridRect()
{
	const Rectangle panel = GetShopPanelRect();
	return Rectangle { { panel.position.x + ShopGridLeft, panel.position.y + ShopGridTop },
		{ ShopGridWidth, ShopGridHeight } };
}

void ResetShopGridSelection()
{
	ShopGridSel = 0;
}

void DrawShopGrid(const Surface &out)
{
	if (!IsShopGridScreen(stextflag))
		return;

	const Rectangle panel = GetShopPanelRect();
	if (HasSidePanelArt()) {
		DrawSidePanelArt(out, panel.position);
		DrawSidePanelBackdrop(out, panel.position);
	} else {
		DrawThemedFill(out, panel);
		DrawOrnateBorder(out, panel);
	}

	const Rectangle labelArea { { panel.position.x + 16, panel.position.y + PanelTitleTop },
		{ panel.size.width - 32, PanelTitleHeight } };
	DrawOutlinedString(out, _(ShopTitle(stextflag)), labelArea,
	    UiFlags::ColorWhitegold | UiFlags::FontSize30 | UiFlags::AlignCenter | UiFlags::VerticalCenter);

	DrawShopTabRow(out);

	const Rectangle grid = GetShopGridRect();
	DrawThemedFill(out, grid, 2);
	if (HasGridBezel(grid.size)) {
		DrawGridBezel(out, grid);
	} else {
		DrawOrnateBorderOutside(out, grid);
	}
	// Same 1px dark rules as the stash, drawn on the last pixel of the preceding cell's span.
	for (int col = 1; col < ShopGridColumns; col++)
		DrawVerticalLine(out, { grid.position.x + col * ShopCellPx - 1, grid.position.y }, grid.size.height, ThemeGridLineColor);
	for (int row = 1; row < ShopGridRows; row++)
		DrawHorizontalLine(out, { grid.position.x, grid.position.y + row * ShopCellPx - 1 }, grid.size.width, ThemeGridLineColor);

	const std::vector<ShopSlot> stock = GetShopStock(stextflag);
	const std::vector<PlacedSlot> placed = PlaceStock(stock);

	// The mouse wins over the keyboard cursor while it is over an item, exactly as the inventory
	// does - otherwise the footer would describe one item while the pointer sits on another.
	int hoveredPlaced = PlacedSlotAt(placed, MousePosition);
	if (hoveredPlaced >= 0)
		ShopGridSel = placed[hoveredPlaced].stockIndex;
	if (ShopGridSel >= static_cast<int>(stock.size()))
		ShopGridSel = stock.empty() ? 0 : static_cast<int>(stock.size()) - 1;

	for (const PlacedSlot &slot : placed) {
		const Item &item = *stock[slot.stockIndex].item;
		InvDrawSlotBack(out, SpriteAnchor(slot),
		    { slot.cells.width * ShopCellPx, slot.cells.height * ShopCellPx }, item);
	}

	for (const PlacedSlot &slot : placed) {
		const Item &item = *stock[slot.stockIndex].item;
		const ClxSprite sprite = GetInvItemSprite(item._iCurs + CURSOR_FIRSTITEM);
		const Point position = SpriteAnchor(slot);
		if (slot.stockIndex == ShopGridSel)
			ClxDrawOutline(out, GetOutlineColor(item, true), position, sprite);
		ClxDraw(out, position, sprite);
	}

	DrawShopControls(out);
	DrawShopClose(out);
}

bool CheckShopGridClick(Point position)
{
	if (!IsShopGridScreen(stextflag))
		return false;
	if (!GetShopPanelRect().contains(position))
		return false;
	if (ShopCloseRect().contains(position)) {
		// Out of the shop entirely, not back to the vendor's dialog - the X on every other Oracool
		// window closes the window, and the tabs are how you move between shop screens.
		stextflag = TalkID::None;
		return true;
	}
	if (CheckShopTabRowClick(position))
		return true;

	const std::vector<ShopAction> actions = GetShopActions(stextflag);
	for (size_t i = 0; i < actions.size(); i++) {
		if (ShopActionRect(i, actions.size()).contains(position)) {
			ShopActivateAction(stextflag, actions[i].line);
			return true;
		}
	}

	const std::vector<ShopSlot> stock = GetShopStock(stextflag);
	const std::vector<PlacedSlot> placed = PlaceStock(stock);
	const int hovered = PlacedSlotAt(placed, position);
	if (hovered >= 0)
		ShopSelectIndex(stextflag, stock[placed[hovered].stockIndex].index);
	// Anywhere else on the panel is absorbed: the shop covers the world, and a click on its
	// background must not walk the player into a wall behind it.
	return true;
}

void MoveShopGridSelection(int columns, int rows)
{
	const std::vector<ShopSlot> stock = GetShopStock(stextflag);
	if (stock.empty())
		return;
	// Both axes move through the STOCK order rather than the grid: the grid is packed row-major, so
	// "next" and "the item to the right" are the same step, and a row is one grid row's worth of
	// single-cell items.
	const int step = columns + rows * ShopGridColumns;
	const int count = static_cast<int>(stock.size());
	ShopGridSel = ((ShopGridSel + step) % count + count) % count;
}

bool SetShopHoverInfoString()
{
	if (!IsShopGridScreen(stextflag))
		return false;
	const std::vector<ShopSlot> stock = GetShopStock(stextflag);
	const std::vector<PlacedSlot> placed = PlaceStock(stock);
	const int hovered = PlacedSlotAt(placed, MousePosition);
	if (hovered < 0)
		return false;

	const ShopSlot &slot = stock[placed[hovered].stockIndex];
	// The same two calls the inventory's own hover makes, in the same order: the name sets the
	// string and its colour, the details append to it. Then the price, which is the one line a shop
	// adds - and it says what the number is FOR, because on Repair and Recharge the player is not
	// buying the item.
	ClearPanelStrings();
	GetItemStr(*slot.item);
	PrintItemDetails(*slot.item);
	AddPanelString(StrCat(_(ShopPriceLabel(stextflag)), ": ", FormatInteger(slot.price)),
	    UiFlags::ColorWhitegold);
	return true;
}

void ActivateShopGridSelection()
{
	const std::vector<ShopSlot> stock = GetShopStock(stextflag);
	if (ShopGridSel < 0 || ShopGridSel >= static_cast<int>(stock.size()))
		return;
	ShopSelectIndex(stextflag, stock[ShopGridSel].index);
}

} // namespace devilution::oracool
