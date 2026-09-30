#include "selstart.h"

#include <algorithm>
#include <cstdint>
#include <vector>

#include <function_ref.hpp>

#include "DiabloUI/diabloui.h"
#include "DiabloUI/scrollbar.h"
#include "control.h"
#include "controls/controller_motion.h"
#include "controls/plrctrls.h"
#include "controls/remap_keyboard.h"
#include "engine/render/text_render.hpp"
#include "hwcursor.hpp"
#include "options.h"
#include "oracool/ui_backgrounds.h"
#include "utils/display.h"
#include "utils/language.h"
#include "utils/stdcompat/optional.hpp"
#include "utils/utf8.hpp"

namespace devilution {
namespace {

constexpr size_t IndexKeyOrPadInput = 1;
constexpr size_t IndexPadTimerText = 2;

bool endMenu = false;
bool backToMain = false;

std::vector<std::unique_ptr<UiListItem>> vecDialogItems;
std::vector<std::unique_ptr<UiItemBase>> vecDialog;
std::vector<OptionEntryBase *> vecOptions;
OptionCategoryBase *selectedCategory = nullptr;
OptionEntryBase *selectedOption = nullptr;

enum class ShownMenuType : uint8_t {
	Categories,
	Settings,
	ListOption,
	KeyInput,
	PadInput,
};

ShownMenuType shownMenu;

char optionDescription[512];

Rectangle rectList;
Rectangle rectDescription;

enum class SpecialMenuEntry : int8_t {
	None = -1,
	PreviousMenu = -2,
	UnbindKey = -3,
	BindPadButton = -4,
	UnbindPadButton = -5,
};

ControllerButtonCombo padEntryCombo {};
Uint32 padEntryStartTime = 0;
std::string padEntryTimerText;

bool IsValidEntry(OptionEntryBase *pOptionEntry)
{
	auto flags = pOptionEntry->GetFlags();
	if (HasAnyOf(flags, OptionEntryFlags::NeedDiabloMpq) && !HaveDiabdat())
		return false;
	if (HasAnyOf(flags, OptionEntryFlags::NeedHellfireMpq) && !HaveHellfire())
		return false;
	return HasNoneOf(flags, OptionEntryFlags::Invisible | (gbIsHellfire ? OptionEntryFlags::OnlyDiablo : OptionEntryFlags::OnlyHellfire));
}

std::vector<DrawStringFormatArg> CreateDrawStringFormatArgForEntry(OptionEntryBase *pEntry)
{
	return std::vector<DrawStringFormatArg> {
		{ pEntry->GetName().data(), UiFlags::ColorUiGold },
		{ pEntry->GetValueDescription().data(), UiFlags::ColorUiSilver }
	};
}

/** @brief Check if the option text can't fit in one list line (list width minus drawn selector) */
bool NeedsTwoLinesToDisplayOption(std::vector<DrawStringFormatArg> &formatArgs)
{
	return GetLineWidth("{}: {}", formatArgs.data(), formatArgs.size(), 0, GameFontTables::GameFont24, 1) >= (rectList.size.width - 90);
}

void CleanUpSettingsUI()
{
	UiInitList_clear();

	vecDialogItems.clear();
	vecDialog.clear();
	vecOptions.clear();

	ArtBackground = std::nullopt;
	ArtBackgroundWidescreen = std::nullopt;
	UnloadScrollBar();
}

void GoBackOneMenuLevel()
{
	endMenu = true;
	switch (shownMenu) {
	case ShownMenuType::Categories:
		backToMain = true;
		break;
	case ShownMenuType::Settings:
		shownMenu = ShownMenuType::Categories;
		break;
	default:
		shownMenu = ShownMenuType::Settings;
		break;
	}
}

void StartPadEntryTimer()
{
	padEntryCombo = ControllerButton_NONE;
	padEntryStartTime = SDL_GetTicks();
	if (padEntryStartTime == 0)
		padEntryStartTime++;
	// Removes access to these dialog items while entering bindings
	for (size_t i = IndexPadTimerText + 1; i < vecDialogItems.size(); i++)
		vecDialogItems[i]->uiFlags |= UiFlags::ElementHidden;
}

void StopPadEntryTimer()
{
	padEntryCombo = ControllerButton_NONE;
	padEntryStartTime = 0;
	padEntryTimerText = "";
	vecDialogItems[IndexPadTimerText]->m_text = padEntryTimerText;
	// Restores access to these dialog items after binding is complete
	for (size_t i = IndexPadTimerText + 1; i < vecDialogItems.size(); i++)
		vecDialogItems[i]->uiFlags &= ~UiFlags::ElementHidden;
}

void UpdatePadEntryTimerText()
{
	if (shownMenu != ShownMenuType::PadInput)
		return;
	Uint32 elapsed = SDL_GetTicks() - padEntryStartTime;
	if (padEntryStartTime == 0 || elapsed > 10000) {
		StopPadEntryTimer();
		return;
	}
	padEntryTimerText = StrCat(_("Press gamepad buttons to change."), " ", 10 - elapsed / 1000);
	vecDialogItems[IndexPadTimerText]->m_text = padEntryTimerText;
}

void UpdateDescription(const OptionEntryBase &option)
{
	auto paragraphs = WordWrapString(option.GetDescription(), rectDescription.size.width, GameFont12, 1);
	CopyUtf8(optionDescription, paragraphs, sizeof(optionDescription));
}

void UpdateDescription(const OptionCategoryBase &category)
{
	auto paragraphs = WordWrapString(category.GetDescription(), rectDescription.size.width, GameFont12, 1);
	CopyUtf8(optionDescription, paragraphs, sizeof(optionDescription));
}

void ItemFocused(int value)
{
	switch (shownMenu) {
	case ShownMenuType::Categories: {
		auto &vecItem = vecDialogItems[value];
		optionDescription[0] = '\0';
		if (vecItem->m_value < 0)
			return;
		auto *pCategory = sgOptions.GetCategories()[vecItem->m_value];
		UpdateDescription(*pCategory);
	} break;
	case ShownMenuType::Settings: {
		auto &vecItem = vecDialogItems[value];
		optionDescription[0] = '\0';
		if (vecItem->m_value < 0)
			return;
		auto *pOption = vecOptions[vecItem->m_value];
		UpdateDescription(*pOption);
	} break;
	default:
		break;
	}
}

bool ChangeOptionValue(OptionEntryBase *pOption, size_t listIndex)
{
	if (HasAnyOf(pOption->GetFlags(), OptionEntryFlags::RecreateUI)) {
		endMenu = true;
		// Clean up all UI related Data
		CleanUpSettingsUI();
		UnloadUiGFX();
		FreeItemGFX();
		selectedOption = pOption;
	}

	switch (pOption->GetType()) {
	case OptionEntryType::Boolean: {
		auto *pOptionBoolean = static_cast<OptionEntryBoolean *>(pOption);
		pOptionBoolean->SetValue(!**pOptionBoolean);
	} break;
	case OptionEntryType::List: {
		auto *pOptionList = static_cast<OptionEntryListBase *>(pOption);
		pOptionList->SetActiveListIndex(listIndex);
	} break;
	case OptionEntryType::Key:
	case OptionEntryType::PadButton:
		break;
	}

	if (HasAnyOf(pOption->GetFlags(), OptionEntryFlags::RecreateUI)) {
		// Reinitialize UI with changed settings (for example game mode, language or resolution)
		UiInitialize();
		InitItemGFX();
		SetHardwareCursor(CursorInfo::UnknownCursor());
		return false;
	}

	return true;
}

void ItemSelected(int value)
{
	const auto index = static_cast<size_t>(value);
	auto &vecItem = vecDialogItems[index];
	int vecItemValue = vecItem->m_value;
	if (vecItemValue < 0) {
		auto specialMenuEntry = static_cast<SpecialMenuEntry>(vecItemValue);
		switch (specialMenuEntry) {
		case SpecialMenuEntry::None:
			break;
		case SpecialMenuEntry::PreviousMenu:
			GoBackOneMenuLevel();
			break;
		case SpecialMenuEntry::UnbindKey: {
			auto *pOptionKey = static_cast<KeymapperOptions::Action *>(selectedOption);
			pOptionKey->SetValue(SDLK_UNKNOWN);
			vecDialogItems[IndexKeyOrPadInput]->m_text = selectedOption->GetValueDescription().data();
			break;
		}
		case SpecialMenuEntry::BindPadButton:
			StartPadEntryTimer();
			break;
		case SpecialMenuEntry::UnbindPadButton:
			auto *pOptionPad = static_cast<PadmapperOptions::Action *>(selectedOption);
			pOptionPad->SetValue(ControllerButton_NONE);
			vecDialogItems[IndexKeyOrPadInput]->m_text = selectedOption->GetValueDescription().data();
			break;
		}
		return;
	}

	switch (shownMenu) {
	case ShownMenuType::Categories: {
		selectedCategory = sgOptions.GetCategories()[vecItemValue];
		endMenu = true;
		shownMenu = ShownMenuType::Settings;
	} break;
	case ShownMenuType::Settings: {
		auto *pOption = vecOptions[vecItemValue];
		bool updateValueDescription = false;
		if (pOption->GetType() == OptionEntryType::List) {
			auto *pOptionList = static_cast<OptionEntryListBase *>(pOption);
			if (pOptionList->GetListSize() > 2) {
				selectedOption = pOption;
				endMenu = true;
				shownMenu = ShownMenuType::ListOption;
			} else {
				// If the list contains only two items, we don't show a submenu and instead change the option value instantly
				size_t nextIndex = pOptionList->GetActiveListIndex() + 1;
				if (nextIndex >= pOptionList->GetListSize())
					nextIndex = 0;
				updateValueDescription = ChangeOptionValue(pOption, nextIndex);
			}
		} else if (pOption->GetType() == OptionEntryType::Key) {
			selectedOption = pOption;
			endMenu = true;
			shownMenu = ShownMenuType::KeyInput;
		} else if (pOption->GetType() == OptionEntryType::PadButton) {
			selectedOption = pOption;
			endMenu = true;
			shownMenu = ShownMenuType::PadInput;
		} else {
			updateValueDescription = ChangeOptionValue(pOption, 0);
		}
		if (updateValueDescription) {
			auto args = CreateDrawStringFormatArgForEntry(pOption);
			bool optionUsesTwoLines = ((index + 1) < vecDialogItems.size() && vecDialogItems[index]->m_value == vecDialogItems[index + 1]->m_value);
			if (NeedsTwoLinesToDisplayOption(args) != optionUsesTwoLines) {
				selectedOption = pOption;
				endMenu = true;
			} else {
				vecItem->args.clear();
				for (auto &arg : args)
					vecItem->args.push_back(arg);
				if (optionUsesTwoLines) {
					vecDialogItems[index + 1]->m_text = pOption->GetValueDescription().data();
				}
			}
		}
	} break;
	case ShownMenuType::ListOption: {
		ChangeOptionValue(selectedOption, vecItemValue);
		GoBackOneMenuLevel();
	} break;
	case ShownMenuType::KeyInput:
	case ShownMenuType::PadInput:
		break;
	}
}

void EscPressed()
{
	GoBackOneMenuLevel();
}

void FullscreenChanged()
{
	auto *fullscreenOption = &sgOptions.Graphics.fullscreen;

	for (auto &vecItem : vecDialogItems) {
		int vecItemValue = vecItem->m_value;
		if (vecItemValue < 0 || static_cast<size_t>(vecItemValue) >= vecOptions.size())
			continue;

		auto *pOption = vecOptions[vecItemValue];
		if (pOption != fullscreenOption)
			continue;

		vecItem->args.clear();
		for (auto &arg : CreateDrawStringFormatArgForEntry(pOption))
			vecItem->args.push_back(arg);
		break;
	}
}

} // namespace

void UiSettingsMenu()
{
	backToMain = false;
	shownMenu = ShownMenuType::Categories;
	selectedCategory = nullptr;
	selectedOption = nullptr;

	do {
		endMenu = false;

		// For the settings menu, we use the full height and allow some more width.
		const int uiWidth = std::clamp<int>(gnScreenWidth, 640, 720);
		const Rectangle uiRectangle = {
			{ (gnScreenWidth - uiWidth) / 2, 0 },
			{ uiWidth, gnScreenHeight }
		};

		// UiLoadBlackBackground still runs: it is what loads this screen's palette
		// (ui_art\diablo.pal), which the background below has to quantize against. It leaves
		// ArtBackground empty, which is why this screen was a black plate - so there is nothing to
		// replace here, only something to add. UiAddBackground stays as the fallback for a missing
		// asset, where it does exactly what it did before: nothing.
		UiLoadBlackBackground();
		LoadScrollBar();
		if (!oracool::AddUiBackground(&vecDialog, oracool::UiBackground::Settings))
			UiAddBackground(&vecDialog);
		// The logo and the title used to be pushed here, hanging from the screen's top edge. They
		// move with the rest of the block now, so they are pushed after its height is known - see
		// blockTop below. Only the background can be added before that, because it has to be first.

		const int descriptionLineHeight = IsSmallFontTall() ? 20 : 18;
		const int descriptionMarginTop = IsSmallFontTall() ? 10 : 16;

		optionDescription[0] = '\0';

		string_view titleText;
		switch (shownMenu) {
		case ShownMenuType::Categories:
			titleText = _("Settings");
			break;
		case ShownMenuType::Settings:
			titleText = selectedCategory->GetName();
			break;
		default:
			titleText = selectedOption->GetName();
			break;
		}
		size_t itemToSelect = 0;
		std::optional<tl::function_ref<bool(SDL_Event &)>> eventHandler;

		switch (shownMenu) {
		case ShownMenuType::Categories: {
			// Oracool: user request - alphabetical, rather than the order options.cpp happens to
			// declare the categories in. Only the DISPLAY order moves: each row's value stays the
			// category's index into GetCategories(), which is what ItemSelected looks it up by.
			struct CategoryRow {
				string_view name;
				size_t index;
			};
			std::vector<CategoryRow> categoryRows;
			size_t catIndex = 0;
			for (auto *pCategory : sgOptions.GetCategories()) {
				for (auto *pEntry : pCategory->GetEntries()) {
					if (!IsValidEntry(pEntry))
						continue;
					categoryRows.push_back({ pCategory->GetName(), catIndex });
					break;
				}
				catIndex++;
			}
			// Compared as displayed, because "alphabetical" means alphabetical in the language on
			// screen - GetName() is already translated.
			std::sort(categoryRows.begin(), categoryRows.end(),
			    [](const CategoryRow &a, const CategoryRow &b) { return a.name < b.name; });
			// itemToSelect is resolved after the sort, so returning from a category still lands on
			// it rather than on whatever now sits at its old position.
			for (const CategoryRow &row : categoryRows) {
				if (selectedCategory == sgOptions.GetCategories()[row.index])
					itemToSelect = vecDialogItems.size();
				vecDialogItems.push_back(std::make_unique<UiListItem>(row.name, static_cast<int>(row.index), UiFlags::ColorUiGold));
			}
		} break;
		case ShownMenuType::Settings: {
			for (auto *pEntry : selectedCategory->GetEntries()) {
				if (!IsValidEntry(pEntry))
					continue;
				if (selectedOption == pEntry)
					itemToSelect = vecDialogItems.size();
				auto formatArgs = CreateDrawStringFormatArgForEntry(pEntry);
				if (NeedsTwoLinesToDisplayOption(formatArgs)) {
					vecDialogItems.push_back(std::make_unique<UiListItem>("{}:", formatArgs, vecOptions.size(), UiFlags::ColorUiGold | UiFlags::NeedsNextElement));
					vecDialogItems.push_back(std::make_unique<UiListItem>(pEntry->GetValueDescription(), vecOptions.size(), UiFlags::ColorUiSilver | UiFlags::ElementDisabled));
				} else {
					vecDialogItems.push_back(std::make_unique<UiListItem>("{}: {}", formatArgs, vecOptions.size(), UiFlags::ColorUiGold));
				}
				vecOptions.push_back(pEntry);
			}
		} break;
		case ShownMenuType::ListOption: {
			auto *pOptionList = static_cast<OptionEntryListBase *>(selectedOption);
			for (size_t i = 0; i < pOptionList->GetListSize(); i++) {
				vecDialogItems.push_back(std::make_unique<UiListItem>(pOptionList->GetListDescription(i), i, UiFlags::ColorUiGold));
			}
			itemToSelect = pOptionList->GetActiveListIndex();
			UpdateDescription(*pOptionList);
		} break;
		case ShownMenuType::KeyInput: {
			vecDialogItems.push_back(std::make_unique<UiListItem>(_("Bound key:"), static_cast<int>(SpecialMenuEntry::None), UiFlags::ColorWhitegold | UiFlags::ElementDisabled));
			vecDialogItems.push_back(std::make_unique<UiListItem>(selectedOption->GetValueDescription(), static_cast<int>(SpecialMenuEntry::None), UiFlags::ColorUiGold));
			assert(IndexKeyOrPadInput == vecDialogItems.size() - 1);
			itemToSelect = IndexKeyOrPadInput;
			eventHandler = [](SDL_Event &event) {
				if (SelectedItem != IndexKeyOrPadInput)
					return false;
				uint32_t key = SDLK_UNKNOWN;
				switch (event.type) {
				case SDL_KEYDOWN: {
					SDL_Keycode keycode = event.key.keysym.sym;
					remap_keyboard_key(&keycode);
					key = static_cast<uint32_t>(keycode);
					if (key >= SDLK_a && key <= SDLK_z) {
						key -= 'a' - 'A';
					}
				} break;
				case SDL_MOUSEBUTTONDOWN:
					switch (event.button.button) {
					case SDL_BUTTON_MIDDLE:
					case SDL_BUTTON_X1:
					case SDL_BUTTON_X2:
						key = event.button.button | KeymapperMouseButtonMask;
						break;
					}
					break;
#if SDL_VERSION_ATLEAST(2, 0, 0)
				case SDL_MOUSEWHEEL:
					if (event.wheel.y > 0) {
						key = MouseScrollUpButton;
					} else if (event.wheel.y < 0) {
						key = MouseScrollDownButton;
					} else if (event.wheel.x > 0) {
						key = MouseScrollLeftButton;
					} else if (event.wheel.x < 0) {
						key = MouseScrollRightButton;
					}
					break;
#endif
				}
				// Ignore unknown keys
				if (key == SDLK_UNKNOWN)
					return false;
				// Nor the reserved ones: F1-F12 are answered before the keymapper, so a binding there never fired and silently
				// unbound whatever held the key (round 37 audit).
				if (key >= SDLK_F1 && key <= SDLK_F12)
					return false;
				auto *pOptionKey = static_cast<KeymapperOptions::Action *>(selectedOption);
				if (!pOptionKey->SetValue(key))
					return false;
				vecDialogItems[IndexKeyOrPadInput]->m_text = selectedOption->GetValueDescription().data();
				return true;
			};
			vecDialogItems.push_back(std::make_unique<UiListItem>(_("Press any key to change."), static_cast<int>(SpecialMenuEntry::None), UiFlags::ColorUiSilver | UiFlags::ElementDisabled));
			vecDialogItems.push_back(std::make_unique<UiListItem>("", static_cast<int>(SpecialMenuEntry::None), UiFlags::ElementDisabled));
			vecDialogItems.push_back(std::make_unique<UiListItem>(_("Unbind key"), static_cast<int>(SpecialMenuEntry::UnbindKey), UiFlags::ColorUiGold));
			UpdateDescription(*selectedOption);
		} break;
		case ShownMenuType::PadInput: {
			vecDialogItems.push_back(std::make_unique<UiListItem>(_("Bound button combo:"), static_cast<int>(SpecialMenuEntry::None), UiFlags::ColorWhitegold | UiFlags::ElementDisabled));
			vecDialogItems.push_back(std::make_unique<UiListItem>(selectedOption->GetValueDescription(), static_cast<int>(SpecialMenuEntry::BindPadButton), UiFlags::ColorUiGold));
			assert(IndexKeyOrPadInput == vecDialogItems.size() - 1);
			itemToSelect = IndexKeyOrPadInput;

			vecDialogItems.push_back(std::make_unique<UiListItem>(padEntryTimerText, static_cast<int>(SpecialMenuEntry::None), UiFlags::ColorUiSilver | UiFlags::ElementDisabled));
			assert(IndexPadTimerText == vecDialogItems.size() - 1);

			vecDialogItems.push_back(std::make_unique<UiListItem>("", static_cast<int>(SpecialMenuEntry::None), UiFlags::ElementDisabled));
			vecDialogItems.push_back(std::make_unique<UiListItem>(_("Unbind button combo"), static_cast<int>(SpecialMenuEntry::UnbindPadButton), UiFlags::ColorUiGold));

			padEntryStartTime = 0;
			eventHandler = [](SDL_Event &event) {
				if (padEntryStartTime == 0)
					return false;

				StaticVector<ControllerButtonEvent, 4> ctrlEvents = ToControllerButtonEvents(event);
				for (ControllerButtonEvent ctrlEvent : ctrlEvents) {
					bool isGamepadMotion = IsControllerMotion(event);
					DetectInputMethod(event, ctrlEvent);
					if (event.type == SDL_KEYUP && event.key.keysym.sym == SDLK_ESCAPE) {
						StopPadEntryTimer();
						return true;
					}
					if (isGamepadMotion || IsAnyOf(ctrlEvent.button, ControllerButton_NONE, ControllerButton_IGNORE)) {
						continue;
					}

					bool modifierPressed = padEntryCombo.modifier != ControllerButton_NONE && IsControllerButtonPressed(padEntryCombo.modifier);
					bool buttonPressed = padEntryCombo.button != ControllerButton_NONE && IsControllerButtonPressed(padEntryCombo.button);
					if (ctrlEvent.up) {
						// When the player has released all relevant inputs, assume the binding is finished and stop the timer
						if (padEntryCombo.button != ControllerButton_NONE && !modifierPressed && !buttonPressed) {
							StopPadEntryTimer();
							return true;
						}
						continue;
					}

					auto *pOptionPad = static_cast<PadmapperOptions::Action *>(selectedOption);
					if (!modifierPressed && buttonPressed)
						padEntryCombo.modifier = padEntryCombo.button;
					padEntryCombo.button = ctrlEvent.button;
					if (pOptionPad->SetValue(padEntryCombo))
						vecDialogItems[IndexKeyOrPadInput]->m_text = selectedOption->GetValueDescription().data();
				}
				return true;
			};
			UpdateDescription(*selectedOption);
		} break;
		}

		vecDialogItems.push_back(std::make_unique<UiListItem>("", static_cast<int>(SpecialMenuEntry::None), UiFlags::ElementDisabled));
		vecDialogItems.push_back(std::make_unique<UiListItem>(_("Previous Menu"), static_cast<int>(SpecialMenuEntry::PreviousMenu), UiFlags::ColorUiGold));

		// Oracool: user request - a bigger font on the CATEGORIES list, and the whole block centred
		// vertically instead of hanging from the top edge with all the slack below it.
		//
		// Bigger only on the categories list, deliberately. Its rows are single words with room to
		// spare; an option list's rows are "Name: Value" and already need two-line handling to fit at
		// 24. IsOptionTooLong also measures against GameFont24 by name, so raising the option lists
		// would need that measurement changed in step or two-line detection would start lying.
		//
		// The row pitch is TIGHTER than the font's natural line height (text_render.cpp's
		// LineHeights: 24 -> 26, 30 -> 38): 34, on the user's call, because at 38 the taller list
		// consumed the vertical slack the block was meant to be centred in. The pitch is therefore
		// passed to UiList as an explicit line height as well - without that, DrawString anchors each
		// glyph's bottom at rect.y + 38 while clipping at rect.y + 34 and quietly shaves four pixels
		// off every row, descenders first, in a list that has three of them.
		const bool isCategoryList = shownMenu == ShownMenuType::Categories;
		const int listItemHeight = isCategoryList ? 34 : 26;
		const UiFlags listFontSize = isCategoryList ? UiFlags::FontSize30 : UiFlags::FontSize24;

		// The block, top-down: logo (from its top), title at +223, list at +266, and a fixed 80px
		// reserved for the option description under it. The list is the only variable-height part, so
		// everything else is a constant and the block's extent falls out of the list's height.
		//
		// Oracool: both offsets moved up by 62 from +161/+204 when the masthead became ui_art\logo,
		// which is 216px tall against ui_art\smlogo's 154 (see LoadUiGFX). The 7px of air the title
		// had under the flames is preserved. blockHeight grows with ListTop and blockTop is derived
		// from it, so the whole block simply re-centres itself - nothing else here needed touching.
		constexpr int ListTop = 266;
		constexpr int DescriptionHeight = 80;
		rectList = { uiRectangle.position + Displacement { 50, ListTop },
			Size { uiRectangle.size.width - 100, std::min<int>(vecDialogItems.size() * listItemHeight, uiRectangle.size.height - ListTop - DescriptionHeight) } };
		// A list long enough to hit that cap fills the screen down to the description's box, and blockTop clamps to 0. The
		// cap was vanilla's H - 272, sized for a list top of 204: from 266 it pushed the description below the screen in
		// the long categories - Diablo Orcl, Keymapping, Game (round 6 audit, v1.12.231).
		const int blockHeight = ListTop + rectList.size.height + DescriptionHeight;
		const int blockTop = std::max(0, (uiRectangle.size.height - blockHeight) / 2);
		rectList.position.y += blockTop;

		// Logo and title, now that the block's top is known. Pushed after the background and before
		// the list, so the draw order is unchanged from when they sat further up this function.
		UiAddLogo(&vecDialog, uiRectangle.position.y + blockTop);
		vecDialog.push_back(std::make_unique<UiArtText>(titleText.data(),
		    MakeSdlRect(uiRectangle.position.x, uiRectangle.position.y + blockTop + 223, uiRectangle.size.width, 35),
		    UiFlags::FontSize30 | UiFlags::ColorUiSilver | UiFlags::AlignCenter, 8));

		rectDescription = { rectList.position + Displacement { -26, rectList.size.height + descriptionMarginTop },
			Size { uiRectangle.size.width - 50, DescriptionHeight - descriptionMarginTop } };
		vecDialog.push_back(std::make_unique<UiScrollbar>((*ArtScrollBarBackground)[0], (*ArtScrollBarThumb)[0],
		    *ArtScrollBarArrow, MakeSdlRect(rectList.position.x + rectList.size.width + 5, rectList.position.y, 25, rectList.size.height)));
		vecDialog.push_back(std::make_unique<UiArtText>(optionDescription, MakeSdlRect(rectDescription),
		    UiFlags::FontSize12 | UiFlags::ColorUiSilverDark | UiFlags::AlignCenter, 1, descriptionLineHeight));
		vecDialog.push_back(std::make_unique<UiList>(vecDialogItems, rectList.size.height / listItemHeight,
		    rectList.position.x, rectList.position.y, rectList.size.width, listItemHeight,
		    listFontSize | UiFlags::AlignCenter, /*spacing=*/1, /*lineHeight=*/listItemHeight));

		UiInitList(ItemFocused, ItemSelected, EscPressed, vecDialog, true, FullscreenChanged, nullptr, itemToSelect);

		while (!endMenu) {
			UiClearScreen();
			UpdatePadEntryTimerText();
			UiPollAndRender(eventHandler);
		}

		CleanUpSettingsUI();
	} while (!backToMain);

	SaveOptions();
}

} // namespace devilution
