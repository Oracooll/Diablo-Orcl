/**
 * @file oracool_minions_test.cpp
 *
 * The army engine (Necromancer phase N4, v1.12.034): the pool above the enemies' 200, the user's group caps,
 * the thinking budget, and the rule that nothing of an army exists in town or outlives a game.
 */

#include <gtest/gtest.h>

#include "levels/gendung.h"
#include "monster.h"
#include "oracool/minions.h"
#include "player.h"

using namespace devilution;

TEST(OracoolMinions, TheArmyHasItsOwnPoolAboveTheEnemies)
{
	EXPECT_EQ(MaxEnemyMonsters, 200U) << "a floor keeps every monster slot it had before armies existed";
	EXPECT_EQ(MaxMinionBodies, 32U);
	EXPECT_EQ(MaxMonsters, MaxEnemyMonsters + MaxMinionBodies);
	EXPECT_LE(MaxMonsters, 255U) << "Monster::enemy is a uint8_t index into Monsters[]";
}

// Decision D7: 8 Skeletons, 8 Mages, 1 Golem, 10 Revived = 27, inside the pool of 32.
TEST(OracoolMinions, TheGroupCapsAreTheUsersAndFitThePool)
{
	EXPECT_EQ(oracool::MinionGroupCap(oracool::MinionGroup::Skeleton), 8);
	EXPECT_EQ(oracool::MinionGroupCap(oracool::MinionGroup::Mage), 8);
	EXPECT_EQ(oracool::MinionGroupCap(oracool::MinionGroup::Golem), 1);
	EXPECT_EQ(oracool::MinionGroupCap(oracool::MinionGroup::Revived), 10);
	int total = 0;
	for (size_t g = 0; g < oracool::MinionGroupCount; g++)
		total += oracool::MinionGroupCap(static_cast<oracool::MinionGroup>(g));
	EXPECT_EQ(total, 27);
	EXPECT_LE(static_cast<size_t>(total), MaxMinionBodies);
}

// An idle minion thinks on exactly one tick in three, and the army is spread evenly over the three.
TEST(OracoolMinions, TheThinkingBudgetIsAThirdAndEvenlySpread)
{
	int perPhase[3] = {};
	for (size_t id = MaxEnemyMonsters; id < MaxMonsters; id++) {
		int thinks = 0;
		for (uint32_t tick = 0; tick < 3; tick++) {
			if (oracool::MinionThinksThisTick(Monsters[id], tick)) {
				thinks++;
				perPhase[tick]++;
			}
		}
		EXPECT_EQ(thinks, 1) << "slot " << id;
	}
	for (const int count : perPhase)
		EXPECT_NEAR(count, static_cast<int>(MaxMinionBodies) / 3, 1);
}

TEST(OracoolMinions, NoArmyInTownAndNoneAfterForgetting)
{
	Players.resize(1);
	devilution::Player &player = Players[0];
	player = {};
	oracool::ForgetMinions();
	leveltype = DTYPE_TOWN;
	const oracool::MinionSpec spec { oracool::MinionGroup::Skeleton, MT_WSKELAX, 30, 2, 6, 60, 10 };
	EXPECT_FALSE(oracool::SummonMinion(player, spec, { 40, 40 }));
	EXPECT_EQ(oracool::MinionCount(player), 0);
	EXPECT_EQ(oracool::ActiveMinionBodies(), 0U);
	for (size_t id = 0; id < MaxMonsters; id++)
		EXPECT_FALSE(oracool::IsMinion(Monsters[id]));
	EXPECT_EQ(oracool::MinionOwner(Monsters[MaxMonsters - 1]), nullptr);
}

// ---- The Summoning page (phase N5, v1.12.035) ----

#include "oracool/corpses.h"
#include "oracool/hero_chunks.h"
#include "oracool/necro_summoning.h"
#include "oracool/readied_spells.h"
#include "oracool/class_skills.h"
#include "spells.h"

