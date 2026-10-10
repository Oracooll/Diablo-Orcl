/**
 * @file oracool/post_fx.cpp
 *
 * World post-effects - see post_fx.h.
 */
#include "oracool/post_fx.h"

#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <initializer_list>
#include <vector>

#include <SDL.h> // SDL_GetTicks - the haze's and the water's clock

#include "engine/palette.h"
#include "engine/render/dun_render.hpp"
#include "engine/surface.hpp"
#include "levels/gendung.h"
#include "options.h"

// Every pass here walks every world pixel (or a large share of them) once a frame, and the game is played from the
// Debug build: this file is optimised even there, as colour_light.cpp is. /Ob0 still forbids inlining, so the per-pixel
// loops are written out by hand - no helpers, no std::min/max/clamp, no lambdas, no container operator[] inside them.
#ifdef _MSC_VER
#pragma runtime_checks("", off)
#pragma optimize("gt", on)
#endif

namespace devilution::oracool {

namespace {

constexpr int FieldW = MAXDUNX;
constexpr int FieldH = MAXDUNY;

/** A sine over 1024 steps, -256..256. */
std::array<int, 1024> SineTable;
bool SineReady = false;

const int *Sine()
{
	if (!SineReady) {
		for (int i = 0; i < 1024; i++)
			SineTable[i] = static_cast<int>(std::lround(std::sin(i * (2.0 * 3.14159265358979323846 / 1024.0)) * 256.0));
		SineReady = true;
	}
	return SineTable.data();
}

/** Two packed 0x00RRGGBB colours mixed, @p w = 0 (all @p a) .. 256 (all @p b). */
#define ORCL_MIX(a, b, w) \
	(((((a) & 0xFF00FFU) * (256U - (w)) + ((b) & 0xFF00FFU) * (w)) >> 8 & 0xFF00FFU) | ((((a) & 0x00FF00U) * (256U - (w)) + ((b) & 0x00FF00U) * (w)) >> 8 & 0x00FF00U))

// ---------------------------------------------------------------------------------------------------------------------
// Area mood and vignette
// ---------------------------------------------------------------------------------------------------------------------

struct Grade {
	float sr, sg, sb; // the gain in the shadows
	float hr, hg, hb; // the gain in the highlights
};

/** The level's grade (user, 2026-10-10): every gain within about 0.9..1.08, rescaled to keep a grey's brightness. */
Grade GradeFor(dungeon_type type)
{
	switch (type) {
	case DTYPE_CATHEDRAL: return { 0.94F, 0.98F, 1.08F, 0.98F, 1.00F, 1.03F }; // cold grey-blue
	case DTYPE_CATACOMBS: return { 1.05F, 1.00F, 0.90F, 1.06F, 1.02F, 0.92F }; // dusty ochre
	case DTYPE_CAVES: return { 0.97F, 1.04F, 0.93F, 1.04F, 1.02F, 0.92F };     // damp green-brown
	case DTYPE_HELL: return { 1.08F, 0.95F, 0.93F, 1.06F, 0.98F, 0.94F };      // blood-warm
	case DTYPE_NEST: return { 1.00F, 1.05F, 0.92F, 1.05F, 1.03F, 0.92F };      // sickly amber-green
	case DTYPE_CRYPT: return { 1.03F, 0.96F, 1.07F, 1.04F, 1.02F, 0.97F };     // pale bone-violet
	default: return { 1, 1, 1, 1, 1, 1 };
	}
}

std::array<uint8_t, 256> MoodR, MoodG, MoodB;
int MoodType = -1;

void BuildMood(dungeon_type type)
{
	Grade g = GradeFor(type);
	const float ls = 0.299F * g.sr + 0.587F * g.sg + 0.114F * g.sb;
	const float lh = 0.299F * g.hr + 0.587F * g.hg + 0.114F * g.hb;
	g.sr /= ls, g.sg /= ls, g.sb /= ls;
	g.hr /= lh, g.hg /= lh, g.hb /= lh;
	for (int v = 0; v < 256; v++) {
		const float t = v / 255.0F;
		const auto channel = [&](float s, float h) {
			const float out = v * (s + (h - s) * t) + 0.5F;
			return static_cast<uint8_t>(out > 255.0F ? 255.0F : (out < 0 ? 0 : out));
		};
		MoodR[v] = channel(g.sr, g.hr);
		MoodG[v] = channel(g.sg, g.hg);
		MoodB[v] = channel(g.sb, g.hb);
	}
	MoodType = static_cast<int>(type);
}

std::vector<uint16_t> VignetteCols, VignetteRows;
int VignetteW = -1, VignetteH = -1;

/** One axis of the vignette, 256 = untouched: flat over the middle fifth, then down to 0.865 at the edge. */
void BuildVignetteAxis(std::vector<uint16_t> &axis, int n)
{
	axis.resize(static_cast<size_t>(n));
	for (int i = 0; i < n; i++) {
		const float centred = std::abs((i + 0.5F) / n * 2.0F - 1.0F);
		float d = (centred - 0.2F) / 0.8F;
		d = d < 0 ? 0 : (d > 1 ? 1 : d);
		const float s = d * d * (3.0F - 2.0F * d); // smoothstep: no visible edge where the darkening starts
		axis[static_cast<size_t>(i)] = static_cast<uint16_t>((1.0F - 0.135F * s) * 256.0F + 0.5F);
	}
}

// ---------------------------------------------------------------------------------------------------------------------
// Liquid tiles
// ---------------------------------------------------------------------------------------------------------------------

enum : uint8_t {
	LiquidNone = 0,
	LiquidLava = 1,
	LiquidWater = 2,
};

constexpr uint8_t PieceUnknown = 0xFF;

std::array<uint8_t, MAXTILES> PieceKind;
std::array<uint8_t, FieldW * FieldH> LiquidMap {};
bool AnyLiquid = false;

struct LevelKey {
	int type = -1;
	int level = -1;
	bool set = false;
	int setNum = -1;
	const void *cels = nullptr;
	bool operator==(const LevelKey &o) const
	{
		return type == o.type && level == o.level && set == o.set && setNum == o.setNum && cels == o.cels;
	}
};
LevelKey LiquidKey;
uint32_t LiquidPieceHash = 0;
bool LiquidMapValid = false;

/**
 * The palette entries a level type cycles - its animated liquid (engine/palette.cpp, diablo_color_cyc_logic): the Caves
 * cycle 1..31 (lava; the Poisoned Water level's pool), Hell rotates 1..31 through its light tables (the blood ramp
 * MakeLightTable writes), the Nest cycles 1..8 and 9..15 (its pools' bubbles and waves) and the Crypt 1..15 (its lava;
 * 16..31 is the glow around things, not a liquid). The Cathedral and the Catacombs cycle nothing.
 */
bool CycleRange(dungeon_type type, int &lo, int &hi)
{
	switch (type) {
	case DTYPE_CAVES:
	case DTYPE_HELL: lo = 1, hi = 31; return true;
	case DTYPE_NEST:
	case DTYPE_CRYPT: lo = 1, hi = 15; return true;
	default: return false;
	}
}

uint32_t PieceHash()
{
	const uint16_t *p = &dPiece[0][0];
	uint32_t h = 2166136261U;
	for (int i = 0; i < FieldW * FieldH; i++) {
		h ^= p[i];
		h *= 16777619U;
	}
	return h;
}

/** Draws @p piece's two floor micros into @p scratch and counts how much of it is the cycling range. */
uint8_t ClassifyPiece(uint16_t piece, const Surface &scratch, const uint8_t *identity, int lo, int hi, uint8_t liquid)
{
	for (int y = 0; y < scratch.h(); y++)
		std::memset(scratch.at(0, y), 0, static_cast<size_t>(scratch.w()));
	for (int i = 0; i < 2; i++) {
		const LevelCelBlock block { DPieceMicros[piece].mt[i] };
		if (block.hasValue())
			RenderTile(scratch, Point { i * 32, scratch.h() - 9 }, block, MaskType::Solid, identity);
	}
	int drawn = 0, cycling = 0;
	for (int y = 0; y < scratch.h(); y++) {
		const uint8_t *row = scratch.at(0, y);
		for (int x = 0; x < scratch.w(); x++) {
			const int c = row[x];
			if (c == 0)
				continue;
			drawn++;
			if (c >= lo && c <= hi)
				cycling++;
		}
	}
	// A fifth of the floor is enough for a shore tile to shimmer with the pool it borders.
	return cycling >= 64 && cycling * 5 >= drawn ? liquid : LiquidNone;
}

/** Brings LiquidMap up to date with the level: the pieces once a level, the map again whenever dPiece changes. */
void RefreshLiquidMap()
{
	const LevelKey key { static_cast<int>(leveltype), currlevel, setlevel, static_cast<int>(setlvlnum), pDungeonCels.get() };
	if (!(key == LiquidKey)) {
		LiquidKey = key;
		PieceKind.fill(PieceUnknown);
		LiquidMapValid = false;
	}
	const uint32_t hash = PieceHash();
	if (LiquidMapValid && hash == LiquidPieceHash)
		return;
	LiquidPieceHash = hash;
	LiquidMapValid = true;
	LiquidMap.fill(LiquidNone);
	AnyLiquid = false;

	int lo = 0, hi = 0;
	if (leveltype == DTYPE_TOWN || pDungeonCels == nullptr || !CycleRange(leveltype, lo, hi))
		return;
	// Lava or water: the cycling entries' own colour says (the Caves are lava, the Poisoned Water pool is not).
	int sr = 0, sg = 0, sb = 0;
	for (int i = lo; i <= hi; i++) {
		sr += (PaletteRGB[i] >> 16) & 0xFF;
		sg += (PaletteRGB[i] >> 8) & 0xFF;
		sb += PaletteRGB[i] & 0xFF;
	}
	const uint8_t liquid = sr * 2 > sg + sb ? LiquidLava : LiquidWater;

	std::array<uint8_t, 256> identity;
	for (int i = 0; i < 256; i++)
		identity[i] = static_cast<uint8_t>(i);
	OwnedSurface scratch(64, 48);
	for (int x = 0; x < FieldW; x++) {
		for (int y = 0; y < FieldH; y++) {
			const uint16_t piece = dPiece[x][y];
			if (piece >= MAXTILES || TileHasAny(piece, TileProperties::Solid))
				continue;
			uint8_t &kind = PieceKind[piece];
			if (kind == PieceUnknown)
				kind = ClassifyPiece(piece, scratch, identity.data(), lo, hi, liquid);
			if (kind != LiquidNone) {
				LiquidMap[static_cast<size_t>(x) * FieldH + y] = kind;
				AnyLiquid = true;
			}
		}
	}
}

/** The floor tile under a world pixel, 16.16 - colour_light.cpp's GroundMap (a tile centre lands on whole numbers). */
struct GroundMap {
	int64_t u0, v0;
	int64_t duX, dvX;
	int64_t duY, dvY;
};

GroundMap MakeGroundMap(Point firstTile, Point firstTileBuffer)
{
	const double cx = firstTileBuffer.x + 32.0;
	const double cy = firstTileBuffer.y - 14.5;
	const double rx = 0.5 - cx;
	const double ry = 0.5 - cy;
	constexpr double Fix = 65536.0;
	GroundMap m;
	m.u0 = static_cast<int64_t>(std::llround((firstTile.x + rx / 64.0 + ry / 32.0) * Fix));
	m.v0 = static_cast<int64_t>(std::llround((firstTile.y + ry / 32.0 - rx / 64.0) * Fix));
	m.duX = 1024;
	m.dvX = -1024;
	m.duY = 2048;
	m.dvY = 2048;
	return m;
}

/** How far above a lava tile the haze still rises, in pixels. */
constexpr int HazeRise = 40;
constexpr int HazeBlock = 8;

std::vector<uint8_t> HazeStrength;
std::vector<int> HazeRowStrength;
std::vector<uint32_t> HazeRow;

// ---------------------------------------------------------------------------------------------------------------------
// Bloom
// ---------------------------------------------------------------------------------------------------------------------

/** Luma where the bright pass starts (a soft knee up to full at 255): about 200 counts a tenth, 230 half. */
constexpr int BloomKnee = 176;
/** How much of the blurred glow goes back on, /256 (0.35). */
constexpr int BloomStrength = 90;
constexpr int BloomRadius = 3;

std::vector<int> BloomR, BloomG, BloomB, BloomTmp;
std::vector<int> BloomRowR, BloomRowG, BloomRowB;
std::vector<uint8_t> BloomRowAny;
std::vector<int> BloomColX0, BloomColX1, BloomColF;
std::array<int, 256> BloomWeight;
bool BloomWeightReady = false;
int BloomW = -1, BloomH = -1;

/** A box of radius BloomRadius along rows (@p stride 1, @p lines rows) or columns (@p stride = width). */
void BoxBlur(const int *src, int *dst, int length, int lines, int step, int lineStep)
{
	for (int line = 0; line < lines; line++) {
		const int *s = src + static_cast<ptrdiff_t>(line) * lineStep;
		int *d = dst + static_cast<ptrdiff_t>(line) * lineStep;
		int sum = 0;
		for (int i = 0; i <= BloomRadius && i < length; i++)
			sum += s[static_cast<ptrdiff_t>(i) * step];
		for (int i = 0; i < length; i++) {
			d[static_cast<ptrdiff_t>(i) * step] = sum / (2 * BloomRadius + 1);
			const int in = i + BloomRadius + 1;
			const int out = i - BloomRadius;
			if (in < length)
				sum += s[static_cast<ptrdiff_t>(in) * step];
			if (out >= 0)
				sum -= s[static_cast<ptrdiff_t>(out) * step];
		}
	}
}

// ---------------------------------------------------------------------------------------------------------------------
// Zoom filter
// ---------------------------------------------------------------------------------------------------------------------

std::vector<uint32_t> ZoomSource;
std::vector<int> ZoomCol0, ZoomCol1, ZoomColW, ZoomRow0, ZoomRow1, ZoomRowW;
int ZoomColsFor[2] = { -1, -1 };
int ZoomRowsFor[2] = { -1, -1 };

/**
 * Sharp bilinear along one axis: each destination pixel is box-filtered over the source span it covers. A pixel inside
 * one source pixel copies it; only the one destination pixel straddling a source edge blends the two - so the picture
 * stays crisp but the nearest-neighbour's uneven 1-or-2 pixel widths are gone.
 */
void BuildZoomAxis(int src, int dst, std::vector<int> &i0, std::vector<int> &i1, std::vector<int> &w)
{
	i0.resize(static_cast<size_t>(dst));
	i1.resize(static_cast<size_t>(dst));
	w.resize(static_cast<size_t>(dst));
	const double ratio = static_cast<double>(src) / dst;
	for (int i = 0; i < dst; i++) {
		const double centre = (i + 0.5) * ratio;
		const double lo = centre - 0.5 * ratio;
		const double hi = centre + 0.5 * ratio;
		int a = static_cast<int>(std::floor(lo + 1e-9));
		a = a < 0 ? 0 : (a > src - 1 ? src - 1 : a);
		const double edge = a + 1.0;
		int weight = 0;
		if (hi > edge + 1e-9 && a + 1 < src)
			weight = static_cast<int>(std::lround((hi - edge) / ratio * 256.0));
		weight = weight < 0 ? 0 : (weight > 256 ? 256 : weight);
		i0[static_cast<size_t>(i)] = a;
		i1[static_cast<size_t>(i)] = a + 1 < src ? a + 1 : a;
		w[static_cast<size_t>(i)] = weight;
	}
}

} // namespace

void ApplyAreaMoodAndVignette(const Surface &out)
{
	if (out.isIndexed() || out.w() <= 0 || out.h() <= 0)
		return;
	const bool mood = *sgOptions.Oracool.areaMood && leveltype != DTYPE_TOWN;
	const bool vignette = *sgOptions.Oracool.vignette;
	if (!mood && !vignette)
		return;
	const int w = out.w();
	const int h = out.h();
	if (mood && MoodType != static_cast<int>(leveltype))
		BuildMood(leveltype);
	if (vignette && (VignetteW != w || VignetteH != h)) {
		BuildVignetteAxis(VignetteCols, w);
		BuildVignetteAxis(VignetteRows, h);
		VignetteW = w;
		VignetteH = h;
	}
	const uint8_t *lr = MoodR.data();
	const uint8_t *lg = MoodG.data();
	const uint8_t *lb = MoodB.data();
	const uint16_t *cols = VignetteCols.data();
	const uint16_t *rows = VignetteRows.data();

	for (int y = 0; y < h; y++) {
		uint32_t *row = out.at<uint32_t>(0, y);
		if (mood && vignette) {
			const uint32_t rf = rows[y];
			for (int x = 0; x < w; x++) {
				const uint32_t c = row[x];
				if (c == 0)
					continue;
				const uint32_t k = (cols[x] * rf) >> 8;
				const uint32_t r = (lr[(c >> 16) & 0xFF] * k) >> 8;
				const uint32_t g = (lg[(c >> 8) & 0xFF] * k) >> 8;
				const uint32_t b = (lb[c & 0xFF] * k) >> 8;
				row[x] = (r << 16) | (g << 8) | b;
			}
		} else if (mood) {
			for (int x = 0; x < w; x++) {
				const uint32_t c = row[x];
				if (c == 0)
					continue;
				row[x] = (static_cast<uint32_t>(lr[(c >> 16) & 0xFF]) << 16) | (static_cast<uint32_t>(lg[(c >> 8) & 0xFF]) << 8) | lb[c & 0xFF];
			}
		} else {
			const uint32_t rf = rows[y];
			if (rf >= 256) {
				// The middle rows: only the columns near the left and right edges darken.
				for (int x = 0; x < w; x++) {
					const uint32_t k = cols[x];
					if (k >= 256)
						continue;
					const uint32_t c = row[x];
					if (c == 0)
						continue;
					row[x] = (((((c >> 16) & 0xFF) * k) >> 8) << 16) | (((((c >> 8) & 0xFF) * k) >> 8) << 8) | (((c & 0xFF) * k) >> 8);
				}
				continue;
			}
			for (int x = 0; x < w; x++) {
				const uint32_t c = row[x];
				if (c == 0)
					continue;
				const uint32_t k = (cols[x] * rf) >> 8;
				row[x] = (((((c >> 16) & 0xFF) * k) >> 8) << 16) | (((((c >> 8) & 0xFF) * k) >> 8) << 8) | (((c & 0xFF) * k) >> 8);
			}
		}
	}
}

void ApplyBloom(const Surface &out)
{
	if (!*sgOptions.Oracool.bloom || out.isIndexed() || out.w() <= 0 || out.h() <= 0)
		return;
	const int w = out.w();
	const int h = out.h();
	const int qw = (w + 3) / 4;
	const int qh = (h + 3) / 4;
	const size_t qn = static_cast<size_t>(qw) * qh;
	if (!BloomWeightReady) {
		constexpr int Range = 255 - BloomKnee;
		for (int l = 0; l < 256; l++) {
			const int t = l - BloomKnee;
			BloomWeight[l] = t <= 0 ? 0 : t * t * 256 / (Range * Range);
		}
		BloomWeightReady = true;
	}
	if (BloomW != w || BloomH != h) {
		BloomR.assign(qn, 0);
		BloomG.assign(qn, 0);
		BloomB.assign(qn, 0);
		BloomTmp.assign(qn, 0);
		BloomRowR.assign(static_cast<size_t>(qw), 0);
		BloomRowG.assign(static_cast<size_t>(qw), 0);
		BloomRowB.assign(static_cast<size_t>(qw), 0);
		BloomRowAny.assign(static_cast<size_t>(qh), 0);
		BloomColX0.resize(static_cast<size_t>(w));
		BloomColX1.resize(static_cast<size_t>(w));
		BloomColF.resize(static_cast<size_t>(w));
		for (int x = 0; x < w; x++) {
			const int q = (2 * x + 1) * 65536 / 8 - 32768; // (x + 0.5) / 4 - 0.5, 16.16
			const int x0 = q >> 16;
			BloomColX0[static_cast<size_t>(x)] = x0 < 0 ? 0 : (x0 > qw - 1 ? qw - 1 : x0);
			BloomColX1[static_cast<size_t>(x)] = x0 + 1 < 0 ? 0 : (x0 + 1 > qw - 1 ? qw - 1 : x0 + 1);
			BloomColF[static_cast<size_t>(x)] = (q >> 8) & 0xFF;
		}
		BloomW = w;
		BloomH = h;
	}
	int *qr = BloomR.data();
	int *qg = BloomG.data();
	int *qb = BloomB.data();
	int *tmp = BloomTmp.data();
	std::memset(qr, 0, qn * sizeof(int));
	std::memset(qg, 0, qn * sizeof(int));
	std::memset(qb, 0, qn * sizeof(int));

	// The bright pass, summed into 4x4 blocks.
	const int *weight = BloomWeight.data();
	int bright = 0;
	for (int y = 0; y < h; y++) {
		const uint32_t *row = out.at<uint32_t>(0, y);
		const int qrow = (y >> 2) * qw;
		for (int x = 0; x < w; x++) {
			const uint32_t c = row[x];
			const int r = static_cast<int>((c >> 16) & 0xFF);
			const int g = static_cast<int>((c >> 8) & 0xFF);
			const int b = static_cast<int>(c & 0xFF);
			const int k = weight[(r * 77 + g * 150 + b * 29) >> 8];
			if (k == 0)
				continue;
			const int i = qrow + (x >> 2);
			qr[i] += r * k;
			qg[i] += g * k;
			qb[i] += b * k;
			bright++;
		}
	}
	if (bright == 0)
		return;
	for (size_t i = 0; i < qn; i++) {
		qr[i] >>= 4;
		qg[i] >>= 4;
		qb[i] >>= 4;
	}

	// Two separable box passes: a soft tent about 13 quarter-pixels (some 50 screen pixels) across.
	for (int pass = 0; pass < 2; pass++) {
		for (int *channel : { qr, qg, qb }) {
			BoxBlur(channel, tmp, qw, qh, 1, qw);
			BoxBlur(tmp, channel, qh, qw, qw, 1);
		}
	}
	uint8_t *rowAny = BloomRowAny.data();
	for (int qy = 0; qy < qh; qy++) {
		int any = 0;
		const int base = qy * qw;
		for (int qx = 0; qx < qw; qx++)
			any |= qr[base + qx] | qg[base + qx] | qb[base + qx];
		rowAny[qy] = any != 0 ? 1 : 0;
	}

	// Back on, bilinear from the quarter buffer.
	int *vr = BloomRowR.data();
	int *vg = BloomRowG.data();
	int *vb = BloomRowB.data();
	const int *cx0 = BloomColX0.data();
	const int *cx1 = BloomColX1.data();
	const int *cf = BloomColF.data();
	for (int y = 0; y < h; y++) {
		const int q = (2 * y + 1) * 65536 / 8 - 32768;
		const int y0 = q >> 16;
		const int ya = y0 < 0 ? 0 : (y0 > qh - 1 ? qh - 1 : y0);
		const int yb = y0 + 1 < 0 ? 0 : (y0 + 1 > qh - 1 ? qh - 1 : y0 + 1);
		if (rowAny[ya] == 0 && rowAny[yb] == 0)
			continue;
		const int fy = (q >> 8) & 0xFF;
		const int *ra = qr + ya * qw, *rb = qr + yb * qw;
		const int *ga = qg + ya * qw, *gb = qg + yb * qw;
		const int *ba = qb + ya * qw, *bb = qb + yb * qw;
		for (int qx = 0; qx < qw; qx++) {
			vr[qx] = (ra[qx] * (256 - fy) + rb[qx] * fy) >> 8;
			vg[qx] = (ga[qx] * (256 - fy) + gb[qx] * fy) >> 8;
			vb[qx] = (ba[qx] * (256 - fy) + bb[qx] * fy) >> 8;
		}
		uint32_t *row = out.at<uint32_t>(0, y);
		for (int x = 0; x < w; x++) {
			const int a = cx0[x], b = cx1[x], f = cf[x];
			const int ar = (((vr[a] * (256 - f) + vr[b] * f) >> 8) * BloomStrength) >> 16;
			const int ag = (((vg[a] * (256 - f) + vg[b] * f) >> 8) * BloomStrength) >> 16;
			const int ab = (((vb[a] * (256 - f) + vb[b] * f) >> 8) * BloomStrength) >> 16;
			if ((ar | ag | ab) == 0)
				continue;
			const uint32_t c = row[x];
			int r = static_cast<int>((c >> 16) & 0xFF) + ar;
			int g = static_cast<int>((c >> 8) & 0xFF) + ag;
			int bl = static_cast<int>(c & 0xFF) + ab;
			r = r > 255 ? 255 : r;
			g = g > 255 ? 255 : g;
			bl = bl > 255 ? 255 : bl;
			row[x] = (static_cast<uint32_t>(r) << 16) | (static_cast<uint32_t>(g) << 8) | static_cast<uint32_t>(bl);
		}
	}
}

void ApplyHeatHazeAndWater(const Surface &out, Point firstTile, Point firstTileBuffer)
{
	if (!*sgOptions.Oracool.heatHaze || out.isIndexed() || leveltype == DTYPE_TOWN || out.w() <= 0 || out.h() <= 0)
		return;
	RefreshLiquidMap();
	if (!AnyLiquid)
		return;
	const int w = out.w();
	const int h = out.h();
	const GroundMap m = MakeGroundMap(firstTile, firstTileBuffer);
	const uint8_t *liquid = LiquidMap.data();

	// The tiles that can reach the screen: the corners' tiles (the bottom ones the haze's rise further down), a margin.
	int64_t minU = INT64_MAX, maxU = INT64_MIN, minV = INT64_MAX, maxV = INT64_MIN;
	for (const Point p : { Point { 0, 0 }, Point { w, 0 }, Point { 0, h + HazeRise + 32 }, Point { w, h + HazeRise + 32 } }) {
		const int64_t u = m.u0 + m.duX * p.x + m.duY * p.y;
		const int64_t v = m.v0 + m.dvX * p.x + m.dvY * p.y;
		minU = u < minU ? u : minU;
		maxU = u > maxU ? u : maxU;
		minV = v < minV ? v : minV;
		maxV = v > maxV ? v : maxV;
	}
	const auto clampField = [](int64_t i, int n) { return static_cast<int>(i < 0 ? 0 : (i > n - 1 ? n - 1 : i)); };
	const int tx0 = clampField((minU >> 16) - 2, FieldW), tx1 = clampField((maxU >> 16) + 2, FieldW);
	const int ty0 = clampField((minV >> 16) - 2, FieldH), ty1 = clampField((maxV >> 16) + 2, FieldH);

	int lavaL = w, lavaR = -1, lavaT = h, lavaB = -1;
	int waterL = w, waterR = -1, waterT = h, waterB = -1;
	for (int tx = tx0; tx <= tx1; tx++) {
		for (int ty = ty0; ty <= ty1; ty++) {
			const uint8_t kind = liquid[static_cast<size_t>(tx) * FieldH + ty];
			if (kind == LiquidNone)
				continue;
			const int cx = firstTileBuffer.x + 32 + (tx - firstTile.x) * 32 - (ty - firstTile.y) * 32;
			const int cy = firstTileBuffer.y - 15 + (tx - firstTile.x) * 16 + (ty - firstTile.y) * 16;
			if (kind == LiquidLava) {
				lavaL = cx - 32 < lavaL ? cx - 32 : lavaL;
				lavaR = cx + 32 > lavaR ? cx + 32 : lavaR;
				lavaT = cy - 16 - HazeRise < lavaT ? cy - 16 - HazeRise : lavaT;
				lavaB = cy + 16 > lavaB ? cy + 16 : lavaB;
			} else {
				waterL = cx - 32 < waterL ? cx - 32 : waterL;
				waterR = cx + 32 > waterR ? cx + 32 : waterR;
				waterT = cy - 16 < waterT ? cy - 16 : waterT;
				waterB = cy + 16 > waterB ? cy + 16 : waterB;
			}
		}
	}
	lavaL = lavaL < 0 ? 0 : lavaL, lavaR = lavaR > w - 1 ? w - 1 : lavaR;
	lavaT = lavaT < 0 ? 0 : lavaT, lavaB = lavaB > h - 1 ? h - 1 : lavaB;
	waterL = waterL < 0 ? 0 : waterL, waterR = waterR > w - 1 ? w - 1 : waterR;
	waterT = waterT < 0 ? 0 : waterT, waterB = waterB > h - 1 ? h - 1 : waterB;

	const int *sine = Sine();
	const uint32_t now = SDL_GetTicks();

	// Water first (the haze above a shore then bends the lit water too): light moving over the floor in tile space.
	if (waterL <= waterR && waterT <= waterB) {
		const uint32_t t1 = (now * 3 / 8) & 1023, t2 = (now * 2 / 7) & 1023, t3 = (now / 5) & 1023;
		for (int py = waterT; py <= waterB; py++) {
			uint32_t *row = out.at<uint32_t>(0, py);
			int64_t u = m.u0 + m.duY * py + m.duX * waterL;
			int64_t v = m.v0 + m.dvY * py + m.dvX * waterL;
			for (int px = waterL; px <= waterR; px++, u += m.duX, v += m.dvX) {
				const int64_t iu = (u + 0x8000) >> 16;
				const int64_t iv = (v + 0x8000) >> 16;
				if (iu < 0 || iu >= FieldW || iv < 0 || iv >= FieldH)
					continue;
				if (liquid[iu * FieldH + iv] != LiquidWater)
					continue;
				const uint32_t c = row[px];
				if (c == 0)
					continue;
				const int s = sine[(static_cast<uint32_t>((u * 3) >> 7) + t1) & 1023]
				    + sine[(static_cast<uint32_t>((v * 5) >> 8) - t2) & 1023]
				    + sine[(static_cast<uint32_t>((u + v) >> 7) + t3) & 1023];
				const uint32_t k = static_cast<uint32_t>(256 + ((s * 27) >> 10)); // +-8%
				uint32_t r = (((c >> 16) & 0xFF) * k) >> 8;
				uint32_t g = (((c >> 8) & 0xFF) * k) >> 8;
				uint32_t b = ((c & 0xFF) * k) >> 8;
				r = r > 255 ? 255 : r;
				g = g > 255 ? 255 : g;
				b = b > 255 ? 255 : b;
				row[px] = (r << 16) | (g << 8) | b;
			}
		}
	}

	if (lavaL > lavaR || lavaT > lavaB)
		return;

	// The haze's strength on a grid of 8 px blocks: full on the lava, fading over HazeRise pixels above it.
	const int nbx = (lavaR - lavaL) / HazeBlock + 2;
	const int nby = (lavaB - lavaT) / HazeBlock + 2;
	HazeStrength.assign(static_cast<size_t>(nbx) * nby, 0);
	HazeRowStrength.resize(static_cast<size_t>(nbx));
	HazeRow.resize(static_cast<size_t>(w) + 8);
	uint8_t *strength = HazeStrength.data();
	int *rowStrength = HazeRowStrength.data();
	uint32_t *copy = HazeRow.data();
	bool anyHaze = false;
	for (int by = 0; by < nby; by++) {
		const int py = lavaT + by * HazeBlock + HazeBlock / 2;
		for (int bx = 0; bx < nbx; bx++) {
			const int px = lavaL + bx * HazeBlock + HazeBlock / 2;
			for (int k = 0; k <= 6; k++) {
				const int sy = py + k * 7;
				const int64_t iu = (m.u0 + m.duX * px + m.duY * sy + 0x8000) >> 16;
				const int64_t iv = (m.v0 + m.dvX * px + m.dvY * sy + 0x8000) >> 16;
				if (iu < 0 || iu >= FieldW || iv < 0 || iv >= FieldH)
					continue;
				if (liquid[iu * FieldH + iv] == LiquidLava) {
					strength[by * nbx + bx] = static_cast<uint8_t>(255 - k * 34);
					anyHaze = true;
					break;
				}
			}
		}
	}
	if (!anyHaze)
		return;

	const uint32_t p1 = (now * 4 / 3) & 1023, p2 = (now * 9 / 5) & 1023;
	const int segL = lavaL - 2 < 0 ? 0 : lavaL - 2;
	const int segR = lavaR + 2 > w - 1 ? w - 1 : lavaR + 2;
	for (int py = lavaT; py <= lavaB; py++) {
		const int t = py - lavaT - HazeBlock / 2;
		const int by0 = t < 0 ? 0 : (t / HazeBlock > nby - 1 ? nby - 1 : t / HazeBlock);
		const int by1 = by0 + 1 > nby - 1 ? nby - 1 : by0 + 1;
		const int fy = t < 0 ? 0 : t % HazeBlock;
		int rowMax = 0;
		for (int bx = 0; bx < nbx; bx++) {
			const int s = (strength[by0 * nbx + bx] * (HazeBlock - fy) + strength[by1 * nbx + bx] * fy) / HazeBlock;
			rowStrength[bx] = s;
			rowMax = s > rowMax ? s : rowMax;
		}
		if (rowMax == 0)
			continue;
		uint32_t *row = out.at<uint32_t>(0, py);
		std::memcpy(copy, row + segL, static_cast<size_t>(segR - segL + 1) * sizeof(uint32_t));
		for (int px = lavaL; px <= lavaR; px++) {
			const int tx = px - lavaL - HazeBlock / 2;
			const int bx0 = tx < 0 ? 0 : (tx >> 3 > nbx - 1 ? nbx - 1 : tx >> 3);
			const int bx1 = bx0 + 1 > nbx - 1 ? nbx - 1 : bx0 + 1;
			const int fx = tx < 0 ? 0 : tx & 7;
			const int a = (rowStrength[bx0] * (8 - fx) + rowStrength[bx1] * fx) >> 3;
			if (a == 0)
				continue;
			// Two waves rising through the rows, slowly varying along them: up to 1.5 px either way.
			const int wave = (sine[(static_cast<uint32_t>(py * 40 + px * 6) + p1) & 1023] * 170
			                     + sine[(static_cast<uint32_t>(py * 67 - px * 3) + p2) & 1023] * 86)
			    >> 8;
			const int at = px * 256 + ((a * wave * 3) >> 9);
			int i = at >> 8;
			const uint32_t f = static_cast<uint32_t>(at & 0xFF);
			i = i < segL ? segL : (i > segR ? segR : i);
			const int j = i + 1 > segR ? segR : i + 1;
			const uint32_t c0 = copy[i - segL];
			const uint32_t c1 = copy[j - segL];
			row[px] = ORCL_MIX(c0, c1, f);
		}
	}
}

void ZoomScaleFiltered(uint8_t *base, int pitch, int srcWidth, int srcHeight, int dstWidth, int dstHeight)
{
	if (srcWidth <= 0 || srcHeight <= 0 || dstWidth < srcWidth || dstHeight < srcHeight)
		return;
	if (srcWidth == dstWidth && srcHeight == dstHeight)
		return; // already the picture

	// The source copied out first: every destination pixel then reads only the copy, so in place is safe.
	const size_t n = static_cast<size_t>(srcWidth) * srcHeight;
	if (ZoomSource.size() < n)
		ZoomSource.resize(n);
	uint32_t *src = ZoomSource.data();
	for (int y = 0; y < srcHeight; y++)
		std::memcpy(src + static_cast<size_t>(y) * srcWidth, base + static_cast<ptrdiff_t>(y) * pitch, static_cast<size_t>(srcWidth) * sizeof(uint32_t));

	if (srcWidth * 2 == dstWidth && srcHeight * 2 == dstHeight) {
		// Scale2x / EPX (Andrea Mazzoleni's public description): each pixel becomes four, a corner taking the colour of
		// the two neighbours it touches when those two agree and the other pair does not - stair-steps become diagonals.
		for (int sy = 0; sy < srcHeight; sy++) {
			const uint32_t *cur = src + static_cast<size_t>(sy) * srcWidth;
			const uint32_t *up = sy > 0 ? cur - srcWidth : cur;
			const uint32_t *down = sy + 1 < srcHeight ? cur + srcWidth : cur;
			auto *out0 = reinterpret_cast<uint32_t *>(base + static_cast<ptrdiff_t>(2 * sy) * pitch);
			auto *out1 = reinterpret_cast<uint32_t *>(base + static_cast<ptrdiff_t>(2 * sy + 1) * pitch);
			for (int sx = 0; sx < srcWidth; sx++) {
				const uint32_t p = cur[sx];
				const uint32_t a = up[sx];                                // above
				const uint32_t b = cur[sx + 1 < srcWidth ? sx + 1 : sx]; // right
				const uint32_t c = cur[sx > 0 ? sx - 1 : sx];            // left
				const uint32_t d = down[sx];                              // below
				out0[2 * sx] = (c == a && c != d && a != b) ? a : p;
				out0[2 * sx + 1] = (a == b && a != c && b != d) ? b : p;
				out1[2 * sx] = (d == c && d != b && c != a) ? c : p;
				out1[2 * sx + 1] = (b == d && b != a && d != c) ? d : p;
			}
		}
		return;
	}

	if (ZoomColsFor[0] != srcWidth || ZoomColsFor[1] != dstWidth) {
		BuildZoomAxis(srcWidth, dstWidth, ZoomCol0, ZoomCol1, ZoomColW);
		ZoomColsFor[0] = srcWidth, ZoomColsFor[1] = dstWidth;
	}
	if (ZoomRowsFor[0] != srcHeight || ZoomRowsFor[1] != dstHeight) {
		BuildZoomAxis(srcHeight, dstHeight, ZoomRow0, ZoomRow1, ZoomRowW);
		ZoomRowsFor[0] = srcHeight, ZoomRowsFor[1] = dstHeight;
	}
	const int *c0 = ZoomCol0.data();
	const int *c1 = ZoomCol1.data();
	const int *cw = ZoomColW.data();
	const int *r0 = ZoomRow0.data();
	const int *r1 = ZoomRow1.data();
	const int *rw = ZoomRowW.data();
	for (int y = 0; y < dstHeight; y++) {
		const uint32_t *top = src + static_cast<size_t>(r0[y]) * srcWidth;
		const uint32_t *bottom = src + static_cast<size_t>(r1[y]) * srcWidth;
		const uint32_t wy = static_cast<uint32_t>(rw[y]);
		auto *out = reinterpret_cast<uint32_t *>(base + static_cast<ptrdiff_t>(y) * pitch);
		if (wy == 0) {
			for (int x = 0; x < dstWidth; x++) {
				const uint32_t wx = static_cast<uint32_t>(cw[x]);
				const uint32_t a = top[c0[x]];
				out[x] = wx == 0 ? a : ORCL_MIX(a, top[c1[x]], wx);
			}
		} else {
			for (int x = 0; x < dstWidth; x++) {
				const uint32_t wx = static_cast<uint32_t>(cw[x]);
				const uint32_t a = top[c0[x]];
				const uint32_t c = bottom[c0[x]];
				const uint32_t upper = wx == 0 ? a : ORCL_MIX(a, top[c1[x]], wx);
				const uint32_t lower = wx == 0 ? c : ORCL_MIX(c, bottom[c1[x]], wx);
				out[x] = ORCL_MIX(upper, lower, wy);
			}
		}
	}
}

uint8_t LiquidKindAt(Point tile)
{
	RefreshLiquidMap();
	if (tile.x < 0 || tile.x >= FieldW || tile.y < 0 || tile.y >= FieldH)
		return LiquidNone;
	return LiquidMap[static_cast<size_t>(tile.x) * FieldH + tile.y];
}

} // namespace devilution::oracool

#undef ORCL_MIX

#ifdef _MSC_VER
#pragma optimize("", on)
#pragma runtime_checks("", restore)
#endif
