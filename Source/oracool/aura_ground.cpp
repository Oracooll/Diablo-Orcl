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
#include "engine/render/primitive_render.hpp" // SetHalfTransparentPixelUnchecked (v1.11)
#include "levels/gendung.h"
#include "missiles.h" // FallingArrowLandedTicks - Rain of Arrows' impact rings
#include "oracool/melee_skills.h" // ArmedClassMeleeSkill - Fend's ring
#include "monster.h"
#include "oracool/aura_field.h"
#include "oracool/class_tree.h"
#include "oracool/minions.h"
#include "oracool/missile_tint.h"
#include "oracool/splats.h" // the rings' glow goes to the graphics card on its path
#include "options.h"
#include "player.h"
#include "utils/log.hpp"
#include "utils/png.h"

namespace devilution::oracool {

namespace {

using Skill = ClassTreeSkill;

constexpr int ArtWidth = 512;
constexpr int ArtHeight = 256;

// The sqrt(2) projection factor that used to live here is gone, and its removal is the fix for
// "shrink auras. they are not 2 tiles in diameter" (user, 2026-08-27).
//
// It was right for what this file used to do: a world-space CIRCLE of radius R tiles projects to an
// ellipse whose extreme point lies on the diagonal, so the semi-axes want scaling by sqrt(2). That
// was the correct conversion while the drawn ring was standing in for the aura's actual reach.
//
// It stopped being the aura's reach on 2026-08-27, when the ring became a mark on the character
// instead of a map of the field. From that point the size is not derived from anything - it is
// simply stated - and a leftover projection factor only made the stated number wrong by 41%: a "2
// tile" ring measured 2.8 tiles across. The number asked for is now the number drawn.

/**
 * @brief One aura's art, quantised once and kept.
 *
 * `index` is the palette entry per pixel and `alpha` the source coverage. They are kept SEPARATE
 * rather than folded into one pre-blended image because the blend depends on what is underneath -
 * a different floor tile every time - so it can only happen at draw.
 */
struct AuraArt {
	/**
	 * Built LAZILY, for an indexed target only (user, 2026-10-10: true-colour migration): the 32-bit
	 * screen draws `rgb`, which is the PNG's own colour and does not depend on the palette, so it is
	 * never dropped when the palette moves. This quantised copy is (re)built from `rgb` by
	 * EnsureAuraIndex when an indexed draw finds it missing or older than the palette.
	 */
	mutable std::vector<uint8_t> index;
	mutable uint32_t indexGeneration = 0;
	std::vector<uint8_t> alpha;
	/** Renderer stage 2 (v1.11): the ring's own colours, XRGB, for the 32-bit screen; `alpha` is shared. */
	std::vector<uint32_t> rgb;
	/** The colour cycle (2026-09-27): each pixel's angle round the ring's centre, 0-255 for a full turn. */
	std::vector<uint8_t> angle;
	/** The glow (2026-10-12): the ring's colour on the whole, alpha-weighted, at full brightness; 0 until first asked. */
	uint32_t mean = 0;
	bool loadAttempted = false;
	bool usable = false;
};

/** @brief File id per aura, matching the delivered pack exactly. */
struct AuraFile {
	Skill skill;
	const char *id;
};

constexpr std::array<AuraFile, 59> AuraFiles { {
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
    // Bard songs (batch 8, 2026-09-11) - held through the same GetActiveClassAura, so they ring alike
    { Skill::MelodyOfLife, "melody_of_life" },
    { Skill::BattleHymn, "battle_hymn" },
    { Skill::SongOfSwiftness, "song_of_swiftness" },
    { Skill::SongOfFortitude, "song_of_fortitude" },
    { Skill::DirgeOfDread, "dirge_of_dread" },
    { Skill::Discord, "discord" },
    { Skill::Inspiration, "inspiration" },
    { Skill::TaleOfHeroes, "tale_of_heroes" },
    { Skill::Weaken, "weaken" },
    // Monk
    { Skill::HealingMantra, "healing_mantra" },
    // RfA-12's sixteen Paladin auras and eleven Bard songs, ringed by RfA-15 (batch 33, 2026-09-13).
    // Until then they burned with no ring at all - a missing file is only a verbose log line.
    { Skill::Valor, "valor" },
    { Skill::Radiance, "radiance" },
    { Skill::BaneOfEvil, "bane_of_evil" },
    { Skill::Condemnation, "condemnation" },
    { Skill::TitheOfAsh, "tithe_of_ash" },
    { Skill::Retaliation, "retaliation" },
    { Skill::DoomProcession, "doom_procession" },
    { Skill::Dominion, "dominion" },
    { Skill::Steadfast, "steadfast" },
    { Skill::ResistMagic, "resist_magic" },
    { Skill::Immovable, "immovable" },
    { Skill::WardingLight, "warding_light" },
    { Skill::Mercy, "mercy" },
    { Skill::AuraOfProtection, "aura_of_protection" },
    { Skill::Endurance, "endurance" },
    { Skill::Sanctity, "sanctity" },
    { Skill::MinstrelsTune, "minstrels_tune" },
    { Skill::BalladOfResilience, "ballad_of_resilience" },
    { Skill::HuntersChant, "hunters_chant" },
    { Skill::SerenadeOfSteel, "serenade_of_steel" },
    { Skill::SongOfPlenty, "song_of_plenty" },
    { Skill::Nocturne, "nocturne" },
    { Skill::AnthemOfValor, "anthem_of_valor" },
    { Skill::SirensCall, "sirens_call" },
    { Skill::HymnOfRenewal, "hymn_of_renewal" },
    { Skill::SymphonyOfWar, "symphony_of_war" },
    { Skill::SovereignMeasure, "sovereign_measure" },
    // The Sorceress's two auras (2026-09-14) - rings requested in RfA-16; until delivered they burn without one.
    { Skill::StaticField, "static_field" },
    { Skill::ThunderStorm, "thunder_storm" },
} };

std::array<AuraArt, AuraFiles.size()> Art;

/**
 * @brief The palette snapshot the art was quantised against.
 *
 * Only entries 128-255 matter: the lower half is redefined per level type and colour-cycled, which
 * is precisely why the brief confined the art to the upper half. If those 128 ever change the
 * quantisation is stale: IndexGeneration moves, and each ring's index is redone on its next indexed
 * draw. The loaded art itself stays (user, 2026-10-10: true-colour migration - it used to be dropped
 * and reloaded from the archive on every front-end <-> level switch).
 */
std::array<SDL_Color, 128> QuantisedAgainst {};
bool HaveQuantised = false;
uint32_t IndexGeneration = 1;

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

