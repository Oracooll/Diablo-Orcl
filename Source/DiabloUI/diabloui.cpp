#include "DiabloUI/diabloui.h"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <variant>
#include <vector>

#include "DiabloUI/button.h"
#include "DiabloUI/dialogs.h"
#include "DiabloUI/scrollbar.h"
#include "capture.h"
#include "controls/controller.h"
#include "controls/devices/kbcontroller.h"
#include "controls/input.h"
#include "controls/menu_controls.h"
#include "controls/plrctrls.h"
#include "diablo.h"
#include "discord/discord.h"
#include "engine/assets.hpp"
#include "engine/clx_sprite.hpp"
#include "engine/dx.h"
#include "engine/load_pcx.hpp"
#include "engine/render/clx_render.hpp"
#include "engine/render/primitive_render.hpp"
#include "hwcursor.hpp"
#include "utils/display.h"
#include "utils/language.h"
#include "utils/log.hpp"
#include "utils/pcx_to_clx.hpp"
#include "utils/sdl_compat.h"
#include "utils/sdl_geometry.h"
#include "utils/sdl_wrap.h"
#include "utils/stdcompat/optional.hpp"
#include "utils/str_case.hpp"
#include "utils/str_cat.hpp"
#include "utils/stubs.h"
#include "utils/utf8.hpp"

#ifdef __SWITCH__
// for virtual keyboard on Switch
#include "platform/switch/keyboard.h"
#endif
#ifdef __vita__
// for virtual keyboard on Vita
#include "platform/vita/keyboard.h"
#endif
#ifdef __3DS__
// for virtual keyboard on 3DS
#include "platform/ctr/keyboard.h"
#endif

namespace devilution {

OptionalOwnedClxSpriteList ArtLogo;
OptionalOwnedClxSpriteList DifficultyIndicator;

std::array<OptionalOwnedClxSpriteList, 3> ArtFocus;

OptionalOwnedClxSpriteList ArtBackgroundWidescreen;
OptionalOwnedClxSpriteList ArtBackground;

/** @brief A menu screenshot was asked for; taken at the end of the frame. See UiHandleEvents. */
bool PendingUiCapture = false;

OptionalOwnedClxSpriteList ArtCursor;

bool textInputActive = true;
std::size_t SelectedItem = 0;

const string_view BannedNames[] = {
	"gvdl",
	"dvou",
	"tiju",
	"cjudi",
	"bttipmf",
	"ojhhfs",
	"cmj{{bse",
	"benjo",
	"cfbofs",
	"dijol",
	"gbhhpu",
	"ezlf",
	"hppl",
	"hzqtz",
	"kjhbcpp",
	"ljlf",
	"lzlf",
	"ojhhb",
	"qpsdinpolfz",
	"sbhifbe",
	"sfubse",
	"tmboufzf",
	"usbooz",
	"usppo",
	"xfucbdl",
	"xijufusbti",
	"xijufqpxfs",
	"cmbdlqpxfs",
	"{jqqfsifbe",
	"ejbtvshjdbm",
	"efwjmvujpo",
};

const size_t BannedNamesCount = sizeof(BannedNames) / sizeof(BannedNames[0]);

namespace {

OptionalOwnedClxSpriteList ArtHero;
std::array<uint8_t, enum_size<HeroClass>::value + 1> ArtHeroPortraitOrder;
std::array<OptionalOwnedClxSpriteList, enum_size<HeroClass>::value + 1> ArtHeroOverrides;

std::size_t SelectedItemMax;
std::size_t ListViewportSize = 1;
std::size_t listOffset = 0;

void (*gfnListFocus)(int value);
void (*gfnListSelect)(int value);
void (*gfnListEsc)();
void (*gfnFullscreen)();
bool (*gfnListYesNo)();
std::vector<UiItemBase *> gUiItems;
UiList *gUiList = nullptr;
bool UiItemsWraps;
std::optional<TextInputState> UiTextInputState;
bool allowEmptyTextInput = false;

/**
 * @brief The screen's buttons, left to right - the second half of the focus rule's path.
 *
 * Oracool: user rule 1 - "all clickable menu items to be reachable with the keyboard and the pentagram
 * selector." The shared focus model only ever knew about a list: `SelectedItem` indexes `gUiList` and
 * nothing else, and a `UiArtTextButton` was a click target and no more. Three screens had each grown
 * their own private answer to that - selhero's action row, selyesno's two answers, selok's lone OK -
 * which meant three keyboard rules, three focus-drawing loops, and the difficulty picker (a fourth
 * screen, with an OK and a CANCEL) getting none of it and staying mouse-only.
 *
 * One ring here instead. Filled by UiInitList from whatever the screen hands it, so a screen written
 * tomorrow satisfies the rule by existing rather than by remembering to.
 *
 * Left-to-right and not push order: the row IS spatial - the zones of hero_layout.h - so Left and
 * Right have to walk it the way it looks, and selgame pushes its OK and CANCEL before its list for
 * an unrelated hit-testing reason (see the note there).
 */
std::vector<UiArtTextButton *> gUiButtons;
/** -1: the list has focus, or nothing does. Otherwise an index into gUiButtons. */
int SelectedButton = -1;
/** @brief Stops the list marking a row while a button is marked - see FocusButton. */
bool UiListSelectorHidden = false;

/**
 * @brief Rule 2's state: which item a click has armed, and for a list, which row.
 *
 * User rule 2 - "all selection actions to require double click or Enter if reached with pentagram.
 * Single click doesn't initiate the action the button carries unless clicked twice."
 *
 * A pointer to the item and not a copy of it: the only question asked is "is this the same thing the
 * last click armed", and the pointer is never dereferenced. Cleared before anything is activated,
 * because activating usually rebuilds the screen and frees what this points at.
 *
 * Lists already came close to this - vanilla focused a row on the first click and acted on a genuine
 * double-click - but only on screens that passed a focus callback. `gfnListFocus == nullptr` was a
 * shortcut straight to the action, and the main menu, the difficulty picker and the class list all
 * take that path. Which is to say the screens the rule is about were exactly the ones without it.
 */
const void *ArmedItem = nullptr;
std::size_t ArmedIndex = 0;

void DisarmClicks()
{
	ArmedItem = nullptr;
	ArmedIndex = 0;
}

uint32_t fadeTc;
int fadeValue = 0;

struct ScrollBarState {
	bool upArrowPressed;
	bool downArrowPressed;

