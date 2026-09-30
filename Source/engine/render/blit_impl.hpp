#pragma once

#include <algorithm>
#include <cstdint>
#include <cstring>

#ifdef __has_include
#if __has_include(<version>)
#include <version>
#endif

#if __cpp_lib_execution >= 201902L
#include <execution>
#endif
#endif

#include "engine/palette.h"
#include "utils/attributes.h"

namespace devilution {

#if __cpp_lib_execution >= 201902L
#define DEVILUTIONX_BLIT_EXECUTION_POLICY std::execution::unseq,
#else
#define DEVILUTIONX_BLIT_EXECUTION_POLICY
#endif

/*
 * Oracool, the 32-bit compositing renderer (v1.11, stage 1).
 *
 * Every blitter below exists twice: for a `uint8_t *dst` it writes palette INDICES, exactly as
 * vanilla did, and for a `uint32_t *dst` it writes COLOURS, resolving each index through PaletteRGB
 * as it goes. The drawing kernels are templated on the destination pixel type and pick the overload
 * by the pointer they hold, so the 8-bit offscreen surfaces and the 32-bit screen share one body.
 *
 * A blend on an 8-bit surface is vanilla's lookup, `paletteTransparencyLookup[a][b]`, which is the
 * NEAREST INDEX to the average of two colours. On a 32-bit surface it is the average itself,
 * exact per channel - the one place the 32-bit picture differs from the 8-bit one, and for the
 * better.
 */

/** @brief The exact per-channel average of two XRGB8888 colours, rounding down. */
DVL_ALWAYS_INLINE DVL_ATTRIBUTE_HOT uint32_t AverageRgb(uint32_t a, uint32_t b)
{
	return (((a ^ b) & 0xFEFEFEFEu) >> 1) + (a & b);
}

// ---------------------------------------------------------------- fill and copy, unmapped

DVL_ALWAYS_INLINE DVL_ATTRIBUTE_HOT void BlitFillDirect(uint8_t *dst, unsigned length, uint8_t color)
{
	DVL_ASSUME(length != 0);
	std::memset(dst, color, length);
}

DVL_ALWAYS_INLINE DVL_ATTRIBUTE_HOT void BlitFillDirect(uint32_t *dst, unsigned length, uint8_t color)
{
	DVL_ASSUME(length != 0);
	std::fill_n(dst, length, PaletteRGB[color]);
}

DVL_ALWAYS_INLINE DVL_ATTRIBUTE_HOT void BlitPixelsDirect(uint8_t *DVL_RESTRICT dst, const uint8_t *DVL_RESTRICT src, unsigned length)
{
	DVL_ASSUME(length != 0);
	std::memcpy(dst, src, length);
}

DVL_ALWAYS_INLINE DVL_ATTRIBUTE_HOT void BlitPixelsDirect(uint32_t *DVL_RESTRICT dst, const uint8_t *DVL_RESTRICT src, unsigned length)
{
	DVL_ASSUME(length != 0);
	std::transform(DEVILUTIONX_BLIT_EXECUTION_POLICY src, src + length, dst, [pal = PaletteRGB.data()](uint8_t srcColor) { return pal[srcColor]; });
}

struct BlitDirect {
	template <typename Pixel>
	DVL_ALWAYS_INLINE DVL_ATTRIBUTE_HOT void operator()(unsigned length, Pixel *DVL_RESTRICT dst, const uint8_t *DVL_RESTRICT src) const
	{
		BlitPixelsDirect(dst, src, length);
	}
	template <typename Pixel>
	DVL_ALWAYS_INLINE DVL_ATTRIBUTE_HOT void operator()(unsigned length, uint8_t color, Pixel *DVL_RESTRICT dst) const
	{
		BlitFillDirect(dst, length, color);
	}
};

// ---------------------------------------------------------------- through a colour map (a TRN or a light table)

template <typename Pixel>
DVL_ALWAYS_INLINE DVL_ATTRIBUTE_HOT void BlitFillWithMap(Pixel *dst, unsigned length, uint8_t color, const uint8_t *DVL_RESTRICT colorMap)
{
	BlitFillDirect(dst, length, colorMap[color]);
}

DVL_ALWAYS_INLINE DVL_ATTRIBUTE_HOT void BlitPixelsWithMap(uint8_t *DVL_RESTRICT dst, const uint8_t *DVL_RESTRICT src, unsigned length, const uint8_t *DVL_RESTRICT colorMap)
{
	DVL_ASSUME(length != 0);
	std::transform(DEVILUTIONX_BLIT_EXECUTION_POLICY src, src + length, dst, [colorMap](uint8_t srcColor) { return colorMap[srcColor]; });
}

DVL_ALWAYS_INLINE DVL_ATTRIBUTE_HOT void BlitPixelsWithMap(uint32_t *DVL_RESTRICT dst, const uint8_t *DVL_RESTRICT src, unsigned length, const uint8_t *DVL_RESTRICT colorMap)
{
	DVL_ASSUME(length != 0);
	std::transform(DEVILUTIONX_BLIT_EXECUTION_POLICY src, src + length, dst, [colorMap, pal = PaletteRGB.data()](uint8_t srcColor) { return pal[colorMap[srcColor]]; });
}

struct BlitWithMap {
	const uint8_t *DVL_RESTRICT colorMap;

