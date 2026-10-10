/**
 * Glowing Spells and Particles (oracool/spell_glow.h, oracool/particles.h, user 2026-10-10).
 *
 * No game archive is needed: the additive kernel draws a tiny CLX built from an 8-bit surface (SurfaceToClx) through a
 * hand-made colour table, the glow rule is asked about bare Missile structs, and the particle pool is pure arithmetic.
 */
#include <gtest/gtest.h>

#include <cstdint>

#include "engine/render/clx_render.hpp"
#include "engine/surface.hpp"
#include "levels/gendung.h"
#include "missiles.h"
#include "options.h"
#include "oracool/particles.h"
#include "oracool/spell_glow.h"
#include "utils/surface_to_clx.hpp"

using namespace devilution;

namespace {

constexpr uint8_t Key = 255; // the source surface's transparent index

/** A 3x1 sprite: index 10, a transparent pixel, index 20. */
OwnedClxSpriteList ThreePixelSprite()
{
	OwnedSurface source(3, 1);
	uint8_t *row = &source[Point { 0, 0 }];
	row[0] = 10;
	row[1] = Key;
	row[2] = 20;
	return SurfaceToClx(source, 1, Key);
}

struct OptionScene {
	bool savedGlow = *sgOptions.Oracool.glowingSpells;
	bool savedParticles = *sgOptions.Oracool.particles;
	dungeon_type savedType = leveltype;

	OptionScene()
	{
		sgOptions.Oracool.glowingSpells.SetValue(true);
		sgOptions.Oracool.particles.SetValue(true);
		leveltype = DTYPE_CATHEDRAL;
		oracool::ForgetParticles();
	}
	~OptionScene()
	{
		oracool::ForgetParticles();
		sgOptions.Oracool.glowingSpells.SetValue(savedGlow);
		sgOptions.Oracool.particles.SetValue(savedParticles);
		leveltype = savedType;
	}
};

} // namespace

TEST(OracoolSpellGlow, AdditiveKernelAddsAndClamps)
{
	OwnedClxSpriteList sprite = ThreePixelSprite();
	uint32_t table[256] = {};
	table[10] = 0x804020;
	table[20] = 0xFFFFFF;

	OwnedSurface out = OwnedSurface::Rgb(3, 1);
	const auto reset = [&]() {
		*out.at<uint32_t>(0, 0) = 0x303030;
		*out.at<uint32_t>(1, 0) = 0x112233;
		*out.at<uint32_t>(2, 0) = 0xF0F0F0;
	};

	reset();
	ClxDrawRgbMapAdditive(out, { 0, 0 }, ClxSpriteList(sprite)[0], table, 255);
	EXPECT_EQ(*out.at<uint32_t>(0, 0), 0xB07050u); // 0x30 + (0x80, 0x40, 0x20)
	EXPECT_EQ(*out.at<uint32_t>(1, 0), 0x112233u); // transparent: untouched
	EXPECT_EQ(*out.at<uint32_t>(2, 0), 0xFFFFFFu); // clamped per channel

	reset();
	ClxDrawRgbMapAdditive(out, { 0, 0 }, ClxSpriteList(sprite)[0], table, 128);
	EXPECT_EQ(*out.at<uint32_t>(0, 0), 0x705040u); // half: 0x30 + (0x40, 0x20, 0x10)

	reset();
	ClxDrawRgbMapAdditive(out, { 0, 0 }, ClxSpriteList(sprite)[0], table, 0);
	EXPECT_EQ(*out.at<uint32_t>(0, 0), 0x303030u); // nothing added

	// Over black it is the sprite as drawn.
	*out.at<uint32_t>(0, 0) = 0;
	ClxDrawRgbMapAdditive(out, { 0, 0 }, ClxSpriteList(sprite)[0], table, 255);
	EXPECT_EQ(*out.at<uint32_t>(0, 0), 0x804020u);
}

TEST(OracoolSpellGlow, AdditiveKernelOnAnIndexedSurfaceDrawsPlain)
{
	OwnedClxSpriteList sprite = ThreePixelSprite();
	uint32_t table[256] = {};
	OwnedSurface out(3, 1);
	uint8_t *row = &out[Point { 0, 0 }];
	row[0] = row[1] = row[2] = 7;
	ClxDrawRgbMapAdditive(out, { 0, 0 }, ClxSpriteList(sprite)[0], table, 220);
	EXPECT_EQ(row[0], 10);
	EXPECT_EQ(row[1], 7);
	EXPECT_EQ(row[2], 20);
}