	ScrollBarState()
	{
		upArrowPressed = false;
		downArrowPressed = false;
	}
} scrollBarState;

void AdjustListOffset(std::size_t itemIndex)
{
	if (itemIndex >= listOffset + ListViewportSize)
		listOffset = itemIndex - (ListViewportSize - 1);
	if (itemIndex < listOffset)
		listOffset = itemIndex;
}

bool IsFocusable(const UiItemBase &item)
{
	return !HasAnyOf(item.GetFlags(), UiFlags::ElementDisabled | UiFlags::ElementHidden);
}

/** @brief Whether the screen has a list with anything in it for focus to sit in. */
bool ListHasRows()
{
	return gUiList != nullptr && !gUiList->m_vecItems.empty();
}

/**
 * @brief The next focusable button @p step away from @p from, wrapping. -1 if the row has none.
 *
 * Delete is disabled whenever the highlighted row is not a real character, and stopping the rule on a
 * greyed-out word would look like the screen had hung.
 */
int NextFocusableButton(int from, int step)
{
	const int count = static_cast<int>(gUiButtons.size());
	if (count == 0)
		return -1;
	for (int i = 1; i <= count; i++) {
		const int candidate = ((from + step * i) % count + count) % count;
		if (IsFocusable(*gUiButtons[candidate]))
			return candidate;
	}
	return -1;
}

/** @brief Moves the pentagrams to button @p index, or back to the list with -1. */
void FocusButton(int index)
{
	SelectedButton = index;
	// The list must stop marking a row while a button is marked, or the screen glows in two places.
	UiListSelectorHidden = index >= 0;
}

} // namespace

void UiInitList(void (*fnFocus)(int value), void (*fnSelect)(int value), void (*fnEsc)(), const std::vector<std::unique_ptr<UiItemBase>> &items, bool itemsWraps, void (*fnFullscreen)(), bool (*fnYesNo)(), size_t selectedItem /*= 0*/)
{
	SelectedItem = selectedItem;
	SelectedItemMax = 0;
	ListViewportSize = 0;
	gfnListFocus = fnFocus;
	gfnListSelect = fnSelect;
	gfnListEsc = fnEsc;
	gfnFullscreen = fnFullscreen;
	gfnListYesNo = fnYesNo;
	gUiItems.clear();
	for (const auto &item : items)
		gUiItems.push_back(item.get());
	UiItemsWraps = itemsWraps;
	listOffset = 0;

	// The button row, rebuilt with the screen. Nothing may survive here across a screen change: every
	// pointer is into the vector the caller is about to own, and activating a button usually frees it.
	gUiButtons.clear();
	for (const auto &item : items) {
		if (item->IsType(UiType::ArtTextButton))
			gUiButtons.push_back(static_cast<UiArtTextButton *>(item.get()));
	}
	std::sort(gUiButtons.begin(), gUiButtons.end(),
	    [](const UiArtTextButton *a, const UiArtTextButton *b) { return a->m_rect.x < b->m_rect.x; });
	FocusButton(-1);
	DisarmClicks();

	if (fnFocus != nullptr)
		fnFocus(selectedItem);

#ifndef __SWITCH__
	SDL_StopTextInput(); // input is enabled by default
#endif
	textInputActive = false;
	UiScrollbar *uiScrollbar = nullptr;
	for (const auto &item : items) {
		if (item->IsType(UiType::Edit)) {
			auto *pItemUIEdit = static_cast<UiEdit *>(item.get());
			SDL_SetTextInputRect(&item->m_rect);
			textInputActive = true;
			allowEmptyTextInput = pItemUIEdit->m_allowEmpty;
#ifdef __SWITCH__
			switch_start_text_input(pItemUIEdit->m_hint, pItemUIEdit->m_value, pItemUIEdit->m_max_length);
#elif defined(__vita__)
			vita_start_text_input(pItemUIEdit->m_hint, pItemUIEdit->m_value, pItemUIEdit->m_max_length);
#elif defined(__3DS__)
			ctr_vkbdInput(pItemUIEdit->m_hint, pItemUIEdit->m_value, pItemUIEdit->m_value, pItemUIEdit->m_max_length);
#else
			SDL_StartTextInput();
#endif
			UiTextInputState.emplace(TextInputState::Options {
			    .value = pItemUIEdit->m_value,
			    .cursor = &pItemUIEdit->m_cursor,
			    .maxLength = pItemUIEdit->m_max_length,
			});
		} else if (item->IsType(UiType::List)) {
			auto *uiList = static_cast<UiList *>(item.get());
			// Oracool: `size() - 1` on an EMPTY list wraps size_t to SIZE_MAX, and std::max then
			// happily keeps it - after which the guarded GetItem(selectedItem) below indexes an empty
			// vector. Latent until the hero-select screen moved "New Hero" out of its list and into a
			// button, which made a character list with nothing in it reachable on a fresh install.
			SelectedItemMax = uiList->m_vecItems.empty() ? 0 : uiList->m_vecItems.size() - 1;
			ListViewportSize = uiList->viewportSize;
			gUiList = uiList;
			if (!uiList->m_vecItems.empty() && selectedItem <= SelectedItemMax
			    && HasAnyOf(uiList->GetItem(selectedItem)->uiFlags, UiFlags::NeedsNextElement))
				AdjustListOffset(selectedItem + 1);
		} else if (item->IsType(UiType::Scrollbar)) {
			uiScrollbar = static_cast<UiScrollbar *>(item.get());
		}
	}

	AdjustListOffset(selectedItem);

	if (uiScrollbar != nullptr) {
		// An EMPTY list has a viewport of 0 (UiList clamps it to the item count) against a
		// SelectedItemMax of 0, which reads as "0 >= 1, so there is more to scroll to" and showed a
		// scrollbar beside nothing. Same trigger as the underflow above: the hero-select screen's
		// character list can now be empty.
		const bool listIsEmpty = gUiList == nullptr || gUiList->m_vecItems.empty();
		if (listIsEmpty || ListViewportSize >= static_cast<std::size_t>(SelectedItemMax + 1)) {
			uiScrollbar->Hide();
		} else {
			uiScrollbar->Show();
		}
	}

	// A screen whose only controls are buttons - the message box, the delete prompt - starts on the
	// leftmost of them, since there is no list for focus to sit in and rule 1 says something must be
	// marked. On the delete prompt that leftmost is Yes, which is where its own default was.
	//
	// Not on a screen with a text box, though. The name box has no list either, but it is where focus
	// belongs on arrival and it wants the arrow keys for its own caret - see MenuAction_LEFT below.
	// Down reaches its OK and Cancel from there, which is the same path the character list uses.
	if (!ListHasRows() && !textInputActive) {
		if (const int first = NextFocusableButton(-1, 1); first >= 0)
			FocusButton(first);
	}
}

void UiRenderListItems()
{
	UiRenderItems(gUiItems);
}

void UiInitList_clear()
{
	SelectedItem = 0;
	SelectedItemMax = 0;
	ListViewportSize = 1;
	gfnListFocus = nullptr;
	gfnListSelect = nullptr;
	gfnListEsc = nullptr;
	gfnFullscreen = nullptr;
	gfnListYesNo = nullptr;
	gUiList = nullptr;
	gUiItems.clear();
	gUiButtons.clear();
	FocusButton(-1);
	DisarmClicks();
	UiItemsWraps = false;
}

void UiPlayMoveSound()
{
	effects_play_sound(IS_TITLEMOV);
}

void UiPlaySelectSound()
{
	effects_play_sound(IS_TITLSLCT);
}

namespace {

void UiFocus(std::size_t itemIndex, bool checkUp, bool ignoreItemsWraps = false)
{
	if (SelectedItem == itemIndex)
		return;

	AdjustListOffset(itemIndex);

	const auto *pItem = gUiList->GetItem(itemIndex);
	while (HasAnyOf(pItem->uiFlags, UiFlags::ElementHidden | UiFlags::ElementDisabled)) {
		if (checkUp) {
			if (itemIndex > 0)
				itemIndex -= 1;
			else if (UiItemsWraps && !ignoreItemsWraps)
				itemIndex = SelectedItemMax;
			else
				checkUp = false;
		} else {
			if (itemIndex < SelectedItemMax)
				itemIndex += 1;
			else if (UiItemsWraps && !ignoreItemsWraps)
				itemIndex = 0;
			else
				checkUp = true;
		}
		pItem = gUiList->GetItem(itemIndex);
	}

	if (HasAnyOf(pItem->uiFlags, UiFlags::NeedsNextElement))
		AdjustListOffset(itemIndex + 1);
	AdjustListOffset(itemIndex);

	SelectedItem = itemIndex;

	UiPlayMoveSound();

	if (gfnListFocus != nullptr)
		gfnListFocus(itemIndex);
}

void UiFocusUp()
{
	if (SelectedItem > 0)
		UiFocus(SelectedItem - 1, true);
	else if (UiItemsWraps)
		UiFocus(SelectedItemMax, true);
}

void UiFocusDown()
{
	if (SelectedItem < SelectedItemMax)
		UiFocus(SelectedItem + 1, false);
	else if (UiItemsWraps)
		UiFocus(0, false);
}

// UiFocusPageUp/Down mimics the slightly weird behaviour of actual Diablo.

void UiFocusPageUp()
{
	if (listOffset == 0) {
		UiFocus(0, true, true);
	} else {
		const std::size_t relpos = SelectedItem - listOffset;
		std::size_t prevPageStart = SelectedItem - relpos;
		if (prevPageStart >= ListViewportSize)
			prevPageStart -= ListViewportSize;
		else
			prevPageStart = 0;
		AdjustListOffset(prevPageStart);
		UiFocus(listOffset + relpos, true, true);
	}
}

void UiFocusPageDown()
{
	if (listOffset + ListViewportSize > static_cast<std::size_t>(SelectedItemMax)) {
		UiFocus(SelectedItemMax, false, true);
	} else {
		const std::size_t relpos = SelectedItem - listOffset;
		std::size_t nextPageEnd = SelectedItem + (ListViewportSize - relpos - 1);
		if (nextPageEnd + ListViewportSize <= static_cast<std::size_t>(SelectedItemMax))
			nextPageEnd += ListViewportSize;
		else
			nextPageEnd = SelectedItemMax;
		AdjustListOffset(nextPageEnd);
		UiFocus(listOffset + relpos, false, true);
	}
}

void SelheroCatToName(const char *inBuf, char *outBuf, int cnt)
{
	size_t outLen = strlen(outBuf);
	char *dest = outBuf + outLen;
	size_t destCount = cnt - outLen;
	CopyUtf8(dest, inBuf, destCount);
}

/** @brief Fires the focused button. Nothing may touch gUiButtons after this - see the note there. */
void ActivateFocusedButton()
{
	if (SelectedButton >= static_cast<int>(gUiButtons.size()))
		return;
	UiArtTextButton *button = gUiButtons[SelectedButton];
	DisarmClicks();
	UiPlaySelectSound();
	button->Activate();
}

/**
 * @brief Rule 1's navigation: the list first, then the button row under it.
 *
 * Written against MenuAction rather than raw SDL keys, which is what the three screens this replaces
 * could not do - they hooked UiPollAndRender's event handler and matched SDLK_ symbols, so their
 * buttons answered a keyboard and nothing else. Here the same code serves the keyboard, the
 * controller and the touch pad, because GetMenuActions already folds all three into these.
 */
bool HandleMenuAction(MenuAction menuAction)
{
	// MenuAction_NONE FIRST, and this is not a tidiness check. UiPollAndRender calls this every frame
	// with GetMenuHeldUpDownAction(), which is NONE whenever nothing is held - so anything done above
	// this line runs once per frame rather than once per input. Disarming there wiped the first click
	// on the very next frame, and the second click found nothing armed and only re-armed: double-click
	// did nothing at all and Enter was the only way to act.
	if (menuAction == MenuAction_NONE)
		return false;

	// Now it is safe. Any deliberate move disarms a half-finished click: the two clicks rule 2 asks
	// for have to be consecutive, or one left over from three screens ago could complete against
	// whatever now occupies that spot.
	DisarmClicks();

	const bool onButtons = SelectedButton >= 0;
	switch (menuAction) {
	case MenuAction_SELECT:
		if (onButtons) {
			ActivateFocusedButton();
			return true;
		}
		UiFocusNavigationSelect();
		return true;
	case MenuAction_UP:
		if (onButtons) {
			// Back to whatever sits above the row - a list, or the name box's text field. Nothing on a
			// screen that is only buttons, where leaving would mean marking nothing at all.
			if (ListHasRows() || textInputActive)
				FocusButton(-1);
			return true;
		}
		UiFocusUp();
		return true;
	case MenuAction_DOWN:
		if (onButtons)
			return true; // already at the bottom of the screen; swallow rather than wrap
		// Only once the list has nowhere further to go, so Down still walks the rows first.
		if (ListHasRows() && SelectedItem < static_cast<std::size_t>(SelectedItemMax)) {
			UiFocusDown();
			return true;
		}
		if (const int first = NextFocusableButton(-1, 1); first >= 0) {
			FocusButton(first);
			return true;
		}
		UiFocusDown(); // no buttons: a wrapping list wraps, exactly as before
		return true;
	case MenuAction_LEFT:
	case MenuAction_RIGHT: {
		// Left/Right reached nothing in the front end before this; the row is the only thing on these
		// screens laid out horizontally, so it is the only thing they can mean.
		//
		// Returning false when focus is NOT on the row is load-bearing rather than tidy: the name box
		// hands unclaimed events to HandleTextInputEvent, and these two are how its caret moves. Take
		// them unconditionally and typing a name loses its arrow keys.
		if (!onButtons)
			return false;
		const int step = menuAction == MenuAction_LEFT ? -1 : 1;
		if (const int next = NextFocusableButton(SelectedButton, step); next >= 0)
			FocusButton(next);
		return true;
	}
	case MenuAction_PAGE_UP:
		if (onButtons)
			return true;
		UiFocusPageUp();
		return true;
	case MenuAction_PAGE_DOWN:
		if (onButtons)
			return true;
		UiFocusPageDown();
		return true;
	case MenuAction_DELETE:
		UiFocusNavigationYesNo();
		return true;
	case MenuAction_BACK:
		if (gfnListEsc == nullptr)
			return false;
		UiFocusNavigationEsc();
		return true;
	default:
		return false;
	}
}

void UiOnBackgroundChange()
{
	fadeTc = 0;
	fadeValue = 0;

	BlackPalette();

	if (IsHardwareCursorEnabled() && ArtCursor && ControlDevice == ControlTypes::KeyboardAndMouse && GetCurrentCursorInfo().type() != CursorType::UserInterface) {
		SetHardwareCursor(CursorInfo::UserInterfaceCursor());
	}

	SDL_FillRect(DiabloUiSurface(), nullptr, 0x000000);
	if (DiabloUiSurface() == PalSurface)
		BltFast(nullptr, nullptr);
	RenderPresent();
}

} // namespace

void UiFocusNavigation(SDL_Event *event)
{
	switch (event->type) {
	case SDL_KEYUP:
	case SDL_MOUSEBUTTONUP:
	case SDL_MOUSEMOTION:
#ifndef USE_SDL1
	case SDL_MOUSEWHEEL:
#endif
	case SDL_JOYBUTTONUP:
	case SDL_JOYAXISMOTION:
	case SDL_JOYBALLMOTION:
	case SDL_JOYHATMOTION:
#ifndef USE_SDL1
	case SDL_FINGERUP:
	case SDL_FINGERMOTION:
	case SDL_CONTROLLERBUTTONUP:
	case SDL_CONTROLLERAXISMOTION:
	case SDL_WINDOWEVENT:
#endif
	case SDL_SYSWMEVENT:
		mainmenu_restart_repintro();
		break;
	}

	bool menuActionHandled = false;
	for (MenuAction menuAction : GetMenuActions(*event))
		menuActionHandled |= HandleMenuAction(menuAction);
	if (menuActionHandled)
		return;

#if SDL_VERSION_ATLEAST(2, 0, 0)
	if (event->type == SDL_MOUSEWHEEL) {
		if (event->wheel.y > 0) {
			UiFocusUp();
		} else if (event->wheel.y < 0) {
			UiFocusDown();
		}
		return;
	}
#else
	if (event->type == SDL_MOUSEBUTTONDOWN) {
		switch (event->button.button) {
		case SDL_BUTTON_WHEELUP:
			UiFocusUp();
			return;
		case SDL_BUTTON_WHEELDOWN:
			UiFocusDown();
			return;
		}
	}
#endif

	if (UiTextInputState && HandleTextInputEvent(*event, *UiTextInputState)) {
		return;
	}

	if (event->type == SDL_MOUSEBUTTONDOWN || event->type == SDL_MOUSEBUTTONUP) {
		if (UiItemMouseEvents(event, gUiItems))
			return;
	}
}

void UiHandleEvents(SDL_Event *event)
{
	if (event->type == SDL_MOUSEMOTION) {
#ifdef USE_SDL1
		OutputToLogical(&event->motion.x, &event->motion.y);
#endif
		MousePosition = { event->motion.x, event->motion.y };
		return;
	}

	// Oracool: user report - screenshots were impossible on every front-end screen. The Screenshot
	// action is a Keymapper binding, and the Keymapper is only consulted while a game is running, so
	// the key reached nothing here.
	//
	// F12 as well as Print Screen, deliberately: Windows 11 binds PrtScn to the Snipping Tool by
	// default and swallows it before the game sees it, which is the likely reason the key "did
	// nothing" even where it was bound. F12 is unclaimed both by the OS and by these screens.
	if (event->type == SDL_KEYDOWN) {
		const SDL_Keycode key = event->key.keysym.sym;
#ifdef USE_SDL1
		const bool isScreenshotKey = key == SDLK_PRINT || key == SDLK_F12;
#else
		const bool isScreenshotKey = key == SDLK_PRINTSCREEN || key == SDLK_F12;
#endif
		if (isScreenshotKey) {
			// Deferred to the end of the frame rather than taken here. Events are polled at the TOP
			// of UiPollAndRender, before UiRenderListItems draws the list, the buttons and the
			// scrollbar - and the previous frame's copy of them has already been wiped by this
			// frame's background blit. Capturing on the keypress therefore saved a screen with its
			// controls missing, which is exactly what the first menu screenshots came out as.
			PendingUiCapture = true;
			return;
		}
	}

#if HAS_KBCTRL == 0
	if (event->type == SDL_KEYDOWN && event->key.keysym.sym == SDLK_RETURN) {
		const Uint8 *state = SDLC_GetKeyState();
		if (state[SDLC_KEYSTATE_LALT] != 0 || state[SDLC_KEYSTATE_RALT] != 0) {
			sgOptions.Graphics.fullscreen.SetValue(!IsFullScreen());
			SaveOptions();
			if (gfnFullscreen != nullptr)
				gfnFullscreen();
			return;
		}
	}
#endif

	if (event->type == SDL_QUIT)
		diablo_quit(0);

#ifndef USE_SDL1
	HandleControllerAddedOrRemovedEvent(*event);

	// The front end's own copy of MainWndProc's switch, and it needs the same guard: only this
	// window's events may speak for this window. See the note there.
	if (event->type == SDL_WINDOWEVENT && ghMainWnd != nullptr
	    && event->window.windowID != SDL_GetWindowID(ghMainWnd)) {
		return;
	}

	if (event->type == SDL_WINDOWEVENT) {
		if (IsAnyOf(event->window.event, SDL_WINDOWEVENT_SHOWN, SDL_WINDOWEVENT_EXPOSED, SDL_WINDOWEVENT_RESTORED)) {
			gbActive = true;
		} else if (IsAnyOf(event->window.event, SDL_WINDOWEVENT_HIDDEN, SDL_WINDOWEVENT_MINIMIZED)) {
			gbActive = false;
		} else if (event->window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
			// We reinitialize immediately (by calling `DoReinitializeHardwareCursor` instead of `ReinitializeHardwareCursor`)
			// because the cursor's Enabled state may have changed, resulting in changes to visibility.
			//
			// For example, if the previous size was too large for a hardware cursor then it was invisible
			// but may now become visible.
			DoReinitializeHardwareCursor();
		} else if (event->window.event == SDL_WINDOWEVENT_FOCUS_LOST) {
			music_mute();
		} else if (event->window.event == SDL_WINDOWEVENT_FOCUS_GAINED) {
			diablo_focus_unpause();
		}
	}
#else
	if (event->type == SDL_ACTIVEEVENT && (event->active.state & SDL_APPINPUTFOCUS) != 0) {
		if (event->active.gain == 0)
			music_mute();
		else
			diablo_focus_unpause();
	}
#endif
}

void UiFocusNavigationSelect()
{
	UiPlaySelectSound();
	if (IsTextInputActive()) {
		if (!allowEmptyTextInput && UiTextInputState->empty()) {
			return;
		}
#ifndef __SWITCH__
		SDL_StopTextInput();
#endif
		UiTextInputState = std::nullopt;
	}
	if (gfnListSelect != nullptr)
		gfnListSelect(SelectedItem);
}

void UiFocusNavigationEsc()
{
	UiPlaySelectSound();
	if (IsTextInputActive()) {
#ifndef __SWITCH__
		SDL_StopTextInput();
#endif
		UiTextInputState = std::nullopt;
	}
	if (gfnListEsc != nullptr)
		gfnListEsc();
}

bool UiClickArms(const void *item)
{
	if (ArmedItem == item) {
		DisarmClicks();
		return true;
	}
	ArmedItem = item;
	ArmedIndex = 0;
	return false;
}

void UiFocusNavigationYesNo()
{
	if (gfnListYesNo == nullptr)
		return;

	if (gfnListYesNo())
		UiPlaySelectSound();
}

namespace {

bool IsInsideRect(const SDL_Event &event, const SDL_Rect &rect)
{
	const SDL_Point point = { event.button.x, event.button.y };
	return SDL_PointInRect(&point, &rect) == SDL_TRUE;
}

void LoadHeros()
{
	constexpr unsigned PortraitHeight = 76;
	ArtHero = LoadPcxSpriteList("ui_art\\heros", -static_cast<int>(PortraitHeight));
	if (!ArtHero)
		return;
	const uint16_t numPortraits = ClxSpriteList { *ArtHero }.numSprites();

	ArtHeroPortraitOrder = { 0, 1, 2, 2, 1, 0, 3 };
	if (numPortraits >= 6) {
		ArtHeroPortraitOrder[static_cast<std::size_t>(HeroClass::Monk)] = 3;
		ArtHeroPortraitOrder[static_cast<std::size_t>(HeroClass::Bard)] = 4;
		ArtHeroPortraitOrder[enum_size<HeroClass>::value] = 5;
	}
	if (numPortraits >= 7) {
		ArtHeroPortraitOrder[static_cast<std::size_t>(HeroClass::Barbarian)] = 6;
	}

	for (size_t i = 0; i <= enum_size<HeroClass>::value; ++i) {
		char portraitPath[18];
		*BufCopy(portraitPath, "ui_art\\hero", i) = '\0';
		ArtHeroOverrides[i] = LoadPcx(portraitPath, /*transparentColor=*/std::nullopt, /*outPalette=*/nullptr, /*logError=*/false);
	}
}

void LoadUiGFX()
{
	// Oracool: user request - the Diablo logo on every menu screen, in Hellfire mode too. Vanilla
	// swaps in ui_art\hf_logo2 here whenever gbIsHellfire is set; this build never does. One of
	// three places the logo used to fork - see gmenu.cpp's sgpLogo and DiabloUI/title.cpp.
	//
	// The BIG one (550x216), not ui_art\smlogo (390x154), on the user's call. Diablo ships two
	// versions of the same flaming letters and vanilla only ever used the small one here; against
	// Hellfire's 640-wide masthead the small one read as a shrunken title rather than as a
	// different one. Both are 15 frames keyed on 250, so this is a filename swap - but it is 62px
	// taller, and the screens that hang off it had to move: see HeroTitleTop in hero/hero_layout.h,
	// and the logo offsets in mainmenu.cpp and settingsmenu.cpp.
	//
	// Neither is interchangeable with hf_logo2, which is 16 frames keyed on 0.
	ArtLogo = LoadPcxSpriteList("ui_art\\logo", /*numFrames=*/15, /*transparentColor=*/250);
	DifficultyIndicator = LoadPcx("ui_art\\r1_gry", /*transparentColor=*/0);
	ArtFocus[FOCUS_SMALL] = LoadPcxSpriteList("ui_art\\focus16", /*numFrames=*/8, /*transparentColor=*/250);
	ArtFocus[FOCUS_MED] = LoadPcxSpriteList("ui_art\\focus", /*numFrames=*/8, /*transparentColor=*/250);
	ArtFocus[FOCUS_BIG] = LoadPcxSpriteList("ui_art\\focus42", /*numFrames=*/8, /*transparentColor=*/250);

	ArtCursor = LoadPcx("ui_art\\cursor", /*transparentColor=*/0);

	LoadHeros();
}

} // namespace

bool IsTextInputActive()
{
	return bool(UiTextInputState);
}

ClxSprite UiGetHeroDialogSprite(size_t heroClassIndex)
{
	return ArtHeroOverrides[heroClassIndex]
	    ? (*ArtHeroOverrides[heroClassIndex])[0]
	    : (*ArtHero)[ArtHeroPortraitOrder[heroClassIndex]];
}

void UnloadUiGFX()
{
	ArtHero = std::nullopt;
	for (OptionalOwnedClxSpriteList &override : ArtHeroOverrides)
		override = std::nullopt;
	ArtCursor = std::nullopt;
	for (auto &art : ArtFocus)
		art = std::nullopt;
	ArtLogo = std::nullopt;
	DifficultyIndicator = std::nullopt;
}

void UiInitialize()
{
	LoadUiGFX();

	if (ArtCursor) {
		if (SDL_ShowCursor(SDL_DISABLE) <= -1) {
			ErrSdl();
		}
	}
}

void UiDestroy()
{
	UnloadFonts();
	UnloadUiGFX();
}

bool UiValidPlayerName(string_view name)
{
	if (name.empty())
		return false;

	// Currently only allow saving PlayerNameLength bytes as a player name, so if the name is too long we'd have to truncate it.
	// That said the input buffer is only 16 bytes long...
	if (name.size() > PlayerNameLength)
		return false;

	if (name.find_first_of(",<>%&\\\"?*#/: ") != name.npos)
		return false;

	// Only basic latin alphabet is supported for multiplayer characters to avoid rendering issues for players who do
	// not have fonts.mpq installed
	if (!std::all_of(name.begin(), name.end(), IsBasicLatin))
		return false;

	std::string buffer { name };

	AsciiStrToLower(buffer);

	for (char &character : buffer)
		character++;

	for (string_view bannedName : BannedNames) {
		if (buffer.find(bannedName) != std::string::npos)
			return false;
	}

	return true;
}

Sint16 GetCenterOffset(Sint16 w, Sint16 bw)
{
	if (bw == 0) {
		bw = gnScreenWidth;
	}

	return (bw - w) / 2;
}

/**
 * @brief The front-end palette. Always Diablo's, never Hellfire's.
 *
 * Oracool: vanilla picks ui_art\hellfire.pal whenever gbIsHellfire is set. This build does not, for
 * the same reason it draws the Diablo logo and says "Exit Diablo" - the front end is Diablo's
 * regardless of what content the game loads.
 *
 * It is also the palette every colour in these screens was MEASURED against: the gold ramp at
 * 176-191, silver at 224-239, the focus glow's amber at 193-207, the scrollbar bevel at 182/186/
 * 188/191. Those indices mean what they are supposed to mean here and nowhere else.
 */
void UiLoadDefaultPalette()
{
	LoadPalette("ui_art\\diablo.pal", /*blend=*/false);
	ApplyGamma(logical_palette, orig_palette, 256);
}

bool UiLoadBlackBackground()
{
	ArtBackground = std::nullopt;
	UiLoadDefaultPalette();
	UiOnBackgroundChange();
	return true;
}

void LoadBackgroundArt(const char *pszFile, int frames)
{
	ArtBackground = std::nullopt;
	SDL_Color pPal[256];
	ArtBackground = LoadPcxSpriteList(pszFile, static_cast<uint16_t>(frames), /*transparentColor=*/std::nullopt, pPal);
	if (!ArtBackground)
		return;

	LoadPalInMem(pPal);
	ApplyGamma(logical_palette, orig_palette, 256);
	UiOnBackgroundChange();
}

void UiAddBackground(std::vector<std::unique_ptr<UiItemBase>> *vecDialog)
{
	const SDL_Rect rect = MakeSdlRect(0, GetUIRectangle().position.y, 0, 0);
	if (ArtBackgroundWidescreen) {
		vecDialog->push_back(std::make_unique<UiImageClx>((*ArtBackgroundWidescreen)[0], rect, UiFlags::AlignCenter));
	}
	if (ArtBackground) {
		vecDialog->push_back(std::make_unique<UiImageClx>((*ArtBackground)[0], rect, UiFlags::AlignCenter));
	}
}

void UiAddLogo(std::vector<std::unique_ptr<UiItemBase>> *vecDialog, int y)
{
	vecDialog->push_back(std::make_unique<UiImageAnimatedClx>(
	    *ArtLogo, MakeSdlRect(0, y, 0, 0), UiFlags::AlignCenter));
}

void UiFadeIn()
{
	if (fadeValue < 256) {
		if (fadeValue == 0 && fadeTc == 0)
			fadeTc = SDL_GetTicks();
		const int prevFadeValue = fadeValue;
		fadeValue = static_cast<int>((SDL_GetTicks() - fadeTc) / 2.083); // 32 frames @ 60hz
		if (fadeValue > 256) {
			fadeValue = 256;
			fadeTc = 0;
		}
		if (fadeValue != prevFadeValue) {
			// We can skip hardware cursor update for fade level 0 (everything is black).
			SetFadeLevel(fadeValue, /*updateHardwareCursor=*/fadeValue != 0);
		}
	}

	if (DiabloUiSurface() == PalSurface)
		BltFast(nullptr, nullptr);
	RenderPresent();
}

namespace {

/**
 * @brief The focus selector: the two rotating pentagrams that flank the highlighted row.
 *
 * Oracool: user request - back, after a spell as a gold border and then as a glow, and a size
 * smaller than vanilla drew them. The art was never removed: ArtFocus holds ui_art\focus16, focus
 * and focus42, eight animation frames each, and LoadUiGFX has been loading all three throughout.
 * Only the size picker below is new.
 *
 * ONE STEP DOWN is the whole of "a bit smaller". Vanilla gave a row 42px or taller the big pair and
 * a 30px row the medium; every row now takes the next size down, so the hero list (52px), the
 * waypoint list (43px) and the Abilities rows (44px) get the medium pair where they used to get the
 * big one, and the settings rows keep the small. Floored at FOCUS_SMALL because there is nothing
 * below it.
 */
void DrawSelector(const SDL_Rect &rect)
{
	const int size = rect.h >= 42 ? FOCUS_MED : FOCUS_SMALL;
	if (!ArtFocus[size])
		return;
	const ClxSpriteList sprites = *ArtFocus[size];
	const ClxSprite sprite = sprites[GetAnimationFrame(sprites.numSprites())];

	const int y = rect.y + (rect.h - static_cast<int>(sprite.height())) / 2;
	const Surface &out = Surface(DiabloUiSurface());
	RenderClxSprite(out, sprite, { rect.x, y });
	RenderClxSprite(out, sprite, { rect.x + rect.w - sprite.width(), y });
}

// The focus glow lived here for a dozen versions - three rings of the row's own text redrawn around
// itself, amber and then bright yellow - and is gone on the user's call in favour of the pentagrams
// above. With it went AllTextColorFlags, WithTextColor and RecolorArgs, which existed only to swap a
// widget's colour for a halo colour and had no other caller.
//
// Two things it leaves behind on purpose. UiFlags::ColorOracoolYellow and its dark companion, with
// fonts\oracool_yellow.trn and oracool_yellows.trn, are still wired through text_render: they are the
// palette's 128-135 yellow ramp, whose top (255,253,159) at luminance 243 is the brightest text this
// front end can draw, and finding that was the expensive part rather than using it. Any future
// highlight can just ask for the flag.

} // namespace

void UiClearScreen()
{
	if (!ArtBackground || gnScreenWidth > (*ArtBackground)[0].width() || gnScreenHeight > (*ArtBackground)[0].height())
		SDL_FillRect(DiabloUiSurface(), nullptr, 0x000000);
}

void UiPollAndRender(std::optional<tl::function_ref<bool(SDL_Event &)>> eventHandler)
{
	SDL_Event event;
	while (PollEvent(&event) != 0) {
		if (eventHandler && (*eventHandler)(event))
			continue;
		UiFocusNavigation(&event);
		UiHandleEvents(&event);
	}
	HandleMenuAction(GetMenuHeldUpDownAction());
	UiRenderListItems();
	DrawMouse();
	UiFadeIn();

	// Must happen after at least one call to `UiFadeIn` with non-zero fadeValue.
	// `UiFadeIn` calls `SetFadeLevel` which reinitializes the hardware cursor.
	if (IsHardwareCursor() && fadeValue != 0)
		SetHardwareCursorVisible(ControlDevice == ControlTypes::KeyboardAndMouse);

	// The frame is complete and presented by here, so this is the only point at which the surface
	// holds everything the player can see. See the note where the flag is set.
	if (PendingUiCapture) {
		PendingUiCapture = false;
		CaptureUiScreen();
	}

#ifdef __3DS__
	// Keyboard blocks until input is finished
	// so defer until after render and fade-in
	ctr_vkbdFlush();
#endif

	discord_manager::UpdateMenu();
}

namespace {

/**
 * @brief Oracool: every front-end label sits in the middle of its own box, not on the box's top edge.
 *
 * User request - "make all available menus in the front end have their items/texts/articles be
 * vertically centered in their invisible text boxes", pointing at the main menu, where "Single
 * Player" hung from the top of an 86px row with 44px of air below it.
 *
 * Added here rather than at the ~40 places a widget is constructed, for three reasons. It cannot be
 * forgotten on a screen added later. It is idempotent - the multiplayer screens already pass
 * VerticalCenter on their buttons and lists, and OR-ing it again changes nothing. And it is a no-op
 * wherever the box was already the size of its text: DrawString centres by
 * `max(0, (rect.h - lines * lineHeight) / 2)`, so a rect of height 0 (the common "no explicit
 * height" case) and a rect exactly one line tall both offset by zero.
 *
 * Safe on the multi-line bodies because that line count comes from counting '\n' in the string, and
 * every paragraph in this front end is already run through WordWrapString before it is handed to a
 * widget (dialogs.cpp, selok, selyesno, settingsmenu, selgame, selconn) - the breaks are in the text
 * itself, not decided at draw time, so the measured height is the real one.
 */
constexpr UiFlags CenteredInBox = UiFlags::VerticalCenter;

/**
 * @brief Width one pentagram reserves at a row's end.
 *
 * Deliberately mirrors DrawSelector's own size pick rather than restating a number: the two must
 * agree or the label is centred against a gap the art does not actually leave. Reads the sprite's
 * real width instead of a constant because ui_art\focus*.pcx comes out of diabdat.mpq, so its size
 * is the archive's fact, not ours - and 0 when the art is absent, which DrawSelector also tolerates.
 */
int SelectorPadding(int rowHeight)
{
	const int size = rowHeight >= 42 ? FOCUS_MED : FOCUS_SMALL;
	if (!ArtFocus[size])
		return 0;
	return static_cast<int>((*ArtFocus[size])[0].width());
}

/**
 * @brief The row minus the two pentagram zones - where a centred label actually belongs.
 *
 * Oracool: user request - "we need to do something about player names fitting between the
 * pentagrams". The bug was that DrawSelector puts its two sprites at rect.x and
 * rect.x + rect.w - sprite.width() while the label was centred across the WHOLE rect, so the row's
 * two ends were being drawn twice over. A long enough string simply ran underneath the art.
 *
 * Applied to every row rather than only the focused one on purpose: inset only when selected and the
 * text would shift sideways every time the selection moved.
 *
 * MEASURED against the four hero-screen buttons before doing this, since they share the treatment
 * and a too-narrow rect would clip them instead: at FontSize42 in a 240px zone less two 28px
 * pentagrams, "New Hero" is the widest at 182px against 184px available. It fits, but by 2px - so if
 * a button label is ever reworded, measure it.
 */
Rectangle LabelRect(const SDL_Rect &rect)
{
	const int pad = SelectorPadding(rect.h);
	Rectangle out = MakeRectangle(rect);
	if (pad <= 0 || 2 * pad >= out.size.width)
		return out; // nothing to reserve, or the row is too narrow to give any of it up
	out.position.x += pad;
	out.size.width -= 2 * pad;
	return out;
}

/**
 * @brief Shortens @p text with a trailing ellipsis until it fits @p maxWidth.
 *
 * The backstop under the hero list's 10-character name cap. MEASURED at FontSize30 against that
 * list's 168px column (224 less the two pentagrams): a ten-character "Bartholome" is 157px and ten
 * digits are 139px, so a normal name never reaches this; ten capital Ms are 219px and ten zeros
 * 209px, so a pathological one still can. Truncating beats the alternative, which with AlignCenter
 * is a string clipped at BOTH ends.
 */
string_view FitToWidth(string_view text, int maxWidth, GameFontTables font, int spacing, std::string &scratch)
{
	if (maxWidth <= 0 || GetLineWidth(text, font, spacing) <= maxWidth)
		return text;
	constexpr string_view Ellipsis = "...";
	const int ellipsisWidth = GetLineWidth(Ellipsis, font, spacing);
	string_view head = text;
	// TruncateUtf8 backs up to a code point boundary, so this always shrinks and always terminates.
	while (!head.empty() && GetLineWidth(head, font, spacing) + ellipsisWidth > maxWidth)
		head = TruncateUtf8(head, head.size() - 1);
	scratch.assign(head);
	scratch.append(Ellipsis);
	return scratch;
}

void Render(const UiText &uiText)
{
	const Surface &out = Surface(DiabloUiSurface());
	DrawString(out, uiText.GetText(), MakeRectangle(uiText.m_rect), { uiText.GetFlags() | UiFlags::FontSizeDialog | CenteredInBox });
}

void Render(const UiArtText &uiArtText)
{
	const Surface &out = Surface(DiabloUiSurface());
	DrawString(out, uiArtText.GetText(), MakeRectangle(uiArtText.m_rect), { uiArtText.GetFlags() | CenteredInBox, uiArtText.GetSpacing(), uiArtText.GetLineHeight() });
}

void Render(const UiImageClx &uiImage)
{
	ClxSprite sprite = uiImage.sprite();
	int x = uiImage.m_rect.x;
	if (uiImage.isCentered()) {
		x += GetCenterOffset(sprite.width(), uiImage.m_rect.w);
	}
	RenderClxSprite(Surface(DiabloUiSurface()), sprite, { x, uiImage.m_rect.y });
}

void Render(const UiImageAnimatedClx &uiImage)
{
	ClxSprite sprite = uiImage.sprite(GetAnimationFrame(uiImage.numFrames()));
	int x = uiImage.m_rect.x;
	if (uiImage.isCentered()) {
		x += GetCenterOffset(sprite.width(), uiImage.m_rect.w);
	}
	RenderClxSprite(Surface(DiabloUiSurface()), sprite, { x, uiImage.m_rect.y });
}

void Render(const UiArtTextButton &uiButton)
{
	const Surface &out = Surface(DiabloUiSurface());

	// Here rather than in each screen's own draw, which is where the three private focus rings had to
	// put it: gUiItems is rendered LAST in a frame (UiPollAndRender), so a screen that drew the
	// pentagrams itself before that had them painted over by its own background. Drawn from inside the
	// last pass, the problem those files each worked around does not arise.
	if (SelectedButton >= 0 && SelectedButton < static_cast<int>(gUiButtons.size())
	    && gUiButtons[SelectedButton] == &uiButton)
		DrawSelector(uiButton.m_rect);

	// LabelRect, not the whole button: the pentagrams stand in the row's two ends. No truncation on a
	// button - its labels are fixed strings that were measured to fit (see LabelRect).
	DrawString(out, uiButton.GetText(), LabelRect(uiButton.m_rect), { uiButton.GetFlags() | CenteredInBox });
}

void Render(const UiList &uiList)
{
	const Surface &out = Surface(DiabloUiSurface());
	std::string scratch; // holds an ellipsised label for as long as DrawString needs it

	for (std::size_t i = listOffset; i < uiList.m_vecItems.size() && (i - listOffset) < ListViewportSize; ++i) {
		SDL_Rect rect = uiList.itemRect(i - listOffset);
		const UiListItem &item = *uiList.GetItem(i);
		// LabelRect, not the row: the pentagrams own the row's two ends. CenteredInBox then puts the
		// label on the same centre line DrawSelector uses, so on a row taller than its text the two
		// stop disagreeing about where the middle is. On the main menu that is a 44px disagreement.
		const Rectangle rectangle = LabelRect(rect);
		const UiFlags flags = uiList.GetFlags() | item.uiFlags | CenteredInBox;
		const bool focused = i == SelectedItem && !UiListSelectorHidden;

		// Before the text, not after: with the label now inset between them nothing overlaps either
		// way, and this only decides which is on top in the degenerate case where LabelRect gave up
		// because the row was too narrow to inset at all.
		if (focused)
			DrawSelector(rect);

		if (item.args.empty()) {
			// Multi-line bodies are left alone: they arrive pre-wrapped (see CenteredInBox above), so
			// GetLineWidth would measure only the first line and "fitting" it would be meaningless.
			const string_view text = item.m_text.find('\n') == string_view::npos
			    ? FitToWidth(item.m_text, rectangle.size.width, GetFontSizeFromUiFlags(flags), uiList.GetSpacing(), scratch)
			    : item.m_text;
			DrawString(out, text, rectangle, { flags, uiList.GetSpacing(), uiList.GetLineHeight() });
		} else {
			DrawStringWithColors(out, item.m_text, item.args, rectangle, { flags, uiList.GetSpacing(), uiList.GetLineHeight() });
		}
	}
}

/**
 * @brief The theme's scrollbar - a narrow recessed groove with a bevelled thumb.
 *
 * Oracool: user request - "put the same elegant scrol bar which you use for Char Stats ui window".
 * The vanilla art (a 25px tiled channel between two arrow buttons) is the last piece of the old
 * front-end furniture on these screens, and next to a painted background it reads as a control bolted
 * on rather than part of the window.
 *
 * The colours below are not an approximation of the in-game bar - they are the SAME colours. The
 * character sheet draws its groove and thumb in indices sampled from `textbox_frame00`, and the warm
 * ramp those come from exists in the front-end palette too, just at different indices. Each constant
 * here is the entry in `ui_art\diablo.pal` whose RGB is identical to the in-game one, so the two bars
 * are the same bar rather than a pair that merely look alike:
 *
 *   in-game 204 -> here 188   (57,49,29)    the shadow the bevel puts on a left edge
 *   in-game 202 -> here 186   (91,81,52)    its mid-gold body
 *   in-game 198 -> here 182   (152,139,93)  the highlight on a right edge
 *
 * The groove is the one thing that had to change rather than move. In game it is `DrawThemedFill`, a
 * half-transparent darkening of the panel behind it - and blending is unavailable in a menu, since
 * `paletteTransparencyLookup` still holds whatever the last dungeon palette generated (see
 * DrawFocusGlow for the same problem). It is drawn solid in the ornate frame's own near-black
 * instead, which is the colour that theme already uses to separate the frame from what it surrounds.
 *
 * The widget's rect is left alone: it is the mouse target, and every hit test in
 * `HandleMouseEventScrollBar` is derived from it. The rect is sized for the hand, the bar for the eye,
 * and the thumb is drawn at `ThumbRect`'s own position so the two never disagree about where it is.
 * The arrow buttons are no longer drawn but their zones still scroll when clicked - the top of the
 * bar scrolls up, which is what a player would expect of it anyway.
 */
void Render(const UiScrollbar &uiSb)
{
	constexpr uint8_t GrooveColor = 191;    // (20,11,0) - the ornate frame's innermost ring
	constexpr uint8_t ShadowColor = 188;    // (57,49,29)
	constexpr uint8_t BodyColor = 186;      // (91,81,52)
	constexpr uint8_t HighlightColor = 182; // (152,139,93)
	constexpr int BarWidth = 3;             // oracool::OrnateBorderWidth, the theme's bevel thickness

	const Surface out = Surface(DiabloUiSurface());
	const int x = uiSb.m_rect.x + std::max(0, (uiSb.m_rect.w - BarWidth) / 2);

	// Only as tall as the thumb can travel, so the bar has no dead ends the thumb never reaches.
	const SDL_Rect track = BarRect(uiSb);
	for (int i = 0; i < BarWidth; i++)
		DrawVerticalLine(out, { x + i, track.y }, track.h, GrooveColor);

	if (SelectedItemMax > 0) {
		const SDL_Rect thumb = ThumbRect(uiSb, SelectedItem, SelectedItemMax + 1);
		// Left to right, matching how the bevel lights a left edge dark and a right edge bright.
		DrawVerticalLine(out, { x, thumb.y }, thumb.h, ShadowColor);
		DrawVerticalLine(out, { x + 1, thumb.y }, thumb.h, BodyColor);
		DrawVerticalLine(out, { x + 2, thumb.y }, thumb.h, HighlightColor);
	}
}

void Render(const UiEdit &uiEdit)
{
	// To simulate padding we inset the region used to draw text in an edit control.
	//
	// 43 -> 12: the old figure was not padding, it was clearance for the two pentagram cursors that
	// used to flank the box, and they are gone. On the hero-name box that reclaims 62px, which is what
	// lets a full 15-character name fit now that the box sits in the 320px list column rather than a
	// 400px centred one.
	Rectangle rect = MakeRectangle(uiEdit.m_rect).inset({ 12, 1 });

	const Surface &out = Surface(DiabloUiSurface());

	// No selector on the edit box, and deliberately: the pentagrams came back to the lists but not to
	// here. An edit box is the only interactive thing on its screen, so marking it "selected" says
	// nothing - and the 12px inset above IS the clearance they used to need. Flanking the box again
	// would mean going back to 43 and giving up the 62px that lets a full 15-character name fit.
	DrawString(out, uiEdit.m_value, rect,
	    {
	        uiEdit.GetFlags() | CenteredInBox,
	        /*spacing=*/1,
	        /*lineHeight=*/-1,
	        /*cursorPosition=*/static_cast<int>(uiEdit.m_cursor.position),
	        /*highlightRange=*/ { static_cast<int>(uiEdit.m_cursor.selection.begin), static_cast<int>(uiEdit.m_cursor.selection.end) },
	        /*highlightColor=*/126,
	    });
}

bool HandleMouseEventArtTextButton(const SDL_Event &event, const UiArtTextButton *uiButton)
{
	if (event.type != SDL_MOUSEBUTTONUP || event.button.button != SDL_BUTTON_LEFT) {
		return false;
	}
	if (!IsFocusable(*uiButton))
		return false;

	// Rule 2. The first click marks the button; only a second one on the SAME button acts. A genuine
	// double-click needs no special case - it is simply those two clicks arriving quickly.
	if (ArmedItem != uiButton) {
		ArmedItem = uiButton;
		ArmedIndex = 0;
		for (int i = 0; i < static_cast<int>(gUiButtons.size()); i++) {
			if (gUiButtons[i] == uiButton) {
				FocusButton(i);
				break;
			}
		}
		UiPlayMoveSound();
		return true;
	}

	DisarmClicks();
	uiButton->Activate();
	return true;
}

// dbClickTimer is gone with the double-click it timed: arming (see below) needs no clock, so the SDL1
// and SDL2 paths - which differed only in how they asked "was that a double-click" - are now one.

bool HandleMouseEventList(const SDL_Event &event, UiList *uiList)
{
	if (event.button.button != SDL_BUTTON_LEFT)
		return false;

	if (event.type != SDL_MOUSEBUTTONUP && event.type != SDL_MOUSEBUTTONDOWN)
		return false;

	// "None" is a real answer now: with the main menu's rows scattered across a painting there is
	// scenery between them that belongs to no row, and a click there must fall through rather than
	// pick whichever row shares its y.
	const std::optional<std::size_t> hit = uiList->itemAt(event.button.x, event.button.y);
	if (!hit)
		return false;
	std::size_t index = *hit;
	if (event.type == SDL_MOUSEBUTTONDOWN) {
		uiList->Press(index);
		return true;
	}

	if (event.type == SDL_MOUSEBUTTONUP && !uiList->IsPressed(index))
		return false;

	index += listOffset;
	// Both halves matter. An EMPTY list has SelectedItemMax 0, so the bound alone would let index 0
	// through into GetItem on a vector with nothing in it - the same underflow UiInitList guards, from
	// the other side. And a disabled row is not a target: the old code checked that only on the branch
	// that acted, so a click could still move the rule onto a locked difficulty.
	if (uiList->m_vecItems.empty() || index > static_cast<std::size_t>(SelectedItemMax))
		return false;
	if (HasAnyOf(uiList->GetItem(index)->uiFlags, UiFlags::ElementHidden | UiFlags::ElementDisabled))
		return false;

	// Rule 2, and the reason this is no longer three branches. What stood here focused a row on the
	// first click and acted on a genuine double-click - but only when the screen had a focus callback;
	// `gfnListFocus == nullptr` fell straight through to the action. The main menu, the class list and
	// the difficulty picker all pass nullptr, so on exactly the screens the rule is about, one click
	// started a game. Arming makes the two paths one: the first click marks the row wherever it came
	// from, the second acts.
	if (ArmedItem != uiList || ArmedIndex != index) {
		ArmedItem = uiList;
		ArmedIndex = index;
		// Off the button row, if a click had left it there, so the screen marks one place at a time.
		FocusButton(-1);
		if (SelectedItem != index)
			UiFocus(index, true, false);
		else
			UiPlayMoveSound();
		return true;
	}

	DisarmClicks();
	SelectedItem = index;
	UiFocusNavigationSelect();
	return true;
}

bool HandleMouseEventScrollBar(const SDL_Event &event, const UiScrollbar *uiSb)
{
	if (event.button.button != SDL_BUTTON_LEFT)
		return false;
	if (event.type == SDL_MOUSEBUTTONUP) {
		if (scrollBarState.upArrowPressed && IsInsideRect(event, UpArrowRect(*uiSb))) {
			UiFocusUp();
			return true;
		}
		if (scrollBarState.downArrowPressed && IsInsideRect(event, DownArrowRect(*uiSb))) {
			UiFocusDown();
			return true;
		}
	} else if (event.type == SDL_MOUSEBUTTONDOWN) {
		if (IsInsideRect(event, BarRect(*uiSb))) {
			// Scroll up or down based on thumb position.
			const SDL_Rect thumbRect = ThumbRect(*uiSb, SelectedItem, SelectedItemMax + 1);
			if (event.button.y < thumbRect.y) {
				UiFocusPageUp();
			} else if (event.button.y > thumbRect.y + thumbRect.h) {
				UiFocusPageDown();
			}
			return true;
		}
		if (IsInsideRect(event, UpArrowRect(*uiSb))) {
			scrollBarState.upArrowPressed = true;
			return true;
		}
		if (IsInsideRect(event, DownArrowRect(*uiSb))) {
			scrollBarState.downArrowPressed = true;
			return true;
		}
	}
	return false;
}

bool HandleMouseEvent(const SDL_Event &event, UiItemBase *item)
{
	if (item->IsNotInteractive() || !IsInsideRect(event, item->m_rect))
		return false;
	switch (item->GetType()) {
	case UiType::ArtTextButton:
		return HandleMouseEventArtTextButton(event, static_cast<UiArtTextButton *>(item));
	case UiType::Button:
		return HandleMouseEventButton(event, static_cast<UiButton *>(item));
	case UiType::List:
		return HandleMouseEventList(event, static_cast<UiList *>(item));
	case UiType::Scrollbar:
		return HandleMouseEventScrollBar(event, static_cast<UiScrollbar *>(item));
	default:
		return false;
	}
}

} // namespace

void LoadPalInMem(const SDL_Color *pPal)
{
	for (int i = 0; i < 256; i++) {
		orig_palette[i] = pPal[i];
	}
}

void UiRenderItem(const UiItemBase &item)
{
	if (item.IsHidden())
		return;
	switch (item.GetType()) {
	case UiType::Text:
		Render(static_cast<const UiText &>(item));
		break;
	case UiType::ArtText:
		Render(static_cast<const UiArtText &>(item));
		break;
	case UiType::ImageClx:
		Render(static_cast<const UiImageClx &>(item));
		break;
	case UiType::ImageAnimatedClx:
		Render(static_cast<const UiImageAnimatedClx &>(item));
		break;
	case UiType::ArtTextButton:
		Render(static_cast<const UiArtTextButton &>(item));
		break;
	case UiType::Button:
		RenderButton(static_cast<const UiButton &>(item));
		break;
	case UiType::List:
		Render(static_cast<const UiList &>(item));
		break;
	case UiType::Scrollbar:
		Render(static_cast<const UiScrollbar &>(item));
		break;
	case UiType::Edit:
		Render(static_cast<const UiEdit &>(item));
		break;
	}
}

void UiRenderItems(const std::vector<UiItemBase *> &items)
{
	for (const UiItemBase *item : items)
		UiRenderItem(*item);
}

void UiRenderItems(const std::vector<std::unique_ptr<UiItemBase>> &items)
{
	for (const std::unique_ptr<UiItemBase> &item : items)
		UiRenderItem(*item);
}

bool UiItemMouseEvents(SDL_Event *event, const std::vector<UiItemBase *> &items)
{
	if (items.empty()) {
		return false;
	}

	// In SDL2 mouse events already use logical coordinates.
#ifdef USE_SDL1
	OutputToLogical(&event->button.x, &event->button.y);
#endif

	bool handled = false;
	for (const auto &item : items) {
		if (HandleMouseEvent(*event, item)) {
			handled = true;
			break;
		}
	}

	if (event->type == SDL_MOUSEBUTTONUP && event->button.button == SDL_BUTTON_LEFT) {
		scrollBarState.downArrowPressed = scrollBarState.upArrowPressed = false;
		for (const auto &item : items) {
			if (item->IsType(UiType::Button)) {
				HandleGlobalMouseUpButton(static_cast<UiButton *>(item));
			} else if (item->IsType(UiType::List)) {
				static_cast<UiList *>(item)->Release();
			}
		}
	}

	return handled;
}

bool UiItemMouseEvents(SDL_Event *event, const std::vector<std::unique_ptr<UiItemBase>> &items)
{
	if (items.empty()) {
		return false;
	}

	// In SDL2 mouse events already use logical coordinates.
#ifdef USE_SDL1
	OutputToLogical(&event->button.x, &event->button.y);
#endif

	bool handled = false;
	for (const auto &item : items) {
		if (HandleMouseEvent(*event, item.get())) {
			handled = true;
			break;
		}
	}

	if (event->type == SDL_MOUSEBUTTONUP && event->button.button == SDL_BUTTON_LEFT) {
		scrollBarState.downArrowPressed = scrollBarState.upArrowPressed = false;
		for (const auto &item : items) {
			if (item->IsType(UiType::Button)) {
				HandleGlobalMouseUpButton(static_cast<UiButton *>(item.get()));
			} else if (item->IsType(UiType::List)) {
				static_cast<UiList *>(item.get())->Release();
			}
		}
	}

	return handled;
}

void DrawMouse()
{
	if (ControlDevice != ControlTypes::KeyboardAndMouse || IsHardwareCursor() || !ArtCursor)
		return;
	RenderClxSprite(Surface(DiabloUiSurface()), (*ArtCursor)[0], MousePosition);
}
} // namespace devilution
