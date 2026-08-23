#include "oracool/shop_grid.h"

#include <algorithm>
#include <cassert>
#include <cstring>
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
/**
 * @brief Rows reserved for the strip, whatever this vendor actually has.
 *
 * Two, because five is the most tabs any vendor offers - Griswold's Basic, Magic, Unique, Supplies
 * and Sold, and the last two of those are already conditional. It was three while Repair and
 * Recharge were still tabs, and leaving it there after they became buttons would have left a blank
 * row of reserved space between the tabs and the controls.
 *
 * Fixed rather than derived from the tab count, so the grid does not move when a vendor has fewer.
 * DrawShopTabRow asserts a vendor has not outgrown it.
 */
constexpr int ShopTabRows = 2;
constexpr int ShopTabStripLeft = 16;
constexpr int ShopTabStripWidth = ShopPanelSize.width - 2 * ShopTabStripLeft;

/** @brief Bulk actions and the gold readout, stacked between the tabs and the grid. */
// The budget between the fixed title band and the pinned grid top is shared by everything here, so
// these gaps are checked rather than chosen: the assert below is what says they fit. GridFrameWidth
// is 6 - the carved stone bezel's, not the procedural bevel's 3 - and reserving the smaller number
// is the mistake it exists to catch. It has already caught it once.
constexpr int ShopActionTop = ShopTabTop + ShopTabRows * (ShopTabHeight + ShopTabRowGap) + 10;
constexpr int ShopActionHeight = 20;
constexpr int ShopGoldTop = ShopActionTop + ShopActionHeight + 8;
constexpr int ShopGoldHeight = 15;

static_assert(ShopGoldTop + ShopGoldHeight <= ShopGridTop - GridFrameWidth,
    "the controls above the shop grid no longer clear it - drop a tab row or shorten the stack");
static_assert(ShopGridLeft >= 0, "the shop grid is wider than the panel");

/** @brief Where one stock entry sits on the grid, and which stock entry it is. */
struct PlacedSlot {
	int stockIndex;
	Point cell;
	Size cells;
	int page;
};

/** @brief Which page of the stock is showing. */
int ShopGridPage = 0;

/**
 * @brief The keyboard cursor, as a position in the tab's stock order.
 *
 * Stock order, not a grid cell: the stock is what the transaction is indexed by, and a cursor that
 * lived on the grid would have to answer "what is selected" for the empty cells too.
 */
int ShopGridSel = 0;

/**
 * @brief Whether the cursor was over a shop item the last time InfoString was rebuilt.
 *
 * Read by cursor_tooltip.cpp, which uses it to decide that this hover wants the PANEL treatment - a
 * padded, darkened, bordered plate - rather than bare outlined text. Set once per frame from
 * SetShopHoverInfoString, which is the same once-per-frame hover pass every other flag that
 * function consults is written by, so it cannot go stale relative to them.
 */
bool ShopHoverActive = false;

/** @brief The three services, in the order they are drawn. */
enum class ServiceButton : uint8_t {
	Repair,
	RepairAll,
	Recharge,
};
constexpr int ServiceButtonSize = 20;
constexpr int ServiceButtonGap = 3;

/**
 * @brief Row-major first-fit across as many pages as the stock needs.
 *
 * Paged, not truncated. The grid is 160 cells and Griswold now carries up to forty-five items; at
 * the four-to-six cells a weapon or a breastplate occupies that is comfortably more than one page
 * holds, and the first version of this function silently dropped whatever did not fit. Silently
 * unbuyable stock is the worst of the available outcomes - worse than a second page, and much worse
 * than a smaller shop.
 *
 * An item that will not fit on the current page starts the next one rather than being squeezed in
 * behind an earlier item's cells.
 */
std::vector<PlacedSlot> PlaceStock(const std::vector<ShopSlot> &stock)
{
	std::vector<PlacedSlot> placed;
	placed.reserve(stock.size());
	bool taken[ShopGridRows][ShopGridColumns] = {};
	int page = 0;

	for (size_t i = 0; i < stock.size(); i++) {
		const Size cells = GetInventorySize(*stock[i].item);
		// An item wider or taller than the whole grid can never be placed on any page. Nothing in
		// the game is, but the page-turn below would loop forever if one ever were.
		if (cells.width > ShopGridColumns || cells.height > ShopGridRows)
			continue;

		bool done = false;
		while (!done) {
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
					placed.push_back({ static_cast<int>(i), { col, row }, cells, page });
					done = true;
				}
			}
			if (done)
				break;
			page++;
			std::memset(taken, 0, sizeof(taken));
		}
	}
	return placed;
}

