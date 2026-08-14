#pragma once

#include <SDL.h>
#include <array>
#include <cstddef>
#include <cstdint>

#include <function_ref.hpp>

#include "DiabloUI/ui_item.h"
#include "engine/clx_sprite.hpp"
#include "engine/load_pcx.hpp" // IWYU pragma: export
#include "player.h"
#include "utils/display.h"
#include "utils/stdcompat/optional.hpp"

namespace devilution {

extern std::size_t SelectedItem;
bool IsTextInputActive();

/**
 * @brief Stops the list glowing its own selected row, for a screen whose focus can leave the list.
 *
 * Oracool: the character-select screen's focus runs list rows -> action buttons, and the buttons are
 * not list items - nothing in the shared focus model knows about them. While focus sits on one, the
 * list must stop marking a row, or the screen glows in two places at once.
 */
extern bool UiListSelectorHidden;

/**
 * @brief Draws @p button's label with the focus glow, over the button as already rendered.
 *
 * Exported so focus that lives outside a list (see UiListSelectorHidden) is marked exactly the way a
 * list row is, rather than by a second indicator that merely looks similar.
 */
void DrawFocusGlow(const UiArtTextButton &button);

extern const string_view BannedNames[];
extern const size_t BannedNamesCount;

enum _artFocus : uint8_t {
	FOCUS_SMALL,
	FOCUS_MED,
	FOCUS_BIG,
};

enum _mainmenu_selections : uint8_t {
	MAINMENU_NONE,
	MAINMENU_SINGLE_PLAYER,
	MAINMENU_MULTIPLAYER,
	MAINMENU_SHOW_SUPPORT,
	MAINMENU_SETTINGS,
	MAINMENU_SHOW_CREDITS,
	MAINMENU_EXIT_DIABLO,
	MAINMENU_ATTRACT_MODE,
};

enum _selhero_selections : uint8_t {
	SELHERO_NEW_DUNGEON,
	SELHERO_CONTINUE,
	SELHERO_CONNECT,
	SELHERO_PREVIOUS,
};

struct _uidefaultstats {
	uint16_t strength;
	uint16_t magic;
	uint16_t dexterity;
	uint16_t vitality;
};

struct _uiheroinfo {
	uint32_t saveNumber;
	char name[16];
	uint8_t level;
	HeroClass heroclass;
	uint8_t herorank;
	uint16_t strength;
	uint16_t magic;
	uint16_t dexterity;
	uint16_t vitality;
	/**
	 * @brief Player::_pgfxnum - which sprite variant this character wears (armour in the high nibble,
	 * weapon in the low one).
	 *
	 * Oracool: carried so the character-select screen can show the character's own animated sprite in
	 * the gear it actually has. Free to provide: pfile_ui_set_hero_infos already unpacks the save and
	 * runs CalcPlrInv before calling Game2UiPlayer, so the value is sitting there fully computed - it
	 * was simply never copied out.
	 */
	uint8_t gfxnum;
	bool hassaved;
	bool spawned;
};

extern OptionalOwnedClxSpriteList ArtLogo;
extern OptionalOwnedClxSpriteList DifficultyIndicator;
extern std::array<OptionalOwnedClxSpriteList, 3> ArtFocus;
extern OptionalOwnedClxSpriteList ArtBackgroundWidescreen;
extern OptionalOwnedClxSpriteList ArtBackground;
extern OptionalOwnedClxSpriteList ArtCursor;

extern bool (*gfnHeroInfo)(bool (*fninfofunc)(_uiheroinfo *));

inline SDL_Surface *DiabloUiSurface()
{
	return PalSurface;
}

void UiDestroy();
void UiTitleDialog();
void UnloadUiGFX();
void UiInitialize();
bool UiValidPlayerName(string_view name); /* check */
void UiSelHeroMultDialog(bool (*fninfo)(bool (*fninfofunc)(_uiheroinfo *)), bool (*fncreate)(_uiheroinfo *), bool (*fnremove)(_uiheroinfo *), void (*fnstats)(unsigned int, _uidefaultstats *), _selhero_selections *dlgresult, uint32_t *saveNumber);
void UiSelHeroSingDialog(bool (*fninfo)(bool (*fninfofunc)(_uiheroinfo *)), bool (*fncreate)(_uiheroinfo *), bool (*fnremove)(_uiheroinfo *), void (*fnstats)(unsigned int, _uidefaultstats *), _selhero_selections *dlgresult, uint32_t *saveNumber, _difficulty *difficulty);
bool UiCreditsDialog();
bool UiSupportDialog();
bool UiMainMenuDialog(const char *name, _mainmenu_selections *pdwResult, int attractTimeOut);
bool UiProgressDialog(int (*fnfunc)());
bool UiSelectGame(GameData *gameData, int *playerId);
bool UiSelectProvider(GameData *gameData);
void UiFadeIn();
void UiHandleEvents(SDL_Event *event);
bool UiItemMouseEvents(SDL_Event *event, const std::vector<UiItemBase *> &items);
bool UiItemMouseEvents(SDL_Event *event, const std::vector<std::unique_ptr<UiItemBase>> &items);
Sint16 GetCenterOffset(Sint16 w, Sint16 bw = 0);
void LoadPalInMem(const SDL_Color *pPal);
void DrawMouse();
void UiLoadDefaultPalette();
bool UiLoadBlackBackground();
void LoadBackgroundArt(const char *pszFile, int frames = 1);
void UiAddBackground(std::vector<std::unique_ptr<UiItemBase>> *vecDialog);
void UiAddLogo(std::vector<std::unique_ptr<UiItemBase>> *vecDialog, int y = GetUIRectangle().position.y);
void UiFocusNavigationSelect();
void UiFocusNavigationEsc();
void UiFocusNavigationYesNo();

void UiInitList(void (*fnFocus)(int value), void (*fnSelect)(int value), void (*fnEsc)(), const std::vector<std::unique_ptr<UiItemBase>> &items, bool wraps = false, void (*fnFullscreen)() = nullptr, bool (*fnYesNo)() = nullptr, size_t selectedItem = 0);
void UiRenderListItems();
void UiInitList_clear();

void UiClearScreen();
void UiPollAndRender(std::optional<tl::function_ref<bool(SDL_Event &)>> eventHandler = std::nullopt);
void UiRenderItem(const UiItemBase &item);
void UiRenderItems(const std::vector<UiItemBase *> &items);
void UiRenderItems(const std::vector<std::unique_ptr<UiItemBase>> &items);
ClxSprite UiGetHeroDialogSprite(size_t heroClassIndex);

void mainmenu_restart_repintro();
} // namespace devilution