TEST(OracoolNecroSummoning, TheRaisedCountClimbsOneEveryThreeRanksToTheCap)
{
	EXPECT_EQ(oracool::RaisedCountAtRank(1, 8), 1);
	EXPECT_EQ(oracool::RaisedCountAtRank(3, 8), 1);
	EXPECT_EQ(oracool::RaisedCountAtRank(4, 8), 2);
	EXPECT_EQ(oracool::RaisedCountAtRank(22, 8), 8);
	EXPECT_EQ(oracool::RaisedCountAtRank(40, 8), 8) << "never past the cap";
	EXPECT_EQ(oracool::RaisedCountAtRank(28, 10), 10);
	EXPECT_EQ(oracool::RaisedCountAtRank(1, 1), 1);
}

TEST(OracoolNecroSummoning, ThirteenActivesAllHaveAnIdPastTheOldCeiling)
{
	int count = 0;
	for (int i = 0; i <= static_cast<int>(SpellID::LAST); i++) {
		if (oracool::IsNecromancerSummoning(static_cast<SpellID>(i)))
			count++;
	}
	EXPECT_EQ(count, 13);
	EXPECT_GT(static_cast<int>(SpellID::ArmyOfTheDead), 254) << "the page is what took the enum past a byte";
	EXPECT_EQ(static_cast<int>(SpellID::LAST) + 1, MAX_SPELLS);
}

// The ceilings the page broke through, each with the form that carries it now.
TEST(OracoolNecroSummoning, AReadiedSpellPastTheByteRoundTripsThroughTheTail)
{
	gbIsHellfire = true; // IsValidSpell admits the fork's ids only in Hellfire mode, as the game always runs
	const SpellID high = SpellID::ArmyOfTheDead;
	EXPECT_EQ(oracool::PackReadiedSpell(high), 0) << "the byte cannot carry it, and must not wrap into another spell";
	EXPECT_EQ(oracool::PackReadiedSpell16(high), static_cast<uint16_t>(static_cast<int>(high) + 1));
	EXPECT_NE(GetSpellBitmask(high), SpellMask {}) << "the fifth mask word";
	EXPECT_EQ(GetSpellBitmask(high) & GetSpellBitmask(SpellID::Firebolt), SpellMask {});

	Players.resize(1);
	devilution::Player &source = Players[0];
	source = {};
	source._pClass = HeroClass::Necromancer;
	source._pLevel = 30;
	source._pSkillInvestment[static_cast<size_t>(high)] = 3;
	source._pSkillInvestment[static_cast<size_t>(SpellID::RaiseSkeleton)] = 5;
	oracool::RefreshInnateSpells(source);
	ASSERT_NE(source._pAblSpells & GetSpellBitmask(high), SpellMask {}) << "test setup: the invested skill is known";
	source._pRSpell = high;
	source._pRSplType = SpellType::Skill;
	source._pSplHotKey[3] = high;
	source._pSplTHotKey[3] = SpellType::Skill;

	const std::vector<uint8_t> tail = oracool::BuildHeroChunkTail(source);
	devilution::Player loaded {};
	loaded._pClass = HeroClass::Necromancer;
	loaded._pLevel = 30;
	oracool::ApplyHeroChunks(loaded, tail.data(), tail.size());
	EXPECT_EQ(loaded._pSkillInvestment[static_cast<size_t>(high)], 3) << "the investment past entry 255 - tag 18";
	EXPECT_EQ(loaded._pSkillInvestment[static_cast<size_t>(SpellID::RaiseSkeleton)], 5);
	EXPECT_EQ(loaded._pRSpell, high) << "the readied spell - tag 15";
	EXPECT_EQ(loaded._pSplHotKey[3], high) << "the hotkey - tag 16";
}

TEST(OracoolNecroSummoning, TheCorpseTableStartsEmptyAndIsEmptyAgainOnALevel)
{
	oracool::ClearCorpses();
	EXPECT_EQ(oracool::CorpseCount(), 0);
	EXPECT_FALSE(oracool::CorpseNear({ 40, 40 }, 3, false));
	EXPECT_FALSE(oracool::TakeCorpseNear({ 40, 40 }, 3, true).has_value());
}

