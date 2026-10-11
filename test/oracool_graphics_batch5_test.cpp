/**
 * The fifth graphics batch (user, 2026-10-11: "do all 10 suggestions"): oracool/scene_feel.h - the level transition's
 * iris, the hit flash's fall-off, where the floors are damp, and that its records are forgotten.
 *
 * No game archive is needed.
 */
#include <gtest/gtest.h>

#include "diablo.h"
#include "levels/gendung.h"
#include "options.h"
#include "oracool/scene_feel.h"

using namespace devilution;

namespace {

TEST(OracoolGraphicsBatch5, TheIrisOpensFromTheMiddle)
{
	const bool wasRunning = gbRunGame;
	const bool wasOn = *sgOptions.Oracool.levelTransition;
	sgOptions.Oracool.levelTransition.SetValue(true);
	gbRunGame = true;
	constexpr int W = 1280, H = 720;
	// Half open: the middle shows, the corners are dark.
	EXPECT_EQ(oracool::IrisFadeAt(W / 2, H / 2, W, H, 128), 256);
	EXPECT_EQ(oracool::IrisFadeAt(0, 0, W, H, 128), 0);
	// Shut, and open.
	EXPECT_EQ(oracool::IrisFadeAt(W / 2, H / 2, W, H, 0), 0);
	EXPECT_EQ(oracool::IrisFadeAt(0, 0, W, H, 256), 256);
	// Nearer the middle never shows less.
	EXPECT_GE(oracool::IrisFadeAt(W / 2 + 100, H / 2, W, H, 140), oracool::IrisFadeAt(W / 2 + 400, H / 2, W, H, 140));
	// The row agrees with the pixel.
	uint16_t row[W];
	oracool::IrisFadeRow(100, W, H, 150, row);
	EXPECT_NEAR(row[300], oracool::IrisFadeAt(300, 100, W, H, 150), 1);
	// Out of a game, or off: the plain fade.
	gbRunGame = false;
	EXPECT_EQ(oracool::IrisFadeAt(W / 2, H / 2, W, H, 128), -1);
	gbRunGame = true;
	sgOptions.Oracool.levelTransition.SetValue(false);
	EXPECT_EQ(oracool::IrisFadeAt(W / 2, H / 2, W, H, 128), -1);
	sgOptions.Oracool.levelTransition.SetValue(wasOn);
	gbRunGame = wasRunning;
}

TEST(OracoolGraphicsBatch5, TheHitFlashIsBriefAndFades)
{
	EXPECT_FLOAT_EQ(oracool::HitFlashStrength(0.0F), 1.0F);
	EXPECT_NEAR(oracool::HitFlashStrength(0.06F), 0.5F, 0.01F);
	EXPECT_FLOAT_EQ(oracool::HitFlashStrength(0.2F), 0.0F);
	EXPECT_FLOAT_EQ(oracool::HitFlashStrength(-1.0F), 0.0F);
}

TEST(OracoolGraphicsBatch5, OnlyTheCavesAndTheNestHaveDampFloors)
{
	const dungeon_type was = leveltype;
	const bool wasWet = *sgOptions.Oracool.wetFloors;
	sgOptions.Oracool.wetFloors.SetValue(true);
	leveltype = DTYPE_CAVES;
	EXPECT_TRUE(oracool::DampFloorsHere());
	leveltype = DTYPE_NEST;
	EXPECT_TRUE(oracool::DampFloorsHere());
	leveltype = DTYPE_CATHEDRAL;
	EXPECT_FALSE(oracool::DampFloorsHere());
	leveltype = DTYPE_TOWN;
	EXPECT_FALSE(oracool::DampFloorsHere());
	leveltype = was;
	sgOptions.Oracool.wetFloors.SetValue(wasWet);
}

TEST(OracoolGraphicsBatch5, ForgettingClearsTheParticles)
{
	oracool::ForgetSceneFeel();
	EXPECT_EQ(oracool::SceneFeelParticleCount(), 0);
	EXPECT_EQ(oracool::CameraGlideOffset().deltaX, 0);
}

} // namespace
