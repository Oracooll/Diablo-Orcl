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

constexpr int ShopTabStripLeft = 16;
constexpr int ShopTabStripWidth = ShopPanelSize.width - 2 * ShopTabStripLeft;

/*
 * THE TABS LIVE OUTSIDE THE PANEL, in a column down its right-hand side.
 *
 * User, 2026-08-27: "cant we just move basic, magic, rare, unique, set, supplies, sold to the right
 * of the store window as vertical tabs one under the other?"
 *
 * Yes, and it is the answer to a problem two previous layouts had both been solving the wrong way.
 * The tabs were a three-row strip INSIDE the panel, which cost three of the 106 pixels between the
 * title band and the pinned grid; turning every control on its side to save that space bought the
 * room back at the price of vertical labels, which are slower to read. Both attempts were rationing
 * the same scarce band.
 *
 * The band was never where the tabs had to be. The panel is 340 wide against a screen that is at
 * least 640, and the column to its right is empty. Moving the tabs there costs the panel NOTHING and
 * gives the tabs ordinary horizontal labels at a comfortable size - so the services and bulk rows go
 * back to being plain wide rows inside the panel, with room to spare for the first time.
 *
 * The tabs float over the play area rather than over a panel of their own. That is the one thing to
 * be careful about, and it is handled in GetShopSurfaceRect: the click and hover routers ask for the
 * shop's whole footprint, not just the panel, so nothing behind the column can be clicked through it.
 */
constexpr int ShopTabColumnGap = 4;
constexpr int ShopTabColumnWidth = 104;
constexpr int ShopTabHeight = 26;
constexpr int ShopTabGap = 2;
/** @brief Aligned with the first control row inside the panel, so the two read as one band. */
constexpr int ShopTabColumnTop = 58;

/*
 * The control rows, between the title band and the pinned grid. There are 106 pixels here (the title
 * band ends at 56, the grid's top is pinned at GridBottom - 16 cells) and the user's instruction on
 * that has not moved: "you dont remove one grid row from shops."
 *
 * With the tabs gone the rows are no longer rationed - services, bulk and gold use 72 of the 106 and
 * the rest is deliberate breathing room rather than a number squeezed to fit.
 */
constexpr int ShopServiceTop = 58;
constexpr int ShopServiceHeight = 26;
constexpr int ShopActionTop = ShopServiceTop + ShopServiceHeight + 2;
constexpr int ShopActionHeight = 26;
constexpr int ShopGoldTop = ShopActionTop + ShopActionHeight + 2;
constexpr int ShopGoldHeight = 16;

static_assert(ShopGoldTop + ShopGoldHeight <= ShopGridTop - GridFrameWidth,
    "the controls above the shop grid no longer clear it - shorten the rows, or ask before "
    "taking a row off the grid");
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

/**
 * @brief Row-major first-fit into ONE page. Stock that will not fit is not stocked.
 *
 * This reverses a deliberate earlier decision, so the reasoning belongs here rather than in a commit
 * nobody will read again.
 *
 * It used to page. The argument was that silently unbuyable stock is worse than a second page, and
 * on its own terms that was right - given a shop that has already generated forty-five items, hiding
 * some of them is the worst way to fit them into a grid that holds about thirty.
 *
 * The premise was the problem. A vendor does not have to generate more than it can show (user,
 * 2026-08-27: "BASIC items tab sometimes offers more than one tab worth of items and in those cases
 * a next/prev tab arrow buttons appear. avoid this from hapenning. keep available items up to 1 page
 * worth of quantities"). What the shop carries is now defined as WHAT FITS, so nothing is dropped
 * from a stock list the player could otherwise have reached: the overflow was never on the shelf.
 *
 * That also makes "a full page worth of items" a thing the other tabs can simply BE, by generating
 * generously and letting the page decide where the shelf ends - which is what the Rare, Set, Unique
 * and Supplies tabs now rely on.
 */
