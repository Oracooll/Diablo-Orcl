#include "controls/game_controls.h"

#include <cstdint>

#include "controls/controller_motion.h"
#ifndef USE_SDL1
#include "controls/devices/game_controller.h"
#endif
#include "control.h" // IsModalPromptOpen
#include "controls/devices/joystick.h"
#include "controls/plrctrls.h"
#include "controls/touch/gamepad.h"
#include "doom.h"
#include "gamemenu.h"
#include "gmenu.h"
#include "inv.h"
#include "options.h"
#include "panels/spell_book.hpp" // HandleAbilityFKey - the pad's quick-spell menu
#include "player.h"
#include "qol/stash.h"
#include "spells.h" // IsValidSpell
#include "stores.h"

namespace devilution {

bool PadMenuNavigatorActive = false;
bool PadHotspellMenuActive = false;
ControllerButton SuppressedButton = ControllerButton_NONE;

namespace {

SDL_Keycode TranslateControllerButtonToGameMenuKey(ControllerButton controllerButton)
{
	switch (TranslateTo(GamepadType, controllerButton)) {
	case ControllerButton_BUTTON_A:
	case ControllerButton_BUTTON_Y:
		return SDLK_RETURN;
	case ControllerButton_BUTTON_B:
	case ControllerButton_BUTTON_BACK:
	case ControllerButton_BUTTON_START:
		return SDLK_ESCAPE;
	case ControllerButton_BUTTON_LEFTSTICK:
		return SDLK_TAB; // Map
	default:
		return SDLK_UNKNOWN;
	}
}

SDL_Keycode TranslateControllerButtonToMenuKey(ControllerButton controllerButton)
{
	switch (TranslateTo(GamepadType, controllerButton)) {
	case ControllerButton_BUTTON_A:
		return SDLK_SPACE;
	case ControllerButton_BUTTON_B:
	case ControllerButton_BUTTON_BACK:
	case ControllerButton_BUTTON_START:
		return SDLK_ESCAPE;
	case ControllerButton_BUTTON_Y:
		return SDLK_RETURN;
	case ControllerButton_BUTTON_LEFTSTICK:
		return SDLK_TAB; // Map
	case ControllerButton_BUTTON_DPAD_LEFT:
		return SDLK_LEFT;
	case ControllerButton_BUTTON_DPAD_RIGHT:
		return SDLK_RIGHT;
	case ControllerButton_BUTTON_DPAD_UP:
		return SDLK_UP;
	case ControllerButton_BUTTON_DPAD_DOWN:
		return SDLK_DOWN;
	default:
		return SDLK_UNKNOWN;
	}
}

SDL_Keycode TranslateControllerButtonToQuestLogKey(ControllerButton controllerButton)
{
	switch (TranslateTo(GamepadType, controllerButton)) {
	case ControllerButton_BUTTON_A:
	case ControllerButton_BUTTON_Y:
		return SDLK_RETURN;
	case ControllerButton_BUTTON_B:
		return SDLK_SPACE;
	case ControllerButton_BUTTON_LEFTSTICK:
		return SDLK_TAB; // Map
	default:
		return SDLK_UNKNOWN;
	}
}

SDL_Keycode TranslateControllerButtonToSpellbookKey(ControllerButton controllerButton)
{
	switch (TranslateTo(GamepadType, controllerButton)) {
	// Not B as Space (round 87 audit): Space is the master closer, so the button shut every window, and B is the
	// padmapper's Primary action - it now reaches PerformPrimaryAction, which clicks the Abilities window where the
	// cursor is (invest, bind, turn a sheet), while the Cancel action closes the window alone (Esc closes the top window
	// only). Not Y as Return (round 92 audit): with the chat line on in single-player, it opened a text box a pad cannot
	// type in. Nor the D-pad as arrows: SpellBookMove takes it with the stick and walks the window's focus.
	case ControllerButton_BUTTON_LEFTSTICK:
		return SDLK_TAB; // Map
	default:
		return SDLK_UNKNOWN;
	}
}

bool GetGameAction(const SDL_Event &event, ControllerButtonEvent ctrlEvent, GameAction *action)
{
	const bool inGameMenu = InGameMenu();
	// Oracool (audit, 2026-08-26): the virtual gamepad builds its actions HERE rather than going
	// through the padmapper, so the modal guard added to CanPlayerTakeAction did not cover it -
	// a touch player could still attack, cast and quaff behind a drop-gold prompt. The same flag
	// also routes the pad's confirm and cancel to the prompt, which is the other half: blocking
	// input without offering a way out would just trap them in it.
	const bool modalPromptOpen = IsModalPromptOpen();

#ifndef USE_SDL1
	if (ControlMode == ControlTypes::VirtualGamepad) {
		if (event.type == SDL_FINGERDOWN) {
			if (VirtualGamepadState.menuPanel.charButton.isHeld && VirtualGamepadState.menuPanel.charButton.didStateChange) {
				*action = GameAction(GameActionType_TOGGLE_CHARACTER_INFO);
				return true;
			}
			if (VirtualGamepadState.menuPanel.questsButton.isHeld && VirtualGamepadState.menuPanel.questsButton.didStateChange) {
				*action = GameAction(GameActionType_TOGGLE_QUEST_LOG);
				return true;
			}
			if (VirtualGamepadState.menuPanel.inventoryButton.isHeld && VirtualGamepadState.menuPanel.inventoryButton.didStateChange) {
				*action = GameAction(GameActionType_TOGGLE_INVENTORY);
				return true;
			}
			if (VirtualGamepadState.menuPanel.mapButton.isHeld && VirtualGamepadState.menuPanel.mapButton.didStateChange) {
				*action = GameActionSendKey { SDLK_TAB, false };
				return true;
			}
			if (VirtualGamepadState.primaryActionButton.isHeld && VirtualGamepadState.primaryActionButton.didStateChange) {
				if (!modalPromptOpen && !inGameMenu && !QuestLogIsOpen && !sbookflag) {
					*action = GameAction(GameActionType_PRIMARY_ACTION);
					if (ControllerActionHeld == GameActionType_NONE) {
						ControllerActionHeld = GameActionType_PRIMARY_ACTION;
					}
				} else if (modalPromptOpen || sgpCurrentMenu != nullptr || stextflag != TalkID::None || QuestLogIsOpen) {
					*action = GameActionSendKey { SDLK_RETURN, false };
				} else {
					*action = GameActionSendKey { SDLK_SPACE, false };
				}
				return true;
			}
			if (VirtualGamepadState.secondaryActionButton.isHeld && VirtualGamepadState.secondaryActionButton.didStateChange) {
				if (!modalPromptOpen && !inGameMenu && !QuestLogIsOpen && !sbookflag) {
					*action = GameAction(GameActionType_SECONDARY_ACTION);
					if (ControllerActionHeld == GameActionType_NONE)
						ControllerActionHeld = GameActionType_SECONDARY_ACTION;
				}
				return true;
			}
			if (VirtualGamepadState.spellActionButton.isHeld && VirtualGamepadState.spellActionButton.didStateChange) {
				if (!modalPromptOpen && !inGameMenu && !QuestLogIsOpen && !sbookflag) {
					*action = GameAction(GameActionType_CAST_SPELL);
					if (ControllerActionHeld == GameActionType_NONE)
						ControllerActionHeld = GameActionType_CAST_SPELL;
				}
				return true;
			}
			if (VirtualGamepadState.cancelButton.isHeld && VirtualGamepadState.cancelButton.didStateChange) {
				if (modalPromptOpen || inGameMenu || DoomFlag || spselflag)
					*action = GameActionSendKey { SDLK_ESCAPE, false };
				else if (invflag)
					*action = GameAction(GameActionType_TOGGLE_INVENTORY);
				else if (sbookflag)
					*action = GameAction(GameActionType_TOGGLE_SPELL_BOOK);
				else if (QuestLogIsOpen)
					*action = GameAction(GameActionType_TOGGLE_QUEST_LOG);
				else if (chrflag)
					*action = GameAction(GameActionType_TOGGLE_CHARACTER_INFO);
				return true;
			}
			if (VirtualGamepadState.healthButton.isHeld && VirtualGamepadState.healthButton.didStateChange) {
				if (!modalPromptOpen && !QuestLogIsOpen && !sbookflag && stextflag == TalkID::None)
					*action = GameAction(GameActionType_USE_HEALTH_POTION);
				return true;
			}
			if (VirtualGamepadState.manaButton.isHeld && VirtualGamepadState.manaButton.didStateChange) {
				if (!modalPromptOpen && !QuestLogIsOpen && !sbookflag && stextflag == TalkID::None)
					*action = GameAction(GameActionType_USE_MANA_POTION);
				return true;
			}
		} else if (event.type == SDL_FINGERUP) {
			if ((!VirtualGamepadState.primaryActionButton.isHeld && ControllerActionHeld == GameActionType_PRIMARY_ACTION)
			    || (!VirtualGamepadState.secondaryActionButton.isHeld && ControllerActionHeld == GameActionType_SECONDARY_ACTION)
			    || (!VirtualGamepadState.spellActionButton.isHeld && ControllerActionHeld == GameActionType_CAST_SPELL)) {
				ControllerActionHeld = GameActionType_NONE;
				LastMouseButtonAction = MouseActionType::None;
			}
		}
	}
#endif

	if (PadMenuNavigatorActive || PadHotspellMenuActive)
		return false;

	SDL_Keycode translation = SDLK_UNKNOWN;

	// Oracool (audit, 2026-08-26): a numeric prompt - drop gold, withdraw gold, "Refresh Until" -
	// was reachable by nothing on a controller. CanPlayerTakeAction now refuses gameplay actions
	// while one is open, which stopped the pad acting THROUGH the prompt but left it with no way to
	// answer the prompt either: a player who opened one with a controller was stuck.
	//
	// The game-menu translation is exactly the right mapping already - A confirms, B cancels - and
	// the prompt reads those two keys, so it is reused rather than duplicated.
	if (IsModalPromptOpen())
		translation = TranslateControllerButtonToGameMenuKey(ctrlEvent.button);
	else if (gmenu_is_active() || stextflag != TalkID::None)
		translation = TranslateControllerButtonToGameMenuKey(ctrlEvent.button);
	else if (inGameMenu)
		translation = TranslateControllerButtonToMenuKey(ctrlEvent.button);
	else if (QuestLogIsOpen)
		translation = TranslateControllerButtonToQuestLogKey(ctrlEvent.button);
	else if (sbookflag)
		translation = TranslateControllerButtonToSpellbookKey(ctrlEvent.button);

	if (translation != SDLK_UNKNOWN) {
		*action = GameActionSendKey { static_cast<uint32_t>(translation), ctrlEvent.up };
		return true;
	}

	return false;
}

void PressControllerButton(ControllerButton button)
{
	if (IsStashOpen) {
		switch (button) {
		case ControllerButton_BUTTON_BACK:
			StartGoldWithdraw();
			return;
		case ControllerButton_BUTTON_LEFTSHOULDER:
			Stash.PreviousPage();
			return;
		case ControllerButton_BUTTON_RIGHTSHOULDER:
			Stash.NextPage();
			return;
		default:
			break;
		}
	} else if (invflag) {
		// The backpack's pages 2-10 (round 87 audit): with the backpack open the shoulders turn its pages, as they turn
		// the stash's, rather than drink a potion behind it. Locked pages are stepped over.
		switch (button) {
		case ControllerButton_BUTTON_LEFTSHOULDER:
			StepInventoryPage(-1);
			return;
		case ControllerButton_BUTTON_RIGHTSHOULDER:
			StepInventoryPage(1);
			return;
		default:
			break;
		}
	}

	if (PadHotspellMenuActive) {
		auto quickSpellAction = [](size_t slot) {
			if (spselflag) {
				SetSpeedSpell(slot);
				return;
			}
			// The F-key's own road (round 87 audit): an aura on the key lights, a left-button binding readies the left
			// button. ToggleSpell and QuickCast know only the right button's array, so both were ignored. Quick cast still
			// casts a right-button binding outright, as before.
			const Player &me = *MyPlayer;
			if (*sgOptions.Gameplay.quickCast && me._pAuraHotKey[slot] == 0xFFFF && IsValidSpell(me._pSplHotKey[slot]))
				QuickCast(slot);
			else
				HandleAbilityFKey(slot, /*shift=*/false);
		};
		switch (button) {
		case devilution::ControllerButton_BUTTON_A:
			quickSpellAction(2);
			return;
		case devilution::ControllerButton_BUTTON_B:
			quickSpellAction(3);
			return;
		case devilution::ControllerButton_BUTTON_X:
			quickSpellAction(0);
			return;
		case devilution::ControllerButton_BUTTON_Y:
			quickSpellAction(1);
			return;
		default:
			break;
		}
	}

	if (PadMenuNavigatorActive) {
		switch (button) {
		case devilution::ControllerButton_BUTTON_DPAD_UP:
			PressEscKey();
			LastMouseButtonAction = MouseActionType::None;
			PadHotspellMenuActive = false;
			PadMenuNavigatorActive = false;
			gamemenu_on();
			return;
		case devilution::ControllerButton_BUTTON_DPAD_DOWN:
			DoAutoMap();
			return;
		case devilution::ControllerButton_BUTTON_DPAD_LEFT:
			ProcessGameAction(GameAction { GameActionType_TOGGLE_CHARACTER_INFO });
			return;
		case devilution::ControllerButton_BUTTON_DPAD_RIGHT:
			ProcessGameAction(GameAction { GameActionType_TOGGLE_INVENTORY });
			return;
		case devilution::ControllerButton_BUTTON_A:
			ProcessGameAction(GameAction { GameActionType_TOGGLE_SPELL_BOOK });
			return;
		case devilution::ControllerButton_BUTTON_B:
			return;
		case devilution::ControllerButton_BUTTON_X:
			ProcessGameAction(GameAction { GameActionType_TOGGLE_QUEST_LOG });
			return;
		case devilution::ControllerButton_BUTTON_Y:
#ifdef __3DS__
			ToggleDungeonZoom();
#endif
			return;
		default:
			break;
		}
	}

	sgOptions.Padmapper.ButtonPressed(button);
}

} // namespace

ControllerButton TranslateTo(GamepadLayout layout, ControllerButton button)
{
	if (layout != GamepadLayout::Nintendo)
		return button;

	switch (button) {
	case ControllerButton_BUTTON_A:
		return ControllerButton_BUTTON_B;
	case ControllerButton_BUTTON_B:
		return ControllerButton_BUTTON_A;
	case ControllerButton_BUTTON_X:
		return ControllerButton_BUTTON_Y;
	case ControllerButton_BUTTON_Y:
		return ControllerButton_BUTTON_X;
	default:
		return button;
	}
}

bool SkipsMovie(ControllerButtonEvent ctrlEvent)
{
	return IsAnyOf(ctrlEvent.button,
	    ControllerButton_BUTTON_A,
	    ControllerButton_BUTTON_B,
	    ControllerButton_BUTTON_START,
	    ControllerButton_BUTTON_BACK);
}

bool IsSimulatedMouseClickBinding(ControllerButtonEvent ctrlEvent)
{
	if (ctrlEvent.button == ControllerButton_NONE)
		return false;
	if (!ctrlEvent.up && ctrlEvent.button == SuppressedButton)
		return false;
	string_view actionName = sgOptions.Padmapper.ActionNameTriggeredByButtonEvent(ctrlEvent);
	return IsAnyOf(actionName, "LeftMouseClick1", "LeftMouseClick2", "RightMouseClick1", "RightMouseClick2");
}

AxisDirection GetMoveDirection()
{
	return GetLeftStickOrDpadDirection(true);
}

bool HandleControllerButtonEvent(const SDL_Event &event, const ControllerButtonEvent ctrlEvent, GameAction &action)
{
	if (ctrlEvent.button == ControllerButton_IGNORE) {
		return false;
	}

	struct ButtonReleaser {
		~ButtonReleaser()
		{
			if (ctrlEvent.up)
				sgOptions.Padmapper.ButtonReleased(ctrlEvent.button, false);
		}
		ControllerButtonEvent ctrlEvent;
	};

	const ButtonReleaser buttonReleaser { ctrlEvent };
	bool isGamepadMotion = IsControllerMotion(event);
	if (!isGamepadMotion) {
		SimulateRightStickWithPadmapper(ctrlEvent);
	}
	DetectInputMethod(event, ctrlEvent);
	if (isGamepadMotion) {
		return true;
	}

	if (ctrlEvent.button != ControllerButton_NONE && ctrlEvent.button == SuppressedButton) {
		if (!ctrlEvent.up)
			return true;
		SuppressedButton = ControllerButton_NONE;
	}

	if (ctrlEvent.up && sgOptions.Padmapper.ActionNameTriggeredByButtonEvent(ctrlEvent) != "") {
		// Button press may have brought up a menu;
		// don't confuse release of that button with intent to interact with the menu
		sgOptions.Padmapper.ButtonReleased(ctrlEvent.button);
		return true;
	} else if (GetGameAction(event, ctrlEvent, &action)) {
		ProcessGameAction(action);
		return true;
	} else if (ctrlEvent.button != ControllerButton_NONE) {
		if (!ctrlEvent.up)
			PressControllerButton(ctrlEvent.button);
		return true;
	}

	return false;
}

} // namespace devilution
