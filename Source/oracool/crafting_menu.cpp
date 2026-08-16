#include "oracool/crafting_menu.h"

#include <fmt/format.h>

#include "DiabloUI/ui_flags.hpp"
#include "control.h"
#include "engine/render/primitive_render.hpp"
#include "engine/render/text_render.hpp"
#include "player.h"
#include "oracool/crafting.h"
#include "oracool/event_log.h"
#include "oracool/ornate_border.h"
#include "utils/language.h"

namespace devilution::oracool {

namespace {

bool MenuOpen = false;

constexpr int WindowWidth = 340;
constexpr int RowHeight = 56;
constexpr int WindowPadding = 12;
constexpr int HeaderHeight = 28;

} // namespace

bool IsCraftingMenuOpen()
{
	return MenuOpen;
}

void OpenCraftingMenu()
{
	MenuOpen = true;
}

void CloseCraftingMenu()
{
	MenuOpen = false;
}

Rectangle GetCraftingMenuRect()
{
	// The left-panel slot, flush to the top-left like the waypoint list - sized to its content
	// rather than the full 720, since three recipes need no scroll.
	const int height = WindowPadding * 2 + HeaderHeight + CraftingRecipeCount * RowHeight;
	return Rectangle { { 0, 0 }, { WindowWidth, height } };
}

void DrawCraftingMenu(const Surface &out)
{
	if (!MenuOpen)
		return;

	const Rectangle window = GetCraftingMenuRect();
	DrawHalfTransparentRectTo(out, window.position.x, window.position.y, window.size.width, window.size.height);
	DrawOrnateBorder(out, window);

	Point cursor = window.position + Displacement { WindowPadding, WindowPadding };
	const int contentWidth = window.size.width - WindowPadding * 2;
	DrawString(out, _("Crafting"), Rectangle { cursor, { contentWidth, HeaderHeight } },
	    { UiFlags::ColorGold | UiFlags::FontSize24 });
	cursor.y += HeaderHeight;

	for (int i = 0; i < CraftingRecipeCount; i++) {
		const bool craftable = CanCraft(*MyPlayer, i);
		// A craftable recipe reads live (gold name, white formula); one missing its materials is
		// dimmed but still listed - the recipes teaching themselves is half their value.
		DrawString(out, _(CraftingRecipeName(i)),
		    Rectangle { cursor, { contentWidth, RowHeight / 2 } },
		    { (craftable ? UiFlags::ColorGold : UiFlags::ColorWhitegold) | UiFlags::FontSize12 });
		DrawString(out, _(CraftingRecipeInputs(i)),
		    Rectangle { { cursor.x, cursor.y + RowHeight / 2 - 4 }, { contentWidth, RowHeight / 2 } },
		    { (craftable ? UiFlags::ColorWhite : UiFlags::ColorWhitegold) | UiFlags::FontSize12 });
		cursor.y += RowHeight;
	}
}

void CheckCraftingMenuClick(Point mousePosition)
{
	const Rectangle window = GetCraftingMenuRect();
	const int rowsTop = window.position.y + WindowPadding + HeaderHeight;
	const int row = (mousePosition.y - rowsTop) / RowHeight;
	if (mousePosition.y < rowsTop || row < 0 || row >= CraftingRecipeCount)
		return; // header/padding clicks are absorbed by the window, not acted on

	const std::string crafted = Craft(*MyPlayer, row);
	if (crafted.empty())
		return; // missing materials or no room; the dimmed row already says which
	LogEvent(fmt::format("Crafted: {:s}", crafted));
}

} // namespace devilution::oracool
