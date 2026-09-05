#include "oracool/levski_roar.h"

#include <SDL.h>

#include <fmt/format.h>

#include <algorithm>
#include <string>

#include "DiabloUI/ui_flags.hpp"
#include "control.h"
#include "cursor.h"
#include "engine/trn.hpp" // GetInfravisionTRN - the unusable-item grey, at 3x
#include "engine/render/clx_render.hpp"
#include "engine/surface.hpp"
#include "engine/render/primitive_render.hpp"
#include "engine/render/text_render.hpp"
#include "inv.h"
#include "items.h"
#include "objects.h"

#include "oracool/badge.h"
#include "oracool/crafting.h"
#include "oracool/event_log.h"
#include "oracool/hud_art.h" // DrawLoosePng, DrawRedCross - the painted skin and its states
#include "oracool/levski_roar_skin.h"
#include "oracool/book_frame.h" // the painted tall frame the recipe book wears
#include "oracool/ornate_border.h"
#include "oracool/salvage.h"
#include "oracool/socket_overlay.h"
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
// THE PAINTED SKIN's cell, not the inventory's (user, 2026-09-04: "Place it as Levski's Interface").
// The painting's grid cells are ~185px; the window is drawn at the scale that makes them CellSize
// (28 since 2026-09-04 - "regular game size"; 84 for the day before) and items at ItemScale, which is
// 1 now and sends the grid through the ordinary DrawItem path - see levski_roar_skin.h. The
// footprint rules are untouched: a 2x3 armour still covers 2x3 cells, and at 1x they are the same cells.
constexpr int CellSize = levski_skin::CellSize;
constexpr int ItemScale = CellSize / InventorySlotSizeInPixels.width;
static_assert(ItemScale * InventorySlotSizeInPixels.width == CellSize, "the skin's cell is not a whole multiple of the item cell");
constexpr int SlotGap = 6;
constexpr int Padding = 14;
constexpr int HeaderHeight = 30;
constexpr int ButtonHeight = 26;
constexpr int GridWidth = LevskiGridColumns * CellSize;
constexpr int GridHeight = LevskiGridRows * CellSize;

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

/**
 * @brief The window is the painted skin, at the size the cutter chose - see levski_roar_skin.h,
 * which tools/CutLevskiRoarSkin.ps1 generates from the same pass that writes the art.
 *
 * Nothing here is measured by hand any more. The quest-log frame this window wore for a day
 * (v1.9.201) and the sized-from-content window before it are both gone: the painting carries the
 * frame, the title, the grid well and every button plate with its label, so the code draws STATE
 * on top of it and nothing else.
 */
constexpr Size FrameSize = levski_skin::WindowSize;
constexpr const char *LevskiBackgroundAsset = "ui\\levski_bg.png";

/** @brief One of the ten painted plates, in window space. */
Rectangle ButtonRect(const Rectangle &window, int index)
{
	const Rectangle &r = levski_skin::ButtonRects[index];
	return Rectangle { window.position + Displacement { r.position.x, r.position.y }, r.size };
}

Rectangle CloseButtonRect(const Rectangle &window)
{
	return ButtonRect(window, levski_skin::Close);
}

/**  How wide the book may be: all the room left of the window, capped, never overlapping it.
 *
 * A constant 420 was wrong twice - first drawn off the left edge, then clamped to x=0 where it sat
 * ON TOP of Levski's own window and covered its title (user screenshot, 2026-08-19). The room to the
 * left of a centred window is what it is; the book has to fit that, not assume it. */
int RecipeBookWidthFor(const Rectangle &window)
{
	// The painted tall frame's width (2026-09-05): a painting cannot be narrower for a narrow
	// screen, so the book is its frame's size and slides to x=0 when the room left of the window
	// runs out - the clamp GetLevskiRecipeBookRect already does.
	(void)window;
	return BookFrameSize(BookFrame::Tall).width;
}

/** @brief The frame's clear core: where the book's title, rows and clips live (the bezel is outside it). */
Rectangle RecipeBookInner(const Rectangle &page)
{
	return BookFrameCore(BookFrame::Tall, page);
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
	const Rectangle inner = RecipeBookInner(page);
	std::vector<RecipeRow> rows;
	rows.reserve(CraftingRecipeCount);
	const int textWidth = inner.size.width - Padding * 2;
	int y = inner.position.y + Padding + HeaderHeight - RecipeBookScroll;
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
	const Rectangle inner = RecipeBookInner(page);
	if (inner.size.height <= 0)
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
	const int visibleBottom = inner.position.y + inner.size.height - Padding;
	return std::max(0, contentBottom - visibleBottom);
}