// The corpse table travels with its level (round 47 audit): a revisited floor drew its bodies and could raise none. What a
// saved table brings back is exactly what was recorded, less any body that is no longer on the floor.
TEST(OracoolNecroSummoning, TheCorpseTableComesBackWithItsLevel)
{
	oracool::ClearCorpses();
	const oracool::Corpse saved[] = {
		{ { 40, 40 }, MT_RFALLSP, 5, 30, 2, 6, 40, 10, true },
		{ { 42, 40 }, MT_NZOMBIE, 7, 45, 3, 8, 45, 12, true },
		{ { 44, 40 }, MT_SKING, 9, 900, 10, 30, 90, 70, false },
	};
	dCorpse[40][40] = 1;
	dCorpse[42][40] = 2;
	dCorpse[44][40] = 0; // eaten since: its record is dropped
	oracool::RestoreCorpseTable(saved, 3);
	EXPECT_EQ(oracool::CorpseCount(), 2);
	const std::optional<oracool::Corpse> zombie = oracool::TakeCorpseNear({ 42, 40 }, 0, true);
	ASSERT_TRUE(zombie.has_value());
	EXPECT_EQ(zombie->type, MT_NZOMBIE);
	EXPECT_EQ(zombie->maxLife, 45) << "the dead one's own numbers came back";

	// A peek leaves the body where it lies (round 48 audit: Raise and Revive took it before the summon could fail).
	const std::optional<oracool::Corpse> peeked = oracool::PeekCorpseNearSeen({ 40, 40 }, 0, { 40, 40 }, true);
	ASSERT_TRUE(peeked.has_value());
	EXPECT_EQ(peeked->type, MT_RFALLSP);
	EXPECT_EQ(oracool::CorpseCount(), 1) << "peeking took nothing";

	// Whatever a short or missing tail says, the last floor's table never survives a load.
	oracool::RestoreCorpseTable(nullptr, 0);
	EXPECT_EQ(oracool::CorpseCount(), 0);
	dCorpse[40][40] = 0;
	dCorpse[42][40] = 0;
}

// ---- The Necromancer Skill Cards page's Summoning notes (v1.12.363, 2026-10-03) ----

#include <array>
#include <cmath>
#include <cstring>

#include "oracool/aura_ground.h"
#include "oracool/class_tree.h"
#include "oracool/missile_tint.h"
#include "oracool/skill_sounds.h"

// "Vanilla golem, tinted in clay colour", "blood red", "iron colour ... colour cycling in random pattern"; the Fire Golem
// keeps its own look until its sprite is proposed.
TEST(OracoolNecro363, EachGolemWearsItsMaterial)
{
	const oracool::MinionLook clay = oracool::GolemLook(oracool::GolemKind::Clay);
	const oracool::MinionLook blood = oracool::GolemLook(oracool::GolemKind::Blood);
	const oracool::MinionLook iron = oracool::GolemLook(oracool::GolemKind::Iron);
	EXPECT_EQ(clay.tint, oracool::Tint::Hue);
	EXPECT_EQ(clay.rgb, oracool::hue::Clay);
	EXPECT_EQ(blood.tint, oracool::Tint::Hue);
	EXPECT_EQ(blood.rgb, oracool::hue::BloodRed);
	EXPECT_EQ(iron.tint, oracool::Tint::Sheen) << "iron glints at random";
	EXPECT_EQ(iron.rgb, oracool::hue::Iron);
	EXPECT_EQ(oracool::GolemLook(oracool::GolemKind::Fire).tint, oracool::Tint::None) << "the Fire Golem's sprite waits for a proposal";
	EXPECT_EQ(oracool::GolemLook(oracool::GolemKind::None).tint, oracool::Tint::None);
	// Blood is red, clay is warm, iron is cool.
	const auto r = [](uint32_t c) { return static_cast<int>((c >> 16) & 0xFF); };
	const auto g = [](uint32_t c) { return static_cast<int>((c >> 8) & 0xFF); };
	const auto b = [](uint32_t c) { return static_cast<int>(c & 0xFF); };
	EXPECT_GT(r(blood.rgb), 3 * g(blood.rgb));
	EXPECT_GT(r(clay.rgb), g(clay.rgb));
	EXPECT_GT(g(clay.rgb), b(clay.rgb));
	EXPECT_GT(b(iron.rgb), r(iron.rgb));
}

