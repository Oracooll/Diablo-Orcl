/**
 * The third graphics batch (user, 2026-10-11: "i like all of your suggestions"): oracool/scene_fx.h - exploration memory,
 * weather, ground fog, the CRT look, and that nothing happens with no entrance or weather under way.
 *
 * Pure arithmetic over synthetic 32-bit surfaces: no game archive is needed.
 */
#include <gtest/gtest.h>

#include <cstdint>
#include <cstdlib>

#include "engine/surface.hpp"
#include "levels/gendung.h"
#include "lighting.h"
#include "options.h"
#include "oracool/scene_fx.h"

using namespace devilution;

namespace {

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

int Luma(uint32_t c)
{
	return static_cast<int>(((c >> 16) & 0xFF) + ((c >> 8) & 0xFF) + (c & 0xFF));
}

class GraphicsBatch3 : public ::testing::Test {
protected:
	void SetUp() override
	{
		savedLevel = leveltype;
		savedMemory = *sgOptions.Oracool.explorationMemory;
		savedWeather = *sgOptions.Oracool.weather;
		savedFog = *sgOptions.Oracool.groundFog;
		savedCrt = *sgOptions.Oracool.crtLook;
		savedDeband = *sgOptions.Oracool.debanding;
		savedLow = *sgOptions.Oracool.lowHealthWarning;
		sgOptions.Oracool.lowHealthWarning.SetValue(false); // needs a hero: not here
	}
	void TearDown() override
	{
		leveltype = savedLevel;
		sgOptions.Oracool.explorationMemory.SetValue(savedMemory);
		sgOptions.Oracool.weather.SetValue(savedWeather);
		sgOptions.Oracool.groundFog.SetValue(savedFog);
		sgOptions.Oracool.crtLook.SetValue(savedCrt);
		sgOptions.Oracool.debanding.SetValue(savedDeband);
		sgOptions.Oracool.lowHealthWarning.SetValue(savedLow);
		oracool::SetWeather(oracool::Weather::Clear);
		for (int x = 0; x < MAXDUNX; x++) {
			for (int y = 0; y < MAXDUNY; y++) {
				dLight[x][y] = 0;
				dFlags[x][y] = DungeonFlag::None;
			}
		}
	}
	dungeon_type savedLevel = DTYPE_TOWN;
	bool savedMemory = false, savedWeather = true, savedFog = true, savedCrt = false, savedDeband = true, savedLow = true;
};

TEST_F(GraphicsBatch3, ExplorationMemoryKeepsAFaintLightOnExploredDarkTiles)
{
	leveltype = DTYPE_CATHEDRAL;
	dFlags[10][10] = DungeonFlag::Explored;
	sgOptions.Oracool.explorationMemory.SetValue(false);
	EXPECT_EQ(oracool::ExplorationMemoryLight({ 10, 10 }, LightsMax), LightsMax); // off: untouched
	sgOptions.Oracool.explorationMemory.SetValue(true);
	EXPECT_EQ(oracool::ExplorationMemoryLight({ 10, 10 }, LightsMax), LightsMax - 3);
	EXPECT_EQ(oracool::ExplorationMemoryLight({ 10, 10 }, 4), 4);               // lit: its own light
	EXPECT_EQ(oracool::ExplorationMemoryLight({ 11, 10 }, LightsMax), LightsMax); // never explored: dark
	EXPECT_EQ(oracool::ExplorationMemoryLight({ -1, 5 }, LightsMax), LightsMax);  // off the map
	leveltype = DTYPE_TOWN;
	EXPECT_EQ(oracool::ExplorationMemoryLight({ 10, 10 }, LightsMax), LightsMax); // town has no darkness to remember
}

TEST_F(GraphicsBatch3, WeatherIsSetAndRead)
{
	oracool::SetWeather(oracool::Weather::Rain);
	float intensity = 0;
	EXPECT_EQ(oracool::CurrentWeather(&intensity), oracool::Weather::Rain);
	EXPECT_FLOAT_EQ(intensity, 1.0F);
	oracool::SetWeather(oracool::Weather::Clear);
	EXPECT_EQ(oracool::CurrentWeather(&intensity), oracool::Weather::Clear);
	EXPECT_FLOAT_EQ(intensity, 0.0F);
}

TEST_F(GraphicsBatch3, RainDarkensTheTownGroundAndSnowWhitensIt)
{
	OwnedSurface s = OwnedSurface::Rgb(64, 32);
	leveltype = DTYPE_TOWN;
	sgOptions.Oracool.weather.SetValue(true);
	oracool::SetWeather(oracool::Weather::Rain);
	Fill(s, Grey(120));
	oracool::ApplyWeatherGround(s, { 0, 0 }, { 0, 0 });
	EXPECT_LT(Luma(Px(s, 5, 5)), Luma(Grey(120)));
	oracool::SetWeather(oracool::Weather::Snow);
	Fill(s, Grey(120));
	oracool::ApplyWeatherGround(s, { 0, 0 }, { 0, 0 });
	EXPECT_GT(Luma(Px(s, 5, 5)), Luma(Grey(120)));
	// Off: nothing.
	sgOptions.Oracool.weather.SetValue(false);
	Fill(s, Grey(120));
	oracool::ApplyWeatherGround(s, { 0, 0 }, { 0, 0 });
	EXPECT_EQ(Px(s, 5, 5), Grey(120));
	// Not in town: nothing.
	sgOptions.Oracool.weather.SetValue(true);
	leveltype = DTYPE_CATHEDRAL;
	oracool::ApplyWeatherGround(s, { 0, 0 }, { 0, 0 });
	EXPECT_EQ(Px(s, 5, 5), Grey(120));
}

TEST_F(GraphicsBatch3, GroundFogShowsOnlyWhereThereIsLight)
{
	OwnedSurface s = OwnedSurface::Rgb(320, 200);
	leveltype = DTYPE_CAVES;
	sgOptions.Oracool.groundFog.SetValue(true);
	const auto changed = [&]() {
		int n = 0;
		for (int y = 0; y < s.h(); y++)
			for (int x = 0; x < s.w(); x++)
				n += Px(s, x, y) != Grey(20) ? 1 : 0;
		return n;
	};
	// All dark: the fog is not seen.
	for (int x = 0; x < MAXDUNX; x++)
		for (int y = 0; y < MAXDUNY; y++)
			dLight[x][y] = LightsMax;
	Fill(s, Grey(20));
	oracool::ApplyGroundFog(s, { 40, 40 }, { 0, 100 });
	EXPECT_EQ(changed(), 0);
	// All bright: somewhere it drifts.
	for (int x = 0; x < MAXDUNX; x++)
		for (int y = 0; y < MAXDUNY; y++)
			dLight[x][y] = 0;
	Fill(s, Grey(20));
	oracool::ApplyGroundFog(s, { 40, 40 }, { 0, 100 });
	EXPECT_GT(changed(), 0);
	// The Cathedral has none.
	leveltype = DTYPE_CATHEDRAL;
	Fill(s, Grey(20));
	oracool::ApplyGroundFog(s, { 40, 40 }, { 0, 100 });
	EXPECT_EQ(changed(), 0);
}

TEST_F(GraphicsBatch3, CrtLookDarkensEveryOtherRowAndIsOffByDefault)
{
	OwnedSurface s = OwnedSurface::Rgb(60, 40);
	sgOptions.Oracool.crtLook.SetValue(false);
	Fill(s, Grey(200));
	oracool::ApplyCrtLook(s, true);
	EXPECT_EQ(Px(s, 30, 20), Grey(200));
	sgOptions.Oracool.crtLook.SetValue(true);
	oracool::ApplyCrtLook(s, false); // a frame not drawn whole: left alone, or it would darken twice
	EXPECT_EQ(Px(s, 30, 20), Grey(200));
	oracool::ApplyCrtLook(s, true);
	EXPECT_LT(Luma(Px(s, 30, 21)), Luma(Px(s, 30, 20))); // a scanline
	EXPECT_LT(Luma(Px(s, 0, 0)), Luma(Px(s, 30, 20)));   // a darker corner
}

TEST_F(GraphicsBatch3, TheFinishedWorldIsLeftAloneWithNothingUnderWay)
{
	// No entrance, clear weather, the warning off: ApplyAfterWorld changes nothing.
	OwnedSurface s = OwnedSurface::Rgb(32, 16);
	leveltype = DTYPE_CATHEDRAL;
	oracool::SetWeather(oracool::Weather::Clear);
	Fill(s, Grey(77));
	oracool::ApplyAfterWorld(s);
	for (int y = 0; y < s.h(); y++)
		for (int x = 0; x < s.w(); x++)
			EXPECT_EQ(Px(s, x, y), Grey(77));
}

TEST_F(GraphicsBatch3, NothingShakesOrShinesWithoutAnEntrance)
{
	EXPECT_FALSE(oracool::GuardianEntranceActive());
	const Displacement shake = oracool::ScreenShakeOffset();
	EXPECT_EQ(shake.deltaX, 0);
	EXPECT_EQ(shake.deltaY, 0);
	OwnedSurface s = OwnedSurface::Rgb(64, 64);
	Fill(s, Grey(50));
	oracool::DrawGuardianBeam(s, { 0, 0 }, { 0, 32 });
	oracool::DrawGuardianEntranceCard(s);
	oracool::ApplyTownSway(s, { 0, 0 }, { 0, 32 });
	EXPECT_EQ(Px(s, 32, 32), Grey(50));
	EXPECT_EQ(Px(s, 32, 0), Grey(50));
}

} // namespace
