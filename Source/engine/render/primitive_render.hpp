#pragma once

#include <cstdint>
#include <cstdlib>

#include "engine/point.hpp"
#include "engine/size.hpp"
#include "engine/surface.hpp"

namespace devilution {

/**
 * @brief Fill a rectangle with the given color.
 */
void FillRect(const Surface &out, int x, int y, int width, int height, uint8_t colorIndex);

/**
 * @brief FillRect with a colour VALUE (0xRRGGBB) on the 32-bit screen; on an indexed surface
 * @p fallbackIndex is used instead, so the call is safe on either.
 */
void FillRectRgb(const Surface &out, int x, int y, int width, int height, uint32_t rgb, uint8_t fallbackIndex);

/**
 * @brief Tint what is already in a rectangle, the way vanilla tints an item's slot.
 *
 * DevilutionX 1.5.5's InvDrawSlotBack covers nothing: it reads the slot art back from the frame
 * and moves every pixel in the grey ramp one shade deeper into the item class's colour ramp, so the
 * stone's cracks and shading survive, coloured. That test is on palette INDICES and the 32-bit screen
 * has none, and the fork's slot wells are true-colour art with nothing in the grey ramp anyway - so
 * this is the same idea restated for colour values: each pixel keeps its luminance and takes the
 * hue of @p hueRgb.
 *
 * @param hueRgb The colour whose hue the pixels take. Only the hue matters: it is normalised so its
 *   brightest channel is full, and the pixel's own luminance supplies the brightness.
 * @param brightnessPercent Applied to the result. 100 keeps the stone's brightness; vanilla's "one
 *   shade deeper" is about 90; above 100 lightens.
 * @param floorPercent The share of the hue even a black pixel shows, 0..100, so a dark well still
 *   reads as coloured rather than as a darker well.
 * @param fallbackRampBase On an indexed surface (tests, golden images) vanilla's own ramp shift is
 *   used instead, into the PAL16/PAL8 ramp that starts at this index.
 */
void TintRectRgb(const Surface &out, int x, int y, int width, int height, uint32_t hueRgb, int brightnessPercent, int floorPercent, uint8_t fallbackRampBase);

/**
 * @brief Oracool: brightens every pixel in the rect by @p percent (100 = unchanged, 115 = a notch
 * brighter), each channel scaled and clamped, hue and saturation kept - the hover state of a painted
 * button (the Rift Monument's menu, 2026-09-20: "when hovering over the buttons make them a notch
 * brighter"). On an indexed surface pixels in the PAL16 ramps step one shade lighter (ramps run light
 * to dark), anything else is left alone.
 */
void BrightenRectRgb(const Surface &out, int x, int y, int width, int height, int percent);

/**
 * @brief Draw a horizontal line segment in the target buffer (left to right)
 * @param out Target buffer
 * @param from Start of the line segment
 * @param width
 * @param colorIndex Color index from current palette
 */
void DrawHorizontalLine(const Surface &out, Point from, int width, std::uint8_t colorIndex);

/** Same as DrawHorizontalLine but without bounds clipping. */
void UnsafeDrawHorizontalLine(const Surface &out, Point from, int width, std::uint8_t colorIndex);

/**
 * @brief Draw a vertical line segment in the target buffer (top to bottom)
 * @param out Target buffer
 * @param from Start of the line segment
 * @param height
 * @param colorIndex Color index from current palette
 */
void DrawVerticalLine(const Surface &out, Point from, int height, std::uint8_t colorIndex);

/** Same as DrawVerticalLine but without bounds clipping. */
void UnsafeDrawVerticalLine(const Surface &out, Point from, int height, std::uint8_t colorIndex);

void DrawHalfTransparentHorizontalLine(const Surface &out, Point from, int width, uint8_t colorIndex);
void DrawHalfTransparentVerticalLine(const Surface &out, Point from, int width, uint8_t colorIndex);

/**
 * Draws a half-transparent rectangle by palette blending with black.
 *
 * @brief Render a transparent black rectangle
 * @param out Target buffer
 * @param sx Screen coordinate
 * @param sy Screen coordinate
 * @param width Rectangle width
 * @param height Rectangle height
 */
void DrawHalfTransparentRectTo(const Surface &out, int sx, int sy, int width, int height);

void DrawHalfTransparentRectTo(const Surface &out, int sx, int sy, int width, int height, uint8_t color);

/**
 * Draws a half-transparent pixel
 *
 * @brief Render a transparent pixel
 * @param out Target buffer
 * @param position Screen coordinates
 * @param col Pixel color
 */
void SetHalfTransparentPixel(const Surface &out, Point position, uint8_t color);

/** @brief SetHalfTransparentPixel without the bounds test - for callers that clipped already. Format-aware (v1.11). */
void SetHalfTransparentPixelUnchecked(const Surface &out, Point position, uint8_t color);

/**
 * Draws a 2px inset border.
 *
 * @param out Target buffer
 * @param rect The rectangle that border pixels are rendered inside of.
 * @param color Border color.
 */
void UnsafeDrawBorder2px(const Surface &out, Rectangle rect, uint8_t color);

/**
 * @brief Packs a straight-alpha colour as ARGB8888, the pixel format BlitArgb takes.
 */
constexpr uint32_t PackArgb(uint8_t a, uint8_t r, uint8_t g, uint8_t b)
{
	return (static_cast<uint32_t>(a) << 24) | (static_cast<uint32_t>(r) << 16) | (static_cast<uint32_t>(g) << 8) | b;
}

/**
 * @brief Composites straight-alpha ARGB8888 pixels onto the 32-bit screen (source over), clipped
 * to @p out. The renderer's stage 2 (v1.11): the fork's own PNG art drawn as it was painted,
 * with its real alpha, instead of quantised to the level palette and keyed at 128.
 *
 * @param pixels The source image, @p srcPitch pixels per row.
 * @param srcRect The part of it to draw.
 * @param alphaPercent Scales every source alpha; 50 is the old half-transparent blit.
 * @return false when @p out is an 8-bit surface (nothing drawn): the caller keeps its palette path
 *         for offscreen surfaces and the golden tests.
 */
bool BlitArgb(const Surface &out, const uint32_t *pixels, int srcPitch, SDL_Rect srcRect, Point position, int alphaPercent = 100);

/**
 * @brief BlitArgb with the source rectangle scaled (nearest neighbour) onto @p dest.
 */
bool BlitArgbScaled(const Surface &out, const uint32_t *pixels, int srcPitch, SDL_Rect srcRect, Rectangle dest, int alphaPercent = 100);

/** @brief One ARGB pixel over one XRGB screen pixel. Exposed for the scaled and masked variants. */
inline uint32_t CompositeArgbOver(uint32_t src, uint32_t dst, int alphaPercent)
{
	const int a = static_cast<int>(src >> 24) * alphaPercent / 100;
	if (a <= 0)
		return dst;
	if (a >= 255)
		return src & 0x00FFFFFF;
	// Per channel. The packed two-channels-at-once form overflowed 32 bits and let the division
	// borrow across channels, which turned every half-transparent draw blue (silhouette, aura
	// ring, orb glass - user screenshots, 2026-09-07).
	const int ia = 255 - a;
	const uint32_t r = ((((src >> 16) & 0xFF) * a) + (((dst >> 16) & 0xFF) * ia)) / 255;
	const uint32_t g = ((((src >> 8) & 0xFF) * a) + (((dst >> 8) & 0xFF) * ia)) / 255;
	const uint32_t b = (((src & 0xFF) * a) + ((dst & 0xFF) * ia)) / 255;
	return (r << 16) | (g << 8) | b;
}

} // namespace devilution
