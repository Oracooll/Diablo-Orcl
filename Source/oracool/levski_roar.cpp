#include "oracool/levski_roar.h"

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

#include "oracool/crafting.h"
#include "oracool/event_log.h"
#include "oracool/ornate_border.h"
#include "oracool/window_close.h"
#include "player.h"
#include "utils/language.h"
#include "utils/str_cat.hpp"

namespace devilution::oracool {

namespace {

bool WindowOpen = false;
bool RecipeBookOpen = false;

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
constexpr int WindowWidth = ContentWidth + Padding * 2;
constexpr int WindowHeight = Padding * 2 + HeaderHeight + GridHeight + SlotGap + ButtonHeight * 2 + SlotGap;

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
 * @brief Rebuilds the occupancy map from GridItems.
 *
 * The recipes rewrite GridItems in place - three gems become one, a socketed item becomes an item
 * plus its stones - without any idea of footprints, and the result's sizes are not the inputs'. So
 * after a transmute the map is re-derived rather than patched: collect what is there, clear, and
 * re-place. Anchors may move, which is correct; the alternative is a stone drawn over a helmet.
 */
void RebuildGridOccupancy()
{
	Item items[LevskiGridSlots];
	int count = 0;
	for (Item &slot : GridItems) {
		if (!slot.isEmpty())
			items[count++] = slot;
		slot.clear();
	}
	for (int8_t &cell : GridCells)
		cell = 0;
	// Largest first: a 2x3 placed after four runes may find no run of free cells left, while the
	// runes always fit around it.
	std::sort(items, items + count, [](const Item &a, const Item &b) {
		const Size sa = GetInventorySize(a);
		const Size sb = GetInventorySize(b);
		return sa.width * sa.height > sb.width * sb.height;
	});
	for (int i = 0; i < count; i++)
		PlaceInGrid(items[i], -1);
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


bool IsLevskiRoarOpen() { return WindowOpen; }
bool IsLevskiRecipeBookOpen() { return WindowOpen && RecipeBookOpen; }

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
	const int y = (gnScreenHeight - WindowHeight) / 3;
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
	const int height = Padding * 2 + HeaderHeight + textHeight;
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
	return Rectangle { { x, window.position.y }, { bookWidth, height } };
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

	const Rectangle transmute = TransmuteButtonRect(window);
	const int ready = FirstReadyLevskiRecipe(GridItems);
	DrawOrnateBorder(out, transmute);
	DrawString(out, _("Transmute"), transmute,
	    { (ready >= 0 ? UiFlags::ColorGold : UiFlags::ColorWhitegold) | UiFlags::FontSize12
	        | UiFlags::AlignCenter | UiFlags::VerticalCenter });

	const Rectangle book = RecipeButtonRect(window);
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
	for (int i = 0; i < CraftingRecipeCount; i++) {
		const bool ready = CanCraftFromLevskiGrid(GridItems, i);
		const int lineHeight = GetLineHeight(_(CraftingRecipeName(i)), GameFont12);
		DrawString(out, _(CraftingRecipeName(i)), Rectangle { cursor, { textWidth, lineHeight } },
		    { (ready ? UiFlags::ColorGold : UiFlags::ColorWhitegold) | UiFlags::FontSize12 });
		cursor.y += lineHeight;

		const std::string formula = WordWrapString(_(CraftingRecipeInputs(i)), textWidth, GameFont12);
		const int formulaLines = static_cast<int>(std::count(formula.begin(), formula.end(), '\n')) + 1;
		DrawString(out, formula, Rectangle { cursor, { textWidth, lineHeight * formulaLines } },
		    { UiFlags::ColorWhite | UiFlags::FontSize12 });
		cursor.y += lineHeight * formulaLines + 6;
	}
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
		if (CheckWindowCloseButtonClick(book, mousePosition))
			RecipeBookOpen = false;
		return true; // otherwise the book is a reference, not a control surface
	}

	// Before every other control: the X is the one click that must always work, and this window
	// absorbs everything else that lands on it.
	if (CheckWindowCloseButtonClick(window, mousePosition)) {
		CloseLevskiRoar();
		return true;
	}

	if (RecipeButtonRect(window).contains(mousePosition)) {
		RecipeBookOpen = !RecipeBookOpen;
		return true;
	}

	if (TransmuteButtonRect(window).contains(mousePosition)) {
		const std::string result = TransmuteLevskiGrid(GridItems);
		RebuildGridOccupancy(); // the recipes rewrite GridItems with no idea of footprints
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
