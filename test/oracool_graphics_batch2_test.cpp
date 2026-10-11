/**
 * The second graphics batch (user, 2026-10-11: "let's do 1,2,3,5,7,8"): day and night in Tristram, floor marks, loot
 * beams, lit surface relief, zoom sharpening and smooth animation.
 *
 * Pure arithmetic over synthetic 32-bit surfaces and a synthetic palette: no game archive is needed.
 */
#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>

#include "engine/animationinfo.h"
#include "engine/clx_sprite.hpp"
#include "engine/palette.h"
#include "engine/surface.hpp"
#include "items.h"
#include "levels/gendung.h"
#include "lighting.h"
#include "missiles.h"
#include "monster.h"
#include "nthread.h"
#include "objects.h"
#include "options.h"
#include "oracool/colour_light.h"
#include "oracool/frame_blend.h"
#include "oracool/ground_fx.h"
#include "oracool/missile_tint.h"
#include "oracool/particles.h"
#include "oracool/post_fx.h"

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

int R(uint32_t c) { return static_cast<int>((c >> 16) & 0xFF); }
int G(uint32_t c) { return static_cast<int>((c >> 8) & 0xFF); }
int B(uint32_t c) { return static_cast<int>(c & 0xFF); }

/** Saves and restores the options and level state a test here touches. */
struct Scene {
	dungeon_type savedType = leveltype;
	bool savedDayNight = *sgOptions.Oracool.dayNightCycle;
	bool savedMarks = *sgOptions.Oracool.floorMarks;
	bool savedBeams = *sgOptions.Oracool.lootBeams;
	bool savedRelief = *sgOptions.Oracool.surfaceRelief;
	bool savedBlend = *sgOptions.Oracool.animationBlending;
	bool savedShadows = *sgOptions.Oracool.rayTracedShadows;
	float savedHour = oracool::TownHour();
	uint8_t savedItems = ActiveItemCount;
	size_t savedMonsters = ActiveMonsterCount;
	int savedObjects = ActiveObjectCount;
	TileProperties savedFloor = SOLData[0];

	Scene()
	{
		SOLData[0] = TileProperties::None; // piece 0 is open floor here
		leveltype = DTYPE_CATHEDRAL;
		sgOptions.Oracool.dayNightCycle.SetValue(true);
		sgOptions.Oracool.floorMarks.SetValue(true);
		sgOptions.Oracool.lootBeams.SetValue(true);
		sgOptions.Oracool.surfaceRelief.SetValue(true);
		sgOptions.Oracool.animationBlending.SetValue(true);
		sgOptions.Oracool.rayTracedShadows.SetValue(false);
		ActiveItemCount = 0;
		ActiveMonsterCount = 0;
		ActiveObjectCount = 0;
		Missiles.clear();
		InitLighting();
		std::memset(dPiece, 0, sizeof(dPiece));
		std::memset(dLight, 0, sizeof(dLight));
		for (int i = 0; i < 256; i++)
			PaletteRGB[i] = Grey(static_cast<uint8_t>(i));
		for (size_t level = 0; level < NumLightingLevels; level++)
			for (int i = 0; i < 256; i++)
				LightTables[level][i] = static_cast<uint8_t>(i * (15 - static_cast<int>(level)) / 15);
		oracool::ForgetFloorMarks();
	}
	~Scene()
	{
		oracool::ForgetFloorMarks();
		Missiles.clear();
		InitLighting();
		ActiveItemCount = savedItems;
		ActiveMonsterCount = savedMonsters;
		ActiveObjectCount = savedObjects;
		sgOptions.Oracool.dayNightCycle.SetValue(savedDayNight);
		sgOptions.Oracool.floorMarks.SetValue(savedMarks);
		sgOptions.Oracool.lootBeams.SetValue(savedBeams);
		sgOptions.Oracool.surfaceRelief.SetValue(savedRelief);
		sgOptions.Oracool.animationBlending.SetValue(savedBlend);
		sgOptions.Oracool.rayTracedShadows.SetValue(savedShadows);
		oracool::SetTownHour(savedHour);
		SOLData[0] = savedFloor;
		leveltype = savedType;
		oracool::RebuildLightColourField();
	}
};

