#include "oracool/hud_menu.h"

#include <algorithm> // std::max, for keeping the icon row clear of the level-up indicator
#include <array>
#include <cstdint>

#include <SDL.h>

#include "DiabloUI/ui_flags.hpp"
#include "automap.h"
#include "control.h"
#include "engine/backbuffer_state.hpp"
#include "engine/palette.h"
#include "engine/rectangle.hpp"
#include "engine/render/primitive_render.hpp"
#include "engine/render/text_render.hpp"
#include "error.h"
#include "gamemenu.h"
#include "inv.h"
#include "levels/gendung.h"
#include "minitext.h"
#include "effects.h" // stream_stop - the quest narration goes with its text
#include "msg.h"
#include "oracool/crafting_menu.h"
#include "stores.h" // stextflag - no sheet over a store dialog
#include "oracool/event_log.h"
#include "oracool/hud_art.h"
#include "oracool/hud_layout.h"
#include "oracool/ornate_border.h"
#include "oracool/window_close.h"
#include "oracool/waypoint_menu.h"
#include "oracool/xp_counter.h" // GetXpCounterDrawRect - the icon row hangs above it
#include "oracool/oracool.h"
#include "oracool/run_toggle.h" // ToggleRun - the belt's seventh cell
#include "oracool/runeword_book.h"
#include "oracool/ui_sound.h"
#include "panels/spell_book.hpp"
#include "panels/spell_icons.hpp"
#include "player.h"
#include "quests.h"
#include "qol/stash.h"
#include "spelldat.h"
#include "utils/language.h"

