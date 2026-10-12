/**
 * @file oracool/whirl_arcs.cpp
 *
 * Whirlwind's arcs (oracool/whirl_arcs.h). Visual only; its random numbers are hashed from the clock, so a frame draws
 * the same every time it is drawn.
 */
#include "oracool/whirl_arcs.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

#include "engine/surface.hpp"
#include "oracool/splats.h"

#ifdef _MSC_VER
#pragma runtime_checks("", off)
#pragma optimize("gt", on)
#endif

#define WA_R(c) (static_cast<int>(((c) >> 16) & 0xFF))
#define WA_G(c) (static_cast<int>(((c) >> 8) & 0xFF))
#define WA_B(c) (static_cast<int>((c) & 0xFF))
#define WA_PACK(r, g, b) ((static_cast<uint32_t>(r) << 16) | (static_cast<uint32_t>(g) << 8) | static_cast<uint32_t>(b))
#define WA_SAT(v) ((v) > 255 ? 255 : (v))

namespace devilution::oracool {

namespace {

constexpr float TwoPi = 6.2831853F;
constexpr float Pi = 3.14159265F;

uint32_t Hash(uint32_t a, uint32_t b, uint32_t c)
{
	uint32_t h = a * 0x9E3779B1U ^ (b + 0x7F4A7C15U) * 0x85EBCA77U ^ (c + 0x165667B1U) * 0xC2B2AE3DU;
	h ^= h >> 15;
	h *= 0x2C1B3C6DU;
	h ^= h >> 12;
	h *= 0x297A2D39U;
	h ^= h >> 15;
	return h;
}

float Signed(uint32_t h)
{
	return static_cast<float>(h & 0xFFFF) / 32767.5F - 1.0F;
}

struct Pen {
	const Surface *out;
	std::vector<Splat> *splats;
	int w, h;
	void add(int x, int y, int r, int g, int b) const
	{
		if (x < 0 || y < 0 || x >= w || y >= h || (r | g | b) <= 0)
			return;
		r = WA_SAT(r);
		g = WA_SAT(g);
		b = WA_SAT(b);
		if (splats != nullptr) {
			splats->push_back({ static_cast<float>(x), static_cast<float>(y), static_cast<float>(r) / 255.0F, static_cast<float>(g) / 255.0F,
			    static_cast<float>(b) / 255.0F, 0.0F });
			return;
		}
		uint32_t &c = *out->at<uint32_t>(x, y);
		c = WA_PACK(WA_SAT(WA_R(c) + r), WA_SAT(WA_G(c) + g), WA_SAT(WA_B(c) + b));
	}
	void addF(int x, int y, uint32_t rgb, float k) const
	{
		if (k <= 0.004F)
			return;
		add(x, y, static_cast<int>(static_cast<float>(WA_R(rgb)) * k), static_cast<int>(static_cast<float>(WA_G(rgb)) * k),
		    static_cast<int>(static_cast<float>(WA_B(rgb)) * k));
	}
};

// The lightning's three layers: a white-hot core, an orange glow round it, a deep ember haze outside that.
constexpr uint32_t Core = WA_PACK(255, 240, 190);
constexpr uint32_t Glow = WA_PACK(255, 140, 40);
constexpr uint32_t Haze = WA_PACK(190, 60, 8);

/** One pixel of a bolt at strength @p k: the core, its glow one pixel round, the haze two out. */
void Brush(const Pen &pen, int x, int y, float k)
{
	pen.addF(x, y, Core, k);
	pen.addF(x - 1, y, Glow, k * 0.55F);
	pen.addF(x + 1, y, Glow, k * 0.55F);
	pen.addF(x, y - 1, Glow, k * 0.55F);
	pen.addF(x, y + 1, Glow, k * 0.55F);
	pen.addF(x - 2, y, Haze, k * 0.22F);
	pen.addF(x + 2, y, Haze, k * 0.22F);
	pen.addF(x, y - 2, Haze, k * 0.22F);
	pen.addF(x, y + 2, Haze, k * 0.22F);
}

/** A straight piece of bolt from (@p x0, @p y0) to (@p x1, @p y1), its strength running from @p k0 to @p k1. */
void Segment(const Pen &pen, float x0, float y0, float x1, float y1, float k0, float k1)
{
	const float dx = x1 - x0, dy = y1 - y0;
	const int steps = static_cast<int>(std::max(std::abs(dx), std::abs(dy))) + 1;
	for (int i = 0; i <= steps; i++) {
		const float t = static_cast<float>(i) / static_cast<float>(steps);
		Brush(pen, static_cast<int>(std::lround(x0 + dx * t)), static_cast<int>(std::lround(y0 + dy * t)), k0 + (k1 - k0) * t);
	}
}

/** A soft pool of light on the floor, (1 - d2)^2 out to @p rx by @p ry. */
void FloorPool(const Pen &pen, int cx, int cy, int rx, int ry, uint32_t rgb, float k)
{
	for (int dy = -ry; dy <= ry; dy++) {
		const float fy = static_cast<float>(dy) / static_cast<float>(ry);
		for (int dx = -rx; dx <= rx; dx++) {
			const float fx = static_cast<float>(dx) / static_cast<float>(rx);
			const float d2 = fx * fx + fy * fy;
			if (d2 >= 1.0F)
				continue;
			pen.addF(cx + dx, cy + dy, rgb, k * (1.0F - d2) * (1.0F - d2));
		}
	}
}

} // namespace

void DrawWhirlArcs(const Surface &out, Point foot, float seconds, bool front, bool floor, bool low)
{
	if (out.isIndexed() || out.w() <= 0 || out.h() <= 0)
		return;
	const Pen pen { &out, SplatCollection(), out.w(), out.h() };
	// The lightning's shape changes 24 times a second; between, it holds, as lightning does.
	const uint32_t frame = static_cast<uint32_t>(seconds * 24.0F);
	const float flicker = 0.8F + 0.2F * Signed(Hash(frame, 7, 1));

	if (floor && front)
		FloorPool(pen, foot.x, foot.y, 64, 32, Glow, 0.26F * flicker);

	constexpr int Arcs = 3;
	constexpr int Pieces = 16;
	for (int i = 0; i < Arcs; i++) {
		// Each arc rides its own ring: waist, chest, head - turning the way he spins, the higher the faster.
		// Low (the strafing Rogue, user 2026-10-12): the rings at her knees and hips, wider, round her rather than above her.
		const float height = (low ? (i == 0 ? 6.0F : (i == 1 ? 14.0F : 24.0F)) : (i == 0 ? 22.0F : (i == 1 ? 42.0F : 62.0F))) + (low ? 2.0F : 4.0F) * std::sin(seconds * 2.7F + static_cast<float>(i) * 2.1F);
		const float rx = (low ? (i == 1 ? 50.0F : 44.0F) : (i == 1 ? 42.0F : 36.0F)) + 3.0F * std::sin(seconds * 3.3F + static_cast<float>(i));
		const float ry = rx * 0.38F;
		const float tilt = 7.0F * std::sin(static_cast<float>(i) * 1.7F + seconds * 0.9F); // the ring leans a little
		const float speed = 6.5F + static_cast<float>(i) * 1.3F;
		const float head = seconds * speed + static_cast<float>(i) * (TwoPi / 3.0F);
		const float span = 1.7F + 0.25F * std::sin(seconds * 4.1F + static_cast<float>(i) * 3.0F);
		// The arc's points: along the ring behind its head, each pushed in or out by its jag.
		float px[Pieces + 1], py[Pieces + 1], pa[Pieces + 1];
		for (int s = 0; s <= Pieces; s++) {
			const float t = static_cast<float>(s) / static_cast<float>(Pieces);
			const float a = head - t * span;
			const float jag = std::sin(t * Pi) * 4.5F * Signed(Hash(frame, static_cast<uint32_t>(i), static_cast<uint32_t>(s)));
			const float r = 1.0F + jag / rx;
			px[s] = static_cast<float>(foot.x) + std::cos(a) * rx * r;
			py[s] = static_cast<float>(foot.y) - height + std::sin(a) * ry * r + std::cos(a) * tilt + jag * 0.4F;
			pa[s] = a;
		}
		for (int s = 0; s < Pieces; s++) {
			// The near half is where the ring dips toward the camera (down the screen).
			const bool near = std::sin((pa[s] + pa[s + 1]) * 0.5F) > 0;
			if (near != front)
				continue;
			// Brightest at the head, fading along the tail.
			const float t0 = static_cast<float>(s) / static_cast<float>(Pieces);
			const float t1 = static_cast<float>(s + 1) / static_cast<float>(Pieces);
			const float k0 = (1.0F - t0 * 0.85F) * flicker * (near ? 1.0F : 0.6F);
			const float k1 = (1.0F - t1 * 0.85F) * flicker * (near ? 1.0F : 0.6F);
			Segment(pen, px[s], py[s], px[s + 1], py[s + 1], k0, k1);
			// Now and then a fork leaps off it, outward.
			const uint32_t f = Hash(frame, static_cast<uint32_t>(i) + 40, static_cast<uint32_t>(s));
			if ((f & 15) == 0) {
				const float a = pa[s];
				const float len = 7.0F + static_cast<float>((f >> 4) & 7);
				const float fx = px[s] + std::cos(a) * len + Signed(f >> 8) * 3.0F;
				const float fy = py[s] + std::sin(a) * len * 0.5F - 3.0F + Signed(f >> 16) * 3.0F;
				Segment(pen, px[s], py[s], fx, fy, k0 * 0.7F, 0.1F);
			}
		}
		// Sparks thrown off the head.
		const bool headNear = std::sin(head) > 0;
		if (headNear == front) {
			for (int k = 0; k < 4; k++) {
				const uint32_t h = Hash(frame, static_cast<uint32_t>(i) + 80, static_cast<uint32_t>(k));
				const int sx = static_cast<int>(px[0] + Signed(h) * 8.0F);
				const int sy = static_cast<int>(py[0] + Signed(h >> 16) * 5.0F);
				pen.addF(sx, sy, Core, 0.7F * flicker);
				pen.addF(sx + 1, sy, Glow, 0.35F * flicker);
			}
		}
	}
}

} // namespace devilution::oracool
