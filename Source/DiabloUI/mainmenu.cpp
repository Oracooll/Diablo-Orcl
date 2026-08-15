#include <algorithm>
#include <cstdint>

#include "DiabloUI/diabloui.h"
#include "DiabloUI/selok.h"
#include "control.h"
#include "engine/load_clx.hpp"
#include "engine/render/text_render.hpp" // GetLineWidth - each entry's box is sized to its own word
#include "oracool/ui_backgrounds.h"
#include "utils/language.h"

namespace devilution {
namespace {
int mainmenu_attract_time_out; // seconds
uint32_t dwAttractTicks;

std::vector<std::unique_ptr<UiItemBase>> vecMainMenuDialog;
std::vector<std::unique_ptr<UiListItem>> vecMenuItems;

_mainmenu_selections MainMenuResult;

/**
 * @brief No painting behind the main menu.
 *
 * Was true for one version, while the user prepared a replacement. It is false again now that
 * `ui\main_menu_bg.png` is the new 1916x821 painting they supplied. Kept as a switch rather than
 * deleted, because "show me this screen bare" is a thing that has now been asked for twice.
 *
 * Hidden, the screen falls to black rather than to the stock `ui_art\mainmenu` plate: that plate is
 * a 640x480 image drawn centred and would read as a picture floating in the middle of the window
 * rather than as "no background". UiClearScreen paints the black, as it already does whenever the
 * screen is wider than the stock art.
 */
constexpr bool MainMenuBackgroundHidden = false;

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
	// Oracool: user request, with a concept image - the words are PLAY, SETTINGS and EXIT now, each
	// standing on the thing behind it. "Single Player" and "Exit Diablo" described a menu that had a
	// Multi Player entry and a Hellfire to leave; neither is true here, and against a painting the
	// shorter words read as places rather than as a list.
	vecMenuItems.push_back(std::make_unique<UiListItem>(_("Play"), MAINMENU_SINGLE_PLAYER));
	vecMenuItems.push_back(std::make_unique<UiListItem>(_("Settings"), MAINMENU_SETTINGS));
#ifndef NOEXIT
	// Never "Exit Hellfire": the game runs Hellfire's content but wears Diablo's name and logo
	// throughout (see LoadUiGFX, gmenu.cpp and title.cpp).
	vecMenuItems.push_back(std::make_unique<UiListItem>(_("Exit"), MAINMENU_EXIT_DIABLO));
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
	if (MainMenuBackgroundHidden) {
		// The palette still has to be pinned. LoadBackgroundArt above ADOPTS ui_art\mainmenu.pcx's
		// palette; AddUiBackground below is what would put ui_art\diablo.pal back over it, so skipping
		// the painting skips the pin too - and the front end renders in the wrong palette, the logo's
		// fire first, because its flames sit at scene-range indices.
		//
		// Written here from the start rather than found again: this exact omission on the character
		// screens at 1.5.22 cost a bug report ("something is off with the diablo logo") and a version
		// to diagnose. It is not a background problem, it only looks like one.
		UiLoadDefaultPalette();
	} else if (!oracool::AddUiBackground(&vecMainMenuDialog, oracool::UiBackground::MainMenu)) {
		UiAddBackground(&vecMainMenuDialog);
	}
	const Point uiPosition = GetUIRectangle().position;

	// Oracool: user request (concept image) - the masthead sits near the top of the WINDOW now rather
	// than hanging off the 640x480 UI rect's own top edge. The painting behind it is a full-screen
	// scene with an empty sky, so the logo has the whole width of that sky to sit in and no reason to
	// be inset. Measured off the concept at 0.036 of the frame's height.
	//
	// The 62px correction this used to carry (for ui_art\logo being taller than the smlogo the screen
	// was composed against) is gone with the layout it was correcting - the menu below is no longer
	// a stack that the logo could grow down into.
	UiAddLogo(&vecMainMenuDialog, gnScreenHeight * 36 / 1000);

	if (gbIsSpawn && gbIsHellfire) {
		SDL_Rect rect1 = { (Sint16)(uiPosition.x), (Sint16)(uiPosition.y + 145), 640, 30 };
		vecMainMenuDialog.push_back(std::make_unique<UiArtText>(_("Shareware").data(), rect1, UiFlags::FontSize30 | UiFlags::ColorUiSilver | UiFlags::AlignCenter, 8));
	}

