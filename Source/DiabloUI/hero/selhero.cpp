#include "DiabloUI/hero/selhero.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <random>

#include <fmt/format.h>

#include "DiabloUI/diabloui.h"
#include "DiabloUI/dialogs.h"
#include "DiabloUI/hero/hero_layout.h"
#include "DiabloUI/multi/selgame.h"
#include "DiabloUI/scrollbar.h"
#include "DiabloUI/selok.h"
#include "DiabloUI/selyesno.h"
#include "control.h"
#include "controls/plrctrls.h"
#include "init.h"
#include "menu.h"
#include "options.h"
#include "oracool/hero_preview.h"
#include "oracool/ui_backgrounds.h"
#include "pfile.h"
#include "utils/language.h"
#include "utils/str_cat.hpp"
#include "utils/utf8.hpp"

namespace devilution {

bool selhero_endMenu;
bool selhero_isMultiPlayer;

bool (*gfnHeroInfo)(bool (*fninfofunc)(_uiheroinfo *));
bool (*gfnHeroCreate)(_uiheroinfo *);
void (*gfnHeroStats)(unsigned int, _uidefaultstats *);

namespace {

std::size_t selhero_SaveCount = 0;
_uiheroinfo selhero_heros[MAX_CHARACTERS];
_uiheroinfo selhero_heroInfo;
// textStats and SELHERO_DIALOG_HERO_IMG are gone with the stat block and the class portrait.
const char *title = "";
_selhero_selections selhero_result;
bool selhero_navigateYesNo;
bool selhero_isSavegame;

std::vector<std::unique_ptr<UiItemBase>> vecSelHeroDialog;
std::vector<std::unique_ptr<UiListItem>> vecSelHeroDlgItems;
std::vector<std::unique_ptr<UiItemBase>> vecSelDlgItems;


/**
 * @brief What SelheroLoadSelect is being asked to do.
 *
 * Oracool: it used to take a row index and read the choice back out of `vecSelHeroDlgItems`, which
 * only works for the ONE caller that has the Continue/New Game list loaded. The other two reach it
 * with that global holding something else entirely - the character list, or the class list - and a
 * hardcoded 1.
 *
 * Vanilla survived that because the character list always ended in a "New Hero" row, so index 1
 * existed and (character i carrying m_value i) happened to answer "not Continue". Moving New Hero out
 * to a button left a one-character account with a one-item list and an index of 1: an out-of-range
 * subscript on the ordinary path of loading your only character.
 *
 * The values match the two rows' m_value so that list's callback can pass them straight through.
 */
constexpr int SelheroChoiceContinue = 0;
constexpr int SelheroChoiceNewGame = 1;

void SelheroListFocus(int value);
void SelheroListSelect(int value);
void SelheroListEsc();
void SelheroLoadFocus(int value);
void SelheroLoadListSelect(int value);
void SelheroLoadSelect(int choice);
void SelheroNameSelect(int value);
void SelheroNameEsc();
void SelheroClassSelectorFocus(int value);
void SelheroClassSelectorSelect(int value);
void SelheroClassSelectorEsc();
const char *SelheroGenerateName(HeroClass heroClass);

void SelheroUiFocusNavigationYesNo()
{
	if (selhero_isSavegame)
		UiFocusNavigationYesNo();
}

/** @brief The character list's geometry. At file scope because the preview area is defined against it. */
constexpr int HeroListWidth = 320;
constexpr int HeroListItemHeight = 52;
/**
 * @brief The scrollbar's column - a rect for the mouse, not for the bar.
 *
 * The themed bar is 3px wide and drawn centred in this (see Render(const UiScrollbar &)); the rest is
 * grab room. 25px of vanilla art became 12px of hand.
 */
constexpr int HeroScrollbarWidth = 12;
constexpr int HeroScrollbarGap = 4;
/** @brief Clearance between the figure and the list column, now that nothing sits between them. */
constexpr int HeroPreviewGap = 8;

/**
 * @brief The list column's left edge - every list on these screens sits centred over the Cancel button.
 *
 * Oracool: user request, twice. First "over the New Hero button" rather than flush with the screen's
 * right edge; then New Hero and Cancel swapped, which left the column where it was and changed what
 * stands under it. Derived from that button's own rect rather than from a second calculation that
 * happens to land in the same place: "over the Cancel button" is a relationship, and writing it as one
 * means moving the button row moves every list with it.
 */
int HeroListX()
{
	const SDL_Rect cancelButton = HeroButtonRect(CancelButtonIndex, HeroButtonCount);
	return cancelButton.x + (cancelButton.w - HeroListWidth) / 2;
}

/**
 * @brief The figure's own top - closer under the title than HeroContentTop's, by 16px.
 *
 * That 24px gap is the LIST's breathing room: rows of text immediately under a heading of the same
 * colour need the separation. The figure does not - it reads as a picture, not as another line - and
 * every pixel here is a pixel of scale, since the preview is height-bound (see PreviewScaleFor).
 */
int HeroPreviewTop()
{
	return GetUIRectangle().position.y + HeroTitleTop + HeroTitleHeight + 8;
}

/** Tall enough that the FontSize30 line inside cannot be clipped - the character list's row height. */
constexpr int HeroFormBoxHeight = HeroListItemHeight;

/** @brief Adds the OK and Cancel pair every one of these screens ends with. */
void AddHeroFormButtons(std::vector<std::unique_ptr<UiItemBase>> &items)
{
	items.push_back(std::make_unique<UiArtTextButton>(_("OK"), &UiFocusNavigationSelect,
	    HeroButtonRect(OkButtonIndex, HeroButtonCount), HeroButtonFlags));
	items.push_back(std::make_unique<UiArtTextButton>(_("Cancel"), &UiFocusNavigationEsc,
	    HeroButtonRect(CancelButtonIndex, HeroButtonCount), HeroButtonFlags));
}

/**
 * @brief Where the animated character stands: everything left of the list and its scrollbar.
 *
 * Oracool: user request - this replaced the class portrait and the stat block, which sat in a
 * 180x76 box with five label/value rows under it. The figure is drawn straight into this rect by the
 * render loop rather than being a UiItemBase, because it animates from the shared frame clock and has
 * no click behaviour - a widget would be all ceremony and no benefit.
 */
Rectangle HeroPreviewRect()
{
	const int left = GetUIRectangle().position.x;
	const int right = HeroListX() - HeroPreviewGap;
	const int top = HeroPreviewTop();
	return { { left, top }, { std::max(0, right - left), std::max(0, HeroContentBottom() - top) } };
}

/**
 * Oracool: user request - the focus rule must be able to leave the character list and walk the action
 * buttons. Nothing in the shared focus model knows about buttons: SelectedItem indexes the UiList and
 * only the UiList, and a UiArtTextButton has never been anything but a click target.
 *
 * So the ring is kept here rather than bolted onto every menu in the game. -1 means the list has
 * focus and behaves exactly as before; 0..3 means one of the buttons does, the list's own rule is
 * hidden (UiListSelectorHidden), and this file draws and activates it.
 */
UiArtTextButton *HeroActionButtons[HeroButtonCount] = {};
int SelectedActionButton = -1;
/** Only the character-list screen has an action row; the class and name screens do not. */
bool ActionRowFocusable = false;

void FocusActionButton(int index)
{
	SelectedActionButton = index;
	UiListSelectorHidden = index >= 0;
}

bool IsActionButtonEnabled(int index)
{
	const UiArtTextButton *button = HeroActionButtons[index];
	return button != nullptr && !HasAnyOf(button->GetFlags(), UiFlags::ElementDisabled | UiFlags::ElementHidden);
}

/**
 * @brief The next selectable button @p step away from @p from, wrapping. -1 if none is selectable.
 *
 * Delete is disabled whenever the highlighted row is not a real character, and stopping the rule on a
 * greyed-out word would look like the screen had hung.
 */
int NextEnabledActionButton(int from, int step)
{
	for (int i = 1; i <= HeroButtonCount; i++) {
		const int candidate = ((from + step * i) % HeroButtonCount + HeroButtonCount) % HeroButtonCount;
		if (IsActionButtonEnabled(candidate))
			return candidate;
	}
	return -1;
}

/**
 * @brief Keyboard focus for the action row, run before the shared navigation gets the event.
 *
 * Returning false hands the event on untouched, which is what keeps every other key - Escape, the
 * mouse, the controller - working exactly as it did.
 */
bool HeroActionRowNavigation(SDL_Event &event)
{
	if (!ActionRowFocusable || event.type != SDL_KEYDOWN)
		return false;

	const bool onButtons = SelectedActionButton >= 0;
	switch (event.key.keysym.sym) {
	case SDLK_DOWN:
		if (onButtons)
			return true; // already at the bottom of the screen; swallow it rather than wrap
		// Only once the list has nowhere further to go, so Down still walks the characters first.
		if (!vecSelHeroDlgItems.empty() && SelectedItem + 1 < vecSelHeroDlgItems.size())
			return false;
		if (const int first = NextEnabledActionButton(-1, 1); first >= 0)
			FocusActionButton(first);
		return true;
	case SDLK_UP:
		if (!onButtons)
			return false;
		FocusActionButton(-1);
		return true;
	case SDLK_LEFT:
	case SDLK_RIGHT: {
		if (!onButtons)
			return false;
		const int step = event.key.keysym.sym == SDLK_LEFT ? -1 : 1;
		if (const int next = NextEnabledActionButton(SelectedActionButton, step); next >= 0)
			FocusActionButton(next);
		return true;
	}
	case SDLK_RETURN:
	case SDLK_KP_ENTER:
		if (!onButtons)
			return false;
		// Nothing may touch HeroActionButtons after this: activating New Hero or Cancel rebuilds
		// vecSelDlgItems, and every pointer in that array is into what it just freed.
		HeroActionButtons[SelectedActionButton]->Activate();
		return true;
	default:
		break;
	}
	return false;
}

/** @brief Puts focus back in the list and forgets the buttons - for a screen about to be rebuilt. */
void ResetActionRowFocus()
{
	ActionRowFocusable = false;
	FocusActionButton(-1);
	for (UiArtTextButton *&button : HeroActionButtons)
		button = nullptr;
}

/**
 * @brief The "New Hero" button's action - the branch selecting the old list's last row used to take.
 *
 * It was a row in the character list until the user asked for it as a button ("very important"), so
 * it is dispatched here by the value that row carried. That also means the list can now be empty, on
 * a fresh install with no characters - see the note in UiInitList.
 */
void SelheroNewHero()
{
	SelheroListSelect(static_cast<int>(selhero_SaveCount));
}

void SelheroFree()
{
	oracool::FreeHeroPreview();
	ResetActionRowFocus();
	ArtBackground = std::nullopt;

	vecSelHeroDialog.clear();

	vecSelDlgItems.clear();
	vecSelHeroDlgItems.clear();
	UnloadScrollBar();
}

/**
 * @brief Oracool: was "fill the portrait and the five stat rows"; now "load the character's sprite".
 *
 * The name is kept because five call sites mean it, and what it does has not changed - it is still
 * "make the left-hand side describe this character". It just describes them by showing them.
 */
void SelheroSetStats()
{
	oracool::SetHeroPreview(selhero_heroInfo.heroclass, selhero_heroInfo.gfxnum);
}

void RenderDifficultyIndicators()
{
	if (!selhero_isSavegame)
		return;
	const uint16_t width = (*DifficultyIndicator)[0].width();
	const uint16_t height = (*DifficultyIndicator)[0].height();
	// Anchored to the preview area's bottom-left, where the portrait's bottom-left used to be.
	const Rectangle preview = HeroPreviewRect();
	SDL_Rect rect = MakeSdlRect(
	    preview.position.x + 1,
	    preview.position.y + preview.size.height - height - 1,
	    width,
	    height);
	for (int i = 0; i <= DIFF_LAST; i++) {
		if (i >= selhero_heroInfo.herorank)
			break;
		UiRenderItem(UiImageClx((*DifficultyIndicator)[0], rect, UiFlags::None));
		rect.x += width;
	}
}

UiArtTextButton *SELLIST_DIALOG_DELETE_BUTTON;

bool SelHeroGetHeroInfo(_uiheroinfo *pInfo)
{
	selhero_heros[selhero_SaveCount] = *pInfo;

	selhero_SaveCount++;

	return true;
}

void SelheroListFocus(int value)
{
	const auto index = static_cast<std::size_t>(value);
	// Must match the Delete button's own flags, since this re-applies them when it enables/disables.
	UiFlags baseFlags = UiFlags::AlignCenter | HeroButtonFontSize;
	if (selhero_SaveCount != 0 && index < selhero_SaveCount) {
		memcpy(&selhero_heroInfo, &selhero_heros[index], sizeof(selhero_heroInfo));
		SelheroSetStats();
		SELLIST_DIALOG_DELETE_BUTTON->SetFlags(baseFlags | UiFlags::ColorUiGold);
		selhero_isSavegame = true;
		return;
	}

	// Nothing under the cursor - an empty character list, since "New Hero" left it for the button row.
	oracool::ClearHeroPreview();
	SELLIST_DIALOG_DELETE_BUTTON->SetFlags(baseFlags | UiFlags::ColorUiSilver | UiFlags::ElementDisabled);
	selhero_isSavegame = false;
}

bool SelheroListDeleteYesNo()
{
	selhero_navigateYesNo = selhero_isSavegame;

	return selhero_navigateYesNo;
}

void SelheroListSelect(int value)
{
	// Every branch below rebuilds vecSelDlgItems, which frees the buttons this file holds pointers
	// to. Dropping them here means there is no window in which a stale one could be reached.
	ResetActionRowFocus();

	if (static_cast<std::size_t>(value) == selhero_SaveCount) {
		vecSelDlgItems.clear();

		// In the list column rather than the centred one, on the user's call: the character list and
		// the class list are the same control doing the same job one step apart, so they stand in the
		// same place with the same button under them. The caption goes with it - a heading belongs
		// over what it heads.
		vecSelDlgItems.push_back(std::make_unique<UiArtText>(_("Choose Class").data(), HeroCaptionRect(HeroListX(), HeroListWidth), UiFlags::AlignCenter | UiFlags::FontSize30 | UiFlags::ColorUiSilver, 3));

		// Oracool: user request - Barbarian, Paladin, Sorcerer, Rogue. Not vanilla's order and not
		// alphabetical; it is the order the user wants them offered in. Only the rows move: each item
		// carries its own HeroClass as m_value, and both the focus and select handlers read that rather
		// than the position, so nothing else in this screen depends on the sequence.
		vecSelHeroDlgItems.clear();
		// Ahead of the rest, but still behind its switch: turning the Barbarian off leaves the other
		// three in the same order rather than reshuffling them.
		if (gbBarbarian || *sgOptions.Gameplay.testBarbarian) {
			vecSelHeroDlgItems.push_back(std::make_unique<UiListItem>(_("Barbarian"), static_cast<int>(HeroClass::Barbarian)));
		}
		vecSelHeroDlgItems.push_back(std::make_unique<UiListItem>(_("Paladin"), static_cast<int>(HeroClass::Warrior)));
		vecSelHeroDlgItems.push_back(std::make_unique<UiListItem>(_("Sorcerer"), static_cast<int>(HeroClass::Sorcerer)));
		vecSelHeroDlgItems.push_back(std::make_unique<UiListItem>(_("Rogue"), static_cast<int>(HeroClass::Rogue)));
		// Last, because the user's ordering does not mention it - it only exists if the player supplied
		// Hellfire's monk data.
		//
		// Oracool: keyed on hfmonk.mpq rather than on `gbIsHellfire`. The Monk is the only added class
		// with a sprite set of its own, so it is the only one that can be offered without turning on
		// Hellfire's quests, levels, monsters and item tables at the same time - which is what
		// gbIsHellfire does. See HaveMonk.
		if (HaveMonk()) {
			vecSelHeroDlgItems.push_back(std::make_unique<UiListItem>(_("Monk"), static_cast<int>(HeroClass::Monk)));
		}
		// The Bard closes out the roster of six (user request). Unlike the Monk it needs nothing from
		// Hellfire at all: PlayersData gives it the Rogue's classPath so it wears the Rogue's sprites,
		// sound_init() folds it into sfx_ROGUE so it speaks with the Rogue's voice, and its stats and
		// its two starting weapons are compiled in. hfbard.mpq only ever replaced the voice, which is
		// why `gbBard` merely forces the row on when that archive happens to be present - the option
		// below is what actually offers the class, and it now defaults to on.
		if (gbBard || *sgOptions.Gameplay.testBard) {
			vecSelHeroDlgItems.push_back(std::make_unique<UiListItem>(_("Bard"), static_cast<int>(HeroClass::Bard)));
		}
		// The character list's own row height and font, centred in the same band it uses. The old
		// "shrink the rows if there are more than four classes" rule is gone with the 176px box it was
		// squeezing them into - the band is the whole screen now and has room for every class.
		const int listHeight = static_cast<int>(vecSelHeroDlgItems.size()) * HeroListItemHeight;
		vecSelDlgItems.push_back(std::make_unique<UiList>(vecSelHeroDlgItems, vecSelHeroDlgItems.size(),
		    static_cast<Sint16>(HeroListX()), static_cast<Sint16>(HeroFormBodyTopFor(listHeight)),
		    HeroListWidth, HeroListItemHeight, UiFlags::AlignCenter | HeroListFontSize | UiFlags::ColorUiGold));

		AddHeroFormButtons(vecSelDlgItems);

		UiInitList(SelheroClassSelectorFocus, SelheroClassSelectorSelect, SelheroClassSelectorEsc, vecSelDlgItems, true);
		memset(&selhero_heroInfo.name, 0, sizeof(selhero_heroInfo.name));
		selhero_heroInfo.saveNumber = pfile_ui_get_first_unused_save_num();
		SelheroSetStats();
		title = selhero_isMultiPlayer ? _("New Multi Player Hero").data() : _("New Single Player Hero").data();
		selhero_isSavegame = false;
		return;
	}

	// Oracool: user request - single-player has no "Continue"/resume-session concept at all.
	// The project's goal is building the strongest character, not finishing a particular game
	// session - autosave already means the character (level, stats, gear, gold) is always
	// current, and dungeon/quest state is intentionally session-only and never worth resuming.
	// So single-player skips this dialog entirely and always falls through to SelheroLoadSelect(1)
	// below, which is single-player's existing "start a fresh dungeon with this character" path
	// (shows the difficulty picker, then StartGame(bNewGame=true, ...) - the character itself was
	// already loaded from selhero_heros[] above, independent of this choice, so nothing about the
	// character is lost). Multiplayer keeps this dialog: resuming a co-op session together is a
	// real, distinct choice there, unlike single-player.
	if (selhero_isMultiPlayer && selhero_heroInfo.hassaved) {
		vecSelDlgItems.clear();

		vecSelDlgItems.push_back(std::make_unique<UiArtText>(_("Character Exists").data(), HeroCaptionRect(HeroFormX(), HeroFormWidth), UiFlags::AlignCenter | UiFlags::FontSize30 | UiFlags::ColorUiSilver, 3));

		vecSelHeroDlgItems.clear();
		// Oracool: user request - "Load Game" renamed to "Continue" as part of removing manual
		// save/load terminology from every menu; with continuous autosave there's no separate
		// "load" action anymore, just resuming the character's always-current state.
		vecSelHeroDlgItems.push_back(std::make_unique<UiListItem>(_("Continue"), 0));
		vecSelHeroDlgItems.push_back(std::make_unique<UiListItem>(_("New Game"), 1));
		const int listHeight = static_cast<int>(vecSelHeroDlgItems.size()) * HeroListItemHeight;
		vecSelDlgItems.push_back(std::make_unique<UiList>(vecSelHeroDlgItems, vecSelHeroDlgItems.size(),
		    static_cast<Sint16>(HeroFormX()), static_cast<Sint16>(HeroFormBodyTopFor(listHeight)),
		    HeroFormWidth, HeroListItemHeight, UiFlags::AlignCenter | HeroListFontSize | UiFlags::ColorUiGold));

		AddHeroFormButtons(vecSelDlgItems);

		UiInitList(SelheroLoadFocus, SelheroLoadListSelect, selhero_List_Init, vecSelDlgItems, true);
		title = _("Single Player Characters").data();
		return;
	}

	SelheroLoadSelect(SelheroChoiceNewGame);
}

void SelheroListEsc()
{
	UiInitList_clear();

	selhero_endMenu = true;
	selhero_result = SELHERO_PREVIOUS;
}

void SelheroClassSelectorFocus(int value)
{
	const auto heroClass = static_cast<HeroClass>(vecSelHeroDlgItems[value]->m_value);

	_uidefaultstats defaults;
	gfnHeroStats(static_cast<unsigned int>(heroClass), &defaults);

	selhero_heroInfo.level = 1;
	selhero_heroInfo.heroclass = heroClass;
	selhero_heroInfo.strength = defaults.strength;
	selhero_heroInfo.magic = defaults.magic;
	selhero_heroInfo.dexterity = defaults.dexterity;
	selhero_heroInfo.vitality = defaults.vitality;
	// A character that does not exist yet has no saved gfxnum, so give it the starting look
	// CreatePlayer will: light armour (the high nibble's zero) and the class's opening weapon. Kept
	// in step with player.cpp's own switch by hand - there is no shared helper for it.
	PlayerWeaponGraphic startingWeapon = PlayerWeaponGraphic::Unarmed;
	switch (heroClass) {
	case HeroClass::Warrior:
	case HeroClass::Bard:
	case HeroClass::Barbarian:
		startingWeapon = PlayerWeaponGraphic::SwordShield;
		break;
	case HeroClass::Rogue:
		startingWeapon = PlayerWeaponGraphic::Bow;
		break;
	case HeroClass::Sorcerer:
	case HeroClass::Monk:
		startingWeapon = PlayerWeaponGraphic::Staff;
		break;
	}
	selhero_heroInfo.gfxnum = static_cast<uint8_t>(startingWeapon);

	SelheroSetStats();
}

bool ShouldPrefillHeroName()
{
#if defined(PREFILL_PLAYER_NAME)
	return true;
#else
	return ControlMode != ControlTypes::KeyboardAndMouse;
#endif
}

void RemoveSelHeroBackground()
{
	vecSelHeroDialog.erase(vecSelHeroDialog.begin());
	ArtBackground = std::nullopt;
}

void AddSelHeroBackground()
{
	// LoadBackgroundArt still runs: it is what loads this screen's palette, which the background
	// below quantizes against, and it is also the fallback if the asset is missing. Inserted at the
	// FRONT either way, because RemoveSelHeroBackground takes the background back off by erasing
	// begin().
	LoadBackgroundArt("ui_art\\selhero");
	if (oracool::AddUiBackground(&vecSelHeroDialog, oracool::UiBackground::HeroSelect, /*atFront=*/true))
		return;
	vecSelHeroDialog.insert(vecSelHeroDialog.begin(),
	    std::make_unique<UiImageClx>((*ArtBackground)[0], MakeSdlRect(0, GetUIRectangle().position.y, 0, 0), UiFlags::AlignCenter));
}

void SelheroClassSelectorSelect(int value)
{
	auto hClass = static_cast<HeroClass>(vecSelHeroDlgItems[value]->m_value);
	if (gbIsSpawn && (hClass == HeroClass::Rogue || hClass == HeroClass::Sorcerer || (hClass == HeroClass::Bard && !gbBard))) {
		RemoveSelHeroBackground();
		UiSelOkDialog(nullptr, _("The Rogue and Sorcerer are only available in the full retail version of Diablo. Visit https://www.gog.com/game/diablo to purchase.").data(), false);
		AddSelHeroBackground();
		SelheroListSelect(selhero_SaveCount);
		return;
	}

	title = selhero_isMultiPlayer ? _("New Multi Player Hero").data() : _("New Single Player Hero").data();
	memset(selhero_heroInfo.name, '\0', sizeof(selhero_heroInfo.name));
	if (ShouldPrefillHeroName())
		strcpy(selhero_heroInfo.name, SelheroGenerateName(selhero_heroInfo.heroclass));
	vecSelDlgItems.clear();
	// In the list column, on the user's call - so every step of making a character happens in the same
	// place on screen: pick a class there, name it there, and the same Cancel underneath throughout.
	vecSelDlgItems.push_back(std::make_unique<UiArtText>(_("Enter Name").data(), HeroCaptionRect(HeroListX(), HeroListWidth), UiFlags::AlignCenter | UiFlags::FontSize30 | UiFlags::ColorUiSilver, 3));

	// A size up to match the character list, and in a box tall enough for it - the old 33px rect was
	// cut for FontSize24. It is the only thing on this screen, so it takes the middle of the band.
	SDL_Rect nameRect = MakeSdlRect(static_cast<Sint16>(HeroListX()),
	    static_cast<Sint16>(HeroFormBodyTopFor(HeroFormBoxHeight)), HeroListWidth, HeroFormBoxHeight);
	vecSelDlgItems.push_back(std::make_unique<UiEdit>(_("Enter Name"), selhero_heroInfo.name, 15, false, nameRect, UiFlags::AlignCenter | HeroListFontSize | UiFlags::ColorUiGold));

	AddHeroFormButtons(vecSelDlgItems);

	UiInitList(nullptr, SelheroNameSelect, SelheroNameEsc, vecSelDlgItems);
}

void SelheroClassSelectorEsc()
{
	vecSelDlgItems.clear();
	vecSelHeroDlgItems.clear();

	if (selhero_SaveCount != 0) {
		selhero_List_Init();
		return;
	}

	SelheroListEsc();
}

void SelheroNameSelect(int /*value*/)
{
	// only check names in multiplayer, we don't care about them in single
	if (selhero_isMultiPlayer && !UiValidPlayerName(selhero_heroInfo.name)) {
		RemoveSelHeroBackground();
		UiSelOkDialog(title, _("Invalid name. A name cannot contain spaces, reserved characters, or reserved words.\n").data(), false);
		AddSelHeroBackground();
	} else {
		if (gfnHeroCreate(&selhero_heroInfo)) {
			SelheroLoadSelect(SelheroChoiceNewGame);
			return;
		}
		UiErrorOkDialog(_(/* TRANSLATORS: Error Message */ "Unable to create character."), vecSelHeroDialog);
	}

	memset(selhero_heroInfo.name, '\0', sizeof(selhero_heroInfo.name));
	SelheroClassSelectorSelect(0);
}

void SelheroNameEsc()
{
	SelheroListSelect(selhero_SaveCount);
}

void SelheroLoadFocus(int value)
{
}

/**
 * @brief The Continue/New Game list's own callback - @p value is a row in THAT list.
 *
 * The only place a row index is a safe thing to turn into a choice, which is why it is the only place
 * that does it. See SelheroChoiceContinue.
 */
void SelheroLoadListSelect(int value)
{
	SelheroLoadSelect(vecSelHeroDlgItems[value]->m_value);
}

void SelheroLoadSelect(int choice)
{
	UiInitList_clear();
	selhero_endMenu = true;
	if (choice == SelheroChoiceContinue) {
		selhero_result = SELHERO_CONTINUE;
		return;
	}

	if (!selhero_isMultiPlayer) {
		// This is part of a dangerous hack to enable difficulty selection in single-player.
		// FIXME: Dialogs should not refer to each other's variables.

		// We disable `selhero_endMenu` and replace the background and art
		// and the item list with the difficulty selection ones.
		//
		// This means selhero's render loop will render selgame's items,
		// which happens to work because the render loops are similar.
		selhero_endMenu = false;

		// Set this to false so that we do not attempt to render difficulty indicators.
		selhero_isSavegame = false;

		SelheroFree();
		LoadBackgroundArt("ui_art\\selgame");
		selgame_GameSelection_Select(0);
	}

	selhero_result = SELHERO_NEW_DUNGEON;
}

const char *SelheroGenerateName(HeroClass heroClass)
{
	static const char *const Names[6][10] = {
		{
		    // Warrior
		    "Aidan",
		    "Qarak",
		    "Born",
		    "Cathan",
		    "Halbu",
		    "Lenalas",
		    "Maximus",
		    "Vane",
		    "Myrdgar",
		    "Rothat",
		},
		{
		    // Rogue
		    "Moreina",
		    "Akara",
		    "Kashya",
		    "Flavie",
		    "Divo",
		    "Oriana",
		    "Iantha",
		    "Shikha",
		    "Basanti",
		    "Elexa",
		},
		{
		    // Sorcerer
		    "Jazreth",
		    "Drognan",
		    "Armin",
		    "Fauztin",
		    "Jere",
		    "Kazzulk",
		    "Ranslor",
		    "Sarnakyle",
		    "Valthek",
		    "Horazon",
		},
		{
		    // Monk
		    "Akyev",
		    "Dvorak",
		    "Kekegi",
		    "Kharazim",
		    "Mikulov",
		    "Shenlong",
		    "Vedenin",
		    "Vhalit",
		    "Vylnas",
		    "Zhota",
		},
		{
		    // Bard (uses Rogue names)
		    "Moreina",
		    "Akara",
		    "Kashya",
		    "Flavie",
		    "Divo",
		    "Oriana",
		    "Iantha",
		    "Shikha",
		    "Basanti",
		    "Elexa",
		},
		{
		    // Barbarian
		    "Alaric",
		    "Barloc",
		    "Egtheow",
		    "Guthlaf",
		    "Heorogar",
		    "Hrothgar",
		    "Oslaf",
		    "Qual-Kehk",
		    "Ragnar",
		    "Ulf",
		},
	};

	int iRand = rand() % 10;

	return Names[static_cast<std::size_t>(heroClass) % 6][iRand];
}

} // namespace

void selhero_Init()
{
	AddSelHeroBackground();
	UiAddLogo(&vecSelHeroDialog, HeroLogoTop());
	LoadScrollBar();

	selhero_SaveCount = 0;
	gfnHeroInfo(SelHeroGetHeroInfo);
	std::reverse(selhero_heros, selhero_heros + selhero_SaveCount);

	vecSelDlgItems.clear();
	vecSelHeroDialog.push_back(std::make_unique<UiArtText>(&title, HeroTitleRect(), UiFlags::AlignCenter | HeroTitleFontSize | UiFlags::ColorUiSilver, 3));

	// Oracool: user request - the class portrait (a 180x76 painting) and the five stat rows under it
	// are gone. Their space is HeroPreviewRect(), and the render loop fills it with the character's
	// own animated sprite instead. Nothing is pushed here for it: it is drawn directly, not as a
	// widget, because it animates off the shared frame clock and has nothing to click.
}

void selhero_List_Init()
{
	const Point uiPosition = GetUIRectangle().position;

	size_t selectedItem = 0;
	vecSelDlgItems.clear();

	// Oracool: user request - the "Select Hero" caption is gone. The screen's own title already says
	// what this is, and the caption sat where the list has now moved away from anyway.

	vecSelHeroDlgItems.clear();
	// "New Hero" is no longer a row here - it is a button in the action row below (user request).
	// That means this list is EMPTY on a fresh install; see the note in UiInitList for why that is
	// now safe, and SelheroListFocus already handled a focus with no character behind it.
	for (std::size_t i = 0; i < selhero_SaveCount; i++) {
		vecSelHeroDlgItems.push_back(std::make_unique<UiListItem>(selhero_heros[i].name, static_cast<int>(i)));
		if (selhero_heros[i].saveNumber == selhero_heroInfo.saveNumber)
			selectedItem = i;
	}

	// Oracool: user request - the list sits over the New Hero button. It was flush with the screen's
	// right edge for a while (the focus pentagram all but touching it); centring it on the button
	// underneath ties the two together instead. See HeroListX.
	const int listX = HeroListX();

	// Oracool: user request - the list sits in the band between the title and the action row, and is
	// centred vertically inside it rather than hanging from a fixed offset. Both the viewport and the
	// top come out of that band, so the list can never reach the buttons whatever the pitch is set
	// to, and it grew from four visible rows to seven simply by no longer starting where the old
	// 640x480 layout put it.
	constexpr int ListToButtonsGap = 12;
	const int listAreaTop = uiPosition.y + HeroTitleTop + HeroTitleHeight + 24;
	const int listAreaBottom = HeroButtonRowTop() - ListToButtonsGap;
	const auto viewportSize = static_cast<size_t>(
	    std::max(1, (listAreaBottom - listAreaTop) / HeroListItemHeight));
	const int listY = listAreaTop
	    + std::max(0, (listAreaBottom - listAreaTop - static_cast<int>(viewportSize) * HeroListItemHeight) / 2);
	vecSelDlgItems.push_back(std::make_unique<UiList>(vecSelHeroDlgItems, viewportSize,
	    static_cast<Sint16>(listX), static_cast<Sint16>(listY), HeroListWidth, HeroListItemHeight,
	    UiFlags::AlignCenter | HeroListFontSize | UiFlags::ColorUiGold));

	// On the list's RIGHT, on the user's call. It was on the left because the vanilla art is 25px wide
	// and the list's right edge is 940 of 960 - a bar beside it would have run off the screen. The
	// themed bar is 3px in a 12px grab column, which fits the 20px that were there all along.
	SDL_Rect rect2 = { static_cast<Sint16>(listX + HeroListWidth + HeroScrollbarGap), static_cast<Sint16>(listY),
		HeroScrollbarWidth, static_cast<Uint16>(viewportSize * HeroListItemHeight) };
	vecSelDlgItems.push_back(std::make_unique<UiScrollbar>((*ArtScrollBarBackground)[0], (*ArtScrollBarThumb)[0], *ArtScrollBarArrow, rect2));

	// The action row, in the order the user listed it. New Hero's index is shared with HeroListX,
	// which centres the list over it; OK's and Cancel's are shared with the class and name screens,
	// which put their own pair in the same two cells.
	auto okButton = std::make_unique<UiArtTextButton>(_("OK"), &UiFocusNavigationSelect, HeroButtonRect(OkButtonIndex, HeroButtonCount), HeroButtonFlags);
	HeroActionButtons[OkButtonIndex] = okButton.get();
	vecSelDlgItems.push_back(std::move(okButton));

	auto setlistDialogDeleteButton = std::make_unique<UiArtTextButton>(_("Delete"), &SelheroUiFocusNavigationYesNo, HeroButtonRect(DeleteButtonIndex, HeroButtonCount), UiFlags::AlignCenter | HeroButtonFontSize | UiFlags::ColorUiSilver | UiFlags::ElementDisabled);
	SELLIST_DIALOG_DELETE_BUTTON = setlistDialogDeleteButton.get();
	HeroActionButtons[DeleteButtonIndex] = setlistDialogDeleteButton.get();
	vecSelDlgItems.push_back(std::move(setlistDialogDeleteButton));

	auto cancelButton = std::make_unique<UiArtTextButton>(_("Cancel"), &UiFocusNavigationEsc, HeroButtonRect(CancelButtonIndex, HeroButtonCount), HeroButtonFlags);
	HeroActionButtons[CancelButtonIndex] = cancelButton.get();
	vecSelDlgItems.push_back(std::move(cancelButton));

	auto newHeroButton = std::make_unique<UiArtTextButton>(_("New Hero"), &SelheroNewHero, HeroButtonRect(NewHeroButtonIndex, HeroButtonCount), HeroButtonFlags);
	HeroActionButtons[NewHeroButtonIndex] = newHeroButton.get();
	vecSelDlgItems.push_back(std::move(newHeroButton));

	// Focus starts in the list and can walk down into these - see HeroActionRowNavigation.
	FocusActionButton(-1);
	ActionRowFocusable = true;

	UiInitList(SelheroListFocus, SelheroListSelect, SelheroListEsc, vecSelDlgItems, false, nullptr, SelheroListDeleteYesNo, selectedItem);
	if (selhero_isMultiPlayer) {
		title = _("Multi Player Characters").data();
	} else {
		title = _("Single Player Characters").data();
	}
}

static void UiSelHeroDialog(
    bool (*fninfo)(bool (*fninfofunc)(_uiheroinfo *)),
    bool (*fncreate)(_uiheroinfo *),
    void (*fnstats)(unsigned int, _uidefaultstats *),
    bool (*fnremove)(_uiheroinfo *),
    _selhero_selections *dlgresult,
    uint32_t *saveNumber)
{
	do {
		gfnHeroInfo = fninfo;
		gfnHeroCreate = fncreate;
		gfnHeroStats = fnstats;
		selhero_result = *dlgresult;

		selhero_navigateYesNo = false;

		selhero_Init();

		if (selhero_SaveCount != 0) {
			selhero_heroInfo = {};
			// Search last used save and remember it as selected item
			for (size_t i = 0; i < selhero_SaveCount; i++) {
				if (selhero_heros[i].saveNumber == *saveNumber) {
					memcpy(&selhero_heroInfo, &selhero_heros[i], sizeof(selhero_heroInfo));
					break;
				}
			}
			selhero_List_Init();
		} else {
			SelheroListSelect(selhero_SaveCount);
		}

		selhero_endMenu = false;
		while (!selhero_endMenu && !selhero_navigateYesNo) {
			UiClearScreen();
			UiRenderItems(vecSelHeroDialog);
			// After the background and before UiPollAndRender's list pass, so the figure sits over
			// the painting and under nothing it could collide with - the list is on the far side.
			oracool::DrawHeroPreview(Surface(DiabloUiSurface()), HeroPreviewRect());
			// The focused button's glow, drawn here because the buttons went down with the rest of
			// vecSelHeroDialog above and the halo belongs behind the label. DrawFocusGlow puts the
			// label back on top of it for exactly that reason.
			if (SelectedActionButton >= 0 && HeroActionButtons[SelectedActionButton] != nullptr)
				DrawFocusGlow(*HeroActionButtons[SelectedActionButton]);
			RenderDifficultyIndicators();
			UiPollAndRender(HeroActionRowNavigation);
		}
		SelheroFree();

		if (selhero_navigateYesNo) {
			char dialogTitle[128];
			char dialogText[256];
			if (selhero_isMultiPlayer) {
				CopyUtf8(dialogTitle, _("Delete Multi Player Hero"), sizeof(dialogTitle));
			} else {
				CopyUtf8(dialogTitle, _("Delete Single Player Hero"), sizeof(dialogTitle));
			}
			strcpy(dialogText, fmt::format(fmt::runtime(_("Are you sure you want to delete the character \"{:s}\"?")), selhero_heroInfo.name).c_str());

			// Oracool: user request - the confirmation shows the character it is asking about, so you
			// see who you are about to destroy rather than reading their name off a line of text.
			//
			// Loaded HERE, not in the dialog: SelheroFree above has just called FreeHeroPreview, and
			// selhero_heroInfo is this file's own. The dialog draws whatever is loaded, the same
			// handshake the character screen itself uses between SelheroSetStats and its render loop.
			oracool::SetHeroPreview(selhero_heroInfo.heroclass, selhero_heroInfo.gfxnum);

			if (UiSelHeroYesNoDialog(dialogTitle, dialogText))
				fnremove(&selhero_heroInfo);
		}
	} while (selhero_navigateYesNo);

	*dlgresult = selhero_result;
	*saveNumber = selhero_heroInfo.saveNumber;
}

void UiSelHeroSingDialog(
    bool (*fninfo)(bool (*fninfofunc)(_uiheroinfo *)),
    bool (*fncreate)(_uiheroinfo *),
    bool (*fnremove)(_uiheroinfo *),
    void (*fnstats)(unsigned int, _uidefaultstats *),
    _selhero_selections *dlgresult,
    uint32_t *saveNumber,
    _difficulty *difficulty)
{
	selhero_isMultiPlayer = false;
	UiSelHeroDialog(fninfo, fncreate, fnstats, fnremove, dlgresult, saveNumber);
	*difficulty = nDifficulty;
}

void UiSelHeroMultDialog(
    bool (*fninfo)(bool (*fninfofunc)(_uiheroinfo *)),
    bool (*fncreate)(_uiheroinfo *),
    bool (*fnremove)(_uiheroinfo *),
    void (*fnstats)(unsigned int, _uidefaultstats *),
    _selhero_selections *dlgresult,
    uint32_t *saveNumber)
{
	selhero_isMultiPlayer = true;
	UiSelHeroDialog(fninfo, fncreate, fnstats, fnremove, dlgresult, saveNumber);
}

} // namespace devilution
