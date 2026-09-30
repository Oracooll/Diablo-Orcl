/**
 * @file dun_render.cpp
 *
 * Implementation of functionality for rendering the level tiles.
 */

// Debugging variables
// #define DEBUG_STR
// #define DEBUG_RENDER_COLOR
// #define DEBUG_RENDER_OFFSET_X 5
// #define DEBUG_RENDER_OFFSET_Y 5

#include "engine/render/dun_render.hpp"

#include <SDL_endian.h>

#include <algorithm>
#include <climits>
#include <cstddef>
#include <cstdint>
#include <string>

#include "engine/render/blit_impl.hpp"
#include "lighting.h"
#include "options.h"
#include "utils/attributes.h"
#include "utils/stdcompat/algorithm.hpp"
#ifdef DEBUG_STR
#include "engine/render/text_render.hpp"
#endif
#if defined(DEBUG_STR) || defined(DUN_RENDER_STATS)
#include "utils/str_cat.hpp"
#endif

namespace devilution {

namespace {

/** Width of a tile rendering primitive. */
constexpr int_fast16_t Width = TILE_WIDTH / 2;

/** Height of a tile rendering primitive (except triangles). */
constexpr int_fast16_t Height = TILE_HEIGHT;

/** Height of the lower triangle of a triangular or a trapezoid tile. */
constexpr int_fast16_t LowerHeight = TILE_HEIGHT / 2;

/** Height of the upper triangle of a triangular tile. */
constexpr int_fast16_t TriangleUpperHeight = TILE_HEIGHT / 2 - 1;

/** Height of the upper rectangle of a trapezoid tile. */
constexpr int_fast16_t TrapezoidUpperHeight = TILE_HEIGHT / 2;

constexpr int_fast16_t TriangleHeight = LowerHeight + TriangleUpperHeight;

/** For triangles, for each pixel drawn vertically, this many pixels are drawn horizontally. */
constexpr int_fast16_t XStep = 2;

int_fast16_t GetTileHeight(TileType tile)
{
	if (tile == TileType::LeftTriangle || tile == TileType::RightTriangle)
		return TriangleHeight;
	return Height;
}

#ifdef DEBUG_STR
std::pair<string_view, UiFlags> GetTileDebugStr(TileType tile)
{
	// clang-format off
	switch (tile) {
		case TileType::Square: return {"S", UiFlags::AlignCenter | UiFlags::VerticalCenter};
		case TileType::TransparentSquare: return {"T", UiFlags::AlignCenter | UiFlags::VerticalCenter};
		case TileType::LeftTriangle: return {"<", UiFlags::AlignRight | UiFlags::VerticalCenter};
		case TileType::RightTriangle: return {">", UiFlags::VerticalCenter};
		case TileType::LeftTrapezoid: return {"\\", UiFlags::AlignCenter};
		case TileType::RightTrapezoid: return {"/", UiFlags::AlignCenter};
		default: return {"", {}};
	}
	// clang-format on
}
#endif

#ifdef DEBUG_RENDER_COLOR
int DBGCOLOR = 0;

int GetTileDebugColor(TileType tile)
{
	// clang-format off
	switch (tile) {
		case TileType::Square: return PAL16_YELLOW + 5;
		case TileType::TransparentSquare: return PAL16_ORANGE + 5;
		case TileType::LeftTriangle: return PAL16_GRAY + 5;
		case TileType::RightTriangle: return PAL16_BEIGE;
		case TileType::LeftTrapezoid: return PAL16_RED + 5;
		case TileType::RightTrapezoid: return PAL16_BLUE + 5;
		default: return 0;
	}
}
#endif // DEBUG_RENDER_COLOR

// Masks are defined by 2 template variables:
//
// 1. `OpaquePrefix`: Whether the line starts with opaque pixels
//    followed by blended pixels or the other way around.
// 2. `PrefixIncrement`: The change to the prefix when going
//    up 1 line.
//
// The Left mask can only be applied to LeftTrapezoid and TransparentSquare.
// The Right mask can only be applied to RightTrapezoid and TransparentSquare.
// The Left/RightFoliage masks can only be applied to TransparentSquare.

// True if the given OpaquePrefix and PrefixIncrement represent foliage.
// For foliage, we skip transparent pixels instead of blending them.
template <bool OpaquePrefix, int8_t PrefixIncrement>
constexpr bool IsFoliage = PrefixIncrement != 0 && (OpaquePrefix == (PrefixIncrement > 0));

// True for foliage:
template <bool OpaquePrefix, int8_t PrefixIncrement>
constexpr bool SkipTransparentPixels = IsFoliage<OpaquePrefix, PrefixIncrement>;

// True if the entire lower half of the mask is transparent.
// True for Transparent, LeftFoliage, and RightFoliage.
template <bool OpaquePrefix, int8_t PrefixIncrement>
constexpr bool LowerHalfTransparent = (OpaquePrefix == (PrefixIncrement >= 0));

// The initial value for the prefix:
template <int8_t PrefixIncrement>
DVL_ALWAYS_INLINE int8_t InitPrefix()
{
	return PrefixIncrement >= 0 ? -32 : 64;
}

// The initial value for the prefix at y-th line (counting from the bottom).
template <int8_t PrefixIncrement>
DVL_ALWAYS_INLINE int8_t InitPrefix(int8_t y)
{
	return InitPrefix<PrefixIncrement>() + PrefixIncrement * y;
}

#ifdef DEBUG_STR
template <bool OpaquePrefix, int8_t PrefixIncrement>
std::string prefixDebugString(int8_t prefix) {
	std::string out(32, OpaquePrefix ? '0' : '1');
	const uint8_t clamped = clamp<int8_t>(prefix, 0, 32);
	out.replace(0, clamped, clamped, OpaquePrefix ? '1' : '0');
	StrAppend(out, " prefix=", prefix, " OpaquePrefix=", OpaquePrefix, " PrefixIncrement=", PrefixIncrement);
	return out;
}
#endif

enum class LightType : uint8_t {
	FullyDark,
	PartiallyLit,
	FullyLit,
};

// v1.11: one body per kernel, on the light TYPE and the destination PIXEL type. The 8-bit body is
// vanilla's; the 32-bit body resolves through PaletteRGB inside the blitters (blit_impl.hpp).
template <LightType Light, typename Pixel>
DVL_ALWAYS_INLINE DVL_ATTRIBUTE_HOT void RenderLineOpaque(Pixel *DVL_RESTRICT dst, [[maybe_unused]] const uint8_t *DVL_RESTRICT src, uint_fast8_t n, [[maybe_unused]] const uint8_t *DVL_RESTRICT tbl)
{
	if constexpr (Light == LightType::FullyDark) {
		BlitFillDirect(dst, n, 0);
	} else if constexpr (Light == LightType::FullyLit) {
#ifndef DEBUG_RENDER_COLOR
		BlitPixelsDirect(dst, src, n);
#else
		BlitFillDirect(dst, n, DBGCOLOR);
#endif
	} else {
#ifndef DEBUG_RENDER_COLOR
		BlitPixelsWithMap(dst, src, n, tbl);
#else
		BlitFillDirect(dst, n, tbl[DBGCOLOR]);
#endif
	}
}

template <LightType Light, typename Pixel>
DVL_ALWAYS_INLINE DVL_ATTRIBUTE_HOT void RenderLineTransparent(Pixel *DVL_RESTRICT dst, [[maybe_unused]] const uint8_t *DVL_RESTRICT src, uint_fast8_t n, [[maybe_unused]] const uint8_t *DVL_RESTRICT tbl)
{
#ifndef DEBUG_RENDER_COLOR
	if constexpr (Light == LightType::FullyDark) {
		BlitFillBlended(dst, n, 0);
	} else if constexpr (Light == LightType::FullyLit) {
		BlitPixelsBlended(dst, src, n);
	} else {
		BlitPixelsBlendedWithMap(dst, src, n, tbl);
	}
#else
	BlitFillBlended(dst, n, tbl[DBGCOLOR + 4]);
#endif
}

template <LightType Light, bool Transparent, typename Pixel>
DVL_ALWAYS_INLINE DVL_ATTRIBUTE_HOT void RenderLineTransparentOrOpaque(Pixel *DVL_RESTRICT dst, const uint8_t *DVL_RESTRICT src, uint_fast8_t width, const uint8_t *DVL_RESTRICT tbl)
{
	if (Transparent) {
		RenderLineTransparent<Light>(dst, src, width, tbl);
	} else {
		RenderLineOpaque<Light>(dst, src, width, tbl);
	}
}

template <LightType Light, bool OpaquePrefix, int8_t PrefixIncrement, typename Pixel>
DVL_ALWAYS_INLINE DVL_ATTRIBUTE_HOT void RenderLineTransparentAndOpaque(Pixel *DVL_RESTRICT dst, const uint8_t *DVL_RESTRICT src, uint_fast8_t prefixWidth, uint_fast8_t width, const uint8_t *DVL_RESTRICT tbl)
{
	if (OpaquePrefix) {
		RenderLineOpaque<Light>(dst, src, prefixWidth, tbl);
		if (!SkipTransparentPixels<OpaquePrefix, PrefixIncrement>)
			RenderLineTransparent<Light>(dst + prefixWidth, src + prefixWidth, width - prefixWidth, tbl);
	} else {
		if (!SkipTransparentPixels<OpaquePrefix, PrefixIncrement>)
			RenderLineTransparent<Light>(dst, src, prefixWidth, tbl);
		RenderLineOpaque<Light>(dst + prefixWidth, src + prefixWidth, width - prefixWidth, tbl);
	}
}

template <LightType Light, bool OpaquePrefix, int8_t PrefixIncrement, typename Pixel>
DVL_ALWAYS_INLINE DVL_ATTRIBUTE_HOT void RenderLine(Pixel *DVL_RESTRICT dst, const uint8_t *DVL_RESTRICT src, uint_fast8_t n, const uint8_t *DVL_RESTRICT tbl, int8_t prefix)
{
	if (PrefixIncrement == 0) {
		RenderLineTransparentOrOpaque<Light, OpaquePrefix>(dst, src, n, tbl);
	} else if (prefix >= static_cast<int8_t>(n)) {
		// We clamp the prefix to (0, n] and avoid calling `RenderLineTransparent/Opaque` with width=0.
		if (OpaquePrefix) {
			RenderLineOpaque<Light>(dst, src, n, tbl);
		} else {
			if (!SkipTransparentPixels<OpaquePrefix, PrefixIncrement>)
				RenderLineTransparent<Light>(dst, src, n, tbl);
		}
	} else if (prefix <= 0) {
		if (!OpaquePrefix) {
			RenderLineOpaque<Light>(dst, src, n, tbl);
		} else {
			if (!SkipTransparentPixels<OpaquePrefix, PrefixIncrement>)
				RenderLineTransparent<Light>(dst, src, n, tbl);
		}
	} else {
		RenderLineTransparentAndOpaque<Light, OpaquePrefix, PrefixIncrement>(dst, src, prefix, n, tbl);
	}
}

struct Clip {
	int_fast16_t top;
	int_fast16_t bottom;
	int_fast16_t left;
	int_fast16_t right;
	int_fast16_t width;
	int_fast16_t height;
};

DVL_ALWAYS_INLINE Clip CalculateClip(int_fast16_t x, int_fast16_t y, int_fast16_t w, int_fast16_t h, const Surface &out)
{
	Clip clip;
	clip.top = y + 1 < h ? h - (y + 1) : 0;
	clip.bottom = y + 1 > out.h() ? (y + 1) - out.h() : 0;
	clip.left = x < 0 ? -x : 0;
	clip.right = x + w > out.w() ? x + w - out.w() : 0;
	clip.width = w - clip.left - clip.right;
	clip.height = h - clip.top - clip.bottom;
	return clip;
}

template <LightType Light, bool Transparent, typename Pixel>
DVL_ALWAYS_INLINE DVL_ATTRIBUTE_HOT void RenderSquareFull(Pixel *DVL_RESTRICT dst, uint16_t dstPitch, const uint8_t *DVL_RESTRICT src, const uint8_t *DVL_RESTRICT tbl)
{
	for (auto i = 0; i < Height; ++i, dst -= dstPitch) {
		RenderLineTransparentOrOpaque<Light, Transparent>(dst, src, Width, tbl);
		src += Width;
	}
}

template <LightType Light, bool Transparent, typename Pixel>
DVL_ALWAYS_INLINE DVL_ATTRIBUTE_HOT void RenderSquareClipped(Pixel *DVL_RESTRICT dst, uint16_t dstPitch, const uint8_t *DVL_RESTRICT src, const uint8_t *DVL_RESTRICT tbl, Clip clip)
{
	src += clip.bottom * Height + clip.left;
	for (auto i = 0; i < clip.height; ++i, dst -= dstPitch) {
		RenderLineTransparentOrOpaque<Light, Transparent>(dst, src, clip.width, tbl);
		src += Width;
	}
}

template <LightType Light, bool Transparent, typename Pixel>
DVL_ALWAYS_INLINE DVL_ATTRIBUTE_HOT void RenderSquare(Pixel *DVL_RESTRICT dst, uint16_t dstPitch, const uint8_t *DVL_RESTRICT src, const uint8_t *DVL_RESTRICT tbl, Clip clip)
{
	if (clip.width == Width && clip.height == Height) {
		RenderSquareFull<Light, Transparent>(dst, dstPitch, src, tbl);
	} else {
		RenderSquareClipped<Light, Transparent>(dst, dstPitch, src, tbl, clip);
	}
}

template <LightType Light, bool OpaquePrefix, int8_t PrefixIncrement, typename Pixel>
DVL_ALWAYS_INLINE DVL_ATTRIBUTE_HOT void RenderTransparentSquareFull(Pixel *DVL_RESTRICT dst, uint16_t dstPitch, const uint8_t *DVL_RESTRICT src, const uint8_t *DVL_RESTRICT tbl)
{
	int8_t prefix = InitPrefix<PrefixIncrement>();
	for (auto i = 0; i < Height; ++i, dst -= dstPitch + Width) {
		uint_fast8_t drawWidth = Width;
		while (drawWidth > 0) {
			auto v = static_cast<int8_t>(*src++);
			if (v > 0) {
				RenderLine<Light, OpaquePrefix, PrefixIncrement>(dst, src, v, tbl, prefix - (Width - drawWidth));
				src += v;
			} else {
				v = -v;
			}
			dst += v;
			drawWidth -= v;
		}
		prefix += PrefixIncrement;
	}
}

template <LightType Light, bool OpaquePrefix, int8_t PrefixIncrement, typename Pixel>
// NOLINTNEXTLINE(readability-function-cognitive-complexity): Actually complex and has to be fast.
DVL_ALWAYS_INLINE DVL_ATTRIBUTE_HOT void RenderTransparentSquareClipped(Pixel *DVL_RESTRICT dst, uint16_t dstPitch, const uint8_t *DVL_RESTRICT src, const uint8_t *DVL_RESTRICT tbl, Clip clip)
{
	const auto skipRestOfTheLine = [&src](int_fast16_t remainingWidth) {
		while (remainingWidth > 0) {
			const auto v = static_cast<int8_t>(*src++);
			if (v > 0) {
				src += v;
				remainingWidth -= v;
			} else {
				remainingWidth -= -v;
			}
		}
		assert(remainingWidth == 0);
	};

	// Skip the bottom clipped lines.
	for (auto i = 0; i < clip.bottom; ++i) {
		skipRestOfTheLine(Width);
	}

	int8_t prefix = InitPrefix<PrefixIncrement>(clip.bottom);
	for (auto i = 0; i < clip.height; ++i, dst -= dstPitch + clip.width) {
		auto drawWidth = clip.width;

		// Skip initial src if clipping on the left.
		// Handles overshoot, i.e. when the RLE segment goes into the unclipped area.
		auto remainingLeftClip = clip.left;
		while (remainingLeftClip > 0) {
			auto v = static_cast<int8_t>(*src++);
			if (v > 0) {
				if (v > remainingLeftClip) {
					const auto overshoot = v - remainingLeftClip;
					RenderLine<Light, OpaquePrefix, PrefixIncrement>(dst, src + remainingLeftClip, overshoot, tbl, prefix - (Width - remainingLeftClip));
					dst += overshoot;
					drawWidth -= overshoot;
				}
				src += v;
			} else {
				v = -v;
				if (v > remainingLeftClip) {
					const auto overshoot = v - remainingLeftClip;
					dst += overshoot;
					drawWidth -= overshoot;
				}
			}
			remainingLeftClip -= v;
		}

		// Draw the non-clipped segment
		while (drawWidth > 0) {
			auto v = static_cast<int8_t>(*src++);
			if (v > 0) {
				if (v > drawWidth) {
					RenderLine<Light, OpaquePrefix, PrefixIncrement>(dst, src, drawWidth, tbl, prefix - (Width - drawWidth));
					src += v;
					dst += drawWidth;
					drawWidth -= v;
					break;
				}
				RenderLine<Light, OpaquePrefix, PrefixIncrement>(dst, src, v, tbl, prefix - (Width - drawWidth));
				src += v;
			} else {
				v = -v;
				if (v > drawWidth) {
					dst += drawWidth;
					drawWidth -= v;
					break;
				}
			}
			dst += v;
			drawWidth -= v;
		}

		// Skip the rest of src line if clipping on the right
		assert(drawWidth <= 0);
		skipRestOfTheLine(clip.right + drawWidth);
		prefix += PrefixIncrement;
	}
}

template <LightType Light, bool OpaquePrefix, int8_t PrefixIncrement = 0, typename Pixel>
DVL_ALWAYS_INLINE DVL_ATTRIBUTE_HOT void RenderTransparentSquare(Pixel *DVL_RESTRICT dst, uint16_t dstPitch, const uint8_t *DVL_RESTRICT src, const uint8_t *DVL_RESTRICT tbl, Clip clip)
{
	if (clip.width == Width && clip.height == Height) {
		RenderTransparentSquareFull<Light, OpaquePrefix, PrefixIncrement>(dst, dstPitch, src, tbl);
	} else {
		RenderTransparentSquareClipped<Light, OpaquePrefix, PrefixIncrement>(dst, dstPitch, src, tbl, clip);
	}
}

/** Vertical clip for the lower and upper triangles of a diamond tile (L/RTRIANGLE).*/
struct DiamondClipY {
	int_fast16_t lowerBottom;
	int_fast16_t lowerTop;
	int_fast16_t upperBottom;
	int_fast16_t upperTop;
};

template <int_fast16_t UpperHeight = TriangleUpperHeight>
DVL_ALWAYS_INLINE DiamondClipY CalculateDiamondClipY(const Clip &clip)
{
	DiamondClipY result;
	if (clip.bottom > LowerHeight) {
		result.lowerBottom = LowerHeight;
		result.upperBottom = clip.bottom - LowerHeight;
		result.lowerTop = result.upperTop = 0;
	} else if (clip.top > UpperHeight) {
		result.upperTop = UpperHeight;
		result.lowerTop = clip.top - UpperHeight;
		result.upperBottom = result.lowerBottom = 0;
	} else {
		result.upperTop = clip.top;
		result.lowerBottom = clip.bottom;
		result.lowerTop = result.upperBottom = 0;
	}
	return result;
}

DVL_ALWAYS_INLINE std::size_t CalculateTriangleSourceSkipLowerBottom(int_fast16_t numLines)
{
	return XStep * numLines * (numLines + 1) / 2 + 2 * ((numLines + 1) / 2);
}

DVL_ALWAYS_INLINE std::size_t CalculateTriangleSourceSkipUpperBottom(int_fast16_t numLines)
{
	return 2 * TriangleUpperHeight * numLines - numLines * (numLines - 1) + 2 * ((numLines + 1) / 2);
}

template <LightType Light, bool Transparent, typename Pixel>
DVL_ALWAYS_INLINE DVL_ATTRIBUTE_HOT void RenderLeftTriangleLower(Pixel *DVL_RESTRICT &dst, uint16_t dstPitch, const uint8_t *DVL_RESTRICT &src, const uint8_t *DVL_RESTRICT tbl)
{
	dst += XStep * (LowerHeight - 1);
	for (auto i = 1; i <= LowerHeight; ++i, dst -= dstPitch + XStep) {
		src += 2 * (i % 2);
		const auto width = XStep * i;
		RenderLineTransparentOrOpaque<Light, Transparent>(dst, src, width, tbl);
		src += width;
	}
}

template <LightType Light, bool Transparent, typename Pixel>
DVL_ALWAYS_INLINE DVL_ATTRIBUTE_HOT void RenderLeftTriangleLowerClipVertical(const DiamondClipY &clipY, Pixel *DVL_RESTRICT &dst, uint16_t dstPitch, const uint8_t *DVL_RESTRICT &src, const uint8_t *DVL_RESTRICT tbl)
{
	src += CalculateTriangleSourceSkipLowerBottom(clipY.lowerBottom);
	dst += XStep * (LowerHeight - clipY.lowerBottom - 1);
	const auto lowerMax = LowerHeight - clipY.lowerTop;
	for (auto i = 1 + clipY.lowerBottom; i <= lowerMax; ++i, dst -= dstPitch + XStep) {
		src += 2 * (i % 2);
		const auto width = XStep * i;
		RenderLineTransparentOrOpaque<Light, Transparent>(dst, src, width, tbl);
		src += width;
	}
}

template <LightType Light, bool Transparent, typename Pixel>
DVL_ALWAYS_INLINE DVL_ATTRIBUTE_HOT void RenderLeftTriangleLowerClipLeftAndVertical(int_fast16_t clipLeft, const DiamondClipY &clipY, Pixel *DVL_RESTRICT &dst, uint16_t dstPitch, const uint8_t *DVL_RESTRICT &src, const uint8_t *DVL_RESTRICT tbl)
{
	src += CalculateTriangleSourceSkipLowerBottom(clipY.lowerBottom);
	dst += XStep * (LowerHeight - clipY.lowerBottom - 1) - clipLeft;
	const auto lowerMax = LowerHeight - clipY.lowerTop;
	for (auto i = 1 + clipY.lowerBottom; i <= lowerMax; ++i, dst -= dstPitch + XStep) {
		src += 2 * (i % 2);
		const auto width = XStep * i;
		const auto startX = Width - XStep * i;
		const auto skip = startX < clipLeft ? clipLeft - startX : 0;
		if (width > skip)
			RenderLineTransparentOrOpaque<Light, Transparent>(dst + skip, src + skip, width - skip, tbl);
		src += width;
	}
}

template <LightType Light, bool Transparent, typename Pixel>
DVL_ALWAYS_INLINE DVL_ATTRIBUTE_HOT void RenderLeftTriangleLowerClipRightAndVertical(int_fast16_t clipRight, const DiamondClipY &clipY, Pixel *DVL_RESTRICT &dst, uint16_t dstPitch, const uint8_t *DVL_RESTRICT &src, const uint8_t *DVL_RESTRICT tbl)
{
	src += CalculateTriangleSourceSkipLowerBottom(clipY.lowerBottom);
	dst += XStep * (LowerHeight - clipY.lowerBottom - 1);
	const auto lowerMax = LowerHeight - clipY.lowerTop;
	for (auto i = 1 + clipY.lowerBottom; i <= lowerMax; ++i, dst -= dstPitch + XStep) {
		src += 2 * (i % 2);
		const auto width = XStep * i;
		if (width > clipRight)
			RenderLineTransparentOrOpaque<Light, Transparent>(dst, src, width - clipRight, tbl);
		src += width;
	}
}

template <LightType Light, bool Transparent, typename Pixel>
DVL_ALWAYS_INLINE DVL_ATTRIBUTE_HOT void RenderLeftTriangleFull(Pixel *DVL_RESTRICT dst, uint16_t dstPitch, const uint8_t *DVL_RESTRICT src, const uint8_t *DVL_RESTRICT tbl)
{
	RenderLeftTriangleLower<Light, Transparent>(dst, dstPitch, src, tbl);
	dst += 2 * XStep;
	for (auto i = 1; i <= TriangleUpperHeight; ++i, dst -= dstPitch - XStep) {
		src += 2 * (i % 2);
		const auto width = Width - XStep * i;
		RenderLineTransparentOrOpaque<Light, Transparent>(dst, src, width, tbl);
		src += width;
	}
}

template <LightType Light, bool Transparent, typename Pixel>
DVL_ALWAYS_INLINE DVL_ATTRIBUTE_HOT void RenderLeftTriangleClipVertical(Pixel *DVL_RESTRICT dst, uint16_t dstPitch, const uint8_t *DVL_RESTRICT src, const uint8_t *DVL_RESTRICT tbl, Clip clip)
{
	const DiamondClipY clipY = CalculateDiamondClipY(clip);
	RenderLeftTriangleLowerClipVertical<Light, Transparent>(clipY, dst, dstPitch, src, tbl);
	src += CalculateTriangleSourceSkipUpperBottom(clipY.upperBottom);
	dst += 2 * XStep + XStep * clipY.upperBottom;
	const auto upperMax = TriangleUpperHeight - clipY.upperTop;
	for (auto i = 1 + clipY.upperBottom; i <= upperMax; ++i, dst -= dstPitch - XStep) {
		src += 2 * (i % 2);
		const auto width = Width - XStep * i;
		RenderLineTransparentOrOpaque<Light, Transparent>(dst, src, width, tbl);
		src += width;
	}
}

template <LightType Light, bool Transparent, typename Pixel>
DVL_ALWAYS_INLINE DVL_ATTRIBUTE_HOT void RenderLeftTriangleClipLeftAndVertical(Pixel *DVL_RESTRICT dst, uint16_t dstPitch, const uint8_t *DVL_RESTRICT src, const uint8_t *DVL_RESTRICT tbl, Clip clip)
{
	const DiamondClipY clipY = CalculateDiamondClipY(clip);
	const int_fast16_t clipLeft = clip.left;
	RenderLeftTriangleLowerClipLeftAndVertical<Light, Transparent>(clipLeft, clipY, dst, dstPitch, src, tbl);
	src += CalculateTriangleSourceSkipUpperBottom(clipY.upperBottom);
	dst += 2 * XStep + XStep * clipY.upperBottom;
	const auto upperMax = TriangleUpperHeight - clipY.upperTop;
	for (auto i = 1 + clipY.upperBottom; i <= upperMax; ++i, dst -= dstPitch - XStep) {
		src += 2 * (i % 2);
		const auto width = Width - XStep * i;
		const auto startX = XStep * i;
		const auto skip = startX < clipLeft ? clipLeft - startX : 0;
		RenderLineTransparentOrOpaque<Light, Transparent>(dst + skip, src + skip, width > skip ? width - skip : 0, tbl);
		src += width;
	}
}

template <LightType Light, bool Transparent, typename Pixel>
DVL_ALWAYS_INLINE DVL_ATTRIBUTE_HOT void RenderLeftTriangleClipRightAndVertical(Pixel *DVL_RESTRICT dst, uint16_t dstPitch, const uint8_t *DVL_RESTRICT src, const uint8_t *DVL_RESTRICT tbl, Clip clip)
{
	const DiamondClipY clipY = CalculateDiamondClipY(clip);
	const int_fast16_t clipRight = clip.right;
	RenderLeftTriangleLowerClipRightAndVertical<Light, Transparent>(clipRight, clipY, dst, dstPitch, src, tbl);
	src += CalculateTriangleSourceSkipUpperBottom(clipY.upperBottom);
	dst += 2 * XStep + XStep * clipY.upperBottom;
	const auto upperMax = TriangleUpperHeight - clipY.upperTop;
	for (auto i = 1 + clipY.upperBottom; i <= upperMax; ++i, dst -= dstPitch - XStep) {
		src += 2 * (i % 2);
		const auto width = Width - XStep * i;
		if (width <= clipRight)
			break;
		RenderLineTransparentOrOpaque<Light, Transparent>(dst, src, width - clipRight, tbl);
		src += width;
	}
}

template <LightType Light, bool Transparent, typename Pixel>
DVL_ALWAYS_INLINE DVL_ATTRIBUTE_HOT void RenderLeftTriangle(Pixel *DVL_RESTRICT dst, uint16_t dstPitch, const uint8_t *DVL_RESTRICT src, const uint8_t *DVL_RESTRICT tbl, Clip clip)
{
	if (clip.width == Width) {
		if (clip.height == TriangleHeight) {
			RenderLeftTriangleFull<Light, Transparent>(dst, dstPitch, src, tbl);
		} else {
			RenderLeftTriangleClipVertical<Light, Transparent>(dst, dstPitch, src, tbl, clip);
		}
	} else if (clip.right == 0) {
		RenderLeftTriangleClipLeftAndVertical<Light, Transparent>(dst, dstPitch, src, tbl, clip);
	} else {
		RenderLeftTriangleClipRightAndVertical<Light, Transparent>(dst, dstPitch, src, tbl, clip);
	}
}

template <LightType Light, bool Transparent, typename Pixel>
DVL_ALWAYS_INLINE DVL_ATTRIBUTE_HOT void RenderRightTriangleLower(Pixel *DVL_RESTRICT &dst, uint16_t dstPitch, const uint8_t *DVL_RESTRICT &src, const uint8_t *DVL_RESTRICT tbl)
{
	for (auto i = 1; i <= LowerHeight; ++i, dst -= dstPitch) {
		const auto width = XStep * i;
		RenderLineTransparentOrOpaque<Light, Transparent>(dst, src, width, tbl);
		src += width + 2 * (i % 2);
	}
}

template <LightType Light, bool Transparent, typename Pixel>
DVL_ALWAYS_INLINE DVL_ATTRIBUTE_HOT void RenderRightTriangleLowerClipVertical(const DiamondClipY &clipY, Pixel *DVL_RESTRICT &dst, uint16_t dstPitch, const uint8_t *DVL_RESTRICT &src, const uint8_t *DVL_RESTRICT tbl)
{
	src += CalculateTriangleSourceSkipLowerBottom(clipY.lowerBottom);
	const auto lowerMax = LowerHeight - clipY.lowerTop;
	for (auto i = 1 + clipY.lowerBottom; i <= lowerMax; ++i, dst -= dstPitch) {
		const auto width = XStep * i;
		RenderLineTransparentOrOpaque<Light, Transparent>(dst, src, width, tbl);
		src += width + 2 * (i % 2);
	}
}

template <LightType Light, bool Transparent, typename Pixel>
DVL_ALWAYS_INLINE DVL_ATTRIBUTE_HOT void RenderRightTriangleLowerClipLeftAndVertical(int_fast16_t clipLeft, const DiamondClipY &clipY, Pixel *DVL_RESTRICT &dst, uint16_t dstPitch, const uint8_t *DVL_RESTRICT &src, const uint8_t *DVL_RESTRICT tbl)
{
	src += CalculateTriangleSourceSkipLowerBottom(clipY.lowerBottom);
	const auto lowerMax = LowerHeight - clipY.lowerTop;
	for (auto i = 1 + clipY.lowerBottom; i <= lowerMax; ++i, dst -= dstPitch) {
		const auto width = XStep * i;
		if (width > clipLeft)
			RenderLineTransparentOrOpaque<Light, Transparent>(dst, src + clipLeft, width - clipLeft, tbl);
		src += width + 2 * (i % 2);
	}
}

template <LightType Light, bool Transparent, typename Pixel>
DVL_ALWAYS_INLINE DVL_ATTRIBUTE_HOT void RenderRightTriangleLowerClipRightAndVertical(int_fast16_t clipRight, const DiamondClipY &clipY, Pixel *DVL_RESTRICT &dst, uint16_t dstPitch, const uint8_t *DVL_RESTRICT &src, const uint8_t *DVL_RESTRICT tbl)
{
	src += CalculateTriangleSourceSkipLowerBottom(clipY.lowerBottom);
	const auto lowerMax = LowerHeight - clipY.lowerTop;
	for (auto i = 1 + clipY.lowerBottom; i <= lowerMax; ++i, dst -= dstPitch) {
		const auto width = XStep * i;
		const auto skip = Width - width < clipRight ? clipRight - (Width - width) : 0;
		if (width > skip)
			RenderLineTransparentOrOpaque<Light, Transparent>(dst, src, width - skip, tbl);
		src += width + 2 * (i % 2);
	}
}

template <LightType Light, bool Transparent, typename Pixel>
DVL_ALWAYS_INLINE DVL_ATTRIBUTE_HOT void RenderRightTriangleFull(Pixel *DVL_RESTRICT dst, uint16_t dstPitch, const uint8_t *DVL_RESTRICT src, const uint8_t *DVL_RESTRICT tbl)
{
	RenderRightTriangleLower<Light, Transparent>(dst, dstPitch, src, tbl);
	for (auto i = 1; i <= TriangleUpperHeight; ++i, dst -= dstPitch) {
		const auto width = Width - XStep * i;
		RenderLineTransparentOrOpaque<Light, Transparent>(dst, src, width, tbl);
		src += width + 2 * (i % 2);
	}
}

template <LightType Light, bool Transparent, typename Pixel>
DVL_ALWAYS_INLINE DVL_ATTRIBUTE_HOT void RenderRightTriangleClipVertical(Pixel *DVL_RESTRICT dst, uint16_t dstPitch, const uint8_t *DVL_RESTRICT src, const uint8_t *DVL_RESTRICT tbl, Clip clip)
{
	const DiamondClipY clipY = CalculateDiamondClipY(clip);
	RenderRightTriangleLowerClipVertical<Light, Transparent>(clipY, dst, dstPitch, src, tbl);
	src += CalculateTriangleSourceSkipUpperBottom(clipY.upperBottom);
	const auto upperMax = TriangleUpperHeight - clipY.upperTop;
	for (auto i = 1 + clipY.upperBottom; i <= upperMax; ++i, dst -= dstPitch) {
		const auto width = Width - XStep * i;
		RenderLineTransparentOrOpaque<Light, Transparent>(dst, src, width, tbl);
		src += width + 2 * (i % 2);
	}
}

template <LightType Light, bool Transparent, typename Pixel>
DVL_ALWAYS_INLINE DVL_ATTRIBUTE_HOT void RenderRightTriangleClipLeftAndVertical(Pixel *DVL_RESTRICT dst, uint16_t dstPitch, const uint8_t *DVL_RESTRICT src, const uint8_t *DVL_RESTRICT tbl, Clip clip)
{
	const DiamondClipY clipY = CalculateDiamondClipY(clip);
	const int_fast16_t clipLeft = clip.left;
	RenderRightTriangleLowerClipLeftAndVertical<Light, Transparent>(clipLeft, clipY, dst, dstPitch, src, tbl);
	src += CalculateTriangleSourceSkipUpperBottom(clipY.upperBottom);
	const auto upperMax = TriangleUpperHeight - clipY.upperTop;
	for (auto i = 1 + clipY.upperBottom; i <= upperMax; ++i, dst -= dstPitch) {
		const auto width = Width - XStep * i;
		if (width <= clipLeft)
			break;
		RenderLineTransparentOrOpaque<Light, Transparent>(dst, src + clipLeft, width - clipLeft, tbl);
		src += width + 2 * (i % 2);
	}
}

template <LightType Light, bool Transparent, typename Pixel>
DVL_ALWAYS_INLINE DVL_ATTRIBUTE_HOT void RenderRightTriangleClipRightAndVertical(Pixel *DVL_RESTRICT dst, uint16_t dstPitch, const uint8_t *DVL_RESTRICT src, const uint8_t *DVL_RESTRICT tbl, Clip clip)
{
	const DiamondClipY clipY = CalculateDiamondClipY(clip);
	const int_fast16_t clipRight = clip.right;
	RenderRightTriangleLowerClipRightAndVertical<Light, Transparent>(clipRight, clipY, dst, dstPitch, src, tbl);
	src += CalculateTriangleSourceSkipUpperBottom(clipY.upperBottom);
	const auto upperMax = TriangleUpperHeight - clipY.upperTop;
	for (auto i = 1 + clipY.upperBottom; i <= upperMax; ++i, dst -= dstPitch) {
		const auto width = Width - XStep * i;
		const auto skip = Width - width < clipRight ? clipRight - (Width - width) : 0;
		RenderLineTransparentOrOpaque<Light, Transparent>(dst, src, width > skip ? width - skip : 0, tbl);
		src += width + 2 * (i % 2);
	}
}

template <LightType Light, bool Transparent, typename Pixel>
DVL_ALWAYS_INLINE DVL_ATTRIBUTE_HOT void RenderRightTriangle(Pixel *DVL_RESTRICT dst, uint16_t dstPitch, const uint8_t *DVL_RESTRICT src, const uint8_t *DVL_RESTRICT tbl, Clip clip)
{
	if (clip.width == Width) {
		if (clip.height == TriangleHeight) {
			RenderRightTriangleFull<Light, Transparent>(dst, dstPitch, src, tbl);
		} else {
			RenderRightTriangleClipVertical<Light, Transparent>(dst, dstPitch, src, tbl, clip);
		}
	} else if (clip.right == 0) {
		RenderRightTriangleClipLeftAndVertical<Light, Transparent>(dst, dstPitch, src, tbl, clip);
	} else {
		RenderRightTriangleClipRightAndVertical<Light, Transparent>(dst, dstPitch, src, tbl, clip);
	}
}

template <LightType Light, bool OpaquePrefix, int8_t PrefixIncrement, typename Pixel>
DVL_ALWAYS_INLINE DVL_ATTRIBUTE_HOT void RenderTrapezoidUpperHalf(Pixel *DVL_RESTRICT dst, uint16_t dstPitch, const uint8_t *DVL_RESTRICT src, const uint8_t *DVL_RESTRICT tbl) {
	if (PrefixIncrement != 0) {
		// The first and the last line are always fully transparent/opaque (or vice-versa).
		// We handle them specially to avoid calling the blitter with width=0.
		const uint8_t *srcEnd = src + Width * TrapezoidUpperHeight;
		constexpr bool FirstLineIsTransparent = OpaquePrefix ^ (PrefixIncrement < 0);
		if (FirstLineIsTransparent) {
			if (!SkipTransparentPixels<OpaquePrefix, PrefixIncrement>)
				RenderLineTransparent<Light>(dst, src, Width, tbl);
		} else {
			RenderLineOpaque<Light>(dst, src, Width, tbl);
		}
		src += Width;
		dst -= dstPitch;
		uint8_t prefixWidth = (PrefixIncrement < 0 ? 32 : 0) + PrefixIncrement;
		do {
			RenderLineTransparentAndOpaque<Light, OpaquePrefix, PrefixIncrement>(dst, src, prefixWidth, Width, tbl);
			prefixWidth += PrefixIncrement;
			src += Width;
			dst -= dstPitch;
		} while (src != srcEnd);
	} else { // PrefixIncrement == 0;
		const uint8_t *srcEnd = src + Width * TrapezoidUpperHeight;
		do {
			RenderLineTransparentOrOpaque<Light, OpaquePrefix>(dst, src, Width, tbl);
			src += Width;
			dst -= dstPitch;
		} while (src != srcEnd);
	}
}

template <LightType Light, bool OpaquePrefix, int8_t PrefixIncrement, typename Pixel>
DVL_ALWAYS_INLINE DVL_ATTRIBUTE_HOT void RenderTrapezoidUpperHalfClipVertical(const Clip &clip, const DiamondClipY &clipY, Pixel *DVL_RESTRICT dst, uint16_t dstPitch, const uint8_t *DVL_RESTRICT src, const uint8_t *DVL_RESTRICT tbl) {
	const auto upperMax = TrapezoidUpperHeight - clipY.upperTop;
	int8_t prefix = InitPrefix<PrefixIncrement>(clip.bottom);
	for (auto i = 1 + clipY.upperBottom; i <= upperMax; ++i, dst -= dstPitch) {
		RenderLine<Light, OpaquePrefix, PrefixIncrement>(dst, src, Width, tbl, prefix);
		src += Width;
		prefix += PrefixIncrement;
	}
}

template <LightType Light, bool OpaquePrefix, int8_t PrefixIncrement, typename Pixel>
DVL_ALWAYS_INLINE DVL_ATTRIBUTE_HOT void RenderTrapezoidUpperHalfClipLeftAndVertical(const Clip &clip, const DiamondClipY &clipY, Pixel *DVL_RESTRICT dst, uint16_t dstPitch, const uint8_t *DVL_RESTRICT src, const uint8_t *DVL_RESTRICT tbl) {
	const auto upperMax = TrapezoidUpperHeight - clipY.upperTop;
	int8_t prefix = InitPrefix<PrefixIncrement>(clip.bottom);
	for (auto i = 1 + clipY.upperBottom; i <= upperMax; ++i, dst -= dstPitch) {
		RenderLine<Light, OpaquePrefix, PrefixIncrement>(dst, src, clip.width, tbl, prefix - clip.left);
		src += Width;
		prefix += PrefixIncrement;
	}
}

template <LightType Light, bool OpaquePrefix, int8_t PrefixIncrement, typename Pixel>
DVL_ALWAYS_INLINE DVL_ATTRIBUTE_HOT void RenderTrapezoidUpperHalfClipRightAndVertical(const Clip &clip, const DiamondClipY &clipY, Pixel *DVL_RESTRICT dst, uint16_t dstPitch, const uint8_t *DVL_RESTRICT src, const uint8_t *DVL_RESTRICT tbl) {
	const auto upperMax = TrapezoidUpperHeight - clipY.upperTop;
	int8_t prefix = InitPrefix<PrefixIncrement>(clip.bottom);
	for (auto i = 1 + clipY.upperBottom; i <= upperMax; ++i, dst -= dstPitch) {
		RenderLine<Light, OpaquePrefix, PrefixIncrement>(dst, src, clip.width, tbl, prefix);
		src += Width;
		prefix += PrefixIncrement;
	}
}

template <LightType Light, bool OpaquePrefix, int8_t PrefixIncrement, typename Pixel>
DVL_ALWAYS_INLINE DVL_ATTRIBUTE_HOT void RenderLeftTrapezoidFull(Pixel *DVL_RESTRICT dst, uint16_t dstPitch, const uint8_t *DVL_RESTRICT src, const uint8_t *DVL_RESTRICT tbl)
{
	RenderLeftTriangleLower<Light, LowerHalfTransparent<OpaquePrefix, PrefixIncrement>>(dst, dstPitch, src, tbl);
	dst += XStep;
	RenderTrapezoidUpperHalf<Light, OpaquePrefix, PrefixIncrement>(dst, dstPitch, src, tbl);
}

template <LightType Light, bool OpaquePrefix, int8_t PrefixIncrement, typename Pixel>
DVL_ALWAYS_INLINE DVL_ATTRIBUTE_HOT void RenderLeftTrapezoidClipVertical(Pixel *DVL_RESTRICT dst, uint16_t dstPitch, const uint8_t *DVL_RESTRICT src, const uint8_t *DVL_RESTRICT tbl, Clip clip)
{
	const DiamondClipY clipY = CalculateDiamondClipY<TrapezoidUpperHeight>(clip);
	RenderLeftTriangleLowerClipVertical<Light, LowerHalfTransparent<OpaquePrefix, PrefixIncrement>>(clipY, dst, dstPitch, src, tbl);
	src += clipY.upperBottom * Width;
	dst += XStep;
	RenderTrapezoidUpperHalfClipVertical<Light, OpaquePrefix, PrefixIncrement>(clip, clipY, dst, dstPitch, src, tbl);
}

template <LightType Light, bool OpaquePrefix, int8_t PrefixIncrement, typename Pixel>
DVL_ALWAYS_INLINE DVL_ATTRIBUTE_HOT void RenderLeftTrapezoidClipLeftAndVertical(Pixel *DVL_RESTRICT dst, uint16_t dstPitch, const uint8_t *DVL_RESTRICT src, const uint8_t *DVL_RESTRICT tbl, Clip clip)
{
	const DiamondClipY clipY = CalculateDiamondClipY<TrapezoidUpperHeight>(clip);
	RenderLeftTriangleLowerClipLeftAndVertical<Light, LowerHalfTransparent<OpaquePrefix, PrefixIncrement>>(clip.left, clipY, dst, dstPitch, src, tbl);
	src += clipY.upperBottom * Width + clip.left;
	dst += XStep + clip.left;
	RenderTrapezoidUpperHalfClipLeftAndVertical<Light, OpaquePrefix, PrefixIncrement>(clip, clipY, dst, dstPitch, src, tbl);
}

template <LightType Light, bool OpaquePrefix, int8_t PrefixIncrement, typename Pixel>
DVL_ALWAYS_INLINE DVL_ATTRIBUTE_HOT void RenderLeftTrapezoidClipRightAndVertical(Pixel *DVL_RESTRICT dst, uint16_t dstPitch, const uint8_t *DVL_RESTRICT src, const uint8_t *DVL_RESTRICT tbl, Clip clip)
{
	const DiamondClipY clipY = CalculateDiamondClipY<TrapezoidUpperHeight>(clip);
	RenderLeftTriangleLowerClipRightAndVertical<Light, LowerHalfTransparent<OpaquePrefix, PrefixIncrement>>(clip.right, clipY, dst, dstPitch, src, tbl);
	src += clipY.upperBottom * Width;
	dst += XStep;
	RenderTrapezoidUpperHalfClipRightAndVertical<Light, OpaquePrefix, PrefixIncrement>(clip, clipY, dst, dstPitch, src, tbl);
}

template <LightType Light, bool OpaquePrefix, int8_t PrefixIncrement = 0, typename Pixel>
DVL_ALWAYS_INLINE DVL_ATTRIBUTE_HOT void RenderLeftTrapezoid(Pixel *DVL_RESTRICT dst, uint16_t dstPitch, const uint8_t *DVL_RESTRICT src, const uint8_t *DVL_RESTRICT tbl, Clip clip)
{
	if (clip.width == Width) {
		if (clip.height == Height) {
			RenderLeftTrapezoidFull<Light, OpaquePrefix, PrefixIncrement>(dst, dstPitch, src, tbl);
		} else {
			RenderLeftTrapezoidClipVertical<Light, OpaquePrefix, PrefixIncrement>(dst, dstPitch, src, tbl, clip);
		}
	} else if (clip.right == 0) {
		RenderLeftTrapezoidClipLeftAndVertical<Light, OpaquePrefix, PrefixIncrement>(dst, dstPitch, src, tbl, clip);
	} else {
		RenderLeftTrapezoidClipRightAndVertical<Light, OpaquePrefix, PrefixIncrement>(dst, dstPitch, src, tbl, clip);
	}
}

template <LightType Light, bool OpaquePrefix, int8_t PrefixIncrement, typename Pixel>
DVL_ALWAYS_INLINE DVL_ATTRIBUTE_HOT void RenderRightTrapezoidFull(Pixel *DVL_RESTRICT dst, uint16_t dstPitch, const uint8_t *DVL_RESTRICT src, const uint8_t *DVL_RESTRICT tbl)
{
	RenderRightTriangleLower<Light, LowerHalfTransparent<OpaquePrefix, PrefixIncrement>>(dst, dstPitch, src, tbl);
	RenderTrapezoidUpperHalf<Light, OpaquePrefix, PrefixIncrement>(dst, dstPitch, src, tbl);
}

template <LightType Light, bool OpaquePrefix, int8_t PrefixIncrement, typename Pixel>
DVL_ALWAYS_INLINE DVL_ATTRIBUTE_HOT void RenderRightTrapezoidClipVertical(Pixel *DVL_RESTRICT dst, uint16_t dstPitch, const uint8_t *DVL_RESTRICT src, const uint8_t *DVL_RESTRICT tbl, Clip clip)
{
	const DiamondClipY clipY = CalculateDiamondClipY<TrapezoidUpperHeight>(clip);
	RenderRightTriangleLowerClipVertical<Light, LowerHalfTransparent<OpaquePrefix, PrefixIncrement>>(clipY, dst, dstPitch, src, tbl);
	src += clipY.upperBottom * Width;
	RenderTrapezoidUpperHalfClipVertical<Light, OpaquePrefix, PrefixIncrement>(clip, clipY, dst, dstPitch, src, tbl);
}

template <LightType Light, bool OpaquePrefix, int8_t PrefixIncrement, typename Pixel>
DVL_ALWAYS_INLINE DVL_ATTRIBUTE_HOT void RenderRightTrapezoidClipLeftAndVertical(Pixel *DVL_RESTRICT dst, uint16_t dstPitch, const uint8_t *DVL_RESTRICT src, const uint8_t *DVL_RESTRICT tbl, Clip clip)
{
	const DiamondClipY clipY = CalculateDiamondClipY<TrapezoidUpperHeight>(clip);
	RenderRightTriangleLowerClipLeftAndVertical<Light, LowerHalfTransparent<OpaquePrefix, PrefixIncrement>>(clip.left, clipY, dst, dstPitch, src, tbl);
	src += clipY.upperBottom * Width + clip.left;
	RenderTrapezoidUpperHalfClipLeftAndVertical<Light, OpaquePrefix, PrefixIncrement>(clip, clipY, dst, dstPitch, src, tbl);
}

template <LightType Light, bool OpaquePrefix, int8_t PrefixIncrement, typename Pixel>
DVL_ALWAYS_INLINE DVL_ATTRIBUTE_HOT void RenderRightTrapezoidClipRightAndVertical(Pixel *DVL_RESTRICT dst, uint16_t dstPitch, const uint8_t *DVL_RESTRICT src, const uint8_t *DVL_RESTRICT tbl, Clip clip)
{
	const DiamondClipY clipY = CalculateDiamondClipY<TrapezoidUpperHeight>(clip);
	RenderRightTriangleLowerClipRightAndVertical<Light, LowerHalfTransparent<OpaquePrefix, PrefixIncrement>>(clip.right, clipY, dst, dstPitch, src, tbl);
	src += clipY.upperBottom * Width;
	RenderTrapezoidUpperHalfClipRightAndVertical<Light, OpaquePrefix, PrefixIncrement>(clip, clipY, dst, dstPitch, src, tbl);
}

template <LightType Light, bool OpaquePrefix, int8_t PrefixIncrement = 0, typename Pixel>
DVL_ALWAYS_INLINE DVL_ATTRIBUTE_HOT void RenderRightTrapezoid(Pixel *DVL_RESTRICT dst, uint16_t dstPitch, const uint8_t *DVL_RESTRICT src, const uint8_t *DVL_RESTRICT tbl, Clip clip)
{
	if (clip.width == Width) {
		if (clip.height == Height) {
			RenderRightTrapezoidFull<Light, OpaquePrefix, PrefixIncrement>(dst, dstPitch, src, tbl);
		} else {
			RenderRightTrapezoidClipVertical<Light, OpaquePrefix, PrefixIncrement>(dst, dstPitch, src, tbl, clip);
		}
	} else if (clip.right == 0) {
		RenderRightTrapezoidClipLeftAndVertical<Light, OpaquePrefix, PrefixIncrement>(dst, dstPitch, src, tbl, clip);
	} else {
		RenderRightTrapezoidClipRightAndVertical<Light, OpaquePrefix, PrefixIncrement>(dst, dstPitch, src, tbl, clip);
	}
}

template <LightType Light, bool Transparent, typename Pixel>
DVL_ALWAYS_INLINE DVL_ATTRIBUTE_HOT void RenderTileType(TileType tile, Pixel *DVL_RESTRICT dst, uint16_t dstPitch, const uint8_t *DVL_RESTRICT src, const uint8_t *DVL_RESTRICT tbl, Clip clip)
{
	switch (tile) {
	case TileType::Square:
		RenderSquare<Light, Transparent>(dst, dstPitch, src, tbl, clip);
		break;
	case TileType::TransparentSquare:
		RenderTransparentSquare<Light, Transparent>(dst, dstPitch, src, tbl, clip);
		break;
	case TileType::LeftTriangle:
		RenderLeftTriangle<Light, Transparent>(dst, dstPitch, src, tbl, clip);
		break;
	case TileType::RightTriangle:
		RenderRightTriangle<Light, Transparent>(dst, dstPitch, src, tbl, clip);
		break;
	case TileType::LeftTrapezoid:
		RenderLeftTrapezoid<Light, Transparent>(dst, dstPitch, src, tbl, clip);
		break;
	case TileType::RightTrapezoid:
		RenderRightTrapezoid<Light, Transparent>(dst, dstPitch, src, tbl, clip);
		break;
	}
}

template <bool OpaquePrefix, int8_t PrefixIncrement, typename Pixel>
DVL_ALWAYS_INLINE DVL_ATTRIBUTE_HOT void RenderTransparentSquareDispatch(Pixel *DVL_RESTRICT dst, uint16_t dstPitch, const uint8_t *DVL_RESTRICT src, const uint8_t *DVL_RESTRICT tbl, Clip clip)
{
	if (tbl == LightTables[LightsMax].data()) {
		RenderTransparentSquare<LightType::FullyDark, OpaquePrefix, PrefixIncrement>(dst, dstPitch, src, tbl, clip);
	} else if (tbl == LightTables[0].data()) {
		RenderTransparentSquare<LightType::FullyLit, OpaquePrefix, PrefixIncrement>(dst, dstPitch, src, tbl, clip);
	} else {
		RenderTransparentSquare<LightType::PartiallyLit, OpaquePrefix, PrefixIncrement>(dst, dstPitch, src, tbl, clip);
	}
}

template <LightType Light, bool OpaquePrefix, int8_t PrefixIncrement, typename Pixel>
DVL_ALWAYS_INLINE DVL_ATTRIBUTE_HOT void RenderLeftTrapezoidOrTransparentSquare(TileType tile, Pixel *DVL_RESTRICT dst, uint16_t dstPitch, const uint8_t *DVL_RESTRICT src, const uint8_t *DVL_RESTRICT tbl, Clip clip)
{
	switch (tile) {
	case TileType::TransparentSquare:
		RenderTransparentSquare<Light, OpaquePrefix, PrefixIncrement>(dst, dstPitch, src, tbl, clip);
		break;
	case TileType::LeftTrapezoid:
		RenderLeftTrapezoid<Light, OpaquePrefix, PrefixIncrement>(dst, dstPitch, src, tbl, clip);
		break;
	default:
		app_fatal("Given mask can only be applied to TransparentSquare or LeftTrapezoid tiles");
	}
}

template <LightType Light, bool OpaquePrefix, int8_t PrefixIncrement, typename Pixel>
DVL_ALWAYS_INLINE DVL_ATTRIBUTE_HOT void RenderRightTrapezoidOrTransparentSquare(TileType tile, Pixel *DVL_RESTRICT dst, uint16_t dstPitch, const uint8_t *DVL_RESTRICT src, const uint8_t *DVL_RESTRICT tbl, Clip clip)
{
	switch (tile) {
	case TileType::TransparentSquare:
		RenderTransparentSquare<Light, OpaquePrefix, PrefixIncrement>(dst, dstPitch, src, tbl, clip);
		break;
	case TileType::RightTrapezoid:
		RenderRightTrapezoid<Light, OpaquePrefix, PrefixIncrement>(dst, dstPitch, src, tbl, clip);
		break;
	default:
		app_fatal("Given mask can only be applied to TransparentSquare or LeftTrapezoid tiles");
	}
}

template <bool OpaquePrefix, int8_t PrefixIncrement, typename Pixel>
DVL_ALWAYS_INLINE DVL_ATTRIBUTE_HOT void RenderLeftTrapezoidOrTransparentSquareDispatch(TileType tile, Pixel *DVL_RESTRICT dst, uint16_t dstPitch, const uint8_t *DVL_RESTRICT src, const uint8_t *DVL_RESTRICT tbl, Clip clip)
{
	if (tbl == LightTables[LightsMax].data()) {
		RenderLeftTrapezoidOrTransparentSquare<LightType::FullyDark, OpaquePrefix, PrefixIncrement>(tile, dst, dstPitch, src, tbl, clip);
	} else if (tbl == LightTables[0].data()) {
		RenderLeftTrapezoidOrTransparentSquare<LightType::FullyLit, OpaquePrefix, PrefixIncrement>(tile, dst, dstPitch, src, tbl, clip);
	} else {
		RenderLeftTrapezoidOrTransparentSquare<LightType::PartiallyLit, OpaquePrefix, PrefixIncrement>(tile, dst, dstPitch, src, tbl, clip);
	}
}

template <bool OpaquePrefix, int8_t PrefixIncrement, typename Pixel>
DVL_ALWAYS_INLINE DVL_ATTRIBUTE_HOT void RenderRightTrapezoidOrTransparentSquareDispatch(TileType tile, Pixel *DVL_RESTRICT dst, uint16_t dstPitch, const uint8_t *DVL_RESTRICT src, const uint8_t *DVL_RESTRICT tbl, Clip clip)
{
	if (tbl == LightTables[LightsMax].data()) {
		RenderRightTrapezoidOrTransparentSquare<LightType::FullyDark, OpaquePrefix, PrefixIncrement>(tile, dst, dstPitch, src, tbl, clip);
	} else if (tbl == LightTables[0].data()) {
		RenderRightTrapezoidOrTransparentSquare<LightType::FullyLit, OpaquePrefix, PrefixIncrement>(tile, dst, dstPitch, src, tbl, clip);
	} else {
		RenderRightTrapezoidOrTransparentSquare<LightType::PartiallyLit, OpaquePrefix, PrefixIncrement>(tile, dst, dstPitch, src, tbl, clip);
	}
}

template <bool Transparent, typename Pixel>
DVL_ALWAYS_INLINE DVL_ATTRIBUTE_HOT void RenderTileDispatch(TileType tile, Pixel *DVL_RESTRICT dst, uint16_t dstPitch, const uint8_t *DVL_RESTRICT src, const uint8_t *DVL_RESTRICT tbl, Clip clip)
{
	if (tbl == LightTables[LightsMax].data()) {
		RenderTileType<LightType::FullyDark, Transparent>(tile, dst, dstPitch, src, tbl, clip);
	} else if (tbl == LightTables[0].data()) {
		RenderTileType<LightType::FullyLit, Transparent>(tile, dst, dstPitch, src, tbl, clip);
	} else {
		RenderTileType<LightType::PartiallyLit, Transparent>(tile, dst, dstPitch, src, tbl, clip);
	}
}

// Blit with left and vertical clipping.
template <typename Pixel>
void RenderBlackTileClipLeftAndVertical(Pixel *DVL_RESTRICT dst, uint16_t dstPitch, int sx, DiamondClipY clipY)
{
	dst += XStep * (LowerHeight - clipY.lowerBottom - 1);
	// Lower triangle (drawn bottom to top):
	const auto lowerMax = LowerHeight - clipY.lowerTop;
	for (auto i = clipY.lowerBottom + 1; i <= lowerMax; ++i, dst -= dstPitch + XStep) {
		const auto w = 2 * XStep * i;
		const auto curX = sx + TILE_WIDTH / 2 - XStep * i;
		if (curX >= 0) {
			BlitFillDirect(dst, w, 0);
		} else if (-curX < w) { // not a zero length: BlitFillDirect assumes one (round 37 audit)
			BlitFillDirect(dst - curX, w + curX, 0);
		}
	}
	dst += 2 * XStep + XStep * clipY.upperBottom;
	// Upper triangle (drawn bottom to top):
	const auto upperMax = TriangleUpperHeight - clipY.upperTop;
	for (auto i = clipY.upperBottom; i < upperMax; ++i, dst -= dstPitch - XStep) {
		const auto w = 2 * XStep * (TriangleUpperHeight - i);
		const auto curX = sx + TILE_WIDTH / 2 - XStep * (TriangleUpperHeight - i);
		if (curX >= 0) {
			BlitFillDirect(dst, w, 0);
		} else if (-curX < w) { // not a zero length: BlitFillDirect assumes one (round 37 audit)
			BlitFillDirect(dst - curX, w + curX, 0);
		} else {
			break;
		}
	}
}

// Blit with right and vertical clipping.
template <typename Pixel>
void RenderBlackTileClipRightAndVertical(Pixel *DVL_RESTRICT dst, uint16_t dstPitch, int_fast16_t maxWidth, DiamondClipY clipY)
{
	dst += XStep * (LowerHeight - clipY.lowerBottom - 1);
	// Lower triangle (drawn bottom to top):
	const auto lowerMax = LowerHeight - clipY.lowerTop;
	for (auto i = clipY.lowerBottom + 1; i <= lowerMax; ++i, dst -= dstPitch + XStep) {
		const auto width = 2 * XStep * i;
		const auto endX = TILE_WIDTH / 2 + XStep * i;
		const auto skip = endX > maxWidth ? endX - maxWidth : 0;
		if (width > skip)
			BlitFillDirect(dst, width - skip, 0);
	}
	dst += 2 * XStep + XStep * clipY.upperBottom;
	// Upper triangle (drawn bottom to top):
	const auto upperMax = TriangleUpperHeight - clipY.upperTop;
	for (auto i = 1 + clipY.upperBottom; i <= upperMax; ++i, dst -= dstPitch - XStep) {
		const auto width = TILE_WIDTH - 2 * XStep * i;
		const auto endX = TILE_WIDTH / 2 + XStep * (TriangleUpperHeight - i + 1);
		const auto skip = endX > maxWidth ? endX - maxWidth : 0;
		if (width <= skip)
			break;
		BlitFillDirect(dst, width - skip, 0);
	}
}

// Blit with vertical clipping only.
template <typename Pixel>
void RenderBlackTileClipY(Pixel *DVL_RESTRICT dst, uint16_t dstPitch, DiamondClipY clipY)
{
	dst += XStep * (LowerHeight - clipY.lowerBottom - 1);
	// Lower triangle (drawn bottom to top):
	const auto lowerMax = LowerHeight - clipY.lowerTop;
	for (auto i = 1 + clipY.lowerBottom; i <= lowerMax; ++i, dst -= dstPitch + XStep) {
		BlitFillDirect(dst, 2 * XStep * i, 0);
	}
	dst += 2 * XStep + XStep * clipY.upperBottom;
	// Upper triangle (drawn bottom to top):
	const auto upperMax = TriangleUpperHeight - clipY.upperTop;
	for (auto i = 1 + clipY.upperBottom; i <= upperMax; ++i, dst -= dstPitch - XStep) {
		BlitFillDirect(dst, TILE_WIDTH - 2 * XStep * i, 0);
	}
}

// Blit a black tile without clipping (must be fully in bounds).
template <typename Pixel>
void RenderBlackTileFull(Pixel *DVL_RESTRICT dst, uint16_t dstPitch)
{
	dst += XStep * (LowerHeight - 1);
	// Tile is fully in bounds, can use constant loop boundaries.
	// Lower triangle (drawn bottom to top):
	for (unsigned i = 1; i <= LowerHeight; ++i, dst -= dstPitch + XStep) {
		BlitFillDirect(dst, 2 * XStep * i, 0);
	}
	dst += 2 * XStep;
	// Upper triangle (drawn bottom to to top):
	for (unsigned i = 1; i <= TriangleUpperHeight; ++i, dst -= dstPitch - XStep) {
		BlitFillDirect(dst, TILE_WIDTH - 2 * XStep * i, 0);
	}
}

} // namespace

#ifdef DUN_RENDER_STATS
std::unordered_map<DunRenderType, size_t, DunRenderTypeHash> DunRenderStats;

string_view TileTypeToString(TileType tileType)
{
	// clang-format off
	switch (tileType) {
	case TileType::Square: return "Square";
	case TileType::TransparentSquare: return "TransparentSquare";
	case TileType::LeftTriangle: return "LeftTriangle";
	case TileType::RightTriangle: return "RightTriangle";
	case TileType::LeftTrapezoid: return "LeftTrapezoid";
	case TileType::RightTrapezoid: return "RightTrapezoid";
	default: return "???";
	}
	// clang-format on
}

string_view MaskTypeToString(MaskType maskType)
{
	// clang-format off
	switch (maskType) {
	case MaskType::Solid: return "Solid";
	case MaskType::Transparent: return "Transparent";
	case MaskType::Right: return "Right";
	case MaskType::Left: return "Left";
	case MaskType::RightFoliage: return "RightFoliage";
	case MaskType::LeftFoliage: return "LeftFoliage";
	default: return "???";
	}
	// clang-format on
}
#endif

void RenderTile(const Surface &out, Point position,
    LevelCelBlock levelCelBlock, MaskType maskType, const uint8_t *tbl)
{
	const TileType tile = levelCelBlock.type();

#ifdef DEBUG_RENDER_OFFSET_X
	position.x += DEBUG_RENDER_OFFSET_X;
#endif
#ifdef DEBUG_RENDER_OFFSET_Y
	position.y += DEBUG_RENDER_OFFSET_Y;
#endif
#ifdef DEBUG_RENDER_COLOR
	DBGCOLOR = GetTileDebugColor(tile);
#endif

	const Clip clip = CalculateClip(position.x, position.y, Width, GetTileHeight(tile), out);
	if (clip.width <= 0 || clip.height <= 0)
		return;

	const auto *pFrameTable = reinterpret_cast<const uint32_t *>(pDungeonCels.get());
	const auto *src = reinterpret_cast<const uint8_t *>(&pDungeonCels[SDL_SwapLE32(pFrameTable[levelCelBlock.frame()])]);
	// v1.11: one dispatch on the surface's format; the kernels are templated on the pixel type.
	const auto render = [&](auto *dst) {
		const uint16_t dstPitch = out.pixelPitch();

#ifdef DUN_RENDER_STATS
	++DunRenderStats[DunRenderType { tile, maskType }];
#endif

	switch (maskType) {
	case MaskType::Solid:
		RenderTileDispatch</*Transparent=*/false>(tile, dst, dstPitch, src, tbl, clip);
		break;
	case MaskType::Transparent:
		RenderTileDispatch</*Transparent=*/true>(tile, dst, dstPitch, src, tbl, clip);
		break;
	case MaskType::Left:
		RenderLeftTrapezoidOrTransparentSquareDispatch</*OpaquePrefix=*/false, /*PrefixIncrement=*/2>(tile, dst, dstPitch, src, tbl, clip);
		break;
	case MaskType::Right:
		RenderRightTrapezoidOrTransparentSquareDispatch</*OpaquePrefix=*/true, /*PrefixIncrement=*/-2>(tile, dst, dstPitch, src, tbl, clip);
		break;
	case MaskType::LeftFoliage:
		RenderTransparentSquareDispatch</*OpaquePrefix=*/true, /*PrefixIncrement=*/2>(dst, dstPitch, src, tbl, clip);
		break;
	case MaskType::RightFoliage:
		RenderTransparentSquareDispatch</*OpaquePrefix=*/false, /*PrefixIncrement=*/-2>(dst, dstPitch, src, tbl, clip);
		break;
	}
	};
	const int dstX = static_cast<int>(position.x + clip.left);
	const int dstY = static_cast<int>(position.y - clip.bottom);
	if (out.isIndexed())
		render(out.at<uint8_t>(dstX, dstY));
	else
		render(out.at<uint32_t>(dstX, dstY));

#ifdef DEBUG_STR
	const std::pair<string_view, UiFlags> debugStr = GetTileDebugStr(tile);
	DrawString(out, debugStr.first, Rectangle { Point { position.x + 2, position.y - 29 }, Size { 28, 28 } }, { debugStr.second });
#endif
}

void world_draw_black_tile(const Surface &out, int sx, int sy)
{
#ifdef DEBUG_RENDER_OFFSET_X
	sx += DEBUG_RENDER_OFFSET_X;
#endif
#ifdef DEBUG_RENDER_OFFSET_Y
	sy += DEBUG_RENDER_OFFSET_Y;
#endif
	auto clip = CalculateClip(sx, sy, TILE_WIDTH, TriangleHeight, out);
	if (clip.width <= 0 || clip.height <= 0)
		return;

	auto clipY = CalculateDiamondClipY(clip);
	const auto render = [&](auto *dst) {
		const uint16_t dstPitch = out.pixelPitch();
		if (clip.width == TILE_WIDTH) {
			if (clip.height == TriangleHeight) {
				RenderBlackTileFull(dst, dstPitch);
			} else {
				RenderBlackTileClipY(dst, dstPitch, clipY);
			}
		} else {
			if (clip.right == 0) {
				RenderBlackTileClipLeftAndVertical(dst, dstPitch, sx, clipY);
			} else {
				RenderBlackTileClipRightAndVertical(dst, dstPitch, clip.width, clipY);
			}
		}
	};
	if (out.isIndexed())
		render(out.at<uint8_t>(sx, static_cast<int>(sy - clip.bottom)));
	else
		render(out.at<uint32_t>(sx, static_cast<int>(sy - clip.bottom)));
}

} // namespace devilution
