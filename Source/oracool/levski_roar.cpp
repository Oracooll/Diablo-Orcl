#include "oracool/levski_roar.h"

#include <fmt/format.h>

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
constexpr int RecipeBookWidth = 360;

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
	const int height = Padding * 2 + HeaderHeight + CraftingRecipeCount * RecipeRowHeight;
	// Beside the window rather than over it - the book is a reference while you work, so covering
	// the grid with it would defeat the point of having both open.
	return Rectangle { { window.position.x + window.size.width + SlotGap, window.position.y },
		{ RecipeBookWidth, height } };
}

void DrawLevskiRoar(const Surface &out)
{
	if (!WindowOpen)
		return;

	const Rectangle window = GetLevskiRoarRect();
	DrawHalfTransparentRectTo(out, window.position.x, window.position.y, window.size.width, window.size.height);
	DrawOrnateBorder(out, window);

	DrawString(out, _("Levski's Roar"),
	    Rectangle { window.position + Displacement { Padding, Padding }, { GridWidth, HeaderHeight } },
	    { UiFlags::ColorGold | UiFlags::FontSize24 });

	for (int slot = 0; slot < LevskiGridSlots; slot++) {
		const Rectangle cell = SlotRect(window, slot);
		DrawHalfTransparentRectTo(out, cell.position.x, cell.position.y, cell.size.width, cell.size.height);
		DrawOrnateBorder(out, cell);
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
	DrawHalfTransparentRectTo(out, page.position.x, page.position.y, page.size.width, page.size.height);
	DrawOrnateBorder(out, page);
	Point cursor = page.position + Displacement { Padding, Padding };
	const int textWidth = page.size.width - Padding * 2;
	DrawString(out, _("Recipes"), Rectangle { cursor, { textWidth, HeaderHeight } },
	    { UiFlags::ColorGold | UiFlags::FontSize24 });
	cursor.y += HeaderHeight;
	for (int i = 0; i < CraftingRecipeCount; i++) {
		const bool ready = CanCraftFromLevskiGrid(GridItems, i);
		DrawString(out, _(CraftingRecipeName(i)), Rectangle { cursor, { textWidth, RecipeRowHeight / 2 } },
		    { (ready ? UiFlags::ColorGold : UiFlags::ColorWhitegold) | UiFlags::FontSize12 });
		DrawString(out, _(CraftingRecipeInputs(i)),
		    Rectangle { { cursor.x, cursor.y + RecipeRowHeight / 2 - 4 }, { textWidth, RecipeRowHeight / 2 } },
		    { UiFlags::ColorWhite | UiFlags::FontSize12 });
		cursor.y += RecipeRowHeight;
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

	if (inBook)
		return true; // the book is a reference, not a control surface

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
