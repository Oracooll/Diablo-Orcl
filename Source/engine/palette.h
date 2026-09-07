/**
 * @file palette.h
 *
 * Interface of functions for handling the engines color palette.
 */
#pragma once

#include <array>
#include <cstdint>

#include "levels/gendung.h"
#include "utils/attributes.h"

namespace devilution {

// Diablo uses a 256 color palette
// Entry 0-127 (0x00-0x7F) are level specific
// Entry 128-255 (0x80-0xFF) are global

// standard palette for all levels
// 8 or 16 shades per color
// example (dark blue): PAL16_BLUE+14, PAL8_BLUE+7
// example (light red): PAL16_RED+2, PAL8_RED
// example (orange): PAL16_ORANGE+8, PAL8_ORANGE+4
#define PAL8_BLUE 128
#define PAL8_RED 136
#define PAL8_YELLOW 144
#define PAL8_ORANGE 152
/**
 * @brief Oracool: the PAL8_ORANGE mini-ramp's eight entries, repurposed as a GREEN ramp.
 *
 * The vanilla palette ships no green anywhere - Belzebub proved the palette is ours to edit (user,
 * 2026-08-15). LoadPalette overwrites these eight entries with a green ramp on every in-game
 * palette load.
 *
 * The donor MOVED once: green first took the PAL8_YELLOW minis, until the user pointed out that
 * bright yellow is rare items' colour and irreplaceable ("Rare items color to be bright YELLOW") -
 * the pale PAL16_YELLOW substitutes read as unique-gold. The orange minis measured nearly as unused
 * in the same frame audit (~2,400 px, mostly the near-black darkest shade), their one code consumer
 * (the automap's player marker) re-points to PAL16_ORANGE, and saturated orange survives there for
 * everything else. Each green shade keeps its donor's brightness.
 */
#define PAL8_GREEN 152
#define PAL8_GREEN_SHADES 8
#define PAL16_BEIGE 160
#define PAL16_BLUE 176
#define PAL16_YELLOW 192
#define PAL16_ORANGE 208
#define PAL16_RED 224
#define PAL16_GRAY 240

extern std::array<SDL_Color, 256> logical_palette;
extern std::array<SDL_Color, 256> system_palette;
extern std::array<SDL_Color, 256> orig_palette;

/**
 * @brief Lookup table for the average of two colors in `logical_palette`.
 */
extern uint8_t paletteTransparencyLookup[256][256];

#if DEVILUTIONX_PALETTE_TRANSPARENCY_BLACK_16_LUT
/**
 * A lookup table from black for a pair of colors.
 *
 * For a pair of colors i and j, the index `i | (j << 8)` contains
 * `paletteTransparencyLookup[0][i] | (paletteTransparencyLookup[0][j] << 8)`.
 *
 * On big-endian platforms, the indices are encoded as `j | (i << 8)`, while the
 * value order remains the same.
 */
extern uint16_t paletteTransparencyLookupBlack16[65536];
#endif

void palette_update(int first = 0, int ncolor = 256);

/**
 * Oracool, the 32-bit compositing renderer (v1.11, stage 1). The screen holds colours, so every
 * drawing kernel resolves a palette index through this table as it writes - XRGB8888, rebuilt by
 * palette_update from whatever the palette is at the time. Colour cycling (water, lava) works as it
 * always did because the world is redrawn every frame and this table follows the cycle.
 *
 * A FADE is different: vanilla faded by repainting the palette under a frame that was NOT redrawn,
 * which colours in a buffer cannot follow. So a fade is a present-time transform instead: this
 * table is built from the UNFADED palette while FadeLevel is below 256, and Blit (dx.cpp) scales
 * the frame by FadeLevel on its way to the screen. The screenshot's red flash is the same idea.
 */
extern DVL_API_FOR_TEST std::array<uint32_t, 256> PaletteRGB;
/** @brief 256 = no fade; 0 = black. Set by SetFadeLevel, applied by Blit. */
extern DVL_API_FOR_TEST int FadeLevel;
/** @brief The screenshot flash: green and blue dropped at present time. Set by RedPalette, cleared by palette_update. */
extern bool PresentRedFlash;
/** @brief An index to the colour PaletteRGB currently holds for it. */
inline uint32_t PaletteIndexToRgb(uint8_t index)
{
	return PaletteRGB[index];
}
void palette_init();
void LoadPalette(const char *pszFileName, bool blend = true);
void LoadRndLvlPal(dungeon_type l);
void IncreaseGamma();
void ApplyGamma(std::array<SDL_Color, 256> &dst, const std::array<SDL_Color, 256> &src, int n);
void DecreaseGamma();
int UpdateGamma(int gamma);
void BlackPalette();
void SetFadeLevel(int fadeval, bool updateHardwareCursor = true);
/**
 * @brief Fade screen from black
 * @param fr Steps per 50ms
 */
void PaletteFadeIn(int fr);
/**
 * @brief Fade screen to black
 * @param fr Steps per 50ms
 */
void PaletteFadeOut(int fr);
void palette_update_caves();
void palette_update_crypt();
void palette_update_hive();
void palette_update_quest_palette(int n);

} // namespace devilution
