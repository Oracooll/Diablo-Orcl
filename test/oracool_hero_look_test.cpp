/**
 * @file oracool_hero_look_test.cpp
 *
 * The Barbarian's look on the Warrior's body (v1.12.021): the light-armour dye table maps the
 * measured ramps and nothing else, and the sheet scaler keeps every drawn pixel - index 0 included,
 * which is the foot shadow and the trap - with its feet on the floor.
 */

#include <gtest/gtest.h>

#include <array>
#include <cstring>
#include <memory>
#include <vector>

#include "engine/clx_sprite.hpp"
#include "engine/palette.h"
#include "engine/render/clx_render.hpp"
#include "engine/surface.hpp"
#include "oracool/hero_look.h"
#include "oracool/sprite_colours.h"
#include "oracool/sprite_import.h"
#include "lighting.h"
#include "player.h"
#include "utils/endian_write.hpp"
#include "utils/surface_to_clx.hpp"

using namespace devilution;

namespace {

devilution::Player LightBarbarian()
{
	devilution::Player player {};
	player._pClass = HeroClass::Barbarian;
	player._pgfxnum = 0; // light armour, unarmed
	return player;
}

/** The same glue sprite_import uses: a uint32 offset per list, then the lists. */
OwnedClxSpriteSheet SheetOf(std::vector<OwnedClxSpriteList> &lists)
{
	const size_t headerSize = 4 * lists.size();
	size_t total = headerSize;
	for (const OwnedClxSpriteList &list : lists)
		total += ClxSpriteList(list).dataSize();
	std::unique_ptr<uint8_t[]> data { new uint8_t[total] };
	size_t offset = headerSize;
	for (size_t i = 0; i < lists.size(); i++) {
		const ClxSpriteList list { lists[i] };
		WriteLE32(&data[i * 4], static_cast<uint32_t>(offset));
		std::memcpy(&data[offset], list.data(), list.dataSize());
		offset += list.dataSize();
	}
	return OwnedClxSpriteSheet { std::move(data), static_cast<uint16_t>(lists.size()) };
}

constexpr int Side = 10;
constexpr uint8_t Key = 1; // never painted below, so it can carry transparency

/** Two frames: one solid index 0 (the shadow colour), one with only its bottom-right corner painted 200. */
OwnedClxSpriteList TwoFrames()
{
	OwnedSurface source(Side, Side * 2);
	for (int y = 0; y < Side * 2; y++)
		std::memset(&source[Point { 0, y }], Key, Side);
	for (int y = 0; y < Side; y++)
		std::memset(&source[Point { 0, y }], 0, Side);
	for (int y = Side + 5; y < Side * 2; y++)
		std::memset(&source[Point { 5, y }], 200, 5);
	return SurfaceToClx(source, 2, Key);
}

struct Rendered {
	OwnedSurface colour;
	OwnedSurface mask;
	Rendered(ClxSprite sprite)
	    : colour(sprite.width(), sprite.height())
	    , mask(sprite.width(), sprite.height())
	{
		std::array<uint8_t, 256> opaque;
		opaque.fill(1);
		for (int y = 0; y < sprite.height(); y++) {
			std::memset(&colour[Point { 0, y }], 7, sprite.width());
			std::memset(&mask[Point { 0, y }], 0, sprite.width());
		}
		ClxDraw(colour, { 0, sprite.height() - 1 }, sprite);
		ClxDrawTRN(mask, { 0, sprite.height() - 1 }, sprite, opaque.data());
	}
	bool drawn(int x, int y) const { return mask[Point { x, y }] != 0; }
	uint8_t at(int x, int y) const { return colour[Point { x, y }]; }
};

} // namespace