// "Tint bone tooth according to type of dmg each mage is causing ... regular skeleton sprites tinted in colour of magic".
TEST(OracoolNecro363, EachMageWearsItsElement)
{
	EXPECT_EQ(oracool::MageElementHue(MissileID::Firebolt), oracool::hue::FireOrange);
	EXPECT_EQ(oracool::MageElementHue(MissileID::ChargedBolt), oracool::hue::LightningBlue);
	EXPECT_EQ(oracool::MageElementHue(MissileID::Acid), oracool::hue::PoisonGreen);
	EXPECT_EQ(oracool::MageElementHue(MissileID::Arrow), oracool::hue::BoneWhite);
	for (const MissileID element : { MissileID::Firebolt, MissileID::ChargedBolt, MissileID::Acid, MissileID::Arrow }) {
		const oracool::MinionLook look = oracool::MageLook(element);
		EXPECT_EQ(look.tint, oracool::Tint::Shimmer) << "a subtle cycle of the same colour";
		EXPECT_EQ(look.rgb, oracool::MageElementHue(element));
	}
	// Four different colours: a line of mages is never one colour.
	const uint32_t hues[] = { oracool::hue::FireOrange, oracool::hue::LightningBlue, oracool::hue::PoisonGreen, oracool::hue::BoneWhite };
	for (size_t i = 0; i < 4; i++)
		for (size_t j = i + 1; j < 4; j++)
			EXPECT_NE(hues[i], hues[j]);
}

namespace {

std::array<uint32_t, 256> GreyRamp()
{
	std::array<uint32_t, 256> base {};
	for (uint32_t i = 0; i < 256; i++) {
		const uint32_t v = i * 110 / 255; // under half luma throughout: no run toward white, no channel clipped at 255
		base[i] = (v << 16) | (v << 8) | v;
	}
	return base;
}

} // namespace

// The mages' shimmer stays the element's colour: only brightness swings, by at most 12%, and black stays black.
TEST(OracoolNecro363, TheShimmerIsASubtleGradeOfTheSameHue)
{
	const std::array<uint32_t, 256> base = GreyRamp();
	const uint32_t hue = oracool::hue::FireOrange;
	std::array<uint32_t, 256> shimmer {};
	std::memcpy(shimmer.data(), oracool::TintedTable(base.data(), oracool::Tint::Shimmer, hue, 0.0), sizeof(shimmer));
	std::array<uint32_t, 256> plain {};
	std::memcpy(plain.data(), oracool::TintedTable(base.data(), oracool::Tint::Hue, hue, 0.0), sizeof(plain));
	EXPECT_EQ(shimmer[0], 0U) << "a shadow stays a shadow";
	for (size_t i = 64; i < 256; i++) {
		const double pr = static_cast<double>((plain[i] >> 16) & 0xFF);
		const double sr = static_cast<double>((shimmer[i] >> 16) & 0xFF);
		const double sg = static_cast<double>((shimmer[i] >> 8) & 0xFF);
		EXPECT_NEAR(sr / std::max(pr, 1.0), 1.0, 0.14) << "entry " << i << ": more than a subtle swing";
		// The hue's own red-to-green proportion, give or take rounding.
		EXPECT_NEAR(sg / std::max(sr, 1.0), 128.0 / 255.0, 0.05) << "entry " << i << ": another colour crept in";
	}
}