	art.index.clear(); // the indexed copy waits for an indexed draw - EnsureAuraIndex
	art.indexGeneration = 0;
	art.alpha.assign(static_cast<size_t>(ArtWidth) * ArtHeight, 0);
	art.rgb.assign(static_cast<size_t>(ArtWidth) * ArtHeight, 0);
	art.angle.assign(static_cast<size_t>(ArtWidth) * ArtHeight, 0);
	const auto *pixels = static_cast<const uint8_t *>(rgba->pixels);
	for (int y = 0; y < ArtHeight; y++) {
		const uint8_t *row = pixels + static_cast<size_t>(y) * rgba->pitch;
		for (int x = 0; x < ArtWidth; x++) {
			const uint8_t a = row[x * 4 + 3];
			const size_t at = static_cast<size_t>(y) * ArtWidth + x;
			if (a == 0)
				continue;
			art.alpha[at] = a;
			art.rgb[at] = PackArgb(0, row[x * 4 + 0], row[x * 4 + 1], row[x * 4 + 2]);
			// Measured on the ellipse's own circle: the art is twice as wide as it is tall, so y counts double.
			const double turn = (std::atan2(2.0 * (y - ArtHeight / 2 + 0.5), x - ArtWidth / 2 + 0.5) + 3.14159265358979) / (2.0 * 3.14159265358979);
			art.angle[at] = static_cast<uint8_t>(std::min(255, static_cast<int>(turn * 256.0)));
		}
	}
	SDL_FreeSurface(rgba);
	art.usable = true;
}

/**
 * @brief Stales every ring's indexed copy when the palette's global half has moved. Nothing else: the
 * art, its alpha, its true colour and its angles do not depend on the palette (user, 2026-10-10:
 * true-colour migration). Name kept for its six callers.
 */
void DropQuantisationIfPaletteMoved()
{
	if (HaveQuantised && std::memcmp(QuantisedAgainst.data(), &orig_palette[128], sizeof(QuantisedAgainst)) == 0)
		return;
	if (HaveQuantised)
		IndexGeneration++;
	std::memcpy(QuantisedAgainst.data(), &orig_palette[128], sizeof(QuantisedAgainst));
	HaveQuantised = true;
}

/** @brief The ring quantised to the shared palette half, built (or rebuilt) for an indexed draw. */
void EnsureAuraIndex(const AuraArt &art)
{
	if (art.indexGeneration == IndexGeneration && art.index.size() == art.rgb.size())
		return;
	art.index.assign(art.rgb.size(), 0);
	std::vector<uint8_t> cache(1 << 15, 0);
	for (size_t at = 0; at < art.rgb.size(); at++) {
		if (art.alpha[at] == 0)
			continue;
		const uint32_t px = art.rgb[at];
		art.index[at] = NearestSharedPaletteIndex(static_cast<uint8_t>(px >> 16), static_cast<uint8_t>(px >> 8), static_cast<uint8_t>(px), cache);
	}
	art.indexGeneration = IndexGeneration;
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
 * @brief The colour cycle (user, 2026-09-27: "can we add colorcylcing to all aura rings to animate them a bit?"): bands of
 * light run round the ring - AuraCycleCrests of them, one lap every AuraCycleMs - brightening the art's own colours as
 * they pass and dimming them between. The sheet's bars cycle the same way along their length.
 */
constexpr int AuraCycleMs = 2400;
constexpr int AuraCycleCrests = 2;
constexpr double AuraCycleSwing = 0.35;

/** @brief The cycle's factor for each of the 256 angles at this moment, in 256ths (256 = the art's own colour). */
std::array<int, 256> AuraCycleFactors()
{
	const int ms = AuraRingClockOverrideMs >= 0 ? AuraRingClockOverrideMs : static_cast<int>(SDL_GetTicks());
	const double phase = static_cast<double>(ms % AuraCycleMs) / AuraCycleMs;
	std::array<int, 256> factors {};
	for (int a = 0; a < 256; a++) {
		// crests * (angle - phase): every crest travels a whole lap per AuraCycleMs, so the pattern repeats every
		// AuraCycleMs / crests. (crests * angle - phase, the first version, moved them only 1/crests of a lap per cycle.)
		const double wave = std::cos(2.0 * 3.14159265358979 * AuraCycleCrests * (a / 256.0 - phase));
		factors[a] = static_cast<int>(256.0 * (1.0 + AuraCycleSwing * wave));
	}
	return factors;
}

/**
 * @brief How wide the ring is DRAWN, in HALF-tiles across. Two tiles, or three once well invested.
 *
 * DIAMETER, not radius, and in half-tiles so "three tiles across" is expressible without a fraction.
 * The previous version of this returned a radius of 2-3 tiles, which is a 4-6 tile ring - twice what
 * was wanted, and then half again as much once the projection factor had been applied to it (user,
 * 2026-08-27: "shrink auras. they are not 2 tiles in diameter"). They were not; they were about
 * five and a half.
 *
 * Separate from AuraRadiusForPoints, which is what the aura actually reaches - see the note at the
 * call site for why the two parted company. Points still change the ring, so investment is still
 * visible; they change it by a tile rather than by five.
 */
int AuraVisualDiameterHalfTiles(int points)
{
	// ONE SIZE, and the smallest that still reads as a ring (user, 2026-08-30: "i dont want aura
	// assets to grow in size with level bumps. keep the circle assets as small as possible. even
	// now i find it too large. lets try to shrink it even more, if possible").
	//
	// Both halves of that are deliberate reversals of what this function was doing. It returned 4
	// half-tiles below five points and 6 at or above - so investment grew the ring, which was the
	// last trace of the old "the circle shows the aura's reach" idea. That idea is already gone
	// (see the call site): the ring is a MARK ON THE CHARACTER, and a mark that changes size as you
	// spend points is just a mark that is sometimes wrong about a reach it no longer shows.
	//
	// Three half-tiles is one and a half tiles across - narrower than the character's own sprite, so
	// it reads as standing IN a circle rather than under a wash. Two would be a smudge under the
	// feet; this is the floor.
	if (points <= 0)
		return 0;
	return 3;
}

/**
 * @brief @p own (XRGB) taken to @p hue at its own brightness - missile_tint.cpp's Hue rule (dark stays dark, the brightest
 * parts run on toward white) - for a ring drawn in another colour than its art's.
 */
uint32_t RingColourInHue(uint32_t own, uint32_t hue)
{
	const double r = (own >> 16) & 0xFF;
	const double g = (own >> 8) & 0xFF;
	const double b = own & 0xFF;
	const double luma = (0.299 * r + 0.587 * g + 0.114 * b) / 255.0;
	const double k = luma * 1.6;
	const double white = std::max(0.0, luma - 0.7) * 1.6 * 255.0;
	const auto ch = [&](int shift) {
		return static_cast<uint32_t>(std::clamp(((hue >> shift) & 0xFF) * k + white, 0.0, 255.0)) << shift;
	};
	return ch(16) | ch(8) | ch(0);
}

// ---- The rings' glow (user, 2026-10-12: "can we now add glowing effect on aura rings? can they light the floor with their
// asset color while colorcycling?") ----------------------------------------------------------------------------------------

/** A ring drawn this frame: where (in the whole buffer's pixels), how big, which art in which hue. */
struct GlowRing {
	Point centre;
	int w, h, pulse;
	const AuraArt *art;
	uint32_t hue;
};
std::vector<GlowRing> GlowRings;

void NoteRingForGlow(const Surface &out, const AuraArt &art, Point centre, int dstW, int dstH, int pulsePercent, uint32_t hue)
{
	if (!*sgOptions.Oracool.auraGlow || GlowRings.size() >= 24)
		return;
	GlowRings.push_back({ centre + Displacement { out.region.x, out.region.y }, dstW, dstH, pulsePercent, &art, hue });
}

/** The ring's colour on the whole: its pixels' alpha-weighted mean, lifted to full brightness. */
uint32_t MeanColour(const AuraArt &art)
{
	if (art.mean != 0)
		return art.mean;
	uint64_t r = 0, g = 0, b = 0, n = 0;
	for (size_t i = 0; i < art.rgb.size(); i++) {
		const uint32_t a = art.alpha[i];
		if (a == 0)
			continue;
		r += ((art.rgb[i] >> 16) & 0xFF) * a;
		g += ((art.rgb[i] >> 8) & 0xFF) * a;
		b += (art.rgb[i] & 0xFF) * a;
		n += a;
	}
	if (n == 0)
		return 0;
	const double mr = static_cast<double>(r) / n, mg = static_cast<double>(g) / n, mb = static_cast<double>(b) / n;
	const double hi = std::max({ mr, mg, mb, 1.0 });
	const auto c = [&](double v) { return static_cast<uint32_t>(std::clamp(v / hi * 255.0, 0.0, 255.0)); };
	const_cast<AuraArt &>(art).mean = (c(mr) << 16) | (c(mg) << 8) | c(mb) | 0x01000000U; // the top byte: "worked out"
	return art.mean;
}

/** Light added at one pixel: onto the picture, or to the graphics card's splats. */
void AddLight(const Surface &out, std::vector<Splat> *splats, int x, int y, int r, int g, int b)
{
	if (x < 0 || y < 0 || x >= out.w() || y >= out.h() || (r | g | b) <= 0)
		return;
	r = std::min(r, 255);
	g = std::min(g, 255);
	b = std::min(b, 255);
	if (splats != nullptr) {
		splats->push_back({ static_cast<float>(x), static_cast<float>(y), r / 255.0F, g / 255.0F, b / 255.0F, 0.0F });
		return;
	}
	uint32_t &c = *out.at<uint32_t>(x, y);
	c = (static_cast<uint32_t>(std::min(255, static_cast<int>((c >> 16) & 0xFF) + r)) << 16)
	    | (static_cast<uint32_t>(std::min(255, static_cast<int>((c >> 8) & 0xFF) + g)) << 8)
	    | static_cast<uint32_t>(std::min(255, static_cast<int>(c & 0xFF) + b));
}

/** A soft pool of light on the floor, (1 - d2)^2, @p rx by @p ry, in @p rgb at @p k. */
void LightPool(const Surface &out, std::vector<Splat> *splats, int cx, int cy, int rx, int ry, uint32_t rgb, float k)
{
	if (rx <= 0 || ry <= 0 || k <= 0.004F)
		return;
	const int kr = static_cast<int>(((rgb >> 16) & 0xFF) * k), kg = static_cast<int>(((rgb >> 8) & 0xFF) * k), kb = static_cast<int>((rgb & 0xFF) * k);
	for (int dy = -ry; dy <= ry; dy++) {
		const float fy = static_cast<float>(dy) / ry;
		for (int dx = -rx; dx <= rx; dx++) {
			const float fx = static_cast<float>(dx) / rx;
			const float d2 = fx * fx + fy * fy;
			if (d2 >= 1.0F)
				continue;
			const int f = static_cast<int>((1.0F - d2) * (1.0F - d2) * 256.0F);
			AddLight(out, splats, cx + dx, cy + dy, (kr * f) >> 8, (kg * f) >> 8, (kb * f) >> 8);
		}
	}
}

/**
 * @brief Blits the ring @p dstW x @p dstH pixels, centred on @p centre; @p hue 0 keeps the art's colours, anything else
 * draws it in that hue (32-bit screen only).
 */
void BlitAuraPixels(const Surface &out, const AuraArt &art, Point centre, int dstW, int dstH, int pulsePercent, uint32_t hue)
{
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

	// Renderer stage 2 (v1.11): on the 32-bit screen the ring is composited with its real alpha in
	// its own colours - no palette match, no dither. The 75% ceiling is the one the two-blend path
	// below has always had, so the ring sits on the floor at the same weight it did.
	if (!out.isIndexed() && !art.rgb.empty()) {
		NoteRingForGlow(out, art, centre, dstW, dstH, pulsePercent, hue);
		const std::array<int, 256> cycle = AuraCycleFactors();
		for (int dy = dyFrom; dy < dyTo; dy++) {
			const int y = top + dy;
			const int sy = dy * ArtHeight / dstH;
			uint32_t *dstRow = out.at<uint32_t>(0, y);
			for (int dx = dxFrom; dx < dxTo; dx++) {
				const int x = left + dx;
				const int sx = dx * ArtWidth / dstW;
				const size_t at = static_cast<size_t>(sy) * ArtWidth + sx;
				const int factor = art.angle.empty() ? 256 : cycle[art.angle[at]];
				// The crest is brighter AND denser, so it reads on a dark floor as well as a lit one.
				const int coverage = std::min(255, art.alpha[at] * pulsePercent / 100 * factor / 256);
				if (coverage == 0)
					continue;
				const uint32_t own = hue == 0 ? art.rgb[at] : RingColourInHue(art.rgb[at], hue);
				const auto channel = [&](int shift) { return static_cast<uint32_t>(std::min(255, static_cast<int>((own >> shift) & 0xFF) * factor / 256)) << shift; };
				const uint32_t lit = channel(16) | channel(8) | channel(0);
				dstRow[x] = CompositeArgbOver(lit | (static_cast<uint32_t>(coverage) << 24), dstRow[x], 75);
			}
		}
		return;
	}

	EnsureAuraIndex(art); // the indexed copy is built only here (2026-10-10: true-colour migration)
	for (int dy = dyFrom; dy < dyTo; dy++) {
		const int y = top + dy;
		const int sy = dy * ArtHeight / dstH;
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

			const uint8_t src = art.index[at];
			SetHalfTransparentPixelUnchecked(out, { x, y }, src); // format-aware (v1.11)
			if (blends > 1)
				SetHalfTransparentPixelUnchecked(out, { x, y }, src);
		}
	}
}

/**
 * @brief Blits the aura @p diameterHalfTiles half-tiles across, centred on @p centre.
 *
 * Nearest-neighbour sampled: the source is a soft gradient with no hard edges to alias, and the
 * shrink is never more than half, so a filtered sample would cost more than it showed.
 */
void BlitAura(const Surface &out, const AuraArt &art, Point centre, int diameterHalfTiles, int pulsePercent)
{
	// The ellipse, stated directly. One tile step on screen is (TILE_WIDTH/2, TILE_HEIGHT/2), so a
	// ring N tiles across is N*TILE_WIDTH wide and N*TILE_HEIGHT tall - and in half-tiles that is
	// the same expression without the factor of two.
	//
	// A two-tile ring is 128x64, so the 512px source is shrunk to a quarter. That is well inside
	// what nearest-neighbour handles on an all-gradient source, and the dither is applied in
	// destination space so it stays fine-grained at any scale.
	BlitAuraPixels(out, art, centre, diameterHalfTiles * (TILE_WIDTH / 2), diameterHalfTiles * (TILE_HEIGHT / 2), pulsePercent, 0);
}

/**
 * Frenzy of the Dead's ring (the Necromancer Skill Cards page, 2026-10-03: "orange aura ring under every minion for the
 * duration of the spell. Colour cycled aura ring. Small scale - 50% of paladin aura rings. Choose one proper aura ring and
 * tint orange"): Holy Fire's ring of flames, drawn in FrenzyOrange at half the Paladin's ring, with the rings' cycle.
 */
constexpr Skill FrenzyRingArt = Skill::HolyFire;
/** Fend's ring (the Rogue Skill Cards page, 2026-10-04: "expanding aura ring with 1 tile radius ... orange-ish"): Might's ring. */
constexpr Skill FendRingArt = Skill::Might;

/** @brief The frenzy ring's art, loaded on first use; null while it is missing. */
const AuraArt *FrenzyRing()
{
	const int slot = IndexOfSkill(FrenzyRingArt);
	if (slot < 0)
		return nullptr;
	DropQuantisationIfPaletteMoved();
	if (!Art[slot].loadAttempted)
		LoadAura(slot);
	return Art[slot].usable ? &Art[slot] : nullptr;
}

} // namespace

