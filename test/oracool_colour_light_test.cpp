/**
 * Coloured and smooth lighting (oracool/colour_light.h, user 2026-10-10).
 *
 * Everything here runs on a synthetic palette and light tables (a grey ramp darkened linearly), so no game archive is
 * needed: the passes are pure arithmetic over dLight, the live lights and a 32-bit surface.
 */
#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>

#include "engine/palette.h"
#include "engine/surface.hpp"
#include "levels/gendung.h"
#include "lighting.h"
#include "missiles.h"
#include "monster.h"
#include "objects.h"
#include "options.h"
#include "oracool/colour_light.h"
#include "oracool/missile_tint.h"
#include "utils/png.h"

using namespace devilution;

namespace {

/** A grey palette and light tables that darken it linearly: level L keeps (15 - L) / 15 of every entry. */
void SyntheticPaletteAndLight()
{
	for (int i = 0; i < 256; i++)
		PaletteRGB[i] = (static_cast<uint32_t>(i) << 16) | (static_cast<uint32_t>(i) << 8) | static_cast<uint32_t>(i);
	for (size_t level = 0; level < NumLightingLevels; level++)
		for (int i = 0; i < 256; i++)
			LightTables[level][i] = static_cast<uint8_t>(i * (15 - static_cast<int>(level)) / 15);
}

/** Saves and restores everything a test here touches. */
struct LightScene {
	dungeon_type savedType = leveltype;
	bool savedSmooth = *sgOptions.Oracool.smoothLighting;
	bool savedColour = *sgOptions.Oracool.colouredLighting;
	size_t savedMonsters = ActiveMonsterCount;
	int savedObjects = ActiveObjectCount;

	LightScene()
	{
		leveltype = DTYPE_CATHEDRAL;
		sgOptions.Oracool.smoothLighting.SetValue(true);
		sgOptions.Oracool.colouredLighting.SetValue(true);
		ActiveMonsterCount = 0;
		ActiveObjectCount = 0;
		Missiles.clear();
		InitLighting();
		std::memset(dPiece, 0, sizeof(dPiece));
		std::memset(dLight, LightsMax, sizeof(dLight));
		SyntheticPaletteAndLight();
	}
	~LightScene()
	{
		Missiles.clear();
		InitLighting();
		ActiveMonsterCount = savedMonsters;
		ActiveObjectCount = savedObjects;
		sgOptions.Oracool.smoothLighting.SetValue(savedSmooth);
		sgOptions.Oracool.colouredLighting.SetValue(savedColour);
		leveltype = savedType;
		oracool::RebuildLightColourField();
	}
};

Missile &AddLitMissile(MissileID type, MissileGraphicID look, Point at, uint8_t radius)
{
	Missile &missile = Missiles.emplace_back(Missile {});
	missile._mitype = type;
	missile._miAnimType = look;
	missile._mlid = AddLight(at, radius);
	return missile;
}

uint32_t Grey(uint8_t v)
{
	return (static_cast<uint32_t>(v) << 16) | (static_cast<uint32_t>(v) << 8) | v;
}

/** The tile (x0, y0) drawn with its 64x32 box's bottom-left at (bx, by): where tile (x, y)'s centre lands. */
Point TileCentre(Point firstTile, Point firstBuffer, Point tile)
{
	const int dx = tile.x - firstTile.x;
	const int dy = tile.y - firstTile.y;
	return { firstBuffer.x + 32 + dx * 32 - dy * 32, firstBuffer.y - 16 + dx * 16 + dy * 16 };
}

} // namespace

TEST(OracoolColourLight, TheLevelsRunFromFullBrightToBlack)
{
	LightScene scene;
	oracool::RebuildLightColourField();
	EXPECT_EQ(oracool::SmoothLevelBrightness(0), 256);
	EXPECT_EQ(oracool::SmoothLevelBrightness(15), 0);
	for (uint8_t level = 1; level < 15; level++) {
		EXPECT_LT(oracool::SmoothLevelBrightness(level), oracool::SmoothLevelBrightness(level - 1)) << int(level);
		// The synthetic tables keep (15 - L) / 15 of every grey: the measured brightness says the same, within a step.
		EXPECT_NEAR(oracool::SmoothLevelBrightness(level), 256 * (15 - level) / 15, 4) << int(level);
	}
}

