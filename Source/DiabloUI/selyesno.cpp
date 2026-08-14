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
 * Oracool: the two answers, in the second and third of the four button columns (the user's naming for
 * the action row's cells). They are buttons rather than the two-row list this dialog used to show,
 * because a list is vertical and the row is not - and because it puts the confirmation on the same
 * line as every other screen's controls, which is the whole point of bringing it up to date.
 */
UiArtTextButton *AnswerButtons[2] = {};
int SelectedAnswer = 0;
constexpr int YesAnswer = 0;
constexpr int NoAnswer = 1;

/** The FontSize30 line height, for measuring the wrapped message so the figure can sit under it. */
constexpr int MessageLineHeight = 38;
/** Clearance between the message and the character standing below it. */
constexpr int MessageToFigureGap = 16;

/** @brief Where the character being deleted stands: the band under the message. */
Rectangle FigureRect(int messageBottom)
{
	const int top = messageBottom + MessageToFigureGap;
	return { { HeroFormX(), top }, { HeroFormWidth, std::max(0, HeroContentBottom() - top) } };
}

void SelyesnoFree()
{
	ArtBackground = std::nullopt;
	oracool::FreeHeroPreview();

	vecSelYesNoDialog.clear();
	vecSelYesNoItems.clear();
	for (UiArtTextButton *&button : AnswerButtons)
		button = nullptr;
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
 * @brief Enter, from the shared focus model - which has no idea these buttons exist.
 *
 * `UiFocusNavigationSelect` passes the selected LIST row, and this dialog no longer has a list. The
 * argument is therefore ignored and the focused button activated instead, which is also what keeps a
 * controller working: it reaches the same shared action.
 */
void SelyesnoSelect(int /*value*/)
{
	if (AnswerButtons[SelectedAnswer] != nullptr)
		AnswerButtons[SelectedAnswer]->Activate();
}

/** @brief Left/right between the two answers. Anything else is handed on untouched. */
bool SelyesnoNavigation(SDL_Event &event)
{
	if (event.type != SDL_KEYDOWN)
		return false;
	switch (event.key.keysym.sym) {
	case SDLK_LEFT:
		SelectedAnswer = YesAnswer;
		return true;
	case SDLK_RIGHT:
		SelectedAnswer = NoAnswer;
		return true;
	default:
		return false;
	}
}

} // namespace

bool UiSelHeroYesNoDialog(const char *title, const char *body)
{
	// Oracool: user request - the delete-character confirmation shares the hero/settings painting
	// rather than dropping to a black plate. UiLoadBlackBackground still runs: it is what loads the
	// palette (ui_art\diablo.pal) the art quantizes against, and it is the fallback if the asset is
	// missing. The Settings slot is reused rather than given its own, because this dialog loads the
	// same palette - which is the only thing a slot caches against.
	UiLoadBlackBackground();
	if (!oracool::AddUiBackground(&vecSelYesNoDialog, oracool::UiBackground::Settings))
		UiAddBackground(&vecSelYesNoDialog);

	// Oracool: user request - "bring the Delete Character screen up to date with the other screens".
	// Everything below comes from hero_layout.h, so this screen cannot drift from them again: the
	// logo's height, the title's band and font, the content column and the action row are all the ones
	// the character, class and name screens use.
	UiAddLogo(&vecSelYesNoDialog, HeroLogoTop());

	vecSelYesNoDialog.push_back(std::make_unique<UiArtText>(title, HeroTitleRect(),
	    UiFlags::AlignCenter | HeroTitleFontSize | UiFlags::ColorUiSilver, 3));

	// Wrapped at the same font it is drawn in - GameFont24 here with FontSize30 text would wrap to the
	// wrong width and overrun the column.
	CopyUtf8(selyesno_confirmationMessage, WordWrapString(body, HeroFormWidth, GameFont30),
	    sizeof(selyesno_confirmationMessage));

	// Measured from the wrapped text rather than given a fixed height, so the figure below sits under
	// the message whether it came out two lines or four. WordWrapString has already put the breaks in.
	int messageLines = 1;
	for (const char *c = selyesno_confirmationMessage; *c != '\0'; ++c) {
		if (*c == '\n')
			messageLines++;
	}
	const int messageHeight = messageLines * MessageLineHeight;

	SDL_Rect messageRect = MakeSdlRect(static_cast<Sint16>(HeroFormX()), static_cast<Sint16>(HeroContentTop()),
	    HeroFormWidth, static_cast<Uint16>(messageHeight));
	vecSelYesNoDialog.push_back(std::make_unique<UiArtText>(selyesno_confirmationMessage, messageRect,
	    UiFlags::AlignCenter | HeroListFontSize | UiFlags::ColorUiSilver));

	auto yesButton = std::make_unique<UiArtTextButton>(_("Yes"), &SelyesnoYes,
	    HeroButtonRect(DeleteButtonIndex), HeroButtonFlags);
	AnswerButtons[YesAnswer] = yesButton.get();
	vecSelYesNoItems.push_back(std::move(yesButton));

	auto noButton = std::make_unique<UiArtTextButton>(_("No"), &SelyesnoNo,
	    HeroButtonRect(NewHeroButtonIndex), HeroButtonFlags);
	AnswerButtons[NoAnswer] = noButton.get();
	vecSelYesNoItems.push_back(std::move(noButton));

	UiInitList(nullptr, SelyesnoSelect, SelyesnoEsc, vecSelYesNoItems);

	// Yes, as the list's first row was - deliberately unchanged. Defaulting a destructive prompt to No
	// would be defensible, but it is a behaviour change and this pass is about how the screen looks.
	SelectedAnswer = YesAnswer;
	selyesno_value = false;
	selyesno_endMenu = false;
	const Rectangle figureRect = FigureRect(HeroContentTop() + messageHeight);
	while (!selyesno_endMenu) {
		UiClearScreen();
		UiRenderItems(vecSelYesNoDialog);
		// The character this is asking about. Drawn directly rather than as a UiItemBase for the same
		// reason the character screen draws its own: it animates off the shared frame clock and has
		// nothing to click. A no-op if selhero did not load one.
		oracool::DrawHeroPreview(Surface(DiabloUiSurface()), figureRect);
		// Before UiPollAndRender, which renders the buttons: the halo goes down first and the label is
		// then drawn over its centre, which is where it belongs.
		if (AnswerButtons[SelectedAnswer] != nullptr)
			DrawFocusGlow(*AnswerButtons[SelectedAnswer]);
		UiPollAndRender(SelyesnoNavigation);
	}

	SelyesnoFree();

	return selyesno_value;
}
} // namespace devilution
