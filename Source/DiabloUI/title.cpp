#include "DiabloUI/diabloui.h"
#include "control.h"
#include "controls/input.h"
#include "controls/menu_controls.h"
#include "discord/discord.h"
#include "engine/load_clx.hpp"
#include "engine/load_pcx.hpp"
#include "utils/language.h"
#include "utils/sdl_geometry.h"
#include "utils/stdcompat/optional.hpp"

namespace devilution {
namespace {

OptionalOwnedClxSpriteList DiabloTitleLogo;

std::vector<std::unique_ptr<UiItemBase>> vecTitleScreen;

// Oracool: user request - the Diablo title screen even in Hellfire mode. Vanilla loads the animated
// ui_art\hf_logo1 (plus its widescreen strip) whenever gbIsHellfire is set; this build always uses
// Diablo's painted title and its logo. The third and last place the logo used to fork - the other
// two are LoadUiGFX in DiabloUI/diabloui.cpp and sgpLogo in gmenu.cpp.
void TitleLoad()
{
	LoadBackgroundArt("ui_art\\title");
	DiabloTitleLogo = LoadPcxSpriteList("ui_art\\logo", /*numFrames=*/15, /*transparentColor=*/250);
}

void TitleFree()
{
	ArtBackground = std::nullopt;
	ArtBackgroundWidescreen = std::nullopt;
	DiabloTitleLogo = std::nullopt;

	vecTitleScreen.clear();
}

} // namespace

void UiTitleDialog()
{
	TitleLoad();
	const Point uiPosition = GetUIRectangle().position;
	// Matches TitleLoad above: the Diablo title, unconditionally. The Hellfire branch that stood
	// here composed hf_logo1 as an animated background with an optional widescreen strip behind it,
	// which is a different construction entirely rather than the same layout with other art.
	UiAddBackground(&vecTitleScreen);

	vecTitleScreen.push_back(std::make_unique<UiImageAnimatedClx>(
	    *DiabloTitleLogo, MakeSdlRect(0, uiPosition.y + 182, 0, 0), UiFlags::AlignCenter));

	SDL_Rect rect = MakeSdlRect(uiPosition.x, uiPosition.y + 410, 640, 26);
	vecTitleScreen.push_back(std::make_unique<UiArtText>(_("Copyright © 1996-2001 Blizzard Entertainment").data(), rect, UiFlags::AlignCenter | UiFlags::FontSize24 | UiFlags::ColorUiSilver));

	bool endMenu = false;
	Uint32 timeOut = SDL_GetTicks() + 7000;

	SDL_Event event;
	while (!endMenu && SDL_GetTicks() < timeOut) {
		UiRenderItems(vecTitleScreen);
		UiFadeIn();

		discord_manager::UpdateMenu();

		while (PollEvent(&event) != 0) {
			std::vector<MenuAction> menuActions = GetMenuActions(event);
			if (std::any_of(menuActions.begin(), menuActions.end(), [](auto menuAction) { return menuAction != MenuAction_NONE; })) {
				endMenu = true;
				break;
			}
			switch (event.type) {
			case SDL_KEYDOWN:
			case SDL_MOUSEBUTTONUP:
				endMenu = true;
				break;
			}
			UiHandleEvents(&event);
		}
	}

	TitleFree();
}

} // namespace devilution
