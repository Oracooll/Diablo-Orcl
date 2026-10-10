/**
 * World post-effects (oracool/post_fx.h, user 2026-10-10).
 *
 * Pure arithmetic over synthetic 32-bit surfaces: no game archive is needed. (The liquid scan draws the level's floor
 * pieces, which needs the level's cels: without them it finds nothing, and the haze pass leaves the frame alone.)
 */
#include <gtest/gtest.h>

#include <cstdint>
#include <initializer_list>
#include <vector>

#include "engine/surface.hpp"
#include "levels/gendung.h"
#include "options.h"
#include "oracool/post_fx.h"

using namespace devilution;

namespace {

constexpr uint32_t White = 0xFFFFFF;
constexpr uint32_t Black = 0x000000;

uint32_t Grey(uint8_t v)
{
	return (static_cast<uint32_t>(v) << 16) | (static_cast<uint32_t>(v) << 8) | v;
}

uint32_t &Px(const Surface &s, int x, int y)
{
	return *s.at<uint32_t>(x, y);
}

void Fill(const Surface &s, uint32_t c)
{
	for (int y = 0; y < s.h(); y++)
		for (int x = 0; x < s.w(); x++)
			Px(s, x, y) = c;
}

int Channel(uint32_t c, int shift)
{
	return static_cast<int>((c >> shift) & 0xFF);
}

/** Saves and restores the options and the level type a test here touches. */
struct FxScene {
	dungeon_type savedType = leveltype;
	bool savedMood = *sgOptions.Oracool.areaMood;
	bool savedVignette = *sgOptions.Oracool.vignette;
	bool savedBloom = *sgOptions.Oracool.bloom;
	bool savedHaze = *sgOptions.Oracool.heatHaze;

	FxScene()
	{
		leveltype = DTYPE_CATHEDRAL;
		sgOptions.Oracool.areaMood.SetValue(false);
		sgOptions.Oracool.vignette.SetValue(false);
		sgOptions.Oracool.bloom.SetValue(false);
		sgOptions.Oracool.heatHaze.SetValue(false);
	}
	~FxScene()
	{
		sgOptions.Oracool.areaMood.SetValue(savedMood);
		sgOptions.Oracool.vignette.SetValue(savedVignette);
		sgOptions.Oracool.bloom.SetValue(savedBloom);
		sgOptions.Oracool.heatHaze.SetValue(savedHaze);
		leveltype = savedType;
	}
};

} // namespace

TEST(OracoolPostFx, Scale2xTurnsAStairIntoADiagonal)
{
	// A white diagonal on black, 3x3, in the top-left of a 6x6 surface, scaled in place.
	OwnedSurface out = OwnedSurface::Rgb(6, 6);
	Fill(out, 0x123456);
	for (int y = 0; y < 3; y++)
		for (int x = 0; x < 3; x++)
			Px(out, x, y) = x == y ? White : Black;

	oracool::ZoomScaleFiltered(out.at(0, 0), out.pitch(), 3, 3, 6, 6);

	// The diagonal's own pixels stay whole blocks where nothing borders them on two sides.
	for (int y = 2; y < 4; y++)
		for (int x = 2; x < 4; x++)
			EXPECT_EQ(Px(out, x, y), White) << x << "," << y;
	// Black (1, 0): its lower-left corner touches the white to its left and below - it turns white (nearest would not).
	EXPECT_EQ(Px(out, 2, 1), White);
	EXPECT_EQ(Px(out, 3, 1), Black);
	EXPECT_EQ(Px(out, 2, 0), Black);
	EXPECT_EQ(Px(out, 3, 0), Black);
	// And its mirror, black (0, 1): the upper-right corner.
	EXPECT_EQ(Px(out, 1, 2), White);
	EXPECT_EQ(Px(out, 0, 2), Black);
	// The far corners stay black.
	EXPECT_EQ(Px(out, 5, 0), Black);
	EXPECT_EQ(Px(out, 0, 5), Black);
}

