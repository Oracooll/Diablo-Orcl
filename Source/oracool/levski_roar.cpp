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
/** Two half-transparent passes plus a dark fill: one pass alone left the town's rooftops legible
 * straight through the grid (user screenshot, 2026-08-19). Every other window in the game sits on
 * painted art; this one sits on open ground, so it has to build its own opacity. */
constexpr uint8_t PanelFillColor = 0;

void DrawPanelGround(const Surface &out, const Rectangle &rect)
{
	DrawHalfTransparentRectTo(out, rect.position.x, rect.position.y, rect.size.width, rect.size.height, PanelFillColor);
	DrawHalfTransparentRectTo(out, rect.position.x, rect.position.y, rect.size.width, rect.size.height);
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
	// LEFT of the window, not right: opening right ran the book under the mini-map, which owns the
	// top-right corner. There is always room on the left - the window is centred.
	return Rectangle { { window.position.x - RecipeBookWidth - SlotGap, window.position.y },
		{ RecipeBookWidth, height } };
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
		DrawPanelGround(out, cell);
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
