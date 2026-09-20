#include "oracool/stonegate_menu.h"

#include <algorithm>
#include <string>

#include <fmt/format.h>

#include "DiabloUI/ui_flags.hpp"
#include "control.h"
#include "engine/render/primitive_render.hpp"
#include "engine/render/text_render.hpp"
#include "levels/gendung.h"
#include "oracool/event_log.h"
#include "oracool/ornate_border.h"
#include "oracool/rift.h"
#include "oracool/stonegate.h"
#include "oracool/ui_sound.h"
#include "oracool/window_close.h"
#include "player.h"
#include "utils/language.h"

namespace devilution::oracool {

namespace {

bool MenuOpen = false;

constexpr Size PanelSize { 330, 158 };
constexpr int Margin = 18;
constexpr int TitleHeight = 26;
constexpr int RowHeight = 30;
constexpr int RowGap = 4;
constexpr int RowCount = 3;
/** The Roar's own dark stone (levski_roar.cpp): the border's shadow tone, opaque. */
constexpr uint8_t PanelFillColor = 204;

enum Row : int {
	Nephalem = 0,
	Guardian = 1,
	CloseGate = 2,
};

Rectangle PanelRect()
{
	const int x = std::max(0, (static_cast<int>(gnScreenWidth) - PanelSize.width) / 2);
	const int y = std::max(0, (static_cast<int>(gnScreenHeight) - PanelSize.height) / 2);
	return Rectangle { { x, y }, PanelSize };
}

Rectangle RowRect(const Rectangle &panel, int row)
{
	return Rectangle { { panel.position.x + Margin, panel.position.y + Margin + TitleHeight + row * (RowHeight + RowGap) },
		{ panel.size.width - Margin * 2, RowHeight } };
}

/** @brief The line each row shows, and whether it can be picked right now. */
std::string RowLabel(int row, bool &enabled)
{
	enabled = true;
	const Player &player = *MyPlayer;
	switch (row) {
	case Nephalem:
		return fmt::format(fmt::runtime(_("Open a Nephalem Rift  (free, tier {:d})")), NephalemRiftTierFor(player));
	case Guardian: {
		const int keystone = FindBestKeystoneInBackpack(player);
		if (keystone < 0) {
			enabled = false;
			return std::string(_("Open a Guardian Rift  (no keystone in the pack)"));
		}
		return fmt::format(fmt::runtime(_("Open a Guardian Rift  (keystone, tier {:d})")), player.InvList[keystone]._iOracoolRiftTier);
	}
	default:
		return std::string(ActiveRift() != RiftKind::None ? _("Close the gate - the rift ends") : _("Leave the gate dark"));
	}
}

} // namespace

bool IsStonegateMenuOpen()
{
	return MenuOpen;
}

void OpenStonegateMenu()
{
	if (MyPlayer == nullptr)
		return;
	MenuOpen = true;
	PlayUiSelectSound();
}

void CloseStonegateMenu()
{
	MenuOpen = false;
}

Rectangle GetStonegateMenuRect()
{
	return MenuOpen ? PanelRect() : Rectangle { { 0, 0 }, { 0, 0 } };
}

void DrawStonegateMenu(const Surface &out)
{
	if (!MenuOpen || MyPlayer == nullptr)
		return;
	const Rectangle panel = PanelRect();
	FillRect(out, panel.position.x, panel.position.y, panel.size.width, panel.size.height, PanelFillColor);
	DrawOrnateBorder(out, panel);
	DrawWindowCloseButton(out, panel);

	DrawString(out, _("The Stonegate"), Rectangle { { panel.position.x + Margin, panel.position.y + Margin }, { panel.size.width - Margin * 2, TitleHeight } },
	    { UiFlags::ColorGold | UiFlags::FontSize24 | UiFlags::AlignCenter | UiFlags::Shadowed });

	for (int row = 0; row < RowCount; row++) {
		bool enabled = true;
		const std::string label = RowLabel(row, enabled);
		const Rectangle rect = RowRect(panel, row);
		const bool hovered = enabled && rect.contains(MousePosition);
		DrawString(out, label, rect,
		    { (enabled ? (hovered ? UiFlags::ColorWhite : UiFlags::ColorWhitegold) : UiFlags::ColorRed) | UiFlags::FontSize12 | UiFlags::VerticalCenter | UiFlags::AlignCenter });
		if (hovered)
			DrawHoverOutline(out, rect);
	}
}

bool CheckStonegateMenuClick(Point mousePosition)
{
	if (!MenuOpen || MyPlayer == nullptr)
		return false;
	const Rectangle panel = PanelRect();
	if (!panel.contains(mousePosition)) {
		// A click away closes the menu, and is spent on that - never on the world behind it.
		CloseStonegateMenu();
		PlayUiMoveSound();
		return true;
	}
	if (CheckWindowCloseButtonClick(panel, mousePosition)) {
		CloseStonegateMenu();
		return true;
	}
	Player &player = *MyPlayer;
	for (int row = 0; row < RowCount; row++) {
		if (!RowRect(panel, row).contains(mousePosition))
			continue;
		bool enabled = true;
		RowLabel(row, enabled);
		if (!enabled) {
			LogEvent("A Guardian Rift needs a Guardian Keystone - a Nephalem Rift's guardian drops one.", UiFlags::ColorRed);
			return true;
		}
		CloseStonegateMenu();
		switch (row) {
		case Nephalem:
			OpenNephalemAtGate();
			break;
		case Guardian:
			if (!UseBestKeystoneFromBackpack(player))
				LogEvent("The keystone would not turn here.", UiFlags::ColorRed);
			break;
		default:
			if (ActiveRift() != RiftKind::None) {
				CloseStonegate();
				LogEvent("The Stonegate falls dark; the rift is gone.", UiFlags::ColorWhitegold);
			}
			break;
		}
		return true;
	}
	return true; // the panel's padding absorbs the click
}

} // namespace devilution::oracool