std::vector<PlacedSlot> PlaceStock(const std::vector<ShopSlot> &stock)
{
	std::vector<PlacedSlot> placed;
	placed.reserve(stock.size());
	bool taken[ShopGridRows][ShopGridColumns] = {};

	for (size_t i = 0; i < stock.size(); i++) {
		const Size cells = GetInventorySize(*stock[i].item);
		// An item wider or taller than the whole grid can never be placed. Nothing in the game is.
		if (cells.width > ShopGridColumns || cells.height > ShopGridRows)
			continue;

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
				placed.push_back({ static_cast<int>(i), { col, row }, cells, 0 });
				done = true;
			}
		}
		// Not placed: the shelf is full. Keep going rather than stopping, because a SMALL item
		// after a large one may still fit in a gap the large one could not use.
	}
	return placed;
}

/**
 * @brief Always one. Kept as a function so the page-aware call sites read honestly.
 *
 * The grid is single-page since 2026-08-27 - see PlaceStock. This returning a constant is what makes
 * the page arrows never appear.
 */
int PageCount(const std::vector<PlacedSlot> & /*placed*/)
{
	return 1;
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
	case TalkID::SmithRareBuy:
	case TalkID::SmithSetBuy:
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
		// "Buy back", not "You get". This tab used to list what the player could sell, and the
		// number was what they would be paid; it lists what has already been sold now, and the
		// number is what taking it back costs. Same number, opposite direction.
		return N_("Buy back");
	default:
		return N_("Price");
	}
}