int AuraRingClockOverrideMs = -1;

void BeginAuraGlowFrame()
{
	GlowRings.clear();
}

size_t AuraGlowRingCount()
{
	return GlowRings.size();
}

void DrawAuraGlow(const Surface &out)
{
	if (GlowRings.empty() || out.isIndexed() || out.w() <= 0 || out.h() <= 0)
		return;
	std::vector<Splat> *splats = SplatCollection();
	const double seconds = static_cast<double>(AuraRingClockOverrideMs >= 0 ? AuraRingClockOverrideMs : static_cast<int>(SDL_GetTicks())) / 1000.0;
	for (size_t n = 0; n < GlowRings.size(); n++) {
		const GlowRing &ring = GlowRings[n];
		const AuraArt &art = *ring.art;
		const Point centre = ring.centre - Displacement { out.region.x, out.region.y };
		if (centre.x < -ring.w * 2 || centre.y < -ring.h * 2 || centre.x > out.w() + ring.w * 2 || centre.y > out.h() + ring.h * 2)
			continue;
		const uint32_t mean = ring.hue == 0 ? MeanColour(art) & 0xFFFFFF : RingColourInHue(MeanColour(art) & 0xFFFFFF, ring.hue);
		// A candle's life, not the colour cycle's bands (user, 2026-10-12: "i prefer steady glow under the entire circle of the
		// aura, independent of the colorcycling animation. we can animate the glow to give it a candle-like live-like
		// behaviour"): three incommensurate sines and a slow breath, its own phase per ring - no period to see, never a strobe.
		const double phi = static_cast<double>(art.rgb.size() % 97) + static_cast<double>(n) * 1.7;
		const double candle = 0.84 + 0.07 * std::sin(seconds * 7.3 + phi) + 0.04 * std::sin(seconds * 13.1 + phi * 2.1)
		    + 0.03 * std::sin(seconds * 23.7 + phi * 3.7) + 0.04 * std::sin(seconds * 1.1 + phi);
		const float k = static_cast<float>(candle) * static_cast<float>(ring.pulse) / 100.0F;
		const int kr = static_cast<int>(((mean >> 16) & 0xFF) * k), kg = static_cast<int>(((mean >> 8) & 0xFF) * k),
		          kb = static_cast<int>((mean & 0xFF) * k);
		// The glow follows the whole circle: brightest on the ring's line, falling off a little inside and further outside,
		// with a faint fill over the floor it encloses. In the ring's own radii, so it fits any size.
		const float rx = ring.w * 0.5F, ry = ring.h * 0.5F;
		const int ex = static_cast<int>(rx * 1.7F), ey = static_cast<int>(ry * 1.7F);
		for (int dy = -ey; dy <= ey; dy++) {
			const int y = centre.y + dy;
			if (y < 0 || y >= out.h())
				continue;
			const float fy = static_cast<float>(dy) / ry;
			for (int dx = -ex; dx <= ex; dx++) {
				const int x = centre.x + dx;
				if (x < 0 || x >= out.w())
					continue;
				const float fx = static_cast<float>(dx) / rx;
				const float rho = std::sqrt(fx * fx + fy * fy); // 1 on the ring's line
				const float off = rho - 1.0F;
				float f = 0;
				if (off < 0) {
					const float in = 1.0F + off / 0.45F; // fades over the inner 45%
					f = (in > 0 ? in * in * 0.55F : 0.0F) + 0.10F * (1.0F - rho * rho);
				} else {
					const float outF = 1.0F - off / 0.7F; // and over 70% beyond
					f = outF > 0 ? outF * outF * 0.55F : 0.0F;
				}
				if (f <= 0.004F)
					continue;
				const int f8 = static_cast<int>(f * 256.0F);
				AddLight(out, splats, x, y, (kr * f8) >> 8, (kg * f8) >> 8, (kb * f8) >> 8);
			}
		}
		// The ring itself glowing a little, at its own colours and the same candle - steady, not in bands.
		const int left = centre.x - ring.w / 2;
		const int top = centre.y - ring.h / 2;
		const int candle256 = static_cast<int>(candle * 256.0);
		for (int dy = 0; dy < ring.h; dy++) {
			const int y = top + dy;
			if (y < 0 || y >= out.h())
				continue;
			const int sy = dy * ArtHeight / ring.h;
			for (int dx = 0; dx < ring.w; dx++) {
				const int x = left + dx;
				if (x < 0 || x >= out.w())
					continue;
				const size_t at = static_cast<size_t>(sy) * ArtWidth + static_cast<size_t>(dx * ArtWidth / ring.w);
				const int a = art.alpha[at];
				if (a == 0)
					continue;
				const uint32_t own = ring.hue == 0 ? art.rgb[at] : RingColourInHue(art.rgb[at], ring.hue);
				const int q = a * candle256 / 256 * ring.pulse / 100; // 0..~180
				AddLight(out, splats, x, y, static_cast<int>((own >> 16) & 0xFF) * q / 700, static_cast<int>((own >> 8) & 0xFF) * q / 700,
				    static_cast<int>(own & 0xFF) * q / 700);
			}
		}
	}
}

