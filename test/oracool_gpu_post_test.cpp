/**
 * The GPU renderer plan, Stage 1 (oracool/gpu_post.h, user 2026-10-11): the graphics card's bloom, mood, vignette, zoom
 * and sharpening against the CPU's own, on the same frames. They must agree to the pixel.
 *
 * Runs on Direct3D's software rasteriser (WARP), so no graphics card is needed; skipped where Direct3D 11 is not.
 */
#include <gtest/gtest.h>

#include <cstdint>
#include <algorithm>
#include <cstring>
#include <vector>

#include "engine/surface.hpp"
#include "levels/gendung.h"
#include "options.h"
#include "oracool/gpu_post.h"
#include "oracool/post_fx.h"

using namespace devilution;

namespace {

/** Saves and restores what a test here touches. */
struct GpuScene {
	dungeon_type savedType = leveltype;
	bool savedBloom = *sgOptions.Oracool.bloom;
	bool savedMood = *sgOptions.Oracool.areaMood;
	bool savedVignette = *sgOptions.Oracool.vignette;
	bool savedGpu = *sgOptions.Oracool.gpuRenderer;

	GpuScene()
	{
		leveltype = DTYPE_CAVES;
		sgOptions.Oracool.gpuRenderer.SetValue(true);
		oracool::GpuWorldPostUseSoftwareDeviceForTests();
	}
	~GpuScene()
	{
		sgOptions.Oracool.bloom.SetValue(savedBloom);
		sgOptions.Oracool.areaMood.SetValue(savedMood);
		sgOptions.Oracool.vignette.SetValue(savedVignette);
		sgOptions.Oracool.gpuRenderer.SetValue(savedGpu);
		oracool::ShutdownGpuWorldPost();
		leveltype = savedType;
	}
};

uint32_t Rgb(int r, int g, int b)
{
	return (static_cast<uint32_t>(r) << 16) | (static_cast<uint32_t>(g) << 8) | static_cast<uint32_t>(b);
}

/** A frame with everything the passes react to: soft gradients, hard edges, noise, black, and bright spots. */
void PaintTestFrame(const Surface &s)
{
	uint32_t seed = 12345;
	const auto rnd = [&seed]() { seed = seed * 1103515245U + 12345U; return static_cast<int>((seed >> 16) & 0xFF); };
	for (int y = 0; y < s.h(); y++) {
		for (int x = 0; x < s.w(); x++) {
			int r = (x * 255) / s.w();
			int g = (y * 255) / s.h();
			int b = ((x + y) * 3) & 0xFF;
			if ((x / 9 + y / 7) % 5 == 0) // blocks of flat colour with hard edges
				r = g = b = 40;
			if ((x * 7 + y * 3) % 11 == 0)
				r = rnd(), g = rnd(), b = rnd();
			if (x % 37 < 3 && y % 29 < 3) // bright spots for the bloom
				r = 255, g = 240, b = 200;
			if (y % 41 == 0)
				r = g = b = 0; // black lines
			*s.at<uint32_t>(x, y) = Rgb(r, g, b);
		}
	}
}

int FirstDifference(const Surface &a, const Surface &b)
{
	for (int y = 0; y < a.h(); y++) {
		for (int x = 0; x < a.w(); x++) {
			if ((*a.at<uint32_t>(x, y) & 0xFFFFFF) != (*b.at<uint32_t>(x, y) & 0xFFFFFF))
				return y * 100000 + x;
		}
	}
	return -1;
}

/**
 * The CPU's chain into @p cpu and the card's into @p gpu: both @p dw x @p dh, the world in the top-left @p w x @p h.
 * The CPU zooms as ZoomScale does (ZoomScaleFiltered in place, then the sharpening).
 */
bool RunBoth(const Surface &cpu, const Surface &gpu, int w, int h, const oracool::GpuWorldPostSettings &settings)
{
	sgOptions.Oracool.bloom.SetValue(settings.bloom);
	sgOptions.Oracool.areaMood.SetValue(settings.mood);
	sgOptions.Oracool.vignette.SetValue(settings.vignette);
	const Surface cpuWorld = cpu.subregion(0, 0, w, h);
	oracool::ApplyBloom(cpuWorld);
	oracool::ApplyAreaMoodAndVignette(cpuWorld);
	if (settings.zoom == oracool::GpuZoom::Filtered) {
		oracool::ZoomScaleFiltered(reinterpret_cast<uint8_t *>(cpu.at<uint32_t>(0, 0)), cpu.pitch(), w, h, cpu.w(), cpu.h());
		if (settings.sharpen)
			oracool::SharpenAdaptive(reinterpret_cast<uint8_t *>(cpu.at<uint32_t>(0, 0)), cpu.pitch(), cpu.w(), cpu.h());
	} else if (settings.zoom == oracool::GpuZoom::Nearest) {
		// ZoomScale's own nearest loop, bottom row first (in place).
		for (int y = cpu.h() - 1; y >= 0; y--) {
			const int sy = std::min(y * h / cpu.h(), h - 1);
			std::vector<uint32_t> row(static_cast<size_t>(cpu.w()));
			for (int x = 0; x < cpu.w(); x++)
				row[static_cast<size_t>(x)] = *cpu.at<uint32_t>(std::min(x * w / cpu.w(), w - 1), sy);
			std::memcpy(cpu.at<uint32_t>(0, y), row.data(), row.size() * 4);
		}
	}
	const Surface gpuWorld = gpu.subregion(0, 0, w, h);
	return oracool::GpuWorldPost(gpuWorld, settings.zoom == oracool::GpuZoom::None ? gpuWorld : gpu, settings);
}

} // namespace