namespace devilution::oracool {

namespace {

struct HudMenuEntry {
	const char *label;
	void (*action)();
	/** @brief Optional: whether whatever this entry opens is showing right now, which lights the
	 * icon. Null for momentary actions (they flash briefly on click instead). */
	bool (*isOn)();
};

// Oracool: straight relocation of control.cpp's old CheckBtnUp switch bodies (see that function's
// history in git blame for the original 8), plus 2 new mini-map entries per the approved HUD
// design. gamemenu_off() is called after every entry except "Game Menu" itself, matching the old
// gamemenuOff bookkeeping CheckBtnUp used to do.
// Not over a store dialog, as the C and Q keys refuse there: the sheet drew over the shop panel while the hidden shop
// took its clicks - a right click bought the item under it (round 25 audit, v1.12.250).
void DoCharacter()
{
	if (stextflag != TalkID::None)
		return;
	ToggleCharPanel();
}
void DoQuests()
{
	if (stextflag != TalkID::None)
		return;
	CloseCharPanel();
	CloseGoldWithdraw();
	CloseStash();
	if (!QuestLogIsOpen)
		StartQuestlog();
	else
		QuestLogIsOpen = false;
}
// Oracool: user request (2026-08-20) - "replace the automap button in the burger menu with RWBook.
// keep the icon, just replace the function and the pop-up tooltip." The map icon stays; only the
// action and the label move.
//
// The automap loses nothing: TAB still opens it, and it was never reachable ONLY from here.
void DoRunewordBookEntry() { ToggleRunewordBook(); }
void DoGameMenu()
{
	// The narration with the text: hidden here without stream_stop, the voice read on under the paused menu (round 32 audit).
	if (qtextflag) {
		qtextflag = false;
		stream_stop();
	}
	gamemenu_handle_previous();
}
void DoInventory()
{
	// The full-screen books close first, as the I key closes them (round 25 audit).
	CloseCraftingMenu();
	CloseRunewordBook();
	sbookflag = false;
	CloseGoldWithdraw();
	CloseStash();
	invflag = !invflag;
	if (DropGoldFlag)
		CloseGoldDrop();
}
// Was a duplicate of ToggleAbilitiesWindow's body; folded into it so the menu entry and the HUD's
// skill buttons cannot disagree about what gets closed on the way.
void DoSpellbook() { ToggleAbilitiesWindow(); }
void DoEventLog() { ToggleEventLog(); }

bool IsCharacterOpen() { return chrflag; }
bool IsQuestsOpen() { return QuestLogIsOpen; }
bool IsInventoryOpen() { return invflag; }
bool IsSpellbookOpen() { return sbookflag; }
bool IsLogOpen() { return IsEventLogOpen(); }

// Oracool: Megaplan Phase 1 - the reserved "Skill Book" slot becomes the Crafting window (the
// book icon reads perfectly as a recipe book). Mirrors DoQuests' close-the-siblings pattern so
// the left panel slot swaps cleanly rather than stacking.
void DoCrafting()
{
	if (IsCraftingMenuOpen()) {
		CloseCraftingMenu();
		return;
	}
	// The four hand-written sibling closes that used to sit here are gone (v1.9.146):
	// OpenCraftingMenu now calls CloseAllWindows itself, which is a superset of them and cannot fall
	// behind as windows are added. Listing a subset here as well was the shape that let the window
	// grow to 944 wide while its opener still only knew about the left panel.
	OpenCraftingMenu();
}
bool IsCraftingOpen() { return IsCraftingMenuOpen(); }

// Oracool: order must match the rows of assets/ui/menu_icons.png exactly - the sprite sheet was
// packed in this order (see scratchpad IconPack.cs). Chat and Friendly Fire were dropped from the
// menu earlier and their art is simply left unused on the artist's sheet.
constexpr std::array<HudMenuEntry, MenuIconCount> MenuEntries { {
    { "Character", DoCharacter, IsCharacterOpen },
    { "Quests", DoQuests, IsQuestsOpen },
    { "Runeword Book", DoRunewordBookEntry, IsRunewordBookOpen },
    { "Game Menu", DoGameMenu, nullptr },
    { "Inventory", DoInventory, IsInventoryOpen },
    { "Spellbook", DoSpellbook, IsSpellbookOpen },
    { "Crafting", DoCrafting, IsCraftingOpen },
    // Replaces the standalone "LOG" button that used to sit under the mini-map (event_log.cpp).
    { "Event Log", DoEventLog, IsLogOpen },
    // Oracool: user request (2026-08-11) - the three mini-map entries (Recenter, Zoom In, Zoom
    // Out) were removed from this row. Only the buttons are gone: RecenterMiniMap(),
    // MiniMapZoomIn() and MiniMapZoomOut() all stay bound to their keys, which is how they were
    // reachable before this menu existed at all.
    //
    // They were the last three entries, so dropping them leaves rows 0-7 of menu_icons.png
    // aligned with the entries above and nothing needs renumbering. The row re-centres by itself
    // because IconRowRect() derives its width from MenuIconCount.
} };

// THE MENU WINDOW (user, 2026-09-06: "i want burger menu to popup a window same as skill pickers.
// width equal belt width. bottom flush with top border of lmb/rmb slots. all icons in it to
// utilize 37x38px backing"). It replaced a bare row of framed icons hung above the XP counter; that
// row's geometry (IconRowRect, the level-up clearance, RowGapAboveCounter) is in the history at
// v1.9.289.
//
// The window is the skill picker's: the dark translucent backing, the ornate border, the red X, a
// title band, and cells of the vanilla 37x38 plate at native size. Its width is the belt's - from
// the first cell's left edge to the last cell's right edge - and its bottom sits ON the plate's top
// edge, which is the top border of the LMB/RMB wells.
constexpr Size MenuPlateSize { 37, 38 }; // the vanilla small spell plate, GetSkillIconPlateSize
constexpr int MenuCellGap = 6;
constexpr int MenuPadding = 14;
constexpr int MenuTitleHeight = 18;

bool HudMenuOpen = false;

/** @brief The belt's span: first cell's left edge to last cell's right edge. */
int BeltSpanLeft() { return GetBeltCellRect(0).position.x; }
int BeltSpanWidth()
{
	const Rectangle last = GetBeltCellRect(BeltVisibleSlotCount - 1);
	return last.position.x + last.size.width - BeltSpanLeft();
}

/** @brief How many plates fit across the window, and how many rows that makes of the entries. */
int MenuColumns()
{
	return std::max(1, (BeltSpanWidth() - 2 * MenuPadding + MenuCellGap) / (MenuPlateSize.width + MenuCellGap));
}
int MenuRows() { return (MenuIconCount + MenuColumns() - 1) / MenuColumns(); }

Rectangle MenuWindowRect()
{
	const int width = BeltSpanWidth();
	const int height = 2 * MenuPadding + MenuTitleHeight
	    + MenuRows() * MenuPlateSize.height + (MenuRows() - 1) * MenuCellGap;
	// Flush: the window's bottom edge IS the plate's top edge.
	const int bottom = GetMiddleHudRect().position.y;
	return { { BeltSpanLeft(), bottom - height }, { width, height } };
}

Rectangle IconRect(int index)
{
	const Rectangle window = MenuWindowRect();
	const int columns = MenuColumns();
	const int gridWidth = columns * MenuPlateSize.width + (columns - 1) * MenuCellGap;
	const int left = window.position.x + (window.size.width - gridWidth) / 2;
	const int top = window.position.y + MenuPadding + MenuTitleHeight;
	return { { left + (index % columns) * (MenuPlateSize.width + MenuCellGap),
		         top + (index / columns) * (MenuPlateSize.height + MenuCellGap) },
		MenuPlateSize };
}

// Oracool: user request - click feedback for the two baked-into-the-art button cells. Held in
// ticks rather than a frame counter so the flash lasts the same wall-clock time regardless of
// frame rate.
constexpr uint32_t ButtonFlashDurationMs = 140;

/**
 * @brief How long each unlit/lit step of the menu button's click blink lasts, in milliseconds.
 *
 * Oracool: user, 2026-08-19 - "when i click on it make a rapid unlit/lit sequence." Four steps
 * inside ButtonFlashDurationMs, so the whole thing is over in the same 140ms every other button
 * flash takes and reads as one flick rather than an animation.
 *
 * Driven from the clock rather than counted in frames, so the sequence is identical at 30fps and
 * 144fps. A frame counter would make the blink twice as long on a slow machine, which is the sort
 * of thing that gets reported as "the menu button feels sluggish" and traced to the wrong place.
 */
constexpr uint32_t MenuBlinkPhaseMs = ButtonFlashDurationMs / 2; // ONE blink (user, 2026-09-05: "reduce it to one blink"): pressed for the first half, back for the second; it was four phases, two blinks
constexpr uint8_t ButtonHighlightColor = PAL16_YELLOW + 6;
// Belt cells use their own index; menu icons are offset past them so one variable tracks both.
constexpr int MenuFlashBase = 100;
int FlashingCell = -1;
uint32_t FlashStartedAtMs = 0;

void StartButtonFlash(int visibleIndex)
{
	FlashingCell = visibleIndex;
	FlashStartedAtMs = SDL_GetTicks();
	// The feedback rides on the belt's draw pass; at canvas widths that use partial redraws the
	// belt is only redrawn when marked dirty, so ask for it explicitly.
	RedrawComponent(PanelDrawComponent::Belt);
}

} // namespace

bool IsHudMenuOpen()
{
	return HudMenuOpen;
}

void CloseHudMenu()
{
	HudMenuOpen = false;
}

void DrawHudMenu(const Surface &out)
{
	const uint32_t elapsed = SDL_GetTicks() - FlashStartedAtMs;
	const bool flashing = FlashingCell >= MenuFlashBase && elapsed < ButtonFlashDurationMs;
	const int flashingIcon = flashing ? FlashingCell - MenuFlashBase : -1;

	const Rectangle window = MenuWindowRect();
	// The skill picker's backing: two half passes, the border, the X, the title band.
	DrawHalfTransparentRectTo(out, window.position.x, window.position.y, window.size.width, window.size.height);
	DrawHalfTransparentRectTo(out, window.position.x, window.position.y, window.size.width, window.size.height);
	DrawOrnateBorder(out, window);
	DrawWindowCloseButton(out, window);
	DrawString(out, _("Menu"),
	    { { window.position.x + MenuPadding, window.position.y + MenuPadding },
	        { window.size.width - 2 * MenuPadding - WindowCloseButtonSize, MenuTitleHeight } },
	    { UiFlags::ColorWhitegold | UiFlags::FontSize12 | UiFlags::VerticalCenter });

	for (int i = 0; i < MenuIconCount; i++) {
		const Rectangle rect = IconRect(i);
		// The plate's colour IS the state (user, 2026-09-06: "hovering over the burger menu items to
		// color the backing from gray to white and to blink once and hold gold on click"): light
		// grey at rest, WHITE under the cursor, and on a click one blink - gold for the first half of
		// the flash, grey for the second - then GOLD held for as long as the entry's window is
		// showing. An entry that opens nothing lasting (Game Menu) blinks and returns to grey.
		const bool lit = MenuEntries[i].isOn != nullptr && MenuEntries[i].isOn();
		SkillPlateTint tint = lit ? SkillPlateTint::Ready : SkillPlateTint::Unspent;
		if (i == flashingIcon)
			tint = (elapsed / MenuBlinkPhaseMs) % 2 == 0 ? SkillPlateTint::Ready : SkillPlateTint::Unspent;
		else if (!lit && rect.contains(MousePosition))
			tint = SkillPlateTint::White;
		DrawPlateIn(out, rect, tint);
		// The entry's glyph (oracool-hud-glyphs-v1, in since v1.9.292); the initial, white with the
		// text shadow, stands in when the strip is missing.
		if (!DrawMenuGlyph(out, rect, i)) {
			const char initial[2] = { MenuEntries[i].label[0], '\0' };
			DrawString(out, initial, rect,
			    { UiFlags::ColorWhite | UiFlags::FontSize24 | UiFlags::AlignCenter | UiFlags::VerticalCenter | UiFlags::Shadowed });
		}

	}
}

int HitTestHudMenuIcon(Point mousePosition)
{
	if (!HudMenuOpen)
		return -1;
	for (int i = 0; i < MenuIconCount; i++) {
		if (IconRect(i).contains(mousePosition))
			return i;
	}
	return -1;
}

string_view GetHudMenuEntryLabel(int index)
{
	if (index < 0 || index >= MenuIconCount)
		return {};
	return MenuEntries[index].label;
}

bool IsPointOverHudMenu(Point mousePosition)
{
	return HudMenuOpen && MenuWindowRect().contains(mousePosition);
}

Rectangle GetHudMenuWindowRect()
{
	return MenuWindowRect();
}

Rectangle GetHudMenuCellRect(int index)
{
	return IconRect(index);
}

void CheckHudMenuClick(Point mousePosition)
{
	// The X closes; so does a click anywhere in the window that is not a cell, and anywhere
	// outside it - the old row closed on any miss too.
	const int index = HitTestHudMenuIcon(mousePosition);
	if (index < 0) {
		PlayUiMoveSound();
		CloseHudMenu();
		return;
	}

	// Oracool: "Game Menu" manages its own menu-active state via gamemenu_handle_previous, matching
	// the old CheckBtnUp's gamemenuOff bookkeeping - every other entry should close a still-open
	// pause menu behind this row.
	constexpr int GameMenuEntryIndex = 3;
	// Dead players get the Game Menu and nothing else (self-audit, 2026-08-15). The dead-mode branch
	// in LeftMouseDown routes clicks here so the OLD panel's dead-mode buttons - Game Menu, Chat -
	// stay reachable from a corpse. But it routes to the WHOLE row, and vanilla's dead mode never let
	// a corpse open its inventory or flip through the Abilities window. The windows would open
	// half-functional anyway: LeftMouseDown returns before any in-window click handling while dead,
	// so an entry like Inventory produced a panel that draws but cannot be clicked.
	if (MyPlayerIsDead && index != GameMenuEntryIndex)
		return;
	MenuEntries[index].action();
	// The entry's click, here rather than in the entries: the W key reaches the same runeword toggle
	// and sounds on its own, so a sound inside the toggle would ring twice from here.
	PlayUiSelectSound();
	// Deliberately left open: the icons show what is currently up, so keeping the row on screen
	// lets several panels be toggled in one go rather than reopening the menu each time.
	StartButtonFlash(MenuFlashBase + index);
	if (index != GameMenuEntryIndex)
		gamemenu_off();
}

bool CheckHudMenuSlotClick(Point mousePosition)
{
	if (!GetBeltSlotRect(BeltMenuSlotIndex).contains(mousePosition))
		return false;

	HudMenuOpen = !HudMenuOpen;
	PlayUiSelectSound();
	StartButtonFlash(BeltMenuSlotIndex);
	return true;
}

bool CheckTownPortalBeltSlotClick(Point mousePosition)
{
	if (!GetBeltSlotRect(BeltTownPortalSlotIndex).contains(mousePosition))
		return false;

	// A cast sounds as the spell. Only the refusal needs a click of its own, or the flash below would
	// be the whole answer.
	if (!CastTownPortalAtFeet())
		PlayUiMoveSound();
	// Flash regardless of whether the cast went through: an unlit button would read as a dead
	// click, when the real reason is "not available here" (in town, or multiplayer).
	StartButtonFlash(BeltTownPortalSlotIndex);
	return true;
}

bool CheckRunToggleBeltSlotClick(Point mousePosition)
{
	if (!GetBeltSlotRect(BeltRunToggleSlotIndex).contains(mousePosition))
		return false;

	// ToggleRun already writes the new mode to the event log, so the click needs no message of its
	// own - only the sound and the flash, like the two buttons beside it.
	ToggleRun();
	PlayUiSelectSound();
	StartButtonFlash(BeltRunToggleSlotIndex);
	return true;
}

void DrawBeltButtonFeedback(const Surface &out)
{
	// Nothing on the belt plate while the chat box is open (user, 2026-08-18: the town portal and RMB
	// skill "remain visible when the text input dialog is ON"). scrollrt already suppresses the plate
	// itself under talkflag; these buttons are drawn on their own pass and so were left behind on a
	// plate that was no longer there.
	if (talkflag)
		return;

	// SDL_GetTicks wraps roughly every 49 days; the subtraction is unsigned, so a wrap mid-flash
	// simply ends it early rather than latching the highlight on forever.
	const bool flashing = FlashingCell >= 0 && FlashingCell < MenuFlashBase;
	if (flashing && SDL_GetTicks() - FlashStartedAtMs >= ButtonFlashDurationMs)
		FlashingCell = -1;
	const bool flashingNow = FlashingCell >= 0 && FlashingCell < MenuFlashBase;

	const auto highlight = [&out](const Rectangle &cell) {
		DrawHalfTransparentRectTo(out, cell.position.x, cell.position.y, cell.size.width, cell.size.height,
		    ButtonHighlightColor);
	};

	// The Menu cell, per the user (2026-08-19): "i dont like the lit effect when hover. use the lit
	// icon as hover and when i click on it make a rapid unlit/lit sequence."
	//
	// So the ART carries hover now and the translucent overlay is off this cell entirely - the
	// overlay WAS the lit effect being complained about. Lit means "the cursor is here, or the popup
	// is open"; the click is a blink rather than a state.
	const Rectangle menuCell = GetBeltSlotRect(BeltMenuSlotIndex);
	const bool menuHot = HudMenuOpen || menuCell.contains(MousePosition);

	// The blink. Alternating on a fixed phase from the click, so it is the same sequence every time
	// rather than however many frames happened to elapse - at 60fps and 140ms this is unlit, lit,
	// unlit, lit, and at 30fps it is still exactly that, because the phase is read from the clock
	// and not counted in frames.
	int menuState = menuHot ? 1 : 0;
	if (FlashingCell == BeltMenuSlotIndex) {
		const uint32_t elapsed = SDL_GetTicks() - FlashStartedAtMs;
		// With the three-state sheet (2026-09-05) the blink alternates hover and CLICK rather than
		// unlit and lit, so the pressed picture is what flashes and the button never goes dark
		// under a cursor that is still on it.
		if (elapsed < ButtonFlashDurationMs)
			menuState = ((elapsed / MenuBlinkPhaseMs) % 2 == 0) ? 2 : 1; // pressed first, then back: one blink
	}
	DrawBurgerMenuButton(out, menuState);

	// The Portal cell has real artwork again as of 2026-08-19 - town.portal.png, three orbs the
	// artist labelled ACTIVE / HOVER / CLICKED, which is exactly the 0/1/2 this asks for. So the art
	// carries the states and the overlay stays off this cell, same as the menu button beside it.
	//
	// Momentary rather than a toggle: pressed for the flash after a click, hovered under the cursor,
	// idle otherwise. The flash fires even when the cast is refused, so "not available here" does
	// not read as a dead click; see TryHandleTownPortalClick.
	const Rectangle portalCell = GetBeltSlotRect(BeltTownPortalSlotIndex);
	int portalState = 0;
	if (flashingNow && FlashingCell == BeltTownPortalSlotIndex)
		portalState = 2;
	else if (portalCell.contains(MousePosition))
		portalState = 1;
	DrawTownPortalIcon(out, portalState);
	if (leveltype == DTYPE_TOWN) // refused here, so it looks it (round 27 audit)
		DrawHalfTransparentRectTo(out, portalCell.position.x, portalCell.position.y, portalCell.size.width, portalCell.size.height);

	// The Walk/Run toggle, in the seventh cell hud-v7 added. Momentary like the Portal beside it -
	// the STATE the toggle is in is carried by which glyph strip is drawn, not by this, so a click
	// here is a blink and then back to idle even though the mode it set persists.
	const Rectangle runCell = GetBeltSlotRect(BeltRunToggleSlotIndex);
	int runState = 0;
	if (flashingNow && FlashingCell == BeltRunToggleSlotIndex)
		runState = 2;
	else if (runCell.contains(MousePosition))
		runState = 1;
	DrawRunToggleButton(out, runState);

	// And the four real item cells, which only ever flash on use.
	if (!flashingNow || FlashingCell == BeltTownPortalSlotIndex || FlashingCell == BeltMenuSlotIndex
	    || FlashingCell == BeltRunToggleSlotIndex)
		return;

	highlight(GetBeltSlotRect(FlashingCell));
}

bool CastTownPortalAtFeet()
{
	if (!IsSinglePlayer())
		return false;
	if (leveltype == DTYPE_TOWN)
		return false;

	NetSendCmdLocParam3(true, CMD_SPELLXY, MyPlayer->position.tile,
	    static_cast<int16_t>(SpellID::TownPortal), static_cast<uint8_t>(SpellType::Spell), 0);
	return true;
}

} // namespace devilution::oracool