/** Where tile (x, y)'s centre lands when the first tile's 64x32 box has its bottom-left at @p firstBuffer. */
Point TileCentre(Point firstTile, Point firstBuffer, Point tile)
{
	const int dx = tile.x - firstTile.x;
	const int dy = tile.y - firstTile.y;
	return { firstBuffer.x + 32 + dx * 32 - dy * 32, firstBuffer.y - 15 + dx * 16 + dy * 16 };
}

} // namespace

// ----- Day and night ----------------------------------------------------------------------------------------------

TEST(OracoolDayNight, NoonIsUntouchedAndMidnightIsDarkBlue)
{
	const oracool::LightTint noon = oracool::TownAmbientAt(12.0F);
	EXPECT_EQ(noon.r, 256);
	EXPECT_EQ(noon.g, 256);
	EXPECT_EQ(noon.b, 256);
	const oracool::LightTint midnight = oracool::TownAmbientAt(0.0F);
	EXPECT_LT(midnight.r, 128);
	EXPECT_LT(midnight.g, 128);
	EXPECT_GT(midnight.b, midnight.r);
	EXPECT_GT(midnight.b, midnight.g);
}

TEST(OracoolDayNight, DuskIsAmberAndDawnRose)
{
	const oracool::LightTint dusk = oracool::TownAmbientAt(18.0F);
	EXPECT_GE(dusk.r, dusk.g);
	EXPECT_GT(dusk.g, dusk.b);
	const oracool::LightTint dawn = oracool::TownAmbientAt(6.0F);
	EXPECT_GT(dawn.r, dawn.g);
	EXPECT_GE(dawn.b, dawn.g);
}

TEST(OracoolDayNight, TheDayTurnsSmoothlyAndWrapsAtMidnight)
{
	for (float hour = 0.0F; hour < 24.0F; hour += 0.05F) {
		const oracool::LightTint a = oracool::TownAmbientAt(hour);
		const oracool::LightTint b = oracool::TownAmbientAt(hour + 0.05F);
		EXPECT_LE(std::abs(a.r - b.r), 12) << hour;
		EXPECT_LE(std::abs(a.g - b.g), 12) << hour;
		EXPECT_LE(std::abs(a.b - b.b), 12) << hour;
	}
	const oracool::LightTint zero = oracool::TownAmbientAt(0.0F);
	const oracool::LightTint day = oracool::TownAmbientAt(24.0F);
	EXPECT_EQ(zero.r, day.r);
	EXPECT_EQ(zero.b, day.b);
}

TEST(OracoolDayNight, TheTownDarkensAtNightAndNotAtNoon)
{
	Scene scene;
	leveltype = DTYPE_TOWN;
	OwnedSurface out = OwnedSurface::Rgb(320, 200);
	const Point firstTile { 40, 40 };
	const Point firstBuffer { 128, 64 };

	oracool::SetTownHour(12.0F);
	Fill(out, Grey(180));
	oracool::ApplyTownDayNight(out, firstTile, firstBuffer);
	EXPECT_EQ(Px(out, 160, 100), Grey(180));

	oracool::SetTownHour(0.0F);
	Fill(out, Grey(180));
	oracool::ApplyTownDayNight(out, firstTile, firstBuffer);
	const uint32_t night = Px(out, 160, 100);
	EXPECT_LT(R(night), 120);
	EXPECT_GT(B(night), R(night));

	// Off: the town is left as it is, whatever the hour.
	sgOptions.Oracool.dayNightCycle.SetValue(false);
	Fill(out, Grey(180));
	oracool::ApplyTownDayNight(out, firstTile, firstBuffer);
	EXPECT_EQ(Px(out, 160, 100), Grey(180));
}

TEST(OracoolDayNight, TheDungeonIsNeverTouched)
{
	Scene scene;
	leveltype = DTYPE_CATACOMBS;
	oracool::SetTownHour(0.0F);
	OwnedSurface out = OwnedSurface::Rgb(64, 64);
	Fill(out, Grey(180));
	oracool::ApplyTownDayNight(out, Point { 10, 10 }, Point { 0, 32 });
	EXPECT_EQ(Px(out, 32, 32), Grey(180));
}

// ----- Floor marks ------------------------------------------------------------------------------------------------

