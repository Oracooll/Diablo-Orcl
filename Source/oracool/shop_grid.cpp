#include "oracool/shop_grid.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <cstring>
#include <string>
#include <vector>

#include <SDL.h>
#include <fmt/format.h>

#include "control.h"
#include "cursor.h"
#include "diablo.h" // sgbMouseDown - a button's pressed plate
#include "engine/load_pcx.hpp" // the vanilla button, read from the player's archive
#include "engine/render/clx_render.hpp"
#include "engine/render/primitive_render.hpp"
#include "engine/render/text_render.hpp"
#include "inv.h"
#include "items.h"
#include "oracool/cursor_tooltip.h" // ShowPanelStringsAsHintCard - the service buttons' card
#include "oracool/grid_bezel.h"
#include "oracool/hud_art.h"
#include "oracool/inventory_layout.h" // GridBottom - the line the stash's grid also ends on
#include "oracool/item_tint.h"
#include "oracool/ornate_border.h"
#include "oracool/shop_tabs.h"
#include "oracool/ui_sound.h"
#include "oracool/window_close.h"
#include "utils/format_int.hpp"
#include "utils/utf8.hpp" // DecodeFirstUtf8CodePoint - vertical labels split by code point, not byte
#include "utils/language.h"
#include "utils/log.hpp"
#include "utils/str_cat.hpp"

