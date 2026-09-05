#include "oracool/crafting_menu.h"

#include "oracool/book_frame.h" // the painted wide frame

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
#include "diablo.h" // CloseAllWindows
#include "oracool/event_log.h"
#include "oracool/hud_menu.h"
#include "oracool/ornate_border.h"
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
constexpr int WindowPadding = 30; // the painted frame's bezel is 21-24 deep on every side (book_frame.cpp); was 12 inside the drawn border
constexpr int HeaderHeight = 28;
/** @brief The line under the title saying where crafting happens. Reserved whether or not it wraps. */
constexpr int SubtitleHeight = 20;

/** @brief Pixels scrolled off the top of the row list. Clamped in DrawCraftingMenu. */
int ScrollOffset = 0;

} // namespace

bool IsCraftingMenuOpen()
{
	return MenuOpen;
}

void OpenCraftingMenu()
{
	// This window is the runeword book's twin - 944x616 on a 960 screen, centred, top flush with the
	// mini-map - and that geometry is the whole argument: it does not share the screen with anything,
	// it IS the screen while it is up. It kept the opener it had as a 340-wide side panel until
	// v1.9.146, which closed its four left-panel siblings and nothing else, so the inventory, the
	// spellbook and the event log stayed up underneath it (audit, 2026-08-31).
	//
	// CloseAllWindows is the master closer the fork's own rule points every new window at, so this
	// cannot fall behind as windows are added. Called BEFORE MenuOpen goes true, because it closes
	// this window too.
	devilution::CloseAllWindows();
	// The burger row is deliberately not in CloseAllWindows - space leaves it up so several panels
	// can be toggled in one go. That argument does not survive this rect either: the row sits just
	// above the HUD plate, this window reaches into that strip, and diablo.cpp routes
	// IsPointOverHudMenu BEFORE the crafting click handler - so the row would draw over the book and
	// eat every click in its bottom strip. The runeword book closes it for the same reason.
	CloseHudMenu();
	MenuOpen = true;
	ScrollOffset = 0; // a window always opens at the top of its list
}

void CloseCraftingMenu()
{
	MenuOpen = false;
}

namespace {

/**
 * @brief The recipes this window lists, in order - ALL of them, monument-only ones included.
 *
 * Was a filtered list until v1.9.140, and that filter is what made the game hold two recipe lists
 * instead of one: the monument's book showed seventeen, this one showed three, and a player who
 * only ever opened the belt's burger menu had no way to learn the other fourteen existed. The
 * filter solved a real problem in the wrong place - "Free the Sockets" sat here permanently grey
 * with nothing saying where it lived - and the fix is to SAY where it lives, per row, rather than
 * to hide it.
 */
std::vector<int> ListedRecipes()
{
	std::vector<int> rows;
	rows.reserve(CraftingRecipeCount);
	for (int i = 0; i < CraftingRecipeCount; i++)
		rows.push_back(i);
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
	const int top = window.position.y + WindowPadding + HeaderHeight + SubtitleHeight;
	return Rectangle { { window.position.x + WindowPadding, top },
		{ window.size.width - WindowPadding * 2,
		    window.position.y + window.size.height - WindowPadding - top } };
}

/** @brief How far the list may scroll, in pixels. Zero when every row already fits. */
int MaxScrollOffset()
{
	const int totalHeight = static_cast<int>(ListedRecipes().size()) * RowHeight;
	return std::max(0, totalHeight - CraftingContentRect().size.height);
}

} // namespace

void DrawCraftingMenu(const Surface &out)
{
	if (!MenuOpen)
		return;

	const Rectangle window = GetCraftingMenuRect();
	// The painted wide frame (user, 2026-09-05), shared with the Runeword book: dark backing in
	// its core, the bezel over it.
	DrawBookFrame(out, BookFrame::Wide, window);
	// No close button drawn here. This is a left-panel content, and scrollrt draws the X for
	// whichever content is open from GetLeftPanelContentRect() - which IS this rect. Drawing one
	// here too stacked a second X exactly on top of the first, which is the failure the central
	// call exists to prevent.

	DrawString(out, _("Crafting"),
	    Rectangle { window.position + Displacement { WindowPadding, WindowPadding },
	        { window.size.width - WindowPadding * 2, HeaderHeight } },
	    { UiFlags::ColorWhitegold | UiFlags::FontSize24 | UiFlags::AlignCenter });

	// Said once, under the title, rather than once per row. It was a per-row column for one version;
	// with the monument the only place a recipe can produce anything, every row said the same three
	// words and the column was seventeen repetitions of a single fact.
	DrawString(out, _("Every recipe is crafted at Levski's Roar, the monument in town."),
	    Rectangle { window.position + Displacement { WindowPadding, WindowPadding + HeaderHeight },
	        { window.size.width - WindowPadding * 2, SubtitleHeight } },
	    { UiFlags::ColorWhitegold | UiFlags::FontSize12 | UiFlags::AlignCenter });

	const Rectangle content = CraftingContentRect();
	ScrollOffset = std::clamp(ScrollOffset, 0, MaxScrollOffset());

	// Clipped to the content area, so a row straddling the bottom edge is cut there rather than
	// spilling over the border - the same reason the runeword book draws into a subregion.
	const Surface view = out.subregion(content.position.x, content.position.y,
	    content.size.width, content.size.height);

	// Every recipe is listed, and every row reads the same. There is no live/dim split any more:
	// this window cannot craft, so "you have the materials" would be a promise it has no way to
	// keep - and the materials that matter are the ones you carry TO the monument, not the ones the
	// backpack happens to hold while you read.
	int top = -ScrollOffset;
	for (const int i : ListedRecipes()) {
		if (top + RowHeight >= 0 && top <= content.size.height) {
			DrawString(view, _(CraftingRecipeName(i)),
			    Rectangle { { 0, top }, { content.size.width, RowHeight / 2 } },
			    { UiFlags::ColorGold | UiFlags::FontSize12 });
			DrawString(view, _(CraftingRecipeInputs(i)),
			    Rectangle { { 0, top + RowHeight / 2 - 4 }, { content.size.width, RowHeight / 2 } },
			    { UiFlags::ColorWhite | UiFlags::FontSize12 });
		}
		top += RowHeight;
	}
}

void CheckCraftingMenuClick(Point mousePosition)
{
	// The close button is not tested here either: diablo.cpp's LeftMouseDown asks
	// CheckWindowCloseButtonClick(GetLeftPanelContentRect()) before it reaches this router, so a
	// click on the X never arrives. A second test here was dead code that read like the live one.
	const Rectangle content = CraftingContentRect();
	if (!content.contains(mousePosition))
		return; // title bar and padding are absorbed by the window, not acted on

	// The scroll offset has to come back out of the hit test, or every click below the fold lands on
	// the recipe that USED to be at that height.
	const int row = (mousePosition.y - content.position.y + ScrollOffset) / RowHeight;
	const std::vector<int> rows = ListedRecipes();
	if (row < 0 || row >= static_cast<int>(rows.size()))
		return;

	// A row is a thing to read, not a button. Nothing here can produce an item - the monument is the
	// only place that can (user, 2026-08-31) - so the click is absorbed and named in the log, which
	// is the difference between a window that ignores you and a window that has told you where to go.
	LogEvent(fmt::format(fmt::runtime(_("{:s} is crafted at Levski's Roar, the monument in town")),
	    _(CraftingRecipeName(rows[row]))));
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
