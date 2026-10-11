/**
 * The GPU renderer plan, Stage 3 (oracool/gpu_post.h, user 2026-10-11): the effects only the graphics card runs. They have
 * no CPU original to match, so these check what each does: where it lights, glows, bends or grades, and where it leaves
 * the picture be.
 *
 * Runs on Direct3D's software rasteriser (WARP); skipped where Direct3D 11 is not.
 */
#include <gtest/gtest.h>

#include <array>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <vector>

#include "engine/surface.hpp"
#include "levels/gendung.h"
#include "options.h"
#include "oracool/colour_light.h"
#include "oracool/gpu_post.h"

using namespace devilution;

namespace {

constexpr size_t FieldCount = static_cast<size_t>(MAXDUNX) * MAXDUNY;

struct GpuScene {
	bool savedGpu = *sgOptions.Oracool.gpuRenderer;
	GpuScene()
	{
		sgOptions.Oracool.gpuRenderer.SetValue(true);
		oracool::GpuWorldPostUseSoftwareDeviceForTests();
	}
	~GpuScene()
	{
		sgOptions.Oracool.gpuRenderer.SetValue(savedGpu);
		oracool::ShutdownGpuWorldPost();
	}
};

uint32_t Grey(int v)
{
	return (static_cast<uint32_t>(v) << 16) | (static_cast<uint32_t>(v) << 8) | static_cast<uint32_t>(v);
}

int R(uint32_t c)
{
	return static_cast<int>((c >> 16) & 0xFF);
}

void Fill(const Surface &s, uint32_t c)
{
	for (int y = 0; y < s.h(); y++)
		for (int x = 0; x < s.w(); x++)
			*s.at<uint32_t>(x, y) = c;
}

/** A first tile of (40, 40) with its box's bottom-left at (128, 64): tile (x, y)'s centre on screen. */
const Point FirstTile { 40, 40 };
const Point FirstBuffer { 128, 64 };
Point Centre(int x, int y)
{
	const int dx = x - FirstTile.x, dy = y - FirstTile.y;
	return { FirstBuffer.x + 32 + (dx - dy) * 32, FirstBuffer.y - 15 + (dx + dy) * 16 };
}

oracool::GpuStage3 Inputs(const oracool::GpuLight *lights, size_t n)
{
	oracool::GpuStage3 in;
	in.firstTile = FirstTile;
	in.firstTileBuffer = FirstBuffer;
	in.lights = lights;
	in.lightCount = n;
	static std::array<uint8_t, FieldCount> open {};
	in.blocks = open.data();
	in.seconds = 1.5F;
	return in;
}

} // namespace

TEST(OracoolGpuStage3, PerPixelLightsPoolRoundALightAndStopAtAWall)
{
	GpuScene scene;
	if (!oracool::GpuWorldPostAvailable())
		GTEST_SKIP() << "no Direct3D 11: " << oracool::GpuWorldPostStatus();
	OwnedSurface out = OwnedSurface::Rgb(400, 260);
	Fill(out, Grey(100));
	const oracool::GpuLight light { 44.0F, 44.0F, 5.0F, 1.0F, 1.0F, 0.8F, 0.5F, 1 };
	oracool::GpuStage3 in = Inputs(&light, 1);
	// A wall line between the light and tile (44, 48).
	static std::array<uint8_t, FieldCount> blocks {};
	for (int x = 40; x <= 48; x++)
		blocks[static_cast<size_t>(x) * MAXDUNY + 46] = 1;
	in.blocks = blocks.data();
	oracool::GpuWorldPostSettings settings;
	settings.perPixelLights = true;
	oracool::GpuStage2 extra;
	extra.stage3 = &in;
	ASSERT_TRUE(oracool::GpuWorldPost(out, out, settings, &extra)) << oracool::GpuWorldPostStatus();
	const Point lit = Centre(44, 43);
	const Point shaded = Centre(44, 48);
	const Point far = Centre(45, 38); // 6 tiles off: out of reach, and on the picture
	EXPECT_GT(R(*out.at<uint32_t>(lit.x, lit.y)), 120);  // in the pool
	EXPECT_EQ(R(*out.at<uint32_t>(shaded.x, shaded.y)), 100); // behind the wall
	EXPECT_EQ(R(*out.at<uint32_t>(far.x, far.y)), 100);    // out of reach
}

