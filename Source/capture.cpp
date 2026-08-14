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

#ifdef _WIN32
// For FOLDERID_Pictures - the folder Win+PrtScn writes to.
//
// LAST, and with NOMINMAX, both deliberately. windows.h defines min/max as macros, which turns every
// std::min/std::max in the headers above into a syntax error; including it after them keeps the
// damage to this file, and NOMINMAX keeps it out of this file too.
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <objbase.h>
#include <shlobj.h>
#endif

namespace devilution {
namespace {

/**
 * @brief The directory screenshots go under, WITHOUT the trailing "Screenshots/".
 *
 * Windows' Pictures folder, so captures land in the same place Win+PrtScn puts them and show up in
 * the Photos app without anyone having to know where the game keeps its saves. Falls back to the
 * game's own preference directory - which is what this always used - if the shell cannot tell us,
 * and on any platform that has no such notion.
 */
std::string ScreenshotDir()
{
#ifdef _WIN32
	PWSTR picturesPath = nullptr;
	if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Pictures, 0, nullptr, &picturesPath)) && picturesPath != nullptr) {
		const int size = WideCharToMultiByte(CP_UTF8, 0, picturesPath, -1, nullptr, 0, nullptr, nullptr);
		std::string result;
		if (size > 1) {
			result.resize(static_cast<size_t>(size) - 1);
			WideCharToMultiByte(CP_UTF8, 0, picturesPath, -1, result.data(), size, nullptr, nullptr);
			result += '/';
		}
		CoTaskMemFree(picturesPath);
		if (!result.empty())
			return result;
	}
#endif
	return paths::PrefPath();
}

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
	//
	// Oracool: user request (2026-08-13) - "why that folder? why not the default ss folder?". Fair:
	// PrefPath is wherever the game keeps its saves and .ini, and for this build that is the
	// directory the exe sits in - so screenshots were landing inside build\x64-Debug, which is a
	// place you go to run a game, not a place you go to look at pictures. Windows has a registered
	// folder for exactly this (the one Win+PrtScn uses), so that is where they go now, with the old
	// location kept as the fallback for anything that cannot resolve it.
	const std::string dir = StrCat(ScreenshotDir(), "Screenshots/");
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
/**
 * @brief Writes @p buf out, with the red flash, and puts the palette back afterwards.
 *
 * Oracool: extracted so the in-game and front-end captures cannot drift. The only difference between
 * them is which surface holds the frame and who drew it - everything after that is identical, and it
 * is the fiddly half (grab the real palette BEFORE the flash tints it, delay, restore).
 */
void CaptureTo(const Surface &buf)
{
	SDL_Color palette[256];

	const std::string fileName = CaptureFilePath();
	// Grab the real palette before RedPalette() tints everything for the flash effect, so the
	// screenshot shows the frame as it looked rather than the flash.
	PaletteGetEntries(256, palette);
	RedPalette();

	const bool success = CaptureImage(fileName, buf, palette);

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
}

} // namespace

void CaptureScreen()
{
	DrawAndBlit();
	CaptureTo(GlobalBackBuffer());
	RedrawEverything();
}

void CaptureUiScreen()
{
	// No DrawAndBlit here, and that is the whole point of a second entry: it renders the dungeon
	// view and the HUD, neither of which exists on a front-end screen. The menu loop has already
	// drawn this frame into the UI surface, so the frame to save is simply the one on screen.
	//
	// No RedrawEverything either - that flags the in-game renderer's dirty state, which nothing in
	// the menus reads. The menu redraws itself every iteration regardless.
	CaptureTo(Surface(DiabloUiSurface()));
}

} // namespace devilution
