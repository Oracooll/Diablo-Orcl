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

#include <SDL.h>

#include "diablo.h"
#include "engine/palette.h"
#include "engine/surface.hpp"
#include "items.h"
#include "levels/gendung.h"
#include "lighting.h"
#include "misdat.h"
#include "missiles.h"
#include "monster.h"
#include "objects.h"
#include "options.h"
#include "gmenu.h"
#include "multi.h"
#include "oracool/ground_fx.h"
#include "oracool/lesser_uniques.h"
#include "oracool/missile_tint.h"
#include "oracool/monster_variants.h"
#include "player.h"
#include "towners.h"

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
/**
 * Lit surface relief (user, 2026-10-11): the way the light comes from at each tile, in screen space, 8.8 - the sum of every
 * light's direction weighted by its falloff, so two lights either side cancel and a single torch points hard. Rebuilt with
 * the colour field.
 */
std::array<int16_t, FieldW * FieldH> ReliefDirX;
std::array<int16_t, FieldW * FieldH> ReliefDirY;
bool ReliefBuilt = false;
/** The smooth pass's per-tile brightness, refilled from dLight each frame. */
std::array<uint16_t, FieldW * FieldH> BrightPlane;
/** The tiles any coloured light reaches, inclusive; empty when minX > maxX. */
int TintMinX = 1, TintMaxX = 0, TintMinY = 1, TintMaxY = 0;

std::array<uint32_t, MAXLIGHTS> IdColour {};
std::array<float, MAXLIGHTS> IdGain {};
std::array<bool, MAXLIGHTS> IdFlame {};
std::array<uint16_t, NumLightingLevels> LevelBrightness {};