TEST(OracoolGpuStage3, TheWideBloomSpreadsFarFromABrightSpot)
{
	GpuScene scene;
	if (!oracool::GpuWorldPostAvailable())
		GTEST_SKIP() << "no Direct3D 11: " << oracool::GpuWorldPostStatus();
	OwnedSurface out = OwnedSurface::Rgb(320, 240);
	Fill(out, Grey(30));
	for (int y = 110; y < 130; y++)
		for (int x = 150; x < 170; x++)
			*out.at<uint32_t>(x, y) = Grey(255);
	oracool::GpuStage3 in = Inputs(nullptr, 0);
	oracool::GpuWorldPostSettings settings;
	settings.wideBloom = true;
	oracool::GpuStage2 extra;
	extra.stage3 = &in;
	ASSERT_TRUE(oracool::GpuWorldPost(out, out, settings, &extra)) << oracool::GpuWorldPostStatus();
	EXPECT_GT(R(*out.at<uint32_t>(200, 120)), 30); // 30 px off the spot, glowing
	EXPECT_LE(R(*out.at<uint32_t>(5, 5)), 31);     // the far corner, a step at most
}

TEST(OracoolGpuStage3, HeatBendsThePictureOnlyNearItsSource)
{
	GpuScene scene;
	if (!oracool::GpuWorldPostAvailable())
		GTEST_SKIP() << "no Direct3D 11: " << oracool::GpuWorldPostStatus();
	OwnedSurface out = OwnedSurface::Rgb(400, 260);
	OwnedSurface before = OwnedSurface::Rgb(400, 260);
	for (int y = 0; y < 260; y++)
		for (int x = 0; x < 400; x++)
			*out.at<uint32_t>(x, y) = *before.at<uint32_t>(x, y) = Grey((x * 37 + y * 11) & 0xFF);
	const oracool::GpuHeat heat { 44.0F, 44.0F, 2.0F, 1.0F };
	oracool::GpuStage3 in = Inputs(nullptr, 0);
	in.heat = &heat;
	in.heatCount = 1;
	oracool::GpuWorldPostSettings settings;
	settings.heatDistortion = true;
	oracool::GpuStage2 extra;
	extra.stage3 = &in;
	ASSERT_TRUE(oracool::GpuWorldPost(out, out, settings, &extra)) << oracool::GpuWorldPostStatus();
	int changedNear = 0;
	const Point near = Centre(44, 44) - Displacement { 0, 20 };
	for (int y = near.y - 8; y <= near.y + 8; y++)
		for (int x = near.x - 8; x <= near.x + 8; x++)
			changedNear += *out.at<uint32_t>(x, y) != *before.at<uint32_t>(x, y) ? 1 : 0;
	EXPECT_GT(changedNear, 20);
	EXPECT_EQ(*out.at<uint32_t>(10, 250), *before.at<uint32_t>(10, 250));
}

TEST(OracoolGpuStage3, TheFilmicGradeDeepensTheBlacks)
{
	GpuScene scene;
	if (!oracool::GpuWorldPostAvailable())
		GTEST_SKIP() << "no Direct3D 11: " << oracool::GpuWorldPostStatus();
	OwnedSurface out = OwnedSurface::Rgb(64, 64);
	Fill(out, Grey(8));
	*out.at<uint32_t>(10, 10) = Grey(200);
	oracool::GpuStage3 in = Inputs(nullptr, 0);
	oracool::GpuWorldPostSettings settings;
	settings.filmicGrade = true;
	settings.moodType = DTYPE_CATHEDRAL;
	oracool::GpuStage2 extra;
	extra.stage3 = &in;
	ASSERT_TRUE(oracool::GpuWorldPost(out, out, settings, &extra)) << oracool::GpuWorldPostStatus();
	EXPECT_LT(R(*out.at<uint32_t>(30, 30)), 8); // a near-black, blacker
	EXPECT_NE(*out.at<uint32_t>(10, 10), Grey(200));
}

