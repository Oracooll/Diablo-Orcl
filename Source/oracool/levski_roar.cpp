#include "oracool/levski_roar.h"

#include <SDL.h>

#include <fmt/format.h>

#include <algorithm>
#include <string>

#include "DiabloUI/ui_flags.hpp"
#include "control.h"
#include "cursor.h"
#include "engine/render/clx_render.hpp"
#include "engine/render/primitive_render.hpp"
#include "engine/render/text_render.hpp"
#include "inv.h"
#include "items.h"
#include "objects.h"

#include "oracool/crafting.h"
#include "oracool/event_log.h"
#include "oracool/ornate_border.h"
#include "oracool/salvage.h"
#include "oracool/window_close.h"
#include "player.h"
#include "utils/language.h"
#include "utils/str_cat.hpp"

namespace devilution::oracool {

namespace {

bool WindowOpen = false;
bool RecipeBookOpen = false;

/**
 * The pressed-button flash. User request, 2026-08-20: "Make some visual feedback when i click on
 * levskis buttons."
 *
 * Held as a button index plus an expiry tick rather than a bool, for the same reason the inventory
 * SORT button is: these buttons act on mouse-DOWN and nothing here polls a mouse-up, so a flag
 * would either linger until the next click or need a second owner to clear it. The index is the
 * salvage tier 0-6, then Transmute and Recipes - one mechanism, every button on the panel.
 */
constexpr int ButtonFlashNone = -1;
constexpr int ButtonFlashTransmute = SalvageTierCount;
constexpr int ButtonFlashRecipes = SalvageTierCount + 1;
int ButtonFlashIndex = ButtonFlashNone;
uint32_t ButtonFlashUntil = 0;
/** Long enough to see, short enough not to read as a mode change - the SORT button's number. */
constexpr uint32_t ButtonFlashMs = 170;
/** A pale gold fill: the same ramp the border is cut from, near its light end. */
constexpr uint8_t ButtonFlashColor = PAL16_YELLOW + 4;

bool ButtonFlashActive(int index)
{
	return ButtonFlashIndex == index && SDL_GetTicks() < ButtonFlashUntil;
}

void FlashButton(int index)
{
	ButtonFlashIndex = index;
	ButtonFlashUntil = SDL_GetTicks() + ButtonFlashMs;
}

/** The transmute slots, indexed by an item's top-left cell. Live only while the window is open - see the header's note on why
 * this is deliberately not save state. */
Item GridItems[LevskiGridSlots];

/**
 * Which cell each item anchors to, and which cells it covers.
 *
 * GridItems is indexed by the item's TOP-LEFT cell, and GridCells[c] holds that anchor's index + 1
 * for every cell the item covers (0 = free) - the same shape as Player::InvGrid, deliberately, so
 * the rules are the ones the player already knows from the backpack and the stash.
 *
 * The first version had neither array: twelve 56x56 boxes, one item each, footprint ignored (user,
 * 2026-08-19: "Why do they fit a whole armor in one? All this makes 0 sense."). It was wrong twice
 * over - a 56px box is two inventory cells square while a 2x3 armour is 56x84, so the armour never
 * fitted the box it was drawn in; and treating a rune and a breastplate as the same "one box" made
 * the cube's capacity mean nothing. A 3x4 cube holds ONE armour, or twelve runes.
 */
int8_t GridCells[LevskiGridSlots];

// Geometry. The window is sized from the grid rather than the other way round, so changing the
// grid's dimensions cannot leave the panel the wrong shape.
//
// The cell is the INVENTORY's cell, exactly - not a size of this window's choosing. Item sprites
// are cut to a whole number of 28px cells, so any other size would either crop them or leave them
// swimming, and the drag the player already knows from the stash would stop lining up.
constexpr int CellSize = InventorySlotSizeInPixels.width;
constexpr int SlotGap = 6;
constexpr int Padding = 14;
constexpr int HeaderHeight = 30;
constexpr int ButtonHeight = 26;
constexpr int GridWidth = LevskiGridColumns * CellSize;
constexpr int GridHeight = LevskiGridRows * CellSize;
/** A 3-wide grid is only 84px across - narrower than the word "Transmute". The window is the wider
 * of the grid and what its own buttons need to read, with the grid centred in it. */
constexpr int ContentWidth = GridWidth > 150 ? GridWidth : 150;

/**
 * @brief The salvage column: seven "salvage all X" buttons, stacked down the right of the grid.
 *
 * User request, 2026-08-20: "add salvage all whites, magic, rare, uniques, primal, set, ethereal
 * buttons on levski ui. increase it's ui window and add these in placeholder gold boxes."
 *
 * A COLUMN beside the grid rather than a row beneath it, and that is the window's shape deciding:
 * seven buttons wide enough to read would be over 700px in a row, half the screen. Stacked, they
 * cost 140px of width and reuse height the 3x4 grid already occupies.
 *
 * PLACEHOLDER, as asked - a gold-bordered box with the tier's name in it, no art. When real button
 * art arrives only DrawSalvageButtons changes; the rects and the routing stay.
 */
constexpr int SalvageColumnWidth = 140;
constexpr int SalvageButtonHeight = 24;
constexpr int SalvageButtonGap = 4;
constexpr int SalvageColumnGap = 10;

/** The window grew by exactly the column plus its gap - the grid and the buttons under it are
 * untouched, so nothing that was already placed had to move. */
constexpr int WindowWidth = ContentWidth + SalvageColumnGap + SalvageColumnWidth + Padding * 2;
/** Tall enough for whichever side is taller: the grid and its two buttons, or the seven. */
constexpr int GridSideHeight = HeaderHeight + GridHeight + SlotGap + ButtonHeight * 2 + SlotGap;
constexpr int SalvageSideHeight = HeaderHeight + SalvageTierCount * SalvageButtonHeight
    + (SalvageTierCount - 1) * SalvageButtonGap;
constexpr int WindowHeight = Padding * 2
    + (GridSideHeight > SalvageSideHeight ? GridSideHeight : SalvageSideHeight);

/**  How wide the book may be: all the room left of the window, capped, never overlapping it.
 *
 * A constant 420 was wrong twice - first drawn off the left edge, then clamped to x=0 where it sat
 * ON TOP of Levski's own window and covered its title (user screenshot, 2026-08-19). The room to the
 * left of a centred window is what it is; the book has to fit that, not assume it. */
int RecipeBookWidthFor(const Rectangle &window)
{
	return std::clamp(window.position.x - SlotGap * 2, 220, 420);
}
/**
 * The panel's ground, opaque.
 *
 * Two half-transparent passes came first and were not enough (user screenshot, 2026-08-19): the
 * town read straight through the grid, and worse, it read through UNEVENLY - the four cells over
 * the lit doorway glowed while the rest sat black, so the grid looked like four different
 * materials. Half-transparency composites against whatever is behind it, and what is behind this
 * window is a moving, unevenly lit town.
 *
 * So: a solid fill. Every other window in the game sits on painted art and hides what is under it
 * completely; this one has no art yet, and "no art" should still mean "not a window you can see
 * through".
 *
 * The indices are DrawOrnateBorder's own, with its measured RGB in the comments - not a `PAL16_x +
 * n` expression. PAL16_GRAY + 12/15 was the first attempt and came out near-white (user screenshot,
 * 2026-08-19): the top of the palette is the UI's white end, not the dark end of a grey ramp, so
 * the arithmetic that reads sensibly - "a high offset is a dark shade", per the palette header's
 * own dark-blue example - is simply false for that one ramp. Naming proven indices removes the
 * guess. Both live in the shared upper half (128-255), identical across town and all four
 * tilesets, so the fill cannot recolour itself by level.
 */
constexpr uint8_t PanelFillColor = 204; // (57, 49, 29) - the border's own shadow tone, dark stone
/** The grid cells, near-black against the panel's dark stone - so a slot reads as a recessed well
 * waiting for a stone rather than as a square someone drew on the stone (user screenshot,
 * 2026-08-19: the cells were the same colour as the panel and read as decoration). */
constexpr uint8_t SlotFillColor = 223; // (15, 5, 0)

void DrawPanelGround(const Surface &out, const Rectangle &rect, uint8_t fill = PanelFillColor)
{
	FillRect(out, rect.position.x, rect.position.y, rect.size.width, rect.size.height, fill);
	DrawOrnateBorder(out, rect);
}

/** @brief The recipe book's lines, pre-wrapped to its own text width - the formulas are long
 * enough that "1 socketed item -> the item, emptied, and its stones back" ran off the panel and
 * the last line was sliced by the bottom edge. */
/**
 * @brief The recipe the Transmute button will run, or -1 for "whatever is ready".
 *
 * WHY THIS EXISTS. Until v1.9.18 the monument auto-picked - first the lowest-numbered ready recipe,
 * then the one consuming the most grid slots. Both worked while the recipes had disjoint inputs.
 * Neither survives the tier ladder: Ennoble Rares and Reroll Rares take the SAME target and the
 * SAME material at different counts, and a reagent stack of five sits in ONE slot, so nearly every
 * item recipe ties at two slots and the tie-break decides for the player.
 *
 * So the player decides. Clicking a recipe in the book selects it; clicking it again clears the
 * selection back to automatic. Not persisted - the grid is not either, and a crafting station that
 * remembers a mode across sessions is a mode you can forget you set.
 */
int SelectedRecipe = -1;

/** @brief How far the recipe book is scrolled, in pixels. Clamped on every draw. */
int RecipeBookScroll = 0;

std::string RecipeBookText(int width)
{
	std::string page;
	for (int i = 0; i < CraftingRecipeCount; i++) {
		if (i > 0)
			page += '\n';
		page += _(CraftingRecipeName(i));
		page += '\n';
		page += WordWrapString(_(CraftingRecipeInputs(i)), width - Padding * 2, GameFont12);
		page += '\n';
	}
	return page;
}

/**
 * @brief Where each recipe's block sits in @p page, scroll already applied.
 *
 * ONE geometry, read by the draw and by the click. The rows are not a fixed height - each formula
 * wraps to as many lines as it needs - so a click handler that divided by a row height would drift
 * further out of step with every recipe added, and would drift silently.
 */
struct RecipeRow {
	int top;
	int height;
};

std::vector<RecipeRow> RecipeBookRows(const Rectangle &page)
{
	std::vector<RecipeRow> rows;
	rows.reserve(CraftingRecipeCount);
	const int textWidth = page.size.width - Padding * 2;
	int y = page.position.y + Padding + HeaderHeight - RecipeBookScroll;
	for (int i = 0; i < CraftingRecipeCount; i++) {
		const int lineHeight = GetLineHeight(_(CraftingRecipeName(i)), GameFont12);
		const std::string formula = WordWrapString(_(CraftingRecipeInputs(i)), textWidth, GameFont12);
		const int formulaLines = static_cast<int>(std::count(formula.begin(), formula.end(), '\n')) + 1;
		const int height = lineHeight + lineHeight * formulaLines + 6;
		rows.push_back({ y, height });
		y += height;
	}
	return rows;
}

/** @brief How far the book can scroll before the last recipe's foot reaches the panel's. */
int RecipeBookMaxScroll(const Rectangle &page)
{
	if (page.size.height <= 0)
		return 0;
	// Measured from the UNSCROLLED layout, so the answer does not depend on where the book already
	// is - a max that moved with the offset is how a scroll runs away from its own bound.
	const int saved = RecipeBookScroll;
	RecipeBookScroll = 0;
	const std::vector<RecipeRow> rows = RecipeBookRows(page);
	RecipeBookScroll = saved;
	if (rows.empty())
		return 0;
	const int contentBottom = rows.back().top + rows.back().height;
	const int visibleBottom = page.position.y + page.size.height - Padding;
	return std::max(0, contentBottom - visibleBottom);
}

Point GridOrigin(const Rectangle &window)
{
	// Centred: the grid is narrower than the window's own buttons.
	return window.position + Displacement { Padding + (ContentWidth - GridWidth) / 2, Padding + HeaderHeight };
}

Rectangle CellRect(const Rectangle &window, int cell)
{
	const Point origin = GridOrigin(window);
	return Rectangle { { origin.x + (cell % LevskiGridColumns) * CellSize, origin.y + (cell / LevskiGridColumns) * CellSize },
		{ CellSize, CellSize } };
}

/** @brief The cell under @p position, or -1. */
int CellAt(const Rectangle &window, Point position)
{
	for (int cell = 0; cell < LevskiGridSlots; cell++) {
		if (CellRect(window, cell).contains(position))
			return cell;
	}
	return -1;
}

/** @brief Whether an item of @p size can sit with its top-left at @p anchor. */
bool FitsAt(int anchor, Size size)
{
	const int column = anchor % LevskiGridColumns;
	const int row = anchor / LevskiGridColumns;
	if (column + size.width > LevskiGridColumns || row + size.height > LevskiGridRows)
		return false;
	for (int y = 0; y < size.height; y++) {
		for (int x = 0; x < size.width; x++) {
			if (GridCells[(row + y) * LevskiGridColumns + column + x] != 0)
				return false;
		}
	}
	return true;
}

void MarkCells(int anchor, Size size, int8_t value)
{
	const int column = anchor % LevskiGridColumns;
	const int row = anchor / LevskiGridColumns;
	for (int y = 0; y < size.height; y++) {
		for (int x = 0; x < size.width; x++)
			GridCells[(row + y) * LevskiGridColumns + column + x] = value;
	}
}

/**
 * @brief Puts @p item in the grid, preferring @p preferredAnchor. True if it found room.
 *
 * @p preferredAnchor of -1, or one the item does not fit at, falls back to the first cell it does
 * fit at - so a click that lands slightly off still does what the player meant, rather than nothing.
 */
bool PlaceInGrid(const Item &item, int preferredAnchor)
{
	const Size size = GetInventorySize(item);
	int anchor = (preferredAnchor >= 0 && FitsAt(preferredAnchor, size)) ? preferredAnchor : -1;
	for (int candidate = 0; anchor < 0 && candidate < LevskiGridSlots; candidate++) {
		if (FitsAt(candidate, size))
			anchor = candidate;
	}
	if (anchor < 0)
		return false;
	GridItems[anchor] = item;
	MarkCells(anchor, size, static_cast<int8_t>(anchor + 1));
	return true;
}

/**
 * @brief Collects the non-empty items of @p source into @p out, largest footprint first.
 *
 * Largest first is the placement order, not merely a tidy one: a 2x3 placed after four runes may
 * find no run of free cells left, while the runes always fit around it. Both the simulation and the
 * real rebuild sort this way, from this one function, so the answer and the act cannot disagree.
 */
int CollectLargestFirst(const Item *source, int sourceCount, Item *out)
{
	int count = 0;
	for (int i = 0; i < sourceCount; i++) {
		if (!source[i].isEmpty())
			out[count++] = source[i];
	}
	std::sort(out, out + count, [](const Item &a, const Item &b) {
		const Size sa = GetInventorySize(a);
		const Size sb = GetInventorySize(b);
		return sa.width * sa.height > sb.width * sb.height;
	});
	return count;
}

/**
 * @brief Rebuilds the occupancy map from GridItems. False if something could not be placed.
 *
 * The recipes rewrite GridItems in place - three gems become one, a socketed item becomes an item
 * plus its stones - without any idea of footprints, and the result's sizes are not the inputs'. So
 * after a transmute the map is re-derived rather than patched: collect what is there, clear, and
 * re-place. Anchors may move, which is correct; the alternative is a stone drawn over a helmet.
 *
 * The return value exists because PlaceInGrid CAN fail - twelve array slots is not twelve free
 * cells - and for a long time its failure was discarded, which turned "no room" into an item that
 * quietly stopped existing. Nothing here can put the item anywhere else, so the honest thing is to
 * report the failure and let the caller undo the whole transmute (see the Transmute button).
 */
bool RebuildGridOccupancy()
{
	Item items[LevskiGridSlots];
	const int count = CollectLargestFirst(GridItems, LevskiGridSlots, items);
	for (Item &slot : GridItems)
		slot.clear();
	for (int8_t &cell : GridCells)
		cell = 0;
	bool allPlaced = true;
	for (int i = 0; i < count; i++) {
		if (!PlaceInGrid(items[i], -1))
			allPlaced = false;
	}
	return allPlaced;
}

Rectangle TransmuteButtonRect(const Rectangle &window)
{
	const int y = window.position.y + Padding + HeaderHeight + GridHeight + SlotGap;
	return Rectangle { { window.position.x + Padding, y }, { ContentWidth, ButtonHeight } };
}

Rectangle RecipeButtonRect(const Rectangle &window)
{
	const Rectangle transmute = TransmuteButtonRect(window);
	return Rectangle { { transmute.position.x, transmute.position.y + ButtonHeight + SlotGap },
		{ ContentWidth, ButtonHeight } };
}

/** @brief Salvage button @p index, counting down the column from the top. */
Rectangle SalvageButtonRect(const Rectangle &window, int index)
{
	const int x = window.position.x + Padding + ContentWidth + SalvageColumnGap;
	const int y = window.position.y + Padding + HeaderHeight
	    + index * (SalvageButtonHeight + SalvageButtonGap);
	return Rectangle { { x, y }, { SalvageColumnWidth, SalvageButtonHeight } };
}

/**
 * @brief Hands everything in the grid back to the player. True when the grid is empty afterwards.
 *
 * A full backpack is the one case that has to be handled rather than assumed away, and the answer
 * is to REFUSE THE CLOSE rather than to drop on the floor: there is no exported "drop this item
 * here" call, and inventing one to solve a UI problem is how a stone ends up on a floor the player
 * has already left. Keeping the window open loses nothing and says why.
 */
bool ReturnGridToPlayer()
{
	Player &player = *MyPlayer;
	bool allReturned = true;
	for (Item &item : GridItems) {
		if (item.isEmpty())
			continue;
		if (AutoPlaceItemInInventory(player, item, true))
			item.clear();
		else
			allReturned = false;
	}
	// Rebuild rather than patch: a partial return leaves some items behind, and their occupancy has
	// to match what is actually still in the grid.
	RebuildGridOccupancy();
	return allReturned;
}

} // namespace


bool LevskiGridCanHold(const Item *items, int count)
{
	// More items than array slots cannot be held whatever their sizes, and the scratch arrays below
	// are exactly LevskiGridSlots long.
	if (count > LevskiGridSlots)
		return false;

	// The real grid arrays are borrowed as the scratch space and put back afterwards. Ugly, but it
	// is what makes this the SAME packing that will actually run: a separate simulation with its
	// own occupancy map is a second implementation, and a second implementation of "does it fit"
	// is exactly how a check comes to disagree with the thing it is checking.
	Item savedItems[LevskiGridSlots];
	int8_t savedCells[LevskiGridSlots];
	std::copy(std::begin(GridItems), std::end(GridItems), savedItems);
	std::copy(std::begin(GridCells), std::end(GridCells), savedCells);

	Item ordered[LevskiGridSlots];
	const int orderedCount = CollectLargestFirst(items, count, ordered);

	for (Item &slot : GridItems)
		slot.clear();
	for (int8_t &cell : GridCells)
		cell = 0;
	bool allFit = true;
	for (int i = 0; i < orderedCount; i++) {
		if (!PlaceInGrid(ordered[i], -1))
			allFit = false;
	}

	std::copy(std::begin(savedItems), std::end(savedItems), GridItems);
	std::copy(std::begin(savedCells), std::end(savedCells), GridCells);
	return allFit;
}

bool HandleLevskiRecipeBookScroll(int notches)
{
	if (!WindowOpen || !RecipeBookOpen)
		return false;
	const Rectangle book = GetLevskiRecipeBookRect();
	if (book.size.height <= 0)
		return false;
	// A wheel notch moves about one recipe's worth. Bounded at BOTH ends, for the reason recorded
	// on the skill picker's own scroll: without the upper bound the wheel pushes the list past its
	// last row and the panel goes blank, which reads as a crash rather than as the end of a list.
	constexpr int PixelsPerNotch = 40;
	RecipeBookScroll = std::clamp(RecipeBookScroll - notches * PixelsPerNotch, 0, RecipeBookMaxScroll(book));
	return true;
}

bool IsLevskiRoarOpen() { return WindowOpen; }
bool IsLevskiRecipeBookOpen() { return WindowOpen && RecipeBookOpen; }

bool IsLevskiRoarObject(const Object &object)
{
	return currlevel == 0 && !setlevel && object._otype == OBJ_STAND;
}

void ToggleLevskiRoar()
{
	if (WindowOpen) {
		CloseLevskiRoar();
		return;
	}
	WindowOpen = true;
	RecipeBookOpen = false;
}

void CloseLevskiRoar()
{
	if (!WindowOpen)
		return;
	if (!ReturnGridToPlayer()) {
		LogEvent(std::string(_("Your pack is full - Levski's Roar keeps what it holds.")), UiFlags::ColorRed);
		return;
	}
	for (int8_t &cell : GridCells)
		cell = 0;
	WindowOpen = false;
	RecipeBookOpen = false;
}

Rectangle GetLevskiRoarRect()
{
	if (!WindowOpen)
		return Rectangle { { 0, 0 }, { 0, 0 } };
	// Centred on the play area, like the other operable-object windows.
	const int x = (gnScreenWidth - WindowWidth) / 2;
	// CENTRED vertically (user, 2026-08-27: "Levski's Roar should be middle of screen"). It sat a
	// third of the way down before, and was briefly bottom-docked by a rule that was never meant for
	// it - the docking rule is about the side panels.
	const int y = std::max(0, (static_cast<int>(gnScreenHeight) - WindowHeight) / 2);
	return Rectangle { { x, y }, { WindowWidth, WindowHeight } };
}

Rectangle GetLevskiRecipeBookRect()
{
	if (!IsLevskiRecipeBookOpen())
		return Rectangle { { 0, 0 }, { 0, 0 } };
	const Rectangle window = GetLevskiRoarRect();
	// Height from the WRAPPED text, not from a per-recipe row guess: the formulas wrap to two lines
	// each and the fixed 40px row left the last one sliced by the panel's bottom edge.
	const int bookWidth = RecipeBookWidthFor(window);
	const std::string page = RecipeBookText(bookWidth);
	const int textHeight = static_cast<int>(GetLineHeight(page, GameFont12) * (std::count(page.begin(), page.end(), '\n') + 1));
	// CAPPED to the screen, and scrolled inside the cap (v1.9.18). The height used to be whatever
	// the wrapped text came to, which was fine for five recipes and stopped being fine at eighteen:
	// the panel simply grew past the bottom of a 720-tall screen and the last recipes could not be
	// read at all, let alone clicked.
	// The band the book is allowed to occupy: the top of the screen down to a 100px reserve above
	// the bottom (user, 2026-08-27: "Recipe book should be next to it, in the middle between top of
	// screen and 100px row above the bottom"), and 620 tall at most (user, 2026-08-27: "recipe
	// window of levski to be 620px high and scrollable").
	//
	// On the 720-tall screens this project targets those two numbers are the same number - 720 minus
	// the reserve IS 620 - so the book fills the band exactly and starts at y=0. They are written as
	// two rules anyway because they are two rules: the reserve is about the HUD, the 620 is a size,
	// and a screen that is not 720 tall must honour both rather than whichever happened to be
	// hardcoded.
	constexpr int BottomReserve = 100;
	constexpr int MaxBookHeight = 620;
	const int band = std::max(0, static_cast<int>(gnScreenHeight) - BottomReserve);
	const int height = std::min({ Padding * 2 + HeaderHeight + textHeight, MaxBookHeight, band });
	// LEFT of the window by preference: opening right ran the book under the mini-map, which owns
	// the top-right corner.
	//
	// But NOT unconditionally, and the previous comment here - "there is always room on the left,
	// the window is centred" - was simply false, which a screenshot caught (user, 2026-08-19: the
	// book's title read "IPES" and every line lost its first characters off the left edge). The
	// window is centred in gnScreenWidth, so the room to its left is (gnScreenWidth - WindowWidth)/2,
	// and at 1024 wide that is 408 against a book needing 426. Centring guarantees symmetry, not
	// space.
	//
	// So: place it left and CLAMP at the screen edge. The first fix flipped it to the right of the
	// window when the left did not fit, and that was worse (user screenshot, 2026-08-19): the right
	// is where the inventory and the mini-map live, so the book landed on top of the panel the
	// player had open. Sliding left until it touches x=0 costs at most a few pixels of overlap with
	// Levski's own window - and the book is drawn after it, so the book stays readable.
	const int x = std::max(0, window.position.x - bookWidth - SlotGap);
	// Centred in that same band, so the book sits in the space it can actually use rather than in
	// the whole screen.
	const int y = std::max(0, (band - height) / 2);
	return Rectangle { { x, y }, { bookWidth, height } };
}

void DrawLevskiRoar(const Surface &out)
{
	if (!WindowOpen)
		return;

	const Rectangle window = GetLevskiRoarRect();
	DrawPanelGround(out, window);
	DrawWindowCloseButton(out, window);

	// The largest size the whole name fits in, rather than a fixed one (user, 2026-08-19: "Reduce
	// the font of the title to fit the name"). At FontSize24 "Levski's Roar" overran a window sized
	// to three 28px cells and rendered as "LEVSKI'S" - the clip was silent, which is how it shipped.
	//
	// Measured rather than chosen, so it stays right if either the name or the window changes: a
	// longer name drops a size on its own, and a wider window lets the name grow back.
	const string_view title = _("Levski's Roar");
	const UiFlags titleSize = GetLineWidth(title, GameFont24) <= ContentWidth
	    ? UiFlags::FontSize24
	    : UiFlags::FontSize12;
	DrawString(out, title,
	    Rectangle { window.position + Displacement { Padding, Padding }, { ContentWidth, HeaderHeight } },
	    { UiFlags::ColorGold | titleSize | UiFlags::VerticalCenter });

	// The empty grid first, as one recessed well with cell lines drawn on it - the cells are 28px
	// now, and twelve individually bordered 28px boxes read as noise rather than as a container.
	const Point gridOrigin = GridOrigin(window);
	DrawPanelGround(out, Rectangle { gridOrigin, { GridWidth, GridHeight } }, SlotFillColor);
	for (int column = 1; column < LevskiGridColumns; column++)
		DrawVerticalLine(out, { gridOrigin.x + column * CellSize, gridOrigin.y }, GridHeight, PanelFillColor);
	for (int row = 1; row < LevskiGridRows; row++)
		DrawHorizontalLine(out, { gridOrigin.x, gridOrigin.y + row * CellSize }, GridWidth, PanelFillColor);

	for (int anchor = 0; anchor < LevskiGridSlots; anchor++) {
		if (GridItems[anchor].isEmpty())
			continue;
		// Centred in the item's OWN footprint, not in one cell: a 2x3 armour occupies 56x84 and
		// must be drawn across all of it, which is the whole point of the rebuild.
		const Size size = GetInventorySize(GridItems[anchor]);
		const Rectangle footprint {
			CellRect(window, anchor).position,
			{ size.width * CellSize, size.height * CellSize }
		};
		const ClxSprite sprite = GetInvItemSprite(GridItems[anchor]._iCurs + CURSOR_FIRSTITEM);
		const int x = footprint.position.x + (footprint.size.width - sprite.width()) / 2;
		const int y = footprint.position.y + (footprint.size.height + sprite.height()) / 2;
		ClxDraw(out, { x, y }, sprite);
	}

	// The salvage column. Gold-bordered placeholder boxes, one per tier, lit when the backpack
	// actually holds something that button would consume - so the column doubles as a readout of
	// what is worth pressing rather than seven identical boxes.
	for (int i = 0; i < SalvageTierCount; i++) {
		const auto tier = static_cast<SalvageTier>(i);
		const Rectangle rect = SalvageButtonRect(window, i);
		// The pressed flash, under the border so the frame stays crisp. Fired on mouse-down and
		// held as an expiry, exactly like the inventory SORT button - these buttons run instantly
		// and nothing here polls a mouse-up, so a bool would either linger or need a second owner.
		if (ButtonFlashActive(i)) {
			FillRect(out, rect.position.x + 1, rect.position.y + 1,
			    rect.size.width - 2, rect.size.height - 2, ButtonFlashColor);
		}
		DrawOrnateBorder(out, rect);
		// EVERY page, matching what the button will actually consume. This read the displayed tab
		// only, so a button could sit dark while page 3 was full of rares.
		const bool any = AnySalvageableInBackpack(*MyPlayer, tier);
		DrawString(out, _(SalvageTierName(tier)), rect,
		    { (ButtonFlashActive(i) ? UiFlags::ColorWhite : (any ? UiFlags::ColorGold : UiFlags::ColorWhitegold))
		        | UiFlags::FontSize12 | UiFlags::AlignCenter | UiFlags::VerticalCenter });
	}

	const Rectangle transmute = TransmuteButtonRect(window);
	const int ready = FirstReadyLevskiRecipe(GridItems);
	if (ButtonFlashActive(ButtonFlashTransmute)) {
		FillRect(out, transmute.position.x + 1, transmute.position.y + 1,
		    transmute.size.width - 2, transmute.size.height - 2, ButtonFlashColor);
	}
	DrawOrnateBorder(out, transmute);
	DrawString(out, _("Transmute"), transmute,
	    { (ButtonFlashActive(ButtonFlashTransmute) ? UiFlags::ColorWhite
	                                               : (ready >= 0 ? UiFlags::ColorGold : UiFlags::ColorWhitegold))
	        | UiFlags::FontSize12 | UiFlags::AlignCenter | UiFlags::VerticalCenter });

	const Rectangle book = RecipeButtonRect(window);
	if (ButtonFlashActive(ButtonFlashRecipes)) {
		FillRect(out, book.position.x + 1, book.position.y + 1,
		    book.size.width - 2, book.size.height - 2, ButtonFlashColor);
	}
	DrawOrnateBorder(out, book);
	DrawString(out, RecipeBookOpen ? _("Close recipes") : _("Recipes"), book,
	    { UiFlags::ColorWhite | UiFlags::FontSize12 | UiFlags::AlignCenter | UiFlags::VerticalCenter });

	if (!RecipeBookOpen)
		return;

	const Rectangle page = GetLevskiRecipeBookRect();
	DrawPanelGround(out, page);
	DrawWindowCloseButton(out, page);
	Point cursor = page.position + Displacement { Padding, Padding };
	const int textWidth = page.size.width - Padding * 2;
	DrawString(out, _("Recipes"), Rectangle { cursor, { textWidth, HeaderHeight } },
	    { UiFlags::ColorGold | UiFlags::FontSize24 });
	cursor.y += HeaderHeight;
	// One wrapped block rather than two DrawStrings per recipe at a guessed row height. The name
	// keeps its own colour, so each recipe is drawn as its own pair - but both lines are measured
	// from the SAME wrapped text the panel was sized from, which is what stops the last one being
	// sliced by the bottom edge.
	// Clamped HERE, every frame, rather than only where the wheel turns - the content's height
	// changes with the window width and with how the formulas wrap, so a scroll that was legal when
	// it was set can be past the end by the time it is drawn.
	RecipeBookScroll = std::clamp(RecipeBookScroll, 0, RecipeBookMaxScroll(page));

	const int clipTop = page.position.y + Padding + HeaderHeight;
	const int clipBottom = page.position.y + page.size.height - Padding;
	const std::vector<RecipeRow> rows = RecipeBookRows(page);
	for (int i = 0; i < CraftingRecipeCount; i++) {
		const RecipeRow &row = rows[i];
		// Wholly outside the visible band: skipped rather than drawn and overdrawn. A partially
		// visible row is skipped too - half a formula reads as a rendering fault, not as a hint
		// that there is more below.
		if (row.top < clipTop || row.top + row.height > clipBottom)
			continue;

		const bool ready = CanCraftFromLevskiGrid(GridItems, i);
		const bool selected = SelectedRecipe == i;
		if (selected) {
			// The selection is a filled band behind the block, because the name's colour is
			// already carrying "can this run right now" and one text colour cannot say two things.
			FillRect(out, page.position.x + Padding - 2, row.top - 2,
			    textWidth + 4, row.height - 2, ButtonFlashColor);
		}

		Point rowCursor { page.position.x + Padding, row.top };
		const int lineHeight = GetLineHeight(_(CraftingRecipeName(i)), GameFont12);
		DrawString(out, _(CraftingRecipeName(i)), Rectangle { rowCursor, { textWidth, lineHeight } },
		    { (selected ? UiFlags::ColorWhite : (ready ? UiFlags::ColorGold : UiFlags::ColorWhitegold)) | UiFlags::FontSize12 });
		rowCursor.y += lineHeight;

		const std::string formula = WordWrapString(_(CraftingRecipeInputs(i)), textWidth, GameFont12);
		const int formulaLines = static_cast<int>(std::count(formula.begin(), formula.end(), '\n')) + 1;
		DrawString(out, formula, Rectangle { rowCursor, { textWidth, lineHeight * formulaLines } },
		    { UiFlags::ColorWhite | UiFlags::FontSize12 });
	}
	(void)cursor;
}

bool CheckLevskiRoarClick(Point mousePosition)
{
	if (!WindowOpen)
		return false;

	const Rectangle window = GetLevskiRoarRect();
	const Rectangle book = GetLevskiRecipeBookRect();
	const bool inWindow = window.contains(mousePosition);
	const bool inBook = RecipeBookOpen && book.contains(mousePosition);
	if (!inWindow && !inBook)
		return false; // outside both panels: the click belongs to whatever is under it

	if (inBook) {
		// The book's own X closes the book, not the window under it - each window owns its button.
		if (CheckWindowCloseButtonClick(book, mousePosition)) {
			RecipeBookOpen = false;
			return true;
		}
		// The book is a control surface now (v1.9.18): clicking a recipe SELECTS it, and clicking
		// the selected one again clears the selection. It stopped being a pure reference the moment
		// two recipes could take the same target and the same material at different costs, because
		// then no auto-pick can be the one the player meant.
		//
		// Walked through the same RecipeBookRows the draw used, so a click lands on the row that
		// was actually under the pointer even though the rows are not a fixed height.
		const std::vector<RecipeRow> rows = RecipeBookRows(book);
		const int clipTop = book.position.y + Padding + HeaderHeight;
		const int clipBottom = book.position.y + book.size.height - Padding;
		for (int i = 0; i < CraftingRecipeCount; i++) {
			const RecipeRow &row = rows[i];
			if (row.top < clipTop || row.top + row.height > clipBottom)
				continue; // not drawn, so not clickable - the invisible-cell rule from the skill picker
			if (mousePosition.y < row.top || mousePosition.y >= row.top + row.height)
				continue;
			SelectedRecipe = (SelectedRecipe == i) ? -1 : i;
			return true;
		}
		return true;
	}

	// Before every other control: the X is the one click that must always work, and this window
	// absorbs everything else that lands on it.
	if (CheckWindowCloseButtonClick(window, mousePosition)) {
		CloseLevskiRoar();
		return true;
	}

	// Salvage. Reports what it did, always - a button that silently does nothing because you own no
	// rares is indistinguishable from a button that is broken, and this fork has shipped that exact
	// ambiguity twice.
	for (int i = 0; i < SalvageTierCount; i++) {
		if (!SalvageButtonRect(window, i).contains(mousePosition))
			continue;
		FlashButton(i); // fires whether or not there was anything to salvage - it acknowledges the CLICK
		const auto tier = static_cast<SalvageTier>(i);
		const int consumed = SalvageAllInBackpack(*MyPlayer, tier);
		if (consumed > 0) {
			LogEvent(StrCat("Salvaged ", consumed, " ", _(SalvageTierName(tier)), " into ",
			             _(AllItemsList[SalvageMaterialFor(tier)].iName)),
			    UiFlags::ColorWhitegold);
			PlaySFX(IS_ISHIEL);
		} else {
			LogEvent(StrCat("Nothing to salvage: ", _(SalvageTierName(tier))), UiFlags::ColorWhite);
		}
		return true;
	}

	if (RecipeButtonRect(window).contains(mousePosition)) {
		FlashButton(ButtonFlashRecipes);
		RecipeBookOpen = !RecipeBookOpen;
		return true;
	}

	if (TransmuteButtonRect(window).contains(mousePosition)) {
		FlashButton(ButtonFlashTransmute);
		// TRANSACTIONAL. The recipes rewrite GridItems with no idea of footprints, and freeing
		// sockets is the one that gives back more than it takes - so the repack afterwards can find
		// it has nowhere to put something. Before this snapshot the repack simply dropped whatever
		// would not fit, and a rune or a jewel stopped existing with no message. TransmuteLevskiGrid
		// pre-checks the footprints now, so a rollback here should be unreachable; it stays because
		// "should be unreachable" is not a guarantee to stake a player's stones on, and the next
		// recipe added will not remember to ask.
		Item snapshotItems[LevskiGridSlots];
		int8_t snapshotCells[LevskiGridSlots];
		std::copy(std::begin(GridItems), std::end(GridItems), snapshotItems);
		std::copy(std::begin(GridCells), std::end(GridCells), snapshotCells);

		// Asked BEFORE the transmute, because the transmute reports what it MADE and a refusal made
		// nothing. A selected recipe that cannot run has to say so out loud - a Transmute button
		// that silently does nothing is the exact ambiguity this fork has shipped twice already.
		if (SelectedRecipe >= 0 && !CanCraftFromLevskiGrid(GridItems, SelectedRecipe)) {
			LogEvent(StrCat("Levski's Roar: ", _(CraftingRecipeName(SelectedRecipe)), " is not ready"));
			return true;
		}
		const std::string result = TransmuteLevskiGridWith(GridItems, SelectedRecipe);
		if (!RebuildGridOccupancy()) {
			std::copy(std::begin(snapshotItems), std::end(snapshotItems), GridItems);
			std::copy(std::begin(snapshotCells), std::end(snapshotCells), GridCells);
			LogEvent("Levski's Roar: not enough room - nothing was transmuted");
			return true;
		}
		if (!result.empty())
			LogEvent(StrCat("Levski's Roar: ", result));
		return true;
	}

	// The grid itself: an empty hand takes an item out, a full one puts it in. Swapping is
	// deliberately absent - a click that both takes and gives is how a stone goes missing.
	Player &player = *MyPlayer;
	const int cell = CellAt(window, mousePosition);
	if (cell >= 0) {
		if (!player.HoldItem.isEmpty()) {
			// The clicked cell is the item's top-left, as in the backpack. If the footprint runs
			// off the grid or over something, PlaceInGrid finds the first cell it does fit.
			if (PlaceInGrid(player.HoldItem, cell)) {
				player.HoldItem.clear();
				NewCursor(CURSOR_HAND);
			}
		} else if (GridCells[cell] != 0) {
			// Any covered cell lifts the item, not just its anchor - clicking the bottom half of a
			// breastplate has to work, or half of every large item is dead surface.
			const int anchor = GridCells[cell] - 1;
			player.HoldItem = GridItems[anchor];
			MarkCells(anchor, GetInventorySize(GridItems[anchor]), 0);
			GridItems[anchor].clear();
			NewCursor(player.HoldItem._iCurs + CURSOR_FIRSTITEM);
		}
		return true;
	}

	return true; // padding and header clicks are absorbed, never passed through to the world
}

} // namespace devilution::oracool
