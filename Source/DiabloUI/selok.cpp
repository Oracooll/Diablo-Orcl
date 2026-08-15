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

// The lone OK used to be caught in a pointer here and have its focus drawn by hand every frame. The
// shared focus rule marks it on entry instead, being the only focusable thing on the screen.

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

	// Zone 2, where OK is on every other screen. This used to quarter the row into thirds to put its
	// single button dead centre; under the user's zone rule OK has a place and does not move to suit
	// the number of buttons beside it - here there simply are none.
	vecSelOkItems.push_back(std::make_unique<UiArtTextButton>(_("OK"), &UiFocusNavigationSelect,
	    HeroButtonRect(OkButtonIndex), HeroButtonFlags));

	UiInitList(nullptr, selok_Select, selok_Esc, vecSelOkItems, false);

	selok_endMenu = false;
	while (!selok_endMenu) {
		UiClearScreen();
		UiRenderItems(vecSelOkDialog);
		UiPollAndRender();
	}

	selok_Free();
}
} // namespace devilution
