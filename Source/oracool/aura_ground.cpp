#include "oracool/aura_ground.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

#include <SDL.h>

#include "engine.h"
#include "engine/palette.h"
#include "oracool/aura_field.h"
#include "oracool/class_tree.h"
#include "player.h"
#include "utils/log.hpp"
#include "utils/png.h"

namespace devilution::oracool {

namespace {

using Skill = ClassTreeSkill;

constexpr int ArtWidth = 512;
constexpr int ArtHeight = 256;

/**
 * @brief sqrt(2) as a fraction, to five decimals.
 *
 * Needed because one tile step is (TILE_WIDTH/2, TILE_HEIGHT/2) on screen, not (TILE_WIDTH, 0): a
 * world-space circle of radius R tiles projects to an ellipse whose semi-axes are R*(TILE_WIDTH/2)
 * and R*(TILE_HEIGHT/2) scaled by sqrt(2), because the extreme point lies on the diagonal.
 *
 * The first cut of this file assumed 512px WAS the eight-tile radius and scaled by R/8. That draws
 * a ring of about 5.7 tiles when it claims eight - every ring 41% too small for the field it
 * stands for, which nothing but the eye can catch.
 */
constexpr int Sqrt2Num = 181;
constexpr int Sqrt2Den = 128;

/**
 * @brief One aura's art, quantised once and kept.
 *
 * `index` is the palette entry per pixel and `alpha` the source coverage. They are kept SEPARATE
 * rather than folded into one pre-blended image because the blend depends on what is underneath -
 * a different floor tile every time - so it can only happen at draw.
 */
struct AuraArt {
	std::vector<uint8_t> index;
	std::vector<uint8_t> alpha;
	bool loadAttempted = false;
	bool usable = false;
};

/** @brief File id per aura, matching the delivered pack exactly. */
struct AuraFile {
	Skill skill;
	const char *id;
};

constexpr std::array<AuraFile, 20> AuraFiles { {
    // Offensive
    { Skill::Might, "might" },
    { Skill::HolyFire, "holy_fire" },
    { Skill::Thorns, "thorns" },
    { Skill::BlessedAim, "blessed_aim" },
    { Skill::Concentration, "concentration" },
    { Skill::HolyFreeze, "holy_freeze" },
    { Skill::HolyShock, "holy_shock" },
    { Skill::Sanctuary, "sanctuary" },
    { Skill::Fanaticism, "fanaticism" },
    { Skill::Conviction, "conviction" },
    // Defensive
    { Skill::Prayer, "prayer" },
    { Skill::ResistFire, "resist_fire" },
    { Skill::Defiance, "defiance" },
    { Skill::ResistCold, "resist_cold" },
    { Skill::Cleansing, "cleansing" },
    { Skill::ResistLightning, "resist_lightning" },
    { Skill::Vigor, "vigor" },
    { Skill::Meditation, "meditation" },
    { Skill::Redemption, "redemption" },
    { Skill::Salvation, "salvation" },
} };

std::array<AuraArt, AuraFiles.size()> Art;

/**
 * @brief The palette snapshot the art was quantised against.
 *
 * Only entries 128-255 matter: the lower half is redefined per level type and colour-cycled, which
 * is precisely why the brief confined the art to the upper half. If those 128 ever change the
 * quantisation is stale and everything is dropped and redone.
 */
std::array<SDL_Color, 128> QuantisedAgainst {};
bool HaveQuantised = false;

int IndexOfSkill(Skill skill)
{
	for (size_t i = 0; i < AuraFiles.size(); i++) {
		if (AuraFiles[i].skill == skill)
			return static_cast<int>(i);
	}
	return -1;
}

uint8_t NearestSharedPaletteIndex(uint8_t r, uint8_t g, uint8_t b, std::vector<uint8_t> &cache)
{
	const uint16_t key = ((r >> 3) << 10) | ((g >> 3) << 5) | (b >> 3);
	if (cache[key] != 0)
		return cache[key];
	int best = 128;
	int bestDist = INT32_MAX;
	for (int i = 128; i < 256; i++) {
		const SDL_Color &c = orig_palette[i];
		const int dr = static_cast<int>(c.r) - r;
		const int dg = static_cast<int>(c.g) - g;
		const int db = static_cast<int>(c.b) - b;
		const int dist = 2 * dr * dr + 4 * dg * dg + 3 * db * db;
		if (dist < bestDist) {
			bestDist = dist;
			best = i;
		}
	}
	cache[key] = static_cast<uint8_t>(best);
	return cache[key];
}

void LoadAura(int slot)
{
	AuraArt &art = Art[slot];
	art.loadAttempted = true;

	char path[64];
	std::snprintf(path, sizeof(path), "ui\\aura_%s.png", AuraFiles[slot].id);

	// LoadPNG, NOT IMG_LoadPNG. The first goes through OpenAssetAsSdlRwOps and so finds the file
	// inside oracool.mpq; the second reads the filesystem directly and would only ever see a loose
	// copy sitting beside the executable. Both compile, and the difference does not show until the
	// art is packed - which is when it matters.
	SDL_Surface *png = LoadPNG(path);
	if (png == nullptr) {
		// Not an error worth shouting about: the pack is droppable art, and an aura with no image
		// simply has no ring. Everything else about it still works.
		LogVerbose("Oracool aura art: {:s} not found - that aura burns without a ring", path);
		return;
	}
	SDL_Surface *rgba = SDL_ConvertSurfaceFormat(png, SDL_PIXELFORMAT_ABGR8888, 0);
	SDL_FreeSurface(png);
	if (rgba == nullptr) {
		LogWarn("Oracool aura art: conversion failed for {:s}: {:s}", path, SDL_GetError());
		return;
	}
	if (rgba->w != ArtWidth || rgba->h != ArtHeight) {
		// Refused rather than scaled to fit. The blit assumes the ring is centred in its frame and
		// reaches its edges, which is what makes the destination ellipse the aura's actual radius;
		// art of another size would land somewhere plausible and wrong, which is the failure that
		// survives a green build.
		LogWarn("Oracool aura art: {:s} is {}x{}, expected {}x{} - ignored",
		    path, rgba->w, rgba->h, ArtWidth, ArtHeight);
		SDL_FreeSurface(rgba);
		return;
	}

	art.index.assign(static_cast<size_t>(ArtWidth) * ArtHeight, 0);
	art.alpha.assign(static_cast<size_t>(ArtWidth) * ArtHeight, 0);
	std::vector<uint8_t> cache(1 << 15, 0);
	const auto *pixels = static_cast<const uint8_t *>(rgba->pixels);
	for (int y = 0; y < ArtHeight; y++) {
		const uint8_t *row = pixels + static_cast<size_t>(y) * rgba->pitch;
		for (int x = 0; x < ArtWidth; x++) {
			const uint8_t a = row[x * 4 + 3];
			const size_t at = static_cast<size_t>(y) * ArtWidth + x;
			if (a == 0)
				continue;
			art.index[at] = NearestSharedPaletteIndex(row[x * 4 + 0], row[x * 4 + 1], row[x * 4 + 2], cache);
			art.alpha[at] = a;
		}
	}
	SDL_FreeSurface(rgba);
	art.usable = true;
}

void DropQuantisationIfPaletteMoved()
{
	if (HaveQuantised && std::memcmp(QuantisedAgainst.data(), &orig_palette[128], sizeof(QuantisedAgainst)) == 0)
		return;
	for (AuraArt &art : Art) {
		art.index.clear();
		art.alpha.clear();
		art.loadAttempted = false;
		art.usable = false;
	}
	std::memcpy(QuantisedAgainst.data(), &orig_palette[128], sizeof(QuantisedAgainst));
	HaveQuantised = true;
}

/**
 * @brief 4x4 ordered dither, values 0-15.
 *
 * The engine's only blend is a fixed 50%, so the levels reachable per pixel are none, 50% and 75%.
 * This is what fills the gaps between them: at a coverage of, say, 30% the matrix lets roughly
 * three pixels in ten take the 50% blend and leaves the rest alone, which at the size these are
 * seen reads as 30%.
 */
constexpr std::array<uint8_t, 16> DitherMatrix { {
    0, 8, 2, 10,
    12, 4, 14, 6,
    3, 11, 1, 9,
    15, 7, 13, 5 } };

/** @brief A slow brightness pulse, so a lit aura reads as burning rather than painted on. */
int PulsePercent()
{
	// About a four-second cycle, +/-12%. Deliberately gentle: this sits under the player for as long
	// as the aura is on, and anything faster becomes something to notice rather than something to
	// stand in.
	const uint32_t phase = SDL_GetTicks() % 4000U;
	const double t = static_cast<double>(phase) / 4000.0 * 2.0 * 3.14159265358979;
	return 100 + static_cast<int>(12.0 * std::sin(t));
}

/**
 * @brief How wide the ring is DRAWN, in tiles. Two, or three once the aura is well invested.
 *
 * Separate from AuraRadiusForPoints, which is what the aura actually reaches - see the note at the
 * call site for why the two parted company. Points still change the ring, so investment is still
 * visible; they change it by a tile rather than by five.
 */
int AuraVisualRadiusTiles(int points)
{
	if (points <= 0)
		return 0;
	return points >= 5 ? 3 : 2;
}

/**
 * @brief Blits the aura, scaled to @p radiusTiles, centred on @p centre.
 *
 * Nearest-neighbour sampled: the source is a soft gradient with no hard edges to alias, and the
 * shrink is never more than half, so a filtered sample would cost more than it showed.
 */
void BlitAura(const Surface &out, const AuraArt &art, Point centre, int radiusTiles, int pulsePercent)
{
	// The screen ellipse the aura's world-space circle projects to. At the eight-tile cap this is
	// 724x362, so the art is enlarged by up to sqrt(2) rather than only ever shrunk - acceptable on
	// a source that is all soft gradient, and the dither is applied in destination space so it stays
	// fine-grained however far the source is stretched.
	const int dstW = 2 * radiusTiles * (TILE_WIDTH / 2) * Sqrt2Num / Sqrt2Den;
	const int dstH = 2 * radiusTiles * (TILE_HEIGHT / 2) * Sqrt2Num / Sqrt2Den;
	if (dstW <= 0 || dstH <= 0)
		return;

	const int left = centre.x - dstW / 2;
	const int top = centre.y - dstH / 2;

	// Clipped ONCE rather than per pixel. At the eight-tile cap this walks 724x362 = 262,000 pixels,
	// and standing near a screen edge threw most of them away one `continue` at a time. Exactly
	// equivalent to the per-pixel test it replaces: the iterations removed are the ones that did
	// nothing, since an out-of-range pixel had no side effect before it was skipped.
	const int dyFrom = std::max(0, -top);
	const int dyTo = std::min(dstH, out.h() - top);
	const int dxFrom = std::max(0, -left);
	const int dxTo = std::min(dstW, out.w() - left);
	if (dyFrom >= dyTo || dxFrom >= dxTo)
		return;

	for (int dy = dyFrom; dy < dyTo; dy++) {
		const int y = top + dy;
		const int sy = dy * ArtHeight / dstH;
		uint8_t *dstRow = out.at(0, y);
		for (int dx = dxFrom; dx < dxTo; dx++) {
			const int x = left + dx;
			const int sx = dx * ArtWidth / dstW;
			const size_t at = static_cast<size_t>(sy) * ArtWidth + sx;
			const uint8_t a = art.alpha[at];
			if (a == 0)
				continue;

			// Coverage in sixteenths of the 75% ceiling two blends can reach. The art peaks at 166
			// of 255 (65%, the brief's cap), so in practice this lands between one and two blends.
			int coverage = a * pulsePercent / 100;
			if (coverage > 255)
				coverage = 255;
			const int level = coverage * 32 / 255; // 0..32, where 16 == one blend, 32 == two
			const uint8_t threshold = DitherMatrix[(y & 3) * 4 + (x & 3)];

			int blends = level / 16;
			if (static_cast<int>(threshold) < (level % 16))
				blends++;
			if (blends <= 0)
				continue;

			uint8_t &dst = dstRow[x];
			const uint8_t src = art.index[at];
			dst = paletteTransparencyLookup[dst][src];
			if (blends > 1)
				dst = paletteTransparencyLookup[dst][src];
		}
	}
}

} // namespace

void DrawAuraGround(const Surface &out, Point tilePosition, Point targetBufferPosition,
    int rows, int columns)
{
	if (MyPlayer == nullptr || !MyPlayer->isOnActiveLevel())
		return;
	const Player &player = *MyPlayer;

	const Skill aura = GetActiveClassAura(player);
	if (aura == Skill::None)
		return;
	if (!IsClassTreeSkillUnlocked(player, aura))
		return;
	const int points = ClassTreeInvestment(player, aura);
	if (AuraRadiusForPoints(points) <= 0)
		return;
	// The DRAWN radius, which is deliberately no longer the aura's reach (user, 2026-08-27: "shrink
	// auras visual assets to 2-3 tile radius. now the aura graphics spans about 10 tile maybe").
	//
	// It did. The gameplay radius runs four to eight tiles, and the projection multiplies it by
	// sqrt(2) to reach the diagonal, so even a single point drew an ellipse about eleven tiles
	// across - a wash of colour under half the screen rather than a ring around the character.
	//
	// So the ring is now a MARK ON THE CHARACTER, not a map of the field. That is a real trade and
	// worth stating: a player can no longer read the aura's reach off the floor. It was not readable
	// before either - at eight tiles the ellipse covered everything already on screen, which is the
	// note AuraRadiusForPoints itself makes about why it caps there - so what is lost is the
	// appearance of information rather than information.
	const int radius = AuraVisualRadiusTiles(points);
	if (radius <= 0)
		return;

	const int slot = IndexOfSkill(aura);
	if (slot < 0)
		return;

	DropQuantisationIfPaletteMoved();
	if (!Art[slot].loadAttempted)
		LoadAura(slot);
	if (!Art[slot].usable)
		return;

	// DrawFloor's walk, repeated exactly - see this file's header for why it is repeated rather
	// than solved. The moment it arrives at the player's tile it knows where on screen that tile
	// is, which is the one thing this needs.
	const Point playerTile = player.position.tile;
	for (int i = 0; i < rows; i++) {
		for (int j = 0; j < columns; j++) {
			if (tilePosition == playerTile) {
				// The CENTRE of the tile diamond, then the same walking offset the player sprite
				// carries - without it the ring would jump a whole tile at a time while the player
				// slid smoothly between them.
				//
				// targetBufferPosition is the diamond's BOTTOM-left: RenderTile draws upward from it,
				// covering TriangleHeight (LowerHeight 16 + TriangleUpperHeight 15) rows above. So the
				// centre is half a tile height ABOVE this point, not below. Getting that sign wrong
				// put every ring a full tile south of the player - visible instantly in play, and
				// invisible to the build.
				Point centre = targetBufferPosition + Displacement { TILE_WIDTH / 2, -TILE_HEIGHT / 2 };
				if (player.isWalking()) {
					const Displacement walk = GetOffsetForWalking(player.AnimInfo, player._pdir);
					centre += walk;
				}
				BlitAura(out, Art[slot], centre, radius, PulsePercent());
				return;
			}
			tilePosition += Direction::East;
			targetBufferPosition.x += TILE_WIDTH;
		}
		tilePosition += Displacement(Direction::West) * columns;
		targetBufferPosition.x -= columns * TILE_WIDTH;
		targetBufferPosition.y += TILE_HEIGHT / 2;
		if ((i & 1) != 0) {
			tilePosition.x++;
			columns--;
			targetBufferPosition.x += TILE_WIDTH / 2;
		} else {
			tilePosition.y++;
			columns++;
			targetBufferPosition.x -= TILE_WIDTH / 2;
		}
	}
}

} // namespace devilution::oracool
