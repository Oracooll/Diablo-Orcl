/**
 * @file oracool/living_aura.cpp
 *
 * Living auras (oracool/living_aura.h). Visual only; every random number is hashed from the clock and a seed, so a moment
 * draws the same every time.
 */
#include "oracool/living_aura.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

#include "engine/surface.hpp"

#ifdef _MSC_VER
#pragma runtime_checks("", off)
#pragma optimize("gt", on)
#endif

namespace devilution::oracool {

namespace {

constexpr float TwoPi = 6.2831853F;
constexpr float Pi = 3.14159265F;

uint32_t Hash(uint32_t a, uint32_t b, uint32_t c = 0)
{
	uint32_t h = a * 0x9E3779B1U ^ (b + 0x7F4A7C15U) * 0x85EBCA77U ^ (c + 0x165667B1U) * 0xC2B2AE3DU;
	h ^= h >> 15;
	h *= 0x2C1B3C6DU;
	h ^= h >> 12;
	h *= 0x297A2D39U;
	h ^= h >> 15;
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

/** Smooth 1D noise, 0..1. */
float Noise(float x, uint32_t seed)
{
	const float i = std::floor(x), f = x - i;
	const float a = Unit(Hash(static_cast<uint32_t>(static_cast<int>(i)), seed)), b = Unit(Hash(static_cast<uint32_t>(static_cast<int>(i) + 1), seed));
	const float s = f * f * (3.0F - 2.0F * f);
	return a + (b - a) * s;
}

struct Pen {
	const Surface *out;
	int w, h;
	void add(int x, int y, float r, float g, float b) const
	{
		if (x < 0 || y < 0 || x >= w || y >= h)
			return;
		uint32_t &c = *out->at<uint32_t>(x, y);
		const int cr = std::min(255, static_cast<int>((c >> 16) & 0xFF) + static_cast<int>(r));
		const int cg = std::min(255, static_cast<int>((c >> 8) & 0xFF) + static_cast<int>(g));
		const int cb = std::min(255, static_cast<int>(c & 0xFF) + static_cast<int>(b));
		c = (static_cast<uint32_t>(cr) << 16) | (static_cast<uint32_t>(cg) << 8) | static_cast<uint32_t>(cb);
	}
	void addK(int x, int y, const float rgb[3], float k) const
	{
		if (k > 0.004F)
			add(x, y, rgb[0] * k, rgb[1] * k, rgb[2] * k);
	}
};

/** The ring's soft band of light on the floor: brightest on the ring's line, @p width (of the radius) either side. */
void Band(const Pen &pen, Point c, int rx, int ry, const float rgb[3], float k, float width, float wobbleT, uint32_t seed)
{
	const int ex = static_cast<int>(static_cast<float>(rx) * (1.0F + width) + 2), ey = static_cast<int>(static_cast<float>(ry) * (1.0F + width) + 2);
	for (int dy = -ey; dy <= ey; dy++) {
		const float fy = static_cast<float>(dy) / static_cast<float>(ry);
		for (int dx = -ex; dx <= ex; dx++) {
			const float fx = static_cast<float>(dx) / static_cast<float>(rx);
			const float rho = std::sqrt(fx * fx + fy * fy);
			float d = std::abs(rho - 1.0F) / width;
			if (d >= 1.0F)
				continue;
			// The band breathes unevenly round the ring, so it never reads as a flat stamped circle.
			const float ang = std::atan2(fy, fx);
			const float live = 0.7F + 0.3F * Noise(ang * 3.0F + wobbleT, seed);
			const float f = (1.0F - d) * (1.0F - d) * live;
			pen.addK(c.x + dx, c.y + dy, rgb, f * k);
		}
	}
}

/** A tapered stroke of light from (@p x, @p y) up @p len pixels, swaying, hot at its root and cooling to its tip. */
void Tongue(const Pen &pen, float x, float y, float len, float sway, float k)
{
	static const float Hot[3] = { 255, 236, 160 }, Warm[3] = { 255, 140, 40 }, Tip[3] = { 200, 40, 10 };
	const int steps = std::max(2, static_cast<int>(len));
	for (int i = 0; i < steps; i++) {
		const float t = static_cast<float>(i) / static_cast<float>(steps); // 0 root .. 1 tip
		const float px = x + sway * t * t * len * 0.35F;
		const float py = y - t * len;
		const float wdt = 2.6F * (1.0F - t) + 0.4F;
		const float *rgb = t < 0.3F ? Hot : (t < 0.7F ? Warm : Tip);
		const float kk = k * (1.0F - t * 0.7F);
		for (int o = -2; o <= 2; o++) {
			const float e = 1.0F - std::abs(static_cast<float>(o)) / (wdt + 0.5F);
			if (e > 0)
				pen.addK(static_cast<int>(px) + o, static_cast<int>(py), rgb, e * kk);
		}
	}
}

void HolyFire(const Pen &pen, Point c, int rx, int ry, float t)
{
	static const float Floor[3] = { 255, 110, 30 };
	Band(pen, c, rx, ry, Floor, 0.55F, 0.28F, t * 1.3F, 11);
	// Flame tongues round the ring, each flickering on its own: height, sway and brightness from noise in time.
	constexpr int Tongues = 34;
	for (int i = 0; i < Tongues; i++) {
		const float a = static_cast<float>(i) / static_cast<float>(Tongues) * TwoPi + 0.07F * std::sin(t * 0.6F + static_cast<float>(i));
		const float x = static_cast<float>(c.x) + std::cos(a) * static_cast<float>(rx);
		const float y = static_cast<float>(c.y) + std::sin(a) * static_cast<float>(ry);
		const float n = Noise(t * 6.0F + static_cast<float>(i) * 1.7F, 21);
		const float len = 5.0F + 13.0F * n * n + 3.0F * Noise(t * 13.0F + static_cast<float>(i), 22);
		const float sway = (Noise(t * 3.0F + static_cast<float>(i) * 0.9F, 23) - 0.5F) * 1.6F;
		const float back = std::sin(a) < 0 ? 0.75F : 1.0F; // the far side a touch dimmer
		Tongue(pen, x, y, len, sway, (0.55F + 0.45F * n) * back);
	}
	// Embers rising off the ring.
	static const float Ember[3] = { 255, 170, 70 };
	for (uint32_t i = 0; i < 16; i++) {
		const float life = Frac(t * (0.5F + Unit(Hash(i, 31)) * 0.4F) + Unit(Hash(i, 32)));
		const float a = Unit(Hash(i, 33, static_cast<uint32_t>(t * (0.5F + Unit(Hash(i, 31)) * 0.4F) + Unit(Hash(i, 32))))) * TwoPi;
		const float x = static_cast<float>(c.x) + std::cos(a) * static_cast<float>(rx) + std::sin(t * 3.0F + static_cast<float>(i)) * 2.0F;
		const float y = static_cast<float>(c.y) + std::sin(a) * static_cast<float>(ry) - life * 34.0F;
		pen.addK(static_cast<int>(x), static_cast<int>(y), Ember, (1.0F - life) * 0.9F);
	}
}

void HolyFreeze(const Pen &pen, Point c, int rx, int ry, float t)
{
	static const float Cold[3] = { 120, 190, 255 }, Pale[3] = { 210, 240, 255 }, White[3] = { 255, 255, 255 };
	// A frost mist band, drifting slowly round.
	Band(pen, c, rx, ry, Cold, 0.5F, 0.32F, t * 0.6F, 41);
	// Ice crystals on the ring: small diamonds, each glinting now and then.
	constexpr int Crystals = 22;
	for (int i = 0; i < Crystals; i++) {
		const float a = static_cast<float>(i) / static_cast<float>(Crystals) * TwoPi + t * 0.15F;
		const float jitter = (Unit(Hash(static_cast<uint32_t>(i), 42)) - 0.5F) * 0.14F;
		const int x = c.x + static_cast<int>(std::cos(a) * static_cast<float>(rx) * (1.0F + jitter));
		const int y = c.y + static_cast<int>(std::sin(a) * static_cast<float>(ry) * (1.0F + jitter));
		const int size = 2 + static_cast<int>(Unit(Hash(static_cast<uint32_t>(i), 43)) * 2.0F);
		for (int dy = -size * 2; dy <= size * 2; dy++)
			for (int dx = -size; dx <= size; dx++) {
				const float d = std::abs(static_cast<float>(dx)) / static_cast<float>(size) + std::abs(static_cast<float>(dy)) / static_cast<float>(size * 2);
				if (d < 1.0F)
					pen.addK(x + dx, y + dy - size, Pale, (1.0F - d) * 0.55F);
			}
		const float glint = Frac(t * 0.45F + Unit(Hash(static_cast<uint32_t>(i), 44)));
		if (glint < 0.08F) {
			const float g = std::sin(glint / 0.08F * Pi);
			for (int s = 1; s <= 5; s++) {
				const float f = g * (1.0F - static_cast<float>(s) / 6.0F);
				pen.addK(x + s, y - size, White, f);
				pen.addK(x - s, y - size, White, f);
				pen.addK(x, y - size + s, White, f * 0.8F);
				pen.addK(x, y - size - s, White, f * 0.8F);
			}
		}
	}
	// Snow motes drifting up and round.
	for (uint32_t i = 0; i < 14; i++) {
		const float life = Frac(t * (0.25F + Unit(Hash(i, 45)) * 0.2F) + Unit(Hash(i, 46)));
		const float a = Unit(Hash(i, 47)) * TwoPi + life * 1.2F;
		const float x = static_cast<float>(c.x) + std::cos(a) * static_cast<float>(rx) * (0.9F + 0.2F * life);
		const float y = static_cast<float>(c.y) + std::sin(a) * static_cast<float>(ry) - life * 26.0F;
		pen.addK(static_cast<int>(x), static_cast<int>(y), White, std::sin(life * Pi) * 0.7F);
	}
}

void HolyShock(const Pen &pen, Point c, int rx, int ry, float t)
{
	static const float Blue[3] = { 120, 160, 255 }, Core[3] = { 235, 240, 255 };
	Band(pen, c, rx, ry, Blue, 0.4F, 0.16F, t * 2.0F, 61);
	// Arcs jumping along the ring: each lives a few frames, spans a stretch of it, jagged, and is replaced by another.
	const uint32_t frame = static_cast<uint32_t>(t * 14.0F);
	for (uint32_t i = 0; i < 4; i++) {
		const uint32_t s = Hash(frame, i, 62);
		if ((s & 3) == 0)
			continue; // not every slot every moment: it crackles
		const float a0 = Unit(s) * TwoPi, span = 0.5F + Unit(s >> 8) * 0.7F;
		float px = 0, py = 0;
		for (int k = 0; k <= 14; k++) {
			const float q = static_cast<float>(k) / 14.0F;
			const float a = a0 + span * q;
			const float jag = std::sin(q * Pi) * 6.0F * (Unit(Hash(s, static_cast<uint32_t>(k))) - 0.5F);
			const float x = static_cast<float>(c.x) + std::cos(a) * (static_cast<float>(rx) + jag);
			const float y = static_cast<float>(c.y) + std::sin(a) * (static_cast<float>(ry) + jag * 0.5F) - std::sin(q * Pi) * 6.0F;
			if (k > 0) {
				const int steps = static_cast<int>(std::max(std::abs(x - px), std::abs(y - py))) + 1;
				for (int j = 0; j <= steps; j++) {
					const float u = static_cast<float>(j) / static_cast<float>(steps);
					const int ix = static_cast<int>(px + (x - px) * u), iy = static_cast<int>(py + (y - py) * u);
					pen.addK(ix, iy, Core, 0.95F);
					pen.addK(ix + 1, iy, Blue, 0.5F);
					pen.addK(ix - 1, iy, Blue, 0.5F);
					pen.addK(ix, iy - 1, Blue, 0.35F);
				}
			}
			px = x;
			py = y;
		}
	}
	// Sparks spitting off.
	for (uint32_t i = 0; i < 10; i++) {
		const uint32_t s = Hash(frame, i, 63);
		const float a = Unit(s) * TwoPi;
		pen.addK(c.x + static_cast<int>(std::cos(a) * static_cast<float>(rx)), c.y + static_cast<int>(std::sin(a) * static_cast<float>(ry)) - static_cast<int>(Unit(s >> 8) * 10.0F), Core, 0.8F);
	}
}

void Might(const Pen &pen, Point c, int rx, int ry, float t)
{
	static const float Gold[3] = { 255, 196, 90 }, Hot[3] = { 255, 240, 190 };
	const float pulse = 0.75F + 0.25F * std::sin(t * 2.4F);
	Band(pen, c, rx, ry, Gold, 0.45F * pulse, 0.12F, t * 0.8F, 81);
	// An inner ring, fainter, turning the other way.
	Band(pen, c, rx * 3 / 4, ry * 3 / 4, Gold, 0.22F * pulse, 0.10F, -t * 0.8F, 82);
	// Rune marks between the two rings, turning slowly, each lighting up as a wave passes.
	constexpr int Runes = 12;
	for (int i = 0; i < Runes; i++) {
		const float a = static_cast<float>(i) / static_cast<float>(Runes) * TwoPi + t * 0.35F;
		const float wave = 0.4F + 0.6F * std::max(0.0F, std::cos(a - t * 2.0F));
		const float cx = static_cast<float>(c.x) + std::cos(a) * static_cast<float>(rx) * 0.875F;
		const float cy = static_cast<float>(c.y) + std::sin(a) * static_cast<float>(ry) * 0.875F;
		// A rune: a short upright stroke and one or two strokes across it, by its hash.
		const uint32_t h = Hash(static_cast<uint32_t>(i), 83);
		for (int k = -3; k <= 3; k++)
			pen.addK(static_cast<int>(cx), static_cast<int>(cy) + k, Hot, 0.75F * wave);
		for (int m = 0; m < 1 + static_cast<int>(h & 1); m++) {
			const int yy = static_cast<int>(cy) - 2 + m * 3;
			const int dir = ((h >> (m + 1)) & 1) ? 1 : -1;
			for (int k = 0; k <= 3; k++)
				pen.addK(static_cast<int>(cx) + k * dir, yy + k / 2, Hot, 0.6F * wave);
		}
	}
}

} // namespace

void DrawLivingAura(const Surface &out, Point centre, int rx, int ry, LivingAuraKind kind, float seconds)
{
	if (out.isIndexed() || rx <= 0 || ry <= 0)
		return;
	const Pen pen { &out, out.w(), out.h() };
	switch (kind) {
	case LivingAuraKind::HolyFire: HolyFire(pen, centre, rx, ry, seconds); break;
	case LivingAuraKind::HolyFreeze: HolyFreeze(pen, centre, rx, ry, seconds); break;
	case LivingAuraKind::HolyShock: HolyShock(pen, centre, rx, ry, seconds); break;
	case LivingAuraKind::Might: Might(pen, centre, rx, ry, seconds); break;
	}
}

} // namespace devilution::oracool