	template <typename Pixel>
	DVL_ALWAYS_INLINE DVL_ATTRIBUTE_HOT void operator()(unsigned length, Pixel *DVL_RESTRICT dst, const uint8_t *DVL_RESTRICT src) const
	{
		BlitPixelsWithMap(dst, src, length, colorMap);
	}
	template <typename Pixel>
	DVL_ALWAYS_INLINE DVL_ATTRIBUTE_HOT void operator()(unsigned length, uint8_t color, Pixel *DVL_RESTRICT dst) const
	{
		BlitFillWithMap(dst, length, color, colorMap);
	}
};

// ---------------------------------------------------------------- through an RGB map (a text colour as values, v1.11 stage 3)

DVL_ALWAYS_INLINE DVL_ATTRIBUTE_HOT void BlitPixelsWithRgbMap(uint32_t *DVL_RESTRICT dst, const uint8_t *DVL_RESTRICT src, unsigned length, const uint32_t *DVL_RESTRICT rgbMap)
{
	DVL_ASSUME(length != 0);
	std::transform(DEVILUTIONX_BLIT_EXECUTION_POLICY src, src + length, dst, [rgbMap](uint8_t srcColor) { return rgbMap[srcColor]; });
}

/** @brief An 8-bit target has no way to hold a colour value; the caller keeps its TRN for those. */
DVL_ALWAYS_INLINE void BlitPixelsWithRgbMap(uint8_t * /*dst*/, const uint8_t * /*src*/, unsigned /*length*/, const uint32_t * /*rgbMap*/)
{
}

/**
 * @brief Renderer stage 3 (v1.11): every source index goes to the colour VALUE the map holds for
 * it. Text drawn this way has no palette between the glyph and the screen, so a text colour can be
 * any RGB the map says - the .trn was a detour through the 256 entries and is no longer needed.
 */
struct BlitWithRgbMap {
	const uint32_t *DVL_RESTRICT rgbMap;

	template <typename Pixel>
	DVL_ALWAYS_INLINE DVL_ATTRIBUTE_HOT void operator()(unsigned length, Pixel *DVL_RESTRICT dst, const uint8_t *DVL_RESTRICT src) const
	{
		BlitPixelsWithRgbMap(dst, src, length, rgbMap);
	}
	DVL_ALWAYS_INLINE DVL_ATTRIBUTE_HOT void operator()(unsigned length, uint8_t color, uint32_t *DVL_RESTRICT dst) const
	{
		// NOT BlitFillDirect: its 32-bit overload takes an INDEX and looks the palette up, and a
		// colour value truncated to a byte is how every wide letter's solid run came out blue
		// (user screenshots, 2026-09-07).
		std::fill_n(dst, length, rgbMap[color]);
	}
	DVL_ALWAYS_INLINE void operator()(unsigned /*length*/, uint8_t /*color*/, uint8_t * /*dst*/) const
	{
	}
};

/** @brief @p src over @p dst at @p alpha of 256, per channel. */
DVL_ALWAYS_INLINE DVL_ATTRIBUTE_HOT uint32_t MixRgb(uint32_t dst, uint32_t src, uint32_t alpha)
{
	const uint32_t rb = (((src & 0xFF00FFu) * alpha + (dst & 0xFF00FFu) * (256 - alpha)) >> 8) & 0xFF00FFu;
	const uint32_t g = (((src & 0x00FF00u) * alpha + (dst & 0x00FF00u) * (256 - alpha)) >> 8) & 0x00FF00u;
	return rb | g;
}

/**
 * @brief BlitWithRgbMap at a share of full strength: the sprite over what is behind it at @p alpha of 256 (Oracool,
 * dev note 2026-10-01: Frost Nova fading as it spreads). 32-bit targets only, like the map itself.
 */
struct BlitWithRgbMapAlpha {
	const uint32_t *DVL_RESTRICT rgbMap;
	uint32_t alpha;