TEST(OracoolColourLight, AFireLightIsOrangeAndTheHerosOwnStaysWhite)
{
	LightScene scene;
	const Missile &fire = AddLitMissile(MissileID::Firebolt, MissileGraphicID::Fireball, { 40, 40 }, 3);
	const int heroLight = AddLight({ 70, 70 }, 10); // nobody owns it: white, like the hero's
	oracool::RebuildLightColourField();

	EXPECT_EQ(oracool::LightColourForId(fire._mlid), oracool::hue::FireOrange);
	EXPECT_EQ(oracool::LightColourForId(heroLight), 0U);

	const oracool::LightTint atFire = oracool::LightTintAt({ 40, 40 });
	EXPECT_GT(atFire.r, 256);
	EXPECT_LT(atFire.b, 256);
	EXPECT_GT(atFire.r, atFire.g);
	EXPECT_GT(atFire.g, atFire.b);

	const oracool::LightTint atHero = oracool::LightTintAt({ 70, 70 });
	EXPECT_EQ(atHero.r, 256);
	EXPECT_EQ(atHero.g, 256);
	EXPECT_EQ(atHero.b, 256);

	// Out of the fire's reach (radius 3 = gone at 4 tiles): untouched.
	const oracool::LightTint beyond = oracool::LightTintAt({ 45, 40 });
	EXPECT_EQ(beyond.r, 256);
}

TEST(OracoolColourLight, TheHerosWhiteLightDilutesAColourSharingItsTile)
{
	LightScene scene;
	AddLitMissile(MissileID::LightningControl, MissileGraphicID::Lightning, { 40, 40 }, 3);
	oracool::RebuildLightColourField();
	const oracool::LightTint alone = oracool::LightTintAt({ 40, 40 });

	AddLight({ 40, 40 }, 10);
	oracool::RebuildLightColourField();
	const oracool::LightTint shared = oracool::LightTintAt({ 40, 40 });

	EXPECT_GT(alone.b, 256); // lightning blue
	EXPECT_LT(shared.b, alone.b);
	EXPECT_GT(shared.b, 256);
}

TEST(OracoolColourLight, ASmoothFloorMatchesEachTileAtItsCentreAndBlendsBetween)
{
	LightScene scene;
	oracool::RebuildLightColourField();
	OwnedSurface out = OwnedSurface::Rgb(320, 200);
	const Point firstTile { 40, 40 };
	const Point firstBuffer { 96, 100 };
	for (int y = 0; y < out.h(); y++)
		for (int x = 0; x < out.w(); x++)
			*out.at<uint32_t>(x, y) = Grey(200);
	std::memset(dLight, 8, sizeof(dLight));
	dLight[40][40] = 0;

	oracool::ApplySmoothFloorLight(out, firstTile, firstBuffer);

	const auto value = [&](Point p) { return static_cast<int>(*out.at<uint32_t>(p.x, p.y) & 0xFF); };
	const int lit = value(TileCentre(firstTile, firstBuffer, { 40, 40 }));
	const int dim = value(TileCentre(firstTile, firstBuffer, { 41, 40 }));
	const Point a = TileCentre(firstTile, firstBuffer, { 40, 40 });
	const Point b = TileCentre(firstTile, firstBuffer, { 41, 40 });
	const int between = value({ (a.x + b.x) / 2, (a.y + b.y) / 2 });
	EXPECT_NEAR(lit, 200, 6);
	EXPECT_NEAR(dim, 200 * oracool::SmoothLevelBrightness(8) / 256, 6);
	EXPECT_GT(between, dim + 10);
	EXPECT_LT(between, lit - 10);
}

TEST(OracoolColourLight, TownAndIndexedSurfacesAreLeftAlone)
{
	LightScene scene;
	OwnedSurface rgb = OwnedSurface::Rgb(8, 8);
	OwnedSurface indexed(8, 8);
	EXPECT_TRUE(oracool::SmoothFloorLightActive(rgb));
	EXPECT_FALSE(oracool::SmoothFloorLightActive(indexed));
	EXPECT_FALSE(oracool::ColouredLightActive(indexed));
	leveltype = DTYPE_TOWN;
	EXPECT_FALSE(oracool::SmoothFloorLightActive(rgb));
	EXPECT_FALSE(oracool::ColouredLightActive(rgb));
	leveltype = DTYPE_NEST; // lava is full bright at every level: smooth floors would dim it
	EXPECT_FALSE(oracool::SmoothFloorLightActive(rgb));
	EXPECT_TRUE(oracool::ColouredLightActive(rgb));
}

/**
 * Preview: a floor of real-looking stripes lit three ways - the per-tile steps of today, the smooth floor, and the smooth
 * floor with a fire, a lightning and a poison light. Run with ORCL_FX_OUT set:
 *   oracool_colour_light_test --gtest_also_run_disabled_tests --gtest_filter=*DISABLED_ExportLightingPreview
 */