TEST(OracoolHeroLook, OnlyTheLightBarbarianIsDyed)
{
	devilution::Player player = LightBarbarian();
	EXPECT_NE(oracool::HeroDyeTrn(player), nullptr);
	player._pgfxnum = 1 << 4; // medium armour: not measured, not dyed
	EXPECT_EQ(oracool::HeroDyeTrn(player), nullptr);
	player._pgfxnum = 0;
	player._pClass = HeroClass::Warrior;
	EXPECT_EQ(oracool::HeroDyeTrn(player), nullptr);
}

TEST(OracoolHeroLook, TheDyeMapsTheMeasuredRampsAndNothingElse)
{
	const devilution::Player player = LightBarbarian();
	const uint8_t *trn = oracool::HeroDyeTrn(player);
	ASSERT_NE(trn, nullptr);

	// Mail 240-255 -> trousers 184-191, two greys a blue, light to dark in step.
	for (int i = 240; i < 256; i++) {
		EXPECT_EQ(trn[i], 184 + (i - 240) / 2) << "mail index " << i;
	}
	// Boots 216-223 and gloves 168-175, one to one.
	for (int i = 0; i < 8; i++) {
		EXPECT_EQ(trn[216 + i], 184 + i) << "boot index " << 216 + i;
		EXPECT_EQ(trn[168 + i], 184 + i) << "glove index " << 168 + i;
	}
	// Hair to visible greys; the face beside it untouched.
	EXPECT_EQ(trn[206], 247);
	EXPECT_EQ(trn[207], 250);
	for (int i = 200; i <= 205; i++)
		EXPECT_EQ(trn[i], i) << "skin index " << i;
	// The trousers are the target, not a source.
	for (int i = 184; i <= 191; i++)
		EXPECT_EQ(trn[i], i) << "trouser index " << i;
	// Everything outside the five ramps is identity - the weapon colours among them.
	for (int i = 0; i < 168; i++)
		EXPECT_EQ(trn[i], i) << "index " << i;
	for (int i = 176; i < 184; i++)
		EXPECT_EQ(trn[i], i) << "index " << i;
	for (int i = 192; i < 200; i++)
		EXPECT_EQ(trn[i], i) << "index " << i;
	for (int i = 208; i < 216; i++)
		EXPECT_EQ(trn[i], i) << "index " << i;
	for (int i = 224; i < 240; i++)
		EXPECT_EQ(trn[i], i) << "index " << i;
}

TEST(OracoolHeroLook, TheBarbarianIsAFifthLargerAndNobodyElseIs)
{
	EXPECT_EQ(oracool::SpriteScalePercent(HeroClass::Barbarian), 120);
	EXPECT_EQ(oracool::SpriteScalePercent(HeroClass::Warrior), 100);
	EXPECT_EQ(oracool::SpriteScalePercent(HeroClass::Rogue), 100);
}

TEST(OracoolHeroLook, ScalingKeepsIndexZeroPixelsAndTheFloor)
{
	std::vector<OwnedClxSpriteList> lists;
	lists.push_back(TwoFrames());
	lists.push_back(TwoFrames());
	const OwnedClxSpriteSheet original = SheetOf(lists);

	EXPECT_FALSE(oracool::ScaleSpriteSheet(original, 100).has_value()) << "100% is a no-op, not a copy";

	OptionalOwnedClxSpriteSheet scaled = oracool::ScaleSpriteSheet(original, 120);
	ASSERT_TRUE(scaled.has_value());
	const ClxSpriteSheet sheet { *scaled };
	ASSERT_EQ(sheet.numLists(), 2U);
	for (size_t dir = 0; dir < 2; dir++) {
		const ClxSpriteList list = sheet[dir];
		ASSERT_EQ(list.numSprites(), 2U);
		EXPECT_EQ(list[0].width(), 12);
		EXPECT_EQ(list[0].height(), 12);

		// The solid index-0 frame is still solid: every pixel drawn, every pixel 0. "Still zero" is
		// not "not drawn", and a scaler that used 0 for transparency would have lost the whole frame.
		const Rendered solid(list[0]);
		for (int y = 0; y < 12; y++) {
			for (int x = 0; x < 12; x++) {
				EXPECT_TRUE(solid.drawn(x, y)) << "(" << x << "," << y << ")";
				EXPECT_EQ(solid.at(x, y), 0);
			}
		}

		// The corner frame: the painted corner is still bottom-right and on the floor, the rest empty.
		const Rendered corner(list[1]);
		EXPECT_FALSE(corner.drawn(0, 0));
		EXPECT_FALSE(corner.drawn(5, 5));
		EXPECT_TRUE(corner.drawn(11, 11));
		EXPECT_EQ(corner.at(11, 11), 200);
		EXPECT_TRUE(corner.drawn(6, 11)) << "the floor row is drawn where the corner was";
		EXPECT_FALSE(corner.drawn(11, 0)) << "nothing floats up into the empty rows";
	}
}

