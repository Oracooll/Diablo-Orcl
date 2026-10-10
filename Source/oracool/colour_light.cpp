/**
 * @file oracool/colour_light.cpp
 *
 * Coloured and smooth dynamic lighting - see colour_light.h.
 */
#include "oracool/colour_light.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

#include "engine/palette.h"
#include "engine/surface.hpp"
#include "levels/gendung.h"
#include "lighting.h"
#include "misdat.h"
#include "missiles.h"
#include "monster.h"
#include "objects.h"
#include "options.h"
#include "oracool/lesser_uniques.h"
#include "oracool/missile_tint.h"

// The two passes walk every world pixel once a frame, and the game is played from the Debug build: this file is
// optimised even there (MSVC honours the pragma under /Od /RTC1 - checked 2026-10-10, the loops come out vectorised).
// Without it the floor pass alone would cost tens of milliseconds a frame at 1080p.
#ifdef _MSC_VER
#pragma runtime_checks("", off)
#pragma optimize("gt", on)
#endif

namespace devilution::oracool {

namespace {

constexpr int FieldW = MAXDUNX;
constexpr int FieldH = MAXDUNY;
constexpr uint16_t One = 256; // 8.8 fixed point

/** How strongly a light's colour shows where it is the only light (1 = the full hue). */
constexpr float ColourStrength = 0.75F;
/** The most one channel may be lifted, so a red light cannot bleach a dark wall to pink. */
constexpr float MaxChannelLift = 1.9F;

struct Accum {
	float coloured; // summed weight of coloured lights
	float neutral;  // summed weight of white lights (the hero's own)
	float r, g, b;  // weight-summed colour, 0..1 a channel
};

std::array<Accum, FieldW * FieldH> Accumulated;
std::array<LightTint, FieldW * FieldH> Tints;
/** The tiles any coloured light reaches, inclusive; empty when minX > maxX. */
int TintMinX = 1, TintMaxX = 0, TintMinY = 1, TintMaxY = 0;

std::array<uint32_t, MAXLIGHTS> IdColour {};
std::array<float, MAXLIGHTS> IdGain {};
std::array<uint16_t, NumLightingLevels> LevelBrightness {};

struct Source {
	float x;
	float y;
	float reach; // tiles at which the light is gone
	uint32_t rgb; // 0 = neutral
	float gain;   // how strongly its colour counts
};
std::vector<Source> Sources;

constexpr uint32_t WarmFlame = Rgb(255, 170, 92); // torches, candles, braziers, the burning crosses
/** The level's own flames are everywhere in the Cathedral: a little over half a spell's hue, or every room goes orange. */
constexpr float FlameGain = 0.55F;

float Luma(float r, float g, float b)
{
	return 0.299F * r + 0.587F * g + 0.114F * b;
}

/**
 * The hue a monster's recolour moves it to: the average colour of the palette entries its TRN rewrites, against the
 * entries they were. A unique wears its own colour; a recolour that only darkens or greys out reads as neutral.
 */
uint32_t TrnHue(const uint8_t *trn)
{
	float r = 0, g = 0, b = 0;
	int n = 0;
	for (int i = 1; i < 255; i++) {
		if (trn[i] == i)
			continue;
		const uint32_t c = PaletteRGB[trn[i]];
		r += static_cast<float>((c >> 16) & 0xFF);
		g += static_cast<float>((c >> 8) & 0xFF);
		b += static_cast<float>(c & 0xFF);
		n++;
	}
	if (n < 8)
		return 0;
	const float hi = std::max({ r, g, b });
	const float lo = std::min({ r, g, b });
	if (hi <= 0 || (hi - lo) < 0.18F * hi) // too grey to call a colour
		return 0;
	const float k = 255.0F / hi;
	return Rgb(static_cast<uint8_t>(r * k), static_cast<uint8_t>(g * k), static_cast<uint8_t>(b * k));
}

uint32_t AffixHue(LesserUniqueAffix affix)
{
	switch (affix) {
	case LesserUniqueAffix::Warded: return hue::WardCyan;
	case LesserUniqueAffix::Relentless: return hue::Infrared;
	case LesserUniqueAffix::Fortified: return hue::Steel;
	case LesserUniqueAffix::Vampiric: return hue::BloodRed;
	case LesserUniqueAffix::Thunderous: return hue::LightningBlue;
	case LesserUniqueAffix::Colossal: return hue::Umber;
	case LesserUniqueAffix::Dread: return hue::DuskViolet;
	default: return 0;
	}
}

uint32_t MonsterLightHue(const Monster &monster)
{
	if (monster.type().type == MT_DIABLO)
		return hue::Infrared;
	if ((monster.flags & MFLAG_BERSERK) != 0)
		return hue::BloodRed;
	if (monster.uniqueMonsterTRN != nullptr) {
		const uint32_t fromTrn = TrnHue(monster.uniqueMonsterTRN.get());
		if (fromTrn != 0)
			return fromTrn;
	}
	return AffixHue(monster.lesserAffix);
}

uint32_t MissileLightHue(const Missile &missile)
{
	if (IsAnyOf(missile.oracoolTint, Tint::Hue, Tint::HueCycle) && missile.oracoolTintRgb != 0)
		return missile.oracoolTintRgb;
	switch (missile._miAnimType) {
	case MissileGraphicID::HolyBolt: return hue::PaladinGold;
	case MissileGraphicID::TownPortal: return hue::HolyBlue;
	case MissileGraphicID::RedPortal: return hue::Infrared;
	case MissileGraphicID::RiftPortalGold:
	case MissileGraphicID::RiftPortalGoldInTown: return hue::PaladinGold;
	case MissileGraphicID::RiftPortalPurple:
	case MissileGraphicID::RiftPortalPurpleInTown: return hue::PhantomPurple;
	case MissileGraphicID::BloodStar: return hue::BloodRed;
	default: break;
	}
	switch (GetMissileData(missile._mitype).damageType()) {
	case DamageType::Fire: return hue::FireOrange;
	case DamageType::Lightning: return hue::LightningBlue;
	case DamageType::Magic: return hue::SpectralLavender;
	case DamageType::Acid: return hue::PoisonGreen;
	case DamageType::Cold: return hue::IceBlue;
	default: return 0;
	}
}

/** The radius AddObjectLight gives a baked light, 0 for an object that has none. */
int BakedObjectLightRadius(const Object &object)
{
	switch (object._otype) {
	case OBJ_STORYCANDLE:
	case OBJ_L5CANDLE:
		return 3;
	case OBJ_L1LIGHT:
	case OBJ_SKFIRE:
	case OBJ_CANDLE1:
	case OBJ_CANDLE2:
	case OBJ_BOOKCANDLE:
	case OBJ_BCROSS:
	case OBJ_TBCROSS:
		return 5;
	case OBJ_TORCHL:
	case OBJ_TORCHR:
	case OBJ_TORCHL2:
	case OBJ_TORCHR2:
		return 8;
	default:
		return 0;
	}
}

void NoteId(int id, uint32_t rgb, float gain = 1.0F)
{
	if (id >= 0 && id < MAXLIGHTS) {
		IdColour[id] = rgb;
		IdGain[id] = gain;
	}
}

void GatherSources()
{
	IdColour.fill(0);
	IdGain.fill(1.0F);
	for (const Missile &missile : Missiles) {
		if (missile._mlid != NO_LIGHT)
			NoteId(missile._mlid, MissileLightHue(missile));
	}
	for (size_t i = 0; i < ActiveMonsterCount; i++) {
		const Monster &monster = Monsters[ActiveMonsters[i]];
		if (monster.lightId != NO_LIGHT)
			NoteId(monster.lightId, MonsterLightHue(monster));
	}

	Sources.clear();
	for (int i = 0; i < ActiveObjectCount; i++) {
		const Object &object = Objects[ActiveObjects[i]];
		if (object._otype == OBJ_FLAMEHOLE) {
			if (object._oVar4 != 0 && object._olid != NO_LIGHT)
				NoteId(object._olid, hue::FireOrange, FlameGain);
			continue;
		}
		const int radius = BakedObjectLightRadius(object);
		if (radius == 0)
			continue;
		if (object._oVar1 == -1) // baked into dPreLight at level load (AddObjectLight): no light id, always there
			Sources.push_back({ static_cast<float>(object.position.x), static_cast<float>(object.position.y), static_cast<float>(radius + 1), WarmFlame, FlameGain });
		else if (object._oVar1 == 1) // switched on by UpdateObjectLight
			NoteId(object._olid, WarmFlame, FlameGain);
	}

	for (int i = 0; i < ActiveLightCount; i++) {
		const int id = ActiveLights[i];
		const Light &light = Lights[id];
		if (light.isInvalid)
			continue;
		if (TileHasAny(dPiece[light.position.tile.x][light.position.tile.y], TileProperties::Solid))
			continue; // ProcessLightList leaves a monster hidden in a wall unlit; so does its colour
		const float x = static_cast<float>(light.position.tile.x) + static_cast<float>(light.position.offset.deltaX) / 8.0F;
		const float y = static_cast<float>(light.position.tile.y) + static_cast<float>(light.position.offset.deltaY) / 8.0F;
		Sources.push_back({ x, y, static_cast<float>(light.radius + 1), IdColour[id], IdGain[id] });
	}
}

void RefreshLevelBrightness()
{
	// What each darkness level does to the level's palette on average: the floor pass darkens a full-bright pixel by
	// this much, so a tile centre comes out as bright as the per-tile draw made it.
	double full = 0;
	for (int i = 1; i < 255; i++) {
		const uint32_t c = PaletteRGB[i];
		full += Luma(static_cast<float>((c >> 16) & 0xFF), static_cast<float>((c >> 8) & 0xFF), static_cast<float>(c & 0xFF));
	}
	LevelBrightness[0] = One;
	for (size_t level = 1; level < NumLightingLevels; level++) {
		double lit = 0;
		for (int i = 1; i < 255; i++) {
			const uint32_t c = PaletteRGB[LightTables[level][i]];
			lit += Luma(static_cast<float>((c >> 16) & 0xFF), static_cast<float>((c >> 8) & 0xFF), static_cast<float>(c & 0xFF));
		}
		const double ratio = full > 0 ? lit / full : 0;
		LevelBrightness[level] = static_cast<uint16_t>(std::clamp(ratio, 0.0, 1.0) * One + 0.5);
	}
	LevelBrightness[NumLightingLevels - 1] = 0;
}

bool InLitDungeon()
{
	return leveltype != DTYPE_TOWN;
}

/** The floor tile under a world pixel: 16.16 tile coordinates, linear in the pixel's position. */
struct GroundMap {
	int64_t u0, v0;      // at pixel (0, 0)'s centre
	int64_t duX, dvX;    // per pixel to the right
	int64_t duY, dvY;    // per pixel down
};

GroundMap MakeGroundMap(Point firstTile, Point firstTileBuffer)
{
	// DrawFloor's walk: the first tile's 64x32 diamond has its bottom-left at firstTileBuffer, so its centre is 32 px
	// right and 16 px up; one tile +x is (+32, +16) on screen and one tile +y is (-32, +16). Inverted:
	//   u = x0 + rx/64 + ry/32,  v = y0 + ry/32 - rx/64   (rx, ry from that centre)
	const double cx = firstTileBuffer.x + 32.0;
	const double cy = firstTileBuffer.y - 16.0;
	const double rx = 0.5 - cx;
	const double ry = 0.5 - cy;
	constexpr double Fix = 65536.0;
	GroundMap m;
	m.u0 = static_cast<int64_t>(std::llround((firstTile.x + rx / 64.0 + ry / 32.0) * Fix));
	m.v0 = static_cast<int64_t>(std::llround((firstTile.y + ry / 32.0 - rx / 64.0) * Fix));
	m.duX = 1024;  // 1/64 tile
	m.dvX = -1024;
	m.duY = 2048; // 1/32 tile
	m.dvY = 2048;
	return m;
}

int ClampX(int64_t i)
{
	return static_cast<int>(std::clamp<int64_t>(i, 0, FieldW - 1));
}
int ClampY(int64_t i)
{
	return static_cast<int>(std::clamp<int64_t>(i, 0, FieldH - 1));
}

/** Bilinear between tile centres of a per-tile value, at 16.16 tile coordinates. */
template <typename Get>
int Bilinear(int64_t u, int64_t v, Get get)
{
	const int64_t iu = u >> 16;
	const int64_t iv = v >> 16;
	const int fu = static_cast<int>((u >> 8) & 0xFF);
	const int fv = static_cast<int>((v >> 8) & 0xFF);
	const int x0 = ClampX(iu), x1 = ClampX(iu + 1);
	const int y0 = ClampY(iv), y1 = ClampY(iv + 1);
	const int a = get(x0, y0), b = get(x1, y0), c = get(x0, y1), d = get(x1, y1);
	const int top = a * (256 - fu) + b * fu;
	const int bottom = c * (256 - fu) + d * fu;
	return (top * (256 - fv) + bottom * fv) >> 16;
}

/** Samples are taken this many pixels apart along a row and joined linearly - the field is smooth over a 64 px tile. */
constexpr int Span = 8;

} // namespace

void RebuildLightColourField()
{
	RefreshLevelBrightness();
	GatherSources();

	std::fill(Accumulated.begin(), Accumulated.end(), Accum {});
	int minX = FieldW, maxX = -1, minY = FieldH, maxY = -1;
	bool anyColour = false;
	for (const Source &s : Sources) {
		const int x0 = ClampX(static_cast<int64_t>(std::floor(s.x - s.reach)));
		const int x1 = ClampX(static_cast<int64_t>(std::ceil(s.x + s.reach)));
		const int y0 = ClampY(static_cast<int64_t>(std::floor(s.y - s.reach)));
		const int y1 = ClampY(static_cast<int64_t>(std::ceil(s.y + s.reach)));
		const float cr = static_cast<float>((s.rgb >> 16) & 0xFF) / 255.0F;
		const float cg = static_cast<float>((s.rgb >> 8) & 0xFF) / 255.0F;
		const float cb = static_cast<float>(s.rgb & 0xFF) / 255.0F;
		for (int y = y0; y <= y1; y++) {
			for (int x = x0; x <= x1; x++) {
				const float dx = static_cast<float>(x) - s.x;
				const float dy = static_cast<float>(y) - s.y;
				const float w = 1.0F - std::sqrt(dx * dx + dy * dy) / s.reach;
				if (w <= 0)
					continue;
				Accum &a = Accumulated[x * FieldH + y];
				if (s.rgb == 0) {
					a.neutral += w;
				} else {
					const float cw = w * s.gain;
					a.coloured += cw;
					a.neutral += w - cw; // the rest of a dimmed flame's light still lights the tile, as white
					a.r += cw * cr;
					a.g += cw * cg;
					a.b += cw * cb;
				}
			}
		}
		if (s.rgb != 0) {
			anyColour = true;
			minX = std::min(minX, x0);
			maxX = std::max(maxX, x1);
			minY = std::min(minY, y0);
			maxY = std::max(maxY, y1);
		}
	}

	std::fill(Tints.begin(), Tints.end(), LightTint { One, One, One });
	if (!anyColour) {
		TintMinX = 1, TintMaxX = 0, TintMinY = 1, TintMaxY = 0;
		return;
	}
	TintMinX = minX, TintMaxX = maxX, TintMinY = minY, TintMaxY = maxY;
	for (int x = minX; x <= maxX; x++) {
		for (int y = minY; y <= maxY; y++) {
			const Accum &a = Accumulated[x * FieldH + y];
			if (a.coloured <= 0)
				continue;
			const float r = a.r / a.coloured, g = a.g / a.coloured, b = a.b / a.coloured;
			const float luma = Luma(r, g, b);
			if (luma < 0.05F)
				continue;
			// The share of this tile's light that is coloured: alone, a light shows its full hue; beside the hero's
			// white light, half of it.
			const float strength = ColourStrength * a.coloured / std::max(a.coloured + a.neutral, 1e-3F);
			const auto lift = [&](float channel) {
				const float m = 1.0F + strength * (channel / luma - 1.0F);
				return static_cast<uint16_t>(std::clamp(m, 0.0F, MaxChannelLift) * One + 0.5F);
			};
			Tints[x * FieldH + y] = { lift(r), lift(g), lift(b) };
		}
	}
}

bool SmoothFloorLightActive(const Surface &out)
{
	// The Nest and the Crypt draw their lava full bright at every light level (MakeLightTable); a floor drawn bright
	// and darkened afterwards would dim it, so those two keep the original floors.
	return *sgOptions.Oracool.smoothLighting && !out.isIndexed() && InLitDungeon() && !IsAnyOf(leveltype, DTYPE_NEST, DTYPE_CRYPT);
}

bool ColouredLightActive(const Surface &out)
{
	return *sgOptions.Oracool.colouredLighting && !out.isIndexed() && InLitDungeon();
}

void ApplySmoothFloorLight(const Surface &out, Point firstTile, Point firstTileBuffer)
{
	if (!SmoothFloorLightActive(out) || out.w() <= 0 || out.h() <= 0)
		return;
	if (LevelBrightness[0] != One)
		RefreshLevelBrightness();
	const GroundMap m = MakeGroundMap(firstTile, firstTileBuffer);
	const auto brightness = [](int x, int y) {
		return static_cast<int>(LevelBrightness[std::min<int>(dLight[x][y], LightsMax)]);
	};
	const int w = out.w();
	for (int py = 0; py < out.h(); py++) {
		auto *row = out.at<uint32_t>(0, py);
		int64_t u = m.u0 + m.duY * py;
		int64_t v = m.v0 + m.dvY * py;
		int f0 = Bilinear(u, v, brightness);
		for (int sx = 0; sx < w; sx += Span) {
			const int len = std::min(Span, w - sx);
			const int64_t u1 = u + m.duX * len;
			const int64_t v1 = v + m.dvX * len;
			const int f1 = Bilinear(u1, v1, brightness);
			if (f0 < One || f1 < One) {
				int f = f0 << 8;
				const int df = ((f1 - f0) << 8) / len;
				for (int i = 0; i < len; i++, f += df) {
					const uint32_t c = row[sx + i];
					if (c == 0)
						continue;
					const uint32_t k = static_cast<uint32_t>(f >> 8);
					const uint32_t r = (((c >> 16) & 0xFF) * k) >> 8;
					const uint32_t g = (((c >> 8) & 0xFF) * k) >> 8;
					const uint32_t b = ((c & 0xFF) * k) >> 8;
					row[sx + i] = (r << 16) | (g << 8) | b;
				}
			}
			u = u1;
			v = v1;
			f0 = f1;
		}
	}
}

void ApplyLightColour(const Surface &out, Point firstTile, Point firstTileBuffer)
{
	if (!ColouredLightActive(out) || TintMinX > TintMaxX || out.w() <= 0 || out.h() <= 0)
		return;
	const GroundMap m = MakeGroundMap(firstTile, firstTileBuffer);

	// Only the screen rectangle the coloured tiles project to: the corners of their tile box, one tile out for the
	// bilinear's reach. (A pixel is tinted by the floor tile under it, so a wall takes the colour of the tiles behind it.)
	int left = out.w(), right = -1, top = out.h(), bottom = -1;
	for (const Point corner : { Point { TintMinX - 1, TintMinY - 1 }, Point { TintMaxX + 1, TintMinY - 1 }, Point { TintMinX - 1, TintMaxY + 1 }, Point { TintMaxX + 1, TintMaxY + 1 } }) {
		const int cx = firstTileBuffer.x + 32 + (corner.x - firstTile.x) * 32 - (corner.y - firstTile.y) * 32;
		const int cy = firstTileBuffer.y - 16 + (corner.x - firstTile.x) * 16 + (corner.y - firstTile.y) * 16;
		left = std::min(left, cx - 32);
		right = std::max(right, cx + 32);
		top = std::min(top, cy - 16);
		bottom = std::max(bottom, cy + 16);
	}
	left = std::max(left, 0);
	right = std::min(right, out.w() - 1);
	top = std::max(top, 0);
	bottom = std::min(bottom, out.h() - 1);
	if (left > right || top > bottom)
		return;

	const auto red = [](int x, int y) { return static_cast<int>(Tints[x * FieldH + y].r); };
	const auto green = [](int x, int y) { return static_cast<int>(Tints[x * FieldH + y].g); };
	const auto blue = [](int x, int y) { return static_cast<int>(Tints[x * FieldH + y].b); };
	for (int py = top; py <= bottom; py++) {
		auto *row = out.at<uint32_t>(0, py);
		int64_t u = m.u0 + m.duY * py + m.duX * left;
		int64_t v = m.v0 + m.dvY * py + m.dvX * left;
		int r0 = Bilinear(u, v, red), g0 = Bilinear(u, v, green), b0 = Bilinear(u, v, blue);
		for (int sx = left; sx <= right; sx += Span) {
			const int len = std::min(Span, right + 1 - sx);
			const int64_t u1 = u + m.duX * len;
			const int64_t v1 = v + m.dvX * len;
			const int r1 = Bilinear(u1, v1, red), g1 = Bilinear(u1, v1, green), b1 = Bilinear(u1, v1, blue);
			if (r0 != One || g0 != One || b0 != One || r1 != One || g1 != One || b1 != One) {
				int fr = r0 << 8, fg = g0 << 8, fb = b0 << 8;
				const int dr = ((r1 - r0) << 8) / len, dg = ((g1 - g0) << 8) / len, db = ((b1 - b0) << 8) / len;
				for (int i = 0; i < len; i++, fr += dr, fg += dg, fb += db) {
					const uint32_t c = row[sx + i];
					if (c == 0)
						continue;
					const uint32_t r = std::min<uint32_t>((((c >> 16) & 0xFF) * static_cast<uint32_t>(fr >> 8)) >> 8, 255);
					const uint32_t g = std::min<uint32_t>((((c >> 8) & 0xFF) * static_cast<uint32_t>(fg >> 8)) >> 8, 255);
					const uint32_t b = std::min<uint32_t>(((c & 0xFF) * static_cast<uint32_t>(fb >> 8)) >> 8, 255);
					row[sx + i] = (r << 16) | (g << 8) | b;
				}
			}
			u = u1;
			v = v1;
			r0 = r1, g0 = g1, b0 = b1;
		}
	}
}

LightTint LightTintAt(Point tile)
{
	return Tints[ClampX(tile.x) * FieldH + ClampY(tile.y)];
}

uint16_t SmoothLevelBrightness(uint8_t level)
{
	return LevelBrightness[std::min<size_t>(level, NumLightingLevels - 1)];
}

uint32_t LightColourForId(int lightId)
{
	return lightId >= 0 && lightId < MAXLIGHTS ? IdColour[lightId] : 0;
}

} // namespace devilution::oracool

#ifdef _MSC_VER
#pragma optimize("", on)
#pragma runtime_checks("", restore)
#endif
