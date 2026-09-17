#include "oracool/hero_preview.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <memory>
#include <optional>

#include <SDL.h>
#include <fmt/format.h>

#include "engine.h" // GetAnimationFrame
#include "engine/clx_sprite.hpp"
#include "engine/direction.hpp"
#include "engine/load_cl2.hpp"
#include "engine/load_file.hpp"
#include "engine/palette.h"
#include "engine/point.hpp"
#include "engine/rectangle.hpp"
#include "engine/render/clx_render.hpp"
#include "oracool/hero_look.h"
#include "oracool/sprite_colours.h"
#include "oracool/sprite_mix.h"
#include "playerdat.hpp"
#include "utils/log.hpp"
#include "utils/sdl_geometry.h"
#include "utils/stdcompat/optional.hpp"

namespace devilution::oracool {

namespace {

/**
 * Any level palette would do. Player sprites live almost entirely in the palette's GLOBAL half
 * (entries 128-255), which is identical across town and every dungeon type - that is exactly why a
 * character looks the same in town as in the catacombs. Town's is the one that always exists.
 */
constexpr char LevelPalettePath[] = "levels\\towndata\\town.pal";

/**
 * Nearest-neighbour blow-up, so the pixel art stays pixel art rather than turning to soup.
 *
 * 3 -> 6 on the user's call ("make the character preview twice bigger. there is a lot of room").
 *
 * The room was real but not reachable at 3: what was being scaled was the whole 96x96 CL2 frame,
 * which is mostly transparent padding sized for the animations that swing a weapon, not for standing
 * still. 96 * 6 = 576 against a 369px-tall area - it only fits because PreviewInk crops the padding
 * away first and this multiplies the FIGURE.
 *
 * Clamped down if even the figure will not fit, so no layout or resolution can push it off its area.
 */
constexpr int TargetPreviewScale = 6;

/**
 * Milliseconds per animation frame.
 *
 * GetAnimationFrame's second parameter is named `fps` but divides SDL_GetTicks, so it is really a
 * millisecond divisor - the period is frames * this. The default 60 read as a fidget rather than a
 * character breathing; 180 (3x) was a touch slow to watch, and 150 is where the user settled it.
 */
constexpr int PreviewFrameMs = 150;

OptionalOwnedClxSpriteSheet PreviewSheet;
HeroClass LoadedClass = HeroClass::Warrior;
int LoadedGfxNum = -1;
uint8_t LoadedGearLook = 0;
/** What the loaded sheet's indices mean - a dye, a mixed sheet's moved pieces - or null for a plain sheet. */
std::shared_ptr<const SpriteColours> PreviewColours;
/** The look being built in the background, shown plain meanwhile. */
std::optional<PlayerSheetRequest> PendingLook;
uint16_t LoadedWidth = 0;
uint16_t LoadedHeight = 0;

/**
 * @brief Where the character actually is inside the frame - the union of every frame's opaque pixels.
 *
 * The UNION, not the current frame's own bounds, and that is the point: a per-frame box would shift as
 * the idle loop breathed, and since the figure is positioned from this box the character would drift
 * around its area instead of standing in it.
 */
Rectangle PreviewInk {};

/** Level-palette index -> nearest index in whatever palette the screen currently has. */
std::array<uint8_t, 256> LevelToUiTrn {};
bool TrnBuilt = false;
std::array<SDL_Color, 256> TrnBuiltFor {};
bool LevelPaletteMissing = false;

/**
 * @brief Bard and Barbarian borrow another class's body when their own artwork is absent.
 *
 * A copy of player.cpp's GetPlayerSpriteClass, which has no header declaration. Duplicated rather
 * than exported because it is two lines and exporting it would widen player.cpp's surface for one
 * caller - but it MUST stay in step, or the preview would look for a sprite the game itself does not
 * use.
 */
HeroClass SpriteClassFor(HeroClass cls)
{
	if (cls == HeroClass::Bard && !gbBard)
		return HeroClass::Rogue;
	if (cls == HeroClass::Barbarian && !gbBarbarian)
		return HeroClass::Warrior;
	if (cls == HeroClass::Necromancer)
		return HeroClass::Sorcerer;
	return cls;
}

/**
 * @brief (Re)builds the palette translation if the screen's palette has changed under it.
 *
 * Index 0 is excluded as a DESTINATION deliberately. The scaling pass below treats 0 as "no pixel
 * here", so a translation that could emit it would punch holes through the character wherever the
 * level palette's colour happened to be closest to the UI palette's entry 0.
 */
bool EnsureTrn()
{
	if (LevelPaletteMissing)
		return false;
	if (TrnBuilt && std::memcmp(TrnBuiltFor.data(), orig_palette.data(), sizeof(TrnBuiltFor)) == 0)
		return true;

	std::array<uint8_t, 768> levelPalette {};
	LoadFileInMem(LevelPalettePath, levelPalette);

	for (int i = 0; i < 256; i++) {
		const int r = levelPalette[i * 3];
		const int g = levelPalette[i * 3 + 1];
		const int b = levelPalette[i * 3 + 2];
		int best = 1;
		int bestDist = INT32_MAX;
		for (int j = 1; j < 256; j++) {
			const SDL_Color &c = orig_palette[j];
			const int dr = static_cast<int>(c.r) - r;
			const int dg = static_cast<int>(c.g) - g;
			const int db = static_cast<int>(c.b) - b;
			// The same channel weighting the background and HUD quantizers use, so every pass in
			// this edition agrees about what "closest colour" means.
			const int dist = 2 * dr * dr + 4 * dg * dg + 3 * db * db;
			if (dist < bestDist) {
				bestDist = dist;
				best = j;
			}
		}
		LevelToUiTrn[i] = static_cast<uint8_t>(best);
	}
	// The sprite's own index 0 is its transparent colour and its shadow's; mapped to the nearest
	// black it drew an opaque blob under the figure (user screenshot, 2026-09-07). 0 stays 0, which
	// the blit skips.
	LevelToUiTrn[0] = 0;

	std::memcpy(TrnBuiltFor.data(), orig_palette.data(), sizeof(TrnBuiltFor));
	TrnBuilt = true;
	return true;
}

/**
 * @brief Measures PreviewInk by drawing every frame once and looking at which pixels were touched.
 *
 * Asks WHERE the sprite draws, not what colour, so it runs off a translation whose every entry is
 * opaque rather than off LevelToUiTrn - which means it does not depend on the palette and can be done
 * at load time, once per character, instead of per frame.
 */
void MeasurePreviewInk()
{
	PreviewInk = {};
	LoadedHeight = 0;
	if (!PreviewSheet)
		return;

	const ClxSpriteList frames = (*PreviewSheet)[static_cast<size_t>(Direction::South)];
	if (frames.numSprites() == 0 || LoadedWidth == 0)
		return;

	// Taken as the maximum rather than assumed uniform: nothing in the format promises every frame in
	// a sheet is the same height, and the scratch surface below has to hold the tallest.
	for (size_t i = 0; i < frames.numSprites(); i++)
		LoadedHeight = std::max(LoadedHeight, frames[i].height());
	if (LoadedHeight == 0)
		return;

	std::array<uint8_t, 256> opaque {};
	opaque.fill(1);

	OwnedSurface scratch(LoadedWidth, LoadedHeight);
	int minX = LoadedWidth;
	int minY = LoadedHeight;
	int maxX = -1;
	int maxY = -1;
	for (size_t i = 0; i < frames.numSprites(); i++) {
		for (int y = 0; y < LoadedHeight; y++)
			std::memset(&scratch[Point { 0, y }], 0, static_cast<size_t>(LoadedWidth));
		ClxDrawTRN(scratch, { 0, LoadedHeight - 1 }, frames[i], opaque.data());
		for (int y = 0; y < LoadedHeight; y++) {
			const uint8_t *row = &scratch[Point { 0, y }];
			for (int x = 0; x < LoadedWidth; x++) {
				if (row[x] == 0)
					continue;
				minX = std::min(minX, x);
				maxX = std::max(maxX, x);
				minY = std::min(minY, y);
				maxY = std::max(maxY, y);
			}
		}
	}

	if (maxX < 0)
		return; // every frame was empty - nothing to draw, and nothing to divide by later
	PreviewInk = { { minX, minY }, { maxX - minX + 1, maxY - minY + 1 } };
}

/** @brief The largest whole-pixel scale that keeps the figure inside @p area, up to the target. */
int PreviewScaleFor(Rectangle area)
{
	if (PreviewInk.size.width <= 0 || PreviewInk.size.height <= 0)
		return 0;
	const int byWidth = area.size.width / PreviewInk.size.width;
	const int byHeight = area.size.height / PreviewInk.size.height;
	// Whole numbers only: a fractional scale through a nearest-neighbour resample gives uneven pixel
	// runs, which on a 96px sprite blown up this far is the difference between pixel art and a mess.
	return std::min({ TargetPreviewScale, byWidth, byHeight });
}

} // namespace

void SetHeroPreview(HeroClass heroClass, uint8_t gfxnum, uint8_t gearLook)
{
	if (PreviewSheet && LoadedClass == heroClass && LoadedGfxNum == gfxnum && LoadedGearLook == gearLook)
		return;

	PreviewSheet = std::nullopt;
	PreviewColours = nullptr;
	PendingLook.reset();
	LoadedClass = heroClass;
	LoadedGfxNum = gfxnum;
	LoadedGearLook = gearLook;

	const HeroClass spriteClass = SpriteClassFor(heroClass);
	const auto weapon = static_cast<size_t>(gfxnum & 0xF);
	const auto armour = static_cast<size_t>(gfxnum >> 4);
	if (weapon >= WepChar.size() || armour >= ArmourChar.size()) {
		LogWarn("Oracool hero preview: gfxnum {:d} is not a sprite this build knows", gfxnum);
		return;
	}

	const char prefix[3] = { CharChar[static_cast<size_t>(spriteClass)], ArmourChar[armour], WepChar[weapon] };
	char path[256];
	// "st" is the TOWN stand - the idle loop. The dungeon stand ("as") is a different, shorter
	// animation and is lit for a dungeon.
	*fmt::format_to(path, R"(plrgfx\{0}\{1}\{1}st)",
	    PlayersData[static_cast<size_t>(spriteClass)].classPath, string_view(prefix, 3))
	    = '\0';

	LoadedWidth = PlayersSpriteData[static_cast<size_t>(spriteClass)].stand;

	// The look the game would draw, out of the cache the game fills (same key). Never built here and now: a list
	// of heroes is arrowed through quickly, and half a second per hero would be felt. Unknown means "ask, show
	// the plain sheet, swap when DrawHeroPreview sees it land".
	const PlayerSheetRequest request = MakePlayerSheetRequest(heroClass, spriteClass, gfxnum, gearLook, "st", LoadedWidth);
	if (WantsMixedSheet(request)) {
		std::optional<ColouredSpriteSheet> cached;
		switch (TakeCachedPlayerSheet(request, cached)) {
		case CachedSheetState::Ready:
			PreviewSheet = std::move(cached->sheet);
			PreviewColours = std::move(cached->colours);
			LoadedWidth = (*PreviewSheet)[0][0].width(); // the cached sheet is already at the size of the class
			MeasurePreviewInk();
			return;
		case CachedSheetState::Unknown:
			RequestPlayerSheet(request);
			PendingLook = request;
			break;
		case CachedSheetState::Nothing:
			break;
		}
	}

	PreviewSheet = LoadCl2Sheet(path, LoadedWidth);
	PreviewColours = HeroColoursFor(heroClass, gfxnum); // the dye of the class, on the plain sheet too
	MeasurePreviewInk();
}

void ClearHeroPreview()
{
	PreviewSheet = std::nullopt;
	PreviewColours = nullptr;
	PendingLook.reset();
	LoadedGearLook = 0;
	LoadedGfxNum = -1;
	PreviewInk = {};
}

void FreeHeroPreview()
{
	ClearHeroPreview();
	TrnBuilt = false;
}

void DrawHeroPreview(const Surface &out, Rectangle area)
{
	// A look being built: this screen has no game tick, so the mixer is fed from here - one archive read a frame -
	// and the moment the sheet is settled the preview is loaded again, which now finds it in the cache.
	if (PendingLook) {
		PumpSpriteMixer();
		TakeFinishedPlayerSheets(); // nobody in a menu is waiting for these; the cache has them
		if (IsPlayerSheetSettled(*PendingLook)) {
			const HeroClass heroClass = LoadedClass;
			const auto gfxnum = static_cast<uint8_t>(LoadedGfxNum);
			const uint8_t gearLook = LoadedGearLook;
			ClearHeroPreview();
			SetHeroPreview(heroClass, gfxnum, gearLook);
		}
	}

	if (!PreviewSheet || !EnsureTrn())
		return;

	const ClxSpriteList frames = (*PreviewSheet)[static_cast<size_t>(Direction::South)];
	if (frames.numSprites() == 0)
		return;
	// South, so the character faces the player rather than showing them its back.
	const ClxSprite sprite = frames[GetAnimationFrame(static_cast<int>(frames.numSprites()), PreviewFrameMs)];

	const int width = LoadedWidth;
	const int height = LoadedHeight;
	if (width <= 0 || height <= 0)
		return;

	const int scale = PreviewScaleFor(area);
	if (scale <= 0)
		return; // the area cannot hold the figure even at 1:1

	// Rendered at 1:1 first because ClxDrawTRN cannot scale, then blown up. The intermediate is
	// cleared to 0 so that whatever the sprite's transparent runs leave untouched stays 0 - which is
	// what the blit below treats as "no pixel", and why EnsureTrn never emits 0.
	OwnedSurface frame(width, height);
	for (int y = 0; y < height; y++)
		std::memset(&frame[Point { 0, y }], 0, static_cast<size_t>(width));
	// This screen draws palette INDICES, so a sheet with colours of its own is shown through each colour's
	// fallback - the nearest level entry - and only then taken to the UI palette. A blue shirt is one of eight
	// blues here rather than sixteen; at menu scale, behind a nearest-match to another palette, that is invisible.
	std::array<uint8_t, 256> trn = LevelToUiTrn;
	if (PreviewColours != nullptr) {
		for (size_t i = 1; i < 256; i++)
			trn[i] = LevelToUiTrn[PreviewColours->Fallback(static_cast<uint8_t>(i))];
	}
	ClxDrawTRN(frame, { 0, height - 1 }, sprite, trn.data());

	// Only the ink is scaled. Blowing up the whole frame would spend most of the area on the padding
	// around the character and cap the figure at a third of the size it can actually be.
	const int scaledWidth = PreviewInk.size.width * scale;
	const int scaledHeight = PreviewInk.size.height * scale;
	OwnedSurface scaled(scaledWidth, scaledHeight);
	for (int y = 0; y < scaledHeight; y++) {
		const uint8_t *srcRow = &frame[Point { PreviewInk.position.x, PreviewInk.position.y + y / scale }];
		uint8_t *dstRow = &scaled[Point { 0, y }];
		for (int x = 0; x < scaledWidth; x++)
			dstRow[x] = srcRow[x / scale];
	}

	// Centred across the area and standing on its bottom edge, so a taller class does not float.
	// PreviewScaleFor guarantees both of these stay inside the area, so neither can go negative.
	const Point origin { area.position.x + (area.size.width - scaledWidth) / 2,
		area.position.y + area.size.height - scaledHeight };
	out.BlitFromSkipColorIndexZero(scaled, MakeSdlRect(0, 0, scaledWidth, scaledHeight), origin);
}

} // namespace devilution::oracool
