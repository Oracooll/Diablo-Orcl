/**
 * @file dx.cpp
 *
 * Implementation of functions setting up the graphics pipeline.
 */
#include "engine/dx.h"

#include <SDL.h>
#include <cstdint>

#include "controls/plrctrls.h"
#include "engine.h"
#include "options.h"
#include "utils/display.h"
#include "utils/log.hpp"
#include "utils/sdl_wrap.h"

#ifndef USE_SDL1
#include "controls/touch/renderers.h"
#endif

#ifdef __3DS__
#include <3ds.h>
#endif

namespace devilution {

int refreshDelay;
SDL_Renderer *renderer;
#ifndef USE_SDL1
SDLTextureUniquePtr texture;
#endif

/** Currently active palette */
SDLPaletteUniquePtr Palette;
unsigned int pal_surface_palette_version = 0;

/** 24-bit renderer texture surface */
SDLSurfaceUniquePtr RendererTextureSurface;

/** The surface we render to: 32-bit XRGB8888 since v1.11 (it was 8-bit; the name stays for its 200 callers). */
SDL_Surface *PalSurface;
namespace {
SDLSurfaceUniquePtr PinnedPalSurface;
} // namespace

/** Whether we render directly to the screen surface, i.e. `PalSurface == GetOutputSurface()` */
bool RenderDirectlyToOutputSurface;

namespace {

bool CanRenderDirectlyToOutputSurface()
{
#ifdef USE_SDL1
#ifdef SDL1_FORCE_DIRECT_RENDER
	return true;
#else
	auto *outputSurface = GetOutputSurface();
	return ((outputSurface->flags & SDL_DOUBLEBUF) == SDL_DOUBLEBUF
	    && outputSurface->w == gnScreenWidth && outputSurface->h == gnScreenHeight
	    && outputSurface->format->BitsPerPixel == 8);
#endif
#else // !USE_SDL1
	return false;
#endif
}

/**
 * @brief Limit FPS to avoid high CPU load, use when v-sync isn't available
 */
void LimitFrameRate()
{
	if (*sgOptions.Graphics.frameRateControl != FrameRateControl::CPUSleep)
		return;
	static uint32_t frameDeadline;
	uint32_t tc = SDL_GetTicks() * 1000;
	uint32_t v = 0;
	if (frameDeadline > tc) {
		v = tc % refreshDelay;
		SDL_Delay(v / 1000 + 1); // ceil
	}
	frameDeadline = tc + v + refreshDelay;
}

} // namespace

void dx_init()
{
#ifndef USE_SDL1
	SDL_RaiseWindow(ghMainWnd);
	SDL_ShowWindow(ghMainWnd);
#endif

	palette_init();
	CreateBackBuffer();
	pal_surface_palette_version = 1;
}

Surface GlobalBackBuffer()
{
	return Surface(PalSurface, SDL_Rect { 0, 0, gnScreenWidth, gnScreenHeight });
}

void dx_cleanup()
{
#ifndef USE_SDL1
	if (ghMainWnd != nullptr)
		SDL_HideWindow(ghMainWnd);
#endif

	PalSurface = nullptr;
	PinnedPalSurface = nullptr;
	Palette = nullptr;
	RendererTextureSurface = nullptr;
#ifndef USE_SDL1
	texture = nullptr;
	if (*sgOptions.Graphics.upscale)
		SDL_DestroyRenderer(renderer);
#endif
	SDL_DestroyWindow(ghMainWnd);
}

void CreateBackBuffer()
{
	if (CanRenderDirectlyToOutputSurface()) {
		Log("{}", "Will render directly to the SDL output surface");
		PalSurface = GetOutputSurface();
		RenderDirectlyToOutputSurface = true;
	} else {
		// Oracool, the 32-bit compositing renderer (v1.11): the back buffer holds COLOURS. Every
		// kernel resolves its indices through PaletteRGB as it draws, so no palette is bound here.
		PinnedPalSurface = SDLWrap::CreateRGBSurfaceWithFormat(
		    /*flags=*/0,
		    /*width=*/gnScreenWidth,
		    /*height=*/gnScreenHeight,
		    /*depth=*/32,
		    SDL_PIXELFORMAT_RGB888);
		PalSurface = PinnedPalSurface.get();
	}

#ifndef USE_SDL1
	// No surface palette: the buffer is 32-bit. The global `Palette` object is still kept current
	// by palette_update for the hardware cursor and the 8-bit offscreen surfaces.
#else
	// In SDL1, `PalSurface` owns its palette and we must update it every
	// time the global `palette` is changed. No need to do anything here as
	// the global `palette` doesn't have any colors set yet.
#endif
}

void InitPalette()
{
	Palette = SDLWrap::AllocPalette();
}

void BltFast(SDL_Rect *srcRect, SDL_Rect *dstRect)
{
	if (RenderDirectlyToOutputSurface)
		return;
	Blit(PalSurface, srcRect, dstRect);
}

namespace {

SDLSurfaceUniquePtr PresentScratch;

/**
 * @brief The present-time transforms (v1.11): a fade scales the whole frame, the screenshot flash
 * drops green and blue. Applied into a scratch copy of the same format, which is then blitted, so
 * the back buffer itself keeps the unfaded frame for the next redraw.
 */
SDL_Surface *ApplyPresentTransforms(SDL_Surface *src)
{
	if (FadeLevel == 256 && !PresentRedFlash)
		return src;
	if (PresentScratch == nullptr || PresentScratch->w != src->w || PresentScratch->h != src->h)
		PresentScratch = SDLWrap::CreateRGBSurfaceWithFormat(0, src->w, src->h, 32, src->format->format);
	const uint32_t fade = static_cast<uint32_t>(std::clamp(FadeLevel, 0, 256));
	const uint32_t gbMask = PresentRedFlash ? 0x00FF0000u : 0x00FFFFFFu;
	for (int y = 0; y < src->h; y++) {
		const auto *s = reinterpret_cast<const uint32_t *>(static_cast<const uint8_t *>(src->pixels) + y * src->pitch);
		auto *d = reinterpret_cast<uint32_t *>(static_cast<uint8_t *>(PresentScratch->pixels) + y * PresentScratch->pitch);
		for (int x = 0; x < src->w; x++) {
			const uint32_t c = s[x] & gbMask;
			if (fade == 256) {
				d[x] = c;
			} else {
				const uint32_t r = ((c >> 16) & 0xFF) * fade / 256;
				const uint32_t g = ((c >> 8) & 0xFF) * fade / 256;
				const uint32_t b = (c & 0xFF) * fade / 256;
				d[x] = (r << 16) | (g << 8) | b;
			}
		}
	}
	return PresentScratch.get();
}

} // namespace

void Blit(SDL_Surface *src, SDL_Rect *srcRect, SDL_Rect *dstRect)
{
	if (HeadlessMode)
		return;

	SDL_Surface *dst = GetOutputSurface();
#ifndef USE_SDL1
	if (SDL_BlitSurface(ApplyPresentTransforms(src), srcRect, dst, dstRect) < 0)
		ErrSdl();
#else
	if (!OutputRequiresScaling()) {
		if (SDL_BlitSurface(src, srcRect, dst, dstRect) < 0)
			ErrSdl();
		return;
	}

	SDL_Rect scaledDstRect;
	if (dstRect != NULL) {
		scaledDstRect = *dstRect;
		ScaleOutputRect(&scaledDstRect);
		dstRect = &scaledDstRect;
	}

	// Same pixel format: We can call BlitScaled directly.
	if (SDLBackport_PixelFormatFormatEq(src->format, dst->format)) {
		if (SDL_BlitScaled(src, srcRect, dst, dstRect) < 0)
			ErrSdl();
		return;
	}

	// If the surface has a color key, we must stretch first and can then call BlitSurface.
	if (SDL_HasColorKey(src)) {
		SDLSurfaceUniquePtr stretched = SDLWrap::CreateRGBSurface(SDL_SWSURFACE, dstRect->w, dstRect->h, src->format->BitsPerPixel,
		    src->format->Rmask, src->format->Gmask, src->format->BitsPerPixel, src->format->Amask);
		SDL_SetColorKey(stretched.get(), SDL_SRCCOLORKEY, src->format->colorkey);
		if (src->format->palette != NULL)
			SDL_SetPalette(stretched.get(), SDL_LOGPAL, src->format->palette->colors, 0, src->format->palette->ncolors);
		SDL_Rect stretched_rect = { 0, 0, dstRect->w, dstRect->h };
		if (SDL_SoftStretch(src, srcRect, stretched.get(), &stretched_rect) < 0
		    || SDL_BlitSurface(stretched.get(), &stretched_rect, dst, dstRect) < 0) {
			ErrSdl();
		}
		return;
	}

	// A surface with a non-output pixel format but without a color key needs scaling.
	// We can convert the format and then call BlitScaled.
	SDLSurfaceUniquePtr converted = SDLWrap::ConvertSurface(src, dst->format, 0);
	if (SDL_BlitScaled(converted.get(), srcRect, dst, dstRect) < 0)
		ErrSdl();
#endif
}

void RenderPresent()
{
	if (HeadlessMode)
		return;

	SDL_Surface *surface = GetOutputSurface();

	// Self-healing, and this is the one that actually fixes the reported freeze. Syncing once at the
	// end of ResizeWindow is not enough on its own: the hide/show traffic a resize generates is still
	// sitting in the event queue at that point, and a HIDDEN dequeued afterwards puts the flag back.
	// Asked here, the window gets to correct the record on the very next frame, so the worst case is
	// one skipped frame rather than a picture that never updates again.
	if (!gbActive)
		SyncWindowActiveState();

	if (!gbActive) {
		LimitFrameRate();
		return;
	}

#ifndef USE_SDL1
	if (renderer != nullptr) {
		if (SDL_UpdateTexture(texture.get(), nullptr, surface->pixels, surface->pitch) <= -1) { // pitch is 2560
			ErrSdl();
		}

		// Clear buffer to avoid artifacts in case the window was resized
		if (SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255) <= -1) { // TODO only do this if window was resized
			ErrSdl();
		}

		if (SDL_RenderClear(renderer) <= -1) {
			ErrSdl();
		}
		if (SDL_RenderCopy(renderer, texture.get(), nullptr, nullptr) <= -1) {
			ErrSdl();
		}
		if (ControlMode == ControlTypes::VirtualGamepad) {
			RenderVirtualGamepad(renderer);
		}
		SDL_RenderPresent(renderer);

		if (*sgOptions.Graphics.frameRateControl != FrameRateControl::VerticalSync) {
			LimitFrameRate();
		}
	} else {
		if (ControlMode == ControlTypes::VirtualGamepad) {
			RenderVirtualGamepad(surface);
		}
		if (SDL_UpdateWindowSurface(ghMainWnd) <= -1) {
			ErrSdl();
		}
		LimitFrameRate();
	}
#else
	if (SDL_Flip(surface) <= -1) {
		ErrSdl();
	}
	if (RenderDirectlyToOutputSurface)
		PalSurface = GetOutputSurface();
	LimitFrameRate();
#endif
}

void PaletteGetEntries(int dwNumEntries, SDL_Color *lpEntries)
{
	for (int i = 0; i < dwNumEntries; i++) {
		lpEntries[i] = system_palette[i];
	}
}
} // namespace devilution
