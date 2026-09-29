/**
 * @file gmenu.cpp
 *
 * Implementation of the in-game navigation and interaction.
 */
#include "gmenu.h"

#include <array>
#include <cstdint>
#include <cstring>

#include "DiabloUI/ui_flags.hpp"
#include "control.h"
#include "controls/axis_direction.h"
#include "controls/controller_motion.h"
#include "engine.h"
#include "engine/clx_sprite.hpp"
#include "engine/load_cel.hpp"
#include "engine/load_pcx.hpp" // LoadPcxSpriteList - the masthead is ui_art\smlogo, not a CEL
#include "engine/palette.h"    // orig_palette - what the masthead's TRN is matched against
#include "engine/render/clx_render.hpp"
#include "engine/render/primitive_render.hpp"
#include "engine/render/text_render.hpp"
#include "options.h"
#include "stores.h"
#include "utils/language.h"
#include "utils/stdcompat/algorithm.hpp"
#include "utils/stdcompat/optional.hpp"
#include "utils/ui_fwd.h"

namespace devilution {

namespace {

// Width of the slider menu item, including the label.
constexpr int SliderItemWidth = 490;

// Horizontal dimensions of the slider value
constexpr int SliderValueBoxLeft = 16 + SliderItemWidth / 2;
constexpr int SliderValueBoxWidth = 287;

constexpr int SliderValueBorderWidth = 2;
constexpr int SliderValueLeft = SliderValueBoxLeft + SliderValueBorderWidth;
constexpr int SliderValueWidth = SliderValueBoxWidth - 2 * SliderValueBorderWidth;
constexpr int SliderValueHeight = 29;
constexpr int SliderValuePaddingTop = 10;
constexpr int SliderMarkerWidth = 27;

constexpr int SliderFillMin = SliderMarkerWidth / 2;
constexpr int SliderFillMax = SliderValueWidth - SliderMarkerWidth / 2 - 1;

constexpr int GMenuTop = 117;
constexpr int GMenuItemHeight = 45;

OptionalOwnedClxSpriteList optbar_cel;
OptionalOwnedClxSpriteList PentSpin_cel;
OptionalOwnedClxSpriteList option_cel;
OptionalOwnedClxSpriteList sgpLogo;

/**
 * @brief The masthead's own palette, and a table translating it into the level's.
 *
 * Oracool: user report - "the fire is funny looking". It was, and the screenshot says exactly how:
 * the LETTERS render correctly and only the flames are wrong. That split is the whole diagnosis.
 *
 * `ui_art\smlogo` is front-end art (it came here at 1.5.9 to escape the crash that `data\diabsmal`
 * causes when hellfire.mpq shadows it - see gmenu_init_menu). Its letters are drawn in the palette's
 * UPPER half, which every palette in the game shares by design, so they survive being drawn in a
 * level. Its flames are down at indices 10-19, in the SCENE half - the half that is recoloured per
 * dungeon type. In town those indices are mud and stone, which is what produced the pink and red
 * blocks over the fire.
 *
 * So the sprite is drawn through a TRN that maps each of its own colours to the nearest one the
 * level palette actually has, matching on RGB rather than on index. Two things fall out of that
 * which are worth knowing: the letters barely move, because a colour already present matches itself,
 * and the fire lands on PAL16_YELLOW/ORANGE/RED (192-239, see engine/palette.h) which is where fire
 * belongs.
 *
 * Matched against `orig_palette` and restricted to 128-255, both for the same reason hud_art.cpp
 * gives: that is the level palette's stable half, and orig_palette is the real one rather than the
 * gamma-corrected copy.
 */
std::array<SDL_Color, 256> LogoPalette;
std::array<uint8_t, 256> LogoTrn;
std::array<SDL_Color, 128> LogoTrnBuiltFor;
bool LogoTrnBuilt;

bool isDraggingSlider;
TMenuItem *sgpCurrItem;
void (*gmenu_current_option)();
int sgCurrentMenuIdx;

/** @brief (Re)builds LogoTrn against the level palette in place now. Cheap enough to do per level. */
void BuildLogoTrn()
{
	for (int src = 0; src < 256; src++) {
		const SDL_Color &want = LogoPalette[src];
		int best = 128;
		int bestDist = INT32_MAX;
		for (int i = 128; i < 256; i++) {
			const SDL_Color &have = orig_palette[i];
			const int dr = static_cast<int>(have.r) - want.r;
			const int dg = static_cast<int>(have.g) - want.g;
			const int db = static_cast<int>(have.b) - want.b;
			// The same channel weighting the HUD's quantizer uses, so a colour cannot land in one
			// place there and a visibly different one here.
			const int dist = 2 * dr * dr + 4 * dg * dg + 3 * db * db;
			if (dist < bestDist) {
				bestDist = dist;
				best = i;
			}
		}
		LogoTrn[src] = static_cast<uint8_t>(best);
	}
	std::memcpy(LogoTrnBuiltFor.data(), &orig_palette[128], sizeof(LogoTrnBuiltFor));
	LogoTrnBuilt = true;
}

void GmenuUpDown(bool isDown)
{
	if (sgpCurrItem == nullptr) {
		return;
	}
	isDraggingSlider = false;
	int i = sgCurrentMenuIdx;
	if (sgCurrentMenuIdx != 0) {
		while (i != 0) {
			i--;
			if (isDown) {
				sgpCurrItem++;
				if (sgpCurrItem->fnMenu == nullptr)
					sgpCurrItem = &sgpCurrentMenu[0];
			} else {
				if (sgpCurrItem == sgpCurrentMenu)
					sgpCurrItem = &sgpCurrentMenu[sgCurrentMenuIdx];
				sgpCurrItem--;
			}
			if (sgpCurrItem->enabled()) {
				if (i != 0)
					PlaySFX(IS_TITLEMOV);
				return;
			}
		}
	}
}

void GmenuLeftRight(bool isRight)
{
	if (!sgpCurrItem->isSlider())
		return;

	uint16_t step = sgpCurrItem->sliderStep();
	if (isRight) {
		if (step >= sgpCurrItem->sliderSteps()) // never past the last step, whatever set it (audit, 2026-09-29)
			return;
		step++;
	} else {
		if (step == 0)
			return;
		step--;
	}
	sgpCurrItem->setSliderStep(step);
	sgpCurrItem->fnMenu(false);
}

int GmenuGetLineWidth(TMenuItem *pItem)
{
	if (pItem->isSlider())
		return SliderItemWidth;

	// Oracool: user request - reduced from GameFont46 to GameFont42. Must stay in sync with the
	// draw flag in GmenuDrawMenuItem below, since this width is what centers the text horizontally
	// - measuring at the old (larger) size while drawing at the new (smaller) one would shift the
	// text off-center instead of properly centering the now-narrower string.
	return GetLineWidth(_(pItem->pszStr), GameFont42, 2);
}

void GmenuDrawMenuItem(const Surface &out, TMenuItem *pItem, int y)
{
	int w = GmenuGetLineWidth(pItem);
	if (pItem->isSlider()) {
		int uiPositionX = GetUIRectangle().position.x;
		ClxDraw(out, { SliderValueBoxLeft + uiPositionX, y + 40 }, (*optbar_cel)[0]);
		const uint16_t step = pItem->dwFlags & 0xFFF;
		const uint16_t steps = std::max<uint16_t>(pItem->sliderSteps(), 2);
		const uint16_t pos = SliderFillMin + step * (SliderFillMax - SliderFillMin) / steps;
		SDL_Rect rect = MakeSdlRect(SliderValueLeft + uiPositionX, y + SliderValuePaddingTop, pos, SliderValueHeight);
		FillRect(out, rect.x, rect.y, rect.w, rect.h, 205); // an index through the surface's fill, not SDL's (v1.11)
		ClxDraw(out, { SliderValueLeft + pos - SliderMarkerWidth / 2 + uiPositionX, y + SliderValuePaddingTop + SliderValueHeight - 1 }, (*option_cel)[0]);
	}

	int x = (gnScreenWidth - w) / 2;
	UiFlags style = pItem->enabled() ? UiFlags::ColorGold : UiFlags::ColorBlack;
	DrawString(out, _(pItem->pszStr), Point { x, y }, { style | UiFlags::FontSize42, 2 });
	if (pItem == sgpCurrItem) {
		const ClxSprite sprite = (*PentSpin_cel)[PentSpn2Spin()];
		ClxDraw(out, { x - 54, y + 51 }, sprite);
		ClxDraw(out, { x + 4 + w, y + 51 }, sprite);
	}
}

void GameMenuMove()
{
	static AxisDirectionRepeater repeater;
	const AxisDirection moveDir = repeater.Get(GetLeftStickOrDpadDirection(false));
	if (moveDir.x != AxisDirectionX_NONE)
		GmenuLeftRight(moveDir.x == AxisDirectionX_RIGHT);
	if (moveDir.y != AxisDirectionY_NONE)
		GmenuUpDown(moveDir.y == AxisDirectionY_DOWN);
}

bool GmenuMouseIsOverSlider()
{
	int uiPositionX = GetUIRectangle().position.x;
	if (MousePosition.x < SliderValueLeft + uiPositionX) {
		return false;
	}
	if (MousePosition.x >= SliderValueLeft + SliderValueWidth + uiPositionX) {
		return false;
	}
	return true;
}

int GmenuGetSliderFill()
{
	return clamp(MousePosition.x - SliderValueLeft - GetUIRectangle().position.x, SliderFillMin, SliderFillMax);
}

} // namespace

TMenuItem *sgpCurrentMenu;

void gmenu_draw_pause(const Surface &out)
{
	if (leveltype != DTYPE_TOWN)
		RedBack(out);
	if (sgpCurrentMenu == nullptr) {
		LightTableIndex = 0;
		// Oracool: user request - reduced from FontSize46 to FontSize42, matching the menu item
		// text size change in GmenuDrawMenuItem/GmenuGetLineWidth above, so the "Pause" title and
		// the menu entries below it stay visually consistent.
		DrawString(out, _("Pause"), { { 0, 0 }, { gnScreenWidth, GetMainPanel().position.y } }, { UiFlags::FontSize42 | UiFlags::ColorGold | UiFlags::AlignCenter | UiFlags::VerticalCenter, 2 });
	}
}

void FreeGMenu()
{
	sgpLogo = std::nullopt;
	PentSpin_cel = std::nullopt;
	option_cel = std::nullopt;
	optbar_cel = std::nullopt;
}

void gmenu_init_menu()
{
	sgpCurrentMenu = nullptr;
	sgpCurrItem = nullptr;
	gmenu_current_option = nullptr;
	sgCurrentMenuIdx = 0;
	isDraggingSlider = false;

	if (HeadlessMode)
		return;

	// Oracool: user request - the pause menu must not say HELLFIRE. It shows ui_art\smlogo, the same
	// Diablo masthead the front end uses, in both modes.
	//
	// Neither of the two files vanilla picks between works here. `data\hf_logo3` IS the Hellfire
	// wordmark. `data\diabsmal` is Diablo's, but it is one of the seven assets hellfire.mpq shadows -
	// its copy is 5,341 bytes against diabdat's 10,829, a different image, because vanilla Hellfire
	// never loads that file at all. Forcing it at 1.5.1 crashed on entering a game: decoded at width
	// 296 it ran off the end of the buffer, an out-of-bounds read in AppendClxPixelsOrFillRun.
	//
	// smlogo is the way out and needs no branch: it is NOT shadowed, so it comes from diabdat
	// whatever archives are present, and nothing has to be shipped inside oracool.mpq - which is
	// what the alternative, packing Blizzard's diabsmal.cel, would have meant. It is a PCX sprite
	// list rather than a CEL, but both load to the same OwnedClxSpriteList, and gmenu_draw centres
	// the sprite by its own width, so the 390px masthead needs no repositioning.
	// The palette comes out with it now - the TRN above needs to know what smlogo's own indices mean
	// before it can say what they should become here.
	sgpLogo = LoadPcxSpriteList("ui_art\\smlogo", /*numFrames=*/15, /*transparentColor=*/250, LogoPalette.data());
	LogoTrnBuilt = false;
	PentSpin_cel = LoadCel("data\\pentspin", 48);
	option_cel = LoadCel("data\\option", SliderMarkerWidth);
	optbar_cel = LoadCel("data\\optbar", SliderValueBoxWidth);
}

bool gmenu_is_active()
{
	return sgpCurrentMenu != nullptr;
}

void gmenu_set_items(TMenuItem *pItem, void (*gmFunc)())
{
	PauseMode = 0;
	isDraggingSlider = false;
	sgpCurrentMenu = pItem;
	gmenu_current_option = gmFunc;
	if (gmenu_current_option != nullptr) {
		gmenu_current_option();
	}
	sgCurrentMenuIdx = 0;
	if (sgpCurrentMenu != nullptr) {
		for (int i = 0; sgpCurrentMenu[i].fnMenu != nullptr; i++) {
			sgCurrentMenuIdx++;
		}
	}
	// BUGFIX: OOB access when sgCurrentMenuIdx is 0; should be set to NULL instead. (fixed)
	sgpCurrItem = sgCurrentMenuIdx > 0 ? &sgpCurrentMenu[sgCurrentMenuIdx - 1] : nullptr;
	GmenuUpDown(true);
	if (sgpCurrentMenu == nullptr)
		SaveOptions();
}

void gmenu_draw(const Surface &out)
{
	if (sgpCurrentMenu != nullptr) {
		GameMenuMove();
		if (gmenu_current_option != nullptr)
			gmenu_current_option();
		int uiPositionY = GetUIRectangle().position.y;
		// Oracool: the banner is ui_art\smlogo in both modes now (see gmenu_init_menu), so it
		// animates in both rather than only under Hellfire, and the frame count comes from the sheet
		// instead of a literal 16 - smlogo has 15, and running past the end would index a sprite that
		// is not there. Guarded, because a masthead that failed to load should cost the menu its
		// picture and nothing else.
		if (sgpLogo) {
			// Rebuilt when the level palette's stable half actually moves, which in practice is once
			// per level load, and never while the menu is up.
			if (!LogoTrnBuilt || std::memcmp(LogoTrnBuiltFor.data(), &orig_palette[128], sizeof(LogoTrnBuiltFor)) != 0)
				BuildLogoTrn();

			const ClxSpriteList frames { *sgpLogo };
			// Oracool: user report - "the animation is too fast". It was hand-rolled here at a frame
			// every 25ms: 15 frames in under four tenths of a second, a flicker rather than a flame.
			// GetAnimationFrame is the shared clock the FRONT END's logo runs on, at 60ms a frame, so
			// using it both slows this to something that reads as fire and makes the two mastheads
			// animate at one speed instead of two. The tick counter and frame index it replaces were
			// this file's alone.
			const ClxSprite sprite = frames[GetAnimationFrame(static_cast<int>(frames.numSprites()))];
			ClxDrawTRN(out, { (gnScreenWidth - sprite.width()) / 2, 102 + uiPositionY }, sprite, LogoTrn.data());
		}
		int y = 110 + uiPositionY;
		TMenuItem *i = sgpCurrentMenu;
		if (sgpCurrentMenu->fnMenu != nullptr) {
			while (i->fnMenu != nullptr) {
				GmenuDrawMenuItem(out, i, y);
				i++;
				y += GMenuItemHeight;
			}
		}
	}
}

bool gmenu_presskeys(SDL_Keycode vkey)
{
	if (sgpCurrentMenu == nullptr)
		return false;
	switch (vkey) {
	case SDLK_KP_ENTER:
	case SDLK_RETURN:
		if (sgpCurrItem->enabled()) {
			PlaySFX(IS_TITLEMOV);
			sgpCurrItem->fnMenu(true);
		}
		break;
	case SDLK_ESCAPE:
		PlaySFX(IS_TITLEMOV);
		gmenu_set_items(nullptr, nullptr);
		break;
	case SDLK_SPACE:
		return false;
	case SDLK_LEFT:
		GmenuLeftRight(false);
		break;
	case SDLK_RIGHT:
		GmenuLeftRight(true);
		break;
	case SDLK_UP:
		GmenuUpDown(false);
		break;
	case SDLK_DOWN:
		GmenuUpDown(true);
		break;
	default:
		break;
	}
	return true;
}

bool gmenu_on_mouse_move()
{
	if (!isDraggingSlider)
		return false;

	const uint16_t step = sgpCurrItem->sliderSteps() * (GmenuGetSliderFill() - SliderFillMin) / (SliderFillMax - SliderFillMin);
	sgpCurrItem->setSliderStep(step);
	sgpCurrItem->fnMenu(false);

	return true;
}

bool gmenu_left_mouse(bool isDown)
{
	if (!isDown) {
		if (isDraggingSlider) {
			isDraggingSlider = false;
			return true;
		}
		return false;
	}

	if (sgpCurrentMenu == nullptr) {
		return false;
	}
	const Point uiPosition = GetUIRectangle().position;
	if (MousePosition.y >= GetMainPanel().position.y) {
		return false;
	}
	if (MousePosition.y - (GMenuTop + uiPosition.y) < 0) {
		return true;
	}
	int i = (MousePosition.y - (GMenuTop + uiPosition.y)) / GMenuItemHeight;
	if (i >= sgCurrentMenuIdx) {
		return true;
	}
	TMenuItem *pItem = &sgpCurrentMenu[i];
	if (!pItem->enabled()) {
		return true;
	}
	int w = GmenuGetLineWidth(pItem);
	uint16_t screenWidth = GetScreenWidth();
	if (MousePosition.x < screenWidth / 2 - w / 2) {
		return true;
	}
	if (MousePosition.x > screenWidth / 2 + w / 2) {
		return true;
	}
	sgpCurrItem = pItem;
	PlaySFX(IS_TITLEMOV);
	if (pItem->isSlider()) {
		isDraggingSlider = GmenuMouseIsOverSlider();
		gmenu_on_mouse_move();
	} else {
		sgpCurrItem->fnMenu(true);
	}
	return true;
}

void gmenu_slider_set(TMenuItem *pItem, int min, int max, int value)
{
	assert(pItem);
	uint16_t nSteps = std::max<uint16_t>(pItem->sliderSteps(), 2);
	pItem->setSliderStep(((max - min - 1) / 2 + (value - min) * nSteps) / (max - min));
}

int gmenu_slider_get(TMenuItem *pItem, int min, int max)
{
	uint16_t step = pItem->sliderStep();
	uint16_t steps = std::max<uint16_t>(pItem->sliderSteps(), 2);
	return min + (step * (max - min) + (steps - 1) / 2) / steps;
}

void gmenu_slider_steps(TMenuItem *pItem, int steps)
{
	pItem->dwFlags &= 0xFF000FFF;
	pItem->setSliderSteps(steps);
}

} // namespace devilution