TEST(OracoolFloorMarks, ExplosionsLeaveTheirElementsMark)
{
	oracool::FloorMarkKind kind;
	float radius;
	ASSERT_TRUE(oracool::ExplosionMarkFor(oracool::hue::FireOrange, kind, radius));
	EXPECT_EQ(kind, oracool::FloorMarkKind::Scorch);
	ASSERT_TRUE(oracool::ExplosionMarkFor(oracool::hue::IceBlue, kind, radius));
	EXPECT_EQ(kind, oracool::FloorMarkKind::Frost);
	ASSERT_TRUE(oracool::ExplosionMarkFor(oracool::hue::PoisonGreen, kind, radius));
	EXPECT_EQ(kind, oracool::FloorMarkKind::Acid);
	ASSERT_TRUE(oracool::ExplosionMarkFor(oracool::hue::LightningBlue, kind, radius));
	EXPECT_EQ(kind, oracool::FloorMarkKind::Scorch);
	EXPECT_LT(radius, 0.6F); // a lightning burn is small
	EXPECT_FALSE(oracool::ExplosionMarkFor(oracool::hue::PaladinGold, kind, radius));
	EXPECT_FALSE(oracool::ExplosionMarkFor(oracool::SteelSparkRgb, kind, radius));
	EXPECT_FALSE(oracool::ExplosionMarkFor(0xFFFFFF, kind, radius));
}

TEST(OracoolFloorMarks, OneMarkASpotASecondAndThePoolIsCapped)
{
	Scene scene;
	oracool::AddFloorMark(20.0F, 20.0F, oracool::FloorMarkKind::Blood, 0.6F);
	oracool::AddFloorMark(20.2F, 20.1F, oracool::FloorMarkKind::Blood, 0.6F); // the same spot: throttled
	EXPECT_EQ(oracool::FloorMarkCount(), 1U);
	oracool::AddFloorMark(20.2F, 20.1F, oracool::FloorMarkKind::Scorch, 0.6F); // another kind lies over it
	EXPECT_EQ(oracool::FloorMarkCount(), 2U);
	for (int i = 0; i < 300; i++)
		oracool::AddFloorMark(static_cast<float>(1 + i % 100), static_cast<float>(1 + i / 100 * 3), oracool::FloorMarkKind::Scorch, 0.5F);
	EXPECT_EQ(oracool::FloorMarkCount(), oracool::MaxFloorMarks);
	oracool::ForgetFloorMarks();
	EXPECT_EQ(oracool::FloorMarkCount(), 0U);

	sgOptions.Oracool.floorMarks.SetValue(false);
	oracool::AddFloorMark(20.0F, 20.0F, oracool::FloorMarkKind::Blood, 0.6F);
	EXPECT_EQ(oracool::FloorMarkCount(), 0U);
}

TEST(OracoolFloorMarks, BloodReddensAndScorchDarkensTheFloorUnderIt)
{
	Scene scene;
	OwnedSurface out = OwnedSurface::Rgb(320, 200);
	const Point firstTile { 40, 40 };
	const Point firstBuffer { 128, 64 };
	const Point bloodAt = TileCentre(firstTile, firstBuffer, { 42, 42 });
	const Point scorchAt = TileCentre(firstTile, firstBuffer, { 44, 41 });
	oracool::AddFloorMark(42.0F, 42.0F, oracool::FloorMarkKind::Blood, 0.7F);
	oracool::AddFloorMark(44.0F, 41.0F, oracool::FloorMarkKind::Scorch, 0.7F);
	oracool::AgeFloorMarks(2.0F); // past blood's spreading and every mark's fade-in
	Fill(out, Grey(160));
	oracool::DrawFloorMarks(out, firstTile, firstBuffer);
	const uint32_t blood = Px(out, bloodAt.x, bloodAt.y);
	EXPECT_GT(R(blood), 2 * G(blood));
	EXPECT_LT(G(blood), 100);
	const uint32_t scorch = Px(out, scorchAt.x, scorchAt.y);
	EXPECT_LT(R(scorch), 100);
	// Far from both, the floor is the floor.
	EXPECT_EQ(Px(out, 10, 190), Grey(160));
	// A mark keeps the floor's light: on a black floor it stays black.
	Fill(out, 0);
	oracool::DrawFloorMarks(out, firstTile, firstBuffer);
	EXPECT_EQ(Px(out, bloodAt.x, bloodAt.y), 0U);
}

