#include "oracool/sprite_import.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include <SDL.h>

#include "engine/load_file.hpp"
#include "engine/point.hpp"
#include "engine/render/clx_render.hpp"
#include "engine/surface.hpp"
#include "utils/endian_write.hpp"
#include "utils/log.hpp"
#include "utils/png.h"
#include "utils/sdl_ptrs.h"
#include "utils/surface_to_clx.hpp"

namespace devilution::oracool {

namespace {

constexpr int Facings = 8;

/**
 * The palette every import is matched against.
 *
 * Any level palette would do for the half that matters. Player sprites live almost entirely in the
 * GLOBAL half (128-255), which is identical across town and all four dungeon tilesets - that is why a
 * character looks the same everywhere. Town's is the one that always exists.
 */
constexpr char LevelPalettePath[] = "levels\\towndata\\town.pal";

/**
 * Index 0 means "no pixel". Nothing else may be quantized to it, and nothing is: matches are
 * restricted to the shared half, so 0 is free to carry transparency unambiguously.
 */
constexpr uint8_t TransparentIndex = 0;
constexpr int SharedHalfFirst = 128;

std::array<uint8_t, 768> LevelPalette;
bool LevelPaletteLoaded = false;
// There was a `bool LevelPaletteMissing` here from the day this file was written, guarding an early
// return, and NOTHING EVER ASSIGNED IT - so the "no palette, degrade gracefully" path it promised
// never existed (2026-09-12 asset sweep).
//
// DELETED, deliberately, rather than made to work - and both attempts at making it work were worse
// than the dead flag:
//
//  1. Setting it as a sticky global. This cache serves TWO palettes now, so one missing front-end
//     palette would have permanently disabled every IN-GAME PNG import as well.
//  2. A per-call `FindAsset(palettePath).ok()` pre-check. That looks obviously right and it is not:
//     FindAsset is STRICTER than the LoadFileInMem it would be guarding, so it rejected a palette
//     the loader can read perfectly well and turned every delivered cold-missile sheet into a
//     no-import. OracoolColdPack.EveryDeliveredSheetLoadsAtItsSpecifiedShape caught it immediately.
//
// The lesson is the second one: an existence pre-check that uses a different lookup than the loader
// it protects is not a guard, it is a second, disagreeing implementation. A missing palette still
// app-fatals inside LoadFileInMem, which is at least loud and honest; making it soft needs the
// LOADER to report failure, not a guess in front of it.
/**
 * @brief Which palette the cache holds, so a caller asking for a different one gets it.
 *
 * A COPY, not the caller's pointer. It held the pointer for about an hour on 2026-09-12 and the
 * first caller to pass a block-scope `constexpr char[]` made it dangle - the next call's compare
 * read a dead stack object. Owning the string means no caller's lifetime can matter, which is the
 * fix that does not depend on every future caller remembering to pass something static.
 */
std::string LoadedPalettePath;

/**
 * @brief Loads @p palettePath into the quantizer's cache, reloading if a different one is cached.
 *
 * The cache used to be keyed on nothing, because there was only ever one palette: every caller was
 * importing IN-GAME art, and the comment above LevelPalettePath explains why any level palette will
 * do for that - the shared half is identical across town and all four tilesets.
 *
 * Then the hero-portrait override started importing FRONT-END art (2026-09-12), and the front end
 * runs on `ui_art\diablo.pal`, which shares only one of its 128 upper entries with town's. Matching
 * against one table and displaying through another is exactly the mistake `ui_backgrounds.cpp`
 * calls UiLoadDefaultPalette before Build to avoid, and this path sat outside that discipline.
 *
 * Measured before fixing, because it matters what the error actually costs: quantizing that
 * portrait against town's shared half, diablo's shared half and diablo's full 256 produces three
 * images that are hard to tell apart - nearest-match finds a close brown either way. So this was a
 * latent trap rather than a visible defect, and the fix is worth making for the next asset whose
 * colours only one of the two palettes carries, not for this one.
 */
bool EnsurePalette(const char *palettePath)
{
	if (LevelPaletteLoaded && LoadedPalettePath == palettePath)
		return true;
	LoadFileInMem(palettePath, LevelPalette);
	LevelPaletteLoaded = true;
	LoadedPalettePath = palettePath;
	return true;
}

/** @brief Nearest entry in the palette's shared half, by the weighting every Oracool quantizer uses. */
uint8_t NearestSharedIndex(int r, int g, int b)
{
	int best = SharedHalfFirst;
	int bestDist = INT32_MAX;
	for (int i = SharedHalfFirst; i < 256; i++) {
		const int dr = static_cast<int>(LevelPalette[i * 3]) - r;
		const int dg = static_cast<int>(LevelPalette[i * 3 + 1]) - g;
		const int db = static_cast<int>(LevelPalette[i * 3 + 2]) - b;
		const int dist = 2 * dr * dr + 4 * dg * dg + 3 * db * db;
		if (dist < bestDist) {
			bestDist = dist;
			best = i;
		}
	}
	return static_cast<uint8_t>(best);
}

/**
 * @brief Glues per-facing lists into one sheet buffer.
 *
 * The format is its own documentation in ClxSpriteSheet: a uint32 offset per list, then the lists.
 * Built by hand because nothing else needs to - every other sheet in the game arrives already in this
 * shape from a .cl2.
 */
OwnedClxSpriteSheet CombineListsIntoSheet(std::vector<OwnedClxSpriteList> &lists)
{
	const size_t headerSize = 4 * lists.size();
	size_t total = headerSize;
	for (const OwnedClxSpriteList &list : lists)
		total += ClxSpriteList(list).dataSize();

	std::unique_ptr<uint8_t[]> data { new uint8_t[total] };
	size_t offset = headerSize;
	for (size_t i = 0; i < lists.size(); i++) {
		const ClxSpriteList list { lists[i] };
		WriteLE32(&data[i * 4], static_cast<uint32_t>(offset));
		std::memcpy(&data[offset], list.data(), list.dataSize());
		offset += list.dataSize();
	}
	return OwnedClxSpriteSheet { std::move(data), static_cast<uint16_t>(lists.size()) };
}

/**
 * @brief One sprite list per ROW of @p surface, quantized into the shared palette half.
 *
 * Split out of SpriteSheetFromSurface on 2026-09-03, when the missile art arrived: a player
 * animation is eight rows and a directional missile is sixteen, so the row count stopped being a
 * constant of this file. Nothing else about the conversion differs between the two, which is the
 * argument for one function rather than two that drift apart.
 *
 * Returns an EMPTY vector when the sheet is not a whole number of @p frameWidth columns by @p rows
 * rows - every caller falls back to whatever it was going to draw anyway, so a bad sheet is a
 * no-import rather than an error.
 */
std::vector<OwnedClxSpriteList> SplitSurfaceIntoRows(SDL_Surface *surface, uint16_t frameWidth, int rows, const char *palettePath = nullptr)
{
	std::vector<OwnedClxSpriteList> lists;
	if (palettePath == nullptr)
		palettePath = LevelPalettePath;
	if (surface == nullptr || frameWidth == 0 || rows <= 0 || !EnsurePalette(palettePath))
		return lists;

	SDLSurfaceUniquePtr rgba { SDL_ConvertSurfaceFormat(surface, SDL_PIXELFORMAT_ABGR8888, 0) };
	if (rgba == nullptr) {
		LogWarn("Oracool sprite import: surface could not be converted to RGBA");
		return lists;
	}

	const int sheetWidth = rgba->w;
	const int sheetHeight = rgba->h;
	if (sheetWidth % frameWidth != 0 || sheetHeight % rows != 0) {
		LogWarn("Oracool sprite import: a {:d}x{:d} sheet is not a whole number of {:d}px columns by "
		        "{:d} rows - ignoring it",
		    sheetWidth, sheetHeight, frameWidth, rows);
		return lists;
	}
	const int frames = sheetWidth / frameWidth;
	const int cellHeight = sheetHeight / rows;
	if (frames == 0 || cellHeight == 0)
		return lists;

	const auto *pixels = static_cast<const uint8_t *>(rgba->pixels);

	lists.reserve(rows);
	for (int row = 0; row < rows; row++) {
		// SurfaceToClx wants the frames stacked VERTICALLY; the sheet has them side by side. This
		// transposes one row of the sheet into that column as it quantizes.
		OwnedSurface column(frameWidth, cellHeight * frames);
		for (int frame = 0; frame < frames; frame++) {
			for (int y = 0; y < cellHeight; y++) {
				uint8_t *dst = &column[Point { 0, frame * cellHeight + y }];
				const uint8_t *src = pixels + static_cast<size_t>(row * cellHeight + y) * rgba->pitch
				    + static_cast<size_t>(frame) * frameWidth * 4;
				for (int x = 0; x < frameWidth; x++) {
					// Binary transparency, like everything else in this renderer: there is no alpha
					// channel to carry a half-transparent pixel into.
					dst[x] = src[x * 4 + 3] < 128
					    ? TransparentIndex
					    : NearestSharedIndex(src[x * 4], src[x * 4 + 1], src[x * 4 + 2]);
				}
			}
		}
		lists.push_back(SurfaceToClx(column, static_cast<unsigned>(frames), TransparentIndex));
	}
	return lists;
}

} // namespace

const char *ClassSpriteFolder(HeroClass heroClass)
{
	// Deliberately NOT PlayersData[].classPath: that returns "warrior" for the Barbarian and "rogue"
	// for the Bard, because those two classes borrow another's CL2s. An import is how a class stops
	// borrowing, so it has to be looked up under its own name.
	switch (heroClass) {
	case HeroClass::Rogue:
		return "rogue";
	case HeroClass::Sorcerer:
		return "sorceror"; // spelled this way in the game's own data
	case HeroClass::Monk:
		return "monk";
	case HeroClass::Bard:
		return "bard";
	case HeroClass::Barbarian:
		return "barbarian";
	case HeroClass::Necromancer:
		return "necromancer";
	default:
		return "warrior";
	}
}

OptionalOwnedClxSpriteSheet LoadPngSpriteSheet(const char *path, uint16_t frameWidth)
{
	SDLSurfaceUniquePtr png { LoadPNG(path) };
	if (png == nullptr)
		return std::nullopt; // no import for this animation; the caller falls back to the CL2
	return SpriteSheetFromSurface(png.get(), frameWidth);
}

OptionalOwnedClxSpriteSheet SpriteSheetFromSurface(SDL_Surface *surface, uint16_t frameWidth)
{
	std::vector<OwnedClxSpriteList> lists = SplitSurfaceIntoRows(surface, frameWidth, Facings);
	if (lists.empty())
		return std::nullopt;
	return CombineListsIntoSheet(lists);
}

std::optional<ColouredSpriteSheet> ColouredSpriteSheetFromSurface(SDL_Surface *surface, uint16_t frameWidth)
{
	if (surface == nullptr || frameWidth == 0 || !EnsurePalette(LevelPalettePath))
		return std::nullopt;
	SDLSurfaceUniquePtr rgba { SDL_ConvertSurfaceFormat(surface, SDL_PIXELFORMAT_ABGR8888, 0) };
	if (rgba == nullptr)
		return std::nullopt;
	const int sheetWidth = rgba->w;
	const int sheetHeight = rgba->h;
	if (sheetWidth % frameWidth != 0 || sheetHeight % Facings != 0) {
		LogWarn("Oracool sprite import: a {:d}x{:d} sheet is not a whole number of {:d}px columns by "
		        "{:d} rows - ignoring it",
		    sheetWidth, sheetHeight, frameWidth, Facings);
		return std::nullopt;
	}
	const int frames = sheetWidth / frameWidth;
	const int cellHeight = sheetHeight / Facings;
	if (frames == 0 || cellHeight == 0)
		return std::nullopt;
	const auto *pixels = static_cast<const uint8_t *>(rgba->pixels);

	// The sheet's own palette: every colour it uses, as long as there are no more than 255 of them
	// (index 0 is "no pixel"). A sheet cut from the game's own art has a few dozen. One with more -
	// a painted or filtered one - is folded by dropping low bits a step at a time until it fits, each
	// surviving entry the average of the colours that fell into it; five bits a channel is 32768
	// cells and no real sheet is still over 255 by then.
	struct Cell {
		uint32_t r = 0, g = 0, b = 0, count = 0;
		uint8_t index = 0;
	};
	std::unordered_map<uint32_t, Cell> cells;
	int shift = 0;
	for (; shift <= 5; shift++) {
		cells.clear();
		for (int y = 0; y < sheetHeight && cells.size() <= 255; y++) {
			const uint8_t *row = pixels + static_cast<size_t>(y) * rgba->pitch;
			for (int x = 0; x < sheetWidth; x++) {
				const uint8_t *p = row + static_cast<size_t>(x) * 4;
				if (p[3] < 128)
					continue;
				Cell &cell = cells[(static_cast<uint32_t>(p[0] >> shift) << 16) | (static_cast<uint32_t>(p[1] >> shift) << 8) | static_cast<uint32_t>(p[2] >> shift)];
				cell.r += p[0];
				cell.g += p[1];
				cell.b += p[2];
				cell.count++;
			}
		}
		if (cells.size() <= 255)
			break;
	}
	if (cells.size() > 255 || cells.empty())
		return std::nullopt;

	auto colours = std::make_shared<SpriteColours>();
	uint8_t next = 1;
	for (auto &[key, cell] : cells) {
		cell.index = next++;
		const int r = static_cast<int>(cell.r / cell.count);
		const int g = static_cast<int>(cell.g / cell.count);
		const int b = static_cast<int>(cell.b / cell.count);
		// The fallback is what the old importer would have drawn: the nearest shared-half entry. It
		// now only shades the colour and stands in for it on an 8-bit target.
		colours->Set(cell.index, (static_cast<uint32_t>(r) << 16) | (static_cast<uint32_t>(g) << 8) | static_cast<uint32_t>(b),
		    NearestSharedIndex(r, g, b));
	}

	std::vector<OwnedClxSpriteList> lists;
	lists.reserve(Facings);
	for (int row = 0; row < Facings; row++) {
		OwnedSurface column(frameWidth, cellHeight * frames);
		for (int frame = 0; frame < frames; frame++) {
			for (int y = 0; y < cellHeight; y++) {
				uint8_t *dst = &column[Point { 0, frame * cellHeight + y }];
				const uint8_t *src = pixels + static_cast<size_t>(row * cellHeight + y) * rgba->pitch
				    + static_cast<size_t>(frame) * frameWidth * 4;
				for (int x = 0; x < frameWidth; x++) {
					const uint8_t *p = src + static_cast<size_t>(x) * 4;
					dst[x] = p[3] < 128
					    ? TransparentIndex
					    : cells[(static_cast<uint32_t>(p[0] >> shift) << 16) | (static_cast<uint32_t>(p[1] >> shift) << 8) | static_cast<uint32_t>(p[2] >> shift)].index;
				}
			}
		}
		lists.push_back(SurfaceToClx(column, static_cast<unsigned>(frames), TransparentIndex));
	}
	return ColouredSpriteSheet { CombineListsIntoSheet(lists), std::move(colours) };
}

OwnedClxSpriteSheet CombineSpriteLists(std::vector<OwnedClxSpriteList> &lists)
{
	return CombineListsIntoSheet(lists);
}

uint32_t SharedPaletteRgb(uint8_t index)
{
	EnsurePalette(LevelPalettePath);
	const size_t at = static_cast<size_t>(index) * 3;
	return (static_cast<uint32_t>(LevelPalette[at]) << 16) | (static_cast<uint32_t>(LevelPalette[at + 1]) << 8) | static_cast<uint32_t>(LevelPalette[at + 2]);
}

std::optional<ColouredSpriteSheet> LoadPngSpriteSheetColoured(const char *path, uint16_t frameWidth)
{
	SDLSurfaceUniquePtr png { LoadPNG(path) };
	if (png == nullptr)
		return std::nullopt; // no import for this animation; the caller falls back to the CL2
	return ColouredSpriteSheetFromSurface(png.get(), frameWidth);
}

std::optional<OwnedClxSpriteListOrSheet> LoadPngMissileSheet(const char *name, uint16_t frameWidth, int rows)
{
	// The archive path missiles already use, with a .png on the end - the same "drop it beside the
	// CL2 it replaces" rule the player sheets follow. See the header.
	char path[MaxMpqPathSize];
	*BufCopy(path, "missiles\\", name, ".png") = '\0';

	SDLSurfaceUniquePtr png { LoadPNG(path) };
	if (png == nullptr)
		return std::nullopt; // no import for this missile; the caller falls back to the CL2

	std::vector<OwnedClxSpriteList> lists = SplitSurfaceIntoRows(png.get(), frameWidth, rows);
	if (lists.empty())
		return std::nullopt;

	// A LIST for a missile drawn one way, a SHEET for one drawn sixteen, and that is not a
	// formality: the engine asks a non-directional missile for its frames directly and a directional
	// one for a facing first. A one-row sheet answers the second question with row zero every time,
	// which on screen is a missile that flies south whichever way it was thrown.
	if (rows == 1)
		return OwnedClxSpriteListOrSheet { std::move(lists[0]) };
	return OwnedClxSpriteListOrSheet { CombineListsIntoSheet(lists) };
}

OptionalOwnedClxSpriteList LoadPngSpriteList(const char *path, uint16_t frameWidth, const char *palettePath)
{
	SDLSurfaceUniquePtr png { LoadPNG(path) };
	if (png == nullptr)
		return std::nullopt;

	// One row. Every caller so far is a single strip; a multi-row sheet wants LoadPngSpriteSheet.
	std::vector<OwnedClxSpriteList> lists = SplitSurfaceIntoRows(png.get(), frameWidth, 1, palettePath);
	if (lists.empty())
		return std::nullopt;
	return std::move(lists[0]);
}

OptionalOwnedClxSpriteList LoadPngItemDropSheet(const char *name, uint16_t frameWidth)
{
	// Beside the CEL it replaces, as for missiles: items\<name>.png.
	char path[MaxMpqPathSize];
	*BufCopy(path, "items\\", name, ".png") = '\0';
	return LoadPngSpriteList(path, frameWidth);
}

OptionalOwnedClxSpriteSheet ScaleSpriteSheet(ClxSpriteSheet sheet, int percent)
{
	if (percent <= 0 || percent == 100 || sheet.numLists() == 0)
		return std::nullopt;

	// The mask pass draws through a table whose every entry is 1, which is the only reliable way to
	// know WHICH pixels a frame touched: a sprite pixel is free to be palette index 0 (the foot shadow
	// is), so "still zero" does not mean "not drawn". Same two-pass trick as the export tool.
	std::array<uint8_t, 256> opaque;
	opaque.fill(1);

	std::vector<OwnedClxSpriteList> lists;
	lists.reserve(sheet.numLists());
	for (size_t dir = 0; dir < sheet.numLists(); dir++) {
		const ClxSpriteList list = sheet[dir];
		const int frames = static_cast<int>(list.numSprites());
		int cellWidth = 0;
		int cellHeight = 0;
		for (int i = 0; i < frames; i++) {
			cellWidth = std::max(cellWidth, static_cast<int>(list[static_cast<size_t>(i)].width()));
			cellHeight = std::max(cellHeight, static_cast<int>(list[static_cast<size_t>(i)].height()));
		}
		if (frames == 0 || cellWidth == 0 || cellHeight == 0)
			return std::nullopt;

		// Every frame rendered into one tall column, feet on its cell's floor - the anchor ClxDraw
		// draws a sprite at, so a taller result grows upward from the ground and needs no offset.
		OwnedSurface colour(cellWidth, cellHeight * frames);
		OwnedSurface mask(cellWidth, cellHeight * frames);
		for (int y = 0; y < cellHeight * frames; y++) {
			std::memset(&colour[Point { 0, y }], 0, static_cast<size_t>(cellWidth));
			std::memset(&mask[Point { 0, y }], 0, static_cast<size_t>(cellWidth));
		}
		std::array<bool, 256> used {};
		for (int i = 0; i < frames; i++) {
			const ClxSprite sprite = list[static_cast<size_t>(i)];
			ClxDraw(colour, { 0, (i + 1) * cellHeight - 1 }, sprite);
			ClxDrawTRN(mask, { 0, (i + 1) * cellHeight - 1 }, sprite, opaque.data());
		}
		for (int y = 0; y < cellHeight * frames; y++) {
			const uint8_t *maskRow = &mask[Point { 0, y }];
			const uint8_t *colourRow = &colour[Point { 0, y }];
			for (int x = 0; x < cellWidth; x++)
				if (maskRow[x] != 0)
					used[colourRow[x]] = true;
		}
		// SurfaceToClx needs one index to mean "no pixel"; any the sprite never draws will do.
		int transparent = -1;
		for (int i = 0; i < 256; i++) {
			if (!used[static_cast<size_t>(i)]) {
				transparent = i;
				break;
			}
		}
		if (transparent < 0)
			return std::nullopt;

		// Nearest neighbour. At 120% every fifth row and column is doubled, which on a 96-pixel body
		// is invisible in motion; a filter would blur the palette indices, and there is no such thing
		// as an in-between index.
		const int scaledWidth = std::max(1, (cellWidth * percent + 50) / 100);
		const int scaledHeight = std::max(1, (cellHeight * percent + 50) / 100);
		OwnedSurface scaled(scaledWidth, scaledHeight * frames);
		for (int i = 0; i < frames; i++) {
			for (int y = 0; y < scaledHeight; y++) {
				const int sourceY = i * cellHeight + std::min(cellHeight - 1, y * cellHeight / scaledHeight);
				const uint8_t *maskRow = &mask[Point { 0, sourceY }];
				const uint8_t *colourRow = &colour[Point { 0, sourceY }];
				uint8_t *dst = &scaled[Point { 0, i * scaledHeight + y }];
				for (int x = 0; x < scaledWidth; x++) {
					const int sourceX = std::min(cellWidth - 1, x * cellWidth / scaledWidth);
					dst[x] = maskRow[sourceX] != 0 ? colourRow[sourceX] : static_cast<uint8_t>(transparent);
				}
			}
		}
		lists.push_back(SurfaceToClx(scaled, static_cast<unsigned>(frames), static_cast<uint8_t>(transparent)));
	}
	return CombineListsIntoSheet(lists);
}

} // namespace devilution::oracool
