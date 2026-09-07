#include "engine/surface.hpp"

#include <cstdint>
#include <cstring>

#include "engine/palette.h"

namespace devilution {

namespace {

/**
 * @brief Row copy from an 8-bit source. The destination is 8-bit (indices copied) or 32-bit
 * (indices resolved through PaletteRGB) - the v1.11 renderer's one rule, applied to a blit.
 */
template <bool SkipColorIndexZero, typename DstPixel>
void SurfaceBlitFromIndexed(const Surface &src, SDL_Rect srcRect, const Surface &dst, Point dstPosition)
{
	const std::uint8_t *srcBuf = src.at(srcRect.x, srcRect.y);
	const auto srcPitch = src.pitch();
	DstPixel *dstBuf = dst.at<DstPixel>(dstPosition);
	const auto dstPitch = dst.pixelPitch();

	for (unsigned h = srcRect.h; h != 0; --h) {
		if constexpr (SkipColorIndexZero) {
			for (unsigned w = srcRect.w; w != 0; --w) {
				if (*srcBuf != 0) {
					if constexpr (sizeof(DstPixel) == 1)
						*dstBuf = *srcBuf;
					else
						*dstBuf = PaletteRGB[*srcBuf];
				}
				++srcBuf, ++dstBuf;
			}
			srcBuf += srcPitch - srcRect.w;
			dstBuf += dstPitch - srcRect.w;
		} else {
			if constexpr (sizeof(DstPixel) == 1) {
				std::memcpy(dstBuf, srcBuf, srcRect.w);
			} else {
				for (unsigned w = 0; w < static_cast<unsigned>(srcRect.w); ++w)
					dstBuf[w] = PaletteRGB[srcBuf[w]];
			}
			srcBuf += srcPitch;
			dstBuf += dstPitch;
		}
	}
}

/** @brief 32-bit to 32-bit: a row copy; the "index zero" skip becomes "the colour index zero resolves to". */
template <bool SkipColorIndexZero>
void SurfaceBlitFromRgb(const Surface &src, SDL_Rect srcRect, const Surface &dst, Point dstPosition)
{
	const std::uint32_t *srcBuf = src.at<uint32_t>(srcRect.x, srcRect.y);
	const auto srcPitch = src.pixelPitch();
	std::uint32_t *dstBuf = dst.at<uint32_t>(dstPosition);
	const auto dstPitch = dst.pixelPitch();
	const uint32_t transparent = PaletteRGB[0];

	for (unsigned h = srcRect.h; h != 0; --h) {
		if constexpr (SkipColorIndexZero) {
			for (unsigned w = 0; w < static_cast<unsigned>(srcRect.w); ++w) {
				if (srcBuf[w] != transparent)
					dstBuf[w] = srcBuf[w];
			}
		} else {
			std::memcpy(dstBuf, srcBuf, static_cast<size_t>(srcRect.w) * sizeof(uint32_t));
		}
		srcBuf += srcPitch;
		dstBuf += dstPitch;
	}
}

template <bool SkipColorIndexZero>
void SurfaceBlit(const Surface &src, SDL_Rect srcRect, const Surface &dst, Point dstPosition)
{
	// We do not use `SDL_BlitSurface` here because the palettes may be different objects
	// and SDL would attempt to map them.

	dst.Clip(&srcRect, &dstPosition);
	if (srcRect.w <= 0 || srcRect.h <= 0)
		return;

	if (src.isIndexed()) {
		if (dst.isIndexed())
			SurfaceBlitFromIndexed<SkipColorIndexZero, uint8_t>(src, srcRect, dst, dstPosition);
		else
			SurfaceBlitFromIndexed<SkipColorIndexZero, uint32_t>(src, srcRect, dst, dstPosition);
	} else {
		// A 32-bit source can only land on a 32-bit destination: there is no way back to an index.
		SurfaceBlitFromRgb<SkipColorIndexZero>(src, srcRect, dst, dstPosition);
	}
}

} // namespace

void Surface::SetPixelUnchecked(Point position, std::uint8_t col) const
{
	if (isIndexed())
		*at<uint8_t>(position) = col;
	else
		*at<uint32_t>(position) = PaletteRGB[col];
}

void Surface::BlitFrom(const Surface &src, SDL_Rect srcRect, Point targetPosition) const
{
	SurfaceBlit</*SkipColorIndexZero=*/false>(src, srcRect, *this, targetPosition);
}

void Surface::BlitFromSkipColorIndexZero(const Surface &src, SDL_Rect srcRect, Point targetPosition) const
{
	SurfaceBlit</*SkipColorIndexZero=*/true>(src, srcRect, *this, targetPosition);
}

} // namespace devilution
