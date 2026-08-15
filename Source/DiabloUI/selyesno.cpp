#include "selyesno.h"

#include "DiabloUI/diabloui.h"
#include "DiabloUI/hero/hero_layout.h"
#include "control.h"
#include "oracool/hero_preview.h"
#include "oracool/ui_backgrounds.h"
#include "utils/language.h"
#include "utils/utf8.hpp"

namespace devilution {
namespace {

bool selyesno_endMenu;
bool selyesno_value;
char selyesno_confirmationMessage[256];

/**
 * The screen splits in two, and it has to.
 *
 * `UiInitList` copies every item it is given into `gUiItems`, and `UiPollAndRender` re-renders that
 * whole list at the END of each frame. So anything handed to it is drawn AFTER this file's own
 * drawing - and if the background is in there, it repaints over the character and the focus glow every
 * frame. That is exactly what happened: the figure never appeared and the glow on Yes/No was invisible.
 *
 * So `vecSelYesNoDialog` is the backdrop this file renders itself, in order, and `vecSelYesNoItems` is
 * only what the shared code needs to own - the buttons, which must be in `gUiItems` for the mouse to
 * find them. The character screen has always been split this way; the dialogs were not.
 */
std::vector<std::unique_ptr<UiItemBase>> vecSelYesNoDialog;
std::vector<std::unique_ptr<UiItemBase>> vecSelYesNoItems;

/**
 * Oracool: the two answers are buttons rather than the two-row list this dialog used to show, because
 * a list is vertical and the row is not - and because it puts the confirmation on the same line as
 * every other screen's controls, which is the whole point of bringing it up to date.
 *
 * They sit in zones 2 and 3, which is to say in OK's and Cancel's places: Yes IS this screen's OK and
 * No is its Cancel, and the user's rule fixes those two zones on every front-end screen. Under the old
 * indices this file named the same two cells `Delete` and `New Hero`, which put the right buttons in
 * the right places for the wrong reason.
 *
 * `AnswerButtons` and `SelectedAnswer` are gone with them - the shared focus rule owns the row now,
 * marks Yes on entry because it is the leftmost, and walks the pair with Left and Right.
 */

// MessageLineHeight, MessageToFigureGap and FigureRect are gone with the sentence they existed to
// lay out. The figure stands on HeroPreviewRect() now - the character screens' own dais - so there
// is nothing left on this screen whose position depends on how long a piece of text came out.

void SelyesnoFree()
{
	ArtBackground = std::nullopt;
	oracool::FreeHeroPreview();

	vecSelYesNoDialog.clear();
	vecSelYesNoItems.clear();
}

void SelyesnoAnswer(bool value)
{
	selyesno_value = value;
	selyesno_endMenu = true;
}

void SelyesnoYes()
{
	SelyesnoAnswer(true);
}

void SelyesnoNo()
{
	SelyesnoAnswer(false);
}

void SelyesnoEsc()
{
	SelyesnoAnswer(false);
}

/**
 * @brief Enter on a screen with no list.
 *
 * Both of the things this used to do are the shared rule's now: Enter activates the focused button
 * itself, and Left/Right walk the pair. Kept as the list-select callback only so the shared code has
 * something non-null to reach if it ever gets here without a button focused - answering No, which is
 * the safe answer to a prompt about deleting a character.
 */
void SelyesnoSelect(int /*value*/)
{
	SelyesnoAnswer(false);
}

} // namespace

bool UiSelHeroYesNoDialog(const char *title, const char *heroName)
{
	// Oracool: user request - the delete prompt wears the character screens' painting now, the same
	// cathedral the hero list stands in. UiLoadBlackBackground still runs: it is what loads the
	// palette (ui_art\diablo.pal) the art quantizes against, and it is the fallback if the asset is
	// missing.
	//
	// The HeroSelect SLOT and not a new one, so the two screens share a single cached, quantized copy
	// of a 1916x821 painting instead of holding two. That is safe because AddUiBackground pins
	// ui_art\diablo.pal before it quantizes, so every slot is built against the same palette - which
	// is the only thing a slot caches against.
	UiLoadBlackBackground();
	if (!oracool::AddUiBackground(&vecSelYesNoDialog, oracool::UiBackground::HeroSelect))
		UiAddBackground(&vecSelYesNoDialog);

	// Oracool: user request - "bring the Delete Character screen up to date with the other screens".
	// Everything below comes from hero_layout.h, so this screen cannot drift from them again: the
	// logo's height, the title's band and font, the content column and the action row are all the ones
	// the character, class and name screens use.
	UiAddLogo(&vecSelYesNoDialog, HeroLogoTop());

	vecSelYesNoDialog.push_back(std::make_unique<UiArtText>(title, HeroTitleRect(),
	    UiFlags::AlignCenter | HeroTitleFontSize | UiFlags::ColorUiSilver, 3));

	// Oracool: user request - the long "Are you sure you want to delete the character ..." sentence is
	// gone. The title above already says what the screen is for and the figure below is the character
	// it means, so the sentence was restating the screen in words.
	//
	// The NAME is kept, alone, and that is a deliberate line to hold: without it this prompt cannot
	// tell two characters of one class apart, and it is the one screen in the front end where picking
	// the wrong one cannot be undone. The caller passes the bare name now rather than a sentence with
	// the name inside it - reading it back out of a translated string would have worked in English
	// and quietly stopped working everywhere else.
	CopyUtf8(selyesno_confirmationMessage, heroName, sizeof(selyesno_confirmationMessage));
	vecSelYesNoDialog.push_back(std::make_unique<UiArtText>(selyesno_confirmationMessage,
	    HeroCaptionRect(HeroFormX(), HeroFormWidth),
	    UiFlags::AlignCenter | HeroTitleFontSize | UiFlags::ColorUiGold));

	vecSelYesNoItems.push_back(std::make_unique<UiArtTextButton>(_("Yes"), &SelyesnoYes,
	    HeroButtonRect(OkButtonIndex), HeroButtonFlags));
	vecSelYesNoItems.push_back(std::make_unique<UiArtTextButton>(_("No"), &SelyesnoNo,
	    HeroButtonRect(CancelButtonIndex), HeroButtonFlags));

	// Marks Yes, being the leftmost of the two - which is where this screen's own default was.
	// Defaulting a destructive prompt to No would be defensible, but it is a behaviour change and
	// neither this pass nor the one before it is about that.
	UiInitList(nullptr, SelyesnoSelect, SelyesnoEsc, vecSelYesNoItems);

	selyesno_value = false;
	selyesno_endMenu = false;
	while (!selyesno_endMenu) {
		UiClearScreen();
		UiRenderItems(vecSelYesNoDialog);
		// The character this is asking about, on the same dais and at the same size as the screen the
		// player just came from (user request) - HeroPreviewRect is shared, so "the same" is a fact
		// rather than a pair of matching numbers. Drawn directly rather than as a UiItemBase for the
		// reason the character screen draws its own: it animates off the shared frame clock and has
		// nothing to click. A no-op if selhero did not load one.
		oracool::DrawHeroPreview(Surface(DiabloUiSurface()), HeroPreviewRect());
		UiPollAndRender();
	}

	SelyesnoFree();

	return selyesno_value;
}
} // namespace devilution