namespace devilution::oracool {

std::vector<ButtonSliceSpan> SliceButtonAxis(int target, int source, int cap)
{
	std::vector<ButtonSliceSpan> spans;
	if (target <= 0 || source <= 0)
		return spans;
	if (target < source) {
		// The first half and the last half, butted: both ends survive and the middle is what goes.
		const int first = (target + 1) / 2;
		spans.push_back({ 0, 0, first });
		if (target > first)
			spans.push_back({ source - (target - first), first, target - first });
		return spans;
	}
	cap = std::clamp(cap, 0, source / 2);
	if (source - 2 * cap <= 0)
		cap = 0;
	const int middle = source - 2 * cap;
	if (cap > 0)
		spans.push_back({ 0, 0, cap });
	for (int dest = cap; dest < target - cap;) {
		const int run = std::min(middle, target - cap - dest);
		spans.push_back({ cap, dest, run });
		dest += run;
	}
	if (cap > 0)
		spans.push_back({ source - cap, target - cap, cap });
	return spans;
}

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
/**
 * The service and bulk-action controls' row: 6px in from the new canvas's bezels (user, 2026-09-05:
 * "reduce width of your coded buttons in griswold. make the total width of them to fit within 6px
 * away from edge bezels of new canvases"). The canvas's inner bezel edges are x=21 and x=318
 * (measured, 2026-09-05), so the row runs 28..312.
 */
constexpr int ShopCanvasBezelInner = 22;
constexpr int ShopControlsInset = 6;
constexpr int ShopControlsLeft = ShopCanvasBezelInner + ShopControlsInset;
constexpr int ShopControlsWidth = ShopPanelSize.width - 2 * ShopControlsLeft;

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
/*
 * The tabs are VERTICAL buttons - tall and narrow, label reading down the tab - stacked one above
 * the next and flush against the panel's right edge (user, 2026-08-27: "i wanted you to make the tab
 * buttons vertical, not horizontal on top of each other. VERTICAL on top of each other. attached
 * flush to the right border of the griswold limestone window").
 *
 * Book spines on a shelf, or the tabs on a filing cabinet. My first pass at this column made the
 * BUTTONS horizontal and only their arrangement vertical, which is a different thing and a wider
 * one: 104px of screen beside the panel per tab, against 26 here.
 *
 * FLUSH means gapless - x is the panel's right edge exactly, so the tabs read as part of the window
 * rather than as a floating strip near it.
 */
/*
 * 27 wide and 3 apart since the controls took the vanilla button (2026-09-11): 27 is that button's face
 * height, laid on its side, and 3 is the one gap every control on this panel keeps - between the
 * buttons on a row, between the rows, and between the tabs.
 */
constexpr int ShopControlGap = 3;
constexpr int ShopTabColumnWidth = 27;
constexpr int ShopTabHeight = 80;
// 2, not ShopControlGap: eight tabs must fit - Basic, Magic, Rare, Set, Unique, Supplies, Sold and Transmute with every
// shelf on - and at 3 the eighth ran off the column, tripping the Debug build's assert (round 13 audit, v1.12.238).
constexpr int ShopTabGap = 2;
/** @brief Below the title band, where the first control row also starts. */
constexpr int ShopTabColumnTop = 60;

/*
 * The control rows, between the title band and the pinned grid. There are 106 pixels here (the title
 * band ends at 56, the grid's top is pinned at GridBottom - 16 cells) and the user's instruction on
 * that has not moved: "you dont remove one grid row from shops."
 *
 * With the tabs gone the rows are no longer rationed. Services, bulk and gold end exactly where the
 * refusal toast's band above the grid begins (shop_toast.cpp), so a refusal never covers a button.
 */
constexpr int ShopServiceTop = ShopTabColumnTop;
constexpr int ShopServiceHeight = 27;
constexpr int ShopActionTop = ShopServiceTop + ShopServiceHeight + ShopControlGap;
constexpr int ShopActionHeight = 27;
constexpr int ShopGoldTop = ShopActionTop + ShopActionHeight + ShopControlGap;
constexpr int ShopGoldHeight = 16;

static_assert(ShopGoldTop + ShopGoldHeight <= ShopGridTop - GridFrameWidth,
    "the controls above the shop grid no longer clear it - shorten the rows, or ask before "
    "taking a row off the grid");
/** @brief shop_toast.cpp's ToastHeight: the refusal banner sits directly above the grid. */
constexpr int ShopToastBand = 34;
static_assert(ShopGoldTop + ShopGoldHeight <= ShopGridTop - ShopToastBand,
    "the gold line runs into the band the refusal toast takes above the grid (shop_toast.cpp)");
static_assert(ShopGridLeft >= 0, "the shop grid is wider than the panel");

/*
 * Oracool: the controls' own art (batch 6, 2026-09-11). Three BLANK limestone plates - the labels are
 * still the game's, drawn on top. Each is cut to the rect it sits in, and the asserts hold the two
 * together: a rect that moves off its plate fails the build rather than drawing a plate the wrong size.
 */
constexpr const char *ShopTabArt = "ui\\shop_tab.png";              // three 26x80 cells: idle, hover, active
constexpr const char *ShopButtonArt = "ui\\shop_button.png";        // three 284x26 rows: idle, hover, pressed
constexpr const char *ShopGoldPlateArt = "ui\\shop_gold_plate.png"; // one 284x16 strip
constexpr Size ShopTabCell { 26, 80 };
constexpr Size ShopButtonCell { 284, 26 };
constexpr Size ShopGoldPlateSize { 284, 16 };
// Since the vanilla button took over (below) these plates are the FALLBACK, and the controls are sized
// to that button rather than to them - the plates are a pixel short of a 27px control, which is the
// price of a fallback, so only the sizes that still hold are held.
static_assert(ShopTabCell.height == ShopTabHeight && ShopButtonCell.width == ShopControlsWidth,
    "shop_tab.png / shop_button.png no longer match the tab height or the control row's width");
static_assert(ShopGoldPlateSize.width == ShopControlsWidth && ShopGoldPlateSize.height == ShopGoldHeight,
    "shop_gold_plate.png no longer matches the gold line");

/** @brief Whether @p assetPath loaded - the fallback test levski_roar.cpp and book_frame.cpp make. */
bool HasShopArt(const char *assetPath)
{
	return GetLoosePngSize(assetPath).width != 0;
}

/**
 * @brief One of the button plate's rows across @p rect: @p state 0 idle, 1 hover, 2 pressed.
 *
 * The plate is cut for a whole row, 284 wide, and a row shared three ways is 94 - Griswold's
 * services. So a narrower rect takes the plate's left half and its right half, each 1:1, butted at
 * the middle: both chamfered ends survive and nothing is scaled. A full-width rect is the whole plate.
 */
void DrawShopButtonPlate(const Surface &out, Rectangle rect, int state)
{
	const int top = state * ShopButtonCell.height;
	const int width = std::min(rect.size.width, ShopButtonCell.width);
	const int left = width / 2;
	const int right = width - left;
	DrawLoosePngPart(out, ShopButtonArt, Rectangle { { 0, top }, { left, ShopButtonCell.height } }, rect.position);
	DrawLoosePngPart(out, ShopButtonArt, Rectangle { { ShopButtonCell.width - right, top }, { right, ShopButtonCell.height } },
	    rect.position + Displacement { left, 0 });
}

/*
 * Oracool: the controls wear the VANILLA small button (user, 2026-09-11: "why dont you use vanilla
 * buttons instead of these", then "we can use desaturated version of them and gold font"). The
 * limestone plates above are the fallback.
 *
 * READ FROM THE PLAYER'S OWN ARCHIVE, never shipped: ui_art\but_sml.pcx is the dialog button the front
 * end already loads (DiabloUI/button.cpp). It is loaded here with its own palette, because in game the
 * palette is the town's or the dungeon's and the PCX's indices mean nothing against it. Each index is
 * resolved to a grey of its own brightness, so the button is desaturated at runtime and the file is
 * never altered.
 *
 * Only the face is used: in each 112x28 frame, row 0 and columns 110-111 are padding, so the face is the
 * 110x27 at (0, 1). Frames 0, 1 and 2 are at rest, pressed and lit - the last with the ring the front
 * end draws round its focused button, which here marks hover and the open tab.
 */
constexpr const char *VanillaButtonPath = "ui_art\\but_sml";
constexpr int VanillaButtonFrameCount = 15;
constexpr int VanillaFaceTop = 1;
constexpr int VanillaFaceWidth = 110;
constexpr int VanillaFaceHeight = 27;
/** @brief The ring and the bevel, kept whole on every side whatever size the control is. */
constexpr int VanillaFaceCap = 8;
/** @brief A touch under the button's own brightness, so it separates from the limestone around it. */
constexpr uint32_t VanillaGreyPercent = 90;

enum class VanillaFace : uint8_t {
	Rest,
	Pressed,
	Lit,
};

struct VanillaButtonFaces {
	bool loaded = false;
	/** @brief One 110x27 face per VanillaFace, as XRGB values. */
	std::array<std::vector<uint32_t>, 3> faces;
	/**
	 * @brief The same three faces in GOLD - the SELECTED control (user, 2026-09-25 dev note: "all vendors tabs new
	 * rule - use gold backing for the selected tab"; the inventory's tab plates have been grey/gold since
	 * v1.9.290, the rule the vendors' tabs and the books' buttons now share).
	 */
	std::array<std::vector<uint32_t>, 3> goldFaces;
};

/** @brief The three faces, decoded once. Not loaded if the archive has no but_sml. */
const VanillaButtonFaces &GetVanillaButtonFaces()
{
	static VanillaButtonFaces art;
	static bool tried = false;
	if (tried)
		return art;
	tried = true;

	std::array<SDL_Color, 256> palette {};
	const OptionalOwnedClxSpriteList sprites = LoadPcxSpriteList(VanillaButtonPath, VanillaButtonFrameCount, std::nullopt, palette.data(), /*logError=*/false);
	if (!sprites) {
		LogError("Shop buttons: {}.pcx did not load - the limestone plates stand in", VanillaButtonPath);
		return art;
	}
	if (ClxSpriteList { *sprites }.numSprites() < art.faces.size()) {
		LogError("Shop buttons: {}.pcx has {} frames, needs {} - the limestone plates stand in", VanillaButtonPath,
		    ClxSpriteList { *sprites }.numSprites(), art.faces.size());
		return art;
	}
	std::array<uint32_t, 256> grey {};
	std::array<uint32_t, 256> gold {};
	for (size_t i = 0; i < grey.size(); i++) {
		const SDL_Color c = palette[i];
		const uint32_t luma = (299U * c.r + 587U * c.g + 114U * c.b) / 1000U;
		const uint32_t value = std::min<uint32_t>(255U, luma * VanillaGreyPercent / 100U);
		grey[i] = (value << 16) | (value << 8) | value;
		// Gold of the same brightness: the red channel ahead, the blue far behind - the ramp the game's own
		// gold lettering sits on, so the bevel and the ring read exactly as on the grey face.
		const uint32_t r = std::min<uint32_t>(255U, luma * 125U / 100U);
		const uint32_t g = std::min<uint32_t>(255U, luma * 98U / 100U);
		const uint32_t b = std::min<uint32_t>(255U, luma * 42U / 100U);
		gold[i] = (r << 16) | (g << 8) | b;
	}
	for (size_t f = 0; f < art.faces.size(); f++) {
		const ClxSprite sprite = (*sprites)[f];
		if (sprite.width() < VanillaFaceWidth || sprite.height() < VanillaFaceTop + VanillaFaceHeight) {
			LogError("Shop buttons: {}.pcx frame {} is {}x{}, smaller than the {}x{} face - the limestone plates stand in",
			    VanillaButtonPath, f, sprite.width(), sprite.height(), VanillaFaceWidth, VanillaFaceTop + VanillaFaceHeight);
			return art;
		}
		for (const bool golden : { false, true }) {
			const OwnedSurface scratch = OwnedSurface::Rgb(sprite.width(), sprite.height());
			RenderClxSpriteWithRgbMap(scratch, sprite, { 0, 0 }, golden ? gold.data() : grey.data());
			std::vector<uint32_t> &face = golden ? art.goldFaces[f] : art.faces[f];
			face.resize(static_cast<size_t>(VanillaFaceWidth) * VanillaFaceHeight);
			for (int y = 0; y < VanillaFaceHeight; y++) {
				for (int x = 0; x < VanillaFaceWidth; x++)
					face[static_cast<size_t>(y) * VanillaFaceWidth + x] = *scratch.at<uint32_t>(x, VanillaFaceTop + y) & 0xFFFFFFU;
			}
		}
	}
	art.loaded = true;
	return art;
}

/**
 * @brief Draws @p which across @p rect at 1:1 - the ends kept, the middle repeated or cut (see
 * SliceButtonAxis).
 *
 * @p onItsSide lays the button down the rect instead of across it, for the tabs. Transposed rather
 * than rotated, so the light still falls from the top left as it does on every button beside it.
 * False when there is nothing to draw with, and the caller draws its fallback.
 */
bool DrawVanillaButton(const Surface &out, Rectangle rect, VanillaFace which, bool onItsSide, bool golden = false)
{
	if (out.isIndexed())
		return false;
	const VanillaButtonFaces &art = GetVanillaButtonFaces();
	if (!art.loaded)
		return false;
	const std::vector<uint32_t> &face = (golden ? art.goldFaces : art.faces)[static_cast<size_t>(which)];
	const int sourceWidth = onItsSide ? VanillaFaceHeight : VanillaFaceWidth;
	const int sourceHeight = onItsSide ? VanillaFaceWidth : VanillaFaceHeight;
	const std::vector<ButtonSliceSpan> columns = SliceButtonAxis(rect.size.width, sourceWidth, VanillaFaceCap);
	const std::vector<ButtonSliceSpan> rows = SliceButtonAxis(rect.size.height, sourceHeight, VanillaFaceCap);
	for (const ButtonSliceSpan &row : rows) {
		for (int j = 0; j < row.length; j++) {
			for (const ButtonSliceSpan &column : columns) {
				for (int i = 0; i < column.length; i++) {
					const Point target = rect.position + Displacement { column.dest + i, row.dest + j };
					if (!out.InBounds(target))
						continue;
					const size_t u = static_cast<size_t>(column.source + i);
					const size_t v = static_cast<size_t>(row.source + j);
					*out.at<uint32_t>(target) = onItsSide ? face[u * VanillaFaceWidth + v] : face[v * VanillaFaceWidth + u];
				}
			}
		}
	}
	return true;
}

/**
 * @brief Draws @p text along @p rect, reading top to bottom - the label of a tab standing on its side.
 *
 * The engine has no rotated text, so the line is drawn flat on a scratch surface and copied a quarter
 * turn clockwise, the tops of the letters to the right. It replaces the stack of single letters
 * (DrawVerticalLabel, still the fallback for an indexed target). The font steps down from 12 until the
 * label fits, as the stacked version's did.
 */
bool DrawSidewaysLabel(const Surface &out, string_view text, Rectangle rect, UiFlags color)
{
	if (out.isIndexed() || rect.size.width <= 0 || rect.size.height <= 0)
		return false;
	const int length = rect.size.height;
	const int thickness = rect.size.width;
	constexpr int EndMargin = 4;
	UiFlags font = UiFlags::FontSize8;
	for (const UiFlags candidate : { UiFlags::FontSize12, UiFlags::FontSize11, UiFlags::FontSize10, UiFlags::FontSize9 }) {
		// +1 for the shadow, which sits a pixel past the last glyph.
		if (GetLineWidth(text, GetFontSizeFromUiFlags(candidate)) + 1 <= length - 2 * EndMargin) {
			font = candidate;
			break;
		}
	}
	constexpr uint32_t Key = 0xFF00FFU; // magenta: in no font colour and no shadow
	const OwnedSurface scratch = OwnedSurface::Rgb(length, thickness);
	for (int y = 0; y < thickness; y++) {
		for (int x = 0; x < length; x++)
			*scratch.at<uint32_t>(x, y) = Key;
	}
	DrawString(scratch, text, Rectangle { { 0, 0 }, { length, thickness } },
	    { color | font | UiFlags::AlignCenter | UiFlags::VerticalCenter | UiFlags::Shadowed });
	for (int y = 0; y < thickness; y++) {
		for (int x = 0; x < length; x++) {
			const uint32_t pixel = *scratch.at<uint32_t>(x, y);
			if ((pixel & 0xFFFFFFU) == Key)
				continue;
			const Point target = rect.position + Displacement { thickness - 1 - y, x };
			if (out.InBounds(target))
				*out.at<uint32_t>(target) = pixel;
		}
	}
	return true;
}

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
	/** Wirt's two grids: lay out a fresh stock, free (user, 2026-09-20: "introduce Refresh buttons to Wirts two shops"). */
	Refresh,
	/**
	 * Griswold's redesigned page only (2026-09-21). Sell arms the hammer to sell ONE item where it
	 * lies; Sell all is the bulk sale that was a text action on the second row before the redesign
	 * gave every service a painted frame of its own.
	 */
	Sell,
	SellAll,
	/** The opt-in reroll-until-you-like-it, drawn apart from the six below the grid (2026-09-21). */
	RefreshUntil,
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
	case TalkID::BoyBuy:
	case TalkID::BoyGamble:
		return N_("WIRT");
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

/*
 * GRISWOLD'S REDESIGNED PAGE (user, 2026-09-21).
 *
 * His tabs wear their own painted 340x720 canvas - the forge, with Griswold standing at it - instead
 * of the shared limestone side panel, carry no title band, and put SIX service buttons in permanent
 * painted frames over the painting with his gold at the foot.
 *
 * EVERY NUMBER BELOW IS MEASURED, not chosen. The user supplied a plain canvas and a guide canvas
 * carrying the frames and the gold in place; diffing the two gives the marks' exact boxes, which is
 * what these are. Three frames stand either side of Griswold, who occupies the middle of his own
 * painting - so the row reads as two groups of three rather than one run of six, and the order the
 * user gave (Repair, Repair All, Recharge | Sell, Sell All, Refresh) falls into that split as the
 * three things done TO your gear and the three done WITH the shelf.
 */
constexpr const char *GriswoldCanvasAsset = "ui\\griswold_canvas.png";
/**
 * The same forge with the 10x16 grid's frame painted into it (user, 2026-09-21), for the tabs that
 * SHOW that grid. His Salvage page keeps the frameless cut above, because it has no grid to frame -
 * which is what "Grid Tabs Only" in the file's own name says.
 *
 * Measured on it: the ornate band runs x 26..313 and y 159..628, around the grid's 280x448 at
 * (30,170) - the same 3px-and-a-black-pixel relationship the other framed canvases have.
 */
constexpr const char *GriswoldGridCanvasAsset = "ui\\griswold_canvas_grid.png";

/**
 * THE VENDOR PORTRAITS (user, 2026-09-21: "take Pepin's new canvas and apply it"; "the same now
 * with Wirt").
 *
 * One painted 340x720 canvas per shop - the vendor standing or sitting in their own place, with the
 * 10x16 grid's frame painted into the picture. MEASURED before a line was written, both of them:
 * each one's ornate band runs x 26..313 and y 159..628, which is the band the stash canvas, the
 * shared grid canvas and Griswold's framed forge all carry, around the same 280x448 grid at
 * (30,170). So these are PAINTING SWAPS, not geometry changes - nothing moves for them, and that is
 * the whole reason they drop in as a table entry.
 *
 * Each takes the four redactions the stash's canvas got the same day ("Apply same redactions to
 * canvas as Stash Canvas if relevant"): no title over the portrait, no dim over the art, no second
 * bezel inside the painted one, and one pass of fill under the grid instead of two. The fifth - the
 * cast shadow - was never drawn here, and the sixth was the stash's own SORT button.
 *
 * Unlike Griswold these are one asset each, not two. He needs a frameless cut for Salvage, which has
 * no grid to frame; Pepin's single tab and Wirt's two both show the shelf, so one file serves.
 */
constexpr const char *PepinCanvasAsset = "ui\\pepin_canvas.png";
constexpr const char *WirtCanvasAsset = "ui\\wirt_canvas.png";
constexpr const char *AdriaCanvasAsset = "ui\\adria_canvas.png";
constexpr const char *ShopButtonFrameAsset = "ui\\shop_button_frame.png";
constexpr const char *ShopGoldIconAsset = "ui\\shop_gold_icon.png";

/**
 * Six in the row over the painting, plus a SEVENTH set apart below the grid beside the gold: "Refresh
 * until" (user, 2026-09-21: "Refresh until is a bit of a cheat, so if someone activates it put a
 * button somewhere bellow the grid near the gold counter").
 *
 * Its distance from the row is the point, not a leftover. The six are Griswold's ordinary services;
 * this one rerolls the shelf until something wanted appears, it is off by default, and the user calls
 * it a cheat - so it is drawn where an opt-in convenience belongs, apart from the honest six, and it
 * is not there at all unless the option that grants it is on.
 */
constexpr int ShopServiceSlotCount = 7;
constexpr int ShopRefreshUntilSlot = 6;
/** @brief The frame art's own size; the guide's marks are exactly this, so the origins are the frames'. */
constexpr Size ShopServiceSlotSize { 34, 34 };
/**
 * The row sits FOUR pixels clear of the grid frame's top (user, 2026-09-21: "move the buttons 4px
 * above grid frame"). The framed canvas begins its ornate band at y 159 - checked level at that row
 * across the whole span, x 30 to 310 - and a 34px button ending four pixels short of it starts at
 * 121: 121..154 of button, 155..158 of air, 159 of frame.
 *
 * It was 128, which was right against the guide canvas and three pixels into the frame on this one.
 * Moved rather than cropped, because these are the user's painted 34x34 plates and a crop would have
 * cut through the frame drawn into the art itself.
 */
constexpr Point ShopServiceSlotAt[ShopServiceSlotCount] = {
	{ 24, 121 }, { 60, 121 }, { 96, 121 },
	{ 210, 121 }, { 246, 121 }, { 282, 121 },
	// Clear of the gold count, which starts at x=25 and cannot run past ~x=105 even at eight digits.
	{ 120, 627 }
};
/** @brief In the user's stated order: "Repair, Repair All, Recharge, Sell, Sell All, Refresh". */
constexpr ServiceButton ShopServiceSlotDoes[ShopServiceSlotCount] = {
	ServiceButton::Repair, ServiceButton::RepairAll, ServiceButton::Recharge,
	ServiceButton::Sell, ServiceButton::SellAll, ServiceButton::Refresh,
	ServiceButton::RefreshUntil
};
/** @brief RfA-25's 24x24 glyphs (batch 48) - the size that fits a 34x34 frame with an even margin. */
constexpr const char *ShopServiceSlotGlyph[ShopServiceSlotCount] = {
	"ui\\shop_glyph_repair.png", "ui\\shop_glyph_repair_all.png", "ui\\shop_glyph_recharge.png",
	"ui\\shop_glyph_sell.png", "ui\\shop_glyph_sell_all.png", "ui\\shop_glyph_refresh.png",
	// TWO DICE (2026-09-22). It wore Refresh's own glyph and was told apart only by its position and
	// its hover text - two buttons on one painting carrying the same picture. Refresh keeps the
	// circular arrows for one restock; this one rolls again and again until the stock answers, which
	// is what a second die says and an identical pair of arrows does not.
	"ui\\shop_glyph_refresh_until.png"
};
// ShopServiceGlyphInset (5, from a 24 px glyph in a 34 px frame) is GONE: the draw centres the glyph
// on whatever size the file actually is, so the art can change without a constant here going stale.

/**
 * The gold pile and its count at the foot of the painting, below the grid.
 *
 * The icon's origin is back-calculated: the guide's mark is the pile's INK at (27,632) and the ink
 * sits five rows down inside the 28x28 file, so the file goes at (27,627). Placing the file where
 * the ink was measured would have dropped the pile five pixels.
 */
constexpr Point ShopGoldIconAt { 27, 627 };
constexpr Point ShopGoldCountAt { 25, 654 };
constexpr int ShopGoldCountHeight = 16;

/** @brief Griswold's shop tabs - the ones the redesign dresses. The Salvage page is its own window. */
bool IsSmithShopScreen(TalkID id)
{
	return IsAnyOf(id, TalkID::SmithBuy, TalkID::SmithPremiumBuy, TalkID::SmithUniqueBuy,
	    TalkID::SmithRareBuy, TalkID::SmithSetBuy, TalkID::SmithConsumables, TalkID::SmithSell,
	    TalkID::SmithRepair, TalkID::SmithRecharge);
}

/**
 * @brief Whether @p id draws the redesigned page rather than the old limestone-and-rows layout.
 *
 * Gated on the ART, not only on the vendor: without the canvas the six frames would float over bare
 * limestone with nothing to anchor them, so a build short of the asset keeps the layout that matches
 * what it can draw. Every other vendor keeps that layout too - this canvas is Griswold's forge, and
 * Adria at a forge would be worse than Adria on limestone.
 */
bool IsRedesignedShopScreen(TalkID id)
{
	return IsSmithShopScreen(id) && HasShopArt(GriswoldCanvasAsset);
}

/** @brief The portrait this page WOULD wear, whether or not the file is installed. */
const char *PortraitCanvasFor(TalkID id)
{
	switch (id) {
	case TalkID::HealerBuy:
		return PepinCanvasAsset;
	case TalkID::BoyBuy:
	case TalkID::BoyGamble:
		// Both of Wirt's tabs, Shop and Gamble. Both show a shelf, so both want the frame.
		return WirtCanvasAsset;
	case TalkID::WitchBuy:
	case TalkID::WitchSell:
		// Adria's two, and the last vendor off the shared limestone canvas (2026-09-21).
		return AdriaCanvasAsset;
	default:
		return nullptr;
	}
}

/**
 * @brief The portrait this page actually HAS, or nullptr - the one question the draw needs answered.
 *
 * Gated on the art for the same reason Griswold's page is: without the canvas the page would lose
 * its title and its bezel and get nothing in exchange, which is a worse window than the one it
 * replaced. A build short of the asset keeps the limestone layout that matches what it can draw.
 *
 * Returning the asset rather than a bool is what keeps the next vendor to one line in the table
 * above: the draw asks "which painting" once and uses the answer for all four redactions.
 */
const char *PaintedPageCanvas(TalkID id)
{
	const char *asset = PortraitCanvasFor(id);
	return (asset != nullptr && HasShopArt(asset)) ? asset : nullptr;
}

/** @brief Where a slot's frame sits on screen. */
Rectangle ShopServiceSlotRect(int slot)
{
	const Rectangle panel = GetShopPanelRect();
	return Rectangle { panel.position + Displacement { ShopServiceSlotAt[slot].x, ShopServiceSlotAt[slot].y },
		ShopServiceSlotSize };
}

/**
 * @brief Whether slot @p slot can do anything on tab @p id.
 *
 * Repair, Repair All, Recharge and Sell are Griswold's OWN services and are live on every one of his
 * tabs: the frames are permanent fixtures now, so gating them per tab the way the old variable row
 * did would leave holes in a painted row. Sell All and Refresh stay gated, because they act on the
 * TAB rather than on the player - and Refresh in particular is absent from Unique and Set by a rule
 * worth keeping (both shelves are drawn without replacement, so a refresh would reshuffle the same
 * contents and read as broken).
 */
/**
 * @brief Whether this page's Refresh is WIRT'S - a free reroll of the boy's shelf.
 *
 * His has never been one of the store's action rows: GetShopActions returns nothing for his tabs and
 * the old wide button called RefreshBoyStock directly. So ShopTabHasRefresh is false on both of them,
 * and a painted Refresh frame that asked only that question would sit greyed out forever on the one
 * vendor the user asked to have it.
 */
bool PageRefreshesBoyStock(TalkID id)
{
	return IsAnyOf(id, TalkID::BoyBuy, TalkID::BoyGamble);
}

bool ShopServiceSlotEnabled(int slot, TalkID id)
{
	switch (ShopServiceSlotDoes[slot]) {
	case ServiceButton::SellAll:
		return ShopTabHasSellAll(id);
	case ServiceButton::Refresh:
		return ShopTabHasRefresh(id) || PageRefreshesBoyStock(id);
	case ServiceButton::RefreshUntil:
		return ShopTabHasRefreshUntil(id);
	default:
		return true;
	}
}

/**
 * @brief Whether the slot is drawn at all - as opposed to drawn greyed.
 *
 * Only "Refresh until" can be absent. The six are fixtures and grey out when a tab cannot do them;
 * this one is an opt-in the user calls a cheat, so when the option is off it is not a disabled button
 * to wonder about - it is simply not part of the shop.
 */
bool ShopServiceSlotVisible(int slot, TalkID id)
{
	if (ShopServiceSlotDoes[slot] != ServiceButton::RefreshUntil)
		return true;
	return ShopTabHasRefreshUntil(id);
}

/** @brief Defined below, beside the rest of the control row it builds. */
std::vector<ServiceButton> ServicesFor(TalkID id);

/**
 * @brief Which of the seven painted frames page @p id actually puts on screen.
 *
 * Griswold shows the lot. A PORTRAIT page shows Griswold's plate for EVERY service its own control
 * row carried, at Griswold's own position for that service (user, 2026-09-21: "just for wirt -
 * remove the current refresh button and use Griswold one", then the same for Adria).
 *
 * Stated as "whatever the row had" rather than as a list of vendors, which is what made Adria free:
 * Wirt's row carried Refresh and Pepin's carried nothing, so Wirt got one plate and Pepin none - and
 * Adria's carries Recharge, so she gets Griswold's Recharge plate without a word of new code. A rule
 * about SERVICES rather than about NAMES is what stops the next vendor needing a case here.
 *
 * Note for Adria specifically: she has no Refresh to replace. GetShopActions has no Witch case and
 * her row's one service is Recharge, so that is the button that became a painted plate.
 *
 * THIS is the single question the draw, the hit test, the hover and the release all ask. They used to
 * ask IsRedesignedShopScreen and then ShopServiceSlotVisible separately, which is two places for a
 * new page to be added to and one to be forgotten in.
 */
bool ShopServiceSlotOnPage(int slot, TalkID id)
{
	if (IsRedesignedShopScreen(id))
		return ShopServiceSlotVisible(slot, id);
	if (PaintedPageCanvas(id) == nullptr)
		return false;
	const std::vector<ServiceButton> services = ServicesFor(id);
	return std::find(services.begin(), services.end(), ShopServiceSlotDoes[slot]) != services.end();
}

/** @brief Whether any painted frame is on this page - the gate the four call sites share. */
bool PageHasServiceFrames(TalkID id)
{
	for (int slot = 0; slot < ShopServiceSlotCount; slot++) {
		if (ShopServiceSlotOnPage(slot, id))
			return true;
	}
	return false;
}

/**
 * @brief Whether @p service already has a painted frame here, and so must NOT also be a row button.
 *
 * The one rule that keeps Wirt from having two Refreshes: the frame is drawn from the slot table and
 * the row is built from ServicesFor, and without this they would both be right.
 */
bool ServiceHasPaintedFrame(ServiceButton service, TalkID id)
{
	for (int slot = 0; slot < ShopServiceSlotCount; slot++) {
		if (ShopServiceSlotDoes[slot] == service && ShopServiceSlotOnPage(slot, id))
			return true;
	}
	return false;
}

/** @brief The slot being held down, or -1. The action runs on the release, inside the same slot. */
int PressedShopServiceSlot = -1;
/** @brief The slot the cursor was last over, so the hover sound fires once on entry. */
int LastHoverShopServiceSlot = -1;

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
	case TalkID::BoyBuy:
	case TalkID::BoyGamble:
		return { ServiceButton::Refresh };
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
	case ServiceButton::Refresh:
		return std::string(_("Refresh"));
	case ServiceButton::Sell:
		return std::string(_("Sell"));
	case ServiceButton::SellAll:
		return std::string(_("Sell all"));
	case ServiceButton::RefreshUntil:
		return std::string(_("Refresh until"));
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
	for (ServiceButton service : ServicesFor(id)) {
		// Not twice. A service with a painted frame on this page is drawn there and nowhere else
		// (user, 2026-09-21: "just for wirt - remove the current refresh button and use Griswold
		// one") - this is the removal half of that sentence, and the slot table is the other.
		if (ServiceHasPaintedFrame(service, id))
			continue;
		buttons.push_back({ ControlKind::Service, TalkID::None, service, 0, ServiceButtonLabel(service) });
	}
	for (const ShopAction &action : GetShopActions(id))
		buttons.push_back({ ControlKind::Action, TalkID::None, ServiceButton::Repair, action.line, std::string(_(action.label)) });
	return buttons;
}