Point GridOrigin(const Rectangle &window)
{
	// Centred: the grid is narrower than the window's own buttons.
	return window.position + Displacement { levski_skin::GridOrigin.x, levski_skin::GridOrigin.y };
}

Rectangle CellRect(const Rectangle &window, int cell)
{
	// Stepped by the PAINTED pitch, sized as the item cell (2026-09-05, the 1:1 painting): the
	// grid is painted at 29 and the item sprite is 28, so the cell sits inside its painted square
	// with the rule around it rather than the painting being squeezed to make the two agree.
	const Point origin = GridOrigin(window);
	return Rectangle { { origin.x + (cell % LevskiGridColumns) * levski_skin::GridPitch, origin.y + (cell / LevskiGridColumns) * levski_skin::GridPitch },
		{ CellSize, CellSize } };
}

/**
 * @brief The cell's whole painted square - the 29px pitch, rules included - for hit-testing.
 *
 * CellRect is the 28px ITEM cell, which leaves a one-pixel seam between neighbours where a click
 * landed on nothing (user, 2026-09-05: "i am having some difficulty placing my items exactly where
 * i want them"). The seam belongs to the cell it borders, so the hit rect is the pitch square.
 */
Rectangle CellHitRect(const Rectangle &window, int cell)
{
	const Point origin = GridOrigin(window);
	return Rectangle { { origin.x + (cell % LevskiGridColumns) * levski_skin::GridPitch, origin.y + (cell / LevskiGridColumns) * levski_skin::GridPitch },
		{ levski_skin::GridPitch, levski_skin::GridPitch } };
}

/** @brief The cell under @p position, or -1. */
int CellAt(const Rectangle &window, Point position)
{
	for (int cell = 0; cell < LevskiGridSlots; cell++) {
		if (CellHitRect(window, cell).contains(position))
			return cell;
	}
	return -1;
}

/**
 * @brief Where a HELD item of @p size lands when dropped at @p position: its top-left cell.
 *
 * The backpack's rule, exactly (inv.cpp FindTargetSlotUnderItemCursor): the cursor carries the
 * item by its CENTRE, so the cell under the cursor is the item's middle cell, not its corner. This
 * grid used to take the clicked cell as the top-left, which put a 2x3 armour one cell right and
 * one down from where it was drawn under the cursor - the difficulty the user reported. An even
 * size has no middle cell, so the half the cursor is in decides, with the same 14px probe. Clamped
 * to the grid, so dropping near an edge slides the item in rather than refusing.
 */
int TargetAnchorUnderItemCursor(const Rectangle &window, Point position, Size size)
{
	const int hot = CellAt(window, position);
	if (hot < 0)
		return -1;
	if (size.width <= 1 && size.height <= 1)
		return hot;
	constexpr int HalfCell = levski_skin::CellSize / 2;
	Displacement offset { (size.width - 1) / 2, (size.height - 1) / 2 };
	const Rectangle hotRect = CellHitRect(window, hot);
	if (size.width % 2 == 0 && hotRect.contains(position + Displacement { HalfCell, 0 }))
		offset.deltaX++;
	if (size.height % 2 == 0 && hotRect.contains(position + Displacement { 0, HalfCell }))
		offset.deltaY++;
	const int row = std::clamp(hot / LevskiGridColumns - offset.deltaY, 0, LevskiGridRows - size.height);
	const int column = std::clamp(hot % LevskiGridColumns - offset.deltaX, 0, LevskiGridColumns - size.width);
	return row * LevskiGridColumns + column;
}

/**
 * @brief The ANCHOR of the item under the cursor, or -1. The grid's answer to pcursinvitem.
 *
 * Anchor rather than cell, because that is what identifies an ITEM here: a 2x3 armour occupies six
 * cells and GridCells[c] holds its anchor + 1 in every one of them, so hovering any part of it has
 * to name the same item. Both the draw and the tooltip ask this, which is what keeps the outlined
 * item and the described item from ever being two different items.
 */
