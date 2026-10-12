/**
 * @file oracool/drawn_fx.cpp
 *
 * Drawn skill effects (oracool/drawn_fx.h). Visual only; random numbers are hashed from a seed, so a moment draws the same
 * every time.
 */
#include "oracool/drawn_fx.h"

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

float Clamp01(float v)
{
	return v < 0 ? 0 : (v > 1 ? 1 : v);
}

struct Px {
	const Surface *out;
	int w, h;
	void add(int x, int y, float r, float g, float b, float k) const
	{
		if (x < 0 || y < 0 || x >= w || y >= h || k <= 0.004F)
			return;
		uint32_t &c = *out->at<uint32_t>(x, y);
		const int cr = std::min(255, static_cast<int>((c >> 16) & 0xFF) + static_cast<int>(r * k));
		const int cg = std::min(255, static_cast<int>((c >> 8) & 0xFF) + static_cast<int>(g * k));
		const int cb = std::min(255, static_cast<int>(c & 0xFF) + static_cast<int>(b * k));
		c = (static_cast<uint32_t>(cr) << 16) | (static_cast<uint32_t>(cg) << 8) | static_cast<uint32_t>(cb);
	}
	void mix(int x, int y, float r, float g, float b, float a) const
	{
		if (x < 0 || y < 0 || x >= w || y >= h || a <= 0.004F)
			return;
		a = std::min(a, 1.0F);
		uint32_t &c = *out->at<uint32_t>(x, y);
		const float cr = static_cast<float>((c >> 16) & 0xFF), cg = static_cast<float>((c >> 8) & 0xFF), cb = static_cast<float>(c & 0xFF);
		c = (static_cast<uint32_t>(cr + (r - cr) * a) << 16) | (static_cast<uint32_t>(cg + (g - cg) * a) << 8) | static_cast<uint32_t>(cb + (b - cb) * a);
	}
};

constexpr float ReachX = 110.0F; // the shockwave's farthest, across; half that down (the floor's diamond)

/** The air bending in a band at @p r across, @p amp pixels at most (the lava haze's ripple, once). */
void Ripple(const Surface &out, Point c, float r, float amp)
{
	if (amp < 0.2F)
		return;
	static std::vector<uint32_t> copy;
	const float band = 14.0F;
	const int x0 = std::max(0, static_cast<int>(static_cast<float>(c.x) - r - band)), x1 = std::min(out.w() - 1, static_cast<int>(static_cast<float>(c.x) + r + band));
	const int y0 = std::max(0, static_cast<int>(static_cast<float>(c.y) - (r + band) * 0.5F - 4)), y1 = std::min(out.h() - 1, static_cast<int>(static_cast<float>(c.y) + (r + band) * 0.5F + 4));
	if (x1 <= x0 || y1 <= y0)
		return;
	const int W = x1 - x0 + 1, H = y1 - y0 + 1;
	copy.resize(static_cast<size_t>(W) * H);
	for (int y = 0; y < H; y++)
		std::memcpy(&copy[static_cast<size_t>(y) * W], out.at<uint32_t>(x0, y0 + y), static_cast<size_t>(W) * 4);
	for (int y = 0; y < H; y++) {
		uint32_t *row = out.at<uint32_t>(x0, y0 + y);
		const float dy = static_cast<float>(y0 + y - c.y) * 2.0F;
		for (int x = 0; x < W; x++) {
			const float dx = static_cast<float>(x0 + x - c.x);
			const float d = std::sqrt(dx * dx + dy * dy) + 0.001F;
			const float off = d - r;
			if (off <= -band || off >= band)
				continue;
			const float push = std::sin(off / band * Pi) * amp * std::cos(off / band * Pi * 0.5F);
			int sx = x - static_cast<int>(dx / d * push), sy = y - static_cast<int>(dy / d * push * 0.5F);
			sx = std::clamp(sx, 0, W - 1);
			sy = std::clamp(sy, 0, H - 1);
			row[x] = copy[static_cast<size_t>(sy) * W + sx];
		}
	}
}

} // namespace

