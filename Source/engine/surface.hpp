#pragma once

#include <cstddef>
#include <cstdint>

#include <SDL_version.h>

#if SDL_VERSION_ATLEAST(2, 0, 0)
#include <SDL_rect.h>
#include <SDL_surface.h>
#else
#include "utils/sdl2_to_1_2_backports.h"
#include <SDL_video.h>
#endif

#include "engine/point.hpp"
#include "utils/sdl_geometry.h"
#include "utils/sdl_wrap.h"

namespace devilution {

/**
 * @brief A drawing surface: 8-bit palette-indexed, or 32-bit XRGB8888.
 *
 * Oracool, the 32-bit compositing renderer (v1.11, stage 1). The SCREEN is 32-bit: every drawing
 * kernel resolves a palette index to a colour through PaletteRGB as it writes, so the frame holds
 * colours rather than indices. OFFSCREEN surfaces (OwnedSurface by default, the sprite work, the
 * golden tests) stay 8-bit and keep writing indices. A kernel asks bytesPerPixel() once and takes
 * the typed path; the byte-pointer accessors below stay for the 8-bit callers and for whole-row
 * copies, where `pitch()` is in bytes and a pixel's first byte is at `x * bytesPerPixel()`.
 */
struct Surface {
	SDL_Surface *surface;
	SDL_Rect region;

	Surface()
	    : surface(nullptr)
	    , region(SDL_Rect { 0, 0, 0, 0 })
	{
	}

	explicit Surface(SDL_Surface *surface)
	    : surface(surface)
	    , region(MakeSdlRect(0, 0, surface->w, surface->h))
	{
	}

	Surface(SDL_Surface *surface, SDL_Rect region)
	    : surface(surface)
	    , region(region)
	{
	}

	Surface(const Surface &other) = default;
	Surface &operator=(const Surface &other) = default;

	int w() const
	{
		return region.w;
	}
	int h() const
	{
		return region.h;
	}

	/** @brief 1 for an 8-bit indexed surface, 4 for the 32-bit screen. */
	[[nodiscard]] int bytesPerPixel() const
	{
		return surface->format->BytesPerPixel;
	}

	/** @brief Whether this surface holds palette indices (true) or colours (false). */
	[[nodiscard]] bool isIndexed() const
	{
		return bytesPerPixel() == 1;
	}

	/**
	 * @brief The index at @p p of an 8-bit surface. Only meaningful on an indexed surface; the
	 * 32-bit kernels go through at<Pixel>() instead.
	 */
	std::uint8_t &operator[](Point p) const
	{
		return *at(p.x, p.y);
	}

	/** @brief The first BYTE of the pixel at (x, y), whatever the format. */
	std::uint8_t *at(int x, int y) const
	{
		return static_cast<uint8_t *>(surface->pixels)
		    + (region.x + x) * surface->format->BytesPerPixel
		    + surface->pitch * (region.y + y);
	}

	/** @brief The pixel at (x, y) as its own type: uint8_t on an indexed surface, uint32_t on the screen. */
	template <typename Pixel>
	Pixel *at(int x, int y) const
	{
		return reinterpret_cast<Pixel *>(at(x, y));
	}

	template <typename Pixel>
	Pixel *at(Point p) const
	{
		return at<Pixel>(p.x, p.y);
	}

	std::uint8_t *begin() const
	{
		return at(0, 0);
	}
	std::uint8_t *end() const
	{
		return at(0, region.h);
	}

	/**
	 * @brief Set the value of a single pixel if it is in bounds.
	 * @param position Target buffer coordinate
	 * @param col Color index from current palette - resolved to a colour on a 32-bit surface.
	 */
	void SetPixel(Point position, std::uint8_t col) const
	{
		if (InBounds(position))
			SetPixelUnchecked(position, col);
	}

	/** @brief SetPixel without the bounds test. Defined in surface.cpp (it needs the palette). */
	void SetPixelUnchecked(Point position, std::uint8_t col) const;

	/**
	 * @brief Line width of the raw underlying byte buffer, in BYTES.
	 * May be wider than its logical width (for power-of-2 alignment).
	 */
	[[nodiscard]] uint16_t pitch() const
	{
		return surface->pitch;
	}

	/** @brief Line width in PIXELS - the stride a typed pointer steps by from one row to the next. */
	[[nodiscard]] uint16_t pixelPitch() const
	{
		return static_cast<uint16_t>(surface->pitch / surface->format->BytesPerPixel);
	}

	bool InBounds(Point position) const
	{
		return position.x >= 0 && position.y >= 0 && position.x < region.w && position.y < region.h;
	}

	/**
	 * @brief Returns a subregion of the given buffer.
	 */
	Surface subregion(int x, int y, int w, int h) const
	{
		return Surface(surface, MakeSdlRect(region.x + x, region.y + y, w, h));
	}

	/**
	 * @brief Returns a buffer that starts at `y` of height `h`.
	 */
	Surface subregionY(int y, int h) const
	{
		SDL_Rect subregion = region;
		subregion.y += static_cast<decltype(SDL_Rect {}.y)>(y);
		subregion.h = static_cast<decltype(SDL_Rect {}.h)>(h);
		return Surface(surface, subregion);
	}

	/**
	 * @brief Clips srcRect and targetPosition to this output buffer.
	 */
	void Clip(SDL_Rect *srcRect, Point *targetPosition) const
	{
		if (targetPosition->x < 0) {
			srcRect->x -= targetPosition->x;
			srcRect->w += targetPosition->x;
			targetPosition->x = 0;
		}
		if (targetPosition->y < 0) {
			srcRect->y -= targetPosition->y;
			srcRect->h += targetPosition->y;
			targetPosition->y = 0;
		}
		if (targetPosition->x + srcRect->w > region.w) {
			srcRect->w = region.w - targetPosition->x;
		}
		if (targetPosition->y + srcRect->h > region.h) {
			srcRect->h = region.h - targetPosition->y;
		}
	}

	/**
	 * @brief Copies the `srcRect` portion of the given buffer to this buffer at `targetPosition`.
	 * An 8-bit source into a 32-bit destination is resolved through the palette.
	 */
	void BlitFrom(const Surface &src, SDL_Rect srcRect, Point targetPosition) const;

	/**
	 * @brief Copies the `srcRect` portion of the given buffer to this buffer at `targetPosition`.
	 * Source pixels with index 0 are not copied.
	 */
	void BlitFromSkipColorIndexZero(const Surface &src, SDL_Rect srcRect, Point targetPosition) const;
};

class OwnedSurface : public Surface {
	SDLSurfaceUniquePtr pinnedSurface;

public:
	explicit OwnedSurface(SDLSurfaceUniquePtr surface)
	    : Surface(surface.get())
	    , pinnedSurface(std::move(surface))
	{
	}

	/** @brief An 8-bit indexed surface - the offscreen default: sprite work and tests write indices. */
	OwnedSurface(int width, int height)
	    : OwnedSurface(SDLWrap::CreateRGBSurfaceWithFormat(0, width, height, 8, SDL_PIXELFORMAT_INDEX8))
	{
	}

	explicit OwnedSurface(Size size)
	    : OwnedSurface(size.width, size.height)
	{
	}

	/** @brief A 32-bit XRGB8888 surface, the screen's own format - for the compositing tests. */
	static OwnedSurface Rgb(int width, int height)
	{
		return OwnedSurface(SDLWrap::CreateRGBSurfaceWithFormat(0, width, height, 32, SDL_PIXELFORMAT_RGB888));
	}
};

} // namespace devilution
