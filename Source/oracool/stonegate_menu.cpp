#include "oracool/stonegate_menu.h"

#include <algorithm>
#include <string>

#include <fmt/format.h>

#include "DiabloUI/ui_flags.hpp"
#include "control.h"
#include "diablo.h" // PauseMode
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
#include "plrmsg.h" // EventPlrMsg
#include "utils/language.h"

namespace devilution::oracool {

namespace {

bool MenuOpen = false;

/**
 * THE USER'S OWN UI (Resources\02. Oracooll Assets\Rift Monument UI, 2026-09-20: "Assemble Rift Monument UI with the assets
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
/** @brief The button the keyboard has chosen (Up/Down), or -1; lit as the mouse's hover is (2026-09-27). */
int KeyRow = -1;

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
		const Item *keystone = FindBestKeystoneInBackpack(player);
		if (keystone == nullptr) {
			enabled = false;
			return std::string(_("Open a Guardian Rift  (no keystone in the pack)"));
		}
		return fmt::format(fmt::runtime(_("Open a Guardian Rift  (keystone, tier {:d})")), keystone->_iOracoolRiftTier);
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
	KeyRow = -1;
	PlayUiSelectSound();
}

void CloseStonegateMenu()
{
	MenuOpen = false;
	PressedRow = -1;
	LastHoveredRow = -1;
	KeyRow = -1;
}

/**
 * @brief LeftMouseUp: the pressed button acts, and ONLY if the release lands inside the button that was pressed
 * (user, 2026-09-21: "apply to Rift Monument menu same click mechanic" - Griswold's CONFIRM / CANCEL rule).
 *
 * A release anywhere else is the user thinking a bit more: the face springs back, nothing runs, the menu stands.
 */
void ReleaseStonegateMenuButton()
{
	const int row = PressedRow;
	PressedRow = -1;
	if (row < 0 || !MenuOpen || MyPlayer == nullptr || PauseMode == 2) // a release after Pause (round 18 audit)
		return;
	const Rectangle panel = PanelRect();
	if (!RowRect(panel, row).contains(MousePosition))
		return; // released off the button: no rift opens and the menu is still up
	ActivateStonegateRow(row);
}

void ActivateStonegateRow(int row)
{
	if (row < 0 || row >= RowCount || !MenuOpen || MyPlayer == nullptr)
		return;
	bool enabled = true;
	RowLabel(row, enabled);
	if (!enabled) {
		LogEvent("A Guardian Rift needs a Guardian Keystone - a Nephalem Rift's guardian drops one.", UiFlags::ColorRed);
		EventPlrMsg("A Guardian Rift needs a Guardian Keystone - a Nephalem Rift's guardian drops one.", UiFlags::ColorRed); // on screen (round 19)
		return;
	}
	Player &player = *MyPlayer;
	CloseStonegateMenu();
	switch (row) {
	case Nephalem:
		OpenNephalemAtGate();
		break;
	case Guardian:
		// Refused only by a rift still open, and EnteredRiftStillOpen has said which: a second, vaguer line followed it
		// (round 18 audit).
		UseBestKeystoneFromBackpack(player);
		break;
	default:
		break; // Leave: the menu is already closed above, the rift - if any - stands
	}
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
			const bool hovered = rect.contains(MousePosition) || row == KeyRow;
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
		const bool hovered = enabled && (rect.contains(MousePosition) || row == KeyRow);
		// The plate, drawn in code as a placeholder for art: a stone edge around a dark well.
		FillRect(out, rect.position.x - 1, rect.position.y - 1, rect.size.width + 2, rect.size.height + 2, PanelFillColor);
		FillRect(out, rect.position.x, rect.position.y, rect.size.width, rect.size.height, SlotFillColor);
		DrawString(out, label, rect,
		    { (enabled ? (hovered ? UiFlags::ColorWhite : UiFlags::ColorWhitegold) : UiFlags::ColorRed) | UiFlags::FontSize12 | UiFlags::VerticalCenter | UiFlags::AlignCenter });
		if (hovered)
			DrawHoverOutline(out, rect);
	}
}

bool HandleStonegateMenuKey(SDL_Keycode key)
{
	if (!MenuOpen || MyPlayer == nullptr)
		return false;
	// The keyboard reaches every choice (user, 2026-09-27: "fix the decisions for me too" - the front-end rule that every
	// window is keyboard-reachable): Up and Down choose, Enter presses, 1-3 press a button directly. Escape is
	// PressEscKey's, as for every window.
	switch (key) {
	case SDLK_UP:
		KeyRow = KeyRow <= 0 ? RowCount - 1 : KeyRow - 1;
		PlayUiMoveSound();
		return true;
	case SDLK_DOWN:
		KeyRow = KeyRow < 0 || KeyRow >= RowCount - 1 ? 0 : KeyRow + 1;
		PlayUiMoveSound();
		return true;
	case SDLK_RETURN:
	case SDLK_KP_ENTER:
		if (KeyRow < 0)
			return true;
		ActivateStonegateRow(KeyRow);
		return true;
	case SDLK_1:
	case SDLK_2:
	case SDLK_3:
		ActivateStonegateRow(static_cast<int>(key - SDLK_1));
		return true;
	default:
		return false;
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
	for (int row = 0; row < RowCount; row++) {
		if (!RowRect(panel, row).contains(mousePosition))
			continue;
		// The press SINKS the face and sounds, and does nothing else: ReleaseStonegateMenuButton decides, and only
		// when the release lands inside this same button (2026-09-21).
		PressedRow = row;
		PlayUiMoveSound();
		return true;
	}
	return true; // the panel's padding absorbs the click
}

} // namespace devilution::oracool
