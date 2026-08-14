#include <cstdint>

#include "DiabloUI/diabloui.h"
#include "DiabloUI/selok.h"
#include "control.h"
#include "engine/load_clx.hpp"
#include "oracool/ui_backgrounds.h"
#include "utils/language.h"

namespace devilution {
namespace {
int mainmenu_attract_time_out; // seconds
uint32_t dwAttractTicks;

std::vector<std::unique_ptr<UiItemBase>> vecMainMenuDialog;
std::vector<std::unique_ptr<UiListItem>> vecMenuItems;

_mainmenu_selections MainMenuResult;

void UiMainMenuSelect(int value)
{
	MainMenuResult = (_mainmenu_selections)vecMenuItems[value]->m_value;
}

#ifndef NOEXIT
void MainmenuEsc()
{
	std::size_t last = vecMenuItems.size() - 1;
	if (SelectedItem == last) {
		UiMainMenuSelect(last);
	} else {
		SelectedItem = last;
	}
}
#endif

void MainmenuLoad(const char *name)
{
	// Oracool: user request - trimmed to just the three entries relevant to this single-player-
	// focused edition. Multi Player, Support, and Show Credits are deliberately omitted (not
	// deleted elsewhere - their screens/handling still exist, they're just unreachable from here).
	vecMenuItems.push_back(std::make_unique<UiListItem>(_("Single Player"), MAINMENU_SINGLE_PLAYER));
	vecMenuItems.push_back(std::make_unique<UiListItem>(_("Settings"), MAINMENU_SETTINGS));
#ifndef NOEXIT
	vecMenuItems.push_back(std::make_unique<UiListItem>(gbIsHellfire ? _("Exit Hellfire") : _("Exit Diablo"), MAINMENU_EXIT_DIABLO));
#endif

	if (!gbIsSpawn || gbIsHellfire) {
		if (gbIsHellfire)
			ArtBackgroundWidescreen = LoadOptionalClx("ui_art\\mainmenuw.clx");
		LoadBackgroundArt("ui_art\\mainmenu");
	} else {
		LoadBackgroundArt("ui_art\\swmmenu");
	}

	// Oracool: user request - the 21:9 painting, cropped to whatever resolution is running. It has to
	// come after LoadBackgroundArt above (which is what puts this screen's palette in place) and it
	// REPLACES the stock background rather than layering over it: the stock one is a 640x480 plate
	// drawn centred, so leaving it in would paint a near-black rectangle over the middle of the new
	// art. Falls back to the stock background if the asset is missing.
	if (!oracool::AddUiBackground(&vecMainMenuDialog, oracool::UiBackground::MainMenu))
		UiAddBackground(&vecMainMenuDialog);
	UiAddLogo(&vecMainMenuDialog);

	const Point uiPosition = GetUIRectangle().position;

	if (gbIsSpawn && gbIsHellfire) {
		SDL_Rect rect1 = { (Sint16)(uiPosition.x), (Sint16)(uiPosition.y + 145), 640, 30 };
		vecMainMenuDialog.push_back(std::make_unique<UiArtText>(_("Shareware").data(), rect1, UiFlags::FontSize30 | UiFlags::ColorUiSilver | UiFlags::AlignCenter, 8));
	}

	// Oracool: user request - the menu looked cramped with only 3 entries left (down from the
	// original 6), so each item's row now reserves a full blank row's worth of extra space below
	// it (86 = double the original 43px row height) instead of sitting back-to-back with the next.
	// UiList::itemRect() spaces every row by this same height, and text renders top-aligned within
	// its row (no VerticalCenter flag below), so the added height shows up as empty space after
	// each item's text rather than stretching the text itself. Total list height (3 * 86 = 258px)
	// matches what the original 6-item menu already occupied (6 * 43 = 258px), so this fits the
	// same vertical space the working, untrimmed menu always used.
	vecMainMenuDialog.push_back(std::make_unique<UiList>(vecMenuItems, vecMenuItems.size(), uiPosition.x + 64, (uiPosition.y + 192), 510, 86, UiFlags::FontSize42 | UiFlags::ColorUiGold | UiFlags::AlignCenter, 5));

	SDL_Rect rect2 = { 17, (Sint16)(gnScreenHeight - 47), 605, 32 };
	vecMainMenuDialog.push_back(std::make_unique<UiArtText>(name, rect2, UiFlags::FontSize12 | UiFlags::ColorUiSilverDark, 1, 16));

#ifndef NOEXIT
	UiInitList(nullptr, UiMainMenuSelect, MainmenuEsc, vecMainMenuDialog, true);
#else
	UiInitList(nullptr, UiMainMenuSelect, nullptr, vecMainMenuDialog, true);
#endif
}

void MainmenuFree()
{
	ArtBackgroundWidescreen = std::nullopt;
	ArtBackground = std::nullopt;

	vecMainMenuDialog.clear();

	vecMenuItems.clear();
}

} // namespace

void mainmenu_restart_repintro()
{
	dwAttractTicks = SDL_GetTicks() + mainmenu_attract_time_out * 1000;
}

bool UiMainMenuDialog(const char *name, _mainmenu_selections *pdwResult, int attractTimeOut)
{
	MainMenuResult = MAINMENU_NONE;
	while (MainMenuResult == MAINMENU_NONE) {
		mainmenu_attract_time_out = attractTimeOut;
		MainmenuLoad(name);

		mainmenu_restart_repintro(); // for automatic starts

		while (MainMenuResult == MAINMENU_NONE) {
			UiClearScreen();
			UiPollAndRender();
			if (SDL_GetTicks() >= dwAttractTicks && (HaveDiabdat() || HaveHellfire())) {
				MainMenuResult = MAINMENU_ATTRACT_MODE;
			}
		}

		MainmenuFree();
	}

	*pdwResult = MainMenuResult;
	return true;
}

} // namespace devilution