/**
 * @brief Where one control sits. Services take the first row, bulk actions the second.
 *
 * Each row shares its full width between whatever is on it, so three services are 92px each, 3px apart, and one
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
	// The same 3px gap between buttons on a row as between the rows, and the row centred on whatever
	// the division leaves over.
	const int count = std::max(onRow, 1);
	const int width = (ShopControlsWidth - ShopControlGap * (count - 1)) / count;
	const int used = width * count + ShopControlGap * (count - 1);
	const int left = panel.position.x + ShopControlsLeft + (ShopControlsWidth - used) / 2 + before * (width + ShopControlGap);
	return Rectangle { { left, panel.position.y + top }, { width, height } };
}

/**
 * @brief How many tabs the column has room for before it runs past the panel's foot.
 *
 * The three-row strip this replaced had an assert against its own capacity, and moving the tabs out
 * of the panel dropped it. Restored, because the failure it guards is the silent kind: a vendor that
 * grew an eighth tab would simply draw it further down the screen, over the world, and nothing would
 * say so.
 *
 * Vertical tabs are TALL, so unlike the old horizontal column this is a real bound rather than a
 * distant tripwire - hence the static_assert below, which fails the build rather than the run if a
 * height or gap change squeezes out a tab Griswold already has.
 */
constexpr int ShopTabColumnSlots = (ShopPanelSize.height - ShopTabColumnTop) / (ShopTabHeight + ShopTabGap);

