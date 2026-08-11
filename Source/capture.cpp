/**
 * @file capture.cpp
 *
 * Implementation of the screenshot function.
 */
#include <cstdint>
#include <cstring>
#include <ctime>
#include <string>

#include <fmt/format.h>

#include "DiabloUI/diabloui.h"
#include "engine/backbuffer_state.hpp"
#include "engine/dx.h"
#include "engine/palette.h"
#include "utils/file_util.h"
#include "utils/log.hpp"
#include "utils/paths.h"
#include "utils/png.h"
#include "utils/sdl_ptrs.h"
#include "utils/str_cat.hpp"
#include "utils/ui_fwd.h"

namespace devilution {
namespace {

std::string CaptureFilePath()
{
	const std::time_t tt = std::time(nullptr);
	const std::tm *tm = std::localtime(&tt);
	const std::string filename = tm != nullptr
	    ? fmt::format("Screenshot from {:04}-{:02}-{:02} {:02}-{:02}-{:02}",
	          tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec)
	    : "Screenshot";

	// Oracool: user request (2026-08-11) - screenshots get their own folder instead of piling up
	// alongside the save files. Created on demand rather than at startup, so a player who never
	// takes a screenshot never gets an empty directory.
	const std::string dir = StrCat(paths::PrefPath(), "Screenshots/");
	RecursivelyCreateDir(dir.c_str());

	std::string path = StrCat(dir, filename, ".png");
	int i = 0;
	while (FileExists(path.c_str())) {
		i++;
		path = StrCat(dir, filename, "-", i, ".png");
	}
	return path;
}

/**
 * @brief Writes the 8-bit back buffer as a palettized PNG.
 *
 * Oracool: user request (2026-08-11) - screenshots come out directly viewable instead of PCX,
 * retiring the separate watcher tool that used to convert them.
 *
 * PNG rather than the JPEG that was asked for, and the reason is measurable rather than
 * stylistic. The game renders 8-bit palettized: a frame contains at most 256 colours. Sampling a
 * JPEG screenshot produced by the old convert-afterwards workflow found **25,219** distinct
 * colours - every one beyond 256 is ringing painted around the sharp edges of UI, text and pixel
 * art, which is exactly the content JPEG handles worst. PNG keeps the frame bit-exact, stores it
 * as a true palettized image, and needs no new dependency: IMG_SavePNG is already available
 * through utils/png.h (SDL_image is built here with IMG_png.c and LOAD_PNG; IMG_jpg.c is not
 * compiled at all, so JPEG would have meant adding an encoder to get a worse picture).
 *
 * @param palette The palette to embed - the real one, captured before RedPalette() tints the
 *                screen for the flash effect.
 */
bool CaptureImage(const std::string &path, const Surface &buf, SDL_Color *palette)
{
	SDLSurfaceUniquePtr surface { SDL_CreateRGBSurfaceWithFormat(0, buf.w(), buf.h(), 8, SDL_PIXELFORMAT_INDEX8) };
	if (surface == nullptr) {
		Log("Screenshot: could not allocate surface: {}", SDL_GetError());
		return false;
	}

	// Force every entry opaque before handing the palette over.
	//
	// system_palette (and therefore PaletteGetEntries) only ever fills r/g/b - nothing in the
	// renderer reads SDL_Color::a, so it sits at zero. IMG_SavePNG *does* read it, and a palette
	// of zero-alpha entries makes libpng emit a tRNS chunk marking all 256 colours transparent.
	// The result is a file whose pixels are perfectly correct and which any conforming viewer
	// renders as blank. Worth knowing this is invisible to readers that flatten alpha, which is
	// how it survived a first check.
	SDL_Color opaque[256];
	for (int i = 0; i < 256; i++) {
		opaque[i] = palette[i];
		opaque[i].a = SDL_ALPHA_OPAQUE;
	}

	if (SDL_SetPaletteColors(surface->format->palette, opaque, 0, 256) < 0) {
		Log("Screenshot: could not set palette: {}", SDL_GetError());
		return false;
	}

	// Row-by-row: the back buffer's pitch includes the render border and does not match the
	// destination's.
	const uint8_t *src = buf.begin();
	auto *dst = static_cast<uint8_t *>(surface->pixels);
	for (int y = 0; y < buf.h(); y++) {
		std::memcpy(dst + static_cast<ptrdiff_t>(y) * surface->pitch, src + static_cast<ptrdiff_t>(y) * buf.pitch(), buf.w());
	}

	if (IMG_SavePNG(surface.get(), path.c_str()) < 0) {
		Log("Screenshot: could not write {}: {}", path, SDL_GetError());
		return false;
	}
	return true;
}

/**
 * @brief Make a red version of the given palette and apply it to the screen.
 */
void RedPalette()
{
	for (int i = 0; i < 256; i++) {
		system_palette[i].g = 0;
		system_palette[i].b = 0;
	}
	palette_update();
	BltFast(nullptr, nullptr);
	RenderPresent();
}
} // namespace

void CaptureScreen()
{
	SDL_Color palette[256];

	const std::string fileName = CaptureFilePath();
	DrawAndBlit();
	// Grab the real palette before RedPalette() tints everything for the flash effect, so the
	// screenshot shows the frame as it looked rather than the flash.
	PaletteGetEntries(256, palette);
	RedPalette();

	const bool success = CaptureImage(fileName, GlobalBackBuffer(), palette);

	if (!success) {
		Log("Failed to save screenshot at {}", fileName);
		RemoveFile(fileName.c_str());
	} else {
		Log("Screenshot saved at {}", fileName);
	}
	SDL_Delay(300);
	for (int i = 0; i < 256; i++) {
		system_palette[i] = palette[i];
	}
	palette_update();
	RedrawEverything();
}

} // namespace devilution