/** @brief How many pages the stock spans. Always at least one, so "Page 1 of 1" is sayable. */
int PageCount(const std::vector<PlacedSlot> &placed)
{
	int pages = 1;
	for (const PlacedSlot &slot : placed)
		pages = std::max(pages, slot.page + 1);
	return pages;
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

/** @brief Which placed slot @p position is over ON THE CURRENT PAGE, or -1. */
int PlacedSlotAt(const std::vector<PlacedSlot> &placed, Point position)
{
	const Rectangle grid = GetShopGridRect();
	if (!grid.contains(position))
		return -1;
	const Point cell { (position.x - grid.position.x) / ShopCellPx, (position.y - grid.position.y) / ShopCellPx };
	for (size_t i = 0; i < placed.size(); i++) {
		const PlacedSlot &slot = placed[i];
		// The page test is not optional: every page reuses the same cells, so without it a click
		// resolves to whichever item happens to occupy that cell on ANY page - and the first match
		// is page 0's.
		if (slot.page != ShopGridPage)
			continue;
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
	// A vendor that grows past the reserved rows would draw its last row over the control row and
	// then over the grid, silently. The strip's height is fixed on purpose (see ShopTabRows), so
	// this is the only thing standing between a new tab and a layout that quietly overlaps.
	assert(tabs.size() <= static_cast<size_t>(ShopTabsPerRow * ShopTabRows)
	    && "a vendor has more tabs than the strip reserves rows for - raise ShopTabRows");
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

/** @brief Which services this vendor performs, in draw order. Empty for a vendor with none. */
std::vector<ServiceButton> ServicesFor(TalkID id)
{
	switch (id) {
	case TalkID::SmithBuy:
	case TalkID::SmithPremiumBuy:
	case TalkID::SmithUniqueBuy:
	case TalkID::SmithConsumables:
	case TalkID::SmithSell:
		return { ServiceButton::Repair, ServiceButton::RepairAll, ServiceButton::Recharge };
	case TalkID::WitchBuy:
	case TalkID::WitchSell:
		return { ServiceButton::Recharge };
	default:
		return {};
	}
}

/** @brief The service buttons sit flush right on the control row; the text actions get what is left. */
Rectangle ServiceButtonRect(size_t index, size_t count)
{
	const Rectangle panel = GetShopPanelRect();
	const int stripRight = panel.position.x + ShopTabStripLeft + ShopTabStripWidth;
	const int blockWidth = static_cast<int>(count) * ServiceButtonSize + (static_cast<int>(count) - 1) * ServiceButtonGap;
	const int left = stripRight - blockWidth + static_cast<int>(index) * (ServiceButtonSize + ServiceButtonGap);
	return Rectangle { { left, panel.position.y + ShopActionTop }, { ServiceButtonSize, ServiceButtonSize } };
}

/** @brief Bulk-action buttons share the control row with the service icons, to their left. */
Rectangle ShopActionRect(size_t index, size_t count)
{
	const Rectangle panel = GetShopPanelRect();
	const size_t services = ServicesFor(stextflag).size();
	int available = ShopTabStripWidth;
	if (services > 0) {
		const int block = static_cast<int>(services) * ServiceButtonSize
		    + (static_cast<int>(services) - 1) * ServiceButtonGap;
		available -= block + ServiceButtonGap;
	}
	const int width = available / static_cast<int>(std::max<size_t>(count, 1));
	return Rectangle { { panel.position.x + ShopTabStripLeft + static_cast<int>(index) * width,
	                       panel.position.y + ShopActionTop },
		{ width, ShopActionHeight } };
}

constexpr int PageButtonWidth = 16;

/** @brief The two page arrows, at the ends of the gold row. 0 is back, 1 is forward. */
Rectangle ShopPageButtonRect(int index)
{
	const Rectangle panel = GetShopPanelRect();
	const int left = index == 0
	    ? panel.position.x + ShopTabStripLeft
	    : panel.position.x + ShopTabStripLeft + ShopTabStripWidth - PageButtonWidth;
	return Rectangle { { left, panel.position.y + ShopGoldTop }, { PageButtonWidth, ShopGoldHeight } };
}

/** @brief The red X, top-right, same as every other Oracool window's. */
Rectangle ShopCloseRect()
{
	const Rectangle panel = GetShopPanelRect();
	return Rectangle { { panel.position.x + panel.size.width - 34, panel.position.y + 14 }, { 20, 20 } };
}

/**
 * @brief The service icons, drawn rather than blitted.
 *
 * PLACEHOLDER, and the only honest option today: there is no art for these, and the request was for
 * icons and no text. They are built out of filled rectangles so they read at 20px - a hammer for
 * Repair, the same hammer over three dots for Repair all, a bolt for Recharge. When art arrives this
 * is one blit per button and the geometry above does not move.
 */
void DrawServiceIcon(const Surface &out, ServiceButton service, Rectangle rect, uint8_t color)
{
	const int x = rect.position.x;
	const int y = rect.position.y;
	switch (service) {
	case ServiceButton::Repair:
	case ServiceButton::RepairAll: {
		// Head across the top, handle down through the middle. RepairAll is the same hammer lifted
		// two pixels to make room for the three dots that say "all of them".
		const int lift = service == ServiceButton::RepairAll ? 2 : 0;
		FillRect(out, x + 3, y + 5 - lift, 14, 4, color);
		FillRect(out, x + 9, y + 9 - lift, 3, 7, color);
		if (service == ServiceButton::RepairAll) {
			FillRect(out, x + 4, y + 15, 2, 2, color);
			FillRect(out, x + 9, y + 15, 2, 2, color);
			FillRect(out, x + 14, y + 15, 2, 2, color);
		}
		break;
	}
	case ServiceButton::Recharge:
		// A bolt: two offset wedges meeting at the middle.
		FillRect(out, x + 10, y + 3, 5, 3, color);
		FillRect(out, x + 8, y + 6, 5, 3, color);
		FillRect(out, x + 6, y + 9, 8, 2, color);
		FillRect(out, x + 7, y + 11, 5, 3, color);
		FillRect(out, x + 5, y + 14, 5, 3, color);
		break;
	}
}

/** @brief What a service button does when it is clicked, or dropped on. */
const char *ServiceHint(ServiceButton service)
{
	switch (service) {
	case ServiceButton::Repair:
		return N_("Repair - drop an item here");
	case ServiceButton::RepairAll:
		return N_("Repair all");
	case ServiceButton::Recharge:
		return N_("Recharge - drop a staff here");
	}
	return "";
}

void DrawServiceButtons(const Surface &out)
{
	const std::vector<ServiceButton> services = ServicesFor(stextflag);
	for (size_t i = 0; i < services.size(); i++) {
		const Rectangle rect = ServiceButtonRect(i, services.size());
		const bool hovered = rect.contains(MousePosition);
		DrawHalfTransparentRectTo(out, rect.position.x, rect.position.y, rect.size.width, rect.size.height);
		DrawOrnateBorder(out, rect);
		DrawServiceIcon(out, services[i], rect, hovered ? PAL8_YELLOW + 2 : ThemeEdgeColor);
	}
}

/**
 * @brief The bulk-action row and the gold readout, both above the grid.
 *
 * The hovered item's name and price used to live down here too. They are a cursor-following popup
 * now (SetShopHoverInfoString) - the player is already looking at the icon they are hovering, and a
 * readout at the far end of the panel made them look away from it to read it.
 */
void DrawShopControls(const Surface &out, int pageCount)
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

	DrawServiceButtons(out);

	// The page arrows share the gold row rather than taking a row of their own: the space between
	// the title band and the grid's pinned top is fully spoken for (see the static_assert above),
	// and the gold readout is one centred line with both ends going spare.
	const Rectangle goldLine { { panel.position.x + ShopTabStripLeft, panel.position.y + ShopGoldTop },
		{ ShopTabStripWidth, ShopGoldHeight } };
	DrawString(out, fmt::format(fmt::runtime(_("Your gold: {:s}")), FormatInteger(TotalPlayerGold())), goldLine,
	    { UiFlags::ColorWhitegold | UiFlags::FontSize12 | UiFlags::AlignCenter | UiFlags::VerticalCenter });

	if (pageCount <= 1)
		return;
	for (int i = 0; i < 2; i++) {
		const Rectangle rect = ShopPageButtonRect(i);
		DrawOrnateBorder(out, rect);
		DrawString(out, i == 0 ? "<" : ">", rect,
		    { UiFlags::ColorWhite | UiFlags::FontSize12 | UiFlags::AlignCenter | UiFlags::VerticalCenter });
	}
	const Rectangle pageLabel { { goldLine.position.x + PageButtonWidth + 2, goldLine.position.y },
		{ 60, ShopGoldHeight } };
	DrawString(out, StrCat(ShopGridPage + 1, "/", pageCount), pageLabel,
	    { UiFlags::ColorWhitegold | UiFlags::FontSize12 | UiFlags::VerticalCenter });
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
	ShopGridPage = 0;
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
	const int pageCount = PageCount(placed);
	ShopGridPage = std::clamp(ShopGridPage, 0, pageCount - 1);

	// The mouse wins over the keyboard cursor while it is over an item, exactly as the inventory
	// does - otherwise the tooltip would describe one item while the pointer sits on another.
	const int hoveredPlaced = PlacedSlotAt(placed, MousePosition);
	if (hoveredPlaced >= 0)
		ShopGridSel = placed[hoveredPlaced].stockIndex;
	if (ShopGridSel >= static_cast<int>(stock.size()))
		ShopGridSel = stock.empty() ? 0 : static_cast<int>(stock.size()) - 1;

	for (const PlacedSlot &slot : placed) {
		if (slot.page != ShopGridPage)
			continue;
		const Item &item = *stock[slot.stockIndex].item;
		InvDrawSlotBack(out, SpriteAnchor(slot),
		    { slot.cells.width * ShopCellPx, slot.cells.height * ShopCellPx }, item);
	}

	for (const PlacedSlot &slot : placed) {
		if (slot.page != ShopGridPage)
			continue;
		const Item &item = *stock[slot.stockIndex].item;
		const ClxSprite sprite = GetInvItemSprite(item._iCurs + CURSOR_FIRSTITEM);
		const Point position = SpriteAnchor(slot);
		if (slot.stockIndex == ShopGridSel)
			ClxDrawOutline(out, GetOutlineColor(item, true), position, sprite);
		ClxDraw(out, position, sprite);
	}

	DrawShopControls(out, pageCount);
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

	{
		const std::vector<PlacedSlot> pages = PlaceStock(GetShopStock(stextflag));
		const int pageCount = PageCount(pages);
		if (pageCount > 1) {
			for (int i = 0; i < 2; i++) {
				if (!ShopPageButtonRect(i).contains(position))
					continue;
				// Wraps, so a two-page shop turns either way with one button. Both arrows exist
				// regardless, because a shop that grows a third page should not change how the
				// first two are reached.
				ShopGridPage = (ShopGridPage + (i == 0 ? -1 : 1) + pageCount) % pageCount;
				return true;
			}
		}
	}

	// A held item is a DROP, not a click, and the target decides what happens to it. Ahead of every
	// other control on the panel: dropping a sword on the Repair button must repair it rather than
	// fall through to whatever that rect does when the hand is empty.
	if (!MyPlayer->HoldItem.isEmpty()) {
		const std::vector<ServiceButton> services = ServicesFor(stextflag);
		for (size_t i = 0; i < services.size(); i++) {
			if (!ServiceButtonRect(i, services.size()).contains(position))
				continue;
			if (services[i] == ServiceButton::Repair)
				ShopRepairHeldItem();
			else if (services[i] == ServiceButton::Recharge)
				ShopRechargeHeldItem();
			// Repair all ignores a held item rather than repairing it: it is a button about the
			// whole inventory, and the held item is not in the inventory.
			return true;
		}
		// Anywhere else on the panel sells it. A refusal leaves the item in the player's hand -
		// swallowing an item a vendor will not buy is how you lose one.
		ShopSellHeldItem();
		return true;
	}

	const std::vector<ServiceButton> services = ServicesFor(stextflag);
	for (size_t i = 0; i < services.size(); i++) {
		if (!ServiceButtonRect(i, services.size()).contains(position))
			continue;
		if (services[i] == ServiceButton::RepairAll)
			ShopRepairAll();
		return true;
	}

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

	// The page follows the cursor. Without this, arrowing off the end of page one moves an invisible
	// selection and Enter buys something the player cannot see.
	for (const PlacedSlot &slot : PlaceStock(stock)) {
		if (slot.stockIndex == ShopGridSel) {
			ShopGridPage = slot.page;
			break;
		}
	}
}

bool IsShopItemHovered()
{
	return ShopHoverActive;
}

bool SetShopHoverInfoString()
{
	ShopHoverActive = false;
	if (!IsShopGridScreen(stextflag))
		return false;

	// Anything inside the panel is answered here, item or not. The panel covers the world, and the
	// producers further down UpdateInfoString were naming towners standing behind it - the user
	// hovered the Repair tab and got "Gillian the Barmaid".
	if (!GetShopPanelRect().contains(MousePosition))
		return false;

	const std::vector<ShopSlot> stock = GetShopStock(stextflag);
	const std::vector<PlacedSlot> placed = PlaceStock(stock);
	const int hovered = PlacedSlotAt(placed, MousePosition);

	const std::vector<ServiceButton> services = ServicesFor(stextflag);
	for (size_t i = 0; i < services.size(); i++) {
		if (!ServiceButtonRect(i, services.size()).contains(MousePosition))
			continue;
		// The icons carry no text, so the hint is the only place their meaning is written down.
		ClearPanelStrings();
		SetPanelString(_(ServiceHint(services[i])), UiFlags::ColorWhitegold);
		return true;
	}

	if (hovered < 0) {
		ClearPanelStrings();
		InfoColor = UiFlags::ColorWhite;
		return true;
	}
	ShopHoverActive = true;

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