/** @brief Basic, Magic, Rare, Set, Unique, Supplies, Sold, Transmute - Griswold with every shelf switched on. */
constexpr int ShopMaxTabsPerVendor = 8;
static_assert(ShopTabColumnSlots >= ShopMaxTabsPerVendor,
    "the tab column no longer fits a fully-stocked Griswold - shorten ShopTabHeight");

/** @brief One tab in the column, flush against the panel's right edge. */
Rectangle ShopTabRect(size_t index)
{
	assert(index < static_cast<size_t>(ShopTabColumnSlots)
	    && "a vendor has more tabs than the column has room for - the strip would run off the panel");
	const Rectangle panel = GetShopPanelRect();
	return Rectangle { { panel.position.x + panel.size.width,
	                       panel.position.y + ShopTabColumnTop + static_cast<int>(index) * (ShopTabHeight + ShopTabGap) },
		{ ShopTabColumnWidth, ShopTabHeight } };
}

/**
 * @brief Draws @p text one character per line down @p rect - a vertical label.
 *
 * The engine has no rotated text, so a vertical label is a stack of glyphs. Uppercased first: caps
 * have no descenders, which is what lets the pitch be tightened below the font's own line height
 * without letters touching, and a stack of mixed-case letters reads worse than a stack of caps
 * anyway.
 *
 * The PITCH comes from the label's length against the height available, and the FONT is then the
 * largest that clears that pitch. A long label therefore shrinks rather than overflowing - which is
 * the failure this panel has already had once, when "Supplies" rendered as "SUPPLIE" and the clip
 * was silent. At the 80px tab height, an eight-letter label lands on FontSize10.
 */
void DrawVerticalLabel(const Surface &out, string_view text, Rectangle rect, UiFlags color)
{
	// Split into CODE POINTS, not bytes (external audit of v1.9.88, finding 5).
	//
	// The first version walked `char`s and handed each single byte to DrawString. That is fine for
	// English and broken for every translation with a non-ASCII tab name - Bulgarian and Russian
	// render "Magic" as "Магия", Simplified Chinese as "魔法", and each byte of those is invalid
	// UTF-8 on its own. DrawString's loop stops at Utf8DecodeError, so the label came out blank or
	// truncated. The pitch was wrong too, computed from a byte count that is two or three times the
	// number of characters, so a translated label was squeezed to a fraction of its space.
	//
	// Uppercasing is ASCII-only and deliberately stays that way: it is a cosmetic touch that lets the
	// pitch tighten (capitals have no descenders), and a Unicode-aware transform would need case
	// tables this engine does not carry. A Cyrillic or CJK label simply keeps its own case, which is
	// correct - CJK has none, and Cyrillic tab names are already capitalised by the translator.
	std::vector<std::string> glyphs;
	glyphs.reserve(text.size());
	string_view rest = text;
	while (!rest.empty()) {
		std::size_t len = 0;
		const char32_t cp = DecodeFirstUtf8CodePoint(rest, &len);
		if (len == 0)
			break; // malformed tail - stop rather than loop forever on it
		std::string one(rest.substr(0, len));
		if (cp != Utf8DecodeError && one.size() == 1 && one[0] >= 'a' && one[0] <= 'z')
			one[0] = static_cast<char>(one[0] - 'a' + 'A');
		glyphs.push_back(std::move(one));
		rest.remove_prefix(len);
	}
	if (glyphs.empty())
		return;

	const int count = static_cast<int>(glyphs.size());
	const int pitch = std::max(1, std::min(12, rect.size.height / count));
	// The four small font sizes exist for exactly this squeeze. Below eight there is nothing
	// smaller and the letters simply tighten by a pixel.
	const UiFlags font = pitch >= 12 ? UiFlags::FontSize12
	    : pitch >= 11                ? UiFlags::FontSize11
	    : pitch >= 10                ? UiFlags::FontSize10
	    : pitch >= 9                 ? UiFlags::FontSize9
	                                 : UiFlags::FontSize8;
	// Centred in whatever is left over, so a short label sits in the middle of the tab rather than
	// hanging from its top edge.
	int y = rect.position.y + (rect.size.height - count * pitch) / 2;
	for (const std::string &glyph : glyphs) {
		// A space is a gap, not a glyph - nothing in the tab names has one today, but a two-word
		// tab would otherwise read as one run of letters.
		if (glyph != " ") {
			// The whole encoded code point, so DrawString gets valid UTF-8 rather than one byte of it.
			DrawString(out, string_view(glyph),
			    Rectangle { { rect.position.x, y }, { rect.size.width, pitch } },
			    { color | font | UiFlags::AlignCenter });
		}
		y += pitch;
	}
}

constexpr int PageButtonWidth = 16;

/** @brief The two page arrows, at the ends of the gold row. 0 is back, 1 is forward. */
Rectangle ShopPageButtonRect(int index)
{
	const Rectangle panel = GetShopPanelRect();
	const int left = index == 0
	    ? panel.position.x + ShopTabStripLeft
	    : panel.position.x + ShopTabStripLeft + ShopTabStripWidth - PageButtonWidth;
	// The redesigned page has no gold ROW - the gold is a pile and a number at the foot of the
	// painting - so the arrows ride at the far end of that line instead, where nothing else sits.
	// Answered HERE rather than at each caller so the draw and the hit test cannot disagree.
	const int top = IsRedesignedShopScreen(stextflag) ? ShopGoldCountAt.y : ShopGoldTop;
	return Rectangle { { left, panel.position.y + top }, { PageButtonWidth, ShopGoldHeight } };
}

