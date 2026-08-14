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
#include "msg.h"
#include "oracool/event_log.h"
#include "oracool/hud_art.h"
#include "oracool/hud_layout.h"
#include "oracool/oracool.h"
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
void DoCharacter() { ToggleCharPanel(); }
void DoQuests()
{
	CloseCharPanel();
	CloseGoldWithdraw();
	CloseStash();
	if (!QuestLogIsOpen)
		StartQuestlog();
	else
		QuestLogIsOpen = false;
}
void DoAutomapEntry() { DoAutoMap(); }
void DoGameMenu()
{
	qtextflag = false;
	gamemenu_handle_previous();
}
void DoInventory()
{
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
bool IsAutomapOpen() { return AutomapActive; }
bool IsInventoryOpen() { return invflag; }
bool IsSpellbookOpen() { return sbookflag; }
bool IsLogOpen() { return IsEventLogOpen(); }

// Oracool: placeholder for the Skills system. Present in the menu now so the entry's slot (and,
// once the icon row replaces this list, its icon) is reserved and familiar before the feature
// itself exists.
void DoSkillBook()
{
	InitDiabloMsg(_("The Skill Book is not available yet."));
}

// Oracool: order must match the rows of assets/ui/menu_icons.png exactly - the sprite sheet was
// packed in this order (see scratchpad IconPack.cs). Chat and Friendly Fire were dropped from the
// menu earlier and their art is simply left unused on the artist's sheet.
constexpr std::array<HudMenuEntry, MenuIconCount> MenuEntries { {
    { "Character", DoCharacter, IsCharacterOpen },
    { "Quests", DoQuests, IsQuestsOpen },
    { "Automap", DoAutomapEntry, IsAutomapOpen },
    { "Game Menu", DoGameMenu, nullptr },
    { "Inventory", DoInventory, IsInventoryOpen },
    { "Spellbook", DoSpellbook, IsSpellbookOpen },
    { "Skill Book", DoSkillBook, nullptr },
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

// Packed with no gap. At 2px the eight icons spanned 254px and the centred row began at plate+51,
// while the level-up indicator occupies plate+0..60 - a 9px overlap on the leftmost icon.
constexpr int IconGap = 0;
// Clears the XP counter, which sits just above the plate's top edge.
constexpr int RowGapAbovePlate = 12;
/** @brief Kept between the level-up indicator and the first icon when the two would collide. */
constexpr int LevelUpClearance = 4;

/**
 * @brief Inset from an icon cell to the area INSIDE its drawn frame.
 *
 * Measured off menu_icons.png: a cell is 30x33 and its content runs x 2..26, so four pixels clears
 * the frame on every side. Used to keep the click flash on the icon rather than over its frame.
 */
constexpr int IconFrameInset = 4;
/** @brief The click flash's colour. PAL16 ramps run light to dark, so a low offset is bright. */
constexpr uint8_t IconFlashColor = PAL16_ORANGE + 4;

bool HudMenuOpen = false;

/** @brief The icon row: centred on the plate, sitting just above it. */
Rectangle IconRowRect()
{
	const int width = MenuIconCount * MenuIconSize.width + (MenuIconCount - 1) * IconGap;
	const Rectangle plate = GetMiddleHudRect();

	// Centred on the plate, then pushed right if that would put the first icon under the level-up
	// indicator - which shares this strip of screen above the plate. Expressed as "do not overlap"
	// rather than as a fixed nudge so it stays correct if either widget is resized. The level-up
	// rect does not change with its visibility, so the row never jumps when the player levels.
	const int centred = plate.position.x + (plate.size.width - width) / 2;
	const Rectangle levelUp = GetLevelUpIconRect();
	const int clearOfLevelUp = levelUp.position.x + levelUp.size.width + LevelUpClearance;
	return { { std::max(centred, clearOfLevelUp),
	             plate.position.y - RowGapAbovePlate - MenuIconSize.height },
		{ width, MenuIconSize.height } };
}

Rectangle IconRect(int index)
{
	const Rectangle row = IconRowRect();
	return { { row.position.x + index * (MenuIconSize.width + IconGap), row.position.y }, MenuIconSize };
}

// Oracool: user request - click feedback for the two baked-into-the-art button cells. Held in
// ticks rather than a frame counter so the flash lasts the same wall-clock time regardless of
// frame rate.
constexpr uint32_t ButtonFlashDurationMs = 140;
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
	const bool flashing = FlashingCell >= MenuFlashBase
	    && (SDL_GetTicks() - FlashStartedAtMs) < ButtonFlashDurationMs;
	const int flashingIcon = flashing ? FlashingCell - MenuFlashBase : -1;

	for (int i = 0; i < MenuIconCount; i++) {
		const Rectangle rect = IconRect(i);
		// Lit whenever the thing this entry opens is showing, so the row doubles as a status
		// readout; momentary actions borrow the same lit frame briefly when clicked.
		int state = 0;
		if (MenuEntries[i].isOn != nullptr && MenuEntries[i].isOn())
			state = 2;
		else if (rect.contains(MousePosition))
			state = 1;
		DrawMenuIcon(out, i, state, rect.position);

		// The click flash is drawn OVER the icon rather than by swapping to the art's lit state.
		// The lit state tints the whole cell, frame included; blending inside the frame keeps the
		// flash on the icon itself, which is what a pressed button looks like. The persistent
		// "this panel is open" indication above still uses the art's own lit state - that one is a
		// status readout, not a press.
		if (i == flashingIcon) {
			const Rectangle inner = rect.inset({ IconFrameInset, IconFrameInset });
			DrawHalfTransparentRectTo(out, inner.position.x, inner.position.y,
			    inner.size.width, inner.size.height, IconFlashColor);
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
	return HudMenuOpen && IconRowRect().contains(mousePosition);
}

void CheckHudMenuClick(Point mousePosition)
{
	const int index = HitTestHudMenuIcon(mousePosition);
	if (index < 0) {
		CloseHudMenu();
		return;
	}

	// Oracool: "Game Menu" manages its own menu-active state via gamemenu_handle_previous, matching
	// the old CheckBtnUp's gamemenuOff bookkeeping - every other entry should close a still-open
	// pause menu behind this row.
	constexpr int GameMenuEntryIndex = 3;
	MenuEntries[index].action();
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
	StartButtonFlash(BeltMenuSlotIndex);
	return true;
}

bool CheckTownPortalBeltSlotClick(Point mousePosition)
{
	if (!GetBeltSlotRect(BeltTownPortalSlotIndex).contains(mousePosition))
		return false;

	CastTownPortalAtFeet();
	// Flash regardless of whether the cast went through: an unlit button would read as a dead
	// click, when the real reason is "not available here" (in town, or multiplayer).
	StartButtonFlash(BeltTownPortalSlotIndex);
	return true;
}

void DrawBeltButtonFeedback(const Surface &out)
{
	// The Menu cell is a toggle - lit for as long as its popup is showing. It now has real
	// three-state art, so it no longer needs the translucent overlay it used to rely on.
	const Rectangle menuCell = GetBeltSlotRect(BeltMenuSlotIndex);
	int menuState = 0;
	if (HudMenuOpen)
		menuState = 2;
	else if (menuCell.contains(MousePosition))
		menuState = 1;
	DrawBurgerMenuButton(out, menuState);

	// SDL_GetTicks wraps roughly every 49 days; the subtraction is unsigned, so a wrap mid-flash
	// simply ends it early rather than latching the highlight on forever.
	const bool flashing = FlashingCell >= 0 && FlashingCell < MenuFlashBase;
	if (flashing && SDL_GetTicks() - FlashStartedAtMs >= ButtonFlashDurationMs)
		FlashingCell = -1;
	const bool flashingNow = FlashingCell >= 0 && FlashingCell < MenuFlashBase;

	// The Portal cell has real three-state artwork, so it shows a pressed frame rather than the
	// translucent overlay the frameless Menu cell has to use.
	const Rectangle portalCell = GetBeltSlotRect(BeltTownPortalSlotIndex);
	int portalState = 0;
	if (flashingNow && FlashingCell == BeltTownPortalSlotIndex)
		portalState = 2;
	else if (portalCell.contains(MousePosition))
		portalState = 1;
	DrawTownPortalIcon(out, portalState);

	// Both button cells now show their press through their own artwork, so the overlay is left
	// only for the four real item cells.
	if (!flashingNow || FlashingCell == BeltTownPortalSlotIndex || FlashingCell == BeltMenuSlotIndex)
		return;

	const Rectangle cell = GetBeltSlotRect(FlashingCell);
	DrawHalfTransparentRectTo(out, cell.position.x, cell.position.y, cell.size.width, cell.size.height, ButtonHighlightColor);
}

void CastTownPortalAtFeet()
{
	if (!IsSinglePlayer())
		return;
	if (leveltype == DTYPE_TOWN)
		return;

	NetSendCmdLocParam3(true, CMD_SPELLXY, MyPlayer->position.tile,
	    static_cast<int8_t>(SpellID::TownPortal), static_cast<uint8_t>(SpellType::Spell), 0);
}

} // namespace devilution::oracool
