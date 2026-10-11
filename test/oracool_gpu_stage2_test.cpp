/**
 * The GPU renderer plan, Stage 2 (oracool/gpu_post.h, user 2026-10-11): the graphics card's relief, light tint, water
 * light, lava haze, splats and outline layer against the CPU's own, on the same frames and the same plans. Exact to the
 * pixel, but for the covering (ash) splats, which may round a step apart.
 *
 * Runs on Direct3D's software rasteriser (WARP); skipped where Direct3D 11 is not.
 */
#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>

#include "engine/surface.hpp"
#include "levels/gendung.h"
#include "options.h"
#include "oracool/colour_light.h"
#include "oracool/gpu_post.h"
#include "oracool/post_fx.h"
#include "oracool/splats.h"

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

uint32_t Seed = 1;
int Rnd(int n)
{
	Seed = Seed * 1103515245U + 12345U;
	return static_cast<int>((Seed >> 16) % static_cast<uint32_t>(n));
}

uint32_t Rgb(int r, int g, int b)
{
	return (static_cast<uint32_t>(r) << 16) | (static_cast<uint32_t>(g) << 8) | static_cast<uint32_t>(b);
}

void Paint(const Surface &s, uint32_t seed)
{
	Seed = seed;
	for (int y = 0; y < s.h(); y++)
		for (int x = 0; x < s.w(); x++)
			*s.at<uint32_t>(x, y) = (y % 23 == 0) ? 0 : Rgb((x * 5 + Rnd(40)) & 0xFF, (y * 3 + Rnd(40)) & 0xFF, ((x + y) * 2) & 0xFF);
}

void Copy(const Surface &from, const Surface &to)
{
	for (int y = 0; y < from.h(); y++)
		std::memcpy(to.at<uint32_t>(0, y), from.at<uint32_t>(0, y), static_cast<size_t>(from.w()) * 4);
}

/** The GroundMap origin colour_light.cpp makes for a first tile and its box's bottom-left. */
void GroundOrigin(Point firstTile, Point firstTileBuffer, int64_t &u0, int64_t &v0)
{
	const double cx = firstTileBuffer.x + 32.0;
	const double cy = firstTileBuffer.y - 14.5;
	const double rx = 0.5 - cx;
	const double ry = 0.5 - cy;
	u0 = static_cast<int64_t>(std::llround((firstTile.x + rx / 64.0 + ry / 32.0) * 65536.0));
	v0 = static_cast<int64_t>(std::llround((firstTile.y + ry / 32.0 - rx / 64.0) * 65536.0));
}

int MaxDifference(const Surface &a, const Surface &b, int &atX, int &atY)
{
	int worst = 0;
	for (int y = 0; y < a.h(); y++) {
		for (int x = 0; x < a.w(); x++) {
			const uint32_t p = *a.at<uint32_t>(x, y), q = *b.at<uint32_t>(x, y);
			for (int shift = 0; shift <= 16; shift += 8) {
				const int d = std::abs(static_cast<int>((p >> shift) & 0xFF) - static_cast<int>((q >> shift) & 0xFF));
				if (d > worst) {
					worst = d;
					atX = x;
					atY = y;
				}
			}
		}
	}
	return worst;
}

#define EXPECT_SAME_PICTURE(a, b, tolerance)                                      \
	do {                                                                          \
		int atX = -1, atY = -1;                                                   \
		const int worst = MaxDifference(a, b, atX, atY);                          \
		EXPECT_LE(worst, tolerance) << "worst at x " << atX << ", y " << atY;     \
	} while (false)

} // namespace

TEST(OracoolGpuStage2, TheLightTintMatchesTheCpu)
{
	GpuScene scene;
	if (!oracool::GpuWorldPostAvailable())
		GTEST_SKIP() << "no Direct3D 11: " << oracool::GpuWorldPostStatus();
	static std::array<oracool::LightTint, FieldCount> tints;
	Seed = 7;
	for (auto &t : tints)
		t = { static_cast<uint16_t>(150 + Rnd(250)), static_cast<uint16_t>(150 + Rnd(250)), static_cast<uint16_t>(120 + Rnd(200)) };
	OwnedSurface cpu = OwnedSurface::Rgb(301, 203);
	OwnedSurface gpu = OwnedSurface::Rgb(301, 203);
	Paint(cpu, 11);
	Copy(cpu, gpu);
	oracool::TintPlan plan;
	plan.tints = tints.data();
	plan.left = 13, plan.right = 287, plan.top = 5, plan.bottom = 190;
	GroundOrigin({ 40, 40 }, { 128, 64 }, plan.u0, plan.v0);
	oracool::ApplyTintPlan(cpu, plan);
	oracool::GpuStage2 stage2;
	stage2.tint = &plan;
	ASSERT_TRUE(oracool::GpuWorldPost(gpu, gpu, {}, &stage2)) << oracool::GpuWorldPostStatus();
	EXPECT_SAME_PICTURE(cpu, gpu, 0);
}