/** @brief The red X, top-right, same as every other Oracool window's. */
Rectangle ShopCloseRect()
{
	// The SHARED rect (audit, 2026-08-31). This was hand-rolled at right-34 / top+14, 20x20, and so
	// the shop's X sat 13px left and 11px lower than the one on every other window - including the
	// stash, character sheet and quest log, which are the same 340x720 panel in the same
	// bottom-left slot. It was also drawn as a text glyph in an ornate border rather than the
	// shared red plate, so it did not even look like the same control.
	//
	// The shared position is already proven to coexist with a PanelTitleTop title band, because
	// every one of those panels has one.
	return GetWindowCloseButtonRect(GetShopPanelRect());
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
		// One sentence, one string: the card wraps it (2026-09-27), where the info box needed it cut in two.
		AddPanelString(_("Repairs everything you carry and wear, dearest first, until your gold runs out."), UiFlags::ColorWhite);
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
	case ServiceButton::Refresh:
		SetPanelString(_("Refresh"), UiFlags::ColorWhitegold);
		AddPanelString(_("A fresh stock is laid out on this tab. Free."), UiFlags::ColorWhite);
		break;
	case ServiceButton::Sell:
		SetPanelString(_("Sell an Item"), UiFlags::ColorWhitegold);
		AddPanelString(_("Click for the hammer, then click any item to sell it."), UiFlags::ColorWhite);
		AddPanelString(_("Or drop an item anywhere on this panel."), UiFlags::ColorWhite);
		break;
	case ServiceButton::SellAll:
		SetPanelString(_("Sell All"), UiFlags::ColorWhitegold);
		AddPanelString(_("Sells everything on the backpack's first page this vendor will take. Pages 2-10 are left alone."), UiFlags::ColorWhite);
		break;
	case ServiceButton::RefreshUntil: {
		SetPanelString(_("Refresh Until"), UiFlags::ColorWhitegold);
		AddPanelString(_("Lays out fresh stock over and over until something you asked for turns up."), UiFlags::ColorWhite);
		// What the old info-box explainer on the text store's row said (DrawRefreshUntilHoverTooltip), folded into the
		// card: what it is looking for, and that the stock follows the hero's level.
		AddPanelString(_("Click to name what to look for, then confirm to start."), UiFlags::ColorWhite);
		const std::string lookingFor = ShopRefreshUntilLookingFor();
		if (lookingFor.empty())
			AddPanelString(_("Nothing named yet."), UiFlags::ColorRed);
		else
			AddPanelString(StrCat(_("Last search"), ": ", lookingFor), UiFlags::ColorWhitegold);
		AddPanelString(_("What can turn up depends on your level."), UiFlags::ColorWhite);
		break;
	}
	}
}

// Defined below with the rest of Griswold's furniture, which the portrait pages now borrow.
void DrawShopServiceFrames(const Surface &out);
void DrawShopGoldPile(const Surface &out);

/**
 * @brief The two page arrows, each in its slot frame and sinking while held (user, 2026-10-02: "all buttons in
 * vendors/artisans ... make sinkable"). The page turns on the click as before; the sink only shows the hold.
 */
void DrawShopPageArrows(const Surface &out)
{
	for (int i = 0; i < 2; i++) {
		const Rectangle rest = ShopPageButtonRect(i);
		const bool pressed = rest.contains(MousePosition) && sgbMouseDown == CLICK_LEFT;
		DrawButtonSlotGround(out, rest, pressed);
		const Rectangle rect { rest.position + (pressed ? ButtonSlotSink : Displacement { 0, 0 }), rest.size };
		DrawOrnateBorder(out, rect);
		DrawString(out, i == 0 ? "<" : ">", rect,
		    { UiFlags::ColorWhite | UiFlags::FontSize12 | UiFlags::AlignCenter | UiFlags::VerticalCenter });
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
	const bool buttonArt = HasShopArt(ShopButtonArt);
	// Every button in its slot frame (user, 2026-10-02), the whole row's frames and shadows FIRST: the buttons
	// stand three pixels apart, inside the six a frame reaches, so a neighbour's frame drawn later would cut
	// into a face. Drawn first they merge into one carved wall between the faces.
	for (size_t i = 0; i < buttons.size(); i++) {
		const Rectangle rest = ShopControlRect(buttons, i);
		DrawButtonSlotGround(out, rest, rest.contains(MousePosition) && sgbMouseDown == CLICK_LEFT);
	}
	for (size_t i = 0; i < buttons.size(); i++) {
		const Rectangle rest = ShopControlRect(buttons, i);
		const bool hovered = rest.contains(MousePosition);
		// Oracool: pressed for as long as the left button is held on it. The shop acts on mouse-DOWN,
		// so this is the span between the click and the release, and sgbMouseDown is that span exactly.
		const bool pressed = hovered && sgbMouseDown == CLICK_LEFT;
		// The face sinks with its frame while held (user, 2026-10-02: "make sinkable"); hit tests stay on the rest rect.
		const Rectangle rect { rest.position + (pressed ? ButtonSlotSink : Displacement { 0, 0 }), rest.size };
		const VanillaFace face = pressed ? VanillaFace::Pressed : hovered ? VanillaFace::Lit : VanillaFace::Rest;
		const bool vanilla = DrawVanillaButton(out, rect, face, /*onItsSide=*/false);
		if (!vanilla && buttonArt) {
			DrawShopButtonPlate(out, rect, pressed ? 2 : hovered ? 1 : 0);
		} else if (!vanilla) {
			// A hover changes the ink and deepens the plate with a SECOND translucent pass (user,
			// 2026-09-05: "when hovering over them add a second dark transparent backing"); the rect
			// itself never moves, so the row does not shift under the pointer.
			DrawHalfTransparentRectTo(out, rect.position.x, rect.position.y, rect.size.width, rect.size.height);
			if (hovered)
				DrawHalfTransparentRectTo(out, rect.position.x, rect.position.y, rect.size.width, rect.size.height);
			DrawOrnateBorder(out, rect);
		}
		// Raised 2px (user, 2026-09-11: "center button labels better by raising them a few px"): the
		// font's letters sit low in their line, so VerticalCenter alone left the words under the plate's
		// middle. The pressed plate's face sits a pixel lower, and the word goes down with it.
		constexpr int LabelLift = 2;
		const int labelShift = ((vanilla || buttonArt) && pressed ? 1 : 0) - LabelLift;
		const Rectangle labelRect { rect.position + Displacement { 0, labelShift }, rect.size };
		// Gold throughout on the vanilla button - its lit ring is the hover (user, 2026-09-11: "gold font").
		DrawString(out, buttons[i].label, labelRect,
		    { (hovered && !vanilla ? UiFlags::ColorWhite : UiFlags::ColorWhitegold)
		        | UiFlags::FontSize12 | UiFlags::AlignCenter | UiFlags::VerticalCenter | UiFlags::Shadowed });
	}

	// A portrait page carries Griswold's furniture instead of this row's own (user, 2026-09-21: "for
	// wirt and pepin - remove the current gold counter and put the Griswold one"): the painted
	// Refresh frame where the page has one, and the pile-and-number at the foot of the painting.
	const bool portrait = PaintedPageCanvas(stextflag) != nullptr;
	if (portrait)
		DrawShopServiceFrames(out);

	// The page arrows share the gold row rather than taking a row of their own: the space between
	// the title band and the grid's pinned top is fully spoken for (see the static_assert above),
	// and the gold readout is one centred line with both ends going spare.
	const Rectangle goldLine { { panel.position.x + ShopControlsLeft, panel.position.y + ShopGoldTop },
		{ ShopControlsWidth, ShopGoldHeight } };
	if (portrait) {
		// The pile at the painting's foot, and the centred "Your gold:" line GONE with it - that
		// line is the "current gold counter" the user asked to be rid of. The arrows below keep
		// their place on this row; they were not part of the request.
		DrawShopGoldPile(out);
	} else {
		// On the bare canvas (user, 2026-09-11: "remove the baground behind gold counter in store"). It sat on
		// the vanilla button pressed in, or the limestone plate, before. Shadowed (user, 2026-09-05: "add text
		// shadow to texts in vendors where needed, like behind the GOLD amount available") - asked for when it
		// was last on the bare canvas, which is where it is again.
		DrawString(out, fmt::format(fmt::runtime(_("Your gold: {:s}")), FormatInteger(TotalPlayerGold())), goldLine,
		    { UiFlags::ColorWhitegold | UiFlags::FontSize12 | UiFlags::AlignCenter | UiFlags::VerticalCenter | UiFlags::Shadowed });
	}

	if (pageCount <= 1)
		return;
	DrawShopPageArrows(out);
	const Rectangle pageLabel { { goldLine.position.x + PageButtonWidth + 2, goldLine.position.y },
		{ 60, ShopGoldHeight } };
	DrawString(out, StrCat(ShopGridPage + 1, "/", pageCount), pageLabel,
	    { UiFlags::ColorWhitegold | UiFlags::FontSize12 | UiFlags::VerticalCenter });
}

/** @brief The 2px down-left sink every pressed button in this mod wears. */
constexpr Displacement ShopServiceSink { -2, 2 };
constexpr int ShopServiceHoverBrighten = 115;

/**
 * @brief Griswold's six service buttons and his gold, drawn over his own canvas.
 *
 * All six frames are drawn on EVERY one of his tabs. One the tab cannot do is desaturated in place
 * rather than left out: the frames are painted fixtures at measured positions now, and a row with a
 * hole in it reads as a bug rather than as a rule. The grey is the same white-hue pass the inactive
 * Act buttons wear, so "not here" looks the same everywhere in the mod.
 */
/**
 * @brief Every painted frame this page carries - Griswold's six-and-one, or a portrait's lone Refresh.
 *
 * Lifted out of DrawRedesignedControls when Wirt's Refresh became one of these (user, 2026-09-21).
 * The loop is unchanged; what changed is that it is now driven by ShopServiceSlotOnPage rather than
 * by being inside the function only Griswold calls, so a page that shows one frame draws it with the
 * same press, grey, hover and glyph-centring rules as a page that shows seven.
 */
void DrawShopServiceFrames(const Surface &out)
{
	const bool frameArt = HasShopArt(ShopButtonFrameAsset);
	// The slot frames and their shadows first, all of them (user, 2026-10-02): the plates stand two pixels
	// apart, so each frame must be under every face, not just its own.
	for (int slot = 0; slot < ShopServiceSlotCount; slot++) {
		if (ShopServiceSlotOnPage(slot, stextflag))
			DrawButtonSlotGround(out, ShopServiceSlotRect(slot), PressedShopServiceSlot == slot);
	}
	int hoveredNow = -1;
	for (int slot = 0; slot < ShopServiceSlotCount; slot++) {
		if (!ShopServiceSlotOnPage(slot, stextflag))
			continue;
		const Rectangle rect = ShopServiceSlotRect(slot);
		const bool hovered = rect.contains(MousePosition);
		const bool enabled = ShopServiceSlotEnabled(slot, stextflag);
		if (hovered)
			hoveredNow = slot;
		// Held down: the face sinks and springs back on the release. The HIT test stays on the unsunk
		// rect, so a button cannot slide out from under a pointer that has not moved.
		const Rectangle face { rect.position + (PressedShopServiceSlot == slot ? ShopServiceSink : Displacement { 0, 0 }), rect.size };
		if (frameArt)
			DrawLoosePng(out, ShopButtonFrameAsset, face.position);
		else
			DrawOrnateBorder(out, face);
		if (const Size glyph = GetLoosePngSize(ShopServiceSlotGlyph[slot]); glyph.width > 0) {
			// CENTRED on the glyph's own size rather than a fixed inset (2026-09-21). The art has
			// changed size once already - 24 px, then 28 when a set arrived at 56 and halved cleanly -
			// and a hard-coded inset silently moves every icon off centre when it does.
			DrawLoosePng(out, ShopServiceSlotGlyph[slot],
			    { face.position.x + (face.size.width - glyph.width) / 2,
			        face.position.y + (face.size.height - glyph.height) / 2 });
		} else {
			// No glyph delivered: the service's own word, which a 34px frame can just hold.
			DrawString(out, ServiceButtonLabel(ShopServiceSlotDoes[slot]), face,
			    { UiFlags::ColorWhitegold | UiFlags::FontSize12 | UiFlags::AlignCenter | UiFlags::VerticalCenter });
		}
		if (!enabled) {
			TintRectRgb(out, face.position.x, face.position.y, face.size.width, face.size.height,
			    0xFFFFFFu, /*brightnessPercent=*/70, /*floorPercent=*/0, PAL16_GRAY);
		} else if (hovered) {
			BrightenRectRgb(out, face.position.x, face.position.y, face.size.width, face.size.height, ShopServiceHoverBrighten);
		}
	}
	if (hoveredNow >= 0 && hoveredNow != LastHoverShopServiceSlot)
		PlayUiMoveSound();
	LastHoverShopServiceSlot = hoveredNow;
}

/**
 * @brief The gold at the foot of the painting: the pile, with the number under it.
 *
 * No plate and no "Your gold:" label - the pile says what the number is. Griswold's since
 * 2026-09-21, and Pepin's and Wirt's from the same day (user: "for wirt and pepin - remove the
 * current gold counter and put the Griswold one").
 *
 * ONE function at ONE pair of positions, which is the point of the request: the three shops now
 * count gold in the same place in the same visual language, and if that place ever moves it moves
 * for all of them.
 */
void DrawShopGoldPile(const Surface &out)
{
	const Rectangle panel = GetShopPanelRect();
	if (HasShopArt(ShopGoldIconAsset))
		DrawLoosePng(out, ShopGoldIconAsset, panel.position + Displacement { ShopGoldIconAt.x, ShopGoldIconAt.y });
	DrawString(out, FormatInteger(TotalPlayerGold()),
	    Rectangle { panel.position + Displacement { ShopGoldCountAt.x, ShopGoldCountAt.y }, { 140, ShopGoldCountHeight } },
	    { UiFlags::ColorWhitegold | UiFlags::FontSize12 | UiFlags::VerticalCenter | UiFlags::Shadowed });
}

void DrawRedesignedControls(const Surface &out, int pageCount)
{
	DrawShopServiceFrames(out);
	DrawShopGoldPile(out);

	if (pageCount <= 1)
		return;
	DrawShopPageArrows(out);
}

/**
 * The tab the player is holding down, and the rect it was pressed at.
 *
 * Every vendor's tabs obey the standing release rule (user, 2026-09-21: "sink holds as long as click and
 * springs back to normal on click release. opening clicked tab counts if release happens within region of
 * button"). The press only sinks the tab and sounds; the shelf opens on the mouse-up, and only when the
 * release lands back inside the tab that was pressed.
 *
 * The RECT is kept beside the id because the column is drawn for two different screens - the shop itself
 * and Griswold's Salvage page, which is not a shop screen - so the release must not have to work out
 * which column the press came from to find the button again.
 */
TalkID PressedShopTab = TalkID::None;
Rectangle PressedShopTabRect { { 0, 0 }, { 0, 0 } };

/** @brief The 2 px down-left sink every pressed button in this mod wears (feedback_button_press_and_sound). */
constexpr Displacement ShopTabSink { -2, 2 };

void DrawShopTabColumn(const Surface &out, TalkID open)
{
	const std::vector<TalkID> tabs = ShopTabsFor(open);
	const bool tabArt = HasShopArt(ShopTabArt);
	// Every tab in its slot frame (user, 2026-10-02), the column's frames first - the tabs are two pixels apart.
	for (size_t i = 0; i < tabs.size(); i++)
		DrawButtonSlotGround(out, ShopTabRect(i), PressedShopTab == tabs[i]);
	for (size_t i = 0; i < tabs.size(); i++) {
		const Rectangle rect = ShopTabRect(i);
		const bool active = tabs[i] == open;
		const bool hovered = rect.contains(MousePosition);
		// Held down: the face sinks 2 px down-left and springs back on the release. The HIT test stays on the
		// unsunk rect, so a tab cannot slide out from under a pointer that has not moved.
		const Rectangle face { rect.position + (PressedShopTab == tabs[i] ? ShopTabSink : Displacement { 0, 0 }), rect.size };
		// The vanilla button on its side: lit for the open shelf, at rest under the pointer and pressed in
		// otherwise, so the shelves not showing step back and the open one stands out.
		const VanillaFace vanillaFace = active ? VanillaFace::Lit : hovered ? VanillaFace::Rest : VanillaFace::Pressed;
		const bool vanilla = DrawVanillaButton(out, face, vanillaFace, /*onItsSide=*/true, /*golden=*/active); // the open shelf in gold (2026-09-25)
		if (!vanilla && tabArt) {
			// Oracool: one 26x80 cell per state - the open shelf, the one under the pointer, or at rest.
			// Its flat edge is on the RIGHT, drawn for a tab left of its panel, so here it faces away from
			// the panel - one of the reasons it is only the fallback now.
			const int state = active ? 2 : hovered ? 1 : 0;
			DrawLoosePngPart(out, ShopTabArt, Rectangle { { state * ShopTabCell.width, 0 }, ShopTabCell }, face.position);
		} else if (!vanilla) {
			// The active tab is filled solid so it reads as part of the panel; the rest are the same
			// half-transparent plate every other floating control wears.
			if (active) {
				DrawThemedFill(out, face, 3);
			} else {
				DrawHalfTransparentRectTo(out, face.position.x, face.position.y, face.size.width, face.size.height);
			}
			DrawOrnateBorder(out, face);
		}
		const string_view label = _(ShopTabName(tabs[i]));
		if (!vanilla || !DrawSidewaysLabel(out, label, face, active ? UiFlags::ColorWhite : UiFlags::ColorWhitegold))
			DrawVerticalLabel(out, label, face, active || hovered ? UiFlags::ColorWhite : UiFlags::ColorWhitegold);
	}
}

/**
 * @brief True if the click landed on a tab, so the caller stops. A LEFT click opens the shelf on the RELEASE.
 *
 * A RIGHT click still switches at once: there is no right mouse-up in this engine to spring the tab back
 * with, so a right-pressed tab would stay sunk until the next left click somewhere else.
 */
bool CheckShopTabColumnClick(Point position, bool rightClick)
{
	if (!rightClick)
		return PressShopTabAt(position, stextflag);
	const std::vector<TalkID> tabs = ShopTabsFor(stextflag);
	for (size_t i = 0; i < tabs.size(); i++) {
		if (!ShopTabRect(i).contains(position))
			continue;
		if (tabs[i] != stextflag) {
			StartStore(tabs[i]);
			ResetShopGridSelection();
			PlayUiMoveSound();
		}
		return true;
	}
	return false;
}

void DrawShopClose(const Surface &out)
{
	// The shared drawing too, not just the shared rect - a button in the right place that still
	// looks like a different control has only half-joined the convention.
	DrawWindowCloseButton(out, GetShopPanelRect());
}

} // namespace

