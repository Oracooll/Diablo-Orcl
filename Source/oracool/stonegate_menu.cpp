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
#include "oracool/hud_art.h" // DrawLoosePng, GetLoosePngSize: the canvas
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

/**
 * THE USER'S OWN UI (Resources\Rift Monument UI, 2026-09-20: "Assemble Rift Monument UI with the assets
 * in this folder"): a 320x352 background with the title, the question and the hall painted in
 * (ui\riftmenu_bg.png), and four buttons delivered at ~970x150 / 585x120 and scaled by
 * tools/ScalePainting.ps1 to the proportions of the user's own assembled sample (1195x1316, x0.268):
 * the two rift buttons 262x40 at x 29, y 123 and 174; Leave 157x32 at x 82, y 224. The Guardian button
 * has two faces - purple with a keystone in the pack, RED "no keystone in the pack" without.
 *
 * Buttons (user, same day): "when hovering over the buttons make them a notch brighter. when clicking
 * on them sink them 2px down and left. hold until click released." - BrightenRectRgb on hover, the
 * face drawn at the cell shifted by (-2, +2) from the press to LeftMouseUp's ReleaseStonegateMenuButton.
 * And the sound the user gave every button today: titlemov.wav on every hover entry and click.
 *
 * Before this the menu drew on the shared artisan canvas with code-drawn plates (v1.12.069); that
 * remains as the fallback when the background file is missing.
 */
constexpr const char *BackgroundAsset = "ui\\riftmenu_bg.png";
constexpr const char *CanvasAsset = "ui\\artisan_canvas.png"; // the fallback's canvas
constexpr const char *NephalemAsset = "ui\\riftmenu_nephalem.png";
constexpr const char *GuardianAsset = "ui\\riftmenu_guardian.png";
constexpr const char *GuardianRedAsset = "ui\\riftmenu_guardian_red.png";
constexpr const char *LeaveAsset = "ui\\riftmenu_leave.png";
constexpr Size PanelSize { 320, 352 };
constexpr int Margin = 30;
constexpr int TitleTop = 34;
constexpr int TitleHeight = 26;
constexpr int RowsTop = 96;
constexpr int RowHeight = 34;
constexpr int RowGap = 12;
constexpr int RowCount = 3;
/** The Roar's own dark stone (levski_roar.cpp): the border's shadow tone, and the wells' near-black. */
constexpr uint8_t PanelFillColor = 204;
constexpr uint8_t SlotFillColor = 223;
constexpr Rectangle CloseRect { { 296, 5 }, { 18, 18 } };
/** @brief The painted buttons' cells, panel-relative, from the user's assembled sample. */
constexpr Rectangle ButtonCells[RowCount] = {
	{ { 29, 123 }, { 262, 40 } },
	{ { 29, 174 }, { 262, 41 } },
	{ { 82, 224 }, { 157, 32 } },
};
constexpr int HoverBrightenPercent = 115;
constexpr Displacement PressSink { -2, 2 };

/** @brief The button held down, or -1; and the one the cursor was over last frame, for the entry sound. */
int PressedRow = -1;
int LastHoveredRow = -1;

enum Row : int {
	Nephalem = 0,
	Guardian = 1,
	CloseGate = 2,
};

/** @brief Whether the painted set is in: the background decides; a missing button draws its fallback plate. */
bool HasPaintedUi()
{
	return GetLoosePngSize(BackgroundAsset).width > 0;
}

Rectangle PanelRect()
{
	const int x = std::max(0, (static_cast<int>(gnScreenWidth) - PanelSize.width) / 2);
	const int y = std::max(0, (static_cast<int>(gnScreenHeight) - PanelSize.height) / 2);
	return Rectangle { { x, y }, PanelSize };
}

Rectangle RowRect(const Rectangle &panel, int row)
{
	if (HasPaintedUi())
		return Rectangle { panel.position + Displacement { ButtonCells[row].position.x, ButtonCells[row].position.y }, ButtonCells[row].size };
	return Rectangle { { panel.position.x + Margin, panel.position.y + RowsTop + row * (RowHeight + RowGap) },
		{ panel.size.width - Margin * 2, RowHeight } };
}