	// Oracool: user request, from a concept image - the three entries are no longer a centred stack.
	// Each one stands on a thing in the painting: PLAY at the foot of the cathedral steps, SETTINGS
	// in front of the blacksmith's forge, EXIT on the blue portal. That is what "match the new
	// background" meant, and a stacked list cannot say it - hence UiList::SetItemRects.
	//
	// In the PAINTING's OWN PIXELS, mapped through the same crop the painting goes through. User
	// request: "keep the main menu buttons fixed regardless of resolution and aspect ratio, they need
	// to obey the context of the background."
	//
	// A fraction of the SCREEN, which is what these were for one version, only works while every
	// resolution shows the same part of the picture. Backgrounds are COVER-cropped, so they do not:
	// at 960x720 the visible slice is 1095 of the source's 1916 columns, at 1280x720 it is 1460. The
	// forge does not move, but how far across the screen it appears does. Measured, those per-mille
	// figures put every label 79-108px off its own feature at 1280x720.
	//
	// MEASURED, by inverting the 960x720 crop on the positions taken off the concept and checking
	// each one against the master image:
	constexpr Size MainMenuArtSize { 1916, 821 };
	constexpr Point Spots[] = {
		{ 1224, 454 }, // Play - the foot of the cathedral steps
		{ 637, 591 },  // Settings - in front of the forge
		{ 1327, 716 }, // Exit - the near edge of the portal dais
	};
	/**
	 * Each row is sized to ITS OWN word rather than all to one width, which matters twice over on a
	 * scattered menu. The pentagrams are drawn at the row's two ends, so a fixed width would leave
	 * them floating out over gravestones beside a short word like EXIT; and the row is the click
	 * target, so a fixed width would make bare scenery either side of it start a game.
	 *
	 * The padding is what the pentagrams stand in. A generous fixed figure rather than the sprite's
	 * measured width, because ArtFocus may legitimately be absent (DrawSelector checks) and a layout
	 * that changes shape depending on whether an optional asset loaded is worse than a few px of air.
	 */
	constexpr int SpotPad = 46;
	constexpr int SpotHeight = 42; // one FontSize42 line, so Render's vertical centring is a no-op

	std::vector<SDL_Rect> itemRects;
	for (size_t i = 0; i < vecMenuItems.size() && i < sizeof(Spots) / sizeof(Spots[0]); i++) {
		const int width = GetLineWidth(vecMenuItems[i]->m_text, GameFont42, /*spacing=*/5) + 2 * SpotPad;
		const Point at = oracool::MapBackgroundPointToScreen(MainMenuArtSize, Spots[i]);

		// Clamped so a label can never leave the window. At the resolutions this build targets all
		// three sit well inside, but an aspect ratio narrow enough would crop the painting past one of
		// them - and a menu entry that is only reachable with the arrow keys is the failure this
		// screen has already had once. The label stops obeying the background exactly when obeying it
		// would mean disappearing.
		const int x = std::clamp(at.x - width / 2, 0, std::max(0, gnScreenWidth - width));
		const int y = std::clamp(at.y - SpotHeight / 2, 0, std::max(0, gnScreenHeight - SpotHeight));
		itemRects.push_back(MakeSdlRect(static_cast<Sint16>(x), static_cast<Sint16>(y),
		    static_cast<Uint16>(width), SpotHeight));
	}

	// The x/y/width/height handed to the constructor are placeholders: SetItemRects replaces the
	// widget's rect with the union of the rows below, which is what the mouse is tested against.
	// itemAt then answers "none" for the scenery between them, so a click on a gravestone does
	// nothing rather than starting a game.
	auto menu = std::make_unique<UiList>(vecMenuItems, vecMenuItems.size(), 0, 0, 0, SpotHeight,
	    UiFlags::FontSize42 | UiFlags::ColorUiGold | UiFlags::AlignCenter, 5);
	menu->SetItemRects(std::move(itemRects));
	vecMainMenuDialog.push_back(std::move(menu));

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