/** @brief The tab column beside the panel, drawn after it so the tabs sit on top of nothing. */
Rectangle GetSideTabRect(int index)
{
	return ShopTabRect(static_cast<size_t>(index));
}

void DrawSideTab(const Surface &out, int index, string_view label, bool active, bool pressed)
{
	const bool tabArt = HasShopArt(ShopTabArt);
	const Rectangle rect = GetSideTabRect(index);
	const bool hovered = rect.contains(MousePosition);
	const Rectangle face { rect.position + (pressed ? ShopTabSink : Displacement { 0, 0 }), rect.size };
	const VanillaFace vanillaFace = active ? VanillaFace::Lit : hovered ? VanillaFace::Rest : VanillaFace::Pressed;
	const bool vanilla = DrawVanillaButton(out, face, vanillaFace, /*onItsSide=*/true, /*golden=*/active); // the open tab in gold (2026-09-25)
	if (!vanilla && tabArt) {
		const int state = active ? 2 : hovered ? 1 : 0;
		DrawLoosePngPart(out, ShopTabArt, Rectangle { { state * ShopTabCell.width, 0 }, ShopTabCell }, face.position);
	} else if (!vanilla) {
		if (active) {
			DrawThemedFill(out, face, 3);
		} else {
			DrawHalfTransparentRectTo(out, face.position.x, face.position.y, face.size.width, face.size.height);
		}
		DrawOrnateBorder(out, face);
	}
	if (!vanilla || !DrawSidewaysLabel(out, label, face, active ? UiFlags::ColorWhite : UiFlags::ColorWhitegold))
		DrawVerticalLabel(out, label, face, active || hovered ? UiFlags::ColorWhite : UiFlags::ColorWhitegold);
}

void DrawSideTabGround(const Surface &out, int index, bool pressed)
{
	DrawButtonSlotGround(out, GetSideTabRect(index), pressed);
}

bool DrawVendorButtonBacking(const Surface &out, Rectangle rect, bool selected, bool hovered)
{
	// The vendors' tab face laid flat: gold and lit when selected, grey at rest under the pointer, grey and
	// pressed in otherwise - the tabs' own three states, so a filter reads like a shelf.
	const VanillaFace which = selected ? VanillaFace::Lit : hovered ? VanillaFace::Rest : VanillaFace::Pressed;
	return DrawVanillaButton(out, rect, which, /*onItsSide=*/false, /*golden=*/selected);
}

void DrawShopTabColumnFor(const Surface &out, TalkID open)
{
	DrawShopTabColumn(out, open);
}

bool PressShopTabAt(Point position, TalkID open)
{
	const std::vector<TalkID> tabs = ShopTabsFor(open);
	for (size_t i = 0; i < tabs.size(); i++) {
		const Rectangle rect = ShopTabRect(i);
		if (!rect.contains(position))
			continue;
		// The tab you are already on presses and springs back like the others; it simply has nothing to open
		// on the release. Pressing it still absorbs the click rather than restarting the screen.
		PressedShopTab = tabs[i];
		PressedShopTabRect = rect;
		PlayUiMoveSound(); // the click sounds at the PRESS, as every other button in this mod does
		return true;
	}
	return false;
}

