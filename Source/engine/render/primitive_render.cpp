#include "engine/render/primitive_render.hpp"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstring>

#include "engine/palette.h"
#include "engine/point.hpp"
#include "engine/render/blit_impl.hpp"
#include "engine/size.hpp"
#include "engine/surface.hpp"

namespace devilution {
namespace {

/*
 * Oracool, the 32-bit compositing renderer (v1.11, stage 1): every primitive below has an 8-bit
 * body (indices, vanilla's) and a 32-bit body (colours through PaletteRGB, blends as exact
 * averages), chosen by the surface's format. The aligned-32 black-blend trick, which packs four
 * INDICES into one word and runs them through a 64 KB table, is meaningful only on an indexed
 * surface and is kept for that case alone.
 */

/** @brief A 32-bit pixel at half brightness: the exact average with black. */
DVL_ALWAYS_INLINE uint32_t HalfRgb(uint32_t c)
{
	return (c >> 1) & 0x7F7F7F7Fu;
}

void DrawHalfTransparentUnalignedBlendedRectTo(const Surface &out, unsigned sx, unsigned sy, unsigned width, unsigned height, uint8_t color)
{
	if (out.isIndexed()) {
		uint8_t *pix = out.at<uint8_t>(static_cast<int>(sx), static_cast<int>(sy));
		const uint8_t *const lookupTable = paletteTransparencyLookup[color];
		const unsigned skipX = out.pixelPitch() - width;
		for (unsigned y = 0; y < height; ++y) {
			for (unsigned x = 0; x < width; ++x, ++pix) {
				*pix = lookupTable[*pix];
			}
			pix += skipX;
		}
		return;
	}
	uint32_t *pix = out.at<uint32_t>(static_cast<int>(sx), static_cast<int>(sy));
	const uint32_t rgb = PaletteRGB[color];
	const unsigned skipX = out.pixelPitch() - width;
	for (unsigned y = 0; y < height; ++y) {
		for (unsigned x = 0; x < width; ++x, ++pix) {
			*pix = AverageRgb(*pix, rgb);
		}
		pix += skipX;
	}
}

/** @brief The black blend on a 32-bit surface: every pixel halved, no table. */
void DrawHalfTransparentBlackRectToRgb(const Surface &out, unsigned sx, unsigned sy, unsigned width, unsigned height)
{
	uint32_t *pix = out.at<uint32_t>(static_cast<int>(sx), static_cast<int>(sy));
	const unsigned skipX = out.pixelPitch() - width;
	for (unsigned y = 0; y < height; ++y) {
		for (unsigned x = 0; x < width; ++x, ++pix) {
			*pix = HalfRgb(*pix);
		}
		pix += skipX;
	}
}

#if DEVILUTIONX_PALETTE_TRANSPARENCY_BLACK_16_LUT
// Expects everything to be 4-byte aligned. 8-bit surfaces only.
void DrawHalfTransparentAligned32BlendedRectTo(const Surface &out, unsigned sx, unsigned sy, unsigned width, unsigned height)
{
	assert(out.pitch() % 4 == 0);

	auto *pix = reinterpret_cast<uint32_t *>(out.at(static_cast<int>(sx), static_cast<int>(sy)));
	assert(reinterpret_cast<intptr_t>(pix) % 4 == 0);

	const uint16_t *lookupTable = paletteTransparencyLookupBlack16;

	const unsigned skipX = (out.pitch() - width) / 4;
	width /= 4;
	while (height-- > 0) {
		for (unsigned i = 0; i < width; ++i, ++pix) {
			const uint32_t v = *pix;
			*pix = lookupTable[v & 0xFFFF] | (lookupTable[(v >> 16) & 0xFFFF] << 16);
		}
		pix += skipX;
	}
}

void DrawHalfTransparentBlendedRectTo(const Surface &out, unsigned sx, unsigned sy, unsigned width, unsigned height)
{
	if (!out.isIndexed()) {
		DrawHalfTransparentBlackRectToRgb(out, sx, sy, width, height);
		return;
	}

	// All SDL surfaces are 4-byte aligned and divisible by 4.
	// However, our coordinates and widths may not be.

	// First, draw the leading unaligned part.
	if (sx % 4 != 0) {
		const unsigned w = 4 - sx % 4;
		DrawHalfTransparentUnalignedBlendedRectTo(out, sx, sy, w, height, 0);
		sx += w;
		width -= w;
	}

	if (static_cast<int>(sx + width) == out.w()) {
		// The pitch is 4-byte aligned, so we can simply extend the width to the pitch.
		width = out.pitch() - sx;
	} else if (width % 4 != 0) {
		// Draw the trailing unaligned part.
		const unsigned w = width % 4;
		DrawHalfTransparentUnalignedBlendedRectTo(out, sx + (width / 4) * 4, sy, w, height, 0);
		width -= w;
	}

	// Now everything is divisible by 4. Draw the aligned part.
	DrawHalfTransparentAligned32BlendedRectTo(out, sx, sy, width, height);
}
#else
void DrawHalfTransparentBlendedRectTo(const Surface &out, unsigned sx, unsigned sy, unsigned width, unsigned height)
{
	if (!out.isIndexed()) {
		DrawHalfTransparentBlackRectToRgb(out, sx, sy, width, height);
		return;
	}
	DrawHalfTransparentUnalignedBlendedRectTo(out, sx, sy, width, height, 0);
}
#endif

} // namespace

void FillRect(const Surface &out, int x, int y, int width, int height, uint8_t colorIndex)
{
	for (int j = 0; j < height; j++) {
		DrawHorizontalLine(out, { x, y + j }, width, colorIndex);
	}
}

void DrawHorizontalLine(const Surface &out, Point from, int width, std::uint8_t colorIndex)
{
	if (from.y < 0 || from.y >= out.h() || from.x >= out.w() || width <= 0 || from.x + width <= 0)
		return;
	if (from.x < 0) {
		width += from.x;
		from.x = 0;
	}
	if (from.x + width > out.w())
		width = out.w() - from.x;
	return UnsafeDrawHorizontalLine(out, from, width, colorIndex);
}

void UnsafeDrawHorizontalLine(const Surface &out, Point from, int width, std::uint8_t colorIndex)
{
	if (out.isIndexed())
		BlitFillDirect(out.at<uint8_t>(from), width, colorIndex);
	else
		BlitFillDirect(out.at<uint32_t>(from), width, colorIndex);
}

void DrawVerticalLine(const Surface &out, Point from, int height, std::uint8_t colorIndex)
{
	if (from.x < 0 || from.x >= out.w() || from.y >= out.h() || height <= 0 || from.y + height <= 0)
		return;
	if (from.y < 0) {
		height += from.y;
		from.y = 0;
	}
	if (from.y + height > out.h())
		height = (from.y + height) - out.h();
	return UnsafeDrawVerticalLine(out, from, height, colorIndex);
}

void UnsafeDrawVerticalLine(const Surface &out, Point from, int height, std::uint8_t colorIndex)
{
	const auto pitch = out.pixelPitch();
	if (out.isIndexed()) {
		uint8_t *dst = out.at<uint8_t>(from);
		while (height-- > 0) {
			*dst = colorIndex;
			dst += pitch;
		}
		return;
	}
	uint32_t *dst = out.at<uint32_t>(from);
	const uint32_t rgb = PaletteRGB[colorIndex];
	while (height-- > 0) {
		*dst = rgb;
		dst += pitch;
	}
}

void DrawHalfTransparentHorizontalLine(const Surface &out, Point from, int width, uint8_t colorIndex)
{
	// completely off-bounds?
	if (from.y < 0 || from.y >= out.h() || width <= 0 || from.x >= out.w() || from.x + width <= 0)
		return;

	const int x0 = std::max(0, from.x);
	const int x1 = std::min(out.w(), from.x + width);

	for (int x = x0; x < x1; ++x) {
		SetHalfTransparentPixel(out, { x, from.y }, colorIndex);
	}
}

// Draw a half-transparent vertical line of `height` pixels starting at `from`.
void DrawHalfTransparentVerticalLine(const Surface &out, Point from, int height, uint8_t colorIndex)
{
	// completely off-bounds?
	if (from.x < 0 || from.x >= out.w() || height <= 0 || from.y >= out.h() || from.y + height <= 0)
		return;

	const int y0 = std::max(0, from.y);
	const int y1 = std::min(out.h(), from.y + height);

	for (int y = y0; y < y1; ++y) {
		SetHalfTransparentPixel(out, { from.x, y }, colorIndex);
	}
}

void DrawHalfTransparentRectTo(const Surface &out, int sx, int sy, int width, int height)
{
	if (sx + width < 0)
		return;
	if (sy + height < 0)
		return;
	if (sx >= out.w())
		return;
	if (sy >= out.h())
		return;

	if (sx < 0) {
		width += sx;
		sx = 0;
	} else if (sx + width >= out.w()) {
		width = out.w() - sx;
	}

	if (sy < 0) {
		height += sy;
		sy = 0;
	} else if (sy + height >= out.h()) {
		height = out.h() - sy;
	}

	DrawHalfTransparentBlendedRectTo(out, sx, sy, width, height);
}

void DrawHalfTransparentRectTo(const Surface &out, int sx, int sy, int width, int height, uint8_t color)
{
	if (sx + width < 0)
		return;
	if (sy + height < 0)
		return;
	if (sx >= out.w())
		return;
	if (sy >= out.h())
		return;

	if (sx < 0) {
		width += sx;
		sx = 0;
	} else if (sx + width >= out.w()) {
		width = out.w() - sx;
	}

	if (sy < 0) {
		height += sy;
		sy = 0;
	} else if (sy + height >= out.h()) {
		height = out.h() - sy;
	}

	DrawHalfTransparentUnalignedBlendedRectTo(out, sx, sy, width, height, color);
}

void SetHalfTransparentPixelUnchecked(const Surface &out, Point position, uint8_t color)
{
	if (out.isIndexed()) {
		uint8_t *pix = out.at<uint8_t>(position);
		*pix = paletteTransparencyLookup[color][*pix];
		return;
	}
	uint32_t *pix = out.at<uint32_t>(position);
	*pix = AverageRgb(*pix, PaletteRGB[color]);
}

void SetHalfTransparentPixel(const Surface &out, Point position, uint8_t color)
{
	if (out.InBounds(position))
		SetHalfTransparentPixelUnchecked(out, position, color);
}

void UnsafeDrawBorder2px(const Surface &out, Rectangle rect, uint8_t color)
{
	const int width = rect.size.width;
	const int height = rect.size.height;
	const Point p = rect.position;
	UnsafeDrawHorizontalLine(out, p, width, color);
	UnsafeDrawHorizontalLine(out, { p.x, p.y + 1 }, width, color);
	for (int i = 2; i < height - 2; ++i) {
		UnsafeDrawHorizontalLine(out, { p.x, p.y + i }, 2, color);
		UnsafeDrawHorizontalLine(out, { p.x + width - 2, p.y + i }, 2, color);
	}
	UnsafeDrawHorizontalLine(out, { p.x, p.y + height - 2 }, width, color);
	UnsafeDrawHorizontalLine(out, { p.x, p.y + height - 1 }, width, color);
}

void FillRectRgb(const Surface &out, int x, int y, int width, int height, uint32_t rgb, uint8_t fallbackIndex)
{
	if (out.isIndexed()) {
		FillRect(out, x, y, width, height, fallbackIndex);
		return;
	}
	const int x0 = std::max(x, 0), y0 = std::max(y, 0);
	const int x1 = std::min(x + width, out.w()), y1 = std::min(y + height, out.h());
	for (int row = y0; row < y1; row++)
		std::fill_n(out.at<uint32_t>(x0, row), std::max(0, x1 - x0), rgb & 0x00FFFFFF);
}

bool BlitArgb(const Surface &out, const uint32_t *pixels, int srcPitch, SDL_Rect srcRect, Point position, int alphaPercent)
{
	if (out.isIndexed())
		return false;
	// Clip the source rectangle to the destination.
	int sx = srcRect.x, sy = srcRect.y, w = srcRect.w, h = srcRect.h;
	int dx = position.x, dy = position.y;
	if (dx < 0) {
		sx -= dx;
		w += dx;
		dx = 0;
	}
	if (dy < 0) {
		sy -= dy;
		h += dy;
		dy = 0;
	}
	w = std::min(w, out.w() - dx);
	h = std::min(h, out.h() - dy);
	if (w <= 0 || h <= 0)
		return true;
	for (int y = 0; y < h; y++) {
		const uint32_t *src = pixels + static_cast<ptrdiff_t>(sy + y) * srcPitch + sx;
		uint32_t *dst = out.at<uint32_t>(dx, dy + y);
		for (int x = 0; x < w; x++) {
			const uint32_t s = src[x];
			if ((s >> 24) == 0)
				continue;
			dst[x] = CompositeArgbOver(s, dst[x], alphaPercent);
		}
	}
	return true;
}

bool BlitArgbScaled(const Surface &out, const uint32_t *pixels, int srcPitch, SDL_Rect srcRect, Rectangle dest, int alphaPercent)
{
	if (out.isIndexed())
		return false;
	if (dest.size.width <= 0 || dest.size.height <= 0 || srcRect.w <= 0 || srcRect.h <= 0)
		return true;
	for (int y = 0; y < dest.size.height; y++) {
		const int dstY = dest.position.y + y;
		if (dstY < 0 || dstY >= out.h())
			continue;
		const int sy = srcRect.y + y * srcRect.h / dest.size.height;
		const uint32_t *src = pixels + static_cast<ptrdiff_t>(sy) * srcPitch;
		for (int x = 0; x < dest.size.width; x++) {
			const int dstX = dest.position.x + x;
			if (dstX < 0 || dstX >= out.w())
				continue;
			const uint32_t s = src[srcRect.x + x * srcRect.w / dest.size.width];
			if ((s >> 24) == 0)
				continue;
			uint32_t *dst = out.at<uint32_t>(dstX, dstY);
			*dst = CompositeArgbOver(s, *dst, alphaPercent);
		}
	}
	return true;
}

} // namespace devilution
