/**
 * @file oracool/colour_light.cpp
 *
 * Coloured and smooth dynamic lighting - see colour_light.h.
 */
#include "oracool/colour_light.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cctype>
#include <cstdint>
#include <initializer_list>
#include <string>
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
#include "oracool/monster_variants.h"

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
/** The most one channel may be lifted, so a red light cannot bleach a dark wall to pink (1.9 until round 2 of the audit). */
constexpr float MaxChannelLift = 1.5F;
constexpr float MaxBlueLift = 1.35F;

struct Accum {
	float coloured; // summed weight of coloured lights
	float neutral;  // summed weight of white lights (the hero's own)
	float r, g, b;  // weight-summed colour, 0..1 a channel
};

std::array<Accum, FieldW * FieldH> Accumulated;
std::array<LightTint, FieldW * FieldH> Tints;
/** The smooth pass's per-tile brightness, refilled from dLight each frame. */
std::array<uint16_t, FieldW * FieldH> BrightPlane;
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
 * The hue a unique's recolour moves it to: the colour its TRN SHIFTS toward - the rewritten entries' average, pushed
 * further along the difference from the entries they were (round 2 audit: the plain average of the targets is half the
 * palette, the same for every recolour). A recolour that only darkens or greys out reads as neutral.
 */
uint32_t TrnHue(const uint8_t *trn)
{
	float tr = 0, tg = 0, tb = 0, sr = 0, sg = 0, sb = 0;
	int n = 0;
	for (int i = 1; i < 255; i++) {
		if (trn[i] == i)
			continue;
		const uint32_t c = PaletteRGB[trn[i]];
		const uint32_t o = PaletteRGB[i];
		tr += static_cast<float>((c >> 16) & 0xFF);
		tg += static_cast<float>((c >> 8) & 0xFF);
		tb += static_cast<float>(c & 0xFF);
		sr += static_cast<float>((o >> 16) & 0xFF);
		sg += static_cast<float>((o >> 8) & 0xFF);
		sb += static_cast<float>(o & 0xFF);
		n++;
	}
	if (n < 8)
		return 0;
	const float r = std::max(0.0F, tr + 2.0F * (tr - sr));
	const float g = std::max(0.0F, tg + 2.0F * (tg - sg));
	const float b = std::max(0.0F, tb + 2.0F * (tb - sb));
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
	// A champion's affix has a colour of its own; its TRN is only a small shift along its own ramps (round 2 audit).
	if (const uint32_t affix = AffixHue(monster.lesserAffix); affix != 0)
		return affix;
	if (VariantOf(monster) == MonsterVariant::Luminous)
		return WarmFlame;
	if (monster.uniqueMonsterTRN != nullptr)
		return TrnHue(monster.uniqueMonsterTRN.get());
	return 0;
}

/**
 * The colour an Orcl effect's ART says it is, from its sheet's file name: the skill effects ride carrier missiles whose
 * rows are Physical (AddArtEffect's AcidCloud, AddArtBolt's AcidJavelin, the census bolts), so the damage type cannot tell
 * a Furnace Flame from an Absolute Zero (round 2 audit, v1.12.421). Worked out once a graphic.
 */
uint32_t ArtNameHue(MissileGraphicID graphic)
{
	static std::array<uint32_t, 256> cache {};
	static std::array<bool, 256> known {};
	const auto i = static_cast<size_t>(graphic);
	if (i >= cache.size() || graphic >= MissileGraphicID::None)
		return 0;
	if (known[i])
		return cache[i];
	known[i] = true;
	std::string name = GetMissileSpriteData(graphic).name;
	for (char &c : name)
		c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
	const auto has = [&](std::initializer_list<const char *> words) {
		for (const char *word : words) {
			if (name.find(word) != std::string::npos)
				return true;
		}
		return false;
	};
	uint32_t rgb = 0;
	if (has({ "holy", "divine", "bless", "heaven", "smite", "sanct", "angel", "zeal" }))
		rgb = hue::PaladinGold;
	else if (has({ "blood", "blod" }))
		rgb = hue::BloodRed;
	else if (has({ "poison", "acid", "venom", "plague", "toxic", "bile" }))
		rgb = hue::PoisonGreen;
	else if (has({ "ice", "frost", "cold", "chill", "zero", "glint", "snow", "freez", "blizz", "glacial", "frozen", "shiver" }))
		rgb = hue::IceBlue;
	else if (has({ "fire", "flame", "ember", "ash", "burn", "infern", "meteor", "lava", "magma", "furnace", "funeral", "blaze" }))
		rgb = hue::FireOrange;
	else if (has({ "lightn", "lght", "storm", "thunder", "spark", "static", "shock", "conduit", "faraday", "rod", "volt", "arc_" }))
		rgb = hue::LightningBlue;
	cache[i] = rgb;
	return rgb;
}