TEST(OracoolSpellGlow, FireBoltGlowsArrowDoesNot)
{
	OptionScene scene;
	Missile arrow {};
	arrow._mitype = MissileID::Arrow;
	arrow._miAnimType = MissileGraphicID::Arrow;
	Missile fire {};
	fire._mitype = MissileID::Firebolt;
	fire._miAnimType = MissileGraphicID::Fireball;

	EXPECT_FALSE(oracool::MissileGlows(arrow));
	EXPECT_TRUE(oracool::MissileGlows(fire));

	// A borrowed item sprite never glows, whatever its graphic.
	Missile tumble = fire;
	static const uint8_t trn[256] = {};
	tumble.oracoolTrn = trn;
	EXPECT_FALSE(oracool::MissileGlows(tumble));
	// Nor a charging monster's body (no graphic of its own).
	Missile charger {};
	charger._mitype = MissileID::Firebolt;
	charger._miAnimType = MissileGraphicID::None;
	EXPECT_FALSE(oracool::MissileGlows(charger));

	// Off with the option, and in town.
	sgOptions.Oracool.glowingSpells.SetValue(false);
	EXPECT_FALSE(oracool::MissileGlows(fire));
	sgOptions.Oracool.glowingSpells.SetValue(true);
	leveltype = DTYPE_TOWN;
	EXPECT_FALSE(oracool::MissileGlows(fire));
	EXPECT_TRUE(oracool::MissileArtGlows(fire));

	// A fading missile glows at its share.
	fire.oracoolAlpha = 128;
	EXPECT_EQ(oracool::MissileGlowStrength(fire), oracool::SpellGlowStrength / 2);
}

TEST(OracoolParticles, PoolNeverPassesItsCapacityAndForgetEmptiesIt)
{
	OptionScene scene;
	for (int i = 0; i < 40; i++)
		oracool::EmitHitSparks({ 40, 40 }, oracool::SteelSparkRgb, 64);
	EXPECT_EQ(oracool::ParticleCount(), oracool::MaxParticles);
	oracool::EmitHitSparks({ 40, 40 }, oracool::SteelSparkRgb, 64);
	EXPECT_EQ(oracool::ParticleCount(), oracool::MaxParticles);
	oracool::ForgetParticles();
	EXPECT_EQ(oracool::ParticleCount(), 0u);
}

TEST(OracoolParticles, OptionOffEmitsNothingAndEmptiesThePool)
{
	OptionScene scene;
	oracool::EmitHitSparks({ 40, 40 }, oracool::SteelSparkRgb, 10);
	EXPECT_EQ(oracool::ParticleCount(), 10u);
	sgOptions.Oracool.particles.SetValue(false);
	oracool::EmitHitSparks({ 40, 40 }, oracool::SteelSparkRgb, 10);
	EXPECT_EQ(oracool::ParticleCount(), 10u);
	OwnedSurface out = OwnedSurface::Rgb(64, 64);
	oracool::UpdateAndDrawParticles(out, { 0, 0 }, { 0, 0 });
	EXPECT_EQ(oracool::ParticleCount(), 0u);
}

TEST(OracoolParticles, SparksAreDrawnNearTheirTile)
{
	OptionScene scene;
	OwnedSurface out = OwnedSurface::Rgb(256, 160);
	for (int y = 0; y < 160; y++)
		for (int x = 0; x < 256; x++)
			*out.at<uint32_t>(x, y) = 0;
	const Point firstTile { 10, 10 };
	const Point firstTileBuffer { 100, 100 };
	oracool::UpdateAndDrawParticles(out, firstTile, firstTileBuffer); // latches the level: a new level empties the pool
	oracool::EmitHitSparks(firstTile, 0xFF8020, 32);
	ASSERT_EQ(oracool::ParticleCount(), 32u);
	oracool::UpdateAndDrawParticles(out, firstTile, firstTileBuffer);
	// The tile's centre is at (132, 85.5); sparks start 30-46 px above it, within a fifth of a tile, and fly at most one
	// 50 ms step before they are drawn.
	int lit = 0;
	int stray = 0;
	for (int y = 0; y < 160; y++) {
		for (int x = 0; x < 256; x++) {
			if (*out.at<uint32_t>(x, y) == 0)
				continue;
			if (x >= 90 && x <= 174 && y >= 10 && y <= 90)
				lit++;
			else
				stray++;
		}
	}
	EXPECT_GT(lit, 0);
	EXPECT_EQ(stray, 0);
}
