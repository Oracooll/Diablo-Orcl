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
#include "oracool/shop_grid.h" // DrawVendorButtonBacking - the book buttons wear the vendors' tab face
#include "oracool/ui_sound.h"
#include "utils/language.h"
#include "utils/ui_fwd.h"
#include "stores.h" // ForceCloseStore

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

/**
 * THE THREE BOOKS (user, 2026-09-22: "it should have three button to show recipes of the three
 * crafting methods - cube, gilian, ogden. make sure recipes are put in proper button").
 *
 * This window listed all twenty-eight at once under a line saying every one of them was crafted at
 * Levski's Roar. That line stopped being true when the recipes were split across three artisans on
 * 2026-09-20, and a flat list of twenty-eight said nothing about where any of them could be run.
 *
 * THREE covers every recipe, and that is checked rather than assumed: HostOfRecipe returns Cube,
 * Tavern or Barmaid and never Smith - his gear recipes moved to the Cube when his window became the
 * Salvage page - so no recipe is left without a button to appear under.
 */
constexpr TransmuteHost HostButtons[] = { TransmuteHost::Cube, TransmuteHost::Tavern, TransmuteHost::Barmaid };
constexpr int HostButtonCount = static_cast<int>(sizeof(HostButtons) / sizeof(HostButtons[0]));
constexpr int HostButtonWidth = 200;
constexpr int HostButtonHeight = 26;
constexpr int HostButtonGap = 24;
constexpr int HostButtonGapBelow = 6;

/** @brief Which book is open. The Cube's, until the player picks another. */
TransmuteHost HostFilter = TransmuteHost::Cube;

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
	// And a shop outright (audit, 2026-09-27): CloseAllWindows only steps a shop tab back to its vendor's dialog, which is
	// modal - it drew under this window and took every click meant for it.
	devilution::ForceCloseStore();
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
	for (int i = 0; i < CraftingRecipeCount; i++) {
		// The open book's own, since 2026-09-22 - the filter this comment once argued against, put
		// back in the right place. The old filter hid recipes with nothing saying where they lived;
		// this one is a button the player pressed, and the other two books are one click away.
		if (RecipeBelongsTo(i, HostFilter))
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

/** @brief Book button @p index, in the row under the title. */
Rectangle HostButtonRect(int index)
{
	const Rectangle window = GetCraftingMenuRect();
	const int span = HostButtonCount * HostButtonWidth + (HostButtonCount - 1) * HostButtonGap;
	const int left = window.position.x + (window.size.width - span) / 2;
	return Rectangle { { left + index * (HostButtonWidth + HostButtonGap),
	                       window.position.y + WindowPadding + HeaderHeight },
		{ HostButtonWidth, HostButtonHeight } };
}

/** @brief The scrolled row area: everything under the title, the book row and the subtitle. */
Rectangle CraftingContentRect()
{
	const Rectangle window = GetCraftingMenuRect();
	const int top = window.position.y + WindowPadding + HeaderHeight
	    + HostButtonHeight + HostButtonGapBelow + SubtitleHeight;
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

	// THE THREE BOOKS. The open one is filled and gold-edged, the others sit back - the same "this is
	// the page you are on" the vendors' tabs use.
	for (int i = 0; i < HostButtonCount; i++) {
		const Rectangle rect = HostButtonRect(i);
		const bool active = HostButtons[i] == HostFilter;
		const bool hovered = rect.contains(MousePosition);
		// The vendors' tab button (user, 2026-09-25 dev note: "crafting book - use vendors tab buttons backing
		// for the three buttons here. selected crafting sheet to have its button gold, rest - grey ... texts on
		// buttons to have text shadow 2px"). The filled plate and ornate edge stay as the fallback for a
		// player whose archive has no vanilla button.
		const bool vendorFace = DrawVendorButtonBacking(out, rect, active, hovered);
		if (!vendorFace) {
			if (active)
				FillRect(out, rect.position.x + 1, rect.position.y + 1, rect.size.width - 2, rect.size.height - 2, PAL16_GRAY + 14);
			DrawOrnateBorder(out, rect);
		}
		DrawString(out, _(TransmuteHostTitle(HostButtons[i])), rect,
		    { (active ? UiFlags::ColorWhite : UiFlags::ColorWhitegold) | UiFlags::FontSize12
		        | UiFlags::AlignCenter | UiFlags::VerticalCenter | UiFlags::Shadowed });
		if (hovered && !active && !vendorFace)
			DrawHoverOutline(out, rect);
	}

	// Said once, under the books, rather than once per row.
	//
	// It used to say every recipe was crafted at Levski's Roar, which stopped being true when the
	// recipes were split across three artisans on 2026-09-20 - the window went on saying it for two
	// days. It names the OPEN book now, so it cannot fall out of step with what is listed beneath it.
	DrawString(out, fmt::format(fmt::runtime(_("Crafted at {:s}.")), _(TransmuteHostTitle(HostFilter))),
	    Rectangle { window.position + Displacement { WindowPadding, WindowPadding + HeaderHeight + HostButtonHeight + HostButtonGapBelow },
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
	// The books first: they sit above the content area, so the early return below would swallow them.
	for (int i = 0; i < HostButtonCount; i++) {
		if (!HostButtonRect(i).contains(mousePosition))
			continue;
		if (HostButtons[i] != HostFilter) {
			HostFilter = HostButtons[i];
			ScrollOffset = 0; // a book always opens at the top of its own list
		}
		PlayUiSelectSound();
		return;
	}

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
	// Names the recipe's OWN host, which since 2026-09-20 is not always the monument. It said
	// "Levski's Roar" for every row of every book, which was wrong for eleven of the twenty-eight -
	// and asking HostOfRecipe rather than the open filter means it stays right even if a recipe is
	// ever moved between books.
	PlayUiMoveSound();
	LogEvent(fmt::format(fmt::runtime(_("{:s} is crafted at {:s}")),
	    _(CraftingRecipeName(rows[row])), _(TransmuteHostTitle(HostOfRecipe(rows[row])))));
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