void DrawGroundStompFx(const Surface &out, Point foot, float age, bool floorLayer)
{
	if (out.isIndexed() || age < 0 || age > GroundStompSeconds)
		return;
	const Px px { &out, out.w(), out.h() };
	const float cx = static_cast<float>(foot.x), cy = static_cast<float>(foot.y);
	constexpr uint32_t Seed = 4021;
	// The shockwave: fast out, slowing - its radius by the square root of time over its first half second.
	const float waveU = Clamp01(age / 0.55F);
	const float waveR = ReachX * std::sqrt(waveU);
	if (floorLayer) {
		// Cracks: eight jagged splits running out from his feet in the first tenth of a second, white-hot, cooling through
		// orange to dark scars that fade with the rest.
		const float grow = Clamp01(age / 0.12F);
		const float heat = Clamp01(1.0F - age / 0.9F);
		const float scar = Clamp01(1.0F - (age - 0.9F) / 0.5F) * (age > 0.9F ? 1.0F : 1.0F);
		for (uint32_t i = 0; i < 8; i++) {
			float ang = (static_cast<float>(i) + Unit(Hash(i, Seed)) * 0.6F) / 8.0F * TwoPi;
			const float len = (55.0F + Unit(Hash(i, Seed + 1)) * 30.0F) * grow;
			float x = cx, y = cy;
			for (int s = 1; s <= 7; s++) {
				ang += (Unit(Hash(i, Seed + 2, static_cast<uint32_t>(s))) - 0.5F) * 0.8F;
				const float nx = cx + std::cos(ang) * len * static_cast<float>(s) / 7.0F;
				const float ny = cy + std::sin(ang) * len * static_cast<float>(s) / 7.0F * 0.5F;
				const int steps = static_cast<int>(std::max(std::abs(nx - x), std::abs(ny - y))) + 1;
				const float tip = static_cast<float>(s) / 7.0F; // the far end cools first and is thinner
				for (int j = 0; j <= steps; j++) {
					const float u = static_cast<float>(j) / static_cast<float>(steps);
					const int ix = static_cast<int>(x + (nx - x) * u), iy = static_cast<int>(y + (ny - y) * u);
					px.mix(ix, iy, 22, 14, 8, 0.85F * scar);
					if (tip < 0.75F)
						px.mix(ix + 1, iy, 30, 20, 12, 0.5F * scar);
					const float h = heat * (1.0F - tip * 0.6F);
					// White-hot at first, then orange, then out.
					px.add(ix, iy, 255, 140 + 100 * h, 40 + 120 * h * h, h);
					px.add(ix, iy - 1, 255, 110, 30, h * 0.45F);
					px.add(ix, iy + 1, 255, 110, 30, h * 0.45F);
				}
				x = nx;
				y = ny;
			}
		}
		// The flash's pool under his feet.
		const float flash = age < 0.04F ? age / 0.04F : std::pow(Clamp01(1.0F - (age - 0.04F) / 0.45F), 1.4F);
		for (int dy = -36; dy <= 36; dy++)
			for (int dx = -72; dx <= 72; dx++) {
				const float d2 = (static_cast<float>(dx) * static_cast<float>(dx)) / (72.0F * 72.0F) + (static_cast<float>(dy) * static_cast<float>(dy)) / (36.0F * 36.0F);
				if (d2 < 1.0F)
					px.add(static_cast<int>(cx) + dx, static_cast<int>(cy) + dy, 255, 215, 150, (1.0F - d2) * (1.0F - d2) * 0.8F * flash);
			}
		// The shockwave: a ring of dusty light on the floor, a darker band of raised dust just inside it.
		if (waveU < 1.0F || age < 0.75F) {
			const float fade = Clamp01(1.0F - (age - 0.1F) / 0.65F);
			const int samples = std::max(64, static_cast<int>(waveR * 7.0F)); // enough that the line is solid at any size
			for (int s = 0; s < samples; s++) {
				const float a = static_cast<float>(s) / static_cast<float>(samples) * TwoPi;
				const float wob = 1.0F + 0.03F * std::sin(a * 5.0F + 1.3F);
				const int x = static_cast<int>(cx + std::cos(a) * waveR * wob), y = static_cast<int>(cy + std::sin(a) * waveR * wob * 0.5F);
				px.add(x, y, 250, 225, 170, 0.55F * fade);
				px.add(x, y - 1, 240, 200, 140, 0.3F * fade);
				px.add(x, y + 1, 240, 200, 140, 0.3F * fade);
				px.add(x, y - 2, 230, 180, 110, 0.12F * fade);
				const int ix = static_cast<int>(cx + std::cos(a) * waveR * 0.85F * wob), iy = static_cast<int>(cy + std::sin(a) * waveR * 0.85F * wob * 0.5F);
				px.mix(ix, iy, 120, 104, 84, 0.35F * fade);
				px.mix(ix, iy + 1, 120, 104, 84, 0.25F * fade);
			}
		}
		// The air bending with the wave.
		Ripple(out, foot, waveR, 5.0F * Clamp01(1.0F - waveU));
		return;
	}
	// ---- In the air ----
	// The flash's core at his feet.
	const float core = age < 0.04F ? age / 0.04F : Clamp01(1.0F - (age - 0.04F) / 0.18F);
	for (int dy = -10; dy <= 10; dy++)
		for (int dx = -18; dx <= 18; dx++) {
			const float d2 = static_cast<float>(dx * dx) / 324.0F + static_cast<float>(dy * dy) / 100.0F;
			if (d2 < 1.0F)
				px.add(static_cast<int>(cx) + dx, static_cast<int>(cy) - 4 + dy, 255, 245, 220, (1.0F - d2) * 0.9F * core);
		}
	// Stone chips: thrown up and out, falling back, bouncing once.
	for (uint32_t i = 0; i < 22; i++) {
		const float a = Unit(Hash(i, Seed + 10)) * TwoPi;
		const float out_ = 40.0F + Unit(Hash(i, Seed + 11)) * 60.0F;
		const float up = 80.0F + Unit(Hash(i, Seed + 12)) * 90.0F;
		float t = age;
		float z = up * t - 330.0F * t * t;
		float reach = out_ * t;
		const float land = up / 165.0F;
		if (t > land) { // the bounce: a third of the speed, then still
			const float tb = t - land;
			const float up2 = up * 0.3F;
			z = std::max(0.0F, up2 * tb - 330.0F * tb * tb);
			reach = out_ * land + out_ * 0.3F * std::min(tb, up2 / 165.0F);
		}
		const float x = cx + std::cos(a) * reach, y = cy + std::sin(a) * reach * 0.5F - z;
		const float alpha = Clamp01(1.0F - (age - 1.0F) / 0.4F);
		// A chip of stone: lit on top, shadowed below, so it reads on a dark floor; and its shadow on the floor under it.
		const int size = 2 + static_cast<int>(Hash(i, Seed + 13) % 2);
		const float fx = cx + std::cos(a) * reach, fy = cy + std::sin(a) * reach * 0.5F;
		if (z > 1.0F)
			for (int ox = -size; ox <= size; ox++)
				px.mix(static_cast<int>(fx) + ox, static_cast<int>(fy), 10, 8, 6, 0.35F * alpha);
		for (int oy = 0; oy <= size; oy++)
			for (int ox = 0; ox <= size; ox++) {
				const float lit = oy == 0 ? 1.0F : (oy == size ? 0.45F : 0.75F);
				px.mix(static_cast<int>(x) + ox, static_cast<int>(y) + oy, 168 * lit, 150 * lit, 120 * lit, 0.95F * alpha);
			}
	}
	// Dust clouds rolling out behind the wave and settling.
	for (uint32_t i = 0; i < 16; i++) {
		const float a = (static_cast<float>(i) + Unit(Hash(i, Seed + 20))) / 16.0F * TwoPi;
		const float lag = 0.75F + Unit(Hash(i, Seed + 21)) * 0.15F;
		const float r = waveR * lag;
		const float life = Clamp01(age / 1.2F);
		const float size = 5.0F + 11.0F * life;
		const float alpha = 0.45F * Clamp01(age / 0.1F) * (1.0F - life);
		const float x = cx + std::cos(a) * r, y = cy + std::sin(a) * r * 0.5F - 6.0F - 10.0F * life;
		const int s = static_cast<int>(size);
		for (int dy = -s; dy <= s; dy++)
			for (int dx = -s; dx <= s; dx++) {
				const float d2 = static_cast<float>(dx * dx + dy * dy) / (size * size);
				if (d2 < 1.0F)
					px.mix(static_cast<int>(x) + dx, static_cast<int>(y) + dy, 132, 118, 98, alpha * (1.0F - d2));
			}
	}
}

} // namespace devilution::oracool