uint32_t MissileLightHue(const Missile &missile)
{
	// Any recolour that names a hue (Hue, HueCycle, Glint...): Guided Arrow's fire glint, a mage's bolt (round 2 audit).
	if (missile.oracoolTint != Tint::None && missile.oracoolTintRgb != 0)
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
	default: return ArtNameHue(missile._miAnimType);
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
	// this much, so a tile centre comes out as bright as the per-tile draw made it. Hell's 1..31 are its own blood ramp
	// (MakeLightTable rewrites them, LightTables[0] included) and would bias the average: left out (round 1 audit).
	const int first = leveltype == DTYPE_HELL ? 32 : 1;
	double full = 0;
	for (int i = first; i < 255; i++) {
		const uint32_t c = PaletteRGB[i];
		full += Luma(static_cast<float>((c >> 16) & 0xFF), static_cast<float>((c >> 8) & 0xFF), static_cast<float>(c & 0xFF));
	}
	LevelBrightness[0] = One;
	for (size_t level = 1; level < NumLightingLevels; level++) {
		double lit = 0;
		for (int i = first; i < 255; i++) {
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
	// right and 14.5 px up; one tile +x is (+32, +16) on screen and one tile +y is (-32, +16). Inverted:
	//   u = x0 + rx/64 + ry/32,  v = y0 + ry/32 - rx/64   (rx, ry from that centre)
	const double cx = firstTileBuffer.x + 32.0;
	const double cy = firstTileBuffer.y - 14.5; // a floor triangle is rows y-30..y, widest at y-15 (round 1 audit)
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
			// Each channel lifted or lowered toward the hue, capped (blue lower: Diablo's art carries little of it, and a
			// lifted blue only clips), then the three rescaled to keep a grey pixel's brightness - so a red light no
			// longer darkens the floor it reddens (round 2 audit, v1.12.421).
			float lr = std::clamp(1.0F + strength * (r / luma - 1.0F), 0.0F, MaxChannelLift);
			float lg = std::clamp(1.0F + strength * (g / luma - 1.0F), 0.0F, MaxChannelLift);
			float lb = std::clamp(1.0F + strength * (b / luma - 1.0F), 0.0F, MaxBlueLift);
			const float lumaLift = Luma(lr, lg, lb);
			if (lumaLift > 0.05F) {
				lr /= lumaLift;
				lg /= lumaLift;
				lb /= lumaLift;
			}
			const auto fixed = [](float m, float cap) { return static_cast<uint16_t>(std::clamp(m, 0.0F, cap) * One + 0.5F); };
			Tints[x * FieldH + y] = { fixed(lr, MaxChannelLift), fixed(lg, MaxChannelLift), fixed(lb, MaxBlueLift) };
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

	// Everything below is written out by hand - no helpers, no std::clamp, no lambdas, no std::array operator[] in the
	// loops. The pragma above optimises this file even in the Debug build, but /Ob0 still forbids inlining, so every
	// call would stay a call: about 25 million a frame at 1080p before round 2 of the audit (v1.12.421).
	uint16_t *plane = BrightPlane.data();
	const uint16_t *levels = LevelBrightness.data();
	const uint8_t *light = &dLight[0][0];
	for (int i = 0; i < FieldW * FieldH; i++) {
		const int level = light[i];
		plane[i] = levels[level > LightsMax ? LightsMax : level];
	}

	const GroundMap m = MakeGroundMap(firstTile, firstTileBuffer);
	const int w = out.w();
	for (int py = 0; py < out.h(); py++) {
		uint32_t *row = out.at<uint32_t>(0, py);
		const int64_t rowU = m.u0 + m.duY * py;
		const int64_t rowV = m.v0 + m.dvY * py;
		int f0 = 0;
		for (int sx = 0;; sx += Span) {
			const int x = sx < w ? sx : w;
			const int64_t u = rowU + m.duX * x;
			const int64_t v = rowV + m.dvX * x;
			// The bilinear sample between the four tile centres around (u, v).
			const int64_t iu = u >> 16;
			const int64_t iv = v >> 16;
			const int fu = static_cast<int>((u >> 8) & 0xFF);
			const int fv = static_cast<int>((v >> 8) & 0xFF);
			const int x0 = iu < 0 ? 0 : (iu > FieldW - 1 ? FieldW - 1 : static_cast<int>(iu));
			const int x1 = iu + 1 < 0 ? 0 : (iu + 1 > FieldW - 1 ? FieldW - 1 : static_cast<int>(iu + 1));
			const int y0 = iv < 0 ? 0 : (iv > FieldH - 1 ? FieldH - 1 : static_cast<int>(iv));
			const int y1 = iv + 1 < 0 ? 0 : (iv + 1 > FieldH - 1 ? FieldH - 1 : static_cast<int>(iv + 1));
			const int top = plane[x0 * FieldH + y0] * (256 - fu) + plane[x1 * FieldH + y0] * fu;
			const int bottom = plane[x0 * FieldH + y1] * (256 - fu) + plane[x1 * FieldH + y1] * fu;
			const int f1 = (top * (256 - fv) + bottom * fv) >> 16;
			if (sx > 0) {
				const int from = sx - Span;
				const int len = x - from;
				if (len > 0 && (f0 < One || f1 < One)) {
					int f = f0 << 8;
					const int df = ((f1 - f0) << 8) / len;
					for (int i = from; i < x; i++, f += df) {
						const uint32_t c = row[i];
						if (c == 0)
							continue;
						const uint32_t k = static_cast<uint32_t>(f >> 8);
						row[i] = (((((c >> 16) & 0xFF) * k) >> 8) << 16) | (((((c >> 8) & 0xFF) * k) >> 8) << 8) | (((c & 0xFF) * k) >> 8);
					}
				}
			}
			f0 = f1;
			if (x >= w)
				break;
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
		const int cy = firstTileBuffer.y - 15 + (corner.x - firstTile.x) * 16 + (corner.y - firstTile.y) * 16;
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

	// Written out by hand, as ApplySmoothFloorLight is (no calls survive /Ob0): the four tints read once per sample and
	// all three channels interpolated with the same weights.
	const LightTint *tints = Tints.data();
	const int end = right + 1;
	for (int py = top; py <= bottom; py++) {
		uint32_t *row = out.at<uint32_t>(0, py);
		const int64_t rowU = m.u0 + m.duY * py;
		const int64_t rowV = m.v0 + m.dvY * py;
		int r0 = One, g0 = One, b0 = One;
		for (int sx = left;; sx += Span) {
			const int x = sx < end ? sx : end;
			const int64_t u = rowU + m.duX * x;
			const int64_t v = rowV + m.dvX * x;
			const int64_t iu = u >> 16;
			const int64_t iv = v >> 16;
			const int fu = static_cast<int>((u >> 8) & 0xFF);
			const int fv = static_cast<int>((v >> 8) & 0xFF);
			const int x0 = iu < 0 ? 0 : (iu > FieldW - 1 ? FieldW - 1 : static_cast<int>(iu));
			const int x1 = iu + 1 < 0 ? 0 : (iu + 1 > FieldW - 1 ? FieldW - 1 : static_cast<int>(iu + 1));
			const int y0 = iv < 0 ? 0 : (iv > FieldH - 1 ? FieldH - 1 : static_cast<int>(iv));
			const int y1 = iv + 1 < 0 ? 0 : (iv + 1 > FieldH - 1 ? FieldH - 1 : static_cast<int>(iv + 1));
			const LightTint &ta = tints[x0 * FieldH + y0];
			const LightTint &tb = tints[x1 * FieldH + y0];
			const LightTint &tc = tints[x0 * FieldH + y1];
			const LightTint &td = tints[x1 * FieldH + y1];
			const int wa = (256 - fu) * (256 - fv), wb = fu * (256 - fv), wc = (256 - fu) * fv, wd = fu * fv;
			const int r1 = (ta.r * wa + tb.r * wb + tc.r * wc + td.r * wd) >> 16;
			const int g1 = (ta.g * wa + tb.g * wb + tc.g * wc + td.g * wd) >> 16;
			const int b1 = (ta.b * wa + tb.b * wb + tc.b * wc + td.b * wd) >> 16;
			if (sx > left) {
				const int from = sx - Span;
				const int len = x - from;
				if (len > 0 && (r0 != One || g0 != One || b0 != One || r1 != One || g1 != One || b1 != One)) {
					int fr = r0 << 8, fg = g0 << 8, fb = b0 << 8;
					const int dr = ((r1 - r0) << 8) / len, dg = ((g1 - g0) << 8) / len, db = ((b1 - b0) << 8) / len;
					for (int i = from; i < x; i++, fr += dr, fg += dg, fb += db) {
						const uint32_t c = row[i];
						if (c == 0)
							continue;
						uint32_t r = (((c >> 16) & 0xFF) * static_cast<uint32_t>(fr >> 8)) >> 8;
						uint32_t g = (((c >> 8) & 0xFF) * static_cast<uint32_t>(fg >> 8)) >> 8;
						uint32_t b = ((c & 0xFF) * static_cast<uint32_t>(fb >> 8)) >> 8;
						r = r > 255 ? 255 : r;
						g = g > 255 ? 255 : g;
						b = b > 255 ? 255 : b;
						row[i] = (r << 16) | (g << 8) | b;
					}
				}
			}
			r0 = r1, g0 = g1, b0 = b1;
			if (x >= end)
				break;
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