int HoveredAnchor()
{
	if (!WindowOpen)
		return -1;
	const int cell = CellAt(GetLevskiRoarRect(), MousePosition);
	if (cell < 0 || GridCells[cell] == 0)
		return -1;
	return GridCells[cell] - 1;
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
	return ButtonRect(window, levski_skin::Transmute);
}

Rectangle RecipeButtonRect(const Rectangle &window)
{
	return ButtonRect(window, levski_skin::Recipes);
}

/** @brief Salvage button @p index, counting down the column from the top. */
Rectangle SalvageButtonRect(const Rectangle &window, int index)
{
	return ButtonRect(window, levski_skin::SalvageFirst + index);
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
	const Rectangle bookInner = RecipeBookInner(book);
	if (bookInner.size.height <= 0)
		return false;
	// A wheel notch moves about one recipe's worth. Bounded at BOTH ends, for the reason recorded
	// on the skill picker's own scroll: without the upper bound the wheel pushes the list past its
	// last row and the panel goes blank, which reads as a crash rather than as the end of a list.
	constexpr int PixelsPerNotch = 40;
	RecipeBookScroll = std::clamp(RecipeBookScroll - notches * PixelsPerNotch, 0, RecipeBookMaxScroll(book));
	return true;
}

const Item *HoveredLevskiGridItem()
{
	const int anchor = HoveredAnchor();
	return anchor < 0 ? nullptr : &GridItems[anchor];
}

