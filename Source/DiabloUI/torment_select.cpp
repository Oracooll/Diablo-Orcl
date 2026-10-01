#include "DiabloUI/torment_select.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <memory>
#include <vector>

#include <SDL.h>

#include "DiabloUI/diabloui.h"
#include "DiabloUI/hero/hero_layout.h"
#include "engine/render/primitive_render.hpp" // PackArgb
#include "engine/surface.hpp"
#include "oracool/ui_backgrounds.h"
#include "utils/language.h"
#include "utils/log.hpp"
#include "utils/png.h"
#include "utils/sdl_geometry.h"

namespace devilution {

namespace {

constexpr int PopupWidth = 800;
constexpr int PopupHeight = 600;
/** @brief Clearance between the window's foot and the OK / Cancel row under it. */
constexpr int PopupToButtonsGap = 12;

/** @brief The eight multipliers, in tenths, left to right as the art prints them. */
constexpr std::array<int, 8> MultiplierTenths { 15, 20, 25, 30, 35, 40, 45, 50 };
constexpr int ColumnCount = static_cast<int>(MultiplierTenths.size());

/**
 * @brief The empty circle at the top of each column, measured on the 800x600 asset (the inner ring is about 52 across;
 * focus42 sits inside it). Window coordinates.
 */
constexpr std::array<int, ColumnCount> CircleX { 75, 166, 260, 352, 446, 540, 634, 726 };
constexpr int CircleY = 95;

/** @brief Each column's clickable area: its panel from the circle to the foot of the painting, between the pillars. */
constexpr int ColumnHalfWidth = 44;
constexpr int ColumnTop = 50;
constexpr int ColumnBottom = 508;

std::vector<uint32_t> Art;
bool ArtLoadTried = false;

/** @brief ui\\torment_select_bg.png as ARGB, once. Empty if missing - the picker still works, on bare black. */
void LoadArt()
{
	if (ArtLoadTried)
		return;
	ArtLoadTried = true;
	SDL_Surface *png = LoadPNG("ui\\torment_select_bg.png");
	if (png == nullptr) {
		LogWarn("Torment picker: ui\\torment_select_bg.png not found");
		return;
	}
	SDL_Surface *rgba = SDL_ConvertSurfaceFormat(png, SDL_PIXELFORMAT_ABGR8888, 0);
	SDL_FreeSurface(png);
	if (rgba == nullptr)
		return;
	if (rgba->w == PopupWidth && rgba->h == PopupHeight) {
		Art.resize(static_cast<size_t>(PopupWidth) * PopupHeight);
		const auto *pixels = static_cast<const uint8_t *>(rgba->pixels);
		for (int y = 0; y < PopupHeight; y++) {
			const uint8_t *row = pixels + static_cast<size_t>(y) * rgba->pitch;
			for (int x = 0; x < PopupWidth; x++) {
				const uint8_t *p = row + static_cast<size_t>(x) * 4;
				Art[static_cast<size_t>(y) * PopupWidth + x] = PackArgb(255, p[0], p[1], p[2]);
			}
		}
	} else {
		LogWarn("Torment picker: ui\\torment_select_bg.png is {:d}x{:d}, not 800x600", rgba->w, rgba->h);
	}
	SDL_FreeSurface(rgba);
}

/** @brief The OK / Cancel row's top: where the difficulty screen this opens over has it (round 51 audit). */
int ButtonRowTop()
{
	return static_cast<int>(gnScreenHeight) - DifficultyButtonRowBottomMargin - HeroButtonRowHeight;
}

SDL_Rect ButtonRect(int zone)
{
	const SDL_Rect cell = HeroButtonRect(zone);
	return MakeSdlRect(cell.x, static_cast<Sint16>(ButtonRowTop()), cell.w, HeroButtonRowHeight);
}

Point PopupOrigin()
{
	// Centred across, and lifted clear of the action row so OK and Cancel stay where the difficulty screen had them.
	return { (static_cast<int>(gnScreenWidth) - PopupWidth) / 2, std::max(0, ButtonRowTop() - PopupToButtonsGap - PopupHeight) };
}

std::vector<std::unique_ptr<UiItemBase>> vecTormentBackdrop;
std::vector<std::unique_ptr<UiItemBase>> vecTormentItems;
/** @brief The column buttons, by column, to tell which one has focus. Point into vecTormentItems. */
std::array<const UiArtTextButton *, ColumnCount> ColumnButtons {};

bool EndMenu = false;
bool Confirmed = false;
/** @brief The column the pentagram is in - the one OK takes. Moves with focus, stays put while focus is on OK / Cancel. */
int Chosen = 1;

void Confirm()
{
	Confirmed = true;
	EndMenu = true;
}

void Cancel()
{
	Confirmed = false;
	EndMenu = true;
}

// A column's own action is its second click or Enter on it: take that column. Function pointers, one per column, since
// a button's callback carries no argument.
template <int Column>
void TakeColumn()
{
	Chosen = Column;
	Confirm();
}

using ColumnAction = void (*)();
constexpr std::array<ColumnAction, ColumnCount> ColumnActions {
	&TakeColumn<0>, &TakeColumn<1>, &TakeColumn<2>, &TakeColumn<3>, &TakeColumn<4>, &TakeColumn<5>, &TakeColumn<6>, &TakeColumn<7>
};

void SelectList(int /*value*/)
{
	Confirm();
}

int ColumnForTenths(int tenths)
{
	int best = 0;
	for (int i = 1; i < ColumnCount; i++) {
		if (std::abs(MultiplierTenths[i] - tenths) < std::abs(MultiplierTenths[best] - tenths))
			best = i;
	}
	return best;
}

void Free()
{
	vecTormentBackdrop.clear();
	vecTormentItems.clear();
	ColumnButtons.fill(nullptr);
}

} // namespace

Point TormentPickerCircleInArt(int column)
{
	return { CircleX[static_cast<size_t>(std::clamp(column, 0, ColumnCount - 1))], CircleY };
}

std::optional<int> UiTormentSelectDialog(int currentTenths)
{
	LoadArt();
	// The difficulty screen's painting behind the window, as it was a moment ago.
	UiLoadBlackBackground();
	if (!oracool::AddUiBackground(&vecTormentBackdrop, oracool::UiBackground::Difficulty))
		UiAddBackground(&vecTormentBackdrop);

	const Point origin = PopupOrigin();
	const bool trueColour = !Surface(DiabloUiSurface()).isIndexed();
	if (trueColour && !Art.empty()) {
		vecTormentBackdrop.push_back(std::make_unique<UiImageRgb>(Art.data(), PopupWidth, PopupHeight,
		    MakeSdlRect(origin.x, origin.y, PopupWidth, PopupHeight)));
	}

	// The columns: wordless buttons, so the shared focus ring walks them with the arrow keys and the click rule (first
	// click marks, second acts) holds for them as for every front-end button. Their own pentagram is drawn below.
	// Never down into the button row: on a window under ~720 tall the popup cannot clear it, and a column reaching over
	// OK would take its clicks and fall into its row of the focus ring (round 51 audit).
	const int columnBottom = std::min(origin.y + ColumnBottom, ButtonRowTop() - 1);
	for (int i = 0; i < ColumnCount; i++) {
		auto button = std::make_unique<UiArtTextButton>(string_view {}, ColumnActions[i],
		    MakeSdlRect(origin.x + CircleX[i] - ColumnHalfWidth, origin.y + ColumnTop, ColumnHalfWidth * 2, std::max(1, columnBottom - origin.y - ColumnTop)));
		ColumnButtons[i] = button.get();
		vecTormentItems.push_back(std::move(button));
	}
	// OK and Cancel in zones 2 and 3, where they are on every front-end screen.
	vecTormentItems.push_back(std::make_unique<UiArtTextButton>(_("OK"), &Confirm, ButtonRect(OkButtonIndex), HeroButtonFlags));
	vecTormentItems.push_back(std::make_unique<UiArtTextButton>(_("Cancel"), &Cancel, ButtonRect(CancelButtonIndex), HeroButtonFlags));

	UiInitList(nullptr, SelectList, Cancel, vecTormentItems, false);
	Chosen = ColumnForTenths(currentTenths);
	UiFocusButton(ColumnButtons[Chosen]);

	EndMenu = false;
	Confirmed = false;
	while (!EndMenu) {
		const UiArtTextButton *focused = UiFocusedButton();
		bool onColumn = false;
		for (int i = 0; i < ColumnCount; i++) {
			if (focused == ColumnButtons[i]) {
				Chosen = i;
				onColumn = true;
			}
		}
		UiClearScreen();
		UiRenderItems(vecTormentBackdrop);
		// Bright on the column with focus; dimmed, still turning, while focus is on OK or Cancel - what OK will take.
		UiDrawFocusPentagram(origin + Displacement { CircleX[Chosen], CircleY }, !onColumn);
		UiPollAndRender();
	}

	const int chosen = Chosen;
	const bool confirmed = Confirmed;
	Free();
	UiInitList_clear();
	if (!confirmed)
		return std::nullopt;
	return MultiplierTenths[chosen];
}

} // namespace devilution