TEST(OracoolColourLight, DISABLED_ExportLightingPreview)
{
	const char *outDir = std::getenv("ORCL_FX_OUT");
	ASSERT_NE(outDir, nullptr) << "set ORCL_FX_OUT";
	InitPNG();
	LightScene scene;
	const Point hero { 50, 50 };
	const int heroLight = AddLight(hero, 7);
	AddLitMissile(MissileID::Firebolt, MissileGraphicID::Fireball, { 46, 52 }, 4);
	AddLitMissile(MissileID::LightningControl, MissileGraphicID::Lightning, { 54, 47 }, 4);
	AddLitMissile(MissileID::Acid, MissileGraphicID::Acid, { 52, 54 }, 3);
	AddLitMissile(MissileID::Firebolt, MissileGraphicID::Fireball, { 58, 56 }, 3); // a lone fire, out in the dark
	// dLight as ProcessLightList would leave it - MakeLightTable's linear falloff, 15 levels over radius + 1 tiles, the
	// brightest light kept. (DoLighting itself needs MakeLightTable's falloff tables, which need the game archives.)
	std::memset(dLight, LightsMax, sizeof(dLight));
	for (int i = 0; i < ActiveLightCount; i++) {
		const Light &light = Lights[ActiveLights[i]];
		const int reach = light.radius + 1;
		for (int y = light.position.tile.y - reach; y <= light.position.tile.y + reach; y++) {
			for (int x = light.position.tile.x - reach; x <= light.position.tile.x + reach; x++) {
				const double d = std::hypot(x - light.position.tile.x, y - light.position.tile.y);
				const int level = d > reach ? LightsMax : static_cast<int>(d / reach * LightsMax + 0.5);
				dLight[x][y] = static_cast<uint8_t>(std::min<int>(dLight[x][y], level));
			}
		}
	}
	oracool::RebuildLightColourField();
	(void)heroLight;

	constexpr int W = 960, H = 540;
	const Point firstTile { 36, 36 };
	const Point firstBuffer { 448, -180 }; // puts the hero's tile (50, 50) at the middle of the frame
	const auto paint = [&](OwnedSurface &s, bool stepped) {
		for (int y = 0; y < H; y++) {
			for (int x = 0; x < W; x++) {
				// A flagstone texture: a base grey with a light grid of mortar lines in tile space.
				const double rx = x + 0.5 - (firstBuffer.x + 32.0), ry = y + 0.5 - (firstBuffer.y - 16.0);
				const double u = firstTile.x + rx / 64.0 + ry / 32.0, v = firstTile.y + ry / 32.0 - rx / 64.0;
				const double fu = u - std::floor(u + 0.5), fv = v - std::floor(v + 0.5);
				const bool mortar = std::abs(fu) > 0.46 || std::abs(fv) > 0.46;
				uint8_t g = mortar ? 70 : static_cast<uint8_t>(150 + ((static_cast<int>(std::floor(u + 0.5)) * 7 + static_cast<int>(std::floor(v + 0.5)) * 13) % 5) * 8);
				if (stepped) {
					const int tx = std::clamp(static_cast<int>(std::floor(u + 0.5)), 0, MAXDUNX - 1);
					const int ty = std::clamp(static_cast<int>(std::floor(v + 0.5)), 0, MAXDUNY - 1);
					g = LightTables[dLight[tx][ty]][g];
				}
				*s.at<uint32_t>(x, y) = PaletteRGB[g];
			}
		}
	};
	OwnedSurface stepped = OwnedSurface::Rgb(W, H);
	paint(stepped, true);
	OwnedSurface smooth = OwnedSurface::Rgb(W, H);
	paint(smooth, false);
	oracool::ApplySmoothFloorLight(smooth, firstTile, firstBuffer);
	OwnedSurface coloured = OwnedSurface::Rgb(W, H);
	paint(coloured, false);
	oracool::ApplySmoothFloorLight(coloured, firstTile, firstBuffer);
	oracool::ApplyLightColour(coloured, firstTile, firstBuffer);

	const std::filesystem::path dir(outDir);
	ASSERT_EQ(IMG_SavePNG(stepped.surface, (dir / "light_1_per_tile_today.png").string().c_str()), 0);
	ASSERT_EQ(IMG_SavePNG(smooth.surface, (dir / "light_2_smooth.png").string().c_str()), 0);
	ASSERT_EQ(IMG_SavePNG(coloured.surface, (dir / "light_3_smooth_coloured.png").string().c_str()), 0);
}