// ---------------------------------------------------------------------------------------------------
// v1.12.022: the dye as colours, and the tables DrawPlayer draws through.
// ---------------------------------------------------------------------------------------------------

namespace {

/** A palette and light tables a test can reason about: entry i is grey i, level L keeps (16-L)/16 of it. */
void UseGreyPaletteAndLinearLight()
{
	for (int i = 0; i < 256; i++)
		PaletteRGB[static_cast<size_t>(i)] = (static_cast<uint32_t>(i) << 16) | (static_cast<uint32_t>(i) << 8) | static_cast<uint32_t>(i);
	PaletteRgbGeneration++;
	for (size_t level = 0; level < NumLightingLevels; level++)
		for (int i = 0; i < 256; i++)
			LightTables[level][static_cast<size_t>(i)] = static_cast<uint8_t>(i * (16 - static_cast<int>(level)) / 16);
}

} // namespace

TEST(OracoolHeroLook, ASheetLeftAloneDrawsExactlyThePaletteThroughTheLightTable)
{
	UseGreyPaletteAndLinearLight();
	const oracool::SpriteColours colours;
	for (int level : { 0, 1, 7, 15 }) {
		const uint32_t *table = colours.Table(level);
		for (int i = 0; i < 256; i++) {
			const uint8_t lit = level == 0 ? static_cast<uint8_t>(i) : LightTables[static_cast<size_t>(level)][static_cast<size_t>(i)];
			ASSERT_EQ(table[i], PaletteRGB[lit]) << "level " << level << " index " << i;
		}
	}
}

TEST(OracoolHeroLook, AnOwnColourIsItselfInFullLightAndDarkensWithItsFallback)
{
	UseGreyPaletteAndLinearLight();
	oracool::SpriteColours colours;
	colours.Set(10, 0x2040C0, 200); // a blue the palette does not hold, shaded like entry 200

	EXPECT_TRUE(colours.HasOwn(10));
	EXPECT_FALSE(colours.HasOwn(11));
	EXPECT_EQ(colours.Table(0)[10], 0x2040C0U) << "full light is the colour as authored";
	EXPECT_EQ(colours.Table(0)[11], PaletteRGB[11]) << "its neighbour is untouched";

	// Level 8 keeps half of entry 200, so it keeps half of the blue - channel by channel.
	const uint32_t half = colours.Table(8)[10];
	EXPECT_EQ((half >> 16) & 0xFF, 0x20U / 2);
	EXPECT_EQ((half >> 8) & 0xFF, 0x40U / 2);
	EXPECT_EQ(half & 0xFF, 0xC0U / 2);

	uint32_t previous = 0xFFFFFF;
	for (int level = 0; level < static_cast<int>(NumLightingLevels); level++) {
		const uint32_t blue = colours.Table(level)[10] & 0xFF;
		EXPECT_LE(blue, previous) << "never brighter in deeper shadow, level " << level;
		previous = blue;
	}

	// A palette change rebuilds the tables rather than serving stale ones.
	PaletteRGB[11] = 0x123456;
	PaletteRgbGeneration++;
	EXPECT_EQ(colours.Table(0)[11], 0x123456U);
}

