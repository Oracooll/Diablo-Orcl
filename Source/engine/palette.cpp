/**
 * @file palette.cpp
 *
 * Implementation of functions for handling the engines color palette.
 */
#include "engine/palette.h"

#include <cstdint>
#include <cstring>

#include <fmt/core.h>

#include "engine/backbuffer_state.hpp"
#include "engine/demomode.h"
#include "engine/dx.h"
#include "engine/load_file.hpp"
#include "engine/random.hpp"
#include "hwcursor.hpp"
#include "levels/gendung.h"
#include "options.h"
#include "oracool/zone_registry.h"
#include "utils/display.h"
#include "utils/sdl_compat.h"

namespace devilution {

std::array<SDL_Color, 256> logical_palette;
std::array<SDL_Color, 256> system_palette;
std::array<SDL_Color, 256> orig_palette;

/**
 * @brief Lookup table for the average of two colors in `logical_palette`.
 */
uint8_t paletteTransparencyLookup[256][256];

#if DEVILUTIONX_PALETTE_TRANSPARENCY_BLACK_16_LUT
uint16_t paletteTransparencyLookupBlack16[65536];
#endif

namespace {

/** Specifies whether the palette has max brightness. */
bool sgbFadedIn = true;

void LoadGamma()
{
	int gammaValue = *sgOptions.Graphics.gammaCorrection;
	gammaValue = clamp(gammaValue, 30, 100);
	sgOptions.Graphics.gammaCorrection.SetValue(gammaValue - gammaValue % 5);
}

Uint8 FindBestMatchForColor(std::array<SDL_Color, 256> &palette, SDL_Color color, int skipFrom, int skipTo)
{
	Uint8 best;
	Uint32 bestDiff = SDL_MAX_UINT32;
	for (int i = 0; i < 256; i++) {
		if (i >= skipFrom && i <= skipTo)
			continue;
		int diffr = palette[i].r - color.r;
		int diffg = palette[i].g - color.g;
		int diffb = palette[i].b - color.b;
		Uint32 diff = diffr * diffr + diffg * diffg + diffb * diffb;

		if (bestDiff > diff) {
			best = i;
			bestDiff = diff;
		}
	}
	return best;
}

/**
 * @brief Generate lookup table for transparency
 *
 * This is based of the same technique found in Quake2.
 *
 * To mimic 50% transparency we figure out what colors in the existing palette are the best match for the combination of any 2 colors.
 * We save this into a lookup table for use during rendering.
 *
 * @param palette The colors to operate on
 * @param skipFrom Do not use colors between this index and skipTo
 * @param skipTo Do not use colors between skipFrom and this index
 * @param toUpdate Only update the first n colors
 */
void GenerateBlendedLookupTable(std::array<SDL_Color, 256> &palette, int skipFrom, int skipTo, int toUpdate = 256)
{
	for (int i = 0; i < 256; i++) {
		for (int j = 0; j < 256; j++) {
			if (i == j) { // No need to calculate transparency between 2 identical colors
				paletteTransparencyLookup[i][j] = j;
				continue;
			}
			if (i > j) { // Half the blends will be mirror identical ([i][j] is the same as [j][i]), so simply copy the existing combination.
				paletteTransparencyLookup[i][j] = paletteTransparencyLookup[j][i];
				continue;
			}
			if (i > toUpdate && j > toUpdate) {
				continue;
			}

			SDL_Color blendedColor;
			blendedColor.r = ((int)palette[i].r + (int)palette[j].r) / 2;
			blendedColor.g = ((int)palette[i].g + (int)palette[j].g) / 2;
			blendedColor.b = ((int)palette[i].b + (int)palette[j].b) / 2;
			Uint8 best = FindBestMatchForColor(palette, blendedColor, skipFrom, skipTo);
			paletteTransparencyLookup[i][j] = best;
		}
	}

#if DEVILUTIONX_PALETTE_TRANSPARENCY_BLACK_16_LUT
	for (unsigned i = 0; i < 256; ++i) {
		for (unsigned j = 0; j < 256; ++j) {
			const std::uint16_t index = i | (j << 8);
			paletteTransparencyLookupBlack16[index] = paletteTransparencyLookup[0][i] | (paletteTransparencyLookup[0][j] << 8);
		}
	}
#endif
}

/**
 * @brief Cycle the given range of colors in the palette
 * @param from First color index of the range
 * @param to First color index of the range
 */
void CycleColors(int from, int to)
{
	std::rotate(system_palette.begin() + from, system_palette.begin() + from + 1, system_palette.begin() + to + 1);

	for (auto &palette : paletteTransparencyLookup) {
		std::rotate(std::begin(palette) + from, std::begin(palette) + from + 1, std::begin(palette) + to + 1);
	}

	std::rotate(&paletteTransparencyLookup[from][0], &paletteTransparencyLookup[from + 1][0], &paletteTransparencyLookup[to + 1][0]);
}

/**
 * @brief Cycle the given range of colors in the palette in reverse direction
 * @param from First color index of the range
 * @param to Last color index of the range
 */
void CycleColorsReverse(int from, int to)
{
	std::rotate(system_palette.begin() + from, system_palette.begin() + to, system_palette.begin() + to + 1);

	for (auto &palette : paletteTransparencyLookup) {
		std::rotate(std::begin(palette) + from, std::begin(palette) + to, std::begin(palette) + to + 1);
	}

	std::rotate(&paletteTransparencyLookup[from][0], &paletteTransparencyLookup[to][0], &paletteTransparencyLookup[to + 1][0]);
}

} // namespace

std::array<uint32_t, 256> PaletteRGB {};
uint32_t PaletteRgbGeneration = 1;
int FadeLevel = 256;
bool PresentRedFlash = false;

namespace {

void RebuildPaletteRgb(const std::array<SDL_Color, 256> &source, int first, int ncolor)
{
	// The generation moves only when an entry at or above 32 changed (audit, 2026-09-08): the
	// colour-cycling levels rewrite 1-31 every frame and a fade rebuilds an unchanged palette, and
	// each bump made every text colour and frost table rebake. Nothing derived from the palette
	// reads the cycling band.
	bool changed = false;
	for (int i = first; i < first + ncolor; i++) {
		const SDL_Color &c = source[i];
		const uint32_t rgb = (static_cast<uint32_t>(c.r) << 16) | (static_cast<uint32_t>(c.g) << 8) | c.b;
		if (i >= 32 && PaletteRGB[i] != rgb)
			changed = true;
		PaletteRGB[i] = rgb;
	}
	if (changed)
		PaletteRgbGeneration++;
}

} // namespace

void palette_update(int first, int ncolor)
{
	if (HeadlessMode)
		return;

	assert(Palette);
	// The SDL palette object still carries the displayed colours: the hardware cursor and every
	// 8-bit offscreen surface that binds it read from there. The back buffer itself is 32-bit since
	// v1.11 and takes no palette; it takes PaletteRGB, below.
	if (SDL_SetPaletteColors(Palette.get(), system_palette.data(), first, ncolor) < 0) {
		ErrSdl();
	}
	// From the UNFADED palette while a fade is running - see PaletteRGB's note in palette.h.
	RebuildPaletteRgb(FadeLevel != 256 ? logical_palette : system_palette, first, ncolor);
	PresentRedFlash = false;
	pal_surface_palette_version++;
}

void ApplyGamma(std::array<SDL_Color, 256> &dst, const std::array<SDL_Color, 256> &src, int n)
{
	// Gamma is a PRESENT-TIME transform since v1.11.022 (audit, 2026-09-08): the curve is applied
	// to every screen pixel as it is shown (dx.cpp, ApplyPresentTransforms), so the palette world
	// and everything drawn as colour values - text, HUD art, the paintings - follow the slider
	// together. The palettes therefore stay linear; this keeps its name and its callers and copies.
	for (int i = 0; i < n; i++)
		dst[i] = src[i];
	RedrawEverything();
}

void palette_init()
{
	LoadGamma();
	system_palette = orig_palette;
	InitPalette();
}

void LoadPalette(const char *pszFileName, bool blend /*= true*/)
{
	assert(pszFileName);

	if (HeadlessMode)
		return;

	struct Color {
		uint8_t r;
		uint8_t g;
		uint8_t b;
	};

	std::array<Color, 256> palData;

	LoadFileInMem(pszFileName, palData);

	for (unsigned i = 0; i < palData.size(); i++) {
		orig_palette[i].r = palData[i].r;
		orig_palette[i].g = palData[i].g;
		orig_palette[i].b = palData[i].b;
#ifndef USE_SDL1
		orig_palette[i].a = SDL_ALPHA_OPAQUE;
#endif
	}

	// Oracool: the green ramp that used to be injected here over the orange minis (152-159, from
	// 2026-08-15 to 2026-09-10) is GONE. Those eight entries are the fire's - Inferno, the fire
	// missiles, every burning thing dithers through them - and painting them green cost the fires
	// their colour (user, 2026-09-10: "the colors responsible for Inferno are messed up from the
	// time we invented green"). Everything that wanted the green now has it as colour VALUES on the
	// 32-bit screen: set-item text (RgbDefinedColors), the green skill plate (spell_icons.cpp), the
	// runeword socket border (inv.cpp). The palette is vanilla's again.

	if (blend) {
		if (leveltype == DTYPE_CAVES || leveltype == DTYPE_CRYPT) {
			GenerateBlendedLookupTable(orig_palette, 1, 31);
		} else if (leveltype == DTYPE_NEST) {
			GenerateBlendedLookupTable(orig_palette, 1, 15);
		} else {
			GenerateBlendedLookupTable(orig_palette, -1, -1);
		}
	}

	// v1.11: the colour table follows the new palette AT ONCE. Vanilla could wait for the fade-in's
	// first palette_update, because an 8-bit frame drawn meanwhile was recoloured by that update;
	// a 32-bit frame is not. Without this the loading screen and the first frame of every level
	// were drawn through the previous palette and revealed as such by the fade (user, 2026-09-07:
	// "loading screens are the ones with issues", "for a fraction of a second it looks incorrectly
	// colored"). Gamma applied as PaletteFadeIn will apply it, so the first frame and the faded
	// ones agree.
	ApplyGamma(logical_palette, orig_palette, 256);
	RebuildPaletteRgb(logical_palette, 0, 256);
}

void LoadRndLvlPal(dungeon_type l)
{
	if (HeadlessMode)
		return;

	if (l == DTYPE_TOWN) {
		LoadPalette("levels\\towndata\\town.pal");
		return;
	}

	int rv = GenerateRnd(4) + 1;
	// Oracool: Megaplan Phase 0.7 - the recolor-zone seam. A zone row may name its own palette
	// (built by tools/PaletteVariant.ps1), replacing the type's normal selection - which is
	// exactly how Hellfire's Crypt is a recolored Cathedral, made data. Checked AFTER the roll
	// above so the deterministic stream advances identically whether or not an override exists.
	if (const char *overridePalette = oracool::GetZonePaletteOverride(currlevel); overridePalette != nullptr) {
		LoadPalette(overridePalette);
		return;
	}
	if (l == DTYPE_CRYPT) {
		LoadPalette("nlevels\\l5data\\l5base.pal");
		return;
	}

	char szFileName[27];
	if (l == DTYPE_NEST) {
		if (!*sgOptions.Graphics.alternateNestArt) {
			rv++;
		}
		*fmt::format_to(szFileName, R"(nlevels\l{0}data\l{0}base{1}.pal)", 6, rv) = '\0';
	} else {
		*fmt::format_to(szFileName, R"(levels\l{0}data\l{0}_{1}.pal)", static_cast<int>(l), rv) = '\0';
	}
	LoadPalette(szFileName);
}

void IncreaseGamma()
{
	int gammaValue = *sgOptions.Graphics.gammaCorrection;
	if (gammaValue < 100) {
		sgOptions.Graphics.gammaCorrection.SetValue(std::min(gammaValue + 5, 100));
		ApplyGamma(system_palette, logical_palette, 256);
		palette_update();
	}
}

void DecreaseGamma()
{
	int gammaValue = *sgOptions.Graphics.gammaCorrection;
	if (gammaValue > 30) {
		sgOptions.Graphics.gammaCorrection.SetValue(std::max(gammaValue - 5, 30));
		ApplyGamma(system_palette, logical_palette, 256);
		palette_update();
	}
}

int UpdateGamma(int gamma)
{
	if (gamma > 0) {
		sgOptions.Graphics.gammaCorrection.SetValue(130 - gamma);
		ApplyGamma(system_palette, logical_palette, 256);
		palette_update();
	}
	return 130 - *sgOptions.Graphics.gammaCorrection;
}

void SetFadeLevel(int fadeval, bool updateHardwareCursor)
{
	if (HeadlessMode)
		return;

	FadeLevel = std::clamp(fadeval, 0, 256); // applied at present time - see PaletteRGB's note
	for (int i = 0; i < 256; i++) {
		system_palette[i].r = (fadeval * logical_palette[i].r) / 256;
		system_palette[i].g = (fadeval * logical_palette[i].g) / 256;
		system_palette[i].b = (fadeval * logical_palette[i].b) / 256;
	}
	palette_update();
	if (updateHardwareCursor && IsHardwareCursor()) {
		ReinitializeHardwareCursor();
	}
}

void BlackPalette()
{
	// With fade level 0 updating the hardware cursor may be redundant
	// since everything is black. The caller should update the cursor
	// when needed instead.
	SetFadeLevel(0, /*updateHardwareCursor=*/false);
}

void PaletteFadeIn(int fr)
{
	if (HeadlessMode)
		return;
	if (demo::IsRunning())
		fr = 0;

	ApplyGamma(logical_palette, orig_palette, 256);

	if (fr > 0) {
		const uint32_t tc = SDL_GetTicks();
		fr *= 3;
		uint32_t prevFadeValue = 255;
		for (uint32_t i = 0; i < 256; i = fr * (SDL_GetTicks() - tc) / 50) {
			if (i != prevFadeValue) {
				// We can skip hardware cursor update for fade level 0 (everything is black).
				SetFadeLevel(i, /*updateHardwareCursor=*/i != 0u);
				prevFadeValue = i;
			}
			BltFast(nullptr, nullptr);
			RenderPresent();
		}
		SetFadeLevel(256);
	} else {
		SetFadeLevel(256);
		BltFast(nullptr, nullptr);
		RenderPresent();
	}

	logical_palette = orig_palette;

	sgbFadedIn = true;
}

void PaletteFadeOut(int fr)
{
	if (!sgbFadedIn || HeadlessMode)
		return;
	if (demo::IsRunning())
		fr = 0;

	if (fr > 0) {
		const uint32_t tc = SDL_GetTicks();
		fr *= 3;
		uint32_t prevFadeValue = 0;
		for (uint32_t i = 0; i < 256; i = fr * (SDL_GetTicks() - tc) / 50) {
			if (i != prevFadeValue) {
				SetFadeLevel(256 - i);
				prevFadeValue = i;
			}
			BltFast(nullptr, nullptr);
			RenderPresent();
		}
		SetFadeLevel(0);
	} else {
		SetFadeLevel(0);
		BltFast(nullptr, nullptr);
		RenderPresent();
	}

	sgbFadedIn = false;
}

void palette_update_caves()
{
	CycleColors(1, 31);
	palette_update(0, 31);
}

/**
 * @brief Cycle the lava every other frame, and glow every frame
 * Lava has 15 colors and the glow 16, so the full animation has 240 frames before it loops
 */
void palette_update_crypt()
{
	static bool delayLava = false;

	if (!delayLava) {
		CycleColorsReverse(1, 15);
		delayLava = false;
	}

	CycleColorsReverse(16, 31);
	palette_update(0, 31);
	delayLava = !delayLava;
}

/**
 * @brief Cycle the pond waves and bubles colors every 3rd frame
 * Bubles have 8 colors and waves 7, so the full animation has 56 frames before it loops
 */
void palette_update_hive()
{
	static uint8_t delay = 0;

	if (delay != 2) {
		delay++;
		return;
	}

	CycleColorsReverse(1, 8);
	CycleColorsReverse(9, 15);
	palette_update(0, 15);
	delay = 0;
}

void palette_update_quest_palette(int n)
{
	int i = 32 - n;
	logical_palette[i] = orig_palette[i];
	ApplyGamma(system_palette, logical_palette, 32);
	palette_update(0, 31);
	// Update blended transparency, but only for the color that was updated
	for (int j = 0; j < 256; j++) {
		if (i == j) { // No need to calculate transparency between 2 identical colors
			paletteTransparencyLookup[i][j] = j;
			continue;
		}
		SDL_Color blendedColor;
		blendedColor.r = ((int)logical_palette[i].r + (int)logical_palette[j].r) / 2;
		blendedColor.g = ((int)logical_palette[i].g + (int)logical_palette[j].g) / 2;
		blendedColor.b = ((int)logical_palette[i].b + (int)logical_palette[j].b) / 2;
		Uint8 best = FindBestMatchForColor(logical_palette, blendedColor, 1, 31);
		paletteTransparencyLookup[i][j] = paletteTransparencyLookup[j][i] = best;
	}
}

} // namespace devilution