TEST(OracoolFloorMarks, AMarkNeverLiesOnAWall)
{
	Scene scene;
	OwnedSurface out = OwnedSurface::Rgb(320, 200);
	const Point firstTile { 40, 40 };
	const Point firstBuffer { 128, 64 };
	// Piece 1 solid, as a wall's is.
	const TileProperties savedProps = SOLData[1];
	SOLData[1] = TileProperties::Solid;
	dPiece[42][42] = 1;
	oracool::AddFloorMark(42.0F, 42.0F, oracool::FloorMarkKind::Blood, 0.7F);
	oracool::AgeFloorMarks(2.0F);
	Fill(out, Grey(160));
	oracool::DrawFloorMarks(out, firstTile, firstBuffer);
	const Point at = TileCentre(firstTile, firstBuffer, { 42, 42 });
	EXPECT_EQ(Px(out, at.x, at.y), Grey(160));
	SOLData[1] = savedProps;
	dPiece[42][42] = 0;
}

TEST(OracoolFloorMarks, MarksFadeAwayAndAreGone)
{
	Scene scene;
	oracool::AddFloorMark(20.0F, 20.0F, oracool::FloorMarkKind::Frost, 0.6F);
	EXPECT_EQ(oracool::FloorMarkCount(), 1U);
	oracool::AgeFloorMarks(500.0F); // longer than any mark lives
	EXPECT_EQ(oracool::FloorMarkCount(), 0U);
}

// ----- Loot beams -------------------------------------------------------------------------------------------------

TEST(OracoolLootBeams, TheBetterItemsStandInALight)
{
	devilution::Item item {};
	item._itype = ItemType::Sword;
	item._iMagical = ITEM_QUALITY_NORMAL;
	EXPECT_EQ(oracool::LootLookFor(item).rgb, 0U);
	item._iMagical = ITEM_QUALITY_MAGIC;
	const oracool::LootLook magic = oracool::LootLookFor(item);
	EXPECT_GT(B(magic.rgb), R(magic.rgb)); // the magic blue
	EXPECT_LT(magic.height, 1.0F);         // the smallest beam
	item._iMagical = ITEM_QUALITY_UNIQUE;
	EXPECT_NE(oracool::LootLookFor(item).rgb, 0U);
	item._iOracoolTier = OracoolItemTier::Set;
	const oracool::LootLook set = oracool::LootLookFor(item);
	EXPECT_GT(G(set.rgb), R(set.rgb)); // the set green
	item._iOracoolTier = OracoolItemTier::Rare;
	const oracool::LootLook rare = oracool::LootLookFor(item);
	EXPECT_NE(rare.rgb, 0U);
	EXPECT_LT(rare.height, set.height); // a rare's beam is the smaller
	devilution::Item none {};
	EXPECT_EQ(oracool::LootLookFor(none).rgb, 0U);
}

TEST(OracoolLootBeams, ATumblingOrUnlitItemShowsNoBeam)
{
	Scene scene;
	devilution::Item item {};
	item._itype = ItemType::Sword;
	item._iMagical = ITEM_QUALITY_UNIQUE;
	item.position = { 30, 30 };
	item._iAnimFlag = true;
	EXPECT_FALSE(oracool::LootShows(item));
	item._iAnimFlag = false;
	EXPECT_TRUE(oracool::LootShows(item));
	dLight[30][30] = LightsMax;
	EXPECT_FALSE(oracool::LootShows(item));
}

// ----- Lit surface relief -----------------------------------------------------------------------------------------