TEST(OracoolGpuPost, BloomMoodAndVignetteMatchTheCpuToThePixel)
{
	GpuScene scene;
	if (!oracool::GpuWorldPostAvailable())
		GTEST_SKIP() << "no Direct3D 11: " << oracool::GpuWorldPostStatus();
	OwnedSurface cpu = OwnedSurface::Rgb(333, 211);
	OwnedSurface gpu = OwnedSurface::Rgb(333, 211);
	PaintTestFrame(cpu);
	PaintTestFrame(gpu);
	oracool::GpuWorldPostSettings settings;
	settings.bloom = true;
	settings.mood = true;
	settings.moodType = DTYPE_CAVES;
	settings.vignette = true;
	ASSERT_TRUE(RunBoth(cpu, gpu, 333, 211, settings)) << oracool::GpuWorldPostStatus();
	const int diff = FirstDifference(cpu, gpu);
	EXPECT_EQ(diff, -1) << "first difference at x " << diff % 100000 << ", y " << diff / 100000;
}

TEST(OracoolGpuPost, TheSharpBilinearZoomAndSharpeningMatchTheCpu)
{
	GpuScene scene;
	if (!oracool::GpuWorldPostAvailable())
		GTEST_SKIP() << "no Direct3D 11: " << oracool::GpuWorldPostStatus();
	// 1.37x: a world of 251 x 160 into 344 x 219.
	OwnedSurface cpu = OwnedSurface::Rgb(344, 219);
	OwnedSurface gpu = OwnedSurface::Rgb(344, 219);
	PaintTestFrame(cpu.subregion(0, 0, 251, 160));
	PaintTestFrame(gpu.subregion(0, 0, 251, 160));
	oracool::GpuWorldPostSettings settings;
	settings.bloom = true;
	settings.vignette = true;
	settings.zoom = oracool::GpuZoom::Filtered;
	settings.sharpen = true;
	ASSERT_TRUE(RunBoth(cpu, gpu, 251, 160, settings)) << oracool::GpuWorldPostStatus();
	const int diff = FirstDifference(cpu, gpu);
	EXPECT_EQ(diff, -1) << "first difference at x " << diff % 100000 << ", y " << diff / 100000;
}

TEST(OracoolGpuPost, Scale2xAtExactlyTwiceMatchesTheCpu)
{
	GpuScene scene;
	if (!oracool::GpuWorldPostAvailable())
		GTEST_SKIP() << "no Direct3D 11: " << oracool::GpuWorldPostStatus();
	OwnedSurface cpu = OwnedSurface::Rgb(240, 160);
	OwnedSurface gpu = OwnedSurface::Rgb(240, 160);
	PaintTestFrame(cpu.subregion(0, 0, 120, 80));
	PaintTestFrame(gpu.subregion(0, 0, 120, 80));
	oracool::GpuWorldPostSettings settings;
	settings.mood = true;
	settings.moodType = DTYPE_CAVES;
	settings.zoom = oracool::GpuZoom::Filtered;
	ASSERT_TRUE(RunBoth(cpu, gpu, 120, 80, settings)) << oracool::GpuWorldPostStatus();
	const int diff = FirstDifference(cpu, gpu);
	EXPECT_EQ(diff, -1) << "first difference at x " << diff % 100000 << ", y " << diff / 100000;
}

TEST(OracoolGpuPost, TheNearestZoomMatchesTheCpu)
{
	GpuScene scene;
	if (!oracool::GpuWorldPostAvailable())
		GTEST_SKIP() << "no Direct3D 11: " << oracool::GpuWorldPostStatus();
	OwnedSurface cpu = OwnedSurface::Rgb(301, 190);
	OwnedSurface gpu = OwnedSurface::Rgb(301, 190);
	PaintTestFrame(cpu.subregion(0, 0, 230, 150));
	PaintTestFrame(gpu.subregion(0, 0, 230, 150));
	oracool::GpuWorldPostSettings settings;
	settings.zoom = oracool::GpuZoom::Nearest;
	ASSERT_TRUE(RunBoth(cpu, gpu, 230, 150, settings)) << oracool::GpuWorldPostStatus();
	const int diff = FirstDifference(cpu, gpu);
	EXPECT_EQ(diff, -1) << "first difference at x " << diff % 100000 << ", y " << diff / 100000;
}

TEST(OracoolGpuPost, OffTheCardIsNeverUsed)
{
	GpuScene scene;
	sgOptions.Oracool.gpuRenderer.SetValue(false);
	EXPECT_FALSE(oracool::GpuWorldPostAvailable());
}
