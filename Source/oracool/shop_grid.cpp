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

/** @brief Tab strip: rows of at most four, under the title band. */
constexpr int ShopTabTop = 62;
constexpr int ShopTabHeight = 22;
constexpr int ShopTabRowGap = 2;
constexpr int ShopTabsPerRow = 4;
constexpr int ShopTabStripLeft = ShopGridLeft;
constexpr int ShopTabStripWidth = ShopGridWidth;

/** @brief Two tab rows plus a gap, then the grid. Fixed, so the grid does not move when a vendor has fewer tabs. */
constexpr int ShopTabRows = 2;
constexpr int ShopGridTop = ShopTabTop + ShopTabRows * (ShopTabHeight + ShopTabRowGap) + 18;
constexpr int ShopFooterTop = ShopGridTop + ShopGridHeight + 10;

static_assert(ShopFooterTop < ShopPanelSize.height, "the shop footer starts below the panel");
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

Rectangle ShopFooterRect()
{
	const Rectangle panel = GetShopPanelRect();
	return Rectangle { { panel.position.x + ShopGridLeft, panel.position.y + ShopFooterTop },
		{ ShopGridWidth, ShopPanelSize.height - ShopFooterTop - 12 } };
}

constexpr int ShopActionHeight = 20;

/** @brief Bulk-action buttons share one row across the footer, at most two of them. */
Rectangle ShopActionRect(size_t index, size_t count)
{
	const Rectangle footer = ShopFooterRect();
	const int width = footer.size.width / static_cast<int>(std::max<size_t>(count, 1));
	return Rectangle { { footer.position.x + static_cast<int>(index) * width, footer.position.y + 42 },
		{ width, ShopActionHeight } };
}

/** @brief The red X, top-right, same as every other Oracool window's. */
Rectangle ShopCloseRect()
{
	const Rectangle panel = GetShopPanelRect();
	return Rectangle { { panel.position.x + panel.size.width - 34, panel.position.y + 14 }, { 20, 20 } };
}

void DrawShopFooter(const Surface &out, const std::vector<ShopSlot> &stock, int hovered)
{
	const Rectangle footer = ShopFooterRect();
	DrawThemedFill(out, footer, 2);
	DrawOrnateBorderOutside(out, footer);

	const int lineHeight = 15;
	Rectangle line { { footer.position.x + 6, footer.position.y + 6 }, { footer.size.width - 12, lineHeight } };

	if (hovered >= 0 && hovered < static_cast<int>(stock.size())) {
		const ShopSlot &slot = stock[hovered];
		DrawString(out, slot.item->getName(), line,
		    { slot.item->getTextColorWithStatCheck() | UiFlags::FontSize12 | UiFlags::AlignCenter });
		line.position.y += lineHeight;
		DrawString(out, StrCat(_(ShopPriceLabel(stextflag)), ": ", FormatInteger(slot.price)), line,
		    { UiFlags::ColorWhitegold | UiFlags::FontSize12 | UiFlags::AlignCenter });
	} else {
		DrawString(out, _("Select an item"), line,
		    { UiFlags::ColorWhitegold | UiFlags::FontSize12 | UiFlags::AlignCenter });
	}

	const std::vector<ShopAction> actions = GetShopActions(stextflag);
	for (size_t i = 0; i < actions.size(); i++) {
		const Rectangle rect = ShopActionRect(i, actions.size());
		DrawHalfTransparentRectTo(out, rect.position.x, rect.position.y, rect.size.width, rect.size.height);
		DrawOrnateBorder(out, rect);
		DrawString(out, _(actions[i].label), rect,
		    { UiFlags::ColorWhite | UiFlags::FontSize12 | UiFlags::AlignCenter | UiFlags::VerticalCenter });
	}

	line.position.y = footer.position.y + footer.size.height - lineHeight - 6;
	DrawString(out, fmt::format(fmt::runtime(_("Your gold: {:s}")), FormatInteger(TotalPlayerGold())), line,
	    { UiFlags::ColorWhitegold | UiFlags::FontSize12 | UiFlags::AlignCenter });
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

	constexpr Displacement SpriteOffset { 0, ShopCellPx - 1 };

	for (const PlacedSlot &slot : placed) {
		const Item &item = *stock[slot.stockIndex].item;
		InvDrawSlotBack(out, CellOrigin(slot.cell) + SpriteOffset,
		    { slot.cells.width * ShopCellPx, slot.cells.height * ShopCellPx }, item);
	}

	for (const PlacedSlot &slot : placed) {
		const Item &item = *stock[slot.stockIndex].item;
		const ClxSprite sprite = GetInvItemSprite(item._iCurs + CURSOR_FIRSTITEM);
		const Point position = CellOrigin(slot.cell) + SpriteOffset;
		if (slot.stockIndex == ShopGridSel)
			ClxDrawOutline(out, GetOutlineColor(item, true), position, sprite);
		ClxDraw(out, position, sprite);
	}

	DrawShopFooter(out, stock, stock.empty() ? -1 : ShopGridSel);
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

void ActivateShopGridSelection()
{
	const std::vector<ShopSlot> stock = GetShopStock(stextflag);
	if (ShopGridSel < 0 || ShopGridSel >= static_cast<int>(stock.size()))
		return;
	ShopSelectIndex(stextflag, stock[ShopGridSel].index);
}

} // namespace devilution::oracool
