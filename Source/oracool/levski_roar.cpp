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

/** The nine transmute slots. Live only while the window is open - see the header's note on why
 * this is deliberately not save state. */
Item GridItems[LevskiGridSlots];

// Geometry. The window is sized from the grid rather than the other way round, so changing the
// grid's dimensions cannot leave the panel the wrong shape.
constexpr int SlotSize = 56; // two inventory cells, so a 2x3 two-hander still reads in a slot
constexpr int SlotGap = 6;
constexpr int Padding = 14;
constexpr int HeaderHeight = 30;
constexpr int ButtonHeight = 26;
constexpr int GridWidth = LevskiGridColumns * SlotSize + (LevskiGridColumns - 1) * SlotGap;
constexpr int GridHeight = LevskiGridRows * SlotSize + (LevskiGridRows - 1) * SlotGap;
constexpr int WindowWidth = GridWidth + Padding * 2;
constexpr int WindowHeight = Padding * 2 + HeaderHeight + GridHeight + SlotGap + ButtonHeight * 2 + SlotGap;

constexpr int RecipeRowHeight = 40;
constexpr int RecipeBookWidth = 420;
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
 * through". A dark entry of the 16-shade grey ramp, from the shared upper half of the palette
 * (128-255) that is identical across town and all four tilesets - the same discipline
 * DrawOrnateBorder follows, so the fill cannot recolour itself by level. Not the ramp's darkest:
 * that is reserved for the cells, which need somewhere darker to go.
 */
constexpr uint8_t PanelFillColor = PAL16_GRAY + 12;
/** The grid cells, two shades darker than the panel they sit in - so a slot reads as a recessed
 * well waiting for a stone rather than as a square someone drew on the stone (user screenshot,
 * 2026-08-19: the cells were the same colour as the panel and read as decoration). */
constexpr uint8_t SlotFillColor = PAL16_GRAY + 15;

void DrawPanelGround(const Surface &out, const Rectangle &rect, uint8_t fill = PanelFillColor)
{
	FillRect(out, rect.position.x, rect.position.y, rect.size.width, rect.size.height, fill);
	DrawOrnateBorder(out, rect);
}

/** @brief The recipe book's lines, pre-wrapped to its own text width - the formulas are long
 * enough that "1 socketed item -> the item, emptied, and its stones back" ran off the panel and
 * the last line was sliced by the bottom edge. */
std::string RecipeBookText()
{
	std::string page;
	for (int i = 0; i < CraftingRecipeCount; i++) {
		if (i > 0)
			page += '\n';
		page += _(CraftingRecipeName(i));
		page += '\n';
		page += WordWrapString(_(CraftingRecipeInputs(i)), RecipeBookWidth - Padding * 2, GameFont12);
		page += '\n';
	}
	return page;
}

Point GridOrigin(const Rectangle &window)
{
	return window.position + Displacement { Padding, Padding + HeaderHeight };
}

Rectangle SlotRect(const Rectangle &window, int slot)
{
	const Point origin = GridOrigin(window);
	const int column = slot % LevskiGridColumns;
	const int row = slot / LevskiGridColumns;
	return Rectangle { { origin.x + column * (SlotSize + SlotGap), origin.y + row * (SlotSize + SlotGap) },
		{ SlotSize, SlotSize } };
}

Rectangle TransmuteButtonRect(const Rectangle &window)
{
	const int y = GridOrigin(window).y + GridHeight + SlotGap;
	return Rectangle { { window.position.x + Padding, y }, { GridWidth, ButtonHeight } };
}

Rectangle RecipeButtonRect(const Rectangle &window)
{
	const Rectangle transmute = TransmuteButtonRect(window);
	return Rectangle { { transmute.position.x, transmute.position.y + ButtonHeight + SlotGap },
		{ GridWidth, ButtonHeight } };
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
	const std::string page = RecipeBookText();
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
	// So: try left, fall back to right if left does not fit, then clamp into the screen regardless -
	// a book too wide for either side is still fully readable, just overlapping.
	int x = window.position.x - RecipeBookWidth - SlotGap;
	if (x < 0)
		x = window.position.x + window.size.width + SlotGap;
	x = std::clamp(x, 0, std::max(0, gnScreenWidth - RecipeBookWidth));
	return Rectangle { { x, window.position.y }, { RecipeBookWidth, height } };
}

void DrawLevskiRoar(const Surface &out)
{
	if (!WindowOpen)
		return;

	const Rectangle window = GetLevskiRoarRect();
	DrawPanelGround(out, window);
	DrawWindowCloseButton(out, window);

	DrawString(out, _("Levski's Roar"),
	    Rectangle { window.position + Displacement { Padding, Padding }, { GridWidth, HeaderHeight } },
	    { UiFlags::ColorGold | UiFlags::FontSize24 });

	for (int slot = 0; slot < LevskiGridSlots; slot++) {
		const Rectangle cell = SlotRect(window, slot);
		DrawPanelGround(out, cell, SlotFillColor);
		if (GridItems[slot].isEmpty())
			continue;
		// Centred in the cell: an item's own frame is 1x1 to 2x3 cells, so anchoring to a corner
		// would leave the small ones floating.
		const ClxSprite sprite = GetInvItemSprite(GridItems[slot]._iCurs + CURSOR_FIRSTITEM);
		const int x = cell.position.x + (cell.size.width - sprite.width()) / 2;
		const int y = cell.position.y + (cell.size.height + sprite.height()) / 2;
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
		if (!result.empty())
			LogEvent(StrCat("Levski's Roar: ", result));
		return true;
	}

	// The grid itself: an empty hand takes an item out, a full one puts it in. Swapping is
	// deliberately absent - a click that both takes and gives is how a stone goes missing.
	Player &player = *MyPlayer;
	for (int slot = 0; slot < LevskiGridSlots; slot++) {
		if (!SlotRect(window, slot).contains(mousePosition))
			continue;
		if (!player.HoldItem.isEmpty()) {
			if (GridItems[slot].isEmpty()) {
				GridItems[slot] = player.HoldItem;
				player.HoldItem.clear();
				NewCursor(CURSOR_HAND);
			}
		} else if (!GridItems[slot].isEmpty()) {
			player.HoldItem = GridItems[slot];
			GridItems[slot].clear();
			NewCursor(player.HoldItem._iCurs + CURSOR_FIRSTITEM);
		}
		return true;
	}

	return true; // padding and header clicks are absorbed, never passed through to the world
}

} // namespace devilution::oracool
