#include "oracool/ui_backgrounds.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>

#include <SDL.h>

#include "DiabloUI/diabloui.h" // UiLoadDefaultPalette - the front end's one palette
#include "engine/palette.h"
#include "engine/point.hpp"
#include "engine/size.hpp"
#include "engine/surface.hpp"
#include "utils/display.h"
#include "utils/log.hpp"
#include "utils/png.h"
#include "utils/sdl_geometry.h"
#include "utils/surface_to_clx.hpp"
#include "DiabloUI/diabloui.h" // DiabloUiSurface: which format the front end draws into
#include "engine/render/primitive_render.hpp" // PackArgb

namespace devilution::oracool {

namespace {

/** @brief One background's asset and its prepared, screen-sized sprite. */
struct BackgroundSlot {
	const char *assetPath;
	/** The prepared, screen-sized sprite. Kept across visits - see the header. */
	OptionalOwnedClxSpriteList sprite;
	/** Renderer stage 2 (v1.11): the same painting as ARGB pixels, built instead of `sprite` on the 32-bit screen. */
	std::vector<uint32_t> argb;
	/** What `sprite` was built for, so a resolution change rebuilds it. */
	Size builtForScreen { 0, 0 };
	/** Ditto for the palette: each screen's own is stable, but a different screen's would invalidate this. */
	std::array<SDL_Color, 256> builtForPalette {};
	/** Set once the asset has been looked for, so a missing file is not re-opened on every visit. */
	bool loadFailed = false;
};

// Indexed by UiBackground; the static_assert below keeps the two in step.
//
// Settings and HeroSelect USED to share hero_settings_bg.png - one painting, two screens, two
// palettes, which is why they were given separate slots even then. The character screens have their
// own painting now (user-supplied): a cathedral with a stone dais, and the dais is load-bearing -
// selhero positions the character preview on a mark measured off it. See HeroSelectGroundInArt.
BackgroundSlot Slots[] = {
	{ "ui\\main_menu_bg.png" },
	{ "ui\\hero_settings_bg.png" },
	{ "ui\\hero_select_bg.png" },
	{ "ui\\difficulty_bg.png" },
};
static_assert(sizeof(Slots) / sizeof(Slots[0]) == static_cast<size_t>(UiBackground::LAST) + 1,
    "A UiBackground was added without its asset path - the two lists are indexed by each other");

struct SourceImage {
	std::vector<uint8_t> rgba;
	int width = 0;
	int height = 0;
};

bool LoadSource(const char *assetPath, SourceImage &image)
{
	SDL_Surface *png = LoadPNG(assetPath);
	if (png == nullptr) {
		LogWarn("Oracool UI background: {:s} not found - keeping the screen's original background", assetPath);
		return false;
	}

	SDL_Surface *rgba = SDL_ConvertSurfaceFormat(png, SDL_PIXELFORMAT_ABGR8888, 0);
	SDL_FreeSurface(png);
	if (rgba == nullptr) {
		LogWarn("Oracool UI background: pixel format conversion failed for {:s}: {:s}", assetPath, SDL_GetError());
		return false;
	}

	image.width = rgba->w;
	image.height = rgba->h;
	image.rgba.resize(static_cast<size_t>(image.width) * image.height * 4);
	const auto *srcPixels = static_cast<const uint8_t *>(rgba->pixels);
	for (int y = 0; y < image.height; y++) {
		std::memcpy(&image.rgba[static_cast<size_t>(y) * image.width * 4],
		    srcPixels + static_cast<size_t>(y) * rgba->pitch,
		    static_cast<size_t>(image.width) * 4);
	}
	SDL_FreeSurface(rgba);
	return true;
}

/**
 * @brief Nearest index across the WHOLE front-end palette, cached per RGB555 bucket.
 *
 * The whole palette, unlike the in-game art in hud_art.cpp, which deliberately restricts itself to
 * entries 128-255. That restriction exists because the lower half of the *level* palette is
 * recoloured per dungeon type; a front-end screen has one palette that never changes while it is up,
 * so confining this to half of it would throw away half the colours for nothing.
 *
 * The cache is seeded with -1 rather than 0, because here 0 is a legitimate answer - it is the
 * palette's black, and a night scene lands on it constantly.
 */
uint8_t NearestPaletteIndex(uint8_t r, uint8_t g, uint8_t b, std::vector<int16_t> &cache)
{
	const uint16_t key = ((r >> 3) << 10) | ((g >> 3) << 5) | (b >> 3);
	if (cache[key] >= 0)
		return static_cast<uint8_t>(cache[key]);

	int best = 0;
	int bestDist = INT32_MAX;
	for (int i = 0; i < 256; i++) {
		const SDL_Color &c = orig_palette[i];
		const int dr = static_cast<int>(c.r) - r;
		const int dg = static_cast<int>(c.g) - g;
		const int db = static_cast<int>(c.b) - b;
		// Same channel weighting as the HUD's quantizer, so the two passes agree about what "closest"
		// means and the same source colour cannot land on visibly different palette entries.
		const int dist = 2 * dr * dr + 4 * dg * dg + 3 * db * db;
		if (dist < bestDist) {
			bestDist = dist;
			best = i;
		}
	}
	cache[key] = static_cast<int16_t>(best);
	return static_cast<uint8_t>(best);
}

/** @brief Bilinear sample of @p image at fractional (@p fx, @p fy), clamped to @p crop. */
void SampleBilinear(const SourceImage &image, const Rectangle &crop, double fx, double fy,
    uint8_t &r, uint8_t &g, uint8_t &b)
{
	const int x0 = std::clamp(static_cast<int>(std::floor(fx)), crop.position.x, crop.position.x + crop.size.width - 1);
	const int y0 = std::clamp(static_cast<int>(std::floor(fy)), crop.position.y, crop.position.y + crop.size.height - 1);
	const int x1 = std::min(x0 + 1, crop.position.x + crop.size.width - 1);
	const int y1 = std::min(y0 + 1, crop.position.y + crop.size.height - 1);
	const double tx = std::clamp(fx - x0, 0.0, 1.0);
	const double ty = std::clamp(fy - y0, 0.0, 1.0);

	const auto at = [&image](int x, int y, int channel) {
		return static_cast<double>(image.rgba[(static_cast<size_t>(y) * image.width + x) * 4 + channel]);
	};
	for (int channel = 0; channel < 3; channel++) {
		const double top = at(x0, y0, channel) * (1 - tx) + at(x1, y0, channel) * tx;
		const double bottom = at(x0, y1, channel) * (1 - tx) + at(x1, y1, channel) * tx;
		const auto value = static_cast<uint8_t>(std::lround(std::clamp(top * (1 - ty) + bottom * ty, 0.0, 255.0)));
		(channel == 0 ? r : channel == 1 ? g : b) = value;
	}
}

/**
 * @brief The source rect to show, scaled to COVER @p screen and centred.
 *
 * Cover rather than fit, because a background that does not reach the screen edges is letterboxing
 * by another name - which is the thing this replaces.
 */
Rectangle CropForScreen(const SourceImage &image, Size screen)
{
	const double scale = std::max(static_cast<double>(screen.width) / image.width,
	    static_cast<double>(screen.height) / image.height);
	const int cropWidth = std::min(image.width, static_cast<int>(std::lround(screen.width / scale)));
	const int cropHeight = std::min(image.height, static_cast<int>(std::lround(screen.height / scale)));
	return { { (image.width - cropWidth) / 2, (image.height - cropHeight) / 2 }, { cropWidth, cropHeight } };
}

bool Build(BackgroundSlot &slot)
{
	const Size screen { gnScreenWidth, gnScreenHeight };
	const bool trueColour = !Surface(DiabloUiSurface()).isIndexed();
	if ((trueColour ? !slot.argb.empty() : slot.sprite.has_value()) && slot.builtForScreen == screen
	    && std::memcmp(slot.builtForPalette.data(), orig_palette.data(), sizeof(slot.builtForPalette)) == 0) {
		return true;
	}
	if (slot.loadFailed)
		return false;

	SourceImage image;
	if (!LoadSource(slot.assetPath, image)) {
		slot.loadFailed = true;
		return false;
	}

	const Rectangle crop = CropForScreen(image, screen);

	if (trueColour) {
		// Stage 2: the painting itself, resampled to the screen and nothing else - no nearest
		// palette entry, no menu-palette cache key that matters.
		slot.sprite = std::nullopt;
		slot.argb.assign(static_cast<size_t>(screen.width) * screen.height, 0);
		for (int y = 0; y < screen.height; y++) {
			const double sy = crop.position.y + (y + 0.5) * crop.size.height / screen.height - 0.5;
			uint32_t *row = &slot.argb[static_cast<size_t>(y) * screen.width];
			for (int x = 0; x < screen.width; x++) {
				const double sx = crop.position.x + (x + 0.5) * crop.size.width / screen.width - 0.5;
				uint8_t r;
				uint8_t g;
				uint8_t b;
				SampleBilinear(image, crop, sx, sy, r, g, b);
				row[x] = PackArgb(255, r, g, b);
			}
		}
		slot.builtForScreen = screen;
		std::memcpy(slot.builtForPalette.data(), orig_palette.data(), sizeof(slot.builtForPalette));
		return true;
	}

	slot.argb.clear();
	OwnedSurface surface(screen.width, screen.height);
	std::vector<int16_t> cache(1 << 15, -1);

	for (int y = 0; y < screen.height; y++) {
		uint8_t *row = &surface[Point { 0, y }];
		// Pixel centres, hence the +/-0.5. At the resolutions where the crop is 1:1 - every 720-tall
		// mode, which includes the 21:9 entry the art was drawn for - this lands on exact integers
		// and the bilinear filter below degenerates to a straight copy, so those get the painting
		// pixel for pixel rather than a resampled version of it.
		const double sy = crop.position.y + (y + 0.5) * crop.size.height / screen.height - 0.5;
		for (int x = 0; x < screen.width; x++) {
			const double sx = crop.position.x + (x + 0.5) * crop.size.width / screen.width - 0.5;
			uint8_t r;
			uint8_t g;
			uint8_t b;
			SampleBilinear(image, crop, sx, sy, r, g, b);
			row[x] = NearestPaletteIndex(r, g, b, cache);
		}
	}

	// No transparent colour: this is a background, it must be opaque everywhere. Passing one would
	// make the palette's black see-through and punch holes in the night sky.
	slot.sprite = SurfaceToClx(surface);
	slot.builtForScreen = screen;
	std::memcpy(slot.builtForPalette.data(), orig_palette.data(), sizeof(slot.builtForPalette));
	return true;
}

} // namespace

Point MapBackgroundPointToScreen(Size artSize, Point pointInArt)
{
	// The SAME CropForScreen the painting itself goes through, deliberately. A screen that wants to
	// put something on a spot marked in the art has to agree with the art about where that spot ended
	// up, and the only way to guarantee that is to ask the one function rather than to re-derive the
	// arithmetic beside it. Cover-cropping is centred, so this is exact rather than approximate.
	SourceImage shape;
	shape.width = artSize.width;
	shape.height = artSize.height;
	const Size screen { gnScreenWidth, gnScreenHeight };
	const Rectangle crop = CropForScreen(shape, screen);
	return { (pointInArt.x - crop.position.x) * screen.width / crop.size.width,
		(pointInArt.y - crop.position.y) * screen.height / crop.size.height };
}

bool AddUiBackground(std::vector<std::unique_ptr<UiItemBase>> *vecDialog, UiBackground which, bool atFront)
{
	BackgroundSlot &slot = Slots[static_cast<size_t>(which)];

	// Oracool: pin the front end to ui_art\diablo.pal, BEFORE Build quantizes into it.
	//
	// Every caller reaches this having just run LoadBackgroundArt, which adopts the palette of
	// whatever stock .pcx it loaded - and once hellfire.mpq is present, FindMpqFile serves
	// `ui_art\mainmenu.pcx` out of THAT archive, so the whole front end ran on Hellfire's menu
	// palette. Hellfire and Diablo agree on the UI half (gold 176-191, silver 224-239, the glow's
	// amber 193-207 are byte-identical) and disagree on the scene half - and the Diablo logo's
	// FLAMES live down at indices 10-19. Measured against ui_art\logo.pcx's own pixels, 37 of the
	// indices it uses are defined differently there, covering 406,143 pixels. That is what punched
	// the black holes through the fire.
	//
	// Before Build and not after, because Build maps the PNG to nearest indices in the current
	// palette and caches the result against it (BackgroundSlot::builtForPalette); pinning first is
	// also what keeps that cache key stable instead of flapping per screen.
	//
	// This call was here at 1.5.5, blamed for a crash, and reverted at 1.5.6. It was innocent: the
	// crash was IMG_LoadPNG_RW being handed a null RWops by LoadPNG for a missing file (fixed at
	// 1.5.7, see utils/png.h), which is also what LoadSource below would have hit. Restored.
	//
	// NOTE for anyone who skips this function: pinning the palette is the front end's, not this
	// painting's. It only lives here because every screen happened to draw a painting. A screen that
	// does not - selhero with its background hidden, at 1.5.22 - has to call UiLoadDefaultPalette
	// itself, and the symptom of forgetting is the logo's fire going black, not a missing picture.
	UiLoadDefaultPalette();

	if (!Build(slot))
		return false;

	// Anchored at the screen's own origin and not centred, unlike UiAddBackground: that one places a
	// 640x480 plate inside the 640x480 UI rect, which sits inset in the screen. This sprite IS the
	// screen, so both of those adjustments would move it off by the size of the inset.
	std::unique_ptr<UiItemBase> item;
	if (!slot.argb.empty())
		item = std::make_unique<UiImageRgb>(slot.argb.data(), slot.builtForScreen.width, slot.builtForScreen.height, MakeSdlRect(0, 0, 0, 0));
	else
		item = std::make_unique<UiImageClx>((*slot.sprite)[0], MakeSdlRect(0, 0, 0, 0));
	if (atFront)
		vecDialog->insert(vecDialog->begin(), std::move(item));
	else
		vecDialog->push_back(std::move(item));
	return true;
}

} // namespace devilution::oracool
