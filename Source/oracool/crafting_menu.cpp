#include "oracool/crafting_menu.h"

#include <algorithm>

#include <fmt/format.h>

#include "DiabloUI/ui_flags.hpp"
#include "automap.h"
#include "control.h"
#include "cursor.h"
#include "engine/render/primitive_render.hpp"
#include "engine/render/text_render.hpp"
#include "player.h"
#include "oracool/crafting.h"
#include "oracool/event_log.h"
#include "oracool/ornate_border.h"
#include "oracool/window_close.h"
#include "utils/language.h"
#include "utils/ui_fwd.h"

namespace devilution::oracool {

namespace {

bool MenuOpen = false;

// The runeword book's own size, to the pixel (user, 2026-08-30: "make CRAFTING book window as big
// as RUNEWORD book"). Stated as its own constant rather than reaching into runeword_book.cpp for
// one, because the two windows are the same SIZE and not the same thing - tying them together would
// mean a change to one silently resizing the other.
constexpr Size WindowSize { 944, 616 };
constexpr int RowHeight = 56;
constexpr int WindowPadding = 12;
constexpr int HeaderHeight = 28;

/** @brief Pixels scrolled off the top of the row list. Clamped in DrawCraftingMenu. */
int ScrollOffset = 0;

} // namespace

bool IsCraftingMenuOpen()
{
	return MenuOpen;
}

void OpenCraftingMenu()
{
	MenuOpen = true;
	ScrollOffset = 0; // a window always opens at the top of its list
}

void CloseCraftingMenu()
{
	MenuOpen = false;
}

namespace {

/** @brief The recipes this window lists, in order. Grid-only ones are not among them. */
std::vector<int> BackpackRecipes()
{
	std::vector<int> rows;
	for (int i = 0; i < CraftingRecipeCount; i++) {
		if (!CraftingRecipeUsesGrid(i))
			rows.push_back(i);
	}
	return rows;
}

} // namespace

Rectangle GetCraftingMenuRect()
{
	// The runeword book's placement as well as its size: horizontally centred, top flush with the
	// mini-map's top border. Was a content-sized 340-wide box flush to the screen's top-left corner
	// until 2026-08-30.
	//
	// Note the rect is no longer derived from the recipe COUNT. It is a fixed window with a scrolled
	// list inside it, which is what makes the list able to outgrow the panel at all.
	const int top = GetMiniMapScreenRect().position.y;
	return Rectangle { { (gnScreenWidth - WindowSize.width) / 2, top }, WindowSize };
}

namespace {

/** @brief The scrolled row area: everything under the title, inside the padding. */
Rectangle CraftingContentRect()
{
	const Rectangle window = GetCraftingMenuRect();
	const int top = window.position.y + WindowPadding + HeaderHeight;
	return Rectangle { { window.position.x + WindowPadding, top },
		{ window.size.width - WindowPadding * 2,
		    window.position.y + window.size.height - WindowPadding - top } };
}

/** @brief How far the list may scroll, in pixels. Zero when every row already fits. */
int MaxScrollOffset()
{
	const int totalHeight = static_cast<int>(BackpackRecipes().size()) * RowHeight;
	return std::max(0, totalHeight - CraftingContentRect().size.height);
}

} // namespace

void DrawCraftingMenu(const Surface &out)
{
	if (!MenuOpen)
		return;

	const Rectangle window = GetCraftingMenuRect();
	DrawHalfTransparentRectTo(out, window.position.x, window.position.y, window.size.width, window.size.height);
	DrawOrnateBorder(out, window);
	DrawWindowCloseButton(out, window);

	DrawString(out, _("Crafting"),
	    Rectangle { window.position + Displacement { WindowPadding, WindowPadding },
	        { window.size.width - WindowPadding * 2, HeaderHeight } },
	    { UiFlags::ColorWhitegold | UiFlags::FontSize24 | UiFlags::AlignCenter });

	const Rectangle content = CraftingContentRect();
	ScrollOffset = std::clamp(ScrollOffset, 0, MaxScrollOffset());

	// Clipped to the content area, so a row straddling the bottom edge is cut there rather than
	// spilling over the border - the same reason the runeword book draws into a subregion.
	const Surface view = out.subregion(content.position.x, content.position.y,
	    content.size.width, content.size.height);

	// The monument's own recipes are not listed here. They transform an item in place and have no
	// backpack path at all, so CanCraft answers false for them forever - which is exactly what
	// "Free the Sockets" did in this window from the day it shipped: a permanently grey row with
	// nothing saying it lived on Levski's Roar instead.
	int top = -ScrollOffset;
	for (const int i : BackpackRecipes()) {
		if (top + RowHeight >= 0 && top <= content.size.height) {
			const bool craftable = CanCraft(*MyPlayer, i);
			// A craftable recipe reads live (gold name, white formula); one missing its materials is
			// dimmed but still listed - the recipes teaching themselves is half their value.
			DrawString(view, _(CraftingRecipeName(i)),
			    Rectangle { { 0, top }, { content.size.width, RowHeight / 2 } },
			    { (craftable ? UiFlags::ColorGold : UiFlags::ColorWhitegold) | UiFlags::FontSize12 });
			DrawString(view, _(CraftingRecipeInputs(i)),
			    Rectangle { { 0, top + RowHeight / 2 - 4 }, { content.size.width, RowHeight / 2 } },
			    { (craftable ? UiFlags::ColorWhite : UiFlags::ColorWhitegold) | UiFlags::FontSize12 });
		}
		top += RowHeight;
	}
}

void CheckCraftingMenuClick(Point mousePosition)
{
	const Rectangle window = GetCraftingMenuRect();
	if (GetWindowCloseButtonRect(window).contains(mousePosition)) {
		CloseCraftingMenu();
		return;
	}

	const Rectangle content = CraftingContentRect();
	if (!content.contains(mousePosition))
		return; // title bar and padding are absorbed by the window, not acted on

	// The scroll offset has to come back out of the hit test, or every click below the fold lands on
	// the recipe that USED to be at that height.
	const int row = (mousePosition.y - content.position.y + ScrollOffset) / RowHeight;
	const std::vector<int> rows = BackpackRecipes();
	if (row < 0 || row >= static_cast<int>(rows.size()))
		return;

	// The clicked ROW is not the recipe INDEX any more - the grid-only recipes are filtered out of
	// the list, so row 3 is whatever the fourth listed recipe happens to be. Mapping through the
	// same list the draw walked is what keeps a click landing on the row the player pointed at.
	const std::string crafted = Craft(*MyPlayer, rows[row]);
	if (crafted.empty())
		return; // missing materials or no room; the dimmed row already says which
	LogEvent(fmt::format("Crafted: {:s}", crafted));
}

bool HandleCraftingMenuScroll(int notches)
{
	if (!MenuOpen)
		return false;
	if (!GetCraftingMenuRect().contains(MousePosition))
		return false;
	// Consumed whether or not it moves: a wheel notch over the window must never reach the world
	// behind it, and at the top of a short list there is nothing to move.
	ScrollOffset = std::clamp(ScrollOffset - notches * RowHeight, 0, MaxScrollOffset());
	return true;
}

} // namespace devilution::oracool