Size FrenzyRingSize()
{
	const int paladin = AuraVisualDiameterHalfTiles(1); // the Paladin's ring, in half-tiles across
	return { paladin * (TILE_WIDTH / 2) * FrenzyRingPercent / 100, paladin * (TILE_HEIGHT / 2) * FrenzyRingPercent / 100 };
}

bool DrawFrenzyRingPreview(const Surface &out, Point centre)
{
	const AuraArt *art = FrenzyRing();
	if (art == nullptr)
		return false;
	const Size size = FrenzyRingSize();
	BlitAuraPixels(out, *art, centre, size.width, size.height, 100, hue::FrenzyOrange);
	return true;
}

void DrawMinionFrenzyRings(const Surface &out, Point tilePosition, Point targetBufferPosition, int rows, int columns)
{
	if (leveltype == DTYPE_TOWN || MyPlayer == nullptr || !MyPlayer->isOnActiveLevel())
		return; // town's dMonster holds towners, and no army stands there
	const AuraArt *art = nullptr;
	const Size size = FrenzyRingSize();
	const int pulse = PulsePercent();
	// DrawFloor's walk again (see DrawAuraGround). At each tile, the body DrawMonsterHelper would draw from it: a monster
	// is drawn from its positive dMonster tile, or - walking sideways to the west - from the negative one, 64px back.
	for (int i = 0; i < rows; i++) {
		for (int j = 0; j < columns; j++) {
			if (InDungeonBounds(tilePosition) && dMonster[tilePosition.x][tilePosition.y] != 0 && IsTileLit(tilePosition)) {
				const int mi = dMonster[tilePosition.x][tilePosition.y];
				const size_t id = static_cast<size_t>(std::abs(mi) - 1);
				if (id < MaxMonsters) {
					const Monster &monster = Monsters[id];
					if ((monster.flags & MFLAG_HIDDEN) == 0 && IsMinionFrenzied(monster)) {
						Displacement offset {};
						bool drawnHere = mi > 0;
						if (monster.isWalking()) {
							const bool sideLeft = monster.mode == MonsterMode::MoveSideways && monster.direction == Direction::West;
							drawnHere = sideLeft ? mi < 0 : mi > 0;
							offset = GetOffsetForWalking(monster.animInfo, monster.direction);
							if (sideLeft)
								offset -= Displacement { 64, 0 };
						}
						if (drawnHere) {
							if (art == nullptr)
								art = FrenzyRing();
							if (art == nullptr)
								return;
							const Point centre = targetBufferPosition + Displacement { TILE_WIDTH / 2, -TILE_HEIGHT / 2 } + offset;
							BlitAuraPixels(out, *art, centre, size.width, size.height, pulse, hue::FrenzyOrange);
						}
					}
				}
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

Size ArrowRingSize(int ticks)
{
	const int t = std::clamp(ticks, 0, FallingArrowRingTicks - 1);
	const int percent = ArrowRingStartPercent + (ArrowRingEndPercent - ArrowRingStartPercent) * t / std::max(FallingArrowRingTicks - 1, 1);
	return { TILE_WIDTH * percent / 100, TILE_HEIGHT * percent / 100 };
}

namespace {

/** @brief Blessed Aim's ring art for the arrows' impacts, loaded on first use; null while it is missing. */
const AuraArt *ArrowRingArt()
{
	const int slot = IndexOfSkill(Skill::BlessedAim);
	if (slot < 0)
		return nullptr;
	DropQuantisationIfPaletteMoved();
	if (!Art[slot].loadAttempted)
		LoadAura(slot);
	return Art[slot].usable ? &Art[slot] : nullptr;
}

/** @brief One impact ring @p ticks after its arrow landed: opening out, bright as it strikes and gone at the end. */
void BlitArrowRing(const Surface &out, const AuraArt &art, Point centre, int ticks, uint32_t ringHue = hue::ArrowAmber)
{
	const Size size = ArrowRingSize(ticks);
	const int fade = 140 * (FallingArrowRingTicks - ticks) / FallingArrowRingTicks;
	BlitAuraPixels(out, art, centre, size.width, size.height, fade, ringHue);
}

} // namespace

bool DrawArrowRingPreview(const Surface &out, Point centre, int ticks, uint32_t ringHue)
{
	const AuraArt *art = ArrowRingArt();
	if (art == nullptr)
		return false;
	BlitArrowRing(out, *art, centre, ticks, ringHue != 0 ? ringHue : hue::ArrowAmber);
	return true;
}

void DrawArrowImpactRings(const Surface &out, Point tilePosition, Point targetBufferPosition, int rows, int columns)
{
	if (leveltype == DTYPE_TOWN || MyPlayer == nullptr || !MyPlayer->isOnActiveLevel())
		return;
	// The landed arrows first - a volley is sixteen, so the walk below asks a short list, not the missile pool per tile.
	struct Landed {
		Point tile;
		Displacement scatter;
		int ticks;
		uint32_t ringHue; // amber for Rain of Arrows, purple for Phantom Volley (2026-10-04)
	};
	std::vector<Landed> landed;
	for (const Missile &missile : Missiles) {
		const int ticks = FallingArrowLandedTicks(missile);
		if (ticks >= 0 && ticks < FallingArrowRingTicks)
			landed.push_back({ missile.position.tile, FallingArrowScatter(missile), ticks, FallingArrowRingHue(missile) });
	}
	if (landed.empty())
		return;
	const AuraArt *art = ArrowRingArt();
	if (art == nullptr)
		return;
	// DrawFloor's walk again (see DrawAuraGround).
	for (int i = 0; i < rows; i++) {
		for (int j = 0; j < columns; j++) {
			if (InDungeonBounds(tilePosition) && IsTileLit(tilePosition)) {
				for (const Landed &arrow : landed) {
					if (arrow.tile != tilePosition)
						continue;
					const Point centre = targetBufferPosition + Displacement { TILE_WIDTH / 2, -TILE_HEIGHT / 2 } + arrow.scatter;
					BlitArrowRing(out, *art, centre, arrow.ticks, arrow.ringHue);
				}
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

Size FendRingSize(double progress)
{
	const double p = std::clamp(progress, 0.0, 1.0);
	// Opens to a tile's radius: two tiles across, 128x64 on screen.
	return { std::max(1, static_cast<int>(2 * TILE_WIDTH * p)), std::max(1, static_cast<int>(2 * TILE_HEIGHT * p)) };
}

void DrawFendRing(const Surface &out, Point tilePosition, Point targetBufferPosition, int rows, int columns)
{
	if (leveltype == DTYPE_TOWN || MyPlayer == nullptr || !MyPlayer->isOnActiveLevel())
		return;
	const Player &player = *MyPlayer;
	const std::optional<ClassMeleeSkill> armed = ArmedClassMeleeSkill();
	if (player._pmode != PM_ATTACK || !armed.has_value() || *armed != ClassMeleeSkill::Fend || player.AnimInfo.numberOfFrames <= 0)
		return;
	const int slot = IndexOfSkill(FendRingArt);
	if (slot < 0)
		return;
	DropQuantisationIfPaletteMoved();
	if (!Art[slot].loadAttempted)
		LoadAura(slot);
	if (!Art[slot].usable)
		return;
	const double progress = static_cast<double>(player.AnimInfo.currentFrame + 1) / player.AnimInfo.numberOfFrames;
	const Size size = FendRingSize(progress);
	for (int i = 0; i < rows; i++) {
		for (int j = 0; j < columns; j++) {
			if (tilePosition == player.position.tile) {
				const Point centre = targetBufferPosition + Displacement { TILE_WIDTH / 2, -TILE_HEIGHT / 2 };
				BlitAuraPixels(out, Art[slot], centre, size.width, size.height, 120, hue::FendOrange);
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

bool DrawFendRingPreview(const Surface &out, Point centre, double progress)
{
	const int slot = IndexOfSkill(FendRingArt);
	if (slot < 0)
		return false;
	DropQuantisationIfPaletteMoved();
	if (!Art[slot].loadAttempted)
		LoadAura(slot);
	if (!Art[slot].usable)
		return false;
	const Size size = FendRingSize(progress);
	BlitAuraPixels(out, Art[slot], centre, size.width, size.height, 120, hue::FendOrange);
	return true;
}

bool DrawAuraRingPreview(const Surface &out, ClassTreeSkill aura, Point centre, int diameterHalfTiles)
{
	const int slot = IndexOfSkill(aura);
	if (slot < 0)
		return false;
	DropQuantisationIfPaletteMoved();
	if (!Art[slot].loadAttempted)
		LoadAura(slot);
	if (!Art[slot].usable)
		return false;
	BlitAura(out, Art[slot], centre, diameterHalfTiles, 100);
	return true;
}

const char *AuraRingFileId(ClassTreeSkill aura)
{
	const int slot = IndexOfSkill(aura);
	return slot < 0 ? nullptr : AuraFiles[slot].id;
}

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
	// The DRAWN size, which is deliberately no longer the aura's reach (user, 2026-08-27: "shrink
	// auras visual assets to 2-3 tile radius. now the aura graphics spans about 10 tile maybe").
	//
	// It did. The gameplay radius runs four to eight tiles, and the projection multiplied it by
	// sqrt(2) to reach the diagonal, so even a single point drew an ellipse about eleven tiles
	// across - a wash of colour under half the screen rather than a ring around the character.
	//
	// So the ring is now a MARK ON THE CHARACTER, not a map of the field. That is a real trade and
	// worth stating: a player can no longer read the aura's reach off the floor. It was not readable
	// before either - at eight tiles the ellipse covered everything already on screen, which is the
	// note AuraRadiusForPoints itself makes about why it caps there - so what is lost is the
	// appearance of information rather than information.
	//
	// The first attempt at that shrink read "2-3 tile radius" as a radius and kept the projection
	// factor, which together drew five and a half tiles across when two were asked for. It is a
	// DIAMETER now, stated in half-tiles and blitted at exactly that size.
	const int diameterHalfTiles = AuraVisualDiameterHalfTiles(points);
	if (diameterHalfTiles <= 0)
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
	//
	// THE TILE THE SPRITE IS DRAWN FROM, which is not always position.tile (user, 2026-08-28:
	// "walking my hero to 3 and 9 oclock moves the auras ring a bit behind him").
	//
	// Three walk shapes, three different answers, and only one of them was being asked. The engine
	// draws a player from the tile whose dPlayer entry is POSITIVE (see DrawDungeon), and the walk
	// helpers in player.cpp disagree about which tile that is:
	//
	//   WalkSouthwards - reassigns position.tile to the destination and marks it positive.
	//   WalkNorthwards - marks the DESTINATION negative, leaving position.tile positive.
	//   WalkSideways   - marks position.tile NEGATIVE and position.future positive, and never
	//                    reassigns position.tile.
	//
	// So for the two sideways directions - due East and due West, the 3 and 9 o'clock the report
	// names - the sprite renders from position.future while this anchored at position.tile, one
	// whole tile behind it. The other six directions were right by coincidence rather than by
	// agreement, which is why this asked the wrong question for months without showing it.
	//
	// Asking dPlayer directly is asking the same question the renderer answers, so the ring cannot
	// disagree with the sprite whatever a walk helper does next.
	const auto positiveHere = [&player](Point tile) {
		return InDungeonBounds(tile) && dPlayer[tile.x][tile.y] == static_cast<int8_t>(player.getId() + 1);
	};
	Point playerTile = player.position.tile;
	if (player.isWalking() && !positiveHere(playerTile) && positiveHere(player.position.future))
		playerTile = player.position.future;
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
				BlitAura(out, Art[slot], centre, diameterHalfTiles, PulsePercent());
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
