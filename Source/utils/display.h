#pragma once

#include <cstdint>
#include <type_traits>

#include <SDL.h>
#ifdef USE_SDL1
#include "utils/sdl2_to_1_2_backports.h"
#else
#include "utils/sdl2_backports.h"
#endif

#include "utils/attributes.h"
#include "utils/sdl_ptrs.h"
#include "utils/ui_fwd.h"

namespace devilution {

extern int refreshDelay; // Screen refresh rate in nanoseconds
extern SDL_Window *window;
extern SDL_Window *ghMainWnd;
extern SDL_Renderer *renderer;
#ifndef USE_SDL1
extern SDLTextureUniquePtr texture;
#endif

extern SDLPaletteUniquePtr Palette;
extern SDL_Surface *PalSurface;
extern unsigned int pal_surface_palette_version;
extern DVL_API_FOR_TEST Size forceResolution;

/**
 * @brief The widest the game's view may be at this height when Fit to Screen widens it to the desktop.
 *
 * 21:9 and no further (user, 2026-09-11: "expand the viewport all the way up to 21:9. no more"). A
 * wider desktop - 32:9, 48:9 - shows the 21:9 view with black bars left and right.
 */
DVL_API_FOR_TEST int FitToScreenMaxWidth(int height);

#ifdef USE_SDL1
void SetVideoMode(int width, int height, int bpp, uint32_t flags);
void SetVideoModeToPrimary(bool fullscreen, int width, int height);
#endif

/**
 * @brief Sets `gbActive` from what the window actually IS, rather than from the last event about it.
 *
 * `gbActive` gates RenderPresent - false and the frame is computed and thrown away. It is otherwise
 * only ever written from SDL_WINDOWEVENT_HIDDEN/MINIMIZED and SHOWN/EXPOSED/RESTORED, which is fine
 * while nothing but the user's window manager moves the window, and not fine across a resize: SDL
 * emits its own hide/show/minimise traffic there, and if the last of it to be processed says
 * "hidden", rendering stops for good while the game carries on running underneath.
 *
 * Call after anything that resizes, recreates or changes the mode of the window. A no-op under SDL1,
 * which has no per-window state to ask.
 */
void SyncWindowActiveState();

bool IsFullScreen();

// Returns:
// SDL1: Video surface.
// SDL2, no upscale: Window surface.
// SDL2, upscale: Renderer texture surface.
SDL_Surface *GetOutputSurface();

// Whether the output surface requires software scaling.
// Always returns false on SDL2.
bool OutputRequiresScaling();

// Scales rect if necessary.
void ScaleOutputRect(SDL_Rect *rect);

// If the output requires software scaling, replaces the given surface with a scaled one.
SDLSurfaceUniquePtr ScaleSurfaceToOutput(SDLSurfaceUniquePtr surface);

// Convert from output coordinates to logical (resolution-independent) coordinates.
template <
    typename T,
    typename = typename std::enable_if<std::is_arithmetic<T>::value, T>::type>
void OutputToLogical(T *x, T *y)
{
#ifndef USE_SDL1
	if (!renderer)
		return;

	float scaleX;
	SDL_RenderGetScale(renderer, &scaleX, NULL);
	float scaleDpi = GetDpiScalingFactor();
	float scale = scaleX / scaleDpi;
	*x = static_cast<T>(*x / scale);
	*y = static_cast<T>(*y / scale);

	SDL_Rect view;
	SDL_RenderGetViewport(renderer, &view);
	*x -= view.x;
	*y -= view.y;
#else
	if (!OutputRequiresScaling())
		return;
	const SDL_Surface *surface = GetOutputSurface();
	*x = *x * gnScreenWidth / surface->w;
	*y = *y * gnScreenHeight / surface->h;
#endif
}

template <
    typename T,
    typename = typename std::enable_if<std::is_arithmetic<T>::value, T>::type>
void LogicalToOutput(T *x, T *y)
{
#ifndef USE_SDL1
	if (!renderer)
		return;
	SDL_Rect view;
	SDL_RenderGetViewport(renderer, &view);
	*x += view.x;
	*y += view.y;

	float scaleX;
	SDL_RenderGetScale(renderer, &scaleX, NULL);
	float scaleDpi = GetDpiScalingFactor();
	float scale = scaleX / scaleDpi;
	*x = static_cast<T>(*x * scale);
	*y = static_cast<T>(*y * scale);
#else
	if (!OutputRequiresScaling())
		return;
	const SDL_Surface *surface = GetOutputSurface();
	*x = *x * surface->w / gnScreenWidth;
	*y = *y * surface->h / gnScreenHeight;
#endif
}

} // namespace devilution