/** @brief Which services this vendor performs, in draw order. Empty for a vendor with none. */
std::vector<ServiceButton> ServicesFor(TalkID id)
{
	switch (id) {
	case TalkID::SmithBuy:
	case TalkID::SmithPremiumBuy:
	case TalkID::SmithUniqueBuy:
	case TalkID::SmithRareBuy:
	case TalkID::SmithSetBuy:
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
/**
 * @brief The word on a service button.
 *
 * Repair All's PRICE used to be appended here on hover. It is in the hover hint now instead (see
 * SetServiceHint's "Cost" line), because a price appended to a vertical label would be a column of
 * digits down the side of the panel. The user's requirement was that the cost appear on hover
 * (2026-08-27, after overruling my always-visible version); the hint is where hover text lives, and
 * it still says the number the moment the button is pointed at.
 */
std::string ServiceButtonLabel(ServiceButton service)
{
	switch (service) {
	case ServiceButton::Repair:
		return std::string(_("Repair"));
	case ServiceButton::RepairAll:
		return std::string(_("Repair All"));
	case ServiceButton::Recharge:
		return std::string(_("Recharge"));
	}
	return {};
}

/**
 * @brief One button on the control strip. Tabs, services and bulk actions all live on it now.
 *
 * A tagged union rather than three parallel lists, because the strip's geometry, hit-testing and
 * drawing are the same for all three and only the CLICK differs. Three lists would mean three rect
 * functions that have to agree about where the row starts, which is what the old three-row layout
 * had and what made adding a control a four-file change.
 */
enum class ControlKind : uint8_t {
	Tab,
	Service,
	Action,
};

struct ControlButton {
	ControlKind kind;
	/** @brief The tab this switches to, for Kind::Tab. */
	TalkID tab = TalkID::None;
	/** @brief Which service, for Kind::Service. */
	ServiceButton service = ServiceButton::Repair;
	/** @brief The store line the bulk action dispatches on, for Kind::Action. */
	int actionLine = 0;
	/** @brief Already translated - the three sources word their labels differently. */
	std::string label;
};

/**
 * @brief The services and bulk actions, in draw order. NOT the tabs - those have their own column.
 *
 * Still one tagged list for the two that share the panel's rows, because their geometry, hit-testing
 * and drawing are identical and only the click differs. Two lists would be two rect functions that
 * have to agree about where a row starts, which is what the pre-2026-08-27 layout had.
 */
std::vector<ControlButton> ShopControlButtons(TalkID id)
{
	std::vector<ControlButton> buttons;
	for (ServiceButton service : ServicesFor(id))
		buttons.push_back({ ControlKind::Service, TalkID::None, service, 0, ServiceButtonLabel(service) });
	for (const ShopAction &action : GetShopActions(id))
		buttons.push_back({ ControlKind::Action, TalkID::None, ServiceButton::Repair, action.line, std::string(_(action.label)) });
	return buttons;
}

/**
 * @brief Where one control sits. Services take the first row, bulk actions the second.
 *
 * Each row shares its full width between whatever is on it, so three services are 102px each and one
 * bulk action is the whole row. They never fight for space because they are on different rows.
 */
Rectangle ShopControlRect(const std::vector<ControlButton> &buttons, size_t index)
{
	const Rectangle panel = GetShopPanelRect();
	const ControlKind kind = buttons[index].kind;
	// Position WITHIN the row, and how many share it - counted rather than assumed, so a vendor with
	// no services still lays its bulk row out correctly.
	int onRow = 0;
	int before = 0;
	for (size_t i = 0; i < buttons.size(); i++) {
		if (buttons[i].kind != kind)
			continue;
		if (i < index)
			before++;
		onRow++;
	}
	const int top = kind == ControlKind::Service ? ShopServiceTop : ShopActionTop;
	const int height = kind == ControlKind::Service ? ShopServiceHeight : ShopActionHeight;
	const int width = ShopTabStripWidth / std::max(onRow, 1);
	return Rectangle { { panel.position.x + ShopTabStripLeft + before * width, panel.position.y + top },
		{ width, height } };
}

/**
 * @brief How many tabs the column has room for before it runs past the panel's foot.
 *
 * The three-row strip this replaced had an assert against its own capacity, and moving the tabs out
 * of the panel dropped it. Restored, because the failure it guards is the silent kind: a vendor that
 * grew an eighth tab would simply draw it further down the screen, over the world, and nothing would
 * say so. The column has room for far more than seven, so this is a tripwire rather than a limit.
 */
constexpr int ShopTabColumnSlots = (ShopPanelSize.height - ShopTabColumnTop) / (ShopTabHeight + ShopTabGap);

/** @brief One tab in the column beside the panel. */
Rectangle ShopTabRect(size_t index)
{
	assert(index < static_cast<size_t>(ShopTabColumnSlots)
	    && "a vendor has more tabs than the column has room for - the strip would run off the panel");
	const Rectangle panel = GetShopPanelRect();
	return Rectangle { { panel.position.x + panel.size.width + ShopTabColumnGap,
	                       panel.position.y + ShopTabColumnTop + static_cast<int>(index) * (ShopTabHeight + ShopTabGap) },
		{ ShopTabColumnWidth, ShopTabHeight } };
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
 * @brief What a service button actually does, written out in full.
 *
 * Rewritten (user, 2026-08-27: "rewrite the hover tooltips of repair and recharge buttons to
 * reflect the true mechanic of how they work"). The old one-liners said "drop an item here", which
 * was the whole mechanic when they were written and is now half of it: both buttons also arm a
 * cursor when your hand is empty. A hint that describes one of two gestures teaches the player that
 * the other does not exist.
 *
 * Each line is its own string because the info box wraps per-string, not per-paragraph.
 */
void SetServiceHint(ServiceButton service)
{
	ClearPanelStrings();
	switch (service) {
	case ServiceButton::Repair:
		SetPanelString(_("Repair"), UiFlags::ColorWhitegold);
		AddPanelString(_("Click for the hammer, then click any item to repair it."), UiFlags::ColorWhite);
		AddPanelString(_("Or drop an item here."), UiFlags::ColorWhite);
		AddPanelString(_("Restores full durability. Priced per item."), UiFlags::ColorWhite);
		break;
	case ServiceButton::RepairAll: {
		SetPanelString(_("Repair All"), UiFlags::ColorWhitegold);
		AddPanelString(_("Repairs everything you carry and wear, dearest first,"), UiFlags::ColorWhite);
		AddPanelString(_("until your gold runs out."), UiFlags::ColorWhite);
		const int price = ShopRepairAllPrice();
		if (price > 0)
			AddPanelString(StrCat(_("Cost"), ": ", FormatInteger(price)), UiFlags::ColorWhitegold);
		else
			AddPanelString(_("Nothing needs repairing."), UiFlags::ColorWhitegold);
		break;
	}
	case ServiceButton::Recharge:
		SetPanelString(_("Recharge"), UiFlags::ColorWhitegold);
		AddPanelString(_("Click for the cursor, then click a staff to recharge it."), UiFlags::ColorWhite);
		AddPanelString(_("Or drop a staff here."), UiFlags::ColorWhite);
		AddPanelString(_("Restores full charges. Priced per staff."), UiFlags::ColorWhite);
		break;
	}
}

/**
 * @brief The control strip and the gold readout, both above the grid.
 *
 * The hovered item's name and price used to live down here too. They are a cursor-following popup
 * now (SetShopHoverInfoString) - the player is already looking at the icon they are hovering, and a
 * readout at the far end of the panel made them look away from it to read it.
 */
void DrawShopControls(const Surface &out, int pageCount)
{
	const Rectangle panel = GetShopPanelRect();

	const std::vector<ControlButton> buttons = ShopControlButtons(stextflag);
	for (size_t i = 0; i < buttons.size(); i++) {
		const Rectangle rect = ShopControlRect(buttons, i);
		// A hover changes the INK only, never the plate, so the row does not shift under the pointer.
		const bool hovered = rect.contains(MousePosition);
		DrawHalfTransparentRectTo(out, rect.position.x, rect.position.y, rect.size.width, rect.size.height);
		DrawOrnateBorder(out, rect);
		DrawString(out, buttons[i].label, rect,
		    { (hovered ? UiFlags::ColorWhite : UiFlags::ColorWhitegold)
		        | UiFlags::FontSize12 | UiFlags::AlignCenter | UiFlags::VerticalCenter });
	}

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

/** @brief The tab column beside the panel, drawn after it so the tabs sit on top of nothing. */
void DrawShopTabColumn(const Surface &out)
{
	const std::vector<TalkID> tabs = ShopTabsFor(stextflag);
	for (size_t i = 0; i < tabs.size(); i++) {
		const Rectangle rect = ShopTabRect(i);
		const bool active = tabs[i] == stextflag;
		const bool hovered = rect.contains(MousePosition);
		// The active tab is filled solid so it reads as part of the panel; the rest are the same
		// half-transparent plate every other floating control wears.
		if (active) {
			DrawThemedFill(out, rect, 3);
		} else {
			DrawHalfTransparentRectTo(out, rect.position.x, rect.position.y, rect.size.width, rect.size.height);
		}
		DrawOrnateBorder(out, rect);
		DrawString(out, _(ShopTabName(tabs[i])), rect,
		    { (active || hovered ? UiFlags::ColorWhite : UiFlags::ColorWhitegold)
		        | UiFlags::FontSize12 | UiFlags::AlignCenter | UiFlags::VerticalCenter });
	}
}

/** @brief True if the click switched tabs, so the caller stops. */
bool CheckShopTabColumnClick(Point position)
{
	const std::vector<TalkID> tabs = ShopTabsFor(stextflag);
	for (size_t i = 0; i < tabs.size(); i++) {
		if (!ShopTabRect(i).contains(position))
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
	// The BOTTOM-left slot, shared with the stash, character sheet and quest log. Nothing else is
	// open while a shop is, so the slot is free - and all four dock together (user, 2026-08-27).
	return Rectangle { { 0, BottomDockedTop(ShopPanelSize.height) }, ShopPanelSize };
}

bool IsPointOverShop(Point position)
{
	// The panel PLUS the tab column beside it, because the tabs float over the play area and every
	// router that asks "is the pointer on the shop" has to count them.
	//
	// A PREDICATE, not a bounding rect, and the difference is the whole point: the column is a short
	// stack partway down the panel's right side, so a rectangle enclosing both would also enclose
	// the tall empty strip above and below it - swallowing clicks meant for the world in a band a
	// hundred pixels wide. Asked in one place rather than at each of the five call sites, because a
	// call site that had not been updated would let a click fall through the tabs to what is behind
	// them, which is the exact bug this panel already had once when towners were named through it.
	if (GetShopPanelRect().contains(position))
		return true;
	if (!IsShopGridScreen(stextflag))
		return false;
	const std::vector<TalkID> tabs = ShopTabsFor(stextflag);
	for (size_t i = 0; i < tabs.size(); i++) {
		if (ShopTabRect(i).contains(position))
			return true;
	}
	return false;
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
	DrawShopTabColumn(out);
	DrawShopClose(out);
}

bool CheckShopGridClick(Point position, bool rightClick)
{
	if (!IsShopGridScreen(stextflag))
		return false;
	if (!IsPointOverShop(position))
		return false;
	// The tabs first, and BEFORE the held-item branch below: dropping an item on a tab must switch
	// tabs rather than sell the item, because the tabs are the shop's navigation and a mis-drop on
	// one should not cost the player a sword.
	if (CheckShopTabColumnClick(position))
		return true;
	if (ShopCloseRect().contains(position)) {
		// Out of the shop entirely, not back to the vendor's dialog - the X on every other Oracool
		// window closes the window, and the tabs are how you move between shop screens.
		stextflag = TalkID::None;
		return true;
	}
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

				// The cursor follows the page. It did not, so after turning a page the selection
				// was still on an item the player could no longer see - and Enter bought THAT one
				// (external audit, 2026-08-25). The mouse hides this whenever it happens to be over
				// the grid, which is most of the time and is why it reads as intermittent.
				for (const PlacedSlot &slot : pages) {
					if (slot.page == ShopGridPage) {
						ShopGridSel = slot.stockIndex;
						break;
					}
				}
				return true;
			}
		}
	}

	// A held item is a DROP, not a click, and the target decides what happens to it. Ahead of every
	// other control on the panel: dropping a sword on the Repair button must repair it rather than
	// fall through to whatever that rect does when the hand is empty.
	const std::vector<ControlButton> buttons = ShopControlButtons(stextflag);
	if (!MyPlayer->HoldItem.isEmpty()) {
		for (size_t i = 0; i < buttons.size(); i++) {
			if (buttons[i].kind != ControlKind::Service)
				continue;
			if (!ShopControlRect(buttons, i).contains(position))
				continue;
			if (buttons[i].service == ServiceButton::Repair)
				ShopRepairHeldItem();
			else if (buttons[i].service == ServiceButton::Recharge)
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

	for (size_t i = 0; i < buttons.size(); i++) {
		if (!ShopControlRect(buttons, i).contains(position))
			continue;
		switch (buttons[i].kind) {
		case ControlKind::Tab:
			// Unreachable - the tabs are their own column now and CheckShopTabColumnClick answers
			// them above. Kept so the switch stays exhaustive over the enum rather than needing a
			// default that would swallow a kind added later.
			break;
		case ControlKind::Service:
			if (buttons[i].service == ServiceButton::RepairAll)
				ShopRepairAll();
			else if (buttons[i].service == ServiceButton::Repair)
				// The hammer, not a drop target (user, 2026-08-27: "make it work as the vanilla
				// Repair Item skill - summon a Hammer cursor instead of the regular cursor, then
				// click on item i want repaired"). Dropping an item on the button still works and
				// is unchanged; this is what the button does when your hand is empty.
				ArmShopRepairCursor();
			else if (buttons[i].service == ServiceButton::Recharge)
				// The same gesture at Adria's (user, 2026-08-27: "make recharge button work as
				// repair button").
				ArmShopRechargeCursor();
			break;
		case ControlKind::Action:
			ShopActivateAction(stextflag, buttons[i].actionLine);
			break;
		}
		return true;
	}

	const std::vector<ShopSlot> stock = GetShopStock(stextflag);
	const std::vector<PlacedSlot> placed = PlaceStock(stock);
	const int hovered = PlacedSlotAt(placed, position);
	if (hovered >= 0) {
		if (rightClick) {
			// The purchase. ShopSelectIndex runs the vendor's own handler and, since 2026-08-26,
			// answers the confirmation itself - so this one gesture is the whole transaction.
			ShopSelectIndex(stextflag, stock[placed[hovered].stockIndex].index);
		} else {
			// Looking, not buying. The selection moves so the footer describes this item, and
			// nothing is spent - which is what makes a right click safe to be unconfirmed.
			//
			// stockIndex, not stock[...].index: ShopGridSel indexes the stock vector, which is what
			// ActivateShopGridSelection reads it back as.
			ShopGridSel = placed[hovered].stockIndex;
		}
	}
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
	if (!IsPointOverShop(MousePosition))
		return false;

	const std::vector<ShopSlot> stock = GetShopStock(stextflag);
	const std::vector<PlacedSlot> placed = PlaceStock(stock);
	const int hovered = PlacedSlotAt(placed, MousePosition);

	const std::vector<ControlButton> buttons = ShopControlButtons(stextflag);
	for (size_t i = 0; i < buttons.size(); i++) {
		if (buttons[i].kind != ControlKind::Service)
			continue;
		if (!ShopControlRect(buttons, i).contains(MousePosition))
			continue;
		// The button says WHICH service; the hint is where what it does - and what Repair All costs
		// - is written down.
		SetServiceHint(buttons[i].service);
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
