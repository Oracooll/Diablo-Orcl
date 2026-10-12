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

} // namespace devilution::oracool