TEST(OracoolGpuStage2, TheReliefMatchesTheCpu)
{
	GpuScene scene;
	if (!oracool::GpuWorldPostAvailable())
		GTEST_SKIP() << "no Direct3D 11: " << oracool::GpuWorldPostStatus();
	static std::array<int16_t, FieldCount> dirX, dirY;
	Seed = 5;
	for (size_t i = 0; i < FieldCount; i++) {
		dirX[i] = static_cast<int16_t>(Rnd(513) - 256);
		dirY[i] = static_cast<int16_t>(Rnd(513) - 256);
	}
	OwnedSurface kept = OwnedSurface::Rgb(250, 170);
	OwnedSurface cpu = OwnedSurface::Rgb(250, 170);
	OwnedSurface gpu = OwnedSurface::Rgb(250, 170);
	Paint(kept, 3);
	Copy(kept, cpu);
	// "Sprites": blocks of new pixels over the kept world, one of them in an outline colour.
	Seed = 9;
	for (int y = 20; y < 150; y++) {
		for (int x = 10; x < 240; x++) {
			if (((x / 17) + (y / 13)) % 3 == 0)
				*cpu.at<uint32_t>(x, y) = Rgb(Rnd(256), Rnd(256), Rnd(256));
		}
	}
	for (int x = 30; x < 90; x++)
		*cpu.at<uint32_t>(x, 60) = Rgb(200, 30, 40);
	Copy(cpu, gpu);
	oracool::ReliefPlan plan;
	plan.dirX = dirX.data();
	plan.dirY = dirY.data();
	plan.outline[0] = Rgb(200, 30, 40);
	plan.slopeShift = 9;
	plan.maxDelta = 40;
	GroundOrigin({ 30, 30 }, { 100, 50 }, plan.u0, plan.v0);
	std::vector<uint32_t> packed(250 * 170);
	for (int y = 0; y < 170; y++)
		std::memcpy(&packed[static_cast<size_t>(y) * 250], kept.at<uint32_t>(0, y), 250 * 4);
	oracool::ApplyReliefPlan(cpu, packed.data(), plan);
	oracool::GpuKeepWorld(kept);
	oracool::GpuStage2 stage2;
	stage2.relief = &plan;
	ASSERT_TRUE(oracool::GpuWorldPost(gpu, gpu, {}, &stage2)) << oracool::GpuWorldPostStatus();
	EXPECT_SAME_PICTURE(cpu, gpu, 0);
}

TEST(OracoolGpuStage2, TheWaterLightAndLavaHazeMatchTheCpu)
{
	GpuScene scene;
	if (!oracool::GpuWorldPostAvailable())
		GTEST_SKIP() << "no Direct3D 11: " << oracool::GpuWorldPostStatus();
	static std::array<uint8_t, FieldCount> liquid;
	Seed = 21;
	for (auto &l : liquid)
		l = static_cast<uint8_t>(Rnd(3));
	OwnedSurface cpu = OwnedSurface::Rgb(280, 190);
	OwnedSurface gpu = OwnedSurface::Rgb(280, 190);
	Paint(cpu, 17);
	Copy(cpu, gpu);
	oracool::HazePlan plan;
	plan.liquid = liquid.data();
	GroundOrigin({ 50, 40 }, { 120, 60 }, plan.u0, plan.v0);
	plan.w = 280, plan.h = 190;
	plan.water = true;
	plan.waterL = 4, plan.waterR = 270, plan.waterT = 3, plan.waterB = 185;
	plan.t1 = 123, plan.t2 = 456, plan.t3 = 789;
	plan.lava = true;
	plan.lavaL = 20, plan.lavaR = 250, plan.lavaT = 10, plan.lavaB = 170;
	plan.nbx = (plan.lavaR - plan.lavaL) / 8 + 2;
	plan.nby = (plan.lavaB - plan.lavaT) / 8 + 2;
	std::vector<uint8_t> strength(static_cast<size_t>(plan.nbx) * plan.nby);
	for (auto &s : strength)
		s = static_cast<uint8_t>(Rnd(4) == 0 ? 0 : 255 - Rnd(7) * 34);
	plan.strength = strength.data();
	plan.p1 = 300, plan.p2 = 900;
	plan.segL = plan.lavaL - 2, plan.segR = plan.lavaR + 2;
	oracool::ApplyHazePlan(cpu, plan);
	oracool::GpuStage2 stage2;
	stage2.haze = &plan;
	ASSERT_TRUE(oracool::GpuWorldPost(gpu, gpu, {}, &stage2)) << oracool::GpuWorldPostStatus();
	EXPECT_SAME_PICTURE(cpu, gpu, 0);
}

