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

float Frac(float v)
{
	return v - std::floor(v);
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

void DrawSeismicSlamFx(const Surface &out, Point foot, int dirX, int dirY, int tiles, float age, bool floorLayer)
{
	if (out.isIndexed() || age < 0 || age > SeismicSlamSeconds || (dirX == 0 && dirY == 0))
		return;
	const Px px { &out, out.w(), out.h() };
	constexpr uint32_t Seed = 5077;
	// The path on screen: a tile step along the direction, and across it on the floor (the tile direction turned a quarter).
	const float stepX = static_cast<float>(dirX - dirY) * 32.0F, stepY = static_cast<float>(dirX + dirY) * 16.0F;
	const float acrossTx = static_cast<float>(-dirY), acrossTy = static_cast<float>(dirX);
	float crossX = (acrossTx - acrossTy) * 32.0F, crossY = (acrossTx + acrossTy) * 16.0F;
	const float crossLen = std::max(1.0F, std::sqrt(crossX * crossX + crossY * crossY));
	crossX /= crossLen;
	crossY /= crossLen;
	const float pathX = stepX * static_cast<float>(tiles), pathY = stepY * static_cast<float>(tiles);
	const float pathLen = std::sqrt(pathX * pathX + pathY * pathY);
	const float ux = pathX / pathLen, uy = pathY / pathLen;
	const float x0 = static_cast<float>(foot.x), y0 = static_cast<float>(foot.y);
	// Where the crest is: out fast, easing at the end.
	const float travel = Clamp01(age / SeismicTravelSeconds);
	const float s = 1.0F - (1.0F - travel) * (1.0F - travel);
	const float crestX = x0 + pathX * s, crestY = y0 + pathY * s;
	const bool rolling = age < SeismicTravelSeconds;
	const float endAge = age - SeismicTravelSeconds; // after the wave reached its end
	const float fadeAll = Clamp01(1.0F - (age - 1.1F) / 0.7F);
	// When the crest passed the point at distance d (0..1) along the path: s inverted.
	const auto passedAt = [](float d) { return SeismicTravelSeconds * (1.0F - std::sqrt(std::max(0.0F, 1.0F - d))); };
	if (floorLayer) {
		// The rupture: the main crack from his feet to the crest, zigzagging, and side cracks off it. Hot where the crest has
		// just been, cooled to a dark scar further back.
		constexpr int Pieces = 40;
		float lx = x0, ly = y0;
		for (int i = 1; i <= Pieces; i++) {
			const float d = static_cast<float>(i) / static_cast<float>(Pieces);
			if (d > s)
				break;
			// Meandering: two slow waves and a little jag, so it wanders like a split in stone rather than zigzagging evenly.
			const float zig = std::sin(d * 9.0F + 0.7F) * 7.0F + std::sin(d * 23.0F + 2.1F) * 3.0F + (Unit(Hash(static_cast<uint32_t>(i), Seed)) - 0.5F) * 4.0F;
			const float nx = x0 + pathX * d + crossX * zig, ny = y0 + pathY * d + crossY * zig;
			const float since = age - passedAt(d);
			const float heat = Clamp01(1.0F - since / 0.7F);
			const int steps = static_cast<int>(std::max(std::abs(nx - lx), std::abs(ny - ly))) + 1;
			// The thinner split running beside the main one, a few pixels off, in and out of it.
			const float twin = 6.0F + 3.0F * std::sin(d * 13.0F);
			for (int j = 0; j <= steps; j++) {
				const float u = static_cast<float>(j) / static_cast<float>(steps);
				const int tx = static_cast<int>(lx + (nx - lx) * u + crossX * twin), ty = static_cast<int>(ly + (ny - ly) * u + crossY * twin);
				px.mix(tx, ty, 24, 16, 9, 0.6F * fadeAll);
				px.add(tx, ty, 255, 120, 35, 0.55F * heat * fadeAll);
			}
			for (int j = 0; j <= steps; j++) {
				const float u = static_cast<float>(j) / static_cast<float>(steps);
				const int ix = static_cast<int>(lx + (nx - lx) * u), iy = static_cast<int>(ly + (ny - ly) * u);
				px.mix(ix, iy, 20, 12, 6, 0.9F * fadeAll);
				px.mix(ix + 1, iy, 26, 18, 10, 0.6F * fadeAll);
				px.mix(ix, iy + 1, 26, 18, 10, 0.4F * fadeAll);
				px.add(ix, iy, 255, 150 + 100 * heat, 50 + 140 * heat * heat, heat * fadeAll);
				px.add(ix, iy - 1, 255, 110, 30, 0.5F * heat * fadeAll);
				px.add(ix + 1, iy, 255, 110, 30, 0.4F * heat * fadeAll);
			}
			// A side crack every few pieces, branching off at an angle.
			if (i % 4 == 2) {
				const float side = (Hash(static_cast<uint32_t>(i), Seed + 1) & 1) ? 1.0F : -1.0F;
				const float blen = 14.0F + Unit(Hash(static_cast<uint32_t>(i), Seed + 2)) * 16.0F;
				float bx = nx, by = ny;
				for (int k = 1; k <= 4; k++) {
					const float q = static_cast<float>(k) / 4.0F;
					const float jx = (Unit(Hash(static_cast<uint32_t>(i), Seed + 3, static_cast<uint32_t>(k))) - 0.5F) * 4.0F;
					const float tx = nx + (crossX * side * 0.8F + ux * 0.6F) * blen * q + jx;
					const float ty = ny + (crossY * side * 0.8F + uy * 0.6F) * blen * q;
					const int bsteps = static_cast<int>(std::max(std::abs(tx - bx), std::abs(ty - by))) + 1;
					for (int j = 0; j <= bsteps; j++) {
						const float u = static_cast<float>(j) / static_cast<float>(bsteps);
						const int ix = static_cast<int>(bx + (tx - bx) * u), iy = static_cast<int>(by + (ty - by) * u);
						px.mix(ix, iy, 22, 14, 8, 0.75F * fadeAll * (1.0F - q * 0.5F));
						px.add(ix, iy, 255, 130, 40, heat * 0.7F * fadeAll * (1.0F - q));
					}
					bx = tx;
					by = ty;
				}
			}
			lx = nx;
			ly = ny;
		}
		// The slam's flash at his feet.
		const float slam = age < 0.04F ? age / 0.04F : std::pow(Clamp01(1.0F - (age - 0.04F) / 0.35F), 1.4F);
		for (int dy = -28; dy <= 28; dy++)
			for (int dx = -56; dx <= 56; dx++) {
				const float d2 = static_cast<float>(dx * dx) / 3136.0F + static_cast<float>(dy * dy) / 784.0F;
				if (d2 < 1.0F)
					px.add(foot.x + dx, foot.y + dy, 255, 215, 150, (1.0F - d2) * (1.0F - d2) * 0.75F * slam);
			}
		// The crest's light on the floor: a band of dusty gold across the path, and the air bending round it.
		if (rolling) {
			for (int c = -46; c <= 46; c++) {
				const float e = 1.0F - std::abs(static_cast<float>(c)) / 47.0F;
				for (int b = -9; b <= 9; b++) {
					const float f = e * (1.0F - std::abs(static_cast<float>(b)) / 10.0F) * 1.6F;
					px.add(static_cast<int>(crestX + crossX * static_cast<float>(c) - ux * static_cast<float>(b)),
					    static_cast<int>(crestY + crossY * static_cast<float>(c) - uy * static_cast<float>(b)), 250, 210, 140, 0.45F * f);
				}
			}
			Ripple(out, { static_cast<int>(crestX), static_cast<int>(crestY) }, 30.0F, 4.5F);
		}
		// The end: a burst where the wave dies - a flash and a short ring.
		if (endAge >= 0 && endAge < 0.5F) {
			const float env = endAge < 0.04F ? endAge / 0.04F : Clamp01(1.0F - (endAge - 0.04F) / 0.4F);
			const float ex = x0 + pathX, ey = y0 + pathY;
			for (int dy = -26; dy <= 26; dy++)
				for (int dx = -52; dx <= 52; dx++) {
					const float d2 = static_cast<float>(dx * dx) / 2704.0F + static_cast<float>(dy * dy) / 676.0F;
					if (d2 < 1.0F)
						px.add(static_cast<int>(ex) + dx, static_cast<int>(ey) + dy, 255, 205, 130, (1.0F - d2) * (1.0F - d2) * 0.7F * env);
				}
			const float R = 60.0F * std::sqrt(Clamp01(endAge / 0.4F));
			for (int k = 0; k < 360; k++) {
				const float a = static_cast<float>(k) / 360.0F * TwoPi;
				px.add(static_cast<int>(ex + std::cos(a) * R), static_cast<int>(ey + std::sin(a) * R * 0.5F), 250, 220, 160, 0.6F * env);
			}
			Ripple(out, { static_cast<int>(ex), static_cast<int>(ey) }, R, 4.0F * env);
		}
		return;
	}
	// ---- In the air ----
	// The ridge: stone blocks across the crest, heaving up as it reaches them and sinking back behind.
	if (rolling || age < SeismicTravelSeconds + 0.25F) {
		const float sink = rolling ? 1.0F : Clamp01(1.0F - endAge / 0.25F);
		// Each block heaves with its own beat as the crest rolls, so the front churns rather than sliding as one.
		const float churn = age * 18.0F;
		for (int b = -4; b <= 4; b++) {
			const float jitter = (Unit(Hash(static_cast<uint32_t>(b + 10), Seed + 20)) - 0.5F) * 6.0F;
			// A front running up the screen stacks its blocks one above the next: there they stand lower and further apart.
			const float spread = 10.0F + 6.0F * std::abs(crossY);
			const float bx = crestX + crossX * (static_cast<float>(b) * spread + jitter);
			const float by = crestY + crossY * (static_cast<float>(b) * spread + jitter);
			const float beat = 0.75F + 0.25F * std::sin(churn + static_cast<float>(b) * 1.9F);
			const float rise = (14.0F + 12.0F * Unit(Hash(static_cast<uint32_t>(b + 10), Seed + 21))) * (1.0F - std::abs(static_cast<float>(b)) / 6.0F) * sink * beat * (0.45F + 0.55F * std::abs(crossX));
			const int half = 5 + static_cast<int>(Unit(Hash(static_cast<uint32_t>(b + 10), Seed + 22)) * 3.0F);
			for (int y = 0; y <= static_cast<int>(rise); y++) {
				const float t = static_cast<float>(y) / std::max(1.0F, rise);
				const int hw = static_cast<int>(static_cast<float>(half) * (1.0F - 0.45F * t)); // narrower at the top
				const float lit = t > 0.8F ? 1.0F : 0.55F + 0.3F * t;
				for (int x = -hw; x <= hw; x++) {
					const float side = x < 0 ? 0.85F : 1.0F;
					px.mix(static_cast<int>(bx) + x, static_cast<int>(by) - y, 150 * lit * side, 132 * lit * side, 104 * lit * side, 0.95F);
				}
			}
			px.add(static_cast<int>(bx), static_cast<int>(by - rise) - 1, 255, 220, 150, 0.4F * sink);
		}
	}
	// The dust curtain riding the crest: dense clouds boiling up across the front.
	if (rolling) {
		for (int c = 0; c < 7; c++) {
			const float off = (static_cast<float>(c) - 3.0F) * 13.0F;
			const float boil = 0.5F + 0.5F * std::sin(age * 14.0F + static_cast<float>(c) * 2.3F);
			const float size = 8.0F + 5.0F * boil;
			const float cx = crestX + crossX * off - ux * 6.0F, cy = crestY + crossY * off - uy * 6.0F - 10.0F - 6.0F * boil;
			const int r = static_cast<int>(size);
			for (int dy = -r; dy <= r; dy++)
				for (int dx = -r; dx <= r; dx++) {
					const float d2 = static_cast<float>(dx * dx + dy * dy) / (size * size);
					if (d2 < 1.0F)
						px.mix(static_cast<int>(cx) + dx, static_cast<int>(cy) + dy, 150, 132, 108, 0.5F * (1.0F - d2) * (1.0F - std::abs(off) / 50.0F));
				}
		}
	}
	// Stone chips thrown up along the path as the crest passes, falling and bouncing.
	constexpr int Chips = 36;
	for (int i = 0; i < Chips; i++) {
		const float d = (static_cast<float>(i) + Unit(Hash(static_cast<uint32_t>(i), Seed + 30))) / static_cast<float>(Chips);
		const float t = age - passedAt(d);
		if (t < 0)
			continue;
		const float side = (Unit(Hash(static_cast<uint32_t>(i), Seed + 31)) - 0.5F) * 2.0F;
		const float out_ = 25.0F + Unit(Hash(static_cast<uint32_t>(i), Seed + 32)) * 35.0F;
		const float up = 70.0F + Unit(Hash(static_cast<uint32_t>(i), Seed + 33)) * 80.0F;
		const float land = up / 165.0F;
		float z, reach;
		if (t <= land) {
			z = up * t - 330.0F * t * t;
			reach = out_ * t;
		} else {
			const float tb = t - land, up2 = up * 0.3F;
			z = std::max(0.0F, up2 * tb - 330.0F * tb * tb);
			reach = out_ * land + out_ * 0.3F * std::min(tb, up2 / 165.0F);
		}
		const float fx = x0 + pathX * d + crossX * side * reach + ux * reach * 0.3F, fy = y0 + pathY * d + crossY * side * reach + uy * reach * 0.3F;
		const float alpha = Clamp01(1.0F - (t - 0.9F) / 0.4F) * fadeAll;
		const int size = 1 + static_cast<int>(Hash(static_cast<uint32_t>(i), Seed + 34) % 3);
		if (z > 1.0F)
			for (int ox = -size; ox <= size; ox++)
				px.mix(static_cast<int>(fx) + ox, static_cast<int>(fy), 10, 8, 6, 0.3F * alpha);
		for (int oy = 0; oy <= size; oy++)
			for (int ox = 0; ox <= size; ox++) {
				const float lit = oy == 0 ? 1.0F : (oy == size ? 0.45F : 0.75F);
				px.mix(static_cast<int>(fx) + ox, static_cast<int>(fy - z) + oy, 168 * lit, 150 * lit, 120 * lit, 0.95F * alpha);
			}
	}
	// Dust rising along the path behind the crest and drifting off.
	constexpr int Clouds = 14;
	for (int i = 0; i < Clouds; i++) {
		const float d = (static_cast<float>(i) + 0.5F) / static_cast<float>(Clouds);
		const float t = age - passedAt(d);
		if (t < 0)
			continue;
		const float life = Clamp01(t / 1.1F);
		const float size = 5.0F + 12.0F * life;
		const float alpha = 0.45F * Clamp01(t / 0.08F) * (1.0F - life);
		const float side = (Unit(Hash(static_cast<uint32_t>(i), Seed + 40)) - 0.5F) * 30.0F;
		const float cx = x0 + pathX * d + crossX * side, cy = y0 + pathY * d + crossY * side - 6.0F - 12.0F * life;
		const int r = static_cast<int>(size);
		for (int dy = -r; dy <= r; dy++)
			for (int dx = -r; dx <= r; dx++) {
				const float d2 = static_cast<float>(dx * dx + dy * dy) / (size * size);
				if (d2 < 1.0F)
					px.mix(static_cast<int>(cx) + dx, static_cast<int>(cy) + dy, 132, 118, 98, alpha * (1.0F - d2));
			}
	}
}

namespace {

/** A line of crack from (x0, y0) to (x1, y1): a dark split with molten light in it at @p heat; @p wide for the main ones. */
void CrackLine(const Px &px, float x0, float y0, float x1, float y1, float heat, float scar, bool wide)
{
	const int steps = static_cast<int>(std::max(std::abs(x1 - x0), std::abs(y1 - y0))) + 1;
	for (int j = 0; j <= steps; j++) {
		const float u = static_cast<float>(j) / static_cast<float>(steps);
		const int ix = static_cast<int>(x0 + (x1 - x0) * u), iy = static_cast<int>(y0 + (y1 - y0) * u);
		px.mix(ix, iy, 18, 10, 5, 0.9F * scar);
		if (wide) {
			px.mix(ix + 1, iy, 24, 15, 8, 0.7F * scar);
			px.mix(ix, iy + 1, 28, 18, 10, 0.5F * scar);
		}
		px.add(ix, iy, 255, 120 + 110 * heat, 30 + 120 * heat * heat, heat);
		px.add(ix, iy - 1, 255, 100, 25, 0.45F * heat);
		if (wide)
			px.add(ix + 1, iy, 255, 100, 25, 0.45F * heat);
	}
}

/** How far into its pulse the quake is: the last pulse's time (0..3), or -1 before the first. */
float LastPulse(float age)
{
	if (age < 0)
		return -1;
	return std::min(std::floor(age), static_cast<float>(EarthquakePulses - 1));
}

/** How long the quake's rocks fall, the speed they hit with and how they roll. */
constexpr int QuakeRocks = 13;
constexpr float RockFallSeconds = 0.5F;
constexpr float RockDropHeight = 260.0F;

/** Rock @p i of the quake: when it drops, where it lands, how big, which way and how far it rolls. */
struct QuakeRock {
	float drop;   // seconds into the quake it starts to fall
	float x, y;   // where it lands, from the centre (pixels)
	float radius; // pixels
	float rollX, rollY, rollDist;
	uint32_t shape;
};

QuakeRock RockOf(uint32_t i)
{
	constexpr uint32_t Seed = 7301;
	QuakeRock r;
	// Spread over the four seconds, about three a second, never two at the same moment.
	r.drop = 0.15F + (static_cast<float>(i) + 0.6F * Unit(Hash(i, Seed))) * (3.6F / static_cast<float>(QuakeRocks));
	const float a = Unit(Hash(i, Seed + 1)) * TwoPi, d = 0.25F + 0.85F * std::sqrt(Unit(Hash(i, Seed + 2)));
	r.x = std::cos(a) * 140.0F * d;
	r.y = std::sin(a) * 70.0F * d;
	r.radius = 4.0F + Unit(Hash(i, Seed + 3)) * 5.0F;
	const float ra = Unit(Hash(i, Seed + 4)) * TwoPi;
	r.rollX = std::cos(ra);
	r.rollY = std::sin(ra) * 0.5F;
	r.rollDist = 14.0F + Unit(Hash(i, Seed + 5)) * 22.0F;
	r.shape = Hash(i, Seed + 6);
	return r;
}

/** How far the rock has rolled @p t seconds after landing: out quickly, slowing to rest over 0.7 s. */
float RockRolled(const QuakeRock &r, float t)
{
	const float u = Clamp01(t / 0.7F);
	return r.rollDist * (1.0F - (1.0F - u) * (1.0F - u));
}

/** A rock: an irregular round shape of @p radius turned by @p turn, lit from above, shaded below and on its dark side. */
void DrawRock(const Px &px, float cx, float cy, float radius, float turn, uint32_t shape, float alpha)
{
	// Its outline: eight lobes of its own sizes, turning with it.
	float lobe[8];
	for (int k = 0; k < 8; k++)
		lobe[k] = 0.72F + 0.28F * Unit(Hash(shape, static_cast<uint32_t>(k)));
	const int R = static_cast<int>(radius) + 1;
	for (int dy = -R; dy <= R; dy++)
		for (int dx = -R; dx <= R; dx++) {
			// Squashed a little: a stone lying on the floor seen from above at an angle.
			const float fx = static_cast<float>(dx), fy = static_cast<float>(dy) * 1.25F;
			const float d = std::sqrt(fx * fx + fy * fy);
			float a = std::atan2(fy, fx) - turn;
			while (a < 0)
				a += TwoPi;
			const float pos = a / TwoPi * 8.0F;
			const int k0 = static_cast<int>(pos) % 8, k1 = (k0 + 1) % 8;
			const float f = pos - std::floor(pos);
			const float edge = radius * (lobe[k0] + (lobe[k1] - lobe[k0]) * f);
			if (d > edge)
				continue;
			// Light from above and a little left: the top bright, the bottom-right dark, a rim darker still.
			const float nx = fx / std::max(edge, 1.0F), ny = fy / std::max(edge, 1.0F);
			float lit = 0.62F - 0.42F * ny - 0.18F * nx;
			if (d > edge - 1.0F)
				lit *= 0.6F;
			// A little grain, fixed to the stone so it turns with it.
			const float gx = std::cos(turn) * fx + std::sin(turn) * fy, gy = -std::sin(turn) * fx + std::cos(turn) * fy;
			lit *= 0.9F + 0.2F * Unit(Hash(shape, static_cast<uint32_t>(static_cast<int>(gx + 20.0F)), static_cast<uint32_t>(static_cast<int>(gy + 20.0F))));
			lit = Clamp01(lit);
			px.mix(static_cast<int>(cx) + dx, static_cast<int>(cy) + dy, 40 + 150 * lit, 34 + 132 * lit, 26 + 104 * lit, alpha);
		}
}

} // namespace

Displacement EarthquakeShake(float age)
{
	if (age < 0 || age > static_cast<float>(EarthquakePulses))
		return {};
	const float since = age - LastPulse(age);
	const float a = 3.5F * std::exp(-since / 0.18F) + 0.8F; // a jolt on each pulse, a tremor between
	return { static_cast<int>(std::lround(std::sin(age * 61.0F) * a)), static_cast<int>(std::lround(std::cos(age * 47.0F) * a * 0.6F)) };
}

void DrawEarthquakeFx(const Surface &out, Point centre, float age, bool floorLayer)
{
	if (out.isIndexed() || age < 0 || age > EarthquakeSeconds)
		return;
	const Px px { &out, out.w(), out.h() };
	constexpr uint32_t Seed = 7211;
	constexpr float Rx = 140.0F, Ry = 70.0F; // the 3-tile reach on the floor
	const float cx = static_cast<float>(centre.x), cy = static_cast<float>(centre.y);
	const bool quaking = age < static_cast<float>(EarthquakePulses);
	const float last = LastPulse(age);
	const float since = age - last;
	// How bright the fissures burn: a flare on each pulse settling to a glow; cooling out once it ends.
	const float after = age - static_cast<float>(EarthquakePulses);
	const float glow = quaking ? 0.4F + 0.6F * std::exp(-since / 0.3F) : 0.4F * Clamp01(1.0F - after / 0.8F);
	const float scar = quaking ? 1.0F : Clamp01(1.0F - (after - 0.3F) / 0.7F);
	// How far the fissures have broken: a step out on each pulse, quick, then holding.
	const float grown = (last + Clamp01(since / 0.15F)) / static_cast<float>(EarthquakePulses);
	if (floorLayer) {
		// The warm glow over the whole broken area, pulsing with the quake.
		for (int dy = -static_cast<int>(Ry); dy <= static_cast<int>(Ry); dy++)
			for (int dx = -static_cast<int>(Rx); dx <= static_cast<int>(Rx); dx++) {
				const float d2 = static_cast<float>(dx * dx) / (Rx * Rx) + static_cast<float>(dy * dy) / (Ry * Ry);
				if (d2 < 1.0F)
					px.add(centre.x + dx, centre.y + dy, 255, 110, 30, (1.0F - d2) * 0.22F * glow);
			}
		// Radial fissures out from under him, growing with each pulse.
		for (uint32_t i = 0; i < 11; i++) {
			float ang = (static_cast<float>(i) + Unit(Hash(i, Seed)) * 0.7F) / 11.0F * TwoPi;
			const float maxLen = 0.6F + 0.4F * Unit(Hash(i, Seed + 1));
			const float len = maxLen * std::min(1.0F, grown * (1.0F + 0.3F * Unit(Hash(i, Seed + 2))));
			float x = cx, y = cy;
			constexpr int Pieces = 10;
			for (int k = 1; k <= Pieces; k++) {
				const float q = static_cast<float>(k) / static_cast<float>(Pieces);
				if (q > len)
					break;
				ang += (Unit(Hash(i, Seed + 3, static_cast<uint32_t>(k))) - 0.5F) * 0.5F;
				const float nx = cx + std::cos(ang) * Rx * q, ny = cy + std::sin(ang) * Ry * q;
				CrackLine(px, x, y, nx, ny, glow * (1.0F - q * 0.4F), scar, q < 0.5F);
				// Now and then a short branch.
				if (k % 3 == 2) {
					const float side = (Hash(i, Seed + 4, static_cast<uint32_t>(k)) & 1) ? 0.7F : -0.7F;
					const float bl = 0.12F + 0.1F * Unit(Hash(i, Seed + 5, static_cast<uint32_t>(k)));
					CrackLine(px, nx, ny, nx + std::cos(ang + side) * Rx * bl, ny + std::sin(ang + side) * Ry * bl, glow * 0.6F, scar * 0.8F, false);
				}
				x = nx;
				y = ny;
			}
		}
		// Arc cracks round him, opening at each pulse's front: two or three a pulse, at a wider ring each time.
		for (int p = 0; p <= static_cast<int>(last); p++) {
			const float open = p < static_cast<int>(last) ? 1.0F : Clamp01(since / 0.25F);
			const float r = 0.35F + 0.17F * static_cast<float>(p);
			for (uint32_t a = 0; a < 3; a++) {
				const uint32_t h = Hash(static_cast<uint32_t>(p), a, Seed + 10);
				const float a0 = Unit(h) * TwoPi, span = (0.4F + 0.6F * Unit(h >> 8)) * open;
				float lx = cx + std::cos(a0) * Rx * r, ly = cy + std::sin(a0) * Ry * r;
				for (int k = 1; k <= 8; k++) {
					const float aa = a0 + span * static_cast<float>(k) / 8.0F;
					const float rr = r * (1.0F + (Unit(Hash(h, static_cast<uint32_t>(k))) - 0.5F) * 0.06F);
					const float nx = cx + std::cos(aa) * Rx * rr, ny = cy + std::sin(aa) * Ry * rr;
					CrackLine(px, lx, ly, nx, ny, glow * 0.8F, scar, false);
					lx = nx;
					ly = ny;
				}
			}
		}
		// The falling rocks' shadows, growing and darkening as they near the floor; then the shadow under it as it rolls.
		for (uint32_t i = 0; i < static_cast<uint32_t>(QuakeRocks); i++) {
			const QuakeRock r = RockOf(i);
			const float t = age - r.drop;
			if (t < 0)
				continue;
			const float near = Clamp01(t / RockFallSeconds);
			const float rolled = t > RockFallSeconds ? RockRolled(r, t - RockFallSeconds) : 0.0F;
			const float sx = cx + r.x + r.rollX * rolled, sy = cy + r.y + r.rollY * rolled;
			const float w = r.radius * (0.6F + 0.7F * near), a = (0.15F + 0.4F * near * near) * scar;
			const int iw = static_cast<int>(w) + 1;
			for (int dy = -iw; dy <= iw; dy++)
				for (int dx = -iw; dx <= iw; dx++) {
					const float d2 = static_cast<float>(dx * dx) / (w * w) + static_cast<float>(dy * dy * 4) / (w * w);
					if (d2 < 1.0F)
						px.mix(static_cast<int>(sx) + dx, static_cast<int>(sy) + 2 + dy, 8, 6, 4, a * (1.0F - d2));
				}
		}
		// Each pulse's shockwave: a ring of dusty light running out to the reach, the air bending with it.
		if (quaking && since < 0.5F) {
			const float w = std::sqrt(since / 0.5F);
			const float R = w, fade = 1.0F - since / 0.5F;
			const int samples = 900;
			for (int s = 0; s < samples; s++) {
				const float a = static_cast<float>(s) / static_cast<float>(samples) * TwoPi;
				const int x = static_cast<int>(cx + std::cos(a) * Rx * R), y = static_cast<int>(cy + std::sin(a) * Ry * R);
				px.add(x, y, 250, 215, 150, 0.6F * fade);
				px.add(x, y + 1, 240, 190, 120, 0.3F * fade);
				px.mix(static_cast<int>(cx + std::cos(a) * Rx * R * 0.9F), static_cast<int>(cy + std::sin(a) * Ry * R * 0.9F), 120, 104, 84, 0.3F * fade);
			}
			Ripple(out, centre, Rx * R, 6.0F * fade);
		}
		return;
	}
	// ---- In the air ----
	// Stones jumping up all over the area on each pulse, falling back.
	if (quaking) {
		for (uint32_t i = 0; i < 26; i++) {
			const uint32_t h = Hash(static_cast<uint32_t>(last), i, Seed + 20);
			const float t = since - Unit(h) * 0.12F; // not all at once
			if (t < 0)
				continue;
			const float up = 50.0F + Unit(h >> 4) * 70.0F;
			const float z = std::max(0.0F, up * t - 330.0F * t * t);
			if (z <= 0 && t > 0.1F)
				continue;
			const float a = Unit(h >> 8) * TwoPi, r = std::sqrt(Unit(h >> 12));
			const float x = cx + std::cos(a) * Rx * r, y = cy + std::sin(a) * Ry * r;
			const int size = 1 + static_cast<int>(h % 3);
			for (int ox = -size; ox <= size; ox++)
				px.mix(static_cast<int>(x) + ox, static_cast<int>(y), 10, 8, 6, 0.3F);
			for (int oy = 0; oy <= size; oy++)
				for (int ox = 0; ox <= size; ox++) {
					const float lit = oy == 0 ? 1.0F : (oy == size ? 0.45F : 0.75F);
					px.mix(static_cast<int>(x) + ox, static_cast<int>(y - z) + oy, 168 * lit, 150 * lit, 120 * lit, 0.95F);
				}
		}
	}
	// The rocks from the roof: falling, landing in a puff of dust with chips flying, rolling a little and coming to rest.
	for (uint32_t i = 0; i < static_cast<uint32_t>(QuakeRocks); i++) {
		const QuakeRock r = RockOf(i);
		const float t = age - r.drop;
		if (t < 0)
			continue;
		const float alpha = scar;
		if (t < RockFallSeconds) {
			// Falling, speeding up: a streak of dust above it while it is fast.
			const float u = t / RockFallSeconds;
			const float z = RockDropHeight * (1.0F - u * u);
			const float x = cx + r.x, y = cy + r.y - z;
			for (int k = 1; k <= 6; k++)
				px.mix(static_cast<int>(x), static_cast<int>(y) - static_cast<int>(r.radius) - k * 3, 150, 135, 110, 0.12F * u * (1.0F - static_cast<float>(k) / 7.0F));
			DrawRock(px, x, y, r.radius, t * 3.0F, r.shape, alpha);
			continue;
		}
		const float tl = t - RockFallSeconds;
		const float rolled = RockRolled(r, tl);
		const float x = cx + r.x + r.rollX * rolled, y = cy + r.y + r.rollY * rolled - r.radius * 0.6F;
		// Turning as it rolls: the distance over its size.
		DrawRock(px, x, y, r.radius, RockFallSeconds * 3.0F + rolled / r.radius, r.shape, alpha);
		// The landing: a puff of dust spreading low, and chips flying off.
		if (tl < 0.6F) {
			const float pu = tl / 0.6F;
			const float size = r.radius * (1.2F + 2.0F * pu);
			const float pa = 0.45F * (1.0F - pu);
			const int s2 = static_cast<int>(size);
			for (int dy = -s2 / 2; dy <= s2 / 2; dy++)
				for (int dx = -s2; dx <= s2; dx++) {
					const float d2 = static_cast<float>(dx * dx) / (size * size) + static_cast<float>(dy * dy * 4) / (size * size);
					if (d2 < 1.0F)
						px.mix(static_cast<int>(cx + r.x) + dx, static_cast<int>(cy + r.y) + dy - 2, 140, 125, 102, pa * (1.0F - d2));
				}
			for (uint32_t c = 0; c < 5; c++) {
				const float ca = Unit(Hash(r.shape, c, 1)) * TwoPi, cv = 30.0F + Unit(Hash(r.shape, c, 2)) * 40.0F;
				const float cz = std::max(0.0F, 60.0F * tl - 300.0F * tl * tl);
				const int chx = static_cast<int>(cx + r.x + std::cos(ca) * cv * tl), chy = static_cast<int>(cy + r.y + std::sin(ca) * cv * tl * 0.5F - cz);
				px.mix(chx, chy, 160, 142, 112, 0.9F * (1.0F - pu));
				px.mix(chx + 1, chy, 110, 96, 76, 0.9F * (1.0F - pu));
			}
		}
	}
	// The dust haze over the area: clouds rising and drifting the whole time, thinning out at the end.
	const float haze = quaking ? Clamp01(age / 0.5F) : Clamp01(1.0F - after / 1.0F);
	for (uint32_t i = 0; i < 18; i++) {
		const float period = 1.4F + Unit(Hash(i, Seed + 30)) * 1.0F;
		const float life = Frac(age / period + Unit(Hash(i, Seed + 31)));
		const uint32_t round = static_cast<uint32_t>(age / period + Unit(Hash(i, Seed + 31)));
		const uint32_t h = Hash(i, round, Seed + 32);
		const float a = Unit(h) * TwoPi, r = 0.2F + 0.8F * std::sqrt(Unit(h >> 8));
		const float size = 6.0F + 12.0F * life;
		const float alpha = 0.32F * std::sin(life * Pi) * haze;
		const float x = cx + std::cos(a) * Rx * r, y = cy + std::sin(a) * Ry * r - 6.0F - 16.0F * life;
		const int s = static_cast<int>(size);
		for (int dy = -s; dy <= s; dy++)
			for (int dx = -s; dx <= s; dx++) {
				const float d2 = static_cast<float>(dx * dx + dy * dy) / (size * size);
				if (d2 < 1.0F)
					px.mix(static_cast<int>(x) + dx, static_cast<int>(y) + dy, 132, 118, 98, alpha * (1.0F - d2));
			}
	}
}

void DrawGlowCracksFx(const Surface &out, Point centre, float age, uint32_t seed)
{
	if (out.isIndexed() || age < 0 || age > GlowCracksSeconds)
		return;
	const Px px { &out, out.w(), out.h() };
	const float cx = static_cast<float>(centre.x), cy = static_cast<float>(centre.y);
	const float grown = Clamp01(age / 0.12F);                     // out in a blink
	const float heat = std::exp(-age / 0.3F);                     // white-hot, cooling
	const float scar = Clamp01(1.0F - (age - 0.5F) / 0.4F);        // then the scars fade
	for (uint32_t i = 0; i < 8; i++) {
		float ang = (static_cast<float>(i) + Unit(Hash(i, seed)) * 0.6F) / 8.0F * TwoPi;
		const float len = (0.6F + 0.4F * Unit(Hash(i, seed + 1))) * grown;
		float x = cx, y = cy;
		for (int k = 1; k <= 6; k++) {
			const float q = static_cast<float>(k) / 6.0F;
			if (q > len)
				break;
			ang += (Unit(Hash(i, seed + 2, static_cast<uint32_t>(k))) - 0.5F) * 0.6F;
			const float nx = cx + std::cos(ang) * 70.0F * q, ny = cy + std::sin(ang) * 35.0F * q;
			CrackLine(px, x, y, nx, ny, heat * (1.0F - q * 0.5F), scar, q < 0.4F);
			x = nx;
			y = ny;
		}
	}
}

namespace {

/** A soft round glow of @p r pixels (squashed to the floor when @p floor) added at strength @p k. */
void Glow(const Px &px, float cx, float cy, float r, float cr, float cg, float cb, float k, bool floor)
{
	if (k <= 0.004F || r < 0.5F)
		return;
	const int R = static_cast<int>(r) + 1, Ry = floor ? R / 2 + 1 : R;
	for (int dy = -Ry; dy <= Ry; dy++)
		for (int dx = -R; dx <= R; dx++) {
			const float fy = static_cast<float>(dy) * (floor ? 2.0F : 1.0F);
			const float d2 = (static_cast<float>(dx * dx) + fy * fy) / (r * r);
			if (d2 < 1.0F) {
				const float f = 1.0F - d2;
				px.add(static_cast<int>(cx) + dx, static_cast<int>(cy) + dy, cr, cg, cb, k * f * f);
			}
		}
}

/** A ray of light from (@p x, @p y) along (@p ux, @p uy), @p len long, bright at the root and fading to nothing. */
void Ray(const Px &px, float x, float y, float ux, float uy, float len, float k)
{
	const int n = static_cast<int>(len);
	for (int s = 0; s <= n; s++) {
		const float f = 1.0F - static_cast<float>(s) / std::max(1.0F, len);
		const int ix = static_cast<int>(x + ux * static_cast<float>(s)), iy = static_cast<int>(y + uy * static_cast<float>(s));
		px.add(ix, iy, 255, 250, 235, k * f * f);
		px.add(ix - static_cast<int>(uy >= 0 ? 1 : -1) * (std::abs(uy) > 0.5F ? 1 : 0), iy + (std::abs(uy) > 0.5F ? 0 : 1), 255, 210, 120, k * f * f * 0.35F);
	}
}

/** A four-pointed star at (@p x, @p y): long across, shorter up and down, short diagonals. */
void Star(const Px &px, float x, float y, float size, float k)
{
	Ray(px, x, y, 1, 0, size, k);
	Ray(px, x, y, -1, 0, size, k);
	Ray(px, x, y, 0, 1, size * 0.6F, k);
	Ray(px, x, y, 0, -1, size * 0.6F, k);
	const float d = 0.7071F;
	Ray(px, x, y, d, d, size * 0.3F, k * 0.7F);
	Ray(px, x, y, -d, d, size * 0.3F, k * 0.7F);
	Ray(px, x, y, d, -d, size * 0.3F, k * 0.7F);
	Ray(px, x, y, -d, -d, size * 0.3F, k * 0.7F);
}

} // namespace

void DrawHolyLanceFx(const Surface &out, Point foot, int dirX, int dirY, int tiles, float age, bool floorLayer)
{
	if (out.isIndexed() || age < -HolyLanceGatherSeconds || age > HolyLanceSeconds || (dirX == 0 && dirY == 0))
		return;
	const Px px { &out, out.w(), out.h() };
	constexpr uint32_t Seed = 6101;
	constexpr float Height = 34.0F; // chest height: the thrust is level
	// One tile along the direction on screen, and the path: from just in front of him to past the last tile it strikes.
	const float stepX = static_cast<float>(dirX - dirY) * 32.0F, stepY = static_cast<float>(dirX + dirY) * 16.0F;
	const float fx = static_cast<float>(foot.x), fy = static_cast<float>(foot.y);
	const float sx = fx + stepX * 0.45F, sy = fy + stepY * 0.45F;                                          // start, on the floor
	const float ex = fx + stepX * (static_cast<float>(tiles) + 0.3F), ey = fy + stepY * (static_cast<float>(tiles) + 0.3F); // end
	const float len = std::max(1.0F, std::sqrt((ex - sx) * (ex - sx) + (ey - sy) * (ey - sy)));
	const float ux = (ex - sx) / len, uy = (ey - sy) / len;
	const float nx = -uy, ny = ux; // across the shaft, on screen
	// The thrust: out in a blink, easing at the end of its reach.
	const float travel = Clamp01(age / HolyLanceTravelSeconds);
	const float reach = 1.0F - (1.0F - travel) * (1.0F - travel);
	const float tipD = len * reach;
	// From 0.3 s the shaft dissolves from the tail toward the tip; then everything fades.
	const float tailD = len * Clamp01((age - 0.3F) / 0.35F);
	const float fade = Clamp01(1.0F - (age - 0.6F) / 0.3F);
	const float hitAge = age - HolyLanceTravelSeconds; // since the tip reached its end
	// When the tip passed distance d (0..len): the ease inverted.
	const auto passedAt = [&](float d) { return HolyLanceTravelSeconds * (1.0F - std::sqrt(std::max(0.0F, 1.0F - d / len))); };

	if (floorLayer) {
		if (age < 0)
			return;
		// The floor under the shaft lit gold, where the shaft still is.
		for (float d = tailD; d <= tipD; d += 4.0F) {
			const float t = d / len;
			Glow(px, sx + ux * d, sy + uy * d, 18.0F, 244, 204, 96, 0.10F * (0.5F + 0.5F * t) * fade, true);
		}
		// Each tile it pierces flashes on the floor as the tip goes through.
		for (int i = 1; i <= tiles; i++) {
			const float cx = fx + stepX * static_cast<float>(i), cy = fy + stepY * static_cast<float>(i);
			const float d = std::sqrt((cx - sx) * (cx - sx) + (cy - sy) * (cy - sy));
			const float since = age - passedAt(std::min(d, len));
			if (since >= 0 && since < 0.4F)
				Glow(px, cx, cy, 34.0F, 255, 226, 150, 0.35F * (1.0F - since / 0.4F), true);
		}
		// At the end: a ring of light running out on the floor, and the air bending with it.
		if (hitAge >= 0 && hitAge < 0.5F) {
			const float u = hitAge / 0.5F;
			const float R = 10.0F + 50.0F * std::sqrt(u);
			for (int s = 0; s < 160; s++) {
				const float a = static_cast<float>(s) / 160.0F * TwoPi;
				const int x = static_cast<int>(ex + std::cos(a) * R), y = static_cast<int>(ey + std::sin(a) * R * 0.5F);
				px.add(x, y, 255, 236, 180, 0.7F * (1.0F - u));
				px.add(x, y + 1, 244, 204, 96, 0.35F * (1.0F - u));
			}
			Glow(px, ex, ey, 40.0F, 255, 220, 140, 0.4F * (1.0F - u), true);
			Ripple(out, { static_cast<int>(ex), static_cast<int>(ey) }, R, 3.0F * (1.0F - u));
		}
		return;
	}

	// ---- In the air ----
	// The gather: a point of light swelling in front of him before the thrust.
	if (age < 0.05F) {
		const float g = Clamp01((age + HolyLanceGatherSeconds) / HolyLanceGatherSeconds);
		Glow(px, sx, sy - Height, 6.0F + 10.0F * g, 255, 236, 180, 0.6F * g, false);
		Star(px, sx, sy - Height, 6.0F + 10.0F * g, 0.5F * g);
		if (age < 0)
			return;
	}
	// The shaft: a white-hot core in a gold halo, thickening toward the head; a shimmer runs along it.
	const float headLen = 16.0F;
	for (float d = tailD; d <= tipD; d += 1.0F) {
		const float t = d / len;
		const float toTip = tipD - d;
		float hw = 1.5F + 1.5F * t; // half-width of the solid shaft
		if (toTip < headLen) {       // the head: a long diamond, widest a third of the way back, to a point
			const float h = toTip / headLen;
			hw = h > 0.66F ? hw + (5.5F - hw) * (1.0F - h) / 0.34F : 5.5F * (h / 0.66F);
		}
		// The tail end softens as it dissolves.
		const float tailSoft = Clamp01((d - tailD) / 20.0F);
		const float shimmer = 0.85F + 0.15F * std::sin(d * 0.35F - age * 60.0F);
		const float k = tailSoft * shimmer * fade;
		const float cx = sx + ux * d, cy = sy + uy * d - Height;
		const int span = static_cast<int>(hw) + 7;
		for (int o = -span; o <= span; o++) {
			const float a = std::abs(static_cast<float>(o));
			const int x = static_cast<int>(cx + nx * static_cast<float>(o)), y = static_cast<int>(cy + ny * static_cast<float>(o));
			if (a <= hw * 0.45F)
				px.add(x, y, 255, 252, 240, 1.0F * k);
			else if (a <= hw)
				px.add(x, y, 255, 226, 150, 0.8F * k);
			else {
				const float f = std::exp(-(a - hw) / 2.5F);
				px.add(x, y, 244, 190, 80, 0.45F * f * k);
			}
		}
		// Two ribbons of light winding round it, fading toward the tail.
		for (int r = 0; r < 2; r++) {
			const float w = std::sin(d * 0.16F - age * 34.0F + static_cast<float>(r) * Pi) * (hw + 5.0F);
			const float depth = std::cos(d * 0.16F - age * 34.0F + static_cast<float>(r) * Pi); // behind the shaft: dimmer
			px.add(static_cast<int>(cx + nx * w), static_cast<int>(cy + ny * w), 255, 214, 110, (depth > 0 ? 0.7F : 0.3F) * k * Clamp01(t * 2.0F));
		}
	}
	// The tip's star while it flies and as it strikes.
	if (tipD > tailD) {
		const float tx = sx + ux * tipD, ty = sy + uy * tipD - Height;
		const float twinkle = 0.8F + 0.2F * std::sin(age * 50.0F);
		Glow(px, tx, ty, 12.0F, 255, 240, 200, 0.6F * fade, false);
		Star(px, tx, ty, 14.0F * twinkle, 0.8F * fade);
	}
	// Each body it passes through flashes white.
	for (int i = 1; i <= tiles; i++) {
		const float cx = fx + stepX * static_cast<float>(i), cy = fy + stepY * static_cast<float>(i);
		const float d = std::sqrt((cx - sx) * (cx - sx) + (cy - sy) * (cy - sy));
		const float since = age - passedAt(std::min(d, len));
		if (since >= 0 && since < 0.25F) {
			const float k = 1.0F - since / 0.25F;
			Glow(px, cx, cy - Height, 22.0F, 255, 246, 220, 0.5F * k, false);
			Star(px, cx, cy - Height, 20.0F * k + 6.0F, 0.6F * k);
		}
	}
	// The strike at the end: a starburst, a flash, and sparks thrown forward and falling.
	if (hitAge >= 0) {
		const float u = Clamp01(hitAge / 0.35F);
		const float burst = std::exp(-hitAge / 0.12F);
		const float x = ex, y = ey - Height;
		Glow(px, x, y, 30.0F, 255, 236, 180, 0.7F * burst, false);
		Star(px, x, y, 26.0F + 30.0F * u, 0.9F * burst + 0.2F * (1.0F - u));
		for (uint32_t i = 0; i < 16; i++) {
			const float a = std::atan2(uy, ux) + (Unit(Hash(i, Seed)) - 0.5F) * 2.2F;
			const float v = 60.0F + Unit(Hash(i, Seed + 1)) * 90.0F;
			const float t = hitAge;
			if (t > 0.55F)
				continue;
			const float px0 = x + std::cos(a) * v * t, py0 = y + std::sin(a) * v * t * 0.6F + 160.0F * t * t;
			const float k = 1.0F - t / 0.55F;
			px.add(static_cast<int>(px0), static_cast<int>(py0), 255, 240, 190, k);
			px.add(static_cast<int>(px0 - std::cos(a) * 2.0F), static_cast<int>(py0 - std::sin(a) * 1.2F), 255, 200, 90, 0.5F * k);
		}
	}
	// The shaft dissolving: gold motes rising off it where it has gone.
	for (uint32_t i = 0; i < 36; i++) {
		const float d = Unit(Hash(i, Seed + 5)) * len;
		const float born = 0.3F + 0.35F * d / len; // as the dissolve passes it
		const float t = age - born;
		if (t < 0 || t > 0.6F)
			continue;
		const float k = std::sin(t / 0.6F * Pi) * 0.9F;
		const float drift = (Unit(Hash(i, Seed + 6)) - 0.5F) * 10.0F;
		const int x = static_cast<int>(sx + ux * d + nx * drift + drift * 0.3F);
		const int y = static_cast<int>(sy + uy * d - Height + ny * drift - 28.0F * t);
		px.add(x, y, 255, 230, 160, k);
		px.add(x, y - 1, 244, 204, 96, 0.4F * k);
	}
}

namespace {

/** A feather of hoarfrost on the floor at (@p x, @p y): a stem along @p a with side barbs, grown to @p grown (0..1). */
void FrostFern(const Px &px, float x, float y, float a, float len, float grown, float k, uint32_t seed)
{
	if (grown <= 0 || k <= 0.01F)
		return;
	const float l = len * grown;
	const float ca = std::cos(a), sa = std::sin(a);
	for (float s = 0; s <= l; s += 1.0F) {
		const int ix = static_cast<int>(x + ca * s), iy = static_cast<int>(y + sa * s * 0.5F);
		const float tip = 1.0F - s / std::max(1.0F, len);
		px.mix(ix, iy, 214, 236, 255, 0.75F * k);
		px.add(ix, iy, 120, 170, 255, 0.25F * k * tip);
		// Barbs every few pixels, alternating sides, shorter toward the tip.
		if (static_cast<int>(s) % 3 == 1) {
			const float side = (static_cast<int>(s) / 3) % 2 ? 1.0F : -1.0F;
			const float ba = a + side * 1.0F;
			const float bl = (2.0F + 4.0F * Unit(Hash(seed, static_cast<uint32_t>(s)))) * tip;
			for (float b = 1; b <= bl; b += 1.0F)
				px.mix(static_cast<int>(x + ca * s + std::cos(ba) * b), static_cast<int>(y + (sa * s + std::sin(ba) * b) * 0.5F), 200, 228, 255, 0.55F * k * (1.0F - b / (bl + 1.0F)));
		}
	}
}

} // namespace

void DrawChillTouchFx(const Surface &out, Point foot, int dirX, int dirY, float age, bool floorLayer)
{
	if (out.isIndexed() || age < 0 || age > ChillTouchSeconds || (dirX == 0 && dirY == 0))
		return;
	const Px px { &out, out.w(), out.h() };
	constexpr uint32_t Seed = 4409;
	// The cone on the floor, worked out flat (y doubled) and squashed back: its axis along the tile direction, 1.7 tiles long.
	const float stepX = static_cast<float>(dirX - dirY) * 32.0F, stepY = static_cast<float>(dirX + dirY) * 16.0F;
	const float axis = std::atan2(stepY * 2.0F, stepX);
	const float reach = 1.7F * std::sqrt(stepX * stepX + stepY * stepY * 4.0F);
	constexpr float Spread = 0.62F; // radians either side
	const float fx = static_cast<float>(foot.x), fy = static_cast<float>(foot.y);
	const float hx = fx + std::cos(axis) * 14.0F, hy = fy + std::sin(axis) * 7.0F; // under her outstretched hand
	const auto at = [&](float a, float r, float &x, float &y) { x = hx + std::cos(a) * r; y = hy + std::sin(a) * r * 0.5F; };
	// How far the breath has rolled out: fast, then slowing.
	const float out01 = 1.0F - std::exp(-age / 0.13F);

	if (floorLayer) {
		// A cold sheen over the cone, following the breath out and lifting as it thaws.
		const float sheen = Clamp01(age / 0.15F) * Clamp01(1.0F - (age - 0.7F) / 0.7F);
		if (sheen > 0) {
			const float R = reach * out01;
			const int span = static_cast<int>(reach) + 2;
			for (int dy = -span / 2; dy <= span / 2; dy++)
				for (int dx = -span; dx <= span; dx++) {
					const float wx = static_cast<float>(dx), wy = static_cast<float>(dy) * 2.0F;
					const float r = std::sqrt(wx * wx + wy * wy);
					if (r > R || r < 2.0F)
						continue;
					float da = std::atan2(wy, wx) - axis;
					while (da > Pi)
						da -= TwoPi;
					while (da < -Pi)
						da += TwoPi;
					const float edge = 1.0F - std::abs(da) / Spread;
					if (edge <= 0)
						continue;
					const float f = std::min(1.0F, edge * 2.5F) * (1.0F - r / (reach + 1.0F) * 0.5F) * sheen;
					px.mix(static_cast<int>(hx) + dx, static_cast<int>(hy) + dy, 150, 190, 230, 0.20F * f);
					px.add(static_cast<int>(hx) + dx, static_cast<int>(hy) + dy, 70, 120, 220, 0.18F * f);
				}
		}
		// Hoarfrost ferns, each growing as the breath reaches it, melting away at the end.
		for (uint32_t i = 0; i < 18; i++) {
			const float a = axis + (Unit(Hash(i, Seed)) * 2.0F - 1.0F) * Spread * 0.9F;
			const float r = reach * (0.25F + 0.75F * std::sqrt(Unit(Hash(i, Seed + 1))));
			const float reached = -0.13F * std::log(std::max(0.001F, 1.0F - r / reach)); // when out01 got there
			const float grown = Clamp01((age - reached) / 0.18F);
			const float melt = Clamp01(1.0F - (age - 0.8F - 0.3F * Unit(Hash(i, Seed + 2))) / 0.4F);
			float x, y;
			at(a, r, x, y);
			const float fa = a + (Unit(Hash(i, Seed + 3)) - 0.5F) * 1.6F;
			FrostFern(px, x, y, fa, 7.0F + 9.0F * Unit(Hash(i, Seed + 4)), grown, melt, Hash(i, Seed + 5));
			FrostFern(px, x, y, fa + Pi * 0.66F, 4.0F + 5.0F * Unit(Hash(i, Seed + 6)), grown, melt * 0.8F, Hash(i, Seed + 7));
		}
		return;
	}

	// ---- In the air ----
	// A flash of cold at her hand as she casts.
	if (age < 0.14F) {
		const float k = 1.0F - age / 0.14F;
		for (int dy = -9; dy <= 9; dy++)
			for (int dx = -9; dx <= 9; dx++) {
				const float d2 = static_cast<float>(dx * dx + dy * dy) / 81.0F;
				if (d2 < 1.0F)
					px.add(static_cast<int>(hx) + dx, static_cast<int>(hy) - 30 + dy, 190, 225, 255, k * (1.0F - d2) * (1.0F - d2));
			}
	}
	// The breath: puffs of frost mist rolling out through the cone, swelling, sinking toward the floor and thinning.
	for (uint32_t i = 0; i < 26; i++) {
		const float t0 = Unit(Hash(i, Seed + 10)) * 0.12F;
		const float t = age - t0;
		const float life = 0.75F + 0.35F * Unit(Hash(i, Seed + 11));
		if (t < 0 || t > life)
			continue;
		const float u = t / life;
		const float a = axis + (Unit(Hash(i, Seed + 12)) * 2.0F - 1.0F) * Spread * 0.85F;
		const float r = reach * (0.35F + 0.75F * Unit(Hash(i, Seed + 13))) * (1.0F - std::exp(-t / 0.14F));
		float x, y;
		at(a, r, x, y);
		y -= 30.0F * (1.0F - Clamp01(t / 0.6F)) + 6.0F; // from hand height, settling low
		const float size = 6.0F + 16.0F * std::sqrt(u);
		const float alpha = 0.5F * std::sin(std::min(1.0F, u * 1.4F + 0.1F) * Pi) * (1.0F - u * 0.4F);
		const int s = static_cast<int>(size);
		for (int dy = -s; dy <= s; dy++)
			for (int dx = -s; dx <= s; dx++) {
				const float d2 = static_cast<float>(dx * dx + dy * dy * 2) / (size * size);
				if (d2 < 1.0F)
					px.mix(static_cast<int>(x) + dx, static_cast<int>(y) + dy, 212, 232, 255, alpha * (1.0F - d2));
			}
	}
	// Ice crystals glittering in it: carried out with the mist, twinkling, falling a little.
	for (uint32_t i = 0; i < 44; i++) {
		const float t0 = Unit(Hash(i, Seed + 20)) * 0.15F;
		const float t = age - t0;
		if (t < 0 || t > 0.9F)
			continue;
		const float a = axis + (Unit(Hash(i, Seed + 21)) * 2.0F - 1.0F) * Spread;
		const float r = reach * (0.2F + 0.9F * Unit(Hash(i, Seed + 22))) * (1.0F - std::exp(-t / 0.12F));
		float x, y;
		at(a, r, x, y);
		y -= 28.0F * (1.0F - Clamp01(t / 0.7F)) + 4.0F;
		const float tw = std::pow(0.5F + 0.5F * std::sin(t * 38.0F + static_cast<float>(i) * 2.1F), 6.0F);
		const float k = (1.0F - t / 0.9F) * (0.35F + 0.65F * tw);
		const int ix = static_cast<int>(x), iy = static_cast<int>(y);
		px.add(ix, iy, 240, 250, 255, k);
		if (tw > 0.6F) { // a sparkling one throws a tiny cross
			px.add(ix - 1, iy, 160, 210, 255, 0.6F * k);
			px.add(ix + 1, iy, 160, 210, 255, 0.6F * k);
			px.add(ix, iy - 1, 160, 210, 255, 0.6F * k);
			px.add(ix, iy + 1, 160, 210, 255, 0.6F * k);
		}
	}
}

void DrawRimeFx(const Surface &out, Point foot, float age, bool floorLayer)
{
	if (out.isIndexed() || age < 0 || age > RimeSeconds)
		return;
	const Px px { &out, out.w(), out.h() };
	constexpr uint32_t Seed = 4513;
	const float fx = static_cast<float>(foot.x), fy = static_cast<float>(foot.y);
	if (floorLayer) {
		// A ring of frost at its feet, crystals pointing out, melting.
		const float k = Clamp01(age / 0.1F) * Clamp01(1.0F - (age - 0.5F) / 0.4F);
		for (uint32_t i = 0; i < 9; i++) {
			const float a = static_cast<float>(i) / 9.0F * TwoPi + Unit(Hash(i, Seed)) * 0.5F;
			FrostFern(px, fx + std::cos(a) * 8.0F, fy + std::sin(a) * 4.0F, a, 5.0F + 6.0F * Unit(Hash(i, Seed + 1)), Clamp01(age / 0.15F), k, Hash(i, Seed + 2));
		}
		return;
	}
	// A white-blue flash on the body, and splinters of ice bursting off it and falling.
	const float flash = std::exp(-age / 0.1F);
	for (int dy = -16; dy <= 16; dy++)
		for (int dx = -12; dx <= 12; dx++) {
			const float d2 = static_cast<float>(dx * dx) / 144.0F + static_cast<float>(dy * dy) / 256.0F;
			if (d2 < 1.0F)
				px.add(static_cast<int>(fx) + dx, static_cast<int>(fy) - 30 + dy, 170, 215, 255, 0.55F * flash * (1.0F - d2));
		}
	for (uint32_t i = 0; i < 9; i++) {
		const float a = Unit(Hash(i, Seed + 3)) * TwoPi;
		const float v = 40.0F + Unit(Hash(i, Seed + 4)) * 50.0F;
		const float t = age;
		if (t > 0.5F)
			continue;
		const float x = fx + std::cos(a) * v * t, y = fy - 30.0F + std::sin(a) * v * t * 0.6F - 40.0F * t + 200.0F * t * t;
		const float k = 1.0F - t / 0.5F;
		const float ex = std::cos(a) * 2.0F, ey = std::sin(a) * 1.2F; // a splinter: a short bright line along its flight
		px.add(static_cast<int>(x), static_cast<int>(y), 235, 248, 255, k);
		px.add(static_cast<int>(x - ex), static_cast<int>(y - ey), 150, 200, 255, 0.7F * k);
	}
}

void DrawFrostNovaFx(const Surface &out, Point centre, float age, bool floorLayer)
{
	if (out.isIndexed() || age < 0 || age > FrostNovaSeconds)
		return;
	const Px px { &out, out.w(), out.h() };
	constexpr uint32_t Seed = 4621;
	constexpr float Rmax = 150.0F; // across; half that down (the floor's diamond)
	const float cx = static_cast<float>(centre.x), cy = static_cast<float>(centre.y);
	// The ring: out fast, slowing toward its reach.
	const float R = Rmax * (1.0F - std::exp(-age / 0.2F));
	const float ringK = Clamp01(1.0F - (age - 0.35F) / 0.45F);
	// When the ring reached radius r (0..Rmax).
	const auto reachedAt = [](float r) { return -0.2F * std::log(std::max(0.001F, 1.0F - r / Rmax)); };

	if (floorLayer) {
		// The cold sheen inside the ring, lifting as it thaws.
		const float sheen = Clamp01(1.0F - (age - 0.6F) / 0.6F);
		for (int dy = -static_cast<int>(R * 0.5F); dy <= static_cast<int>(R * 0.5F); dy++)
			for (int dx = -static_cast<int>(R); dx <= static_cast<int>(R); dx++) {
				const float d = std::sqrt(static_cast<float>(dx * dx) + static_cast<float>(dy * dy) * 4.0F);
				if (d > R)
					continue;
				// Patchy, as frost takes a floor: denser in places, thin in others.
				const float patch = 0.45F + 0.55F * std::max(0.0F, std::sin(static_cast<float>(dx) * 0.11F + static_cast<float>(dy) * 0.23F) * std::sin(static_cast<float>(dx) * 0.07F - static_cast<float>(dy) * 0.31F + 1.3F) + 0.3F);
				const float f = (0.4F + 0.6F * d / Rmax) * sheen * patch;
				px.mix(static_cast<int>(cx) + dx, static_cast<int>(cy) + dy, 150, 190, 230, 0.12F * f);
				px.add(static_cast<int>(cx) + dx, static_cast<int>(cy) + dy, 60, 110, 220, 0.10F * f);
			}
		// The ring of frost light running out, and the air bending with it.
		if (ringK > 0) {
			for (int s = 0; s < 360; s++) {
				const float a = static_cast<float>(s) / 360.0F * TwoPi;
				for (int w = -3; w <= 3; w++) {
					const float rr = R + static_cast<float>(w);
					const float k = ringK * (1.0F - std::abs(static_cast<float>(w)) / 4.0F);
					px.add(static_cast<int>(cx + std::cos(a) * rr), static_cast<int>(cy + std::sin(a) * rr * 0.5F), 190, 225, 255, 0.55F * k);
				}
			}
			Ripple(out, centre, R, 4.0F * ringK);
		}
		// Hoarfrost left behind the ring.
		for (uint32_t i = 0; i < 34; i++) {
			const float a = Unit(Hash(i, Seed)) * TwoPi;
			const float r = Rmax * (0.2F + 0.75F * std::sqrt(Unit(Hash(i, Seed + 1))));
			const float grown = Clamp01((age - reachedAt(r)) / 0.2F);
			const float melt = Clamp01(1.0F - (age - 0.75F - 0.3F * Unit(Hash(i, Seed + 2))) / 0.35F);
			const float x = cx + std::cos(a) * r, y = cy + std::sin(a) * r * 0.5F;
			FrostFern(px, x, y, a + (Unit(Hash(i, Seed + 3)) - 0.5F) * 1.2F, 6.0F + 9.0F * Unit(Hash(i, Seed + 4)), grown, melt, Hash(i, Seed + 5));
		}
		return;
	}

	// ---- In the air ----
	// The burst at the caster: a white-blue flash.
	if (age < 0.18F) {
		const float k = 1.0F - age / 0.18F;
		for (int dy = -20; dy <= 20; dy++)
			for (int dx = -26; dx <= 26; dx++) {
				const float d2 = static_cast<float>(dx * dx) / 676.0F + static_cast<float>(dy * dy) / 400.0F;
				if (d2 < 1.0F)
					px.add(static_cast<int>(cx) + dx, static_cast<int>(cy) - 26 + dy, 190, 225, 255, 0.8F * k * (1.0F - d2) * (1.0F - d2));
			}
	}
	// The mist riding the ring: puffs along its front, rolling outward and thinning.
	if (ringK > 0)
		for (uint32_t i = 0; i < 30; i++) {
			const float a = (static_cast<float>(i) + Unit(Hash(i, Seed + 10))) / 30.0F * TwoPi;
			const float x = cx + std::cos(a) * (R - 4.0F), y = cy + std::sin(a) * (R - 4.0F) * 0.5F - 6.0F;
			const float size = 7.0F + 6.0F * Unit(Hash(i, Seed + 11)) + 6.0F * age;
			const float alpha = 0.32F * ringK;
			const int s = static_cast<int>(size);
			for (int dy = -s / 2; dy <= s / 2; dy++)
				for (int dx = -s; dx <= s; dx++) {
					const float d2 = static_cast<float>(dx * dx) / (size * size) + static_cast<float>(dy * dy * 4) / (size * size);
					if (d2 < 1.0F)
						px.mix(static_cast<int>(x) + dx, static_cast<int>(y) + dy, 214, 234, 255, alpha * (1.0F - d2));
				}
		}
	// Ice spikes bursting up from the floor as the ring passes, standing a moment, then shattering into falling shards.
	for (uint32_t i = 0; i < 30; i++) {
		const float a = Unit(Hash(i, Seed + 20)) * TwoPi;
		const float r = Rmax * (0.3F + 0.68F * Unit(Hash(i, Seed + 21)));
		const float t = age - reachedAt(r);
		if (t < 0)
			continue;
		const float x = cx + std::cos(a) * r, y = cy + std::sin(a) * r * 0.5F;
		const float h = 10.0F + 14.0F * Unit(Hash(i, Seed + 22));
		const float lean = (Unit(Hash(i, Seed + 23)) - 0.5F) * 0.6F + std::cos(a) * 0.25F; // leaning outward a little
		constexpr float Stand = 0.35F;
		if (t < Stand) {
			const float up = Clamp01(t / 0.07F);
			const float hh = h * up;
			for (float z = 0; z <= hh; z += 1.0F) {
				const float w = 2.6F * (1.0F - z / (h + 0.01F));
				const int iy = static_cast<int>(y - z), ix0 = static_cast<int>(x + lean * z);
				for (int o = -static_cast<int>(w + 0.5F); o <= static_cast<int>(w + 0.5F); o++) {
					// Lit from the upper left: the left face white, the right face blue.
					if (o < 0)
						px.mix(ix0 + o, iy, 225, 242, 255, 0.85F);
					else
						px.mix(ix0 + o, iy, 120, 170, 225, 0.85F);
				}
			}
			px.add(static_cast<int>(x + lean * hh), static_cast<int>(y - hh), 255, 255, 255, 0.9F); // a glint at its point
			continue;
		}
		// Shattered: a few shards thrown out and falling.
		const float ts = t - Stand;
		if (ts > 0.45F)
			continue;
		for (uint32_t s = 0; s < 4; s++) {
			const float sa = Unit(Hash(i, Seed + 30, s)) * TwoPi;
			const float v = 20.0F + 40.0F * Unit(Hash(i, Seed + 31, s));
			const float sx = x + std::cos(sa) * v * ts, sy = y - h * 0.5F + std::sin(sa) * v * ts * 0.5F - 30.0F * ts + 220.0F * ts * ts;
			if (sy > y + 4.0F)
				continue;
			const float k = 1.0F - ts / 0.45F;
			px.add(static_cast<int>(sx), static_cast<int>(sy), 230, 245, 255, k);
			px.add(static_cast<int>(sx) + 1, static_cast<int>(sy), 140, 190, 245, 0.6F * k);
		}
	}
	// Crystals glittering in the cold air over the whole area.
	for (uint32_t i = 0; i < 40; i++) {
		const float a = Unit(Hash(i, Seed + 40)) * TwoPi;
		const float r = Rmax * std::sqrt(Unit(Hash(i, Seed + 41)));
		const float t = age - reachedAt(r);
		if (t < 0 || t > 0.8F)
			continue;
		const float x = cx + std::cos(a) * r, y = cy + std::sin(a) * r * 0.5F - 8.0F - 20.0F * Unit(Hash(i, Seed + 42)) + 10.0F * t;
		const float tw = std::pow(0.5F + 0.5F * std::sin(t * 36.0F + static_cast<float>(i) * 1.7F), 6.0F);
		px.add(static_cast<int>(x), static_cast<int>(y), 240, 250, 255, (1.0F - t / 0.8F) * (0.3F + 0.7F * tw));
	}
}

namespace {

/** The orb's facets: fixed points on the unit sphere, each the centre of one face of ice. */
struct OrbFacets {
	float p[18][3];
	OrbFacets()
	{
		for (uint32_t i = 0; i < 18; i++) {
			// Spread evenly (a Fibonacci sphere), nudged so the faces are not regular.
			const float y = 1.0F - (static_cast<float>(i) + 0.5F) / 9.0F;
			const float r = std::sqrt(std::max(0.0F, 1.0F - y * y));
			const float a = static_cast<float>(i) * 2.39996F + (Unit(Hash(i, 7707)) - 0.5F) * 0.5F;
			p[i][0] = std::cos(a) * r;
			p[i][1] = y;
			p[i][2] = std::sin(a) * r;
		}
	}
};

} // namespace

void DrawFrozenOrbFx(const Surface &out, Point start, Point at, float age, bool floorLayer)
{
	if (out.isIndexed() || age < 0)
		return;
	static const OrbFacets Facets;
	const Px px { &out, out.w(), out.h() };
	constexpr uint32_t Seed = 4733;
	constexpr float R = FrozenOrbRadius;
	const float ax = static_cast<float>(at.x), ay = static_cast<float>(at.y);
	const float tx = ax - static_cast<float>(start.x), ty = ay - static_cast<float>(start.y);
	const float screenDist = std::sqrt(tx * tx + ty * ty);
	const float ux = screenDist > 0 ? tx / screenDist : 1.0F, uy = screenDist > 0 ? ty / screenDist : 0.0F;
	// How far it has rolled over the floor (worked out flat, y doubled): the turn of the ball.
	const float flatDist = std::sqrt(tx * tx + ty * ty * 4.0F);
	const float turn = flatDist / R;

	if (floorLayer) {
		// The frost it leaves: a pale band along its track, hoarfrost feathering off it, thawing behind it.
		for (float s = 0; s <= screenDist; s += 1.0F) {
			const float behind = screenDist - s;
			const float k = Clamp01(1.0F - behind / 150.0F) * Clamp01(age / 0.1F);
			if (k <= 0)
				continue;
			const float x = static_cast<float>(start.x) + ux * s, y = static_cast<float>(start.y) + uy * s;
			const float wob = std::sin(s * 0.21F) * 1.2F;
			for (int o = -4; o <= 4; o++) {
				const float e = 1.0F - std::abs(static_cast<float>(o)) / 5.0F;
				px.mix(static_cast<int>(x - uy * (static_cast<float>(o) + wob)), static_cast<int>(y + ux * (static_cast<float>(o) + wob) * 0.5F), 196, 226, 252, 0.35F * e * k);
			}
			if (static_cast<int>(s) % 16 == 7) {
				const uint32_t h = Hash(static_cast<uint32_t>(s), Seed);
				const float side = (h & 1) ? 1.0F : -1.0F;
				const float a = std::atan2(uy * 2.0F, ux) + side * (0.9F + 0.5F * Unit(h >> 4));
				FrostFern(px, x, y, a, 3.0F + 4.0F * Unit(h >> 8), 1.0F, 0.7F * k, h);
			}
		}
		// Its shadow, and its cold light on the floor round it.
		for (int dy = -6; dy <= 6; dy++)
			for (int dx = -14; dx <= 14; dx++) {
				const float d2 = static_cast<float>(dx * dx) / 196.0F + static_cast<float>(dy * dy) / 36.0F;
				if (d2 < 1.0F)
					px.mix(static_cast<int>(ax) + dx, static_cast<int>(ay) + dy + 1, 8, 10, 16, 0.45F * (1.0F - d2));
			}
		for (int dy = -20; dy <= 20; dy++)
			for (int dx = -40; dx <= 40; dx++) {
				const float d2 = static_cast<float>(dx * dx) / 1600.0F + static_cast<float>(dy * dy) / 400.0F;
				if (d2 < 1.0F)
					px.add(static_cast<int>(ax) + dx, static_cast<int>(ay) + dy, 90, 150, 255, 0.28F * (1.0F - d2) * (1.0F - d2));
			}
		return;
	}

	// ---- In the air ----
	// Mist streaming off its back.
	for (int i = 1; i <= 7; i++) {
		const float back = static_cast<float>(i) * 6.0F;
		if (back > screenDist + 2.0F)
			break;
		const float wob = std::sin(age * 9.0F + static_cast<float>(i) * 1.7F) * 2.0F;
		const float x = ax - ux * back - uy * wob, y = ay - uy * back + ux * wob - R - 1.0F + static_cast<float>(i) * 0.6F;
		const float size = 5.0F + static_cast<float>(i) * 1.3F;
		const float alpha = 0.32F * (1.0F - static_cast<float>(i) / 8.0F);
		const int s = static_cast<int>(size);
		for (int dy = -s; dy <= s; dy++)
			for (int dx = -s; dx <= s; dx++) {
				const float d2 = static_cast<float>(dx * dx + dy * dy) / (size * size);
				if (d2 < 1.0F)
					px.mix(static_cast<int>(x) + dx, static_cast<int>(y) + dy, 214, 234, 255, alpha * (1.0F - d2));
			}
	}
	// The ball: a sphere of ice resting on the floor, rolling about the axis across its path - its facets turn the way it goes.
	const float cx = ax, cy = ay - R - 1.0F;
	// The roll as a rotation of the surface about the on-screen axis across the path (-uy, ux, 0), by -turn so the front
	// face rolls down and forward.
	const float kx = -uy, ky = ux; // the axis (unit, in the picture plane)
	const float cs = std::cos(-turn), sn = std::sin(-turn);
	const float Lx = -0.45F, Ly = -0.65F, Lz = 0.61F; // light from the upper left, toward the eye
	for (int dy = -static_cast<int>(R) - 1; dy <= static_cast<int>(R) + 1; dy++)
		for (int dx = -static_cast<int>(R) - 1; dx <= static_cast<int>(R) + 1; dx++) {
			const float fx = (static_cast<float>(dx) + 0.5F) / R, fy = (static_cast<float>(dy) + 0.5F) / R;
			const float rr = fx * fx + fy * fy;
			if (rr >= 1.0F)
				continue;
			const float nz = std::sqrt(1.0F - rr);
			// The surface point turned back by the roll (Rodrigues about k), to find which facet shows here.
			const float dot = kx * fx + ky * fy;
			const float crx = ky * nz, cry = -kx * nz, crz = kx * fy - ky * fx; // k x n
			const float qx = fx * cs + crx * sn + kx * dot * (1.0F - cs);
			const float qy = fy * cs + cry * sn + ky * dot * (1.0F - cs);
			const float qz = nz * cs + crz * sn;
			float best = -2.0F, second = -2.0F;
			int bi = 0;
			for (int i = 0; i < 18; i++) {
				const float d = qx * Facets.p[i][0] + qy * Facets.p[i][1] + qz * Facets.p[i][2];
				if (d > best) {
					second = best;
					best = d;
					bi = i;
				} else if (d > second) {
					second = d;
				}
			}
			const float edge = best - second; // small at a facet's edge
			// A faceted look: each face lit a little differently, as if cut flat.
			const float faceShade = 0.6F + 0.75F * Unit(Hash(static_cast<uint32_t>(bi), Seed + 1));
			const float diffuse = std::max(0.0F, fx * Lx + fy * Ly + nz * Lz) * faceShade;
			const float rim = (1.0F - nz) * (1.0F - nz);
			float r = 60 + 150 * diffuse + 120 * rim, g = 110 + 125 * diffuse + 120 * rim, b = 190 + 65 * diffuse + 60 * rim;
			// The cold glowing inside it, strongest at the middle.
			r += 30 * nz;
			g += 50 * nz;
			b += 40 * nz;
			if (edge < 0.05F) { // the cracks between faces catch the light
				const float c = 1.0F - edge / 0.05F;
				r += 120 * c;
				g += 110 * c;
				b += 60 * c;
			}
			// A highlight where the light glances off.
			const float hx = Lx, hy = Ly, hz = Lz + 1.0F, hl = std::sqrt(hx * hx + hy * hy + hz * hz);
			const float spec = std::pow(std::max(0.0F, (fx * hx + fy * hy + nz * hz) / hl), 40.0F);
			r += 255 * spec;
			g += 255 * spec;
			b += 255 * spec;
			// Its edge pixel half-covered: soften the outline.
			const float cover = std::min(1.0F, (1.0F - std::sqrt(rr)) * R * 1.5F + 0.2F);
			px.mix(static_cast<int>(cx) + dx, static_cast<int>(cy) + dy, std::min(255.0F, r), std::min(255.0F, g), std::min(255.0F, b), 0.93F * cover);
		}
	// A halo of cold round it.
	for (int dy = -18; dy <= 18; dy++)
		for (int dx = -18; dx <= 18; dx++) {
			const float d = std::sqrt(static_cast<float>(dx * dx + dy * dy));
			if (d <= R || d > 18.0F)
				continue;
			const float f = 1.0F - (d - R) / (18.0F - R);
			px.add(static_cast<int>(cx) + dx, static_cast<int>(cy) + dy, 110, 170, 255, 0.35F * f * f);
		}
	// Crystals flung off as it turns: each from where the orb was a moment ago, outward and falling, twinkling.
	for (uint32_t i = 0; i < 16; i++) {
		const float period = 0.5F;
		const float phase = Frac(age / period + Unit(Hash(i, Seed + 10)));
		const uint32_t round = static_cast<uint32_t>(age / period + Unit(Hash(i, Seed + 10)));
		const float t = phase * period;
		const uint32_t h = Hash(i, round, Seed + 11);
		const float back = std::min(screenDist, t * 70.0F); // it has rolled on since
		const float a = Unit(h) * TwoPi;
		const float v = 18.0F + 22.0F * Unit(h >> 8);
		const float x = ax - ux * back + std::cos(a) * v * t;
		const float y = ay - uy * back - R + std::sin(a) * v * t * 0.6F - 20.0F * t + 60.0F * t * t;
		const float tw = std::pow(0.5F + 0.5F * std::sin(t * 40.0F + static_cast<float>(i)), 5.0F);
		px.add(static_cast<int>(x), static_cast<int>(y), 235, 248, 255, (1.0F - phase) * (0.35F + 0.65F * tw));
	}
}

void DrawFrozenOrbBurstFx(const Surface &out, Point centre, float age, bool floorLayer)
{
	if (out.isIndexed() || age < 0 || age > FrozenOrbBurstSeconds)
		return;
	const Px px { &out, out.w(), out.h() };
	constexpr uint32_t Seed = 4877;
	const float cx = static_cast<float>(centre.x), cy = static_cast<float>(centre.y);
	if (floorLayer) {
		// A star of hoarfrost thrown across the floor, and a ring of cold light running out.
		const float melt = Clamp01(1.0F - (age - 0.5F) / 0.4F);
		for (uint32_t i = 0; i < 12; i++) {
			const float a = static_cast<float>(i) / 12.0F * TwoPi + Unit(Hash(i, Seed)) * 0.3F;
			FrostFern(px, cx + std::cos(a) * 6.0F, cy + std::sin(a) * 3.0F, a, 14.0F + 14.0F * Unit(Hash(i, Seed + 1)), Clamp01(age / 0.12F), melt, Hash(i, Seed + 2));
		}
		const float u = Clamp01(age / 0.4F);
		if (u < 1.0F) {
			const float Rr = 10.0F + 55.0F * std::sqrt(u);
			for (int s = 0; s < 180; s++) {
				const float a = static_cast<float>(s) / 180.0F * TwoPi;
				px.add(static_cast<int>(cx + std::cos(a) * Rr), static_cast<int>(cy + std::sin(a) * Rr * 0.5F), 190, 225, 255, 0.6F * (1.0F - u));
			}
			Ripple(out, centre, Rr, 3.0F * (1.0F - u));
		}
		return;
	}
	// The flash as it shatters.
	const float flash = std::exp(-age / 0.08F);
	for (int dy = -22; dy <= 22; dy++)
		for (int dx = -22; dx <= 22; dx++) {
			const float d2 = static_cast<float>(dx * dx + dy * dy) / 484.0F;
			if (d2 < 1.0F)
				px.add(static_cast<int>(cx) + dx, static_cast<int>(cy) - static_cast<int>(FrozenOrbRadius) + dy, 200, 230, 255, flash * (1.0F - d2) * (1.0F - d2));
		}
	// Shards of the ball flying out and falling: each a little wedge of ice, tumbling.
	for (uint32_t i = 0; i < 14; i++) {
		const float a = Unit(Hash(i, Seed + 3)) * TwoPi;
		const float v = 50.0F + 60.0F * Unit(Hash(i, Seed + 4));
		if (age > 0.6F)
			continue;
		const float x = cx + std::cos(a) * v * age, y = cy - FrozenOrbRadius + std::sin(a) * v * age * 0.55F - 50.0F * age + 200.0F * age * age;
		if (y > cy + std::sin(a) * v * age * 0.55F + 2.0F)
			continue; // landed
		const float spin = age * 20.0F + static_cast<float>(i);
		const float k = 1.0F - age / 0.6F;
		for (int s = -2; s <= 2; s++) {
			const int ix = static_cast<int>(x + std::cos(spin) * static_cast<float>(s)), iy = static_cast<int>(y + std::sin(spin) * static_cast<float>(s));
			px.mix(ix, iy, s < 0 ? 225 : 130, s < 0 ? 242 : 180, s < 0 ? 255 : 235, 0.9F * k);
		}
	}
	// Crystals hanging in the air.
	for (uint32_t i = 0; i < 24; i++) {
		const float a = Unit(Hash(i, Seed + 5)) * TwoPi, r = 40.0F * std::sqrt(Unit(Hash(i, Seed + 6))) * Clamp01(age / 0.2F + 0.2F);
		const float x = cx + std::cos(a) * r, y = cy - 12.0F + std::sin(a) * r * 0.5F + 12.0F * age;
		const float tw = std::pow(0.5F + 0.5F * std::sin(age * 36.0F + static_cast<float>(i) * 1.9F), 5.0F);
		px.add(static_cast<int>(x), static_cast<int>(y), 240, 250, 255, (1.0F - age / FrozenOrbBurstSeconds) * (0.3F + 0.7F * tw));
	}
}

namespace {

constexpr float AzR = 120.0F;        // the frozen area across; half that down
constexpr float AzBlast = 0.3F;      // the detonation, after the implosion
constexpr float AzVortexFrom = 0.45F;
constexpr float AzVortexTo = 7.45F;  // seven seconds of vortex

/** When the detonation's front reached flat radius @p r. */
float AzReached(float r)
{
	return AzBlast + 0.4F * (r / (AzR * 1.4F)) * (r / (AzR * 1.4F));
}

/** A faceted spike of ice @p h tall and @p w wide at its foot (@p x, @p y), leaning @p lean, at strength @p k. */
void IceSpire(const Px &px, float x, float y, float h, float w, float lean, float k)
{
	for (float z = 0; z <= h; z += 1.0F) {
		const float t = z / h;
		const float half = w * (1.0F - t) + 0.5F;
		const float mx = x + lean * z;
		const int iy = static_cast<int>(y - z);
		for (int o = -static_cast<int>(half); o <= static_cast<int>(half); o++) {
			const float s = static_cast<float>(o) / std::max(1.0F, half);
			float r, g, b;
			if (s < -0.15F) { // the lit face
				r = 200 + 40 * t;
				g = 228 + 20 * t;
				b = 255;
			} else if (s < 0.1F) { // the ridge
				r = 245;
				g = 252;
				b = 255;
			} else { // the face in shadow
				r = 70 + 50 * t;
				g = 120 + 50 * t;
				b = 200 + 30 * t;
			}
			px.mix(static_cast<int>(mx) + o, iy, r, g, b, 0.9F * k);
		}
	}
	px.add(static_cast<int>(x + lean * h), static_cast<int>(y - h), 255, 255, 255, k);
}

} // namespace

void DrawAbsoluteZeroFx(const Surface &out, Point castAt, Point heroAt, float age, bool floorLayer)
{
	if (out.isIndexed() || age < 0 || age > AbsoluteZeroSeconds)
		return;
	const Px px { &out, out.w(), out.h() };
	constexpr uint32_t Seed = 5003;
	const float cx = static_cast<float>(castAt.x), cy = static_cast<float>(castAt.y);
	const float vx = static_cast<float>(heroAt.x), vy = static_cast<float>(heroAt.y);
	const float vortex = Clamp01((age - AzVortexFrom) / 0.5F) * Clamp01((AzVortexTo - age) / 0.6F);
	const float freeze = Clamp01((age - AzBlast) / 0.1F) * Clamp01(1.0F - (age - 6.8F) / 1.0F); // the ice sheet

	if (floorLayer) {
		// The implosion: the floor darkening as the warmth is drawn in.
		if (age < AzBlast + 0.1F) {
			const float k = Clamp01(age / AzBlast) * (age < AzBlast ? 1.0F : 1.0F - (age - AzBlast) / 0.1F);
			for (int dy = -static_cast<int>(AzR * 0.6F); dy <= static_cast<int>(AzR * 0.6F); dy++)
				for (int dx = -static_cast<int>(AzR * 1.2F); dx <= static_cast<int>(AzR * 1.2F); dx++) {
					const float d2 = static_cast<float>(dx * dx) / (AzR * AzR * 1.44F) + static_cast<float>(dy * dy) / (AzR * AzR * 0.36F);
					if (d2 < 1.0F)
						px.mix(static_cast<int>(cx) + dx, static_cast<int>(cy) + dy, 4, 8, 20, 0.45F * k * (1.0F - d2));
				}
		}
		// The ice sheet: the floor frozen glassy over the area, cracked, a sheen sliding across it; thawing at the end.
		if (freeze > 0) {
			const float sheet = AzR * Clamp01((age - AzBlast) / 0.35F);
			const float slide = age * 40.0F;
			for (int dy = -static_cast<int>(sheet * 0.5F); dy <= static_cast<int>(sheet * 0.5F); dy++)
				for (int dx = -static_cast<int>(sheet); dx <= static_cast<int>(sheet); dx++) {
					const float d = std::sqrt(static_cast<float>(dx * dx) + static_cast<float>(dy * dy) * 4.0F);
					if (d > sheet)
						continue;
					const float edge = Clamp01((sheet - d) / 10.0F);
					const float gloss = std::pow(0.5F + 0.5F * std::sin((static_cast<float>(dx) + static_cast<float>(dy) * 1.6F - slide) * 0.045F), 8.0F);
					px.mix(static_cast<int>(cx) + dx, static_cast<int>(cy) + dy, 150, 196, 236, 0.36F * freeze * edge);
					px.add(static_cast<int>(cx) + dx, static_cast<int>(cy) + dy, 120, 170, 255, (0.10F + 0.22F * gloss) * freeze * edge);
				}
			// Cracks across the ice: white lines branching out from under her.
			for (uint32_t i = 0; i < 14; i++) {
				float ang = Unit(Hash(i, Seed)) * TwoPi;
				float x = cx, y = cy;
				const float len = Clamp01((age - AzBlast) / 0.25F) * (0.6F + 0.4F * Unit(Hash(i, Seed + 1)));
				for (int k = 1; k <= 12; k++) {
					const float q = static_cast<float>(k) / 12.0F;
					if (q > len)
						break;
					ang += (Unit(Hash(i, Seed + 2, static_cast<uint32_t>(k))) - 0.5F) * 0.5F;
					const float nx = cx + std::cos(ang) * AzR * q, ny = cy + std::sin(ang) * AzR * q * 0.5F;
					const int steps = static_cast<int>(std::max(std::abs(nx - x), std::abs(ny - y))) + 1;
					for (int j = 0; j <= steps; j++) {
						const float u = static_cast<float>(j) / static_cast<float>(steps);
						px.mix(static_cast<int>(x + (nx - x) * u), static_cast<int>(y + (ny - y) * u), 235, 248, 255, 0.75F * freeze);
					}
					x = nx;
					y = ny;
				}
			}
		}
		// The detonation's shockwave running out, the air bending with it.
		const float sinceBlast = age - AzBlast;
		if (sinceBlast >= 0 && sinceBlast < 0.5F) {
			const float u = sinceBlast / 0.5F;
			const float Rr = AzR * 1.4F * std::sqrt(u);
			for (int s = 0; s < 480; s++) {
				const float a = static_cast<float>(s) / 480.0F * TwoPi;
				for (int w = -4; w <= 4; w++)
					px.add(static_cast<int>(cx + std::cos(a) * (Rr + static_cast<float>(w))), static_cast<int>(cy + std::sin(a) * (Rr + static_cast<float>(w)) * 0.5F), 220, 240, 255,
					    0.8F * (1.0F - u) * (1.0F - std::abs(static_cast<float>(w)) / 5.0F));
			}
			Ripple(out, castAt, Rr, 8.0F * (1.0F - u));
		}
		// The vortex's arms of frost light turning on the ice round her, and a ring pulsing with each strike.
		if (vortex > 0) {
			const float spin = age * 2.4F;
			for (int arm = 0; arm < 3; arm++)
				for (float r = 8.0F; r < AzR; r += 1.5F) {
					const float a = spin + static_cast<float>(arm) * TwoPi / 3.0F + r * 0.035F;
					const float k = vortex * 0.35F * (1.0F - r / AzR);
					for (int w = -2; w <= 2; w++)
						px.add(static_cast<int>(vx + std::cos(a) * r + static_cast<float>(w)), static_cast<int>(vy + std::sin(a) * r * 0.5F), 140, 200, 255, k * (1.0F - std::abs(static_cast<float>(w)) / 3.0F));
				}
			const float pulse = Frac((age - AzVortexFrom) / 0.25F);
			const float pk = vortex * (1.0F - pulse) * 0.45F;
			const float Rp = AzR * (0.75F + 0.25F * pulse);
			for (int s = 0; s < 240; s++) {
				const float a = static_cast<float>(s) / 240.0F * TwoPi;
				px.add(static_cast<int>(vx + std::cos(a) * Rp), static_cast<int>(vy + std::sin(a) * Rp * 0.5F), 190, 225, 255, pk);
			}
		}
		return;
	}

	// ---- In the air ----
	// The implosion: streams of mist and light drawn inward to a swelling core above her.
	if (age < AzBlast) {
		const float u = age / AzBlast;
		for (uint32_t i = 0; i < 40; i++) {
			const float a = Unit(Hash(i, Seed + 10)) * TwoPi;
			const float r0 = AzR * (0.8F + 0.6F * Unit(Hash(i, Seed + 11)));
			const float r1 = r0 * (1.0F - u * u);
			const float h = 40.0F * u;
			for (float s = 0; s < 14.0F; s += 1.0F) {
				const float r = r1 + s * 1.5F;
				px.add(static_cast<int>(cx + std::cos(a) * r), static_cast<int>(cy + std::sin(a) * r * 0.5F - h * (1.0F - r / (r0 + 30.0F))), 170, 215, 255, 0.5F * (1.0F - s / 14.0F) * u);
			}
		}
		const float core = 6.0F + 18.0F * u;
		for (int dy = -static_cast<int>(core); dy <= static_cast<int>(core); dy++)
			for (int dx = -static_cast<int>(core); dx <= static_cast<int>(core); dx++) {
				const float d2 = static_cast<float>(dx * dx + dy * dy) / (core * core);
				if (d2 < 1.0F)
					px.add(static_cast<int>(cx) + dx, static_cast<int>(cy) - 46 + dy, 200, 235, 255, u * (1.0F - d2) * (1.0F - d2));
			}
	}
	// The detonation: a blinding flash.
	const float sinceBlast = age - AzBlast;
	if (sinceBlast >= 0 && sinceBlast < 0.5F) {
		const float f = std::exp(-sinceBlast / 0.1F);
		for (int dy = -70; dy <= 70; dy++)
			for (int dx = -110; dx <= 110; dx++) {
				const float d2 = static_cast<float>(dx * dx) / 12100.0F + static_cast<float>(dy * dy) / 4900.0F;
				if (d2 < 1.0F)
					px.add(static_cast<int>(cx) + dx, static_cast<int>(cy) - 30 + dy, 235, 248, 255, f * (1.0F - d2));
			}
	}
	// The spires: great faceted spikes of ice erupting round her as the blast reaches them, standing through the freeze,
	// then shattering.
	for (uint32_t i = 0; i < 18; i++) {
		const float a = (static_cast<float>(i) + Unit(Hash(i, Seed + 20)) * 0.8F) / 18.0F * TwoPi;
		const float r = AzR * (0.55F + 0.5F * Unit(Hash(i, Seed + 21)));
		const float t = age - AzReached(r);
		if (t < 0)
			continue;
		const float x = cx + std::cos(a) * r, y = cy + std::sin(a) * r * 0.5F;
		const float h = 26.0F + 36.0F * Unit(Hash(i, Seed + 22));
		const float w = 5.0F + 4.0F * Unit(Hash(i, Seed + 23));
		const float lean = std::cos(a) * 0.3F + (Unit(Hash(i, Seed + 24)) - 0.5F) * 0.2F;
		const float stand = 2.0F + 0.15F * Unit(Hash(i, Seed + 25));
		if (t < stand) {
			const float up = Clamp01(t / 0.07F);
			IceSpire(px, x, y, h * up, w, lean, 1.0F);
			if (Unit(Hash(i, Seed + 26)) > 0.4F) // a smaller one beside it
				IceSpire(px, x + 7.0F, y + 2.0F, h * 0.5F * up, w * 0.6F, lean + 0.25F, 1.0F);
			continue;
		}
		const float ts = t - stand;
		if (ts > 0.6F)
			continue;
		for (uint32_t s = 0; s < 8; s++) {
			const float sa = Unit(Hash(i, Seed + 30, s)) * TwoPi;
			const float sv = 30.0F + 50.0F * Unit(Hash(i, Seed + 31, s));
			const float sz = h * Unit(Hash(i, Seed + 32, s));
			const float sx = x + std::cos(sa) * sv * ts, sy = y - sz + std::sin(sa) * sv * ts * 0.5F - 30.0F * ts + 260.0F * ts * ts;
			if (sy > y + 3.0F)
				continue;
			const float k = 1.0F - ts / 0.6F;
			px.mix(static_cast<int>(sx), static_cast<int>(sy), 225, 242, 255, k);
			px.mix(static_cast<int>(sx) + 1, static_cast<int>(sy), 120, 170, 230, k);
			px.mix(static_cast<int>(sx), static_cast<int>(sy) + 1, 160, 205, 245, k);
		}
	}
	// The vortex: a funnel of snow spinning round her, wider as it rises; shards of ice orbiting in it; mist rolling at its foot.
	if (vortex > 0) {
		for (uint32_t i = 0; i < 220; i++) {
			const float hz = Unit(Hash(i, Seed + 40)); // 0 at the floor, 1 at the top
			const float h = 8.0F + 120.0F * hz;
			const float r = (16.0F + AzR * 0.55F * hz + 28.0F * Unit(Hash(i, Seed + 41))) * (0.85F + 0.15F * std::sin(age * 3.0F + static_cast<float>(i)));
			const float w = 3.5F + 50.0F / r;
			const float a = Unit(Hash(i, Seed + 42)) * TwoPi + w * age;
			const float x = vx + std::cos(a) * r, y = vy + std::sin(a) * r * 0.45F - h;
			const float back = std::sin(a) < 0 ? 0.45F : 1.0F; // the far side dimmer
			const float k = vortex * back * (0.4F + 0.6F * (1.0F - hz * 0.6F));
			// A short streak along its turn.
			const float tx = -std::sin(a), ty = std::cos(a) * 0.45F;
			for (int s = 0; s < 4; s++)
				px.add(static_cast<int>(x - tx * static_cast<float>(s)), static_cast<int>(y - ty * static_cast<float>(s)), 230, 245, 255, k * (1.0F - static_cast<float>(s) / 4.0F));
		}
		for (uint32_t i = 0; i < 10; i++) {
			const float a = static_cast<float>(i) / 10.0F * TwoPi + age * 2.2F;
			const float r = AzR * (0.6F + 0.15F * std::sin(age * 1.7F + static_cast<float>(i)));
			const float h = 18.0F + 22.0F * Unit(Hash(i, Seed + 50)) + 6.0F * std::sin(age * 4.0F + static_cast<float>(i));
			const float x = vx + std::cos(a) * r, y = vy + std::sin(a) * r * 0.5F - h;
			const float back = std::sin(a) < 0 ? 0.5F : 1.0F;
			const float spin = age * 9.0F + static_cast<float>(i);
			for (int s = -4; s <= 4; s++) {
				const float e = 1.0F - std::abs(static_cast<float>(s)) / 5.0F;
				px.mix(static_cast<int>(x + std::cos(spin) * static_cast<float>(s)), static_cast<int>(y + std::sin(spin) * static_cast<float>(s) * 0.6F), s < 0 ? 230 : 130, s < 0 ? 245 : 180, 255, 0.9F * vortex * back * e);
			}
			px.add(static_cast<int>(x), static_cast<int>(y) - 1, 255, 255, 255, 0.6F * vortex * back);
		}
		for (uint32_t i = 0; i < 16; i++) {
			const float a = static_cast<float>(i) / 16.0F * TwoPi + age * 1.6F;
			const float r = AzR * 0.85F;
			const float x = vx + std::cos(a) * r, y = vy + std::sin(a) * r * 0.5F - 6.0F;
			const float size = 12.0F + 4.0F * std::sin(age * 2.0F + static_cast<float>(i));
			const int s = static_cast<int>(size);
			for (int dy = -s / 2; dy <= s / 2; dy++)
				for (int dx = -s; dx <= s; dx++) {
					const float d2 = static_cast<float>(dx * dx) / (size * size) + static_cast<float>(dy * dy * 4) / (size * size);
					if (d2 < 1.0F)
						px.mix(static_cast<int>(x) + dx, static_cast<int>(y) + dy, 214, 234, 255, 0.28F * vortex * (1.0F - d2));
				}
		}
	}
}

void DrawIceEncaseFx(const Surface &out, Point foot, float age, bool floorLayer)
{
	if (out.isIndexed() || age < 0 || age > IceEncaseSeconds + 0.6F)
		return;
	const Px px { &out, out.w(), out.h() };
	const float fx = static_cast<float>(foot.x), fy = static_cast<float>(foot.y);
	if (floorLayer) {
		const float k = Clamp01(age / 0.1F) * Clamp01(1.0F - (age - IceEncaseSeconds) / 0.3F);
		for (int dy = -8; dy <= 8; dy++)
			for (int dx = -20; dx <= 20; dx++) {
				const float d2 = static_cast<float>(dx * dx) / 400.0F + static_cast<float>(dy * dy) / 64.0F;
				if (d2 < 1.0F)
					px.mix(static_cast<int>(fx) + dx, static_cast<int>(fy) + dy, 190, 225, 250, 0.5F * k * (1.0F - d2));
			}
		return;
	}
	if (age < IceEncaseSeconds) {
		// A block of ice grown up round the body: translucent, its faces lit differently, bright edges, a glint sliding down.
		const float up = Clamp01(age / 0.12F);
		const float H = 66.0F * up, W = 19.0F;
		const float glint = Frac(age * 0.7F) * 90.0F - 10.0F;
		for (float z = 0; z <= H; z += 1.0F) {
			const float taper = z > H - 12.0F ? (H - z) / 12.0F : 1.0F; // the top cut at an angle
			const int half = static_cast<int>(W * (0.55F + 0.45F * taper));
			const int iy = static_cast<int>(fy - z);
			for (int o = -half; o <= half; o++) {
				const float s = static_cast<float>(o) / static_cast<float>(half);
				const bool edgeLine = std::abs(o) == half || std::abs(o) == half - 1 || std::abs(s) < 0.06F || z >= H - 1.0F;
				if (edgeLine) {
					px.mix(static_cast<int>(fx) + o, iy, 230, 246, 255, 0.75F);
					continue;
				}
				if (s < 0)
					px.mix(static_cast<int>(fx) + o, iy, 175, 215, 250, 0.32F);
				else
					px.mix(static_cast<int>(fx) + o, iy, 90, 140, 215, 0.36F);
				const float g = 1.0F - std::abs((H - z) - glint - s * 10.0F) / 4.0F;
				if (g > 0)
					px.add(static_cast<int>(fx) + o, iy, 255, 255, 255, 0.5F * g);
			}
		}
		return;
	}
	// Shattering: the block in pieces, thrown out and falling.
	const float ts = age - IceEncaseSeconds;
	for (uint32_t i = 0; i < 22; i++) {
		const float a = Unit(Hash(i, 5101)) * TwoPi;
		const float v = 30.0F + 50.0F * Unit(Hash(i, 5102));
		const float z = 66.0F * Unit(Hash(i, 5103));
		const float x = fx + std::cos(a) * v * ts, y = fy - z + std::sin(a) * v * ts * 0.5F - 20.0F * ts + 300.0F * ts * ts;
		if (y > fy + 3.0F)
			continue;
		const float k = 1.0F - ts / 0.6F;
		for (int s = 0; s < 3; s++)
			px.mix(static_cast<int>(x) + s, static_cast<int>(y), s == 0 ? 235 : 140, s == 0 ? 248 : 190, 255, 0.85F * k);
	}
}

void ApplyAbsoluteZeroGrade(const Surface &out, float age)
{
	if (out.isIndexed() || age < AzBlast || age > AbsoluteZeroSeconds)
		return;
	// The world draining to frozen blue while it lasts, and frost creeping in over the edges of the picture.
	const float k = Clamp01((age - AzBlast) / 0.5F) * Clamp01((AbsoluteZeroSeconds - age) / 0.8F);
	const float creep = Clamp01((age - AzBlast) / 1.2F) * Clamp01((AbsoluteZeroSeconds - age) / 0.8F);
	const int W = out.w(), H = out.h();
	const float reach = 70.0F * creep; // how far in from the edge the frost has grown
	for (int y = 0; y < H; y++) {
		uint32_t *row = out.at<uint32_t>(0, y);
		for (int x = 0; x < W; x++) {
			const uint32_t c = row[x];
			float r = static_cast<float>((c >> 16) & 0xFF), g = static_cast<float>((c >> 8) & 0xFF), b = static_cast<float>(c & 0xFF);
			const float l = 0.3F * r + 0.59F * g + 0.11F * b;
			// Desaturated and cooled.
			r += (l * 0.78F - r) * 0.55F * k;
			g += (l * 0.95F - g) * 0.55F * k;
			b += (std::min(255.0F, l * 1.25F + 12.0F) - b) * 0.55F * k;
			// The frost at the edges: a crystalline rime, thicker at the corners, with a fern-like grain.
			const float edge = std::min({ static_cast<float>(x), static_cast<float>(W - 1 - x), static_cast<float>(y) * 1.3F, static_cast<float>(H - 1 - y) * 1.3F });
			// Ragged: the frost reaches further in some places than others.
			const float ragged = 0.5F + 0.5F * std::sin(static_cast<float>(x) * 0.043F + std::sin(static_cast<float>(y) * 0.031F) * 2.0F) * std::sin(static_cast<float>(y) * 0.052F + std::sin(static_cast<float>(x) * 0.027F) * 2.0F);
			const float reachHere = reach * (0.45F + 0.75F * ragged);
			if (edge < reachHere) {
				const float grain = 0.5F + 0.5F * std::sin(static_cast<float>(x) * 0.31F + std::sin(static_cast<float>(y) * 0.17F) * 3.0F) * std::sin(static_cast<float>(y) * 0.27F + std::sin(static_cast<float>(x) * 0.13F) * 3.0F);
				const float f = std::pow(1.0F - edge / std::max(1.0F, reachHere), 1.5F) * (0.8F + 0.2F * grain);
				r += (215.0F - r) * 0.75F * f;
				g += (235.0F - g) * 0.75F * f;
				b += (255.0F - b) * 0.75F * f;
			}
			row[x] = (static_cast<uint32_t>(std::min(255.0F, r)) << 16) | (static_cast<uint32_t>(std::min(255.0F, g)) << 8) | static_cast<uint32_t>(std::min(255.0F, b));
		}
	}
}

namespace {

/** A jagged bolt from (@p x0, @p y0) to (@p x1, @p y1) by midpoint displacement, @p rough pixels at its first split. */
void ArcBolt(const Px &px, float x0, float y0, float x1, float y1, float rough, uint32_t seed, float k, bool thick)
{
	float xs[33], ys[33];
	xs[0] = x0;
	ys[0] = y0;
	xs[32] = x1;
	ys[32] = y1;
	for (int step = 16, level = 0; step >= 1; step /= 2, level++) {
		const float spread = rough / static_cast<float>(1 << level);
		for (int i = step; i < 32; i += step * 2) {
			const float mx = (xs[i - step] + xs[i + step]) * 0.5F, my = (ys[i - step] + ys[i + step]) * 0.5F;
			const float sx = -(ys[i + step] - ys[i - step]), sy = xs[i + step] - xs[i - step];
			const float l = std::max(1.0F, std::sqrt(sx * sx + sy * sy));
			const float off = (Unit(Hash(seed, static_cast<uint32_t>(i), static_cast<uint32_t>(level))) - 0.5F) * spread;
			xs[i] = mx + sx / l * off;
			ys[i] = my + sy / l * off;
		}
	}
	for (int i = 0; i < 32; i++) {
		const int n = static_cast<int>(std::max(std::abs(xs[i + 1] - xs[i]), std::abs(ys[i + 1] - ys[i]))) + 1;
		for (int j = 0; j <= n; j++) {
			const float u = static_cast<float>(j) / static_cast<float>(n);
			const int x = static_cast<int>(xs[i] + (xs[i + 1] - xs[i]) * u), y = static_cast<int>(ys[i] + (ys[i + 1] - ys[i]) * u);
			px.add(x, y, 235, 245, 255, k);
			px.add(x + 1, y, 120, 170, 255, 0.5F * k);
			px.add(x - 1, y, 120, 170, 255, 0.5F * k);
			px.add(x, y - 1, 120, 170, 255, 0.4F * k);
			if (thick) {
				px.add(x, y + 1, 200, 225, 255, 0.7F * k);
				px.add(x + 2, y, 80, 130, 255, 0.25F * k);
				px.add(x - 2, y, 80, 130, 255, 0.25F * k);
			}
		}
	}
}

/** A soft round glow, added. */
void Halo(const Px &px, float cx, float cy, float rx, float ry, float r, float g, float b, float k)
{
	if (k <= 0.004F)
		return;
	for (int dy = -static_cast<int>(ry); dy <= static_cast<int>(ry); dy++)
		for (int dx = -static_cast<int>(rx); dx <= static_cast<int>(rx); dx++) {
			const float d2 = static_cast<float>(dx * dx) / (rx * rx) + static_cast<float>(dy * dy) / (ry * ry);
			if (d2 < 1.0F)
				px.add(static_cast<int>(cx) + dx, static_cast<int>(cy) + dy, r, g, b, k * (1.0F - d2) * (1.0F - d2));
		}
}

/** Fire's colour at heat @p h (1 white-hot .. 0 dying red), added at strength @p k. */
void FireAdd(const Px &px, int x, int y, float h, float k)
{
	const float r = 255.0F, g = 60.0F + 190.0F * std::min(1.0F, h * 1.2F), b = 20.0F + 200.0F * std::max(0.0F, h - 0.6F) * 2.5F;
	px.add(x, y, r, g, std::min(255.0F, b), k);
}

/** A tongue of flame @p h tall from (@p x, @p y), swaying with @p t; @p w wide at its foot. */
void FlameTongue(const Px &px, float x, float y, float h, float w, float t, float sway, float k)
{
	for (float z = 0; z <= h; z += 1.0F) {
		const float q = z / h;
		const float half = w * std::pow(1.0F - q, 0.7F) * (0.6F + 0.4F * std::sin(q * Pi));
		const float mx = x + std::sin(z * 0.22F + t * 13.0F + sway) * z * 0.12F;
		for (int o = -static_cast<int>(half + 0.5F); o <= static_cast<int>(half + 0.5F); o++) {
			const float e = 1.0F - std::abs(static_cast<float>(o)) / (half + 1.0F);
			FireAdd(px, static_cast<int>(mx) + o, static_cast<int>(y - z), (1.0F - q) * e, k * (0.45F + 0.55F * e) * (1.0F - q * 0.5F));
		}
	}
}

} // namespace

void DrawStaticChargeFx(const Surface &out, Point foot, float age, bool floorLayer)
{
	if (out.isIndexed() || age < 0)
		return;
	const Px px { &out, out.w(), out.h() };
	constexpr uint32_t Seed = 5203;
	const float fx = static_cast<float>(foot.x), fy = static_cast<float>(foot.y);
	const float on = Clamp01(age / 0.3F);
	// A surge every so often: everything brighter for a moment.
	const float surge = std::pow(std::max(0.0F, std::sin(age * 5.6F)), 24.0F);
	if (floorLayer) {
		Halo(px, fx, fy, 30, 14, 90, 150, 255, (0.22F + 0.25F * surge) * on);
		// Sparks crackling on the floor round his feet.
		const uint32_t tick = static_cast<uint32_t>(age * 14.0F);
		for (uint32_t i = 0; i < 6; i++) {
			const uint32_t h = Hash(i, tick, Seed);
			const float a = Unit(h) * TwoPi, r = 10.0F + 14.0F * Unit(h >> 8);
			px.add(static_cast<int>(fx + std::cos(a) * r), static_cast<int>(fy + std::sin(a) * r * 0.5F), 200, 230, 255, 0.9F * on);
		}
		return;
	}
	// The charge crawling over him: arcs between points round his body, new ones fourteen times a second.
	const uint32_t tick = static_cast<uint32_t>(age * 14.0F);
	const float cx = fx, cy = fy - 30.0F;
	const int arcs = 4 + static_cast<int>(surge * 4.0F);
	for (int i = 0; i < arcs; i++) {
		const uint32_t h = Hash(static_cast<uint32_t>(i), tick, Seed + 1);
		const float a0 = Unit(h) * TwoPi, a1 = a0 + (0.5F + 1.2F * Unit(h >> 8)) * ((h & 1) ? 1.0F : -1.0F);
		const float x0 = cx + std::cos(a0) * 13.0F, y0 = cy + std::sin(a0) * 26.0F, x1 = cx + std::cos(a1) * 13.0F, y1 = cy + std::sin(a1) * 26.0F;
		ArcBolt(px, x0, y0, x1, y1, 9.0F, h, (0.55F + 0.45F * surge) * on, false);
	}
	// Now and then an arc leaping to the floor.
	if ((Hash(tick, Seed + 2) & 3) == 0) {
		const uint32_t h = Hash(tick, Seed + 3);
		const float a = Unit(h) * TwoPi;
		ArcBolt(px, cx + std::cos(a) * 10.0F, cy + 20.0F, fx + std::cos(a) * 26.0F, fy + std::sin(a) * 10.0F, 8.0F, h, 0.8F * on, false);
	}
	// Charged motes circling him, each with a short tail.
	for (int i = 0; i < 6; i++) {
		const float a = age * (2.6F + 0.3F * static_cast<float>(i)) + static_cast<float>(i) * 1.05F;
		const float h = -12.0F + 28.0F * Unit(Hash(static_cast<uint32_t>(i), Seed + 4)) + 4.0F * std::sin(age * 3.0F + static_cast<float>(i));
		for (int s = 0; s < 5; s++) {
			const float aa = a - static_cast<float>(s) * 0.09F;
			const float x = cx + std::cos(aa) * 20.0F, y = cy + h + std::sin(aa) * 8.0F;
			const float back = std::sin(aa) < 0 ? 0.45F : 1.0F;
			px.add(static_cast<int>(x), static_cast<int>(y), 200, 230, 255, (1.0F - static_cast<float>(s) / 5.0F) * 0.9F * back * on);
		}
	}
	Halo(px, cx, cy, 20, 32, 90, 150, 255, (0.12F + 0.3F * surge) * on);
}

void DrawStaticDischargeFx(const Surface &out, Point from, Point to, float age)
{
	if (out.isIndexed() || age < 0 || age > StaticDischargeSeconds)
		return;
	const Px px { &out, out.w(), out.h() };
	const float k = std::exp(-age / 0.09F) * 0.7F + 0.3F * (1.0F - age / StaticDischargeSeconds);
	const uint32_t seed = Hash(static_cast<uint32_t>(age * 30.0F), static_cast<uint32_t>(to.x * 7 + to.y), 5301);
	const float x0 = static_cast<float>(from.x), y0 = static_cast<float>(from.y) - 30.0F;
	const float x1 = static_cast<float>(to.x), y1 = static_cast<float>(to.y) - 30.0F;
	ArcBolt(px, x0, y0, x1, y1, 22.0F, seed, k, true);
	ArcBolt(px, x0, y0, x1 + 6.0F, y1 - 8.0F, 16.0F, seed + 9, 0.5F * k, false);
	Halo(px, x1, y1, 22, 26, 140, 190, 255, 0.8F * k);
	Halo(px, x0, y0, 14, 18, 140, 190, 255, 0.5F * k);
	// Sparks thrown off where it struck.
	for (uint32_t i = 0; i < 10; i++) {
		const float a = Unit(Hash(i, 5302)) * TwoPi, v = 40.0F + 60.0F * Unit(Hash(i, 5303));
		const float x = x1 + std::cos(a) * v * age, y = y1 + std::sin(a) * v * age + 150.0F * age * age;
		px.add(static_cast<int>(x), static_cast<int>(y), 220, 240, 255, 1.0F - age / StaticDischargeSeconds);
	}
}

void DrawCinderTouchFx(const Surface &out, Point foot, Point target, float age, bool floorLayer)
{
	if (out.isIndexed() || age < 0 || age > CinderTouchSeconds)
		return;
	const Px px { &out, out.w(), out.h() };
	constexpr uint32_t Seed = 5401;
	const float hx = static_cast<float>(foot.x), hy = static_cast<float>(foot.y) - 30.0F;
	const float tx = static_cast<float>(target.x), ty = static_cast<float>(target.y);
	if (floorLayer) {
		// The fire's light on the floor under it, flickering; a scorched ring where it stood, cooling.
		const float burn = Clamp01((age - 0.08F) / 0.1F) * Clamp01((CinderTouchSeconds - 0.3F - age) / 0.4F);
		const float flick = 0.8F + 0.2F * std::sin(age * 23.0F) * std::sin(age * 9.0F);
		Halo(px, tx, ty, 46, 22, 255, 130, 40, 0.45F * burn * flick);
		for (int dy = -7; dy <= 7; dy++)
			for (int dx = -18; dx <= 18; dx++) {
				const float d2 = static_cast<float>(dx * dx) / 324.0F + static_cast<float>(dy * dy) / 49.0F;
				if (d2 < 1.0F)
					px.mix(static_cast<int>(tx) + dx, static_cast<int>(ty) + dy, 26, 16, 10, 0.35F * Clamp01(age / 0.5F) * (1.0F - d2));
			}
		return;
	}
	// The touch: embers swept from her hand onto it, and a flash where they strike.
	if (age < 0.25F) {
		for (uint32_t i = 0; i < 26; i++) {
			const float t0 = Unit(Hash(i, Seed)) * 0.08F;
			const float u = Clamp01((age - t0) / 0.12F);
			if (u <= 0 || u >= 1.0F)
				continue;
			const float bend = (Unit(Hash(i, Seed + 1)) - 0.5F) * 30.0F * std::sin(u * Pi);
			const float x = hx + (tx - hx) * u, y = hy + (ty - 30.0F - hy) * u + bend;
			FireAdd(px, static_cast<int>(x), static_cast<int>(y), 0.9F, 1.0F);
			FireAdd(px, static_cast<int>(x) - 1, static_cast<int>(y), 0.5F, 0.6F);
		}
	}
	const float flash = std::exp(-std::max(0.0F, age - 0.1F) / 0.08F) * (age > 0.1F ? 1.0F : 0.0F);
	Halo(px, tx, ty - 30.0F, 24, 30, 255, 200, 110, flash);
	// It burns: tongues of flame licking up its body, flickering, embers and smoke rising off it.
	const float burn = Clamp01((age - 0.1F) / 0.15F) * Clamp01((CinderTouchSeconds - 0.3F - age) / 0.5F);
	if (burn > 0) {
		for (int j = 0; j < 7; j++) {
			const float ox = (static_cast<float>(j) - 3.0F) * 4.2F + std::sin(age * 3.0F + static_cast<float>(j)) * 1.5F;
			const float base = ty - 6.0F - 30.0F * Unit(Hash(static_cast<uint32_t>(j), Seed + 2)) * (1.0F - std::abs(static_cast<float>(j) - 3.0F) / 4.0F);
			const float h = (18.0F + 20.0F * Unit(Hash(static_cast<uint32_t>(j), Seed + 3))) * (0.7F + 0.3F * std::sin(age * (11.0F + static_cast<float>(j)) + static_cast<float>(j) * 2.0F)) * burn;
			FlameTongue(px, tx + ox, base, h, 4.0F + 2.5F * Unit(Hash(static_cast<uint32_t>(j), Seed + 4)), age, static_cast<float>(j) * 1.3F, 0.85F * burn);
		}
		for (uint32_t i = 0; i < 18; i++) {
			const float period = 0.9F;
			const float ph = Frac(age / period + Unit(Hash(i, Seed + 5)));
			const float x = tx + (Unit(Hash(i, Seed + 6)) - 0.5F) * 26.0F + std::sin(ph * 6.0F + static_cast<float>(i)) * 4.0F;
			const float y = ty - 20.0F - 70.0F * ph;
			FireAdd(px, static_cast<int>(x), static_cast<int>(y), 0.7F * (1.0F - ph), (1.0F - ph) * burn);
		}
		for (uint32_t i = 0; i < 5; i++) {
			const float ph = Frac(age / 1.4F + static_cast<float>(i) / 5.0F);
			const float x = tx + std::sin(ph * 4.0F + static_cast<float>(i)) * 6.0F, y = ty - 50.0F - 40.0F * ph;
			const float size = 4.0F + 8.0F * ph;
			const int s = static_cast<int>(size);
			for (int dy = -s; dy <= s; dy++)
				for (int dx = -s; dx <= s; dx++) {
					const float d2 = static_cast<float>(dx * dx + dy * dy) / (size * size);
					if (d2 < 1.0F)
						px.mix(static_cast<int>(x) + dx, static_cast<int>(y) + dy, 50, 44, 40, 0.22F * std::sin(ph * Pi) * burn * (1.0F - d2));
				}
		}
		Halo(px, tx, ty - 26.0F, 22, 30, 255, 140, 50, 0.3F * burn);
	}
}

void DrawFurnaceFlameFx(const Surface &out, Point floor, int dirX, int dirY, float age, bool floorLayer)
{
	if (out.isIndexed() || age < 0 || age > FurnaceFlameSeconds || (dirX == 0 && dirY == 0))
		return;
	const Px px { &out, out.w(), out.h() };
	constexpr uint32_t Seed = 5503;
	// Its head on the furnace: forward of the floor point along the facing, raised to the dragon's mouth.
	const float stepX = static_cast<float>(dirX - dirY) * 32.0F, stepY = static_cast<float>(dirX + dirY) * 16.0F;
	const float sl = std::sqrt(stepX * stepX + stepY * stepY);
	const float ux = stepX / sl, uy = stepY / sl;
	const float mx = static_cast<float>(floor.x) + ux * 20.0F, my = static_cast<float>(floor.y) + uy * 20.0F - 34.0F;
	const float reach = sl * 3.2F;
	// The breath: catching, roaring, dying away.
	const float roar = Clamp01(age / 0.06F) * Clamp01((FurnaceFlameSeconds - 0.12F - age) / 0.2F);
	if (floorLayer) {
		// Its light running out along the floor under the stream, and the floor scorching where it lands.
		for (float s = 10.0F; s < reach; s += 4.0F) {
			const float q = s / reach;
			const float flick = 0.7F + 0.3F * std::sin(age * 31.0F + s * 0.2F);
			Halo(px, static_cast<float>(floor.x) + ux * s, static_cast<float>(floor.y) + uy * s, 18.0F + 16.0F * q, 9.0F + 8.0F * q, 255, 140, 40, 0.07F * roar * flick * (1.0F - q * 0.4F));
		}
		const float scorch = Clamp01(age / 0.4F) * Clamp01((FurnaceFlameSeconds + 0.6F - age) / 0.8F);
		Halo(px, static_cast<float>(floor.x) + ux * reach * 0.8F, static_cast<float>(floor.y) + uy * reach * 0.8F, 26, 12, 255, 90, 20, 0.25F * scorch);
		Ripple(out, { static_cast<int>(static_cast<float>(floor.x) + ux * reach * 0.6F), static_cast<int>(static_cast<float>(floor.y) + uy * reach * 0.6F) - 20 }, 24.0F, 2.5F * roar);
		return;
	}
	// The flame: a stream of burning gas from the mouth - white-hot near it, through yellow and orange to dying red, then
	// smoke - widening and curling as it goes, sinking toward the floor at its far end and rising as it dies.
	constexpr int N = 380;
	constexpr float Life = 0.42F, Emit = 0.72F;
	for (int i = 0; i < N; i++) {
		const float born = static_cast<float>(i) / static_cast<float>(N) * Emit;
		const float t = age - born;
		if (t < 0 || t > Life)
			continue;
		const float u = t / Life;
		const uint32_t h = Hash(static_cast<uint32_t>(i), Seed);
		const float dist = reach * (1.0F - std::exp(-u * 2.2F)) / (1.0F - std::exp(-2.2F));
		const float spread = (Unit(h) - 0.5F) * dist * 0.32F;
		const float curl = std::sin(t * 14.0F + static_cast<float>(i)) * dist * 0.06F;
		const float drop = 22.0F * (dist / reach) * (dist / reach); // the stream falls toward the floor
		const float rise = 26.0F * std::max(0.0F, u - 0.6F);     // the burnt-out end lifts as smoke
		const float x = mx + ux * dist - uy * (spread + curl);
		const float y = my + uy * dist + ux * (spread + curl) * 0.5F + drop - rise;
		const float size = 3.5F + 15.0F * u;
		const float heat = 1.0F - u;
		const int s = static_cast<int>(size);
		if (u < 0.75F) {
			for (int dy = -s; dy <= s; dy++)
				for (int dx = -s; dx <= s; dx++) {
					const float d2 = static_cast<float>(dx * dx + dy * dy) / (size * size);
					if (d2 < 1.0F)
						FireAdd(px, static_cast<int>(x) + dx, static_cast<int>(y) + dy, heat * (1.0F - d2 * 0.5F), 0.16F * (1.0F - d2) * roar);
				}
		} else {
			for (int dy = -s; dy <= s; dy++)
				for (int dx = -s; dx <= s; dx++) {
					const float d2 = static_cast<float>(dx * dx + dy * dy) / (size * size);
					if (d2 < 1.0F)
						px.mix(static_cast<int>(x) + dx, static_cast<int>(y) + dy, 46, 40, 36, 0.18F * (1.0F - d2) * (1.0F - u) * 4.0F * roar);
				}
		}
	}
	// The white-hot core at the mouth.
	for (float s = 0; s < 26.0F; s += 1.0F)
		Halo(px, mx + ux * s, my + uy * s + 0.5F * s * s / reach, 3.0F, 3.0F, 255, 245, 200, 0.5F * roar * (1.0F - s / 26.0F));
	Halo(px, mx, my, 10, 8, 255, 220, 140, 0.8F * roar);
	// Embers thrown out of the stream, arcing and falling.
	for (uint32_t i = 0; i < 24; i++) {
		const float born = Unit(Hash(i, Seed + 1)) * Emit;
		const float t = age - born;
		if (t < 0 || t > 0.6F)
			continue;
		const float v = 120.0F + 120.0F * Unit(Hash(i, Seed + 2));
		const float side = (Unit(Hash(i, Seed + 3)) - 0.5F) * 80.0F;
		const float x = mx + ux * v * t - uy * side * t, y = my + uy * v * t + ux * side * t * 0.5F - 40.0F * t + 160.0F * t * t;
		FireAdd(px, static_cast<int>(x), static_cast<int>(y), 0.8F * (1.0F - t / 0.6F), 1.0F - t / 0.6F);
	}
}

void DrawFurnaceLife(const Surface &out, Point floor, int dirX, int dirY, float sinceBreath, float age)
{
	if (out.isIndexed())
		return;
	const Px px { &out, out.w(), out.h() };
	const float stepX = static_cast<float>(dirX - dirY) * 32.0F, stepY = static_cast<float>(dirX + dirY) * 16.0F;
	const float sl = std::max(1.0F, std::sqrt(stepX * stepX + stepY * stepY));
	const float ux = stepX / sl, uy = stepY / sl;
	const float mx = static_cast<float>(floor.x) + ux * 20.0F, my = static_cast<float>(floor.y) + uy * 20.0F - 34.0F;
	// The coals inside it glowing through the grille, brightest as it breathes; and its mouth smouldering between breaths.
	const float breath = sinceBreath >= 0 && sinceBreath < FurnaceFlameSeconds ? 1.0F : 0.0F;
	const float ember = 0.55F + 0.2F * std::sin(age * 7.0F) * std::sin(age * 3.1F) + 0.4F * breath;
	Halo(px, static_cast<float>(floor.x), static_cast<float>(floor.y) - 10.0F, 26, 14, 255, 110, 30, 0.35F * ember);
	Halo(px, mx, my, 8, 6, 255, 160, 60, 0.5F * ember);
	// Heat shimmering over it.
	Ripple(out, { static_cast<int>(floor.x), static_cast<int>(floor.y) - 50 }, 10.0F + 6.0F * Frac(age * 1.5F), 1.6F);
	// Smoke curling from its back.
	for (uint32_t i = 0; i < 4; i++) {
		const float ph = Frac(age / 1.6F + static_cast<float>(i) / 4.0F);
		const float x = static_cast<float>(floor.x) - ux * 10.0F + std::sin(ph * 5.0F + static_cast<float>(i)) * 5.0F, y = static_cast<float>(floor.y) - 40.0F - 50.0F * ph;
		const float size = 3.0F + 7.0F * ph;
		const int s = static_cast<int>(size);
		for (int dy = -s; dy <= s; dy++)
			for (int dx = -s; dx <= s; dx++) {
				const float d2 = static_cast<float>(dx * dx + dy * dy) / (size * size);
				if (d2 < 1.0F)
					px.mix(static_cast<int>(x) + dx, static_cast<int>(y) + dy, 52, 46, 42, 0.2F * std::sin(ph * Pi) * (1.0F - d2));
			}
	}
}

void DrawWeaponArcFx(const Surface &out, const WeaponArc &arc, float age, bool floorLayer)
{
	if (out.isIndexed() || age < 0 || age > arc.swing + 0.35F)
		return;
	const Px px { &out, out.w(), out.h() };
	const float cx = static_cast<float>(arc.centre.x), cy = static_cast<float>(arc.centre.y);
	const float r = static_cast<float>(arc.rgb >> 16 & 0xFF), g = static_cast<float>(arc.rgb >> 8 & 0xFF), b = static_cast<float>(arc.rgb & 0xFF);
	// The tip's way round: fast out, easing at the end of the swing.
	const float u = Clamp01(age / arc.swing);
	const float eased = 1.0F - (1.0F - u) * (1.0F - u) * (1.0F - u);
	const float tipA = arc.from + (arc.to - arc.from) * eased;
	const float fade = Clamp01(1.0F - (age - arc.swing) / 0.35F);
	const float span = arc.to - arc.from;
	const float trail = std::abs(span) * 0.75F; // how far behind the tip the light still hangs
	const auto at = [&](float a, float rr, float h, float &x, float &y) { x = cx + std::cos(a) * rr; y = cy + std::sin(a) * rr * 0.5F - h; };
	if (floorLayer) {
		// Its light on the floor under the sweep; dust kicked up along the arc by a low one.
		for (int s = 0; s <= 40; s++) {
			const float q = static_cast<float>(s) / 40.0F;
			const float a = tipA - (span > 0 ? 1.0F : -1.0F) * trail * q;
			if ((span > 0 && a < arc.from) || (span < 0 && a > arc.from))
				break;
			float x, y;
			at(a, arc.radius * 0.8F, 0, x, y);
			const float k = (1.0F - q) * fade * 0.12F;
			for (int dy = -5; dy <= 5; dy++)
				for (int dx = -10; dx <= 10; dx++) {
					const float d2 = static_cast<float>(dx * dx) / 100.0F + static_cast<float>(dy * dy) / 25.0F;
					if (d2 < 1.0F)
						px.add(static_cast<int>(x) + dx, static_cast<int>(y) + dy, r, g, b, k * (1.0F - d2));
				}
		}
		if (arc.dust > 0) {
			for (uint32_t i = 0; i < 16; i++) {
				const float q = (static_cast<float>(i) + 0.5F) / 16.0F;
				const float a = arc.from + span * q;
				const float passed = arc.swing * (1.0F - std::cbrt(std::max(0.0F, 1.0F - q))); // when the tip got there
				const float t = age - passed;
				if (t < 0 || t > 0.7F)
					continue;
				float x, y;
				at(a, arc.radius * (0.85F + 0.25F * Unit(Hash(i, 6001))), 0, x, y);
				const float size = 4.0F + 10.0F * std::sqrt(t / 0.7F);
				const float alpha = 0.4F * arc.dust * std::sin(std::min(1.0F, t / 0.7F) * Pi);
				const int s = static_cast<int>(size);
				for (int dy = -s / 2; dy <= s / 2; dy++)
					for (int dx = -s; dx <= s; dx++) {
						const float d2 = static_cast<float>(dx * dx) / (size * size) + static_cast<float>(dy * dy * 4) / (size * size);
						if (d2 < 1.0F)
							px.mix(static_cast<int>(x) + dx, static_cast<int>(y) + dy - static_cast<int>(10.0F * t), 128, 114, 94, alpha * (1.0F - d2));
					}
			}
		}
		return;
	}
	// The trail: a crescent of light from the tip back along its way, the band between the weapon's grip and its tip,
	// brightest at the leading edge, thinning to nothing behind.
	constexpr int Steps = 260;
	for (int s = 0; s <= Steps; s++) {
		const float q = static_cast<float>(s) / static_cast<float>(Steps);
		const float a = tipA - (span > 0 ? 1.0F : -1.0F) * trail * q;
		if ((span > 0 && a < arc.from - 0.01F) || (span < 0 && a > arc.from + 0.01F))
			break;
		const float k = std::pow(1.0F - q, 1.6F) * fade;
		const float inner = arc.radius * (0.45F + 0.35F * q); // the band narrows toward its tail
		for (float rr = inner; rr <= arc.radius; rr += 1.0F) {
			const float w = (rr - inner) / std::max(1.0F, arc.radius - inner); // 0 at the grip side, 1 at the tip side
			float x, y;
			at(a, rr, arc.height, x, y);
			const float e = w * w;
			px.add(static_cast<int>(x), static_cast<int>(y), r, g, b, 0.45F * k * e);
			if (s < 8 && w > 0.85F) // the edge itself, white
				px.add(static_cast<int>(x), static_cast<int>(y), 255, 255, 255, 0.9F * fade);
		}
	}
	// A glint at the tip while it moves.
	if (u < 1.0F) {
		float x, y;
		at(tipA, arc.radius, arc.height, x, y);
		for (int o = -5; o <= 5; o++) {
			const float k = (1.0F - std::abs(static_cast<float>(o)) / 6.0F) * 0.9F;
			px.add(static_cast<int>(x) + o, static_cast<int>(y), 255, 255, 255, k);
			px.add(static_cast<int>(x), static_cast<int>(y) + o / 2, 255, 255, 255, k * 0.7F);
		}
	}
}

void DrawStrikeFlashFx(const Surface &out, Point foot, uint32_t rgb, float age)
{
	if (out.isIndexed() || age < 0 || age > 0.3F)
		return;
	const Px px { &out, out.w(), out.h() };
	const float r = static_cast<float>(rgb >> 16 & 0xFF), g = static_cast<float>(rgb >> 8 & 0xFF), b = static_cast<float>(rgb & 0xFF);
	const float k = 1.0F - age / 0.3F;
	const float x = static_cast<float>(foot.x), y = static_cast<float>(foot.y) - 30.0F;
	// A slash of light across the body, and sparks.
	for (int s = -14; s <= 14; s++) {
		const float e = 1.0F - std::abs(static_cast<float>(s)) / 15.0F;
		px.add(static_cast<int>(x + static_cast<float>(s)), static_cast<int>(y - static_cast<float>(s) * 0.6F), 255, 255, 255, k * e);
		px.add(static_cast<int>(x + static_cast<float>(s)), static_cast<int>(y - static_cast<float>(s) * 0.6F) + 1, r, g, b, 0.7F * k * e);
	}
	for (uint32_t i = 0; i < 8; i++) {
		const float a = Unit(Hash(i, 6101)) * TwoPi, v = 50.0F + 50.0F * Unit(Hash(i, 6102));
		px.add(static_cast<int>(x + std::cos(a) * v * age), static_cast<int>(y + std::sin(a) * v * age * 0.6F + 120.0F * age * age), 255, 240, 200, k);
	}
}

void DrawThrustFx(const Surface &out, const Thrust &th, float age, bool floorLayer)
{
	if (out.isIndexed() || age < 0 || age > 0.6F)
		return;
	const Px px { &out, out.w(), out.h() };
	const float r = static_cast<float>(th.rgb >> 16 & 0xFF), g = static_cast<float>(th.rgb >> 8 & 0xFF), b = static_cast<float>(th.rgb & 0xFF);
	const float sx = static_cast<float>(th.from.x), sy = static_cast<float>(th.from.y);
	const float ex = static_cast<float>(th.to.x), ey = static_cast<float>(th.to.y);
	const float len = std::max(1.0F, std::sqrt((ex - sx) * (ex - sx) + (ey - sy) * (ey - sy)));
	const float ux = (ex - sx) / len, uy = (ey - sy) / len;
	const float nx = -uy, ny = ux;
	const float out01 = 1.0F - std::pow(1.0F - Clamp01(age / 0.1F), 3.0F);
	const float fade = Clamp01(1.0F - (age - 0.12F) / 0.35F);
	const float tip = len * out01;
	if (floorLayer) {
		for (float d = 0; d < tip; d += 3.0F) {
			const float x = sx + ux * d, y = sy + uy * d + th.height;
			for (int dy = -4; dy <= 4; dy++)
				for (int dx = -8; dx <= 8; dx++) {
					const float d2 = static_cast<float>(dx * dx) / 64.0F + static_cast<float>(dy * dy) / 16.0F;
					if (d2 < 1.0F)
						px.add(static_cast<int>(x) + dx, static_cast<int>(y) + dy, r, g, b, 0.05F * fade * (1.0F - d2));
				}
		}
		return;
	}
	// The streak: a tapered shaft of light along the thrust, fine at the back, a bright point at its tip.
	for (float d = 0; d <= tip; d += 1.0F) {
		const float q = d / len;
		const float w = 0.6F + 2.4F * q;
		const float x = sx + ux * d, y = sy + uy * d;
		for (int o = -static_cast<int>(w + 3.0F); o <= static_cast<int>(w + 3.0F); o++) {
			const float a = std::abs(static_cast<float>(o));
			const float k = (a <= w * 0.5F ? 1.0F : (a <= w ? 0.6F : 0.3F * std::exp(-(a - w)))) * fade * (0.3F + 0.7F * q);
			px.add(static_cast<int>(x + nx * static_cast<float>(o)), static_cast<int>(y + ny * static_cast<float>(o)), a <= w * 0.5F ? 255 : r, a <= w * 0.5F ? 255 : g, a <= w * 0.5F ? 255 : b, k);
		}
	}
	// Rings of pressure riding the thrust: thin ellipses across its line, running out and widening.
	for (int ring = 0; ring < 3; ring++) {
		const float rt = age - 0.03F * static_cast<float>(ring);
		if (rt < 0 || rt > 0.3F)
			continue;
		const float d = len * Clamp01(rt / 0.15F) * (0.4F + 0.2F * static_cast<float>(ring));
		const float R = 6.0F + 30.0F * rt;
		const float k = 0.7F * (1.0F - rt / 0.3F);
		const float x = sx + ux * d, y = sy + uy * d;
		for (int s = 0; s < 60; s++) {
			const float a = static_cast<float>(s) / 60.0F * TwoPi;
			const float across = std::cos(a) * R, along = std::sin(a) * R * 0.3F;
			px.add(static_cast<int>(x + nx * across + ux * along), static_cast<int>(y + ny * across + uy * along), r, g, b, k);
		}
	}
	// The point's flash where it ends.
	if (out01 >= 1.0F) {
		const float k = std::exp(-(age - 0.1F) / 0.1F);
		for (int o = -10; o <= 10; o++) {
			const float e = 1.0F - std::abs(static_cast<float>(o)) / 11.0F;
			px.add(static_cast<int>(ex + nx * static_cast<float>(o)), static_cast<int>(ey + ny * static_cast<float>(o)), 255, 255, 255, k * e);
			px.add(static_cast<int>(ex + ux * static_cast<float>(o) * 0.6F), static_cast<int>(ey + uy * static_cast<float>(o) * 0.6F), 255, 255, 255, k * e);
		}
	}
}

void DrawHurledSpearFx(const Surface &out, Point from, Point to, float age, bool floorLayer)
{
	if (out.isIndexed() || age < 0)
		return;
	const Px px { &out, out.w(), out.h() };
	const float sx = static_cast<float>(from.x), sy = static_cast<float>(from.y);
	const float ex = static_cast<float>(to.x), ey = static_cast<float>(to.y);
	const float dist = std::sqrt((ex - sx) * (ex - sx) + (ey - sy) * (ey - sy) * 4.0F);
	const float flight = std::max(0.15F, dist / 420.0F);
	const float land = age - flight;
	if (land > HurledSpearImpactSeconds)
		return;
	const float ux = (ex - sx) / std::max(1.0F, std::sqrt((ex - sx) * (ex - sx) + (ey - sy) * (ey - sy))), uy = (ey - sy) / std::max(1.0F, std::sqrt((ex - sx) * (ex - sx) + (ey - sy) * (ey - sy)));
	// Where it flies: a shallow arc from shoulder height down into the floor.
	const auto pos = [&](float t, float &x, float &y) {
		const float u = Clamp01(t / flight);
		x = sx + (ex - sx) * u;
		y = sy + (ey - sy) * u - (40.0F * (1.0F - u) + dist * 0.18F * 4.0F * u * (1.0F - u));
	};
	if (floorLayer) {
		if (land < 0) { // its shadow running along the floor under it
			const float u = Clamp01(age / flight);
			const float x = sx + (ex - sx) * u, y = sy + (ey - sy) * u;
			for (int s = -12; s <= 12; s++)
				px.mix(static_cast<int>(x + ux * static_cast<float>(s)), static_cast<int>(y + uy * static_cast<float>(s)), 10, 10, 14, 0.35F);
			return;
		}
		// The landing: a ring of light across the tile it bursts on, dust thrown, the air bending.
		const float u = Clamp01(land / 0.4F);
		const float R = 8.0F + 52.0F * std::sqrt(u);
		for (int s = 0; s < 200; s++) {
			const float a = static_cast<float>(s) / 200.0F * TwoPi;
			px.add(static_cast<int>(ex + std::cos(a) * R), static_cast<int>(ey + std::sin(a) * R * 0.5F), 255, 236, 180, 0.7F * (1.0F - u));
		}
		for (int dy = -20; dy <= 20; dy++)
			for (int dx = -40; dx <= 40; dx++) {
				const float d2 = static_cast<float>(dx * dx) / 1600.0F + static_cast<float>(dy * dy) / 400.0F;
				if (d2 < 1.0F)
					px.add(static_cast<int>(ex) + dx, static_cast<int>(ey) + dy, 255, 220, 140, 0.5F * (1.0F - u) * (1.0F - d2));
			}
		Ripple(out, to, R, 4.0F * (1.0F - u));
		return;
	}
	float x, y;
	const float tNow = std::min(age, flight);
	pos(tNow, x, y);
	float px0, py0;
	pos(std::max(0.0F, tNow - 0.02F), px0, py0);
	float dx = x - px0, dy = y - py0;
	const float dl = std::sqrt(dx * dx + dy * dy);
	if (dl > 0.01F) {
		dx /= dl;
		dy /= dl;
	} else {
		dx = ux;
		dy = uy;
	}
	const float fade = land < 0 ? 1.0F : Clamp01(1.0F - (land - 0.35F) / 0.3F);
	// A trail of pale gold light behind it while it flies, and two wings of light swept back from its head.
	if (land < 0.1F) {
		for (int s = 1; s <= 40; s++) {
			float tx, ty;
			pos(std::max(0.0F, tNow - static_cast<float>(s) * 0.006F), tx, ty);
			const float k = (1.0F - static_cast<float>(s) / 40.0F) * 0.6F * (land < 0 ? 1.0F : 1.0F - land / 0.1F);
			px.add(static_cast<int>(tx), static_cast<int>(ty), 255, 226, 150, k);
			px.add(static_cast<int>(tx), static_cast<int>(ty) + 1, 255, 200, 110, 0.5F * k);
		}
		if (land < 0)
			for (int side = -1; side <= 1; side += 2)
				for (int s = 0; s < 14; s++) {
					const float q = static_cast<float>(s) / 14.0F;
					const float wx = x - dx * (6.0F + 14.0F * q) - dy * static_cast<float>(side) * 10.0F * q;
					const float wy = y - dy * (6.0F + 14.0F * q) + dx * static_cast<float>(side) * 10.0F * q * 0.7F;
					px.add(static_cast<int>(wx), static_cast<int>(wy), 255, 240, 200, 0.7F * (1.0F - q));
				}
	}
	// The spear itself: a dark shaft, a steel head catching the light; planted, slanted in the floor once it lands.
	// A pale gold glow round it, so it reads against the dark floor.
	for (float s = -40.0F; s <= 12.0F; s += 2.0F)
		for (int o = -4; o <= 4; o++)
			px.add(static_cast<int>(x + dx * s - dy * static_cast<float>(o)), static_cast<int>(y + dy * s + dx * static_cast<float>(o)), 255, 220, 140, 0.07F * (1.0F - std::abs(static_cast<float>(o)) / 5.0F) * fade);
	const float shaft = 42.0F;
	for (float s = -shaft; s <= 12.0F; s += 1.0F) {
		const int ix = static_cast<int>(x + dx * s), iy = static_cast<int>(y + dy * s);
		if (s > 0) { // the head: a broad leaf of steel
			const float w = s < 6.0F ? s / 3.0F : (12.0F - s) / 3.0F;
			for (int o = -static_cast<int>(w); o <= static_cast<int>(w); o++)
				px.mix(ix - static_cast<int>(dy * static_cast<float>(o)), iy + static_cast<int>(dx * static_cast<float>(o)), o <= 0 ? 240 : 150, o <= 0 ? 244 : 160, o <= 0 ? 250 : 185, fade);
		} else {
			px.mix(ix, iy, 176, 128, 78, fade);
			px.mix(ix, iy - 1, 214, 168, 104, 0.8F * fade);
			px.mix(ix, iy + 1, 110, 78, 46, 0.8F * fade);
		}
	}
	px.add(static_cast<int>(x + dx * 4.0F), static_cast<int>(y + dy * 4.0F) - 1, 255, 255, 255, (0.6F + 0.4F * std::sin(age * 40.0F)) * fade);
	// The burst as it strikes: rays of light and a flash.
	if (land >= 0) {
		const float k = std::exp(-land / 0.12F);
		for (int ray = 0; ray < 10; ray++) {
			const float a = static_cast<float>(ray) / 10.0F * TwoPi;
			const float L = 20.0F + 30.0F * Clamp01(land / 0.15F);
			for (float s = 4.0F; s < L; s += 1.0F)
				px.add(static_cast<int>(ex + std::cos(a) * s), static_cast<int>(ey - 14.0F + std::sin(a) * s * 0.6F), 255, 240, 200, k * (1.0F - s / L));
		}
		for (int dy2 = -16; dy2 <= 16; dy2++)
			for (int dx2 = -16; dx2 <= 16; dx2++) {
				const float d2 = static_cast<float>(dx2 * dx2 + dy2 * dy2) / 256.0F;
				if (d2 < 1.0F)
					px.add(static_cast<int>(ex) + dx2, static_cast<int>(ey) - 14 + dy2, 255, 246, 220, k * (1.0F - d2));
			}
	}
}

void DrawKiPillarFx(const Surface &out, Point foot, uint32_t rgb, float age)
{
	if (out.isIndexed() || age < 0 || age > 0.7F)
		return;
	const Px px { &out, out.w(), out.h() };
	const float r = static_cast<float>(rgb >> 16 & 0xFF), g = static_cast<float>(rgb >> 8 & 0xFF), b = static_cast<float>(rgb & 0xFF);
	const float env = age < 0.06F ? age / 0.06F : std::pow(1.0F - (age - 0.06F) / 0.64F, 1.5F);
	const float H = 110.0F * Clamp01(age / 0.08F);
	for (float z = 0; z < H; z += 1.0F) {
		const float t = z / 110.0F;
		const int half = static_cast<int>(7.0F + 6.0F * t);
		for (int o = -half; o <= half; o++) {
			const float e = 1.0F - std::abs(static_cast<float>(o)) / static_cast<float>(half + 1);
			const float core = std::abs(o) <= 1 ? 1.0F : 0.0F;
			px.add(foot.x + o, foot.y - static_cast<int>(z), core > 0 ? 255 : r, core > 0 ? 250 : g, core > 0 ? 235 : b, env * (1.0F - t) * (0.25F * e * e + 0.6F * core));
		}
	}
	// Motes of ki rising up it.
	for (uint32_t i = 0; i < 14; i++) {
		const float ph = Frac(age * 1.6F + Unit(Hash(i, 6201)));
		px.add(foot.x + static_cast<int>((Unit(Hash(i, 6202)) - 0.5F) * 20.0F), foot.y - static_cast<int>(ph * 100.0F), 255, 245, 210, env * (1.0F - ph));
	}
}

void DrawEchoRingsFx(const Surface &out, Point foot, uint32_t rgb, float age)
{
	if (out.isIndexed() || age < 0 || age > 0.8F)
		return;
	const Px px { &out, out.w(), out.h() };
	const float r = static_cast<float>(rgb >> 16 & 0xFF), g = static_cast<float>(rgb >> 8 & 0xFF), b = static_cast<float>(rgb & 0xFF);
	// Three rings going out from the struck body one after another, like a struck bell's sound.
	for (int ring = 0; ring < 3; ring++) {
		const float t = age - 0.12F * static_cast<float>(ring);
		if (t < 0 || t > 0.5F)
			continue;
		const float R = 8.0F + 70.0F * t;
		const float k = 0.8F * (1.0F - t / 0.5F);
		for (int s = 0; s < 120; s++) {
			const float a = static_cast<float>(s) / 120.0F * TwoPi;
			px.add(foot.x + static_cast<int>(std::cos(a) * R), foot.y - 26 + static_cast<int>(std::sin(a) * R * 0.75F), r, g, b, k);
			px.add(foot.x + static_cast<int>(std::cos(a) * R), foot.y + static_cast<int>(std::sin(a) * R * 0.5F), r, g, b, 0.4F * k);
		}
	}
}

void DrawJadeLeavesFx(const Surface &out, Point centre, float age)
{
	if (out.isIndexed() || age < 0 || age > 1.2F)
		return;
	const Px px { &out, out.w(), out.h() };
	// Slivers of jade light drifting down round the flurry, turning as they fall, like bamboo leaves.
	for (uint32_t i = 0; i < 22; i++) {
		const float t0 = Unit(Hash(i, 6301)) * 0.4F;
		const float t = age - t0;
		if (t < 0 || t > 0.8F)
			continue;
		const float x = static_cast<float>(centre.x) + (Unit(Hash(i, 6302)) - 0.5F) * 150.0F + std::sin(t * 6.0F + static_cast<float>(i)) * 8.0F;
		const float y = static_cast<float>(centre.y) - 80.0F + Unit(Hash(i, 6303)) * 30.0F + 70.0F * t;
		const float turn = t * 9.0F + static_cast<float>(i);
		const float k = std::sin(t / 0.8F * Pi);
		for (int s = -3; s <= 3; s++) {
			const float e = 1.0F - std::abs(static_cast<float>(s)) / 4.0F;
			px.add(static_cast<int>(x + std::cos(turn) * static_cast<float>(s)), static_cast<int>(y + std::sin(turn) * static_cast<float>(s) * 0.4F), 150, 240, 150, 0.8F * k * e);
		}
	}
}

void DrawHeavenSplitterFx(const Surface &out, Point foot, int dirX, int dirY, int tiles, float age, bool floorLayer)
{
	if (out.isIndexed() || age < 0 || age > 1.2F || (dirX == 0 && dirY == 0))
		return;
	const Px px { &out, out.w(), out.h() };
	const float stepX = static_cast<float>(dirX - dirY) * 32.0F, stepY = static_cast<float>(dirX + dirY) * 16.0F;
	const float fx = static_cast<float>(foot.x), fy = static_cast<float>(foot.y);
	const float sx = fx + stepX * 0.6F, sy = fy + stepY * 0.6F; // where the blow comes down
	const float ex = fx + stepX * (static_cast<float>(tiles) + 0.5F), ey = fy + stepY * (static_cast<float>(tiles) + 0.5F);
	const float len = std::max(1.0F, std::sqrt((ex - sx) * (ex - sx) + (ey - sy) * (ey - sy)));
	const float ux = (ex - sx) / len, uy = (ey - sy) / len;
	constexpr float Down = 0.12F; // the blow lands
	const float since = age - Down;
	const float front = len * Clamp01(since / 0.3F);
	const float fade = Clamp01(1.0F - (age - 0.6F) / 0.6F);
	if (floorLayer) {
		if (since < 0)
			return;
		// The split: a white-gold fissure racing along the line, the floor lit either side of it.
		for (float d = 0; d <= front; d += 1.0F) {
			const float hot = std::exp(-(front - d) / 30.0F);
			const float x = sx + ux * d + std::sin(d * 0.3F) * 1.5F * -uy, y = sy + uy * d + std::sin(d * 0.3F) * 1.5F * ux * 0.5F;
			px.mix(static_cast<int>(x), static_cast<int>(y), 20, 14, 8, 0.8F * fade);
			px.add(static_cast<int>(x), static_cast<int>(y), 255, 236, 170, (0.4F + 0.6F * hot) * fade);
			px.add(static_cast<int>(x), static_cast<int>(y) + 1, 255, 200, 110, 0.5F * hot * fade);
			for (int o = -10; o <= 10; o += 2) {
				const float e = 1.0F - std::abs(static_cast<float>(o)) / 11.0F;
				px.add(static_cast<int>(x - uy * static_cast<float>(o)), static_cast<int>(y + ux * static_cast<float>(o) * 0.5F), 255, 210, 120, 0.12F * e * (0.3F + 0.7F * hot) * fade);
			}
		}
		Ripple(out, { static_cast<int>(sx + ux * front), static_cast<int>(sy + uy * front) }, 16.0F, 3.0F * fade);
		return;
	}
	// The overhead blow: an arc of light coming down in front of him.
	if (age < Down + 0.25F) {
		const float u = Clamp01(age / Down);
		const float k = age < Down ? 1.0F : 1.0F - (age - Down) / 0.25F;
		for (int s = 0; s < 40; s++) {
			const float q = static_cast<float>(s) / 40.0F;
			const float a = -1.5F + 1.6F * u * q; // from overhead to the floor ahead
			const float rr = 44.0F;
			const float x = fx + ux * std::cos(a + 1.5F) * rr * 0.9F, y = fy - 30.0F - std::cos(a) * rr + uy * std::sin(a + 1.5F) * rr * 0.4F;
			for (int w = -2; w <= 2; w++)
				px.add(static_cast<int>(x) + w, static_cast<int>(y), 255, 236, 180, k * q * (1.0F - std::abs(static_cast<float>(w)) / 3.0F));
		}
	}
	if (since < 0)
		return;
	// A curtain of light rising along the split behind its front, highest at the front, sinking behind it.
	for (float d = 0; d <= front; d += 1.0F) {
		const float behind = front - d;
		const float h = 40.0F * std::exp(-behind / 50.0F) * fade;
		const float x = sx + ux * d, y = sy + uy * d;
		for (float z = 0; z < h; z += 1.0F) {
			const float t = z / std::max(1.0F, h);
			px.add(static_cast<int>(x), static_cast<int>(y - z), 255, 230, 160, 0.35F * (1.0F - t) * (1.0F - t));
		}
	}
	// Chips of stone thrown up either side as it passes.
	for (uint32_t i = 0; i < 20; i++) {
		const float d = Unit(Hash(i, 6401)) * len;
		const float t = since - 0.3F * d / len;
		if (t < 0 || t > 0.5F)
			continue;
		const float side = (Hash(i, 6402) & 1) ? 1.0F : -1.0F;
		const float v = 30.0F + 30.0F * Unit(Hash(i, 6403));
		const float x = sx + ux * d - uy * side * v * t, y = sy + uy * d + ux * side * v * t * 0.5F - 70.0F * t + 260.0F * t * t;
		px.mix(static_cast<int>(x), static_cast<int>(y), 150, 132, 104, 1.0F - t / 0.5F);
		px.mix(static_cast<int>(x) + 1, static_cast<int>(y), 100, 86, 66, 1.0F - t / 0.5F);
	}
}

void DrawFlurryPathFx(const Surface &out, const Point *stops, int count, uint32_t rgb, float age)
{
	if (out.isIndexed() || age < 0 || count < 2)
		return;
	const Px px { &out, out.w(), out.h() };
	const float r = static_cast<float>(rgb >> 16 & 0xFF), g = static_cast<float>(rgb >> 8 & 0xFF), b = static_cast<float>(rgb & 0xFF);
	constexpr float Leg = 0.09F; // seconds from one enemy to the next
	for (int i = 0; i + 1 < count; i++) {
		const float t = age - Leg * static_cast<float>(i);
		if (t < 0 || t > Leg + 0.4F)
			continue;
		const float x0 = static_cast<float>(stops[i].x), y0 = static_cast<float>(stops[i].y) - 30.0F;
		const float x1 = static_cast<float>(stops[i + 1].x), y1 = static_cast<float>(stops[i + 1].y) - 30.0F;
		const float drawn = Clamp01(t / Leg);
		const float fade = Clamp01(1.0F - (t - Leg) / 0.4F);
		const float len = std::sqrt((x1 - x0) * (x1 - x0) + (y1 - y0) * (y1 - y0));
		for (float d = 0; d <= len * drawn; d += 1.0F) {
			const float q = d / std::max(1.0F, len);
			const float x = x0 + (x1 - x0) * q, y = y0 + (y1 - y0) * q;
			const float k = fade * (0.35F + 0.65F * q);
			px.add(static_cast<int>(x), static_cast<int>(y), 255, 255, 240, k);
			px.add(static_cast<int>(x), static_cast<int>(y) - 1, r, g, b, 0.6F * k);
			px.add(static_cast<int>(x), static_cast<int>(y) + 1, r, g, b, 0.6F * k);
			px.add(static_cast<int>(x), static_cast<int>(y) + 2, r, g, b, 0.25F * k);
		}
		// A blur of him at each stop, and the strike flash.
		if (t >= Leg) {
			const float k = Clamp01(1.0F - (t - Leg) / 0.3F);
			for (int dy = -18; dy <= 18; dy++)
				for (int dx = -10; dx <= 10; dx++) {
					const float d2 = static_cast<float>(dx * dx) / 100.0F + static_cast<float>(dy * dy) / 324.0F;
					if (d2 < 1.0F)
						px.add(static_cast<int>(x1) + dx, static_cast<int>(y1) + dy, r, g, b, 0.45F * k * (1.0F - d2));
				}
		}
	}
}

void DrawClawRakeFx(const Surface &out, Point foot, float age, bool floorLayer)
{
	if (out.isIndexed() || age < 0 || age > ClawRakeSeconds)
		return;
	const Px px { &out, out.w(), out.h() };
	const float fx = static_cast<float>(foot.x), fy = static_cast<float>(foot.y);
	if (floorLayer) {
		// The blood it sheds pooling under it while it bleeds.
		const float pool = Clamp01((age - 0.3F) / 1.5F) * Clamp01((ClawRakeSeconds - age) / 0.6F);
		for (int dy = -5; dy <= 5; dy++)
			for (int dx = -12; dx <= 12; dx++) {
				const float d2 = static_cast<float>(dx * dx) / 144.0F + static_cast<float>(dy * dy) / 25.0F;
				if (d2 < 1.0F)
					px.mix(static_cast<int>(fx) + dx + 3, static_cast<int>(fy) + dy + 2, 90, 10, 12, 0.55F * pool * (1.0F - d2));
			}
		return;
	}
	// Three rakes across the body, one after another: curved strokes, white-hot at their middle, tiger orange at the edges,
	// leaving glowing scars that cool.
	for (int c = 0; c < 3; c++) {
		const float t = age - 0.07F * static_cast<float>(c);
		if (t < 0 || t > 0.6F)
			continue;
		const float drawn = Clamp01(t / 0.06F);
		const float glow = std::exp(-std::max(0.0F, t - 0.06F) / 0.2F);
		const float ox = static_cast<float>(c - 1) * 7.0F;
		for (int s = 0; s <= 30; s++) {
			const float q = static_cast<float>(s) / 30.0F;
			if (q > drawn)
				break;
			const float x = fx - 14.0F + ox + 26.0F * q, y = fy - 52.0F + 34.0F * q + std::sin(q * Pi) * -5.0F;
			const float w = std::sin(q * Pi) * 2.2F;
			for (int o = -static_cast<int>(w + 1.0F); o <= static_cast<int>(w + 1.0F); o++) {
				const float e = 1.0F - std::abs(static_cast<float>(o)) / (w + 1.5F);
				if (std::abs(static_cast<float>(o)) <= w * 0.5F)
					px.add(static_cast<int>(x) + o, static_cast<int>(y), 255, 250, 220, glow * e);
				else
					px.add(static_cast<int>(x) + o, static_cast<int>(y), 255, 130, 30, 0.8F * glow * e);
			}
		}
	}
	// It bleeds: red drops falling from the wounds, now and then, for its three seconds.
	if (age > 0.25F)
		for (uint32_t i = 0; i < 8; i++) {
			const float period = 0.6F + 0.4F * Unit(Hash(i, 6501));
			const float ph = Frac((age - 0.25F) / period + Unit(Hash(i, 6502)));
			const float x = fx - 10.0F + 20.0F * Unit(Hash(i, 6503));
			const float y = fy - 40.0F + 20.0F * Unit(Hash(i, 6504)) + 40.0F * ph * ph;
			if (y > fy)
				continue;
			px.mix(static_cast<int>(x), static_cast<int>(y), 170, 16, 20, 0.9F * Clamp01((ClawRakeSeconds - age) / 0.6F));
			px.mix(static_cast<int>(x), static_cast<int>(y) - 1, 120, 10, 14, 0.6F * Clamp01((ClawRakeSeconds - age) / 0.6F));
		}
}

} // namespace devilution::oracool
