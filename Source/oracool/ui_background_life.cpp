/**
 * @file oracool/ui_background_life.cpp
 *
 * The front end's paintings alive (oracool/ui_background_life.h). Every spot is in the painting's own pixels; the frame maps
 * it to the screen through the crop the painting was drawn with.
 */
#include "oracool/ui_background_life.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>

#include "engine/surface.hpp"

#ifdef _MSC_VER
#pragma runtime_checks("", off)
#pragma optimize("gt", on)
#endif

namespace devilution::oracool {

namespace {

constexpr float Pi = 3.14159265F;
constexpr float TwoPi = 6.2831853F;

uint32_t Hash(uint32_t a, uint32_t b, uint32_t c = 0)
{
	uint32_t h = a * 0x9E3779B1U ^ (b + 0x7F4A7C15U) * 0x85EBCA77U ^ (c + 0x165667B1U) * 0xC2B2AE3DU;
	h ^= h >> 15;
	h *= 0x2C1B3C6DU;
	h ^= h >> 12;
	return h;
}

float Unit(uint32_t h)
{
	return static_cast<float>(h & 0xFFFFFF) / 16777216.0F;
}

float Frac(float v)
{
	return v - std::floor(v);
}

/**
 * The painting's pixels to the screen's: where the crop was drawn and how much larger, across and down; and the part of the
 * surface the painting covers, which nothing here draws outside (audit, v1.13.050).
 */
struct Map {
	float ox, oy, kx, ky; // screen = (art - crop.pos) * k + screen.pos
	int x0, y0, x1, y1;   // the clip, inclusive: the screen rect within the surface
	float X(float ax) const { return ox + ax * kx; }
	float Y(float ay) const { return oy + ay * ky; }
	bool In(int x, int y) const { return x >= x0 && y >= y0 && x <= x1 && y <= y1; }
};

/** A flicker from three sines: a flame's breath, never quite repeating. */
float Flicker(float t, float seed)
{
	return 0.5F + 0.22F * std::sin(t * 7.3F + seed) + 0.17F * std::sin(t * 13.1F + seed * 2.3F) + 0.11F * std::sin(t * 23.7F + seed * 0.7F);
}

void Add(const Surface &out, const Map &m, int x, int y, float r, float g, float b, float k)
{
	if (!m.In(x, y) || !(k > 0.004F))
		return;
	uint32_t &c = *out.at<uint32_t>(x, y);
	const int cr = std::min(255, static_cast<int>((c >> 16) & 0xFF) + static_cast<int>(r * k));
	const int cg = std::min(255, static_cast<int>((c >> 8) & 0xFF) + static_cast<int>(g * k));
	const int cb = std::min(255, static_cast<int>(c & 0xFF) + static_cast<int>(b * k));
	c = (c & 0xFF000000U) | (static_cast<uint32_t>(cr) << 16) | (static_cast<uint32_t>(cg) << 8) | static_cast<uint32_t>(cb);
}

void Mix(const Surface &out, const Map &m, int x, int y, float r, float g, float b, float a)
{
	if (!m.In(x, y) || !(a > 0.004F))
		return;
	a = std::min(a, 1.0F);
	uint32_t &c = *out.at<uint32_t>(x, y);
	const float cr = static_cast<float>((c >> 16) & 0xFF), cg = static_cast<float>((c >> 8) & 0xFF), cb = static_cast<float>(c & 0xFF);
	c = (c & 0xFF000000U) | (static_cast<uint32_t>(cr + (r - cr) * a) << 16) | (static_cast<uint32_t>(cg + (g - cg) * a) << 8) | static_cast<uint32_t>(cb + (b - cb) * a);
}

/**
 * Bends the screen inside the painting's rect (@p x0, @p y0)-(@p x1, @p y1): each pixel takes the colour @p shift says (in the
 * painting's pixels, from where it is - u, v across the whole rect 0..1, even where the screen cuts it), sampled from the rect
 * as it was. @p RowOnly: the shift depends on v alone, so it is asked once a row (audit, v1.13.050: the banners' cost).
 */
template <bool RowOnly = false, typename Shift>
void Warp(const Surface &out, const Map &m, float x0, float y0, float x1, float y1, Shift shift)
{
	const float fx0 = m.X(x0), fy0 = m.Y(y0), fw = m.X(x1) - fx0, fh = m.Y(y1) - fy0;
	if (!(fw > 1.0F) || !(fh > 1.0F))
		return;
	const int sx0 = std::max(m.x0, static_cast<int>(fx0)), sy0 = std::max(m.y0, static_cast<int>(fy0));
	const int sx1 = std::min(m.x1, static_cast<int>(m.X(x1))), sy1 = std::min(m.y1, static_cast<int>(m.Y(y1)));
	if (sx1 <= sx0 || sy1 <= sy0)
		return;
	const int W = sx1 - sx0 + 1, H = sy1 - sy0 + 1;
	static std::vector<uint32_t> copy;
	copy.resize(static_cast<size_t>(W) * H);
	for (int y = 0; y < H; y++)
		std::memcpy(&copy[static_cast<size_t>(y) * W], out.at<uint32_t>(sx0, sy0 + y), static_cast<size_t>(W) * 4);
	for (int y = 0; y < H; y++) {
		uint32_t *row = out.at<uint32_t>(sx0, sy0 + y);
		const float v = std::clamp((static_cast<float>(sy0 + y) - fy0) / fh, 0.0F, 1.0F);
		float dx = 0, dy = 0;
		if constexpr (RowOnly)
			shift(0.0F, v, dx, dy);
		for (int x = 0; x < W; x++) {
			if constexpr (!RowOnly) {
				const float u = std::clamp((static_cast<float>(sx0 + x) - fx0) / fw, 0.0F, 1.0F);
				dx = 0;
				dy = 0;
				shift(u, v, dx, dy);
			}
			const int tx = std::clamp(x + static_cast<int>(std::lround(dx * m.kx)), 0, W - 1);
			const int ty = std::clamp(y + static_cast<int>(std::lround(dy * m.ky)), 0, H - 1);
			row[x] = copy[static_cast<size_t>(ty) * W + tx];
		}
	}
}

/** A glow round (@p ax, @p ay): light added that falls off over @p rx by @p ry painting pixels. */
void Glow(const Surface &out, const Map &m, float ax, float ay, float rx, float ry, float r, float g, float b, float k)
{
	const float cx = m.X(ax), cy = m.Y(ay), Rx = rx * m.kx, Ry = ry * m.ky;
	if (!(Rx > 0.5F) || !(Ry > 0.5F))
		return;
	const int icx = static_cast<int>(cx), icy = static_cast<int>(cy);
	// Only the part on the painting's rect is walked (audit, v1.13.050).
	const int dy0 = std::max(-static_cast<int>(Ry), m.y0 - icy), dy1 = std::min(static_cast<int>(Ry), m.y1 - icy);
	const int dx0 = std::max(-static_cast<int>(Rx), m.x0 - icx), dx1 = std::min(static_cast<int>(Rx), m.x1 - icx);
	for (int dy = dy0; dy <= dy1; dy++)
		for (int dx = dx0; dx <= dx1; dx++) {
			const float d2 = static_cast<float>(dx * dx) / (Rx * Rx) + static_cast<float>(dy * dy) / (Ry * Ry);
			if (d2 < 1.0F)
				Add(out, m, static_cast<int>(cx) + dx, static_cast<int>(cy) + dy, r, g, b, k * (1.0F - d2) * (1.0F - d2));
		}
}

/** The bright pixels of a rect brightened by @p k (a window's light, a sigil's): only what already shines. */
void Brighten(const Surface &out, const Map &m, float x0, float y0, float x1, float y1, float r, float g, float b, float k)
{
	const int sx0 = std::max(m.x0, static_cast<int>(m.X(x0))), sy0 = std::max(m.y0, static_cast<int>(m.Y(y0)));
	const int sx1 = std::min(m.x1, static_cast<int>(m.X(x1))), sy1 = std::min(m.y1, static_cast<int>(m.Y(y1)));
	for (int y = sy0; y <= sy1; y++) {
		uint32_t *row = out.at<uint32_t>(0, y);
		for (int x = sx0; x <= sx1; x++) {
			const uint32_t c = row[x];
			const int l = (static_cast<int>((c >> 16) & 0xFF) * 77 + static_cast<int>((c >> 8) & 0xFF) * 151 + static_cast<int>(c & 0xFF) * 28) >> 8;
			if (l < 70)
				continue;
			Add(out, m, x, y, r, g, b, k * static_cast<float>(l - 70) / 185.0F);
		}
	}
}

/**
 * A painted flame (rect around it, its base at the bottom): no movement in the picture, only its light (user, 2026-10-13: "remove
 * wave effects. add just a glowing light in the flaming areas") - the flame's pixels brightening with its flicker and a soft glow
 * round it pulsing.
 */
void Flame(const Surface &out, const Map &m, float x0, float y0, float x1, float y1, float t, float seed, float glowR)
{
	const float f = Flicker(t, seed);
	const float h = y1 - y0;
	Brighten(out, m, x0, y0, x1, y1, 255, 170, 70, 0.22F * (f - 0.35F));
	Glow(out, m, (x0 + x1) * 0.5F, y1 - h * 0.35F, glowR, glowR * 0.75F, 255, 140, 50, 0.06F + 0.10F * f);
}

/** A banner hanging from its top edge, swaying a hair: nothing at the top, most at the bottom. */
void Banner(const Surface &out, const Map &m, float x0, float y0, float x1, float y1, float t, float seed, float amp)
{
	Warp</*RowOnly=*/true>(out, m, x0 - amp, y0, x1 + amp, y1, [&](float, float v, float &dx, float &dy) {
		const float hang = std::pow(v, 1.4F);
		// Barely moving (user, 2026-10-13: "make flags movement more subtle. barely moving"): one slow sway, no ripple.
		dx = hang * amp * std::sin(t * 0.55F + seed + v * 1.2F);
		dy = 0;
	});
}

/** A hanged body or a lamp on a chain, turning a little about the rope's top at (@p px, @p py). */
void Pendulum(const Surface &out, const Map &m, float x0, float y0, float x1, float y1, float px, float py, float t, float maxAngle)
{
	const float a = maxAngle * std::sin(t * 0.8F);
	const float ca = std::cos(a), sa = std::sin(a);
	Warp(out, m, x0, y0, x1, y1, [&](float u, float v, float &dx, float &dy) {
		const float ax = x0 + u * (x1 - x0) - px, ay = y0 + v * (y1 - y0) - py;
		// The point that turns onto here came from the other way round.
		dx = (ax * ca + ay * sa) - ax;
		dy = (-ax * sa + ay * ca) - ay;
	});
}

/** Smoke rising from (@p ax, @p ay): soft dark puffs drifting up and to the side, growing and thinning. */
void Smoke(const Surface &out, const Map &m, float ax, float ay, float t, float drift)
{
	for (uint32_t i = 0; i < 16; i++) {
		const float period = 5.0F;
		const float ph = Frac(t / period + static_cast<float>(i) / 16.0F);
		const float x = ax + drift * ph * 90.0F + std::sin(ph * 5.0F + static_cast<float>(i)) * 6.0F;
		const float y = ay - ph * 150.0F;
		const float size = (6.0F + 26.0F * ph) * m.kx;
		const float alpha = 0.22F * std::sin(std::min(1.0F, ph * 1.3F) * Pi) * (1.0F - ph * 0.5F);
		const int s = static_cast<int>(size);
		const int cx = static_cast<int>(m.X(x)), cy = static_cast<int>(m.Y(y));
		for (int dy = -s; dy <= s; dy++)
			for (int dx = -s; dx <= s; dx++) {
				const float d2 = static_cast<float>(dx * dx + dy * dy) / (size * size);
				if (d2 < 1.0F)
					Mix(out, m, cx + dx, cy + dy, 34, 34, 40, alpha * (1.0F - d2));
			}
	}
}

/** The blue portal's flame: no movement in the picture, only its light breathing and its ring's glow (user, 2026-10-13). */
void Portal(const Surface &out, const Map &m, float x0, float y0, float x1, float y1, float ringX, float ringY, float t)
{
	const float f = Flicker(t * 0.8F, 3.1F);
	Brighten(out, m, x0, y0, x1, y1, 90, 140, 255, 0.3F * (f - 0.3F));
	Glow(out, m, ringX, ringY, 120, 50, 70, 120, 255, 0.06F + 0.07F * f);
}

/** A small light in the distance (a window, a candle, a lantern): its glow breathing. */
void Lamp(const Surface &out, const Map &m, float ax, float ay, float r, float t, float seed, float k = 1.0F)
{
	Glow(out, m, ax, ay, r, r, 255, 170, 80, k * (0.05F + 0.09F * Flicker(t, seed)));
}

} // namespace

void AnimateUiBackground(const Surface &out, UiBackground which, const Rectangle &crop, const Rectangle &screen, float t)
{
	if (out.isIndexed() || crop.size.width <= 0 || crop.size.height <= 0 || screen.size.width <= 0 || screen.size.height <= 0)
		return;
	const float kx = static_cast<float>(screen.size.width) / static_cast<float>(crop.size.width);
	const float ky = static_cast<float>(screen.size.height) / static_cast<float>(crop.size.height);
	const Map m { static_cast<float>(screen.position.x) - static_cast<float>(crop.position.x) * kx, static_cast<float>(screen.position.y) - static_cast<float>(crop.position.y) * ky, kx, ky,
		std::max(0, screen.position.x), std::max(0, screen.position.y), std::min(out.w(), screen.position.x + screen.size.width) - 1, std::min(out.h(), screen.position.y + screen.size.height) - 1 };
	if (m.x1 < m.x0 || m.y1 < m.y0)
		return;
	switch (which) {
	case UiBackground::Settings: // the gallows road under the moon (1680x720)
		Glow(out, m, 1150, 140, 70, 70, 210, 220, 255, 0.04F + 0.02F * std::sin(t * 0.7F));
		Banner(out, m, 448, 190, 528, 352, t, 0.0F, 1.8F);
		Pendulum(out, m, 1350, 232, 1405, 372, 1378, 232, t, 0.012F);
		Flame(out, m, 484, 418, 512, 470, t, 1.0F, 90);
		Flame(out, m, 1360, 438, 1388, 480, t, 2.7F, 90);
		for (const auto &[x, y, s] : { std::array<float, 3> { 897, 392, 1 }, std::array<float, 3> { 1101, 402, 2 }, std::array<float, 3> { 1047, 400, 3 }, std::array<float, 3> { 1012, 413, 4 } })
			Lamp(out, m, x, y, 10, t, s, 0.8F);
		break;
	case UiBackground::HeroSelect: // the cathedral hall (1916x821)
		Banner(out, m, 192, 52, 268, 470, t, 0.0F, 1.5F);
		Banner(out, m, 375, 55, 470, 310, t, 1.3F, 1.2F);
		Banner(out, m, 1592, 70, 1690, 560, t, 2.1F, 1.8F);
		Banner(out, m, 1690, 0, 1790, 420, t, 3.4F, 1.5F);
		Banner(out, m, 1488, 385, 1585, 640, t, 4.2F, 1.2F);
		Brighten(out, m, 905, 55, 1005, 480, 255, 120, 60, 0.15F + 0.12F * Flicker(t * 0.6F, 5.0F)); // the window's fire
		Brighten(out, m, 760, 620, 1140, 760, 255, 220, 150, 0.06F + 0.05F * std::sin(t * 1.1F)); // the dais's sigil breathing
		Flame(out, m, 450, 468, 515, 565, t, 1.0F, 130);
		Flame(out, m, 732, 448, 769, 492, t, 2.2F, 80);
		Flame(out, m, 1145, 450, 1183, 494, t, 3.3F, 80);
		Flame(out, m, 1400, 468, 1465, 565, t, 4.4F, 130);
		break;
	case UiBackground::MainMenu: // the town with the forge and the blue portal (1916x821)
		Smoke(out, m, 497, 168, t, -1.0F);
		Flame(out, m, 562, 470, 606, 522, t, 1.0F, 110); // the forge
		Lamp(out, m, 742, 440, 22, t, 2.0F);
		Lamp(out, m, 1116, 428, 16, t, 3.0F);
		Lamp(out, m, 1299, 428, 16, t, 4.0F);
		Portal(out, m, 1262, 478, 1338, 640, 1300, 628, t);
		break;
	default: break;
	}
}

} // namespace devilution::oracool