TEST(OracoolGpuStage2, AdditiveSplatsMatchTheCpuAndCoveringOnesComeWithinAStep)
{
	GpuScene scene;
	if (!oracool::GpuWorldPostAvailable())
		GTEST_SKIP() << "no Direct3D 11: " << oracool::GpuWorldPostStatus();
	OwnedSurface cpu = OwnedSurface::Rgb(160, 120);
	OwnedSurface gpu = OwnedSurface::Rgb(160, 120);
	Paint(cpu, 23);
	Copy(cpu, gpu);
	std::vector<oracool::Splat> splats;
	Seed = 31;
	for (int i = 0; i < 3000; i++) {
		const int x = Rnd(160), y = Rnd(120);
		const bool cover = Rnd(5) == 0;
		const int cr = Rnd(256), cg = Rnd(256), cb = Rnd(256), a = 1 + Rnd(256);
		uint32_t &d = *cpu.at<uint32_t>(x, y);
		int r = static_cast<int>((d >> 16) & 0xFF), g = static_cast<int>((d >> 8) & 0xFF), b = static_cast<int>(d & 0xFF);
		if (!cover) {
			const int ar = (cr * a) >> 8, ag = (cg * a) >> 8, ab = (cb * a) >> 8;
			r = std::min(r + ar, 255), g = std::min(g + ag, 255), b = std::min(b + ab, 255);
			splats.push_back({ static_cast<float>(x), static_cast<float>(y), ar / 255.0F, ag / 255.0F, ab / 255.0F, 0.0F });
		} else {
			r = (cr * a + r * (256 - a)) >> 8, g = (cg * a + g * (256 - a)) >> 8, b = (cb * a + b * (256 - a)) >> 8;
			splats.push_back({ static_cast<float>(x), static_cast<float>(y), cr * a / (256.0F * 255.0F), cg * a / (256.0F * 255.0F), cb * a / (256.0F * 255.0F), a / 256.0F });
		}
		d = Rgb(r, g, b);
	}
	oracool::GpuStage2 stage2;
	stage2.splats = &splats;
	ASSERT_TRUE(oracool::GpuWorldPost(gpu, gpu, {}, &stage2)) << oracool::GpuWorldPostStatus();
	// A pixel under several covering splats may gather a step per splat.
	EXPECT_SAME_PICTURE(cpu, gpu, 3);
}

TEST(OracoolGpuStage2, TheOutlineLayerGoesOnWhereItHoldsAnOutline)
{
	GpuScene scene;
	if (!oracool::GpuWorldPostAvailable())
		GTEST_SKIP() << "no Direct3D 11: " << oracool::GpuWorldPostStatus();
	OwnedSurface cpu = OwnedSurface::Rgb(120, 90);
	OwnedSurface gpu = OwnedSurface::Rgb(120, 90);
	OwnedSurface layer = OwnedSurface::Rgb(120, 90);
	Paint(cpu, 41);
	Copy(cpu, gpu);
	for (int y = 0; y < 90; y++)
		for (int x = 0; x < 120; x++)
			*layer.at<uint32_t>(x, y) = 0xFF000000U;
	for (int x = 20; x < 70; x++) {
		*layer.at<uint32_t>(x, 30) = Rgb(220, 20, 20);
		*cpu.at<uint32_t>(x, 30) = Rgb(220, 20, 20);
	}
	oracool::GpuStage2 stage2;
	stage2.outlines = &layer;
	stage2.outlineBox = Rectangle { Point { 15, 25 }, Size { 60, 10 } };
	ASSERT_TRUE(oracool::GpuWorldPost(gpu, gpu, {}, &stage2)) << oracool::GpuWorldPostStatus();
	EXPECT_SAME_PICTURE(cpu, gpu, 0);
}