bool SetLevskiHoverInfoString()
{
	// The controls first (2026-09-05): icon plates carry no label, so the info panel says what each
	// one does while the cursor is on it. The salvage line names the BACKPACK, because that is what
	// SalvageAllInBackpack acts on - not the grid the cursor is next to.
	if (WindowOpen) {
		const Rectangle window = GetLevskiRoarRect();
		for (int i = levski_skin::Close + 1; i < levski_skin::ButtonCount; i++) {
			if (!ButtonRect(window, i).contains(MousePosition))
				continue;
			if (i == levski_skin::Transmute)
				SetPanelString(_("Transmute"), UiFlags::ColorWhitegold);
			else if (i == levski_skin::Recipes)
				SetPanelString(_("Recipes"), UiFlags::ColorWhitegold);
			else
				SetPanelString(StrCat("Salvage all ", _(SalvageTierName(static_cast<SalvageTier>(i - levski_skin::SalvageFirst))), " in backpack"),
				    UiFlags::ColorWhitegold);
			return true;
		}
	}

	const int anchor = HoveredAnchor();
	if (anchor < 0)
		return false;

	// The stash's own three lines, and deliberately those exact three (see CheckStashHLight): the
	// name through SetPanelString so the tier colour is recorded as line 0's, then the full block for
	// an identified item and the durability line for one that is not. Written the same way so an item
	// reads identically wherever the player is looking at it - which is the whole of the request.
	const Item &item = GridItems[anchor];
	SetPanelString(item.getName(), item.getTextColor());
	if (item._iIdentified)
		PrintItemDetails(item);
	else
		PrintItemDur(item);
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

void ResetLevskiRoarForNewGame()
{
	// Unconditional, and it does NOT try to give anything back - by the time this runs the player is
	// being torn down and has already been saved, so a return would go nowhere.
	//
	// Audit, 2026-08-30. GridItems and WindowOpen are file-local statics, so they live for the whole
	// PROCESS, not the game. Leaving a game does not close this window: "Main Menu" and "Exit Game"
	// both funnel through GamemenuNewGame, which saves the character and clears gbRunGame without
	// closing anything, and CloseLevskiRoar is allowed to REFUSE while the pack is full. So the
	// window stayed open and the grid stayed full into the next character started in the same
	// session - which showed them someone else's items and let them take them out.
	//
	// CloseLevskiRoar is attempted before the save (see GamemenuNewGame), so anything that fits in
	// the backpack is kept and persisted. This is the backstop for what did not fit.
	for (Item &slot : GridItems)
		slot.clear();
	for (int8_t &cell : GridCells)
		cell = 0;
	WindowOpen = false;
	RecipeBookOpen = false;
	RecipeBookScroll = 0;
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

bool PlaceItemInLevskiGrid(const Item &item)
{
	// The grid is packed by FOOTPRINT, so "is a slot free" and "does this fit" are different
	// questions and only PlaceInGrid answers the second. Callers must place before they remove.
	return WindowOpen && PlaceInGrid(item, -1);
}

Rectangle GetLevskiRoarRect()
{
	if (!WindowOpen)
		return Rectangle { { 0, 0 }, { 0, 0 } };
	// Centred on the play area, like the other operable-object windows.
	const int x = (gnScreenWidth - FrameSize.width) / 2;
	// CENTRED vertically (user, 2026-08-27: "Levski's Roar should be middle of screen"). It sat a
	// third of the way down before, and was briefly bottom-docked by a rule that was never meant for
	// it - the docking rule is about the side panels.
	const int y = std::max(0, (static_cast<int>(gnScreenHeight) - FrameSize.height) / 2);
	return Rectangle { { x, y }, FrameSize };
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
	// The painted frame's height (2026-09-05), not the text's: a painting is one size. The band
	// still caps it on a screen shorter than the frame; the text scrolls inside whatever is left.
	(void)textHeight;
	(void)MaxBookHeight;
	const int height = std::min(BookFrameSize(BookFrame::Tall).height, band);
	// LEFT of the window by preference: opening right ran the book under the mini-map, which owns
	// the top-right corner.
	//
	// But NOT unconditionally, and the previous comment here - "there is always room on the left,
	// the window is centred" - was simply false, which a screenshot caught (user, 2026-08-19: the
	// book's title read "IPES" and every line lost its first characters off the left edge). The
	// window is centred in gnScreenWidth, so the room to its left is (gnScreenWidth - FrameSize.width)/2,
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

namespace {

/** @brief The press-flash slot a painted button maps to, or -1 for the close button, which has none. */
int FlashIndexForButton(int button)
{
	if (button == levski_skin::Transmute)
		return ButtonFlashTransmute;
	if (button == levski_skin::Recipes)
		return ButtonFlashRecipes;
	if (button >= levski_skin::SalvageFirst)
		return button - levski_skin::SalvageFirst;
	return -1;
}

/**
 * @brief Nearest-neighbour blit of @p sprite at @p scale, top-left at @p topLeft, through @p trn if given.
 *
 * Through a scratch surface rather than a scaled CLX: the grid holds at most twelve items and is a
 * window, so the per-frame cost is nothing, and a scaled list per cursor id would be a cache to
 * invalidate. Index 0 is treated as transparent - the item art's baked shadows drop at 3x, which is
 * a smaller wrong than a black halo three pixels wide.
 */
void DrawSpriteScaled(const Surface &out, Point topLeft, ClxSprite sprite, int scale, const uint8_t *trn)
{
	const int w = static_cast<int>(sprite.width());
	const int h = static_cast<int>(sprite.height());
	if (w <= 0 || h <= 0)
		return;
	OwnedSurface scratch(w, h);
	SDL_FillRect(scratch.surface, nullptr, 0);
	ClxDraw(scratch, { 0, h - 1 }, sprite);
	for (int y = 0; y < h; y++) {
		for (int x = 0; x < w; x++) {
			uint8_t index = *scratch.at(x, y);
			if (index == 0)
				continue;
			if (trn != nullptr)
				index = trn[index];
			for (int yy = 0; yy < scale; yy++) {
				const int dy = topLeft.y + y * scale + yy;
				if (dy < 0 || dy >= out.h())
					continue;
				for (int xx = 0; xx < scale; xx++) {
					const int dx = topLeft.x + x * scale + xx;
					if (dx < 0 || dx >= out.w())
						continue;
					*out.at(dx, dy) = index;
				}
			}
		}
	}
}

} // namespace

void DrawLevskiRoar(const Surface &out)
{
	if (!WindowOpen)
		return;

	const Rectangle window = GetLevskiRoarRect();
	// The painted skin. Everything the old window drew itself - frame, title, grid well, plates and
	// labels - is in the painting; what is drawn here is STATE: items in the grid, a plate under the
	// cursor or mid-press, and a plate dimmed because pressing it would do nothing.
	if (GetLoosePngSize(LevskiBackgroundAsset).width == 0)
		DrawPanelGround(out, window); // the skin did not load: the flat ground, so the window still exists
	DrawLoosePng(out, LevskiBackgroundAsset, window.position);

	const int hoveredAnchor = HoveredAnchor();
	for (int anchor = 0; anchor < LevskiGridSlots; anchor++) {
		if (GridItems[anchor].isEmpty())
			continue;
		const Item &item = GridItems[anchor];
		const Size size = GetInventorySize(item);
		const Rectangle footprint {
			CellRect(window, anchor).position,
			{ size.width * CellSize, size.height * CellSize }
		};
		const ClxSprite sprite = GetInvItemSprite(item._iCurs + CURSOR_FIRSTITEM);
		// Centred in the footprint at ItemScale - the sprite is cut to 28px cells.
		const Point topLeft {
			footprint.position.x + (footprint.size.width - sprite.width() * ItemScale) / 2,
			footprint.position.y + (footprint.size.height - sprite.height() * ItemScale) / 2
		};
		// At any other scale, what DrawItem does at 1x done by hand: the grey for gear the character cannot use, the
		// red X for a broken item, the stack count in the corner. The socket overlay is NOT drawn -
		// its dots are placed for a 1x sprite - but the hover panel still names the gems, which is
		// what the user asked for when this grid learned to hover (2026-09-03).
		const bool usable = !IsInspectingPlayer() ? item._iStatFlag : InspectPlayer->CanUseItem(item);
		if constexpr (ItemScale == 1) {
			// Regular game size (user, 2026-09-04): the ordinary item draw, with everything it
			// carries - shadows, the grey, the red X, the stack count - and the socket overlay and
			// outline the backpack gives an item under the cursor. Nothing here is a copy of it.
			const Point bottomLeft { topLeft.x, topLeft.y + sprite.height() - 1 };
			if (anchor == hoveredAnchor)
				ClxDrawOutline(out, GetOutlineColor(item, true), bottomLeft, sprite);
			DrawItem(item, out, bottomLeft, sprite);
			if (anchor == hoveredAnchor)
				DrawSocketOverlay(out, item, bottomLeft, size);
			continue;
		}
		DrawSpriteScaled(out, topLeft, sprite, ItemScale, usable ? nullptr : GetInfravisionTRN());
		if (item._iOracoolBroken)
			DrawRedCross(out, footprint);
		if (item.isStackableConsumable() && item.stackCount() > 1)
			DrawBadge(out, footprint, BadgeCorner::BottomRight, StrCat(item.stackCount()));
		if (anchor == hoveredAnchor)
			DrawColoredOutline(out, footprint, GetOutlineColor(item, true));
	}

	// The close button: the game's own red X, where the skin puts it (the painting has no plate for
	// it, and its frame's corner is not the rect's corner - see the cutter).
	DrawWindowCloseButtonAt(out, CloseButtonRect(window));

	// The SALVAGE title over the block (user, 2026-09-05: "Gold, with text shadow. Appropriate font
	// size"): 24px, the window title's own gold, and the same shadow the hero sheet's text wears.
	// Skipped when the skin gives it no room: the 2026-09-05 painting carries SALVAGE on its own
	// stone plate, and a second SALVAGE drawn over it would be the one thing worse than none.
	if (const Rectangle &t = levski_skin::SalvageTitleRect; t.size.height > 0) {
		DrawString(out, _("SALVAGE"), Rectangle { window.position + Displacement { t.position.x, t.position.y }, t.size },
		    { UiFlags::ColorGold | UiFlags::FontSize24 | UiFlags::AlignCenter | UiFlags::VerticalCenter | UiFlags::Shadowed });
	}

	// The nine controls (2026-09-05, "3 rows of 3 icons"): GPT's 32px icon plates, three states each.
	// The painting carries NO plates for them, so the DEFAULT frame goes down at rest and the hover
	// or pressed frame replaces it while the cursor is on it or the press flash is running. Where a
	// hover file is the plain plate (HoverIsPlain), the hover is marked with the theme's outline.
	const int ready = FirstReadyLevskiRecipe(GridItems);
	for (int i = levski_skin::Close + 1; i < levski_skin::ButtonCount; i++) {
		const Rectangle rect = ButtonRect(window, i);
		const bool hovered = rect.contains(MousePosition);
		const int flash = FlashIndexForButton(i);
		const bool pressed = flash >= 0 && ButtonFlashActive(flash);
		// At rest the painting is the control (the 2026-09-05 skin paints every plate in); only a
		// HOVER or PRESSED overlay is ever drawn, cut to the painted plate's own size.
		if (pressed || hovered) {
			const std::string state = StrCat("ui\\levski_", levski_skin::ButtonStems[i], pressed ? "_pressed.png" : "_hover.png");
			DrawLoosePng(out, state.c_str(), rect.position);
			if (hovered && !pressed && levski_skin::HoverIsPlain[i])
				DrawHoverOutline(out, rect);
			continue;
		}
		// The readout the old gold-vs-whitegold label carried: a plate that would do nothing right
		// now sits under a shade, so the column still says what is worth pressing.
		bool idle = false;
		if (i == levski_skin::Transmute)
			idle = ready < 0;
		else if (i >= levski_skin::SalvageFirst)
			idle = !AnySalvageableInBackpack(*MyPlayer, static_cast<SalvageTier>(i - levski_skin::SalvageFirst));
		if (idle)
			DrawHalfTransparentRectTo(out, rect.position.x, rect.position.y, rect.size.width, rect.size.height);
	}

	if (!RecipeBookOpen)
		return;

	const Rectangle page = GetLevskiRecipeBookRect();
	// The painted tall frame (user, 2026-09-05): dark backing in its core, the bezel over it, the
	// red X at the frame's top-right.
	DrawBookFrame(out, BookFrame::Tall, page);
	DrawWindowCloseButton(out, page);
	const Rectangle inner = RecipeBookInner(page);
	Point cursor = inner.position + Displacement { Padding, Padding };
	const int textWidth = inner.size.width - Padding * 2;
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

	const int clipTop = inner.position.y + Padding + HeaderHeight;
	const int clipBottom = inner.position.y + inner.size.height - Padding;
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
			FillRect(out, inner.position.x + Padding - 2, row.top - 2,
			    textWidth + 4, row.height - 2, ButtonFlashColor);
		}

		Point rowCursor { inner.position.x + Padding, row.top };
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

bool CheckLevskiRoarClick(Point mousePosition, bool isCtrlHeld)
{
	if (!WindowOpen)
		return false;

	const Rectangle window = GetLevskiRoarRect();
	const Rectangle book = GetLevskiRecipeBookRect();
	const Rectangle bookInner = RecipeBookInner(book); // the rows and clips are laid out from the frame's core
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
		const int clipTop = bookInner.position.y + Padding + HeaderHeight;
		const int clipBottom = bookInner.position.y + bookInner.size.height - Padding;
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
	if (CloseButtonRect(window).contains(mousePosition)) {
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
		// STAMP USABILITY on everything the transmute left behind (user, 2026-08-28: "picking up a
		// crafted item the first time colors it in RED").
		//
		// It was red because _iStatFlag was false. Nothing in the crafting path ever set it - the
		// recipes build items and hand them back, and the flag is normally written by CalcPlrInv,
		// which walks the worn slots and the backpack and has no idea this grid exists. So a fresh
		// item sat here with the flag clear, DrawItem read that as "the character cannot use this"
		// and tinted it through the infravision TRN, which is red. It corrected itself the moment
		// the item reached the backpack and CalcPlrInv ran over it, which is exactly why it was only
		// ever seen once per item.
		for (Item &item : GridItems) {
			if (!item.isEmpty())
				item._iStatFlag = MyPlayer->CanUseItem(item);
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
		// CTRL sends it straight back to the backpack instead of onto the cursor (user, 2026-08-28:
		// "ctrl+click to send items to levskis grid and back to my inv grid, not drop them on the
		// ground"). The same gesture the stash uses, in the same direction: ctrl means "move it to
		// the other container", never "pick it up".
		if (isCtrlHeld && player.HoldItem.isEmpty() && GridCells[cell] != 0) {
			const int anchor = GridCells[cell] - 1;
			if (!AutoPlaceItemInInventory(player, GridItems[anchor], true)) {
				LogEvent(std::string(_("Your pack is full.")), UiFlags::ColorRed);
				return true;
			}
			PlaySFX(ItemInvSnds[GetItemDropAnimIndex(GridItems[anchor]._iCurs)]);
			MarkCells(anchor, GetInventorySize(GridItems[anchor]), 0);
			GridItems[anchor].clear();
			return true;
		}
		if (!player.HoldItem.isEmpty()) {
			// The item lands where it is DRAWN under the cursor - its centre on the clicked cell,
			// as in the backpack (TargetAnchorUnderItemCursor). If that footprint is over something,
			// PlaceInGrid finds the first cell it does fit.
			const int anchor = TargetAnchorUnderItemCursor(window, mousePosition, GetInventorySize(player.HoldItem));
			if (PlaceInGrid(player.HoldItem, anchor)) {
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