// The iron's sheen: black stays black, every entry stays cool iron, and the glints fall on different entries - a random
// pattern, not one band - so at any moment some entries are lit and most are not.
TEST(OracoolNecro363, TheIronSheenGlintsAtRandom)
{
	const std::array<uint32_t, 256> base = GreyRamp();
	std::array<uint32_t, 256> sheen {};
	std::memcpy(sheen.data(), oracool::TintedTable(base.data(), oracool::Tint::Sheen, oracool::hue::Iron, 0.0), sizeof(sheen));
	std::array<uint32_t, 256> plain {};
	std::memcpy(plain.data(), oracool::TintedTable(base.data(), oracool::Tint::Hue, oracool::hue::Iron, 0.0), sizeof(plain));
	EXPECT_EQ(sheen[0], 0U);
	int glinting = 0;
	for (size_t i = 1; i < 256; i++) {
		const int r = static_cast<int>((sheen[i] >> 16) & 0xFF);
		const int b = static_cast<int>(sheen[i] & 0xFF);
		EXPECT_GE(b + 2, r) << "entry " << i << ": iron is cool, never warm";
		EXPECT_GE(sheen[i] & 0xFF, plain[i] & 0xFF) << "entry " << i << ": a glint only brightens";
		if ((sheen[i] & 0xFF) > (plain[i] & 0xFF) + 8)
			glinting++;
	}
	EXPECT_GT(glinting, 0) << "no glint anywhere";
	EXPECT_LT(glinting, 200) << "the whole body flashed at once - a band, not a random sheen";
}

// "Small scale - 50% of paladin aura rings": the Paladin's ring is three half-tiles across (96x48).
TEST(OracoolNecro363, TheFrenzyRingIsHalfThePaladinsRing)
{
	EXPECT_EQ(oracool::FrenzyRingPercent, 50);
	const Size size = oracool::FrenzyRingSize();
	EXPECT_EQ(size.width, 48);
	EXPECT_EQ(size.height, 24);
	EXPECT_NE(oracool::AuraRingFileId(oracool::ClassTreeSkill::HolyFire), nullptr) << "the frenzy wears Holy Fire's ring";
}

// The ring and the looks are a minion's only: a monster that is no minion wears neither.
TEST(OracoolNecro363, OnlyAFrenziedMinionWearsTheRing)
{
	oracool::ForgetMinions();
	for (const size_t id : { size_t { 0 }, size_t { MaxEnemyMonsters }, size_t { MaxMonsters - 1 } }) {
		EXPECT_FALSE(oracool::IsMinionFrenzied(Monsters[id])) << "slot " << id;
		EXPECT_EQ(oracool::MinionBodyLook(Monsters[id]).tint, oracool::Tint::None) << "slot " << id;
	}
}

// The sounds the page picked: the golem grunt on a Clay, Blood or Iron Golem's landed blow, Firebolt on the Fire Golem's,
// and the Elemental's cue for casting it.
TEST(OracoolNecro363, TheGolemsPlayThePagesSounds)
{
	using oracool::ClassTreeSkill;
	using oracool::SkillSoundEvent;
	for (const ClassTreeSkill golem : { ClassTreeSkill::ClayGolem, ClassTreeSkill::BloodGolem, ClassTreeSkill::IronGolem }) {
		const char *impact = oracool::SkillSoundPath(golem, SkillSoundEvent::Impact);
		ASSERT_NE(impact, nullptr) << static_cast<int>(golem);
		EXPECT_STREQ(impact, "monsters\\golem\\golma1.wav");
		EXPECT_STREQ(oracool::SkillSoundPath(golem, SkillSoundEvent::Cast), "sfx\\misc\\golum.wav") << "its cast is unchanged";
	}
	ASSERT_NE(oracool::SkillSoundPath(ClassTreeSkill::FireGolem, SkillSoundEvent::Impact), nullptr);
	EXPECT_STREQ(oracool::SkillSoundPath(ClassTreeSkill::FireGolem, SkillSoundEvent::Impact), "sfx\\misc\\fbolt1.wav");
	EXPECT_STREQ(oracool::SkillSoundPath(ClassTreeSkill::FireGolem, SkillSoundEvent::Cast), "sfx\\misc\\elementl.wav");
}
