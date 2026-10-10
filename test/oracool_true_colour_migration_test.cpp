/**
 * The true-colour migration (user, 2026-10-10): the colour tables that are built from the light tables follow
 * LightTableVersion as well as PaletteRgbGeneration, and the divine shine is a tint by colour values.
 *
 * Pure table arithmetic: no game archive is needed. Every global a test here touches is saved and restored.
 */
#include <gtest/gtest.h>

#include <array>
#include <cstdint>

#include "engine/palette.h"
#include "lighting.h"
#include "oracool/cold.h"
#include "oracool/missile_tint.h"
#include "oracool/sprite_colours.h"

using namespace devilution;

namespace {

/** Saves PaletteRGB, its generation, the light tables and their version; restores them on the way out. */
struct LightScene {
	std::array<uint32_t, 256> savedRgb = PaletteRGB;
	std::array<std::array<uint8_t, 256>, NumLightingLevels> savedTables = LightTables;

	LightScene()
	{
		// A grey ramp, so every index has a colour of its own to tell it apart by.
		for (size_t i = 0; i < 256; i++)
			PaletteRGB[i] = static_cast<uint32_t>(i) * 0x010101u;
		PaletteRgbGeneration++;
		if (PaletteRgbGeneration == 0)
			PaletteRgbGeneration++; // 0 means "never built" to the caches
	}
	~LightScene()
	{
		// Moved on rather than put back, so nothing built from the scene's tables is mistaken for current afterwards.
		PaletteRGB = savedRgb;
		PaletteRgbGeneration++;
		LightTables = savedTables;
		LightTableVersion++;
	}
};

constexpr uint8_t Index = 200;
constexpr int Level = 3;

} // namespace

TEST(OracoolTrueColourMigration, LitPaletteTableFollowsTheLightTables)
{
	LightScene scene;
	LightTables[Level][Index] = 40;
	ASSERT_EQ(oracool::LitPaletteTable(Level)[Index], PaletteRGB[40]);

	// MakeLightTable rebuilds the tables for the next level (Hell's blood walls, the Nest's lava) and leaves the palette
	// generation where it was: the version is what tells the cache.
	LightTables[Level][Index] = 41;
	LightTableVersion++;
	EXPECT_EQ(oracool::LitPaletteTable(Level)[Index], PaletteRGB[41]) << "a lit table outlived the light tables it was built from";
}

TEST(OracoolTrueColourMigration, SpriteColoursTablesFollowTheLightTables)
{
	LightScene scene;
	oracool::SpriteColours colours; // every index left alone: the palette through the light table
	LightTables[Level][Index] = 40;
	ASSERT_EQ(colours.Table(Level)[Index], PaletteRGB[40]);

	LightTables[Level][Index] = 41;
	LightTableVersion++;
	EXPECT_EQ(colours.Table(Level)[Index], PaletteRGB[41]) << "a sheet's lit table outlived the light tables";
}

TEST(OracoolTrueColourMigration, FrozenAndConvertedTablesFollowTheLightTables)
{
	LightScene scene;
	LightTables[Level][Index] = 40;
	const uint32_t frozenBefore = oracool::FrozenRgbTable(Level)[Index];
	const uint32_t convertedBefore = oracool::ConvertedRgbTable(Level)[Index];

	LightTables[Level][Index] = 240; // a far brighter entry, so the frost and the green both have to move
	LightTableVersion++;
	EXPECT_NE(oracool::FrozenRgbTable(Level)[Index], frozenBefore) << "the frost table kept the old light";
	EXPECT_NE(oracool::ConvertedRgbTable(Level)[Index], convertedBefore) << "the converted table kept the old light";
}

TEST(OracoolTrueColourMigration, DivineTintIsTheShineTheTrnLookedFor)
{
	// divine_trn.cpp's Brighten in values: each channel 55% of the way to white, then 100/112/125% (clamped).
	std::array<uint32_t, 256> base {};
	base[0] = oracool::Rgb(0x12, 0x34, 0x56);
	base[5] = oracool::Rgb(100, 50, 0);
	base[6] = oracool::Rgb(255, 255, 255);
	const uint32_t *shine = oracool::TintedTable(base.data(), oracool::Tint::Divine, 0, 0.0);
	EXPECT_EQ(shine[0], base[0]) << "entry 0 stays as it is, as DivineTrn[0] = 0 does";
	// r: 100 + 155*55/100 = 185; g: (50 + 205*55/100) * 112/100 = 162 * 1.12 = 181; b: (0 + 140) * 125/100 = 175.
	EXPECT_EQ(shine[5], oracool::Rgb(185, 181, 175));
	EXPECT_EQ(shine[6], oracool::Rgb(255, 255, 255)) << "white stays white (clamped, not wrapped)";
}