TEST(OracoolPostFx, SharpBilinearIsACopyAtOneAndKeepsFlatColourFlat)
{
	OwnedSurface out = OwnedSurface::Rgb(12, 12);
	for (int y = 0; y < 12; y++)
		for (int x = 0; x < 12; x++)
			Px(out, x, y) = Grey(static_cast<uint8_t>(x * 20 + y));
	std::vector<uint32_t> before;
	for (int y = 0; y < 12; y++)
		for (int x = 0; x < 12; x++)
			before.push_back(Px(out, x, y));
	oracool::ZoomScaleFiltered(out.at(0, 0), out.pitch(), 12, 12, 12, 12);
	for (int y = 0; y < 12; y++)
		for (int x = 0; x < 12; x++)
			EXPECT_EQ(Px(out, x, y), before[static_cast<size_t>(y * 12 + x)]);

	Fill(out, 0x405060);
	oracool::ZoomScaleFiltered(out.at(0, 0), out.pitch(), 8, 8, 12, 12);
	for (int y = 0; y < 12; y++)
		for (int x = 0; x < 12; x++)
			EXPECT_EQ(Px(out, x, y), 0x405060U);
}

TEST(OracoolPostFx, SharpBilinearBlendsOnlyAtSourceEdges)
{
	// Columns alternating black and white, 8 wide, scaled to 12 (1.5x): every pixel is black, white, or one blend at an
	// edge - and most are exact.
	OwnedSurface out = OwnedSurface::Rgb(12, 4);
	for (int y = 0; y < 4; y++)
		for (int x = 0; x < 12; x++)
			Px(out, x, y) = x < 8 && (x & 1) != 0 ? White : Black;
	oracool::ZoomScaleFiltered(out.at(0, 0), out.pitch(), 8, 4, 12, 4);
	int exact = 0;
	for (int x = 0; x < 12; x++) {
		const uint32_t c = Px(out, x, 0);
		EXPECT_EQ(Channel(c, 16), Channel(c, 0)) << x; // still grey
		if (c == White || c == Black)
			exact++;
	}
	EXPECT_GE(exact, 6);
	EXPECT_EQ(Px(out, 0, 0), Black); // the first source pixel's own inside
}

TEST(OracoolPostFx, MoodAndVignetteKeepBlackAndLeaveIndexedSurfacesAlone)
{
	FxScene scene;
	sgOptions.Oracool.areaMood.SetValue(true);
	sgOptions.Oracool.vignette.SetValue(true);

	OwnedSurface out = OwnedSurface::Rgb(64, 48);
	Fill(out, Grey(160));
	Px(out, 0, 0) = Black;
	Px(out, 32, 24) = Black;
	oracool::ApplyAreaMoodAndVignette(out);
	EXPECT_EQ(Px(out, 0, 0), Black);
	EXPECT_EQ(Px(out, 32, 24), Black);

	OwnedSurface indexed(16, 16);
	for (int y = 0; y < 16; y++)
		for (int x = 0; x < 16; x++)
			*indexed.at(x, y) = static_cast<uint8_t>(x + y * 16);
	oracool::ApplyAreaMoodAndVignette(indexed);
	oracool::ApplyBloom(indexed);
	for (int y = 0; y < 16; y++)
		for (int x = 0; x < 16; x++)
			EXPECT_EQ(*indexed.at(x, y), static_cast<uint8_t>(x + y * 16));
}

TEST(OracoolPostFx, TheVignetteDarkensTheCornersByAQuarterAtMost)
{
	FxScene scene;
	sgOptions.Oracool.vignette.SetValue(true);
	OwnedSurface out = OwnedSurface::Rgb(200, 100);
	Fill(out, Grey(200));
	oracool::ApplyAreaMoodAndVignette(out);
	EXPECT_EQ(Px(out, 100, 50), Grey(200)); // the middle untouched
	const int corner = Channel(Px(out, 0, 0), 0);
	EXPECT_LT(corner, 170);
	EXPECT_GE(corner, 145); // 0.865 an axis: 25% darker = 150, less a step of rounding
	const int edge = Channel(Px(out, 0, 50), 0);
	EXPECT_LT(edge, 200);
	EXPECT_GT(edge, corner);
}