TalkID TakeReleasedShopTab()
{
	const TalkID pressed = PressedShopTab;
	const Rectangle rect = PressedShopTabRect;
	PressedShopTab = TalkID::None;
	PressedShopTabRect = Rectangle { { 0, 0 }, { 0, 0 } };
	if (pressed == TalkID::None || !rect.contains(MousePosition))
		return TalkID::None; // released off the tab it was pressed on: nothing happens
	return pressed;
}

void ReleaseShopServiceButton()
{
	const int slot = PressedShopServiceSlot;
	PressedShopServiceSlot = -1; // always taken, so a press that outlived its screen cannot fire late
	if (slot < 0 || !PageHasServiceFrames(stextflag))
		return;
	if (!ShopServiceSlotRect(slot).contains(MousePosition))
		return; // released off the button: nothing happens
	if (!ShopServiceSlotOnPage(slot, stextflag) || !ShopServiceSlotEnabled(slot, stextflag))
		return;
	switch (ShopServiceSlotDoes[slot]) {
	case ServiceButton::Repair:
		// The hammer, not an instant repair (user, 2026-08-27): pick it up, then click the item.
		ArmShopRepairCursor();
		break;
	case ServiceButton::RepairAll:
		ShopRepairAll();
		break;
	case ServiceButton::Recharge:
		ArmShopRechargeCursor();
		break;
	case ServiceButton::Sell:
		// The same hammer, on the same gesture, selling instead of repairing (user, 2026-09-21).
		ArmShopSellCursor();
		break;
	case ServiceButton::SellAll:
		ShopRunSellAll(stextflag);
		break;
	case ServiceButton::Refresh:
		// Wirt's shelf is rerolled by its own call, not by a store action row - his tabs have none
		// (see PageRefreshesBoyStock). The frame is Griswold's; what it does is still the boy's.
		//
		// And it keeps the click sound the wide button had: ShopRunRefresh's action path plays one,
		// RefreshBoyStock does not, so without this the button the user asked to REPLACE would come
		// back silent.
		if (PageRefreshesBoyStock(stextflag)) {
			RefreshBoyStock(stextflag);
			PlayUiSelectSound();
		} else {
			ShopRunRefresh(stextflag);
		}
		break;
	case ServiceButton::RefreshUntil:
		ShopRunRefreshUntil(stextflag);
		break;
	}
}

void ReleaseShopTabButton()
{
	// Always taken, so a press that outlived its screen cannot leave a tab sunk or fire late.
	const TalkID tab = TakeReleasedShopTab();
	if (tab == TalkID::None || !IsShopGridScreen(stextflag) || tab == stextflag)
		return;
	StartStore(tab);
	ResetShopGridSelection();
}

TalkID ShopTabAt(Point position, TalkID open)
{
	const std::vector<TalkID> tabs = ShopTabsFor(open);
	for (size_t i = 0; i < tabs.size(); i++) {
		if (ShopTabRect(i).contains(position))
			return tabs[i];
	}
	return TalkID::None;
}

bool ShopVanillaButtonArtLoaded()
{
	return GetVanillaButtonFaces().loaded;
}

bool IsShopGridScreen(TalkID id)
{
	// A TAB is not the same thing as a GRID, and the Salvage tab is where the two part company (user,
	// 2026-09-21: "we need to make sure all tabs for all vendors are to be considered Stores, not
	// windows"). It is one of Griswold's tabs, it is a real store screen, and it draws a painted page
	// instead of a shelf of items - so it belongs in the tab column and in every piece of store
	// machinery, and nowhere near the code that draws and hit-tests a grid of stock.
	//
	// These two questions used to be one function because no tab had ever been anything but a grid.
	return IsShopTab(id) && id != TalkID::SmithTransmute;
}

Rectangle GetShopPanelRect()
{
	// The BOTTOM-left slot, shared with the stash, character sheet and quest log. Nothing else is
	// open while a shop is, so the slot is free - and all four dock together (user, 2026-08-27).
	return Rectangle { { 0, BottomDockedTop(ShopPanelSize.height) }, ShopPanelSize };
}

Rectangle GetShopCloseButtonRect()
{
	return ShopCloseRect();
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
	// Every shop TAB, not just the grid ones: Griswold's Salvage tab is a store screen that draws a
	// painted page in this same rect, and its tab column is the same column. Asking IsShopGridScreen
	// here would have left the column outside the shop's footprint while the Salvage page was up, so
	// a click on a tab would fall through it to whatever is behind - the bug this function exists for.
	if (!IsShopTab(stextflag))
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

/** @brief Which entries of @p stock a placement pass can actually fit on the page. */
std::vector<bool> PlacedFlags(const std::vector<ShopSlot> &stock)
{
	std::vector<bool> onShelf(stock.size(), false);
	for (const PlacedSlot &slot : PlaceStock(stock)) {
		if (slot.stockIndex >= 0 && static_cast<size_t>(slot.stockIndex) < onShelf.size())
			onShelf[static_cast<size_t>(slot.stockIndex)] = true;
	}
	return onShelf;
}

void TrimShopStockToOnePage(TalkID id)
{
	const std::vector<ShopSlot> stock = GetShopStock(id);
	if (stock.empty())
		return;
	const std::vector<bool> onShelf = PlacedFlags(stock);

	for (size_t i = 0; i < stock.size(); i++) {
		// SmithConsumables used to be refused outright, because its stock is Pepin's four infinite
		// potions followed by Adria's array and clearing through that view would empty the potions.
		// That protected the potions and left the real problem standing: Adria's array was sized to
		// fill a page ALONE, so prepending the potions pushed her last few items off the Supplies
		// shelf while they stayed alive in witchitem - a hidden reserve that surfaced the moment
		// one of the visible ones was bought (external audit of v1.9.97, finding 2).
		//
		// The refusal is now per-ENTRY instead of per-tab: a protected entry is placed and shown
		// like any other but never cleared, so the combined shelf can be materialised without
		// touching the fixtures on it.
		if (onShelf[i] || stock[i].neverTrim)
			continue;
		stock[i].item->clear();
	}
}

bool ShopStockFitsOnePage(TalkID id)
{
	const std::vector<ShopSlot> stock = GetShopStock(id);
	if (stock.empty())
		return true;
	const std::vector<bool> onShelf = PlacedFlags(stock);
	return std::find(onShelf.begin(), onShelf.end(), false) == onShelf.end();
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
	const bool redesigned = IsRedesignedShopScreen(stextflag);
	// His grid tabs take the framed cut of the forge when it is there; the frameless one is the
	// fallback, and remains what his Salvage page draws, that page having no grid to frame.
	const bool griswoldFramed = redesigned && HasShopArt(GriswoldGridCanvasAsset);
	// Asked ONCE, and the answer is the painting itself: which file to blit, and - as a bool - the
	// four redactions that come with having one.
	const char *const portrait = PaintedPageCanvas(stextflag);
	const bool portraitFramed = portrait != nullptr;
	if (redesigned) {
		// Griswold's own forge, on every one of his tabs (user, 2026-09-21: "We replace the canvas for
		// all his tabs"). No title band with it: the painting is a portrait of the man, so naming him
		// above it says nothing the picture does not ("We remove the title Griswold from all tabs").
		DrawLoosePng(out, griswoldFramed ? GriswoldGridCanvasAsset : GriswoldCanvasAsset, panel.position);
	} else if (portraitFramed) {
		// This vendor's own place - Pepin's doorway, Wirt's alley (user, 2026-09-21). Drawn with a
		// bare DrawLoosePng rather than through DrawSidePanelGridArt, and that IS the "remove the
		// tint" redaction: the shared helper lays the canvas dim over whatever it draws, and this
		// path never calls it. Same as Griswold's above, for the same reason - a half-transparent
		// grey over a painting is the thing the user asked to be rid of on the stash.
		DrawLoosePng(out, portrait, panel.position);
	} else if (HasSidePanelGridArt()) {
		// The canvas with the grid's frame painted into it (user, 2026-09-21: "apply it to all windows
		// which use the 10x16 grid. It has new grid frame embedded in it"). Adria's, now: Griswold
		// keeps his forge, and Pepin and Wirt took portraits of their own in the branch above.
		DrawSidePanelGridArt(out, panel.position);
	} else if (HasSidePanelArt()) {
		DrawSidePanelArt(out, panel.position);
	} else {
		DrawThemedFill(out, panel);
		DrawOrnateBorder(out, panel);
	}

	// No name over a portrait. Griswold's tabs lost theirs on 2026-09-21 ("We remove the title
	// Griswold from all tabs") and the stash lost its own the same day; these canvases are the same
	// kind of picture, with the vendor standing or sitting in them, so printing the name above one
	// says nothing the painting does not.
	if (!redesigned && !portraitFramed) {
		const Rectangle labelArea { { panel.position.x + 16, panel.position.y + PanelTitleTop },
			{ panel.size.width - 32, PanelTitleHeight } };
		DrawOutlinedString(out, _(ShopTitle(stextflag)), labelArea,
		    UiFlags::ColorWhitegold | UiFlags::FontSize30 | UiFlags::AlignCenter | UiFlags::VerticalCenter);
	}

	const Rectangle grid = GetShopGridRect();
	// ONE pass of fill wherever the frame is PAINTED, two where this code draws the bezel itself
	// (user, 2026-09-21: "make the grid a bit transparent", asked of the stash and applied here with
	// its canvas). Each pass is the same half-transparent blend, so two leave about a quarter of what
	// is under them and one leaves about half - over a painting that is the difference between a grid
	// drawn ON the room and a dark plate laid over it.
	//
	// Applied to every painted canvas here, not only the portraits: these pages are tabs of one shop
	// in the user's own words (2026-09-21, "all tabs for all vendors are to be considered Stores"), and a
	// grid that changes density as you move between vendors is the kind of difference that reads as a
	// bug. Where there is no painting the fill is not covering art, it IS the grid's face, so the
	// procedural path keeps its two.
	const bool paintedFrame = griswoldFramed || portraitFramed || (!redesigned && HasSidePanelGridArt());
	DrawThemedFill(out, grid, paintedFrame ? 1 : 2);
	// The frame is PAINTED INTO the grid canvas (2026-09-21), so drawing one here would put a second
	// bezel inside the first. The fill above and the cell rules below still come from code - the art
	// brings the frame and nothing else.
	if ((!HasSidePanelGridArt() || redesigned) && !griswoldFramed && !portraitFramed) {
		if (HasGridBezel(grid.size)) {
			DrawGridBezel(out, grid);
		} else {
			DrawOrnateBorderOutside(out, grid);
		}
	}
	// THE SLOT FACE, one per cell (user, 2026-09-22: "also apply it to all vendors grids"). Before
	// the rules below, which are what has always separated these cells - the art carries its own
	// bevel and the two agree.
	for (int row = 0; row < ShopGridRows; row++) {
		for (int col = 0; col < ShopGridColumns; col++) {
			DrawSlotBackground(out,
			    { { grid.position.x + col * ShopCellPx, grid.position.y + row * ShopCellPx }, { ShopCellPx, ShopCellPx } });
		}
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
	// Snapped to something ON THE SHELF, not merely in range. A stock index that no longer has a
	// PlacedSlot - the shelf repacked after a purchase - would otherwise leave the outline drawn
	// nowhere while the selection still pointed at a real, invisible item.
	if (!placed.empty()
	    && std::none_of(placed.begin(), placed.end(),
	        [](const PlacedSlot &slot) { return slot.stockIndex == ShopGridSel; })) {
		ShopGridSel = placed[0].stockIndex;
	}

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
		if (const uint8_t *tint = oracool::ItemTRN(item); tint != nullptr)
			ClxDrawTRN(out, position, sprite, tint);
		else
			ClxDraw(out, position, sprite);
	}

	if (redesigned)
		DrawRedesignedControls(out, pageCount);
	else
		DrawShopControls(out, pageCount);
	DrawShopTabColumn(out, stextflag);
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
	if (CheckShopTabColumnClick(position, rightClick))
		return true;
	if (ShopCloseRect().contains(position)) {
		// Out of the shop entirely, not back to the vendor's dialog - the X on every other Oracool
		// window closes the window, and the tabs are how you move between shop screens.
		stextflag = TalkID::None;
		PlayUiMoveSound(); // its own hit test, so it does not get CheckWindowCloseButtonClick's click
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
				PlayUiMoveSound();
				return true;
			}
		}
	}

	// The painted frames at measured positions, where a page has any - Griswold's six, or the single
	// Refresh a portrait page carries. Where they ARE drawn the old variable rows are not, so those
	// rects are bare painting and must not be hit-tested: a drop on one would repair an item with
	// nothing on screen to explain it.
	const bool redesigned = IsRedesignedShopScreen(stextflag);
	if (PageHasServiceFrames(stextflag)) {
		for (int slot = 0; slot < ShopServiceSlotCount; slot++) {
			if (!ShopServiceSlotOnPage(slot, stextflag) || !ShopServiceSlotRect(slot).contains(position))
				continue;
			if (!MyPlayer->HoldItem.isEmpty()) {
				// A held item is a DROP, not a click, and three of the six take one: the Sell plate too (round 69 audit: the hint
				// says "drop an item anywhere on this panel", and the plate was the one spot it did nothing).
				if (ShopServiceSlotDoes[slot] == ServiceButton::Repair)
					ShopRepairHeldItem();
				else if (ShopServiceSlotDoes[slot] == ServiceButton::Recharge)
					ShopRechargeHeldItem();
				else if (ShopServiceSlotDoes[slot] == ServiceButton::Sell)
					ShopSellHeldItem();
				return true;
			}
			// A greyed button absorbs the click and does nothing - it is still a button, so the click
			// must not fall through it to the painting behind.
			if (!ShopServiceSlotEnabled(slot, stextflag))
				return true;
			// A right press has no release in this engine: it sank the frame, and the next LEFT release inside it fired
			// Sell All or Refresh (round 23 audit). Absorbed, as the tab column absorbs it.
			if (rightClick)
				return true;
			PressedShopServiceSlot = slot; // sinks until LeftMouseUp; the action waits for the release
			PlayUiMoveSound();            // titlemov at the PRESS, as every other button in the mod
			return true;
		}
	}

	// A held item is a DROP, not a click, and the target decides what happens to it. Ahead of every
	// other control on the panel: dropping a sword on the Repair button must repair it rather than
	// fall through to whatever that rect does when the hand is empty.
	const std::vector<ControlButton> buttons = redesigned ? std::vector<ControlButton>() : ShopControlButtons(stextflag);
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
			else if (buttons[i].service == ServiceButton::Refresh)
				RefreshBoyStock(stextflag); // Wirt's fresh stock, free (2026-09-20)
			// Picking up the hammer is silent in itself; Repair all sounds when the work is done.
			if (buttons[i].service != ServiceButton::RepairAll)
				PlayUiSelectSound();
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
			//
			// With Ctrl held, a restocking potion is bought as a whole stack (2026-09-13). Anything the
			// stack buy does not apply to falls through to the single purchase, so Ctrl never makes a
			// right click do nothing.
			const int stockIndex = stock[placed[hovered].stockIndex].index;
			if ((SDL_GetModState() & KMOD_CTRL) == 0 || ShopBuyPotionStack(stextflag, stockIndex) < 0)
				ShopSelectIndex(stextflag, stockIndex);
		} else {
			// Looking, not buying. The selection moves so the footer describes this item, and
			// nothing is spent - which is what makes a right click safe to be unconfirmed.
			//
			// stockIndex, not stock[...].index: ShopGridSel indexes the stock vector, which is what
			// ActivateShopGridSelection reads it back as.
			ShopGridSel = placed[hovered].stockIndex;
			PlayUiMoveSound(); // the grid's StoreUp/StoreDown - vanilla's shops sound every row moved to
		}
	}
	// Anywhere else on the panel is absorbed: the shop covers the world, and a click on its
	// background must not walk the player into a wall behind it.
	return true;
}