TEST(OracoolSurfaceRelief, TheLightPullsTowardItself)
{
	Scene scene;
	// A light at (30, 30): a tile three to its -x has it to its +x, which on screen is right and down (+32, +16 a tile).
	const int id = AddLight({ 30, 30 }, 8);
	ASSERT_NE(id, NO_LIGHT);
	oracool::RebuildLightColourField();
	int dx, dy;
	oracool::ReliefDirectionAt({ 27, 30 }, dx, dy);
	EXPECT_GT(dx, 40);
	// A tile three to its -y has it to its +y: left and down on screen.
	oracool::ReliefDirectionAt({ 30, 27 }, dx, dy);
	EXPECT_LT(dx, -40);
	// Under it: no pull, or a sprite standing in a light (the hero in his own) is lit from below (audit round 1).
	oracool::ReliefDirectionAt({ 30, 30 }, dx, dy);
	EXPECT_EQ(dx, 0);
	EXPECT_EQ(dy, 0);
	// Far from it: no pull.
	oracool::ReliefDirectionAt({ 70, 70 }, dx, dy);
	EXPECT_EQ(dx, 0);
	EXPECT_EQ(dy, 0);
}

TEST(OracoolSurfaceRelief, TheSideFacingTheLightBrightensAndTheFloorIsLeftAlone)
{
	Scene scene;
	const Point firstTile { 20, 20 };
	const Point firstBuffer { 128, 64 };
	// The light far to the +x of the wall, so its pull at the wall points right on screen.
	AddLight({ 30, 24 }, 12);
	oracool::RebuildLightColourField();
	int dx, dy;
	oracool::ReliefDirectionAt({ 24, 24 }, dx, dy);
	ASSERT_GT(dx, 0);

	OwnedSurface out = OwnedSurface::Rgb(320, 200);
	Fill(out, Grey(90));
	oracool::BeginSurfaceRelief(out);
	// A "wall" over the floor round tile (24, 24): a bump, bright in its middle column and darker either side - its left
	// slope faces left (away from the light), its right slope faces right (toward it).
	const Point c = TileCentre(firstTile, firstBuffer, { 24, 24 });
	for (int y = c.y - 20; y <= c.y + 20; y++) {
		Px(out, c.x - 2, y) = Grey(100);
		Px(out, c.x - 1, y) = Grey(140);
		Px(out, c.x, y) = Grey(180);
		Px(out, c.x + 1, y) = Grey(140);
		Px(out, c.x + 2, y) = Grey(100);
	}
	oracool::ApplySurfaceRelief(out, firstTile, firstBuffer);
	EXPECT_GT(R(Px(out, c.x + 1, c.y)), 140); // the right slope, toward the light
	EXPECT_LT(R(Px(out, c.x - 1, c.y)), 140); // the left slope, away from it
	EXPECT_EQ(Px(out, c.x + 40, c.y), Grey(90)); // the floor
}

TEST(OracoolSurfaceRelief, OffOrInTownItDoesNothing)
{
	Scene scene;
	AddLight({ 30, 24 }, 12);
	oracool::RebuildLightColourField();
	OwnedSurface out = OwnedSurface::Rgb(64, 64);
	sgOptions.Oracool.surfaceRelief.SetValue(false);
	EXPECT_FALSE(oracool::SurfaceReliefActive(out));
	sgOptions.Oracool.surfaceRelief.SetValue(true);
	EXPECT_TRUE(oracool::SurfaceReliefActive(out));
	leveltype = DTYPE_TOWN;
	EXPECT_FALSE(oracool::SurfaceReliefActive(out));
}

// ----- Zoom sharpening --------------------------------------------------------------------------------------------

TEST(OracoolZoomSharpen, ASoftEdgeComesOutCrisperAndFlatStaysFlat)
{
	OwnedSurface out = OwnedSurface::Rgb(16, 8);
	// A soft S-shaped edge (a straight ramp has nothing to sharpen): 80 to 160 over columns 6..8.
	const uint8_t ramp[16] = { 80, 80, 80, 80, 80, 80, 90, 130, 150, 160, 160, 160, 160, 160, 160, 160 };
	for (int y = 0; y < 8; y++)
		for (int x = 0; x < 16; x++)
			Px(out, x, y) = Grey(ramp[x]);
	oracool::SharpenAdaptive(reinterpret_cast<uint8_t *>(out.at<uint32_t>(0, 0)), out.pitch(), 16, 8);
	EXPECT_LT(R(Px(out, 6, 4)), 90);  // the dark foot of the edge darker
	EXPECT_GT(R(Px(out, 8, 4)), 150); // the bright shoulder brighter
	EXPECT_EQ(Px(out, 2, 4), Grey(80)); // flat
	EXPECT_EQ(Px(out, 13, 4), Grey(160));
}

