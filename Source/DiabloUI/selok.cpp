#include "DiabloUI/selok.h"

#include "DiabloUI/diabloui.h"
#include "DiabloUI/hero/hero_layout.h"
#include "control.h"
#include "engine/render/text_render.hpp"
#include "oracool/ui_backgrounds.h"
#include "utils/language.h"
#include "utils/utf8.hpp"

namespace devilution {

namespace {

char dialogText[256];

/**
 * Oracool: the lone OK, drawn every frame - it is the only control on the screen, so it is always the
 * focused one and there is nothing to navigate between.
 */
UiArtTextButton *OkButton = nullptr;

} // namespace

bool selok_endMenu;

/**
 * Split for the reason spelled out in selyesno.cpp: `UiInitList` copies whatever it is given into
 * `gUiItems`, and `UiPollAndRender` re-renders that at the END of the frame - so a background handed
 * to it repaints over anything this file draws, the focus glow included.
 *
 * `vecSelOkDialog` is the backdrop this file renders itself; `vecSelOkItems` is only what the shared
 * code must own - the button, which has to be in `gUiItems` for the mouse to find it.
 */
std::vector<std::unique_ptr<UiItemBase>> vecSelOkDialog;
std::vector<std::unique_ptr<UiItemBase>> vecSelOkItems;

void selok_Free()
{
	ArtBackground = std::nullopt;

	vecSelOkDialog.clear();
	vecSelOkItems.clear();
	OkButton = nullptr;
}

void selok_Select(int /*value*/)
{
	selok_endMenu = true;
}

void selok_Esc()
{
	selok_endMenu = true;
}

void UiSelOkDialog(const char *title, const char *body, bool background)
{
	if (!background) {
		// Oracool: user request - the same painting the delete-character prompt uses, rather than the
		// black plate this dropped to. UiLoadBlackBackground still runs: it is what loads the palette
		// (ui_art\diablo.pal) the art quantizes against, and it is the fallback if the asset is
		// missing. The Settings slot is reused rather than given its own, because this dialog loads
		// the same palette - which is the only thing a slot caches against.
		//
		// Only this branch. `background = true` still means the main-menu art, and nothing passes it:
		// all eight callers pass false, so this is the path every one of them takes.
		UiLoadBlackBackground();
		if (!oracool::AddUiBackground(&vecSelOkDialog, oracool::UiBackground::Settings))
			UiAddBackground(&vecSelOkDialog);
	} else {
		if (!gbIsSpawn) {
			LoadBackgroundArt("ui_art\\mainmenu");
		} else {
			LoadBackgroundArt("ui_art\\swmmenu");
		}
		UiAddBackground(&vecSelOkDialog);
	}

	// Oracool: the shared chrome from hero_layout.h - the logo's height, the title's band and font, the
	// centred column and the action row. The last front-end screen still composing against the 640x480
	// dialog art (`uiPosition.x + 140`, a 560px body wrapped at 400) rather than against the window.
	UiAddLogo(&vecSelOkDialog, HeroLogoTop());

	// Wrapped at the font it is drawn in. The old code wrapped at GameFont24 into a 400px width and
	// then drew into a 560px rect, so the text was breaking 160px short of its own box.
	CopyUtf8(dialogText, WordWrapString(body, HeroFormWidth, GameFont30), sizeof(dialogText));

	// With no title the message takes the title's band as well, so it does not float below a gap where
	// a heading would have been.
	const int bodyTop = title != nullptr ? HeroContentTop() : GetUIRectangle().position.y + HeroTitleTop;
	if (title != nullptr) {
		vecSelOkDialog.push_back(std::make_unique<UiArtText>(title, HeroTitleRect(),
		    UiFlags::AlignCenter | HeroTitleFontSize | UiFlags::ColorUiSilver, 3));
	}
	SDL_Rect bodyRect = MakeSdlRect(static_cast<Sint16>(HeroFormX()), static_cast<Sint16>(bodyTop),
	    HeroFormWidth, static_cast<Uint16>(std::max(0, HeroContentBottom() - bodyTop)));
	vecSelOkDialog.push_back(std::make_unique<UiArtText>(dialogText, bodyRect,
	    UiFlags::AlignCenter | HeroListFontSize | UiFlags::ColorUiSilver));

	// Centred on the action row's own line: a lone answer belongs in the middle of it, the way the
	// delete prompt's two sit either side of the middle. Three cells rather than the row's usual four
	// is what puts a single one dead centre - and keeps its click target 266px wide rather than the
	// whole 800px strip, which a one-cell row would have made quietly clickable.
	auto okButton = std::make_unique<UiArtTextButton>(_("OK"), &UiFocusNavigationSelect,
	    HeroButtonRect(1, 3), HeroButtonFlags);
	OkButton = okButton.get();
	vecSelOkItems.push_back(std::move(okButton));

	UiInitList(nullptr, selok_Select, selok_Esc, vecSelOkItems, false);

	selok_endMenu = false;
	while (!selok_endMenu) {
		UiClearScreen();
		UiRenderItems(vecSelOkDialog);
		if (OkButton != nullptr)
			DrawFocusGlow(*OkButton);
		UiPollAndRender();
	}

	selok_Free();
}
} // namespace devilution