	DVL_ALWAYS_INLINE DVL_ATTRIBUTE_HOT void operator()(unsigned length, uint32_t *DVL_RESTRICT dst, const uint8_t *DVL_RESTRICT src) const
	{
		for (unsigned i = 0; i < length; i++)
			dst[i] = MixRgb(dst[i], rgbMap[src[i]], alpha);
	}
	DVL_ALWAYS_INLINE void operator()(unsigned /*length*/, uint8_t * /*dst*/, const uint8_t * /*src*/) const
	{
	}
	DVL_ALWAYS_INLINE DVL_ATTRIBUTE_HOT void operator()(unsigned length, uint8_t color, uint32_t *DVL_RESTRICT dst) const
	{
		for (unsigned i = 0; i < length; i++)
			dst[i] = MixRgb(dst[i], rgbMap[color], alpha);
	}
	DVL_ALWAYS_INLINE void operator()(unsigned /*length*/, uint8_t /*color*/, uint8_t * /*dst*/) const
	{
	}
};

// ---------------------------------------------------------------- half-transparent

DVL_ALWAYS_INLINE DVL_ATTRIBUTE_HOT void BlitFillBlended(uint8_t *dst, unsigned length, uint8_t color)
{
	DVL_ASSUME(length != 0);
	std::for_each(DEVILUTIONX_BLIT_EXECUTION_POLICY dst, dst + length, [tbl = paletteTransparencyLookup[color]](uint8_t &dstColor) {
		dstColor = tbl[dstColor];
	});
}

DVL_ALWAYS_INLINE DVL_ATTRIBUTE_HOT void BlitFillBlended(uint32_t *dst, unsigned length, uint8_t color)
{
	DVL_ASSUME(length != 0);
	std::for_each(DEVILUTIONX_BLIT_EXECUTION_POLICY dst, dst + length, [rgb = PaletteRGB[color]](uint32_t &dstColor) {
		dstColor = AverageRgb(dstColor, rgb);
	});
}

DVL_ALWAYS_INLINE DVL_ATTRIBUTE_HOT void BlitPixelsBlended(uint8_t *DVL_RESTRICT dst, const uint8_t *DVL_RESTRICT src, unsigned length)
{
	DVL_ASSUME(length != 0);
	std::transform(DEVILUTIONX_BLIT_EXECUTION_POLICY src, src + length, dst, dst, [pal = paletteTransparencyLookup](uint8_t srcColor, uint8_t dstColor) {
		return pal[srcColor][dstColor];
	});
}

DVL_ALWAYS_INLINE DVL_ATTRIBUTE_HOT void BlitPixelsBlended(uint32_t *DVL_RESTRICT dst, const uint8_t *DVL_RESTRICT src, unsigned length)
{
	DVL_ASSUME(length != 0);
	std::transform(DEVILUTIONX_BLIT_EXECUTION_POLICY src, src + length, dst, dst, [pal = PaletteRGB.data()](uint8_t srcColor, uint32_t dstColor) {
		return AverageRgb(dstColor, pal[srcColor]);
	});
}

struct BlitBlended {
	template <typename Pixel>
	DVL_ALWAYS_INLINE DVL_ATTRIBUTE_HOT void operator()(unsigned length, Pixel *DVL_RESTRICT dst, const uint8_t *DVL_RESTRICT src) const
	{
		BlitPixelsBlended(dst, src, length);
	}
	template <typename Pixel>
	DVL_ALWAYS_INLINE DVL_ATTRIBUTE_HOT void operator()(unsigned length, uint8_t color, Pixel *DVL_RESTRICT dst) const
	{
		BlitFillBlended(dst, length, color);
	}
};

DVL_ALWAYS_INLINE DVL_ATTRIBUTE_HOT void BlitPixelsBlendedWithMap(uint8_t *DVL_RESTRICT dst, const uint8_t *DVL_RESTRICT src, unsigned length, const uint8_t *DVL_RESTRICT colorMap)
{
	DVL_ASSUME(length != 0);
	std::transform(DEVILUTIONX_BLIT_EXECUTION_POLICY src, src + length, dst, dst, [colorMap, pal = paletteTransparencyLookup](uint8_t srcColor, uint8_t dstColor) {
		return pal[dstColor][colorMap[srcColor]];
	});
}

DVL_ALWAYS_INLINE DVL_ATTRIBUTE_HOT void BlitPixelsBlendedWithMap(uint32_t *DVL_RESTRICT dst, const uint8_t *DVL_RESTRICT src, unsigned length, const uint8_t *DVL_RESTRICT colorMap)
{
	DVL_ASSUME(length != 0);
	std::transform(DEVILUTIONX_BLIT_EXECUTION_POLICY src, src + length, dst, dst, [colorMap, pal = PaletteRGB.data()](uint8_t srcColor, uint32_t dstColor) {
		return AverageRgb(dstColor, pal[colorMap[srcColor]]);
	});
}

struct BlitBlendedWithMap {
	const uint8_t *DVL_RESTRICT colorMap;

	template <typename Pixel>
	DVL_ALWAYS_INLINE DVL_ATTRIBUTE_HOT void operator()(unsigned length, Pixel *DVL_RESTRICT dst, const uint8_t *DVL_RESTRICT src) const
	{
		BlitPixelsBlendedWithMap(dst, src, length, colorMap);
	}
	template <typename Pixel>
	DVL_ALWAYS_INLINE DVL_ATTRIBUTE_HOT void operator()(unsigned length, uint8_t color, Pixel *DVL_RESTRICT dst) const
	{
		BlitFillBlended(dst, length, colorMap[color]);
	}
};

} // namespace devilution