TEST(OracoolZoomSharpen, AHardEdgeAtBlackAndWhiteIsLeftAlone)
{
	OwnedSurface out = OwnedSurface::Rgb(8, 6);
	for (int y = 0; y < 6; y++)
		for (int x = 0; x < 8; x++)
			Px(out, x, y) = x < 4 ? 0x000000 : 0xFFFFFF;
	oracool::SharpenAdaptive(reinterpret_cast<uint8_t *>(out.at<uint32_t>(0, 0)), out.pitch(), 8, 6);
	for (int x = 0; x < 8; x++)
		EXPECT_EQ(Px(out, x, 3), x < 4 ? 0x000000U : 0xFFFFFFU) << x; // no room to sharpen: no halo
}

// ----- Smooth animation -------------------------------------------------------------------------------------------

TEST(OracoolFrameBlend, TheAlphaFollowsTheFraction)
{
	EXPECT_EQ(oracool::FrameBlendAlphaForFraction(0), 0);
	EXPECT_EQ(oracool::FrameBlendAlphaForFraction(2), 0); // too little to show
	EXPECT_EQ(oracool::FrameBlendAlphaForFraction(64), 128);
	EXPECT_EQ(oracool::FrameBlendAlphaForFraction(96), 192);
	EXPECT_EQ(oracool::FrameBlendAlphaForFraction(AnimationInfo::baseValueFraction), 256); // a due tick shows the next whole
}

TEST(OracoolFrameBlend, TheFractionCountsTicksAndTheTickInProgress)
{
	const uint8_t savedProgress = ProgressToNextGameTick;
	AnimationInfo anim {};
	anim.setNewAnimation(std::nullopt, 8, 2);
	ProgressToNextGameTick = 64; // half a tick
	EXPECT_EQ(anim.getFrameBlendFraction(), 32); // a quarter of a two-tick frame
	anim.processAnimation();
	EXPECT_EQ(anim.currentFrame, 0);
	EXPECT_EQ(anim.getFrameBlendFraction(), 96); // one tick and a half of two
	anim.processAnimation();
	EXPECT_EQ(anim.currentFrame, 1);
	EXPECT_EQ(anim.getFrameBlendFraction(), 32);
	ProgressToNextGameTick = savedProgress;
}

TEST(OracoolFrameBlend, TheFrameLeftBehindFadesOutAsTheNextFadesIn)
{
	const std::array<uint8_t, 6> header { 6, 0, 4, 0, 3, 0 }; // a 4x3 sprite: only its size is read
	const ClxSprite sprite(header.data(), 6);
	OwnedSurface out = OwnedSurface::Rgb(10, 10);
	Fill(out, Grey(40));
	oracool::BeginFrameBlend(out, Point { 2, 6 }, sprite, Point { 3, 6 }, sprite); // rows 4..6, columns 2..6
	EXPECT_TRUE(oracool::InFrameBlendPass());
	Px(out, 2, 5) = Grey(200); // the first frame: only it covers (2, 5)
	Px(out, 4, 5) = Grey(200);
	oracool::SwapFrameBlend(out);
	EXPECT_EQ(Px(out, 2, 5), Grey(40)); // the ground put back for the second
	Px(out, 4, 5) = Grey(100); // the next frame: both cover (4, 5)
	Px(out, 6, 5) = Grey(240); // only it covers (6, 5)
	oracool::EndFrameBlend(out, 128);
	EXPECT_FALSE(oracool::InFrameBlendPass());
	EXPECT_EQ(Px(out, 2, 5), Grey(120)); // the frame left behind, half gone
	EXPECT_EQ(Px(out, 4, 5), Grey(150)); // half of each
	EXPECT_EQ(Px(out, 6, 5), Grey(140)); // the next, half come
	EXPECT_EQ(Px(out, 3, 4), Grey(40));  // neither: the ground
}

TEST(OracoolFrameBlend, NoBlendIntoTheFirstFrameOrWithoutASheet)
{
	AnimationInfo anim {};
	anim.setNewAnimation(std::nullopt, 8, 1);
	EXPECT_FALSE(oracool::PlanAnimationFrameBlend(anim).has_value()); // no sprites
}
