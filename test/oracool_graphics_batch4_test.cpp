/**
 * The fourth graphics batch (user, 2026-10-11: "proceed with all suggested"): oracool/scene_life.h - the ground's
 * wetness, the puddles' scatter, the missile trails' colours, the shockwaves' record and the shake's kick.
 *
 * No game archive is needed: the map is the empty one (every tile open), the missile table is compiled in.
 */
#include <gtest/gtest.h>

#include <cstdint>
#include <cstdlib>

#include "levels/gendung.h"
#include "options.h"
#include "oracool/scene_fx.h"
#include "oracool/scene_life.h"
#include "spelldat.h"

using namespace devilution;

namespace {

int R(uint32_t c)
{
	return static_cast<int>((c >> 16) & 0xFF);
}

int B(uint32_t c)
{
	return static_cast<int>(c & 0xFF);
}

TEST(OracoolGraphicsBatch4, RainSoaksTheGroundAndItDriesSlowlyAfter)
{
	oracool::SetGroundWetness(0);
	oracool::SetWeather(oracool::Weather::Rain);
	oracool::AdvanceGroundWetness(0.25F);
	EXPECT_GT(oracool::GroundWetness(), 0.0F);
	for (int i = 0; i < 200; i++)
		oracool::AdvanceGroundWetness(0.25F); // 50 s of full rain: soaked (40 s to the full)
	EXPECT_FLOAT_EQ(oracool::GroundWetness(), 1.0F);
	oracool::SetWeather(oracool::Weather::Clear);
	for (int i = 0; i < 300; i++)
		oracool::AdvanceGroundWetness(0.25F); // 75 s dry: half way (150 s to dry)
	EXPECT_NEAR(oracool::GroundWetness(), 0.5F, 0.02F);
	oracool::SetGroundWetness(3.0F);
	EXPECT_FLOAT_EQ(oracool::GroundWetness(), 1.0F);
	oracool::SetGroundWetness(0);
}

TEST(OracoolGraphicsBatch4, PuddlesLieOnAboutOneOpenTileInSix)
{
	// Open ground (another test in the binary may have left a level behind).
	SOLData[0] = TileProperties::None;
	for (int x = 18; x < 82; x++)
		for (int y = 18; y < 82; y++)
			dPiece[x][y] = 0;
	int puddles = 0;
	int tiles = 0;
	for (int x = 20; x < 80; x++) {
		for (int y = 20; y < 80; y++) {
			tiles++;
			puddles += oracool::TileHoldsPuddle({ x, y }) ? 1 : 0;
		}
	}
	const double share = static_cast<double>(puddles) / tiles;
	EXPECT_GT(share, 1.0 / 8.0);
	EXPECT_LT(share, 1.0 / 4.5);
	// Never off the map.
	EXPECT_FALSE(oracool::TileHoldsPuddle({ -1, 5 }));
	EXPECT_FALSE(oracool::TileHoldsPuddle({ MAXDUNX, 5 }));
}

TEST(OracoolGraphicsBatch4, MissileTrailsTakeTheirElementsColour)
{
	const uint32_t fire = oracool::MissileTrailColour(static_cast<int>(MissileID::Firebolt));
	ASSERT_NE(fire, 0U);
	EXPECT_GT(R(fire), B(fire) + 100); // orange
	const uint32_t arrow = oracool::MissileTrailColour(static_cast<int>(MissileID::Arrow));
	ASSERT_NE(arrow, 0U);
	EXPECT_LT(std::abs(R(arrow) - B(arrow)), 30); // a pale grey
	// Portals, walls and a charging monster draw no trail.
	EXPECT_EQ(oracool::MissileTrailColour(static_cast<int>(MissileID::TownPortal)), 0U);
	EXPECT_EQ(oracool::MissileTrailColour(static_cast<int>(MissileID::FireWall)), 0U);
	EXPECT_EQ(oracool::MissileTrailColour(static_cast<int>(MissileID::Rhino)), 0U);
}

TEST(OracoolGraphicsBatch4, ShockwavesAreRecordedAndForgotten)
{
	oracool::ForgetSceneLife();
	EXPECT_EQ(oracool::ShockwaveCount(), 0U);
	oracool::AddShockwave({ 40, 40 }, {}, 1.0F);
	oracool::AddShockwave({ 41, 40 }, {}, 0.5F);
	EXPECT_EQ(oracool::ShockwaveCount(), 2U);
	oracool::AddShockwave({ 41, 40 }, {}, 0.0F); // no strength, no wave
	EXPECT_EQ(oracool::ShockwaveCount(), 2U);
	for (int i = 0; i < 20; i++)
		oracool::AddShockwave({ 40, 40 }, {}, 1.0F);
	EXPECT_LE(oracool::ShockwaveCount(), 8U); // a cap, so a storm of explosions cannot pile up work
	oracool::ForgetSceneLife();
	EXPECT_EQ(oracool::ShockwaveCount(), 0U);
	EXPECT_EQ(oracool::FootprintCount(), 0U);
}

TEST(OracoolGraphicsBatch4, AKickShakesThePicture)
{
	oracool::ForgetSceneFx();
	EXPECT_EQ(oracool::ScreenShakeOffset().deltaY, 0);
	oracool::KickScreenShake(6.0F, 0.8F);
	EXPECT_NE(oracool::ScreenShakeOffset().deltaY, 0); // at its start: 6 x 0.6 x 0.7 = 2.5 px down
	oracool::ForgetSceneFx();
	EXPECT_EQ(oracool::ScreenShakeOffset().deltaY, 0);
}

} // namespace