TEST(OracoolHeroLook, TheBarbariansMailIsSixteenBluesNotEightDoubled)
{
	const devilution::Player player = LightBarbarian();
	const std::shared_ptr<const oracool::SpriteColours> colours = oracool::HeroColours(player);
	ASSERT_NE(colours, nullptr);
	const uint8_t *dye = oracool::HeroDyeTrn(player);

	uint32_t previous = 0xFFFFFFFF;
	for (int i = 240; i < 256; i++) {
		ASSERT_TRUE(colours->HasOwn(static_cast<uint8_t>(i)));
		const uint32_t blue = colours->Own(static_cast<uint8_t>(i));
		EXPECT_NE(blue, previous) << "mail index " << i << " repeats its neighbour";
		EXPECT_GT(blue & 0xFF, (blue >> 16) & 0xFF) << "mail index " << i << " is not blue";
		EXPECT_EQ(colours->Fallback(static_cast<uint8_t>(i)), dye[i]) << "the index dye is the fallback";
		previous = blue;
	}
	EXPECT_EQ(colours->Own(240), 0x4E587DU) << "the lightest mail is the lightest trouser blue";
	EXPECT_EQ(colours->Own(255), 0x05070CU) << "and the darkest the darkest";
	// Boots and gloves are the trousers exactly; hair is a silver; the face and the trousers are left alone.
	EXPECT_EQ(colours->Own(216), 0x4E587DU);
	EXPECT_EQ(colours->Own(175), 0x05070CU);
	EXPECT_TRUE(colours->HasOwn(206));
	EXPECT_TRUE(colours->HasOwn(207));
	for (int i = 184; i <= 191; i++)
		EXPECT_FALSE(colours->HasOwn(static_cast<uint8_t>(i)));
	for (int i = 200; i <= 205; i++)
		EXPECT_FALSE(colours->HasOwn(static_cast<uint8_t>(i)));

	devilution::Player warrior = LightBarbarian();
	warrior._pClass = HeroClass::Warrior;
	EXPECT_EQ(oracool::HeroColours(warrior), nullptr);
}

TEST(OracoolHeroLook, DrawingThroughColoursWritesValuesOn32BitAndFallbacksOn8Bit)
{
	UseGreyPaletteAndLinearLight();
	oracool::SpriteColours colours;
	colours.Set(200, 0xAA5500, 90);

	// One frame, fully painted with index 200 (TwoFrames' corner frame carries it).
	OwnedClxSpriteList frames = TwoFrames();
	const ClxSprite corner = ClxSpriteList { frames }[1];

	OwnedSurface screen = OwnedSurface::Rgb(Side, Side);
	for (int y = 0; y < Side; y++)
		for (int x = 0; x < Side; x++)
			*screen.at<uint32_t>(x, y) = 0x010203;
	oracool::DrawSpriteWithColours(screen, { 0, Side - 1 }, corner, colours, 0);
	EXPECT_EQ(*screen.at<uint32_t>(Side - 1, Side - 1), 0xAA5500U) << "the authored colour, not a palette entry";
	EXPECT_EQ(*screen.at<uint32_t>(0, 0), 0x010203U) << "nothing outside the sprite is touched";

	OwnedSurface indexed(Side, Side);
	for (int y = 0; y < Side; y++)
		std::memset(&indexed[Point { 0, y }], 3, Side);
	oracool::DrawSpriteWithColours(indexed, { 0, Side - 1 }, corner, colours, 0);
	EXPECT_EQ((indexed[Point { Side - 1, Side - 1 }]), 90) << "an 8-bit target gets the fallback index";
	oracool::DrawSpriteWithColours(indexed, { 0, Side - 1 }, corner, colours, 8);
	EXPECT_EQ((indexed[Point { Side - 1, Side - 1 }]), LightTables[8][90]) << "lit through the same table";
	EXPECT_EQ((indexed[Point { 0, 0 }]), 3);
}