void MoveShopGridSelection(int columns, int rows)
{
	const std::vector<ShopSlot> stock = GetShopStock(stextflag);
	const std::vector<PlacedSlot> placed = PlaceStock(stock);
	if (placed.empty())
		return;

	// Through the PLACED entries, not the whole stock vector (external audit of v1.9.88, finding 3).
	//
	// It used to step modulo `stock.size()`, and a shelf generates more than one page can hold - that
	// is deliberate, and PlaceStock's own note explains why: what the shop CARRIES is defined as what
	// fits, so the overflow was never on the shelf. But the keyboard did not know that. Arrowing far
	// enough moved the selection onto an entry with no PlacedSlot, the outline vanished because
	// nothing was drawn for it, and Enter bought an item the player could not see. Mouse hit-testing
	// went through the placed list all along, which is why this only ever showed on keyboard and
	// controller.
	//
	// Stock order is preserved, because PlaceStock appends in stock order - so "next" still means the
	// next item along the shelf, which is what the row-major packing makes it look like.
	const int step = columns + rows * ShopGridColumns;
	const int count = static_cast<int>(placed.size());

	int current = 0;
	for (int i = 0; i < count; i++) {
		if (placed[i].stockIndex == ShopGridSel) {
			current = i;
			break;
		}
	}
	// A selection that is no longer placed - the shelf repacked under it after a purchase - lands on
	// the first entry rather than nowhere.
	const int next = ((current + step) % count + count) % count;
	ShopGridSel = placed[next].stockIndex;
	ShopGridPage = placed[next].page;
}

bool IsShopItemHovered()
{
	return ShopHoverActive;
}

/** The item under the cursor on the last hover pass, for the comparison panel. Points into the
 * vendor's or the player's own item storage, which outlives the frame; cleared with the flag. */
const Item *ShopHoverItem = nullptr;

const Item *HoveredShopItem()
{
	return ShopHoverActive ? ShopHoverItem : nullptr;
}

bool SetShopHoverInfoString()
{
	ShopHoverActive = false;
	ShopHoverItem = nullptr;
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

	// The painted frames answer first and instead - where they are drawn the old rows are not, so
	// those rects are bare painting and must name nothing.
	if (PageHasServiceFrames(stextflag)) {
		for (int slot = 0; slot < ShopServiceSlotCount; slot++) {
			if (!ShopServiceSlotOnPage(slot, stextflag) || !ShopServiceSlotRect(slot).contains(MousePosition))
				continue;
			SetServiceHint(ShopServiceSlotDoes[slot]);
			if (!ShopServiceSlotEnabled(slot, stextflag))
				AddPanelString(_("Not on this shelf."), UiFlags::ColorRed);
			ShowPanelStringsAsHintCard(); // the unique-item card, 250px at most (2026-09-27)
			return true;
		}
	} else {
		const std::vector<ControlButton> buttons = ShopControlButtons(stextflag);
		for (size_t i = 0; i < buttons.size(); i++) {
			if (buttons[i].kind != ControlKind::Service)
				continue;
			if (!ShopControlRect(buttons, i).contains(MousePosition))
				continue;
			// The button says WHICH service; the hint is where what it does - and what Repair All
			// costs - is written down.
			SetServiceHint(buttons[i].service);
			ShowPanelStringsAsHintCard();
			return true;
		}
	}

	if (hovered < 0) {
		ClearPanelStrings();
		InfoColor = UiFlags::ColorWhite;
		return true;
	}
	ShopHoverActive = true;

	const ShopSlot &slot = stock[placed[hovered].stockIndex];
	ShopHoverItem = slot.item;
	// The same two calls the inventory's own hover makes, in the same order: the name sets the
	// string and its colour, the details append to it. Then the price, which is the one line a shop
	// adds - and it says what the number is FOR, because on Repair and Recharge the player is not
	// buying the item.
	ClearPanelStrings();
	GetItemStr(*slot.item);
	// The unidentified form for unidentified stock, as the backpack and the stash already print it (tooltip
	// audit, 2026-09-25): Wirt's Gamble bases showed the identified layout - quality, tier, item level - over a
	// base name, promising a roll that has not happened yet.
	if (slot.item->_iIdentified)
		PrintItemDetails(*slot.item);
	else
		PrintItemDur(*slot.item);
	AddPanelString(StrCat(_(ShopPriceLabel(stextflag)), ": ", FormatInteger(slot.price)),
	    UiFlags::ColorWhitegold);
	return true;
}

void ActivateShopGridSelection()
{
	const std::vector<ShopSlot> stock = GetShopStock(stextflag);
	if (ShopGridSel < 0 || ShopGridSel >= static_cast<int>(stock.size()))
		return;
	// The selection must be ON THE SHELF, not merely in the stock vector (external audit of v1.9.88,
	// finding 3). A range check against stock.size() is not the same question: the stock deliberately
	// holds more than one page can show, and buying an entry that was never placed is buying
	// something invisible. Belt and braces alongside the navigation fix above - that stops the
	// selection getting there, and this refuses to act if it somehow does.
	const std::vector<PlacedSlot> placed = PlaceStock(stock);
	const bool onShelf = std::any_of(placed.begin(), placed.end(),
	    [](const PlacedSlot &slot) { return slot.stockIndex == ShopGridSel; });
	if (!onShelf)
		return;
	ShopSelectIndex(stextflag, stock[ShopGridSel].index);
}

} // namespace devilution::oracool