TEST(OracoolGpuStage3, LightShaftsReachOutFromAFlame)
{
	GpuScene scene;
	if (!oracool::GpuWorldPostAvailable())
		GTEST_SKIP() << "no Direct3D 11: " << oracool::GpuWorldPostStatus();
	OwnedSurface out = OwnedSurface::Rgb(400, 260);
	Fill(out, Grey(20));
	const Point flame = Centre(44, 44) - Displacement { 0, 30 };
	for (int y = flame.y - 6; y <= flame.y + 6; y++)
		for (int x = flame.x - 6; x <= flame.x + 6; x++)
			*out.at<uint32_t>(x, y) = Grey(255);
	const oracool::GpuLight light { 44.0F, 44.0F, 6.0F, 1.0F, 1.0F, 0.7F, 0.4F, 1 };
	oracool::GpuStage3 in = Inputs(&light, 1);
	oracool::GpuWorldPostSettings settings;
	settings.lightShafts = true;
	oracool::GpuStage2 extra;
	extra.stage3 = &in;
	ASSERT_TRUE(oracool::GpuWorldPost(out, out, settings, &extra)) << oracool::GpuWorldPostStatus();
	EXPECT_GT(R(*out.at<uint32_t>(flame.x + 40, flame.y + 20)), 20); // along a ray, lit
}

TEST(OracoolGpuStage3, LitSpritesShadeTheSideAwayFromTheLight)
{
	GpuScene scene;
	if (!oracool::GpuWorldPostAvailable())
		GTEST_SKIP() << "no Direct3D 11: " << oracool::GpuWorldPostStatus();
	OwnedSurface kept = OwnedSurface::Rgb(400, 260);
	OwnedSurface out = OwnedSurface::Rgb(400, 260);
	Fill(kept, Grey(60));
	Fill(out, Grey(60));
	// A "pillar" over tile (44, 44): a bright ridge down its middle, darker either side.
	const Point c = Centre(44, 44);
	for (int y = c.y - 40; y <= c.y; y++)
		for (int x = c.x - 12; x <= c.x + 12; x++)
			*out.at<uint32_t>(x, y) = Grey(200 - std::abs(x - c.x) * 8);
	const oracool::GpuLight light { 41.0F, 47.0F, 8.0F, 1.0F, 1.0F, 1.0F, 1.0F, 1 }; // to the pillar's left on screen
	oracool::GpuStage3 in = Inputs(&light, 1);
	oracool::ReliefPlan mask;
	ASSERT_TRUE(oracool::PlanLitSprites(out, FirstTile, FirstBuffer, mask));
	oracool::GpuKeepWorld(kept);
	oracool::GpuWorldPostSettings settings;
	settings.litSprites = true;
	oracool::GpuStage2 extra;
	extra.relief = &mask;
	extra.stage3 = &in;
	ASSERT_TRUE(oracool::GpuWorldPost(out, out, settings, &extra)) << oracool::GpuWorldPostStatus();
	const int left = R(*out.at<uint32_t>(c.x - 6, c.y - 20));
	const int right = R(*out.at<uint32_t>(c.x + 6, c.y - 20));
	EXPECT_GT(left, right); // the same brightness drawn, the light's side brighter
	EXPECT_EQ(*out.at<uint32_t>(c.x + 80, c.y - 20), Grey(60)); // the floor
}
