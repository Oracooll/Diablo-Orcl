/**
 * @file oracool/post_fx.h
 *
 * World post-effects (user, 2026-10-10: four of "the nine visual techniques"). All of them are visual only - no game
 * RNG, no game state - and work on the 32-bit WORLD surface only (scrollrt.cpp's DrawGame, the pre-zoom world region,
 * before the HUD); an 8-bit surface (the golden tests' world draws) is never touched.
 *
 *  - Area mood (option areaMood): a subtle colour grade per level type - the Cathedral cold grey-blue, the Catacombs
 *    dusty ochre, the Caves damp green-brown, Hell blood-warm, the Nest sickly amber-green, the Crypt pale bone-violet.
 *    Brightness-preserving; off in town.
 *  - Vignette (option vignette): the world darkens toward its edges, at most about a quarter in the corners. Town too.
 *  - Bloom (option bloom): bright pixels (fire, spells, lava, torches) glow - a quarter-resolution bright pass, blurred,
 *    added back. Town too (its torches); skipped outright on a frame with nothing bright.
 *  - Heat haze and water (option heatHaze): the air shimmers above lava and light moves over water. The liquid tiles
 *    are found once a level by drawing each floor piece into an 8-bit scratch surface and counting its colour-cycling
 *    palette entries. Off in town.
 *  - Zoom filter (option zoomFilter): the world zoom (ZoomScale) uses Scale2x at exactly 2x and a sharp bilinear
 *    (blending only across source pixel edges) at any other factor, instead of nearest-neighbour's uneven pixels.
 */
#pragma once

#include <cstdint>
#include <vector>

#include "engine/point.hpp"
#include "levels/gendung.h"

namespace devilution {
struct Surface;
}

namespace devilution::oracool {

// ----- Shared with the graphics-card path (oracool/gpu_post.h, GPU renderer plan Stage 1): the very tables and
// constants the CPU passes use, so both give the same pixels. ---------------------------------------------------------

/** @brief Bloom: how much of the blurred glow goes back on, /65536 after the bilinear; the box radius. */
constexpr int BloomStrengthShared = 90;
constexpr int BloomRadiusShared = 3;
/** @brief Bloom: the bright pass's weight for a luma, 0..256. */
int BloomWeightFor(int luma);
/** @brief Area mood: the level's three channel curves, 256 entries each. */
void MoodCurves(dungeon_type type, uint8_t *r, uint8_t *g, uint8_t *b);
/** @brief Vignette: one axis of @p n pixels, 256 = untouched. */
void VignetteAxis(std::vector<uint16_t> &axis, int n);
/** @brief Sharp bilinear zoom: one axis from @p src to @p dst pixels (the two source pixels and the weight of the second). */
void ZoomAxis(int src, int dst, std::vector<int> &i0, std::vector<int> &i1, std::vector<int> &w);
/** @brief Zoom sharpening: sqrt(i / 256) * 256 for i in 0..256, and the negative lobe at full amplitude (/256). */
const uint16_t *CasAmplitudeTable();
constexpr int CasPeakShared = 41;

/** @brief Area mood and vignette, one pass over the world surface @p out. Pure black pixels are left black. */
void ApplyAreaMoodAndVignette(const Surface &out);

/** @brief Bloom over the world surface @p out. */
void ApplyBloom(const Surface &out);

/**
 * @brief Heat haze over lava and moving light over water. @p firstTile and @p firstTileBuffer are DrawFloor's start
 * (that tile's 64x32 box bottom-left in @p out), as ApplyLightColour takes them.
 */
void ApplyHeatHazeAndWater(const Surface &out, Point firstTile, Point firstTileBuffer);

/**
 * @brief The filtered world zoom: scales the 32-bit region [0, srcWidth) x [0, srcHeight) at @p base up to
 * [0, dstWidth) x [0, dstHeight), in place. @p pitch is in bytes. dstWidth >= srcWidth and dstHeight >= srcHeight.
 */
void ZoomScaleFiltered(uint8_t *base, int pitch, int srcWidth, int srcHeight, int dstWidth, int dstHeight);

/**
 * @brief Zoom sharpening (user, 2026-10-11): contrast-adaptive sharpening after AMD FidelityFX CAS, in place over the
 * @p width x @p height 32-bit picture at @p base. Each pixel is sharpened against its four neighbours by an amount set
 * by how much room its neighbourhood leaves before clipping - strong on soft detail, none on a hard edge already at black
 * or white - so the filtered zoom comes out crisp without halos. The outermost rows and columns are left as they are.
 */
void SharpenAdaptive(uint8_t *base, int pitch, int width, int height);

/** @brief Test hook: what the liquid scan found for the current level at @p tile (0 none, 1 lava, 2 water). */
uint8_t LiquidKindAt(Point tile);

} // namespace devilution::oracool
