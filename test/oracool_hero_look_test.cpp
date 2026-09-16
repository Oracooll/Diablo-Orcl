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
#include "engine/render/clx_render.hpp"
#include "engine/surface.hpp"
#include "oracool/hero_look.h"
#include "oracool/sprite_import.h"
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