Rectangle CloseButtonRect(const Rectangle &panel)
{
	return Rectangle { panel.position + Displacement { CloseRect.position.x, CloseRect.position.y }, CloseRect.size };
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
		// LEAVE closes this menu and nothing else (user, 2026-09-20: "Leave is meant to close the Rift
		// Monument UI"): a rift is never closed from here - a cleared Nephalem Rift closes itself a
		// minute after its guardian falls (NephalemRiftCloseSeconds), and nothing but a new game ends
		// one otherwise.
		return std::string(_("Leave"));
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
	PressedRow = -1;
	LastHoveredRow = -1;
}

void ReleaseStonegateMenuButton()
{
	PressedRow = -1;
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
	if (HasPaintedUi()) {
		// The painted set: the background carries the title and the question; the buttons are drawn
		// over it, brighter under the cursor, sunk while pressed. Hover-tested with the same rects the
		// click uses, and the entry sound on the frame the cursor arrives over a button.
		DrawLoosePng(out, BackgroundAsset, panel.position);
		DrawWindowCloseButtonAt(out, CloseButtonRect(panel));
		int hoveredNow = -1;
		for (int row = 0; row < RowCount; row++) {
			bool enabled = true;
			RowLabel(row, enabled);
			const Rectangle rect = RowRect(panel, row);
			const bool hovered = rect.contains(MousePosition);
			if (hovered)
				hoveredNow = row;
			const char *asset = row == Nephalem ? NephalemAsset : (row == Guardian ? (enabled ? GuardianAsset : GuardianRedAsset) : LeaveAsset);
			const Rectangle face { rect.position + (PressedRow == row ? PressSink : Displacement { 0, 0 }), rect.size };
			if (GetLoosePngSize(asset).width > 0) {
				DrawLoosePng(out, asset, face.position);
			} else {
				FillRect(out, face.position.x, face.position.y, face.size.width, face.size.height, SlotFillColor);
				DrawString(out, RowLabel(row, enabled), face,
				    { (enabled ? UiFlags::ColorWhitegold : UiFlags::ColorRed) | UiFlags::FontSize12 | UiFlags::VerticalCenter | UiFlags::AlignCenter });
			}
			if (hovered)
				BrightenRectRgb(out, face.position.x, face.position.y, face.size.width, face.size.height, HoverBrightenPercent);
		}
		if (hoveredNow >= 0 && hoveredNow != LastHoveredRow)
			PlayUiMoveSound();
		LastHoveredRow = hoveredNow;
		return;
	}
	if (GetLoosePngSize(CanvasAsset).width > 0) {
		DrawLoosePng(out, CanvasAsset, panel.position);
	} else {
		FillRect(out, panel.position.x, panel.position.y, panel.size.width, panel.size.height, PanelFillColor);
		DrawOrnateBorder(out, panel);
	}
	DrawWindowCloseButtonAt(out, CloseButtonRect(panel));

	DrawString(out, _("Rift Monument"), Rectangle { { panel.position.x + 22, panel.position.y + TitleTop }, { 276, TitleHeight } },
	    { UiFlags::ColorGold | UiFlags::FontSize24 | UiFlags::AlignCenter | UiFlags::Shadowed });
	DrawString(out, _("Which portal shall it open?"), Rectangle { { panel.position.x + 22, panel.position.y + TitleTop + TitleHeight + 4 }, { 276, 16 } },
	    { UiFlags::ColorWhitegold | UiFlags::FontSize12 | UiFlags::AlignCenter });

	for (int row = 0; row < RowCount; row++) {
		bool enabled = true;
		const std::string label = RowLabel(row, enabled);
		const Rectangle rect = RowRect(panel, row);
		const bool hovered = enabled && rect.contains(MousePosition);
		// The plate, drawn in code as a placeholder for art: a stone edge around a dark well.
		FillRect(out, rect.position.x - 1, rect.position.y - 1, rect.size.width + 2, rect.size.height + 2, PanelFillColor);
		FillRect(out, rect.position.x, rect.position.y, rect.size.width, rect.size.height, SlotFillColor);
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
	if (CloseButtonRect(panel).contains(mousePosition)) {
		CloseStonegateMenu();
		PlayUiMoveSound();
		return true;
	}
	Player &player = *MyPlayer;
	for (int row = 0; row < RowCount; row++) {
		if (!RowRect(panel, row).contains(mousePosition))
			continue;
		// The press: the face sinks until the release and the click sounds, whatever it then does.
		PressedRow = row;
		PlayUiMoveSound();
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
			break; // Leave: the menu is already closed above, the rift - if any - stands
		}
		return true;
	}
	return true; // the panel's padding absorbs the click
}

} // namespace devilution::oracool
