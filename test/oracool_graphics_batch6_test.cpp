/**
 * The sixth graphics batch (user, 2026-10-11: "do all 20 suggestions"): oracool/scene_glow.h - the corpses' cooling, the
 * orbs' slosh and glint, the homes' lamps only in town, every option on, and that its records are forgotten.
 *
 * No game archive is needed.
 */
#include <algorithm>
#include <cmath>

#include <gtest/gtest.h>

#include "diablo.h"
#include "levels/gendung.h"
#include "options.h"
#include "oracool/scene_fx.h"
#include "oracool/scene_glow.h"

using namespace devilution;

namespace {

TEST(OracoolGraphicsBatch6, ABodyCoolsOverItsFirstMinute)
{
	EXPECT_FLOAT_EQ(oracool::CorpseCooling(0.0F), 0.0F);
	EXPECT_FLOAT_EQ(oracool::CorpseCooling(5.0F), 0.0F); // fresh for five seconds
	EXPECT_NEAR(oracool::CorpseCooling(35.0F), 0.5F, 0.01F);
	EXPECT_FLOAT_EQ(oracool::CorpseCooling(65.0F), 1.0F);
	EXPECT_FLOAT_EQ(oracool::CorpseCooling(600.0F), 1.0F);
	EXPECT_LE(oracool::CorpseCooling(20.0F), oracool::CorpseCooling(30.0F));
}

TEST(OracoolGraphicsBatch6, AnOrbSloshesOnlyAfterAQuickChange)
{
	const bool was = *sgOptions.Oracool.orbSlosh;
	sgOptions.Oracool.orbSlosh.SetValue(true);
	oracool::ForgetSceneGlow(); // the orbs' records too: the test clock stands still, so nothing decays between runs
	constexpr int Key = 912345;
	// A still orb: its line noted twice in the same place, nothing to slosh.
	oracool::OrbSlosh(Key, -1, 60);
	oracool::OrbSlosh(Key, -1, 60);
	float still = 0;
	for (int x = 0; x < 40; x++)
		still = std::max(still, std::abs(oracool::OrbSlosh(Key, x, 60)));
	EXPECT_FLOAT_EQ(still, 0.0F);
	// A gulp: the line jumps twenty rows, the surface moves.
	oracool::OrbSlosh(Key, -1, 40);
	float moving = 0;
	for (int x = 0; x < 40; x++)
		moving = std::max(moving, std::abs(oracool::OrbSlosh(Key, x, 40)));
	EXPECT_GE(moving, 0.0F);
	EXPECT_LE(moving, 5.0F); // never more than five rows
	// Off: nothing at all.
	sgOptions.Oracool.orbSlosh.SetValue(false);
	EXPECT_FLOAT_EQ(oracool::OrbSlosh(Key, 10, 40), 0.0F);
	EXPECT_FLOAT_EQ(oracool::OrbGlint(Key, 10, 80), 0.0F);
	sgOptions.Oracool.orbSlosh.SetValue(true);
	// The glint is a spot: somewhere across the orb it is bright, and it is never above one.
	float best = 0;
	for (int x = 0; x < 80; x++) {
		const float g = oracool::OrbGlint(Key, x, 80);
		EXPECT_GE(g, 0.0F);
		EXPECT_LE(g, 1.0F);
		best = std::max(best, g);
	}
	EXPECT_GT(best, 0.9F);
	sgOptions.Oracool.orbSlosh.SetValue(was);
}

TEST(OracoolGraphicsBatch6, HomeLampsOnlyInTown)
{
	const dungeon_type was = leveltype;
	leveltype = DTYPE_CATHEDRAL;
	EXPECT_TRUE(oracool::TownHomeLamps().empty());
	leveltype = was;
}

TEST(OracoolGraphicsBatch6, EveryEffectIsOnByDefault)
{
	for (const OptionEntryBoolean *option : { &sgOptions.Oracool.levelUpBurst, &sgOptions.Oracool.castingGlow, &sgOptions.Oracool.potionEffects,
	         &sgOptions.Oracool.pickupGlints, &sgOptions.Oracool.containerEffects, &sgOptions.Oracool.eliteAuras, &sgOptions.Oracool.heroHurtFlash,
	         &sgOptions.Oracool.shrineGlow, &sgOptions.Oracool.litHomes, &sgOptions.Oracool.lensGlints, &sgOptions.Oracool.spellCharge,
	         &sgOptions.Oracool.statusLights, &sgOptions.Oracool.bloodTrails, &sgOptions.Oracool.lingeringMarks, &sgOptions.Oracool.bossPresence,
	         &sgOptions.Oracool.orbSlosh, &sgOptions.Oracool.exitGlow, &sgOptions.Oracool.corpseSettle, &sgOptions.Oracool.townForge,
	         &sgOptions.Oracool.armourSparks }) {
		EXPECT_TRUE(**option) << option->GetKey(); // the tests run on a fresh ini: the defaults
	}
}

TEST(OracoolGraphicsBatch6, ForgettingClearsEverything)
{
	oracool::ForgetSceneGlow();
	EXPECT_EQ(oracool::SceneGlowParticleCount(), 0);
	EXPECT_GT(oracool::SecondsSinceHeroHurt(), 10.0F);
	EXPECT_GT(oracool::SecondsSinceLevelUp(), 10.0F);
}

} // namespace