TEST(OracoolPostFx, TheMoodIsSubtleKeepsBrightnessAndIsOffInTown)
{
	FxScene scene;
	sgOptions.Oracool.areaMood.SetValue(true);
	OwnedSurface out = OwnedSurface::Rgb(8, 8);
	for (const dungeon_type type : { DTYPE_CATHEDRAL, DTYPE_CATACOMBS, DTYPE_CAVES, DTYPE_HELL, DTYPE_NEST, DTYPE_CRYPT }) {
		leveltype = type;
		Fill(out, Grey(128));
		oracool::ApplyAreaMoodAndVignette(out);
		const uint32_t c = Px(out, 3, 3);
		const int r = Channel(c, 16), g = Channel(c, 8), b = Channel(c, 0);
		EXPECT_NE(c, Grey(128)) << int(type); // it does something
		EXPECT_NEAR(r, 128, 16) << int(type);
		EXPECT_NEAR(g, 128, 16) << int(type);
		EXPECT_NEAR(b, 128, 16) << int(type);
		EXPECT_NEAR((r * 299 + g * 587 + b * 114) / 1000, 128, 3) << int(type);
	}
	leveltype = DTYPE_TOWN;
	Fill(out, Grey(128));
	oracool::ApplyAreaMoodAndVignette(out);
	EXPECT_EQ(Px(out, 3, 3), Grey(128));
}

TEST(OracoolPostFx, BloomLeavesADarkFrameAloneAndGlowsAroundABrightSpot)
{
	FxScene scene;
	sgOptions.Oracool.bloom.SetValue(true);
	OwnedSurface out = OwnedSurface::Rgb(128, 96);
	Fill(out, Grey(90));
	oracool::ApplyBloom(out);
	for (int y = 0; y < out.h(); y++)
		for (int x = 0; x < out.w(); x++)
			ASSERT_EQ(Px(out, x, y), Grey(90)) << x << "," << y;

	for (int y = 44; y < 52; y++)
		for (int x = 60; x < 68; x++)
			Px(out, x, y) = White;
	oracool::ApplyBloom(out);
	EXPECT_EQ(Px(out, 64, 48), White);
	EXPECT_GT(Channel(Px(out, 74, 48), 0), 90); // 6 px out from the spot: lifted
	EXPECT_GT(Channel(Px(out, 64, 40), 0), 90);
	EXPECT_EQ(Px(out, 2, 2), Grey(90)); // far away: untouched
	EXPECT_EQ(Px(out, 125, 93), Grey(90));
}

TEST(OracoolPostFx, HeatHazeLeavesTownIndexedAndDryLevelsAlone)
{
	FxScene scene;
	sgOptions.Oracool.heatHaze.SetValue(true);
	OwnedSurface out = OwnedSurface::Rgb(64, 48);
	Fill(out, Grey(120));
	Px(out, 10, 10) = White;
	for (const dungeon_type type : { DTYPE_TOWN, DTYPE_CATHEDRAL, DTYPE_CAVES }) {
		leveltype = type;
		oracool::ApplyHeatHazeAndWater(out, { 40, 40 }, { 0, 40 });
		if (pDungeonCels == nullptr)
			EXPECT_EQ(oracool::LiquidKindAt({ 40, 40 }), 0) << int(type);
	}
	for (int y = 0; y < out.h(); y++)
		for (int x = 0; x < out.w(); x++)
			ASSERT_EQ(Px(out, x, y), x == 10 && y == 10 ? White : Grey(120)) << x << "," << y;
}