struct Source {
	float x;
	float y;
	float reach; // tiles at which the light is gone
	uint32_t rgb; // 0 = neutral
	float gain;   // how strongly its colour counts
	bool flame;   // a torch, candle, brazier, burning cross or flame trap: it flickers
};
std::vector<Source> Sources;
/** Flicker: the level's flames, each wavering on its own phase. Rebuilt with the sources; applied each frame. */
struct Flame {
	float x;
	float y;
	float reach;
	float phase;
};
std::vector<Flame> Flames;

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
 * palette, the same for every recolour). A recolour that darkens by a third or more, or greys out, reads as neutral; a
 * gentler darkening reads as its sources' own hue, saturated - only real uniques reach this, and theirs are hues.
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
	// Round 3 of the audit: "unholy" before "holy", "ashen" not "ash" (manashld, blinding_flash), "storm_" not "storm"
	// (bone_storm), and the vanilla sheets a Physical row can borrow (magblos, miniltng, the flare explosions).
	if (has({ "unholy" }))
		rgb = hue::DuskViolet;
	else if (has({ "holy", "divine", "bless", "heaven", "smite", "sanct", "angel", "zeal", "ex_yel" }))
		rgb = hue::PaladinGold;
	else if (has({ "blood", "blod", "ex_red" }))
		rgb = hue::BloodRed;
	else if (has({ "poison", "acid", "venom", "plague", "toxic", "bile" }))
		rgb = hue::PoisonGreen;
	else if (has({ "ice", "frost", "cold", "chill", "zero", "glint", "snow", "freez", "blizz", "glacial", "frozen", "shiver", "whiteout" }))
		rgb = hue::IceBlue;
	else if (has({ "fire", "flame", "ember", "ashen", "burn", "infern", "meteor", "lava", "magma", "furnace", "funeral", "blaze", "magblos", "magball", "cinder", "immolat", "ex_ora" }))
		rgb = hue::FireOrange;
	else if (has({ "lightn", "lght", "lghning", "miniltng", "ex_blu", "storm_", "thunder", "spark", "static", "shock", "conduit", "faraday", "rod", "volt", "arc_" }))
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
	switch (missile._mitype) {
	case MissileID::RuneOfFire:
	case MissileID::RuneOfImmolation: return hue::FireOrange;
	case MissileID::RuneOfLight:
	case MissileID::RuneOfNova: return hue::LightningBlue;
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

void NoteId(int id, uint32_t rgb, float gain = 1.0F, bool flame = false)
{
	if (id >= 0 && id < MAXLIGHTS) {
		IdColour[id] = rgb;
		IdGain[id] = gain;
		IdFlame[id] = flame;
	}
}

void GatherSources()
{
	IdColour.fill(0);
	IdGain.fill(1.0F);
	IdFlame.fill(false);
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
				NoteId(object._olid, hue::FireOrange, FlameGain, true);
			continue;
		}
		const int radius = BakedObjectLightRadius(object);
		if (radius == 0)
			continue;
		if (object._oVar1 == -1) // baked into dPreLight at level load (AddObjectLight): no light id, always there
			Sources.push_back({ static_cast<float>(object.position.x), static_cast<float>(object.position.y), static_cast<float>(radius + 1), WarmFlame, FlameGain, true });
		else if (object._oVar1 == 1) // switched on by UpdateObjectLight
			NoteId(object._olid, WarmFlame, FlameGain, true);
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
		Sources.push_back({ x, y, static_cast<float>(light.radius + 1), IdColour[id], IdGain[id], IdFlame[id] });
	}

	Flames.clear();
	for (const Source &source : Sources) {
		if (!source.flame)
			continue;
		// A phase from the flame's place, so two torches side by side never pulse together.
		const uint32_t seed = static_cast<uint32_t>(source.x * 73.0F) * 2654435761U ^ static_cast<uint32_t>(source.y * 151.0F) * 40503U;
		Flames.push_back({ source.x, source.y, source.reach, static_cast<float>(seed % 6283U) / 1000.0F });
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

// =================================================================================================================
// Ray-traced shadows, wall shadows and flicker (user, 2026-10-10: three of the nine visual techniques)
// =================================================================================================================

/**
 * Ray-traced shadows: how much darker each tile is because walls stop the lights - every light worked out twice with
 * MakeLightTable's linear falloff (15 levels over radius + 1 tiles), once as the engine does it (through walls) and once
 * counting a light only when the ray from it to the tile's centre crosses no BlockLight tile. The floor pass adds the
 * difference to dLight. A DARKENING, not a replacement (visual audit round 1): light the engine has that is no source
 * here - the Caves' lava, baked by PlaceCaveLights into dPreLight - survives, and the two falloffs' rounding cancels.
 * Visual only: dLight, which decides what the hero sees and what is drawn, is untouched.
 */
std::array<uint8_t, FieldW * FieldH> ShadowDarkening;
std::array<uint8_t, FieldW * FieldH> OpenLevel;
std::array<uint8_t, FieldW * FieldH> ShadedLevel;
std::array<uint8_t, FieldW * FieldH> Blocks;
uint64_t BlocksKey = 0;
bool ShadowsBuilt = false;
uint64_t LevelKey();

/**
 * Walks the tiles between @p from and the centre of @p to (Amanatides-Woo); true when one of them blocks light. A light
 * standing in a wall (the Catacombs' wall torches, AddL2Torches) ignores the walls touching its own tile, or its rays
 * along the wall would leave through the next wall tile and the torch would light a wedge (visual audit round 1).
 */
bool RayBlocked(float sx, float sy, int tx, int ty, bool fromInsideWall)
{
	int cx = static_cast<int>(std::floor(sx + 0.5F));
	int cy = static_cast<int>(std::floor(sy + 0.5F));
	const int startX = cx;
	const int startY = cy;
	const float dx = static_cast<float>(tx) - sx;
	const float dy = static_cast<float>(ty) - sy;
	const int stepX = dx > 0 ? 1 : -1;
	const int stepY = dy > 0 ? 1 : -1;
	constexpr float Far = 1e9F;
	const float tDeltaX = dx != 0 ? std::abs(1.0F / dx) : Far;
	const float tDeltaY = dy != 0 ? std::abs(1.0F / dy) : Far;
	float tMaxX = dx != 0 ? ((static_cast<float>(cx) + 0.5F * static_cast<float>(stepX)) - sx) / dx : Far;
	float tMaxY = dy != 0 ? ((static_cast<float>(cy) + 0.5F * static_cast<float>(stepY)) - sy) / dy : Far;
	for (int guard = 0; guard < 64 && (cx != tx || cy != ty); guard++) {
		if (tMaxX < tMaxY) {
			tMaxX += tDeltaX;
			cx += stepX;
		} else {
			tMaxY += tDeltaY;
			cy += stepY;
		}
		if (cx == tx && cy == ty)
			return false; // the target itself is never its own blocker: a lit wall face is a wall
		if (cx < 0 || cy < 0 || cx >= FieldW || cy >= FieldH)
			return true;
		if (Blocks[cx * FieldH + cy] != 0) {
			if (fromInsideWall && std::abs(cx - startX) <= 1 && std::abs(cy - startY) <= 1)
				continue;
			return true;
		}
	}
	return false;
}

void BuildShadows()
{
	ShadowsBuilt = false;
	if (!*sgOptions.Oracool.rayTracedShadows || !InLitDungeon())
		return;
	// The walls change only with the map: rebuilt when the level key moves, not every tick (visual audit round 1).
	if (const uint64_t key = LevelKey(); key != BlocksKey) {
		BlocksKey = key;
		for (int x = 0; x < FieldW; x++)
			for (int y = 0; y < FieldH; y++)
				Blocks[x * FieldH + y] = TileHasAny(dPiece[x][y], TileProperties::BlockLight) ? 1 : 0;
	}
	OpenLevel.fill(LightsMax);
	ShadedLevel.fill(LightsMax);
	for (const Source &s : Sources) {
		const int sx = static_cast<int>(std::floor(s.x + 0.5F));
		const int sy = static_cast<int>(std::floor(s.y + 0.5F));
		const bool insideWall = sx >= 0 && sy >= 0 && sx < FieldW && sy < FieldH && Blocks[sx * FieldH + sy] != 0;
		const int x0 = ClampX(static_cast<int64_t>(std::floor(s.x - s.reach)));
		const int x1 = ClampX(static_cast<int64_t>(std::ceil(s.x + s.reach)));
		const int y0 = ClampY(static_cast<int64_t>(std::floor(s.y - s.reach)));
		const int y1 = ClampY(static_cast<int64_t>(std::ceil(s.y + s.reach)));
		for (int y = y0; y <= y1; y++) {
			for (int x = x0; x <= x1; x++) {
				const float dx = static_cast<float>(x) - s.x;
				const float dy = static_cast<float>(y) - s.y;
				const float d = std::sqrt(dx * dx + dy * dy);
				if (d > s.reach)
					continue;
				const int level = static_cast<int>(d / s.reach * LightsMax + 0.5F);
				const size_t i = static_cast<size_t>(x) * FieldH + y;
				if (level < OpenLevel[i])
					OpenLevel[i] = static_cast<uint8_t>(level);
				if (level >= ShadedLevel[i])
					continue;
				if (RayBlocked(s.x, s.y, x, y, insideWall))
					continue;
				ShadedLevel[i] = static_cast<uint8_t>(level);
			}
		}
	}
	for (size_t i = 0; i < ShadowDarkening.size(); i++)
		ShadowDarkening[i] = ShadedLevel[i] > OpenLevel[i] ? static_cast<uint8_t>(ShadedLevel[i] - OpenLevel[i]) : 0;
	ShadowsBuilt = true;
}

/**
 * Wall shadows (ambient occlusion): a darkening of the floor where it meets a solid tile, at half-tile resolution so the
 * bilinear sample makes a gradient along the wall's foot rather than darkening whole tiles. Built once a level, keyed on
 * where we are and a checksum of dPiece.
 */
constexpr int AoW = FieldW * 2;
constexpr int AoH = FieldH * 2;
constexpr float AoReach = 0.6F;    // tiles from a wall's edge at which the shadow is gone
constexpr float AoStrength = 0.38F; // how dark the floor gets right at the wall's foot
std::array<uint16_t, AoW * AoH> AoPlane;
uint64_t AoKey = 0;
bool AoBuilt = false;

uint64_t LevelKey()
{
	uint64_t h = 1469598103934665603ULL;
	const auto mix = [&h](uint64_t v) { h = (h ^ v) * 1099511628211ULL; };
	mix(static_cast<uint64_t>(leveltype));
	mix(currlevel);
	mix(setlevel ? 1 : 0);
	mix(static_cast<uint64_t>(setlvlnum));
	const uint16_t *pieces = &dPiece[0][0];
	for (int i = 0; i < FieldW * FieldH; i++) // every piece: a small map change must not leave the shadows stale
		h = (h ^ pieces[i]) * 1099511628211ULL;
	return h;
}

void EnsureAoPlane()
{
	const uint64_t key = LevelKey();
	if (AoBuilt && key == AoKey)
		return;
	AoKey = key;
	AoBuilt = true;
	for (int x = 0; x < FieldW; x++) {
		for (int y = 0; y < FieldH; y++) {
			// BlockLight, not Solid (visual audit round 1): lava and water stop feet, not light, and must get no dark rim.
			const bool selfSolid = TileHasAny(dPiece[x][y], TileProperties::BlockLight);
			for (int i = 0; i < 2; i++) {
				for (int j = 0; j < 2; j++) {
					const float px = static_cast<float>(x) - 0.25F + 0.5F * static_cast<float>(i);
					const float py = static_cast<float>(y) - 0.25F + 0.5F * static_cast<float>(j);
					float occlusion = selfSolid ? 1.0F : 0.0F;
					for (int nx = x - 1; nx <= x + 1 && occlusion < 1.0F; nx++) {
						for (int ny = y - 1; ny <= y + 1; ny++) {
							if ((nx == x && ny == y) || nx < 0 || ny < 0 || nx >= FieldW || ny >= FieldH)
								continue;
							if (!TileHasAny(dPiece[nx][ny], TileProperties::BlockLight))
								continue;
							// Distance from the sample to the solid tile's square.
							const float ex = std::max({ static_cast<float>(nx) - 0.5F - px, 0.0F, px - (static_cast<float>(nx) + 0.5F) });
							const float ey = std::max({ static_cast<float>(ny) - 0.5F - py, 0.0F, py - (static_cast<float>(ny) + 0.5F) });
							occlusion = std::max(occlusion, 1.0F - std::sqrt(ex * ex + ey * ey) / AoReach);
						}
					}
					const float factor = 1.0F - AoStrength * std::clamp(occlusion, 0.0F, 1.0F);
					AoPlane[(2 * x + i) * AoH + (2 * y + j)] = static_cast<uint16_t>(factor * One + 0.5F);
				}
			}
		}
	}
}

std::array<LightTint, FieldW * FieldH> FrameTints;
std::array<float, FieldW * FieldH> FlickerMul;
std::array<float, FieldW * FieldH> ReliefSumX;
std::array<float, FieldW * FieldH> ReliefSumY;
/** How far up the relief's light direction leans, against a light's own pull (1). */
constexpr float ReliefUpBias = 0.3F;
/** The most the relief brightens or darkens a pixel, of 256 (audit round 1: 56 hit on almost every edge). */
constexpr int ReliefMax = 40;
/** How far from a light its pull is full, in tiles. */
constexpr float ReliefNear = 3.0F;

/**
 * Multiplies the pixels of @p out in [left, right] x [top, bottom] by the tint field @p tints under them - the colour
 * pass's loop, shared with the town's day and night. Written out by hand: no calls survive /Ob0.
 */
void TintRect(const Surface &out, const GroundMap &m, const LightTint *tints, int left, int right, int top, int bottom)
{
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

// =================================================================================================================
// Day and night in Tristram (user, 2026-10-11)
// =================================================================================================================

/** A full day, in real seconds: twenty minutes, a game hour every 50 s. */
constexpr float DaySeconds = 1200.0F;
/** The town's clock, in hours; the first game starts mid-morning. Stopped while paused. */
float TownClock = 10.0F;
uint32_t TownClockTicks = 0;

struct AmbientKey {
	float hour;
	float r, g, b;
};
// Night, dawn's rose, the day, dusk's amber, back to night.
constexpr AmbientKey AmbientKeys[] = {
	{ 0.0F, 0.34F, 0.38F, 0.60F },
	{ 4.5F, 0.34F, 0.38F, 0.60F },
	{ 6.0F, 0.86F, 0.70F, 0.70F },
	{ 7.5F, 1.00F, 1.00F, 1.00F },
	{ 16.5F, 1.00F, 1.00F, 1.00F },
	{ 18.0F, 1.00F, 0.76F, 0.52F },
	{ 19.5F, 0.46F, 0.42F, 0.60F },
	{ 20.5F, 0.34F, 0.38F, 0.60F },
	{ 24.0F, 0.34F, 0.38F, 0.60F },
};

void AdvanceTownClock()
{
	const uint32_t now = SDL_GetTicks();
	float dt = TownClockTicks == 0 ? 0.0F : static_cast<float>(now - TownClockTicks) / 1000.0F;
	TownClockTicks = now == 0 ? 1 : now;
	dt = dt < 0 ? 0 : (dt > 0.25F ? 0.25F : dt);
	if (PauseMode != 0 || (!gbIsMultiplayer && gmenu_is_active()))
		dt = 0; // the game stands still with the single-player menu up, and so does the clock (audit round 1)
	TownClock = std::fmod(TownClock + dt * 24.0F / DaySeconds, 24.0F);
}

/** The town's ambient light at @p hour, per channel 0..1. */
void AmbientAt(float hour, float &r, float &g, float &b)
{
	hour = std::fmod(hour, 24.0F);
	if (hour < 0)
		hour += 24.0F;
	for (size_t i = 1; i < sizeof(AmbientKeys) / sizeof(AmbientKeys[0]); i++) {
		const AmbientKey &k1 = AmbientKeys[i];
		if (hour > k1.hour)
			continue;
		const AmbientKey &k0 = AmbientKeys[i - 1];
		const float t = k1.hour > k0.hour ? (hour - k0.hour) / (k1.hour - k0.hour) : 0.0F;
		r = k0.r + (k1.r - k0.r) * t;
		g = k0.g + (k1.g - k0.g) * t;
		b = k0.b + (k1.b - k0.b) * t;
		return;
	}
	r = g = b = 1.0F;
}

struct TownLight {
	float x, y, reach, strength;
	float r, g, b;
};

/** The town's lights after dark: the heroes' own, the townsfolk's lamps, the portals, the loot. */
const std::vector<TownLight> &GatherTownLights()
{
	static std::vector<TownLight> lights; // reused, not grown every frame from dusk to dawn (audit round 1)
	lights.clear();
	const auto addRgb = [](float x, float y, float reach, float strength, uint32_t rgb) {
		const float r = static_cast<float>((rgb >> 16) & 0xFF) / 255.0F;
		const float g = static_cast<float>((rgb >> 8) & 0xFF) / 255.0F;
		const float b = static_cast<float>(rgb & 0xFF) / 255.0F;
		const float hi = std::max({ r, g, b, 1e-3F });
		lights.push_back({ x, y, reach, strength, r / hi, g / hi, b / hi });
	};
	for (const Player &player : Players) {
		if (!player.plractive || !player.isOnActiveLevel())
			continue;
		float x = static_cast<float>(player.position.tile.x);
		float y = static_cast<float>(player.position.tile.y);
		if (player.isWalking()) {
			// The walk between tiles, as UpdatePlayerLightOffset works it out in the dungeon (eighths of a tile).
			const WorldTileDisplacement offset = player.position.CalculateWalkingOffset(player._pdir, player.AnimInfo);
			const auto light = offset.screenToLight();
			x += static_cast<float>(light.deltaX) / 8.0F;
			y += static_cast<float>(light.deltaY) / 8.0F;
		}
		lights.push_back({ x, y, &player == MyPlayer ? 9.5F : 7.0F, 1.0F, 1.0F, 0.92F, 0.80F });
	}
	for (size_t t = 0; t < NUM_TOWNERS; t++) {
		const Towner &towner = Towners[t];
		// Only one standing where it says (audit round 1): InitTowners fills only the townsfolk present, and a slot past
		// them still holds whoever stood there last game. The town's dMonster holds a towner's index + 1.
		const Point at = towner.position;
		if (!towner.anim || towner._ttype == TOWN_COW || at.x <= 0 || at.y <= 0 || at.x >= MAXDUNX || at.y >= MAXDUNY
		    || dMonster[at.x][at.y] != static_cast<int>(t) + 1)
			continue;
		lights.push_back({ static_cast<float>(towner.position.x), static_cast<float>(towner.position.y), 4.5F, 0.9F, 1.0F, 0.74F, 0.46F });
	}
	for (const Missile &missile : Missiles) {
		// A portal in town has no light of its own (ProcessTownPortal lights it only in the dungeon): by its kind.
		if (missile._mlid == NO_LIGHT && missile._mitype != MissileID::TownPortal && missile._mitype != MissileID::RedPortal)
			continue;
		const uint32_t rgb = MissileLightHue(missile);
		addRgb(static_cast<float>(missile.position.tile.x), static_cast<float>(missile.position.tile.y), 5.0F, 0.9F, rgb != 0 ? rgb : WarmFlame);
	}
	if (*sgOptions.Oracool.lootBeams) {
		for (uint8_t i = 0; i < ActiveItemCount; i++) {
			const Item &item = Items[ActiveItems[i]];
			const LootLook look = LootLookFor(item);
			if (look.rgb != 0 && LootShows(item))
				addRgb(static_cast<float>(item.position.x), static_cast<float>(item.position.y), 2.5F, 0.7F * look.strength, look.rgb);
		}
	}
	return lights;
}

std::array<float, FieldW * FieldH> TownLitR;
std::array<float, FieldW * FieldH> TownLitG;
std::array<float, FieldW * FieldH> TownLitB;
std::array<LightTint, FieldW * FieldH> TownTints;
constexpr float FlickerDepth = 0.13F; // at a flame's own tile, light swings this far either way

float FlameWaver(float seconds, float phase)
{
	// Three incommensurate sines: no visible period, never a strobe.
	return 0.5F * std::sin(seconds * 7.3F + phase) + 0.3F * std::sin(seconds * 13.1F + phase * 2.1F) + 0.2F * std::sin(seconds * 23.7F + phase * 3.7F);
}

} // namespace

void RebuildLightColourField()
{
	RefreshLevelBrightness();
	GatherSources();
	BuildShadows(); // ray-traced shadows (user, 2026-10-10): the same lights, stopped by walls
	// Loot beams (user, 2026-10-11): an item worth the turn lights the floor round it in its colour. After the shadows -
	// the engine has no such light, and the shadows are the engine's lights stopped by walls.
	if (*sgOptions.Oracool.lootBeams) {
		for (uint8_t i = 0; i < ActiveItemCount; i++) {
			const Item &item = Items[ActiveItems[i]];
			const LootLook look = LootLookFor(item);
			if (look.rgb != 0 && LootShows(item))
				Sources.push_back({ static_cast<float>(item.position.x), static_cast<float>(item.position.y), 3.0F, look.rgb, 0.7F * look.strength, false });
		}
	}

	std::fill(Accumulated.begin(), Accumulated.end(), Accum {});
	std::array<float, FieldW * FieldH> &dirX = ReliefSumX;
	std::array<float, FieldW * FieldH> &dirY = ReliefSumY;
	dirX.fill(0);
	dirY.fill(0);
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
				{
					// The relief's light direction: from the tile toward the light, on screen (one tile +x is (+32, +16),
					// +y is (-32, +16)), and a little up - the lights hang above the floor.
					const float vx = (-dx + dy) * 32.0F;
					const float vy = (-dx - dy) * 16.0F;
					const float len = std::sqrt(vx * vx + vy * vy);
					const size_t i = static_cast<size_t>(x) * FieldH + y;
					// No pull from a light within a few tiles (audit round 1): a sprite's pixels lie over the tiles
					// behind it, so the hero's own light pulled down the screen through his body and lit him from
					// below. The pull grows from nothing at the light to full at ReliefNear tiles.
					const float dist = std::sqrt(dx * dx + dy * dy);
					const float pull = dist >= ReliefNear ? 1.0F : dist / ReliefNear;
					if (len > 4.0F) {
						dirX[i] += w * pull * vx / len;
						dirY[i] += w * pull * vy / len;
					}
					dirY[i] -= ReliefUpBias * w * pull;
				}
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

	for (size_t i = 0; i < ReliefDirX.size(); i++) {
		float vx = dirX[i];
		float vy = dirY[i];
		const float len = std::sqrt(vx * vx + vy * vy);
		if (len > 1.0F) {
			vx /= len;
			vy /= len;
		}
		ReliefDirX[i] = static_cast<int16_t>(vx * 256.0F);
		ReliefDirY[i] = static_cast<int16_t>(vy * 256.0F);
	}
	ReliefBuilt = true;

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

bool FloorPassActive(const Surface &out)
{
	// Wall shadows darken the floor too, whether or not it is smoothed (the Nest and the Crypt included: a darker foot
	// of the wall dims no lava in the middle of a room).
	return SmoothFloorLightActive(out) || (*sgOptions.Oracool.ambientOcclusion && !out.isIndexed() && InLitDungeon());
}

bool ColouredLightActive(const Surface &out)
{
	return (*sgOptions.Oracool.colouredLighting || *sgOptions.Oracool.torchFlicker) && !out.isIndexed() && InLitDungeon();
}

void ApplySmoothFloorLight(const Surface &out, Point firstTile, Point firstTileBuffer)
{
	if (!FloorPassActive(out) || out.w() <= 0 || out.h() <= 0)
		return;
	if (LevelBrightness[0] != One)
		RefreshLevelBrightness();
	const bool smooth = SmoothFloorLightActive(out);
	const bool shadows = smooth && ShadowsBuilt && *sgOptions.Oracool.rayTracedShadows;
	const bool ao = *sgOptions.Oracool.ambientOcclusion;
	if (ao)
		EnsureAoPlane();

	// Everything below is written out by hand - no helpers, no std::clamp, no lambdas, no std::array operator[] in the
	// loops. The pragma above optimises this file even in the Debug build, but /Ob0 still forbids inlining, so every
	// call would stay a call: about 25 million a frame at 1080p before round 2 of the audit (v1.12.421).
	//
	// The brightness plane: the floor drawn bright is darkened to its light (smooth floors) - by the darker of dLight and
	// the ray-traced shadows, so a shadow only ever takes light away. Without smooth floors the floor was drawn at its
	// own per-tile light, and only the wall shadows apply.
	uint16_t *plane = BrightPlane.data();
	const uint16_t *levels = LevelBrightness.data();
	const uint8_t *light = &dLight[0][0];
	const uint8_t *shadow = ShadowDarkening.data();
	const uint8_t *open = OpenLevel.data();
	for (int i = 0; i < FieldW * FieldH; i++) {
		if (!smooth) {
			plane[i] = One;
			continue;
		}
		int level = light[i];
		// A tile dLight lights clearly brighter than every source here would, unblocked, leave it is lit by a light that
		// is no source - the Caves' lava, baked by PlaceCaveLights into dPreLight - and keeps that light (visual audit
		// round 2: lava shores behind a wall from the hero went dark as he came near).
		if (shadows && level + 1 >= open[i])
			level += shadow[i];
		plane[i] = levels[level > LightsMax ? LightsMax : level];
	}
	const uint16_t *aoPlane = AoPlane.data();

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
			int f1 = (top * (256 - fv) + bottom * fv) >> 16;
			if (ao) {
				// The same sample on the half-tile wall-shadow grid: its cell centres sit at u - 0.25 and u + 0.25, so the
				// grid coordinate is 2u + 0.5.
				const int64_t au = (u << 1) + 32768;
				const int64_t av = (v << 1) + 32768;
				const int64_t ia = au >> 16;
				const int64_t ja = av >> 16;
				const int fa = static_cast<int>((au >> 8) & 0xFF);
				const int fb = static_cast<int>((av >> 8) & 0xFF);
				const int a0 = ia < 0 ? 0 : (ia > AoW - 1 ? AoW - 1 : static_cast<int>(ia));
				const int a1 = ia + 1 < 0 ? 0 : (ia + 1 > AoW - 1 ? AoW - 1 : static_cast<int>(ia + 1));
				const int b0 = ja < 0 ? 0 : (ja > AoH - 1 ? AoH - 1 : static_cast<int>(ja));
				const int b1 = ja + 1 < 0 ? 0 : (ja + 1 > AoH - 1 ? AoH - 1 : static_cast<int>(ja + 1));
				const int aoTop = aoPlane[a0 * AoH + b0] * (256 - fa) + aoPlane[a1 * AoH + b0] * fa;
				const int aoBottom = aoPlane[a0 * AoH + b1] * (256 - fa) + aoPlane[a1 * AoH + b1] * fa;
				const int occ = (aoTop * (256 - fb) + aoBottom * fb) >> 16;
				f1 = (f1 * occ) >> 8;
			}
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
	if (!ColouredLightActive(out) || out.w() <= 0 || out.h() <= 0)
		return;
	const bool colour = *sgOptions.Oracool.colouredLighting;
	const bool flicker = *sgOptions.Oracool.torchFlicker && !Flames.empty();
	if (!colour && !flicker)
		return;

	// This frame's tints: the colour field (or none), and every flame's waver over it. The box grows to the flames.
	int minX = colour ? TintMinX : FieldW, maxX = colour ? TintMaxX : -1, minY = colour ? TintMinY : FieldH, maxY = colour ? TintMaxY : -1;
	const LightTint *tints = Tints.data();
	if (flicker) {
		if (colour)
			FrameTints = Tints;
		else
			FrameTints.fill(LightTint { One, One, One });
		const float seconds = static_cast<float>(SDL_GetTicks() % 3600000U) / 1000.0F;
		FlickerMul.fill(1.0F);
		for (const Flame &flame : Flames) {
			const float waver = FlameWaver(seconds, flame.phase) * FlickerDepth;
			const int fx0 = ClampX(static_cast<int64_t>(std::floor(flame.x - flame.reach)));
			const int fx1 = ClampX(static_cast<int64_t>(std::ceil(flame.x + flame.reach)));
			const int fy0 = ClampY(static_cast<int64_t>(std::floor(flame.y - flame.reach)));
			const int fy1 = ClampY(static_cast<int64_t>(std::ceil(flame.y + flame.reach)));
			for (int x = fx0; x <= fx1; x++) {
				for (int y = fy0; y <= fy1; y++) {
					const float dx = static_cast<float>(x) - flame.x;
					const float dy = static_cast<float>(y) - flame.y;
					const float weight = 1.0F - std::sqrt(dx * dx + dy * dy) / flame.reach;
					if (weight <= 0)
						continue;
					// Combined per tile and capped (visual audit round 2): three overlapping flames multiplied
					// flame by flame dipped a spot by up to 40%.
					float &mul = FlickerMul[x * FieldH + y];
					mul = std::clamp(mul * (1.0F + waver * weight), 1.0F - FlickerDepth, 1.0F + FlickerDepth);
				}
			}
			minX = std::min(minX, fx0);
			maxX = std::max(maxX, fx1);
			minY = std::min(minY, fy0);
			maxY = std::max(maxY, fy1);
		}
		for (int x = std::max(minX, 0); x <= maxX && x < FieldW; x++) {
			for (int y = std::max(minY, 0); y <= maxY && y < FieldH; y++) {
				const float k = FlickerMul[x * FieldH + y];
				if (k == 1.0F)
					continue;
				LightTint &t = FrameTints[x * FieldH + y];
				t.r = static_cast<uint16_t>(std::min(static_cast<float>(t.r) * k, 400.0F));
				t.g = static_cast<uint16_t>(std::min(static_cast<float>(t.g) * k, 400.0F));
				t.b = static_cast<uint16_t>(std::min(static_cast<float>(t.b) * k, 400.0F));
			}
		}
		tints = FrameTints.data();
	}
	if (minX > maxX || minY > maxY)
		return;
	const GroundMap m = MakeGroundMap(firstTile, firstTileBuffer);

	// Only the screen rectangle the coloured tiles project to: the corners of their tile box, one tile out for the
	// bilinear's reach. (A pixel is tinted by the floor tile under it, so a wall takes the colour of the tiles behind it.)
	int left = out.w(), right = -1, top = out.h(), bottom = -1;
	for (const Point corner : { Point { minX - 1, minY - 1 }, Point { maxX + 1, minY - 1 }, Point { minX - 1, maxY + 1 }, Point { maxX + 1, maxY + 1 } }) {
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

	TintRect(out, m, tints, left, right, top, bottom);
}

bool SurfaceReliefActive(const Surface &out)
{
	return *sgOptions.Oracool.surfaceRelief && !out.isIndexed() && InLitDungeon() && ReliefBuilt;
}

namespace {
std::vector<uint32_t> ReliefKept;
int ReliefKeptW = 0;
int ReliefKeptH = 0;
std::vector<int16_t> ReliefLuma; // three rows of brightness; -1 where the pixel is floor (unchanged since the keep)

/**
 * One row's brightness, -1 where the pixel is as it was kept - or is one of the hover outlines' colours (audit round 1:
 * an outline is a line, not a surface, and took a one-sided shift beside the sprite).
 */
void ReliefLumaRow(const uint32_t *row, const uint32_t *kept, int16_t *dst, int w)
{
	const uint32_t o1 = PaletteRGB[165] & 0xFFFFFF, o2 = PaletteRGB[233] & 0xFFFFFF, o3 = PaletteRGB[181] & 0xFFFFFF, o4 = PaletteRGB[129] & 0xFFFFFF;
	for (int x = 0; x < w; x++) {
		const uint32_t c = row[x];
		const uint32_t rgb = c & 0xFFFFFF;
		dst[x] = c == kept[x] || rgb == o1 || rgb == o2 || rgb == o3 || rgb == o4 ? static_cast<int16_t>(-1) : static_cast<int16_t>(((((c >> 16) & 0xFF) * 77) + (((c >> 8) & 0xFF) * 150) + ((c & 0xFF) * 29)) >> 8);
	}
}
} // namespace

void BeginSurfaceRelief(const Surface &out)
{
	ReliefKeptW = ReliefKeptH = 0;
	if (!SurfaceReliefActive(out) || out.w() <= 0 || out.h() <= 0)
		return;
	ReliefKeptW = out.w();
	ReliefKeptH = out.h();
	ReliefKept.resize(static_cast<size_t>(ReliefKeptW) * ReliefKeptH);
	for (int y = 0; y < ReliefKeptH; y++)
		std::copy_n(out.at<uint32_t>(0, y), ReliefKeptW, ReliefKept.data() + static_cast<size_t>(y) * ReliefKeptW);
}

void ApplySurfaceRelief(const Surface &out, Point firstTile, Point firstTileBuffer)
{
	if (!SurfaceReliefActive(out) || ReliefKeptW != out.w() || ReliefKeptH != out.h() || ReliefKeptW <= 2 || ReliefKeptH <= 2)
		return;
	const int w = ReliefKeptW;
	const int h = ReliefKeptH;
	ReliefLuma.resize(static_cast<size_t>(w) * 3);
	int16_t *rows[3] = { ReliefLuma.data(), ReliefLuma.data() + w, ReliefLuma.data() + 2 * w };
	const uint32_t *kept = ReliefKept.data();
	const int16_t *dirX = ReliefDirX.data();
	const int16_t *dirY = ReliefDirY.data();
	const GroundMap m = MakeGroundMap(firstTile, firstTileBuffer);

	// The surface is what DrawTileContent drew - every pixel changed since the keep - and its height is its brightness:
	// the light direction under it (bilinear between tile centres, joined linearly every Span pixels) against the
	// brightness slope gives how much it faces the light, n.l of the normal (-dL/dx, -dL/dy, 1). The inner loop is
	// written out by hand (no calls survive /Ob0); the row helper runs once a row.
	ReliefLumaRow(out.at<uint32_t>(0, 0), kept, rows[1], w);
	ReliefLumaRow(out.at<uint32_t>(0, 1), kept + w, rows[2], w);
	std::copy_n(rows[1], w, rows[0]); // the row above the first is the first itself
	for (int py = 0; py < h; py++) {
		const int16_t *up = rows[0];
		const int16_t *mid = rows[1];
		const int16_t *down = rows[2];
		uint32_t *row = out.at<uint32_t>(0, py);
		const int64_t rowU = m.u0 + m.duY * py;
		const int64_t rowV = m.v0 + m.dvY * py;
		int dx0 = 0, dy0 = 0;
		for (int sx = 0;; sx += Span) {
			const int x = sx < w ? sx : w;
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
			const int wa = (256 - fu) * (256 - fv), wb = fu * (256 - fv), wc = (256 - fu) * fv, wd = fu * fv;
			const int dx1 = (dirX[x0 * FieldH + y0] * wa + dirX[x1 * FieldH + y0] * wb + dirX[x0 * FieldH + y1] * wc + dirX[x1 * FieldH + y1] * wd) / 65536;
			const int dy1 = (dirY[x0 * FieldH + y0] * wa + dirY[x1 * FieldH + y0] * wb + dirY[x0 * FieldH + y1] * wc + dirY[x1 * FieldH + y1] * wd) / 65536;
			if (sx > 0) {
				const int from = sx - Span;
				const int len = x - from;
				if (len > 0 && (dx0 != 0 || dy0 != 0 || dx1 != 0 || dy1 != 0)) {
					int fx = dx0 << 8, fy = dy0 << 8;
					const int sdx = ((dx1 - dx0) << 8) / len, sdy = ((dy1 - dy0) << 8) / len;
					for (int i = from; i < x; i++, fx += sdx, fy += sdy) {
						const int l = mid[i];
						if (l < 0)
							continue;
						// A neighbour that is floor is no slope: it stands at this pixel's own height.
						int left = i > 0 ? mid[i - 1] : l;
						int right = i + 1 < w ? mid[i + 1] : l;
						int above = up[i];
						int below = down[i];
						left = left < 0 ? l : left;
						right = right < 0 ? l : right;
						above = above < 0 ? l : above;
						below = below < 0 ? l : below;
						const int gx = right - left;
						const int gy = below - above;
						// A quarter of the slope (audit round 1): a mortar line of 56 levels hit the cap at once - an
						// emboss filter, not shading. Now a step of 160 does.
						int delta = -((gx * (fx >> 8) + gy * (fy >> 8)) >> 10);
						delta = delta > ReliefMax ? ReliefMax : (delta < -ReliefMax ? -ReliefMax : delta);
						if (delta == 0)
							continue;
						const uint32_t c = row[i];
						const uint32_t f = static_cast<uint32_t>(256 + delta);
						uint32_t r = (((c >> 16) & 0xFF) * f) >> 8;
						uint32_t g = (((c >> 8) & 0xFF) * f) >> 8;
						uint32_t b = ((c & 0xFF) * f) >> 8;
						r = r > 255 ? 255 : r;
						g = g > 255 ? 255 : g;
						b = b > 255 ? 255 : b;
						row[i] = (r << 16) | (g << 8) | b;
					}
				}
			}
			dx0 = dx1, dy0 = dy1;
			if (x >= w)
				break;
		}
		// Roll the rows: the next one's brightness from the pixels as drawn (the rows below are not touched yet).
		int16_t *oldUp = rows[0];
		rows[0] = rows[1];
		rows[1] = rows[2];
		rows[2] = oldUp;
		const int next = py + 2;
		if (next < h)
			ReliefLumaRow(out.at<uint32_t>(0, next), kept + static_cast<size_t>(next) * w, rows[2], w);
		else
			std::copy_n(rows[1], w, rows[2]);
	}
}

void ApplyTownDayNight(const Surface &out, Point firstTile, Point firstTileBuffer)
{
	AdvanceTownClock();
	if (!*sgOptions.Oracool.dayNightCycle || out.isIndexed() || leveltype != DTYPE_TOWN || out.w() <= 0 || out.h() <= 0)
		return;
	float ar, ag, ab;
	AmbientAt(TownClock, ar, ag, ab);
	if (ar > 0.995F && ag > 0.995F && ab > 0.995F)
		return; // broad day: nothing to do
	TownLitR.fill(0);
	TownLitG.fill(0);
	TownLitB.fill(0);
	for (const TownLight &light : GatherTownLights()) {
		const int x0 = ClampX(static_cast<int64_t>(std::floor(light.x - light.reach)));
		const int x1 = ClampX(static_cast<int64_t>(std::ceil(light.x + light.reach)));
		const int y0 = ClampY(static_cast<int64_t>(std::floor(light.y - light.reach)));
		const int y1 = ClampY(static_cast<int64_t>(std::ceil(light.y + light.reach)));
		for (int x = x0; x <= x1; x++) {
			for (int y = y0; y <= y1; y++) {
				const float dx = static_cast<float>(x) - light.x;
				const float dy = static_cast<float>(y) - light.y;
				const float w = 1.0F - std::sqrt(dx * dx + dy * dy) / light.reach;
				if (w <= 0)
					continue;
				const float t = w * w * (3.0F - 2.0F * w) * light.strength;
				const size_t i = static_cast<size_t>(x) * FieldH + y;
				TownLitR[i] += t * light.r;
				TownLitG[i] += t * light.g;
				TownLitB[i] += t * light.b;
			}
		}
	}
	for (size_t i = 0; i < TownTints.size(); i++) {
		const float lr = std::min(TownLitR[i], 1.0F);
		const float lg = std::min(TownLitG[i], 1.0F);
		const float lb = std::min(TownLitB[i], 1.0F);
		TownTints[i] = {
			static_cast<uint16_t>((ar + (1.0F - ar) * lr) * One + 0.5F),
			static_cast<uint16_t>((ag + (1.0F - ag) * lg) * One + 0.5F),
			static_cast<uint16_t>((ab + (1.0F - ab) * lb) * One + 0.5F),
		};
	}
	TintRect(out, MakeGroundMap(firstTile, firstTileBuffer), TownTints.data(), 0, out.w() - 1, 0, out.h() - 1);
}

float TownHour()
{
	return TownClock;
}

void SetTownHour(float hour)
{
	hour = std::fmod(hour, 24.0F);
	TownClock = hour < 0 ? hour + 24.0F : hour;
}

LightTint TownAmbientAt(float hour)
{
	float r, g, b;
	AmbientAt(hour, r, g, b);
	return { static_cast<uint16_t>(r * One + 0.5F), static_cast<uint16_t>(g * One + 0.5F), static_cast<uint16_t>(b * One + 0.5F) };
}

LightTint TownTintAt(Point tile)
{
	return TownTints[ClampX(tile.x) * FieldH + ClampY(tile.y)];
}

void ReliefDirectionAt(Point tile, int &dx, int &dy)
{
	const size_t i = static_cast<size_t>(ClampX(tile.x)) * FieldH + ClampY(tile.y);
	dx = ReliefDirX[i];
	dy = ReliefDirY[i];
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
