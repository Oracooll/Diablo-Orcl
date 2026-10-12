/**
 * @file oracool_minions_test.cpp
 *
 * The army engine (Necromancer phase N4, v1.12.034): the pool above the enemies' 200, the user's group caps,
 * the thinking budget, and the rule that nothing of an army exists in town or outlives a game.
 */

#include <gtest/gtest.h>

#include "options.h"
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
	// The Elemental-sheet golem is the Drawn Skills off; with them on it is a yellow golem burning (DrawnFireGolemIsYellow).
	struct DrawnSkillsOff {
		bool saved = *sgOptions.Oracool.drawnSkills;
		DrawnSkillsOff() { sgOptions.Oracool.drawnSkills.SetValue(false); }
		~DrawnSkillsOff() { sgOptions.Oracool.drawnSkills.SetValue(saved); }
	} drawnOff;
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

// ---- The Fire Golem as the Hellfire Elemental, the user's LUNGE pick (v1.12.364) ----

#include "engine/animationinfo.h"
#include "missiles.h"

namespace {

/** @brief Ticks an animation started as a monster's step starts (NewMonsterAnim's walk: pending, one frame back) runs to its last frame. */
int TicksToLastFrame(int frames, int rate, int8_t skipped)
{
	AnimationInfo anim {};
	anim.setNewAnimation(std::nullopt, static_cast<int8_t>(frames), static_cast<int8_t>(rate), AnimationDistributionFlags::ProcessAnimationPending, skipped);
	int ticks = 0;
	while (!anim.isLastFrame() && ticks < 1000) {
		anim.processAnimation();
		ticks++;
	}
	return ticks;
}

} // namespace

// "Reduce its movement speed a bit": a tile takes the run sheet's 12 frames at two ticks against the vanilla golem's 16 at
// one - 2/3 of the pace - and the step, which ends on the last frame, is the same animation the body shows.
TEST(OracoolNecro364, TheFireGolemWalksAtTwoThirdsTheGolemsPace)
{
	const MonsterData &golem = MonstersData[MT_GOLEM];
	ASSERT_EQ(golem.frames[1], 16) << "the vanilla golem's walk";
	ASSERT_EQ(golem.rate[1], 1);
	const oracool::FireGolemTiming walk = oracool::FireGolemAnimTiming(MonsterGraphic::Walk);
	EXPECT_EQ(walk.frames, 12) << "the Elemental's run sheet has twelve frames a facing";
	EXPECT_EQ(walk.rate, 2);
	const int vanillaTicks = TicksToLastFrame(golem.frames[1], golem.rate[1], -1);
	const int fireTicks = TicksToLastFrame(walk.frames, walk.rate, -1);
	EXPECT_EQ(vanillaTicks, 16);
	EXPECT_EQ(fireTicks, 24);
	EXPECT_NEAR(static_cast<double>(vanillaTicks) / fireTicks, 2.0 / 3.0, 0.01);
	// It stands on its walk (M_StartStand), so the idle loop is the same slowed run.
	EXPECT_EQ(oracool::FireGolemAnimTiming(MonsterGraphic::Stand).rate, walk.rate);
}

// The blow keeps its timing: the attack runs the sheet at one tick a frame, the golem's own 12 ticks, and the golem's hit
// frame (animFrameNum 7) is on the sheet with frames to spare.
TEST(OracoolNecro364, TheFireGolemsBlowLandsWhenItAlwaysDid)
{
	const MonsterData &golem = MonstersData[MT_GOLEM];
	const oracool::FireGolemTiming attack = oracool::FireGolemAnimTiming(MonsterGraphic::Attack);
	EXPECT_EQ(attack.frames * attack.rate, golem.frames[2] * golem.rate[2]) << "the attack lasts what the golem's did";
	EXPECT_EQ(golem.animFrameNum, 7);
	EXPECT_LT(golem.animFrameNum, attack.frames);
	EXPECT_EQ(oracool::FireGolemAnimTiming(MonsterGraphic::Death).frames, 12);
}

// The lunge: out to half a tile at the middle of the attack (the hit frame) and back, easing, the same on the way back as
// on the way out, and nothing at either end - the body ends where it started.
TEST(OracoolNecro364, TheLungeSurgesHalfATileAndComesBack)
{
	constexpr int End = AnimationInfo::baseValueFraction;
	EXPECT_NEAR(oracool::FireGolemLungeFraction(0), 0.0, 1e-9);
	EXPECT_NEAR(oracool::FireGolemLungeFraction(End), 0.0, 1e-9);
	EXPECT_NEAR(oracool::FireGolemLungeFraction(End / 2), 1.0, 1e-9);
	EXPECT_NEAR(oracool::FireGolemLungeFraction(-20), 0.0, 1e-9) << "clamped";
	EXPECT_NEAR(oracool::FireGolemLungeFraction(End + 20), 0.0, 1e-9) << "clamped";
	double last = -1.0;
	for (int p = 0; p <= End; p++) {
		EXPECT_NEAR(oracool::FireGolemLungeFraction(p), oracool::FireGolemLungeFraction(End - p), 1e-9) << "progress " << p;
		const double f = oracool::FireGolemLungeFraction(p);
		if (p <= End / 2) {
			EXPECT_GE(f, last) << "out all the way to the middle, progress " << p;
			last = f;
		}
	}
	EXPECT_GT(oracool::FireGolemLungeFraction(End / 4), 0.6) << "eases out: fast first, slow at the peak";
	// The golem's hit frame (7 of 12) falls on the lunge's peak region.
	EXPECT_GT(oracool::FireGolemLungeFraction(End * 6 / 12), 0.95);

	// As a screen offset: half a tile toward the facing at the peak, nothing at the ends, mirror-equal around the middle.
	EXPECT_EQ(oracool::FireGolemLungeOffset(Direction::South, End / 2), (Displacement { 0, 16 }));
	EXPECT_EQ(oracool::FireGolemLungeOffset(Direction::North, End / 2), (Displacement { 0, -16 }));
	EXPECT_EQ(oracool::FireGolemLungeOffset(Direction::East, End / 2), (Displacement { 32, 0 }));
	EXPECT_EQ(oracool::FireGolemLungeOffset(Direction::West, End / 2), (Displacement { -32, 0 }));
	EXPECT_EQ(oracool::FireGolemLungeOffset(Direction::SouthEast, End / 2), (Displacement { 16, 8 }));
	EXPECT_EQ(oracool::FireGolemLungeOffset(Direction::NorthWest, End / 2), (Displacement { -16, -8 }));
	for (int d = 0; d < 8; d++) {
		const auto facing = static_cast<Direction>(d);
		EXPECT_EQ(oracool::FireGolemLungeOffset(facing, 0), (Displacement { 0, 0 })) << d;
		EXPECT_EQ(oracool::FireGolemLungeOffset(facing, End), (Displacement { 0, 0 })) << d;
		for (int p = 0; p <= End; p += 8)
			EXPECT_EQ(oracool::FireGolemLungeOffset(facing, p), oracool::FireGolemLungeOffset(facing, End - p)) << d << " at " << p;
	}
	EXPECT_EQ(oracool::FireGolemLungeOffset(Direction::NoDirection, End / 2), (Displacement { 0, 0 }));
}

// The three bursts: a small splash on the target, the Apocalypse boom to rise in, the Elemental's own explosion to end in -
// the splash smaller than the end.
TEST(OracoolNecro364, TheFireGolemsBurstsAreSmallSplashFlareAndTheElementalsEnd)
{
	EXPECT_EQ(oracool::FireGolemDeathArt, MissileGraphicID::BigExplosion) << "the missile the Elemental spell ends in";
	EXPECT_EQ(oracool::FireGolemSummonArt, MissileGraphicID::ApocalypseBoom);
	const MissileGraphicID strike = oracool::FireGolemStrikeArt();
	EXPECT_TRUE(strike == MissileGraphicID::HitFire || strike == MissileGraphicID::MagmaBallExplosion);
	if (!MissileArtLoaded(MissileGraphicID::HitFire))
		EXPECT_EQ(strike, MissileGraphicID::MagmaBallExplosion) << "the magma ball's burst stands in for hit_fire.png";
	EXPECT_LT(GetMissileSpriteData(strike).animWidth, GetMissileSpriteData(oracool::FireGolemDeathArt).animWidth) << "a splash, not the big explosion";
	EXPECT_LT(GetMissileSpriteData(oracool::FireGolemSummonArt).animWidth, GetMissileSpriteData(oracool::FireGolemDeathArt).animWidth);
}

// None of it touches anything that is no Fire Golem: no other sheet, no lunge, no hidden body, a corpse as before, no burst.
TEST(OracoolNecro364, NothingButAFireGolemWearsTheElemental)
{
	// The Elemental-sheet golem is the Drawn Skills off; with them on it is a yellow golem burning (DrawnFireGolemIsYellow).
	struct DrawnSkillsOff {
		bool saved = *sgOptions.Oracool.drawnSkills;
		DrawnSkillsOff() { sgOptions.Oracool.drawnSkills.SetValue(false); }
		~DrawnSkillsOff() { sgOptions.Oracool.drawnSkills.SetValue(saved); }
	} drawnOff;
	oracool::ForgetMinions();
	EXPECT_EQ(oracool::GolemLook(oracool::GolemKind::Fire).tint, oracool::Tint::None) << "the sheet is fire already: no tint";
	for (const size_t id : { size_t { 0 }, size_t { MaxEnemyMonsters }, size_t { MaxMonsters - 1 } }) {
		Monster &monster = Monsters[id];
		monster.mode = MonsterMode::MeleeAttack;
		EXPECT_FALSE(oracool::IsFireGolem(monster)) << id;
		EXPECT_EQ(oracool::GetMinionAnim(monster, MonsterGraphic::Walk), nullptr) << id;
		EXPECT_EQ(oracool::MinionLungeOffset(monster), (Displacement { 0, 0 })) << id;
		EXPECT_FALSE(oracool::MinionDrawsUnlit(monster)) << id;
		EXPECT_FALSE(oracool::MinionLeavesNoBody(monster)) << id;
		monster.mode = MonsterMode::Death;
		EXPECT_FALSE(oracool::MinionBodyHidden(monster)) << id;
		const size_t before = Missiles.size();
		oracool::OnMinionDeathStarted(monster);
		oracool::OnMinionStrikeFrame(monster);
		EXPECT_EQ(Missiles.size(), before) << id;
		monster.mode = MonsterMode::Stand;
	}
}

// ---- The Necromancer Skill Cards page's notes on Amplify Damage and Unholy Offering (v1.12.369, 2026-10-03) ----

#include <cstdlib>
#include <iterator>

#include "oracool/curses.h"
#include "spelldat.h"

namespace {

constexpr oracool::CurseKind EveryCurse[] = {
	oracool::CurseKind::AmplifyDamage, oracool::CurseKind::DimVision, oracool::CurseKind::Weaken, oracool::CurseKind::Frailty,
	oracool::CurseKind::IronMaiden, oracool::CurseKind::Terror, oracool::CurseKind::Bane, oracool::CurseKind::Confuse,
	oracool::CurseKind::LifeTap, oracool::CurseKind::Attract, oracool::CurseKind::Decrepify, oracool::CurseKind::DeathMark,
	oracool::CurseKind::LowerResist, oracool::CurseKind::Doom
};

int Red(uint32_t c) { return static_cast<int>((c >> 16) & 0xFF); }
int Green(uint32_t c) { return static_cast<int>((c >> 8) & 0xFF); }
int Blue(uint32_t c) { return static_cast<int>(c & 0xFF); }

} // namespace

// "Tint all curses from this skill sheet appropriately. Lets have a number of colours for sake of diversity": every one of
// the fourteen curses that draws the cast ring has a colour, and no two share one - nor come close to it.
TEST(OracoolNecro369, EveryCurseCastRingHasItsOwnColour)
{
	EXPECT_EQ(oracool::CurseCastHue(oracool::CurseKind::None), 0U);
	EXPECT_EQ(std::size(EveryCurse), static_cast<size_t>(oracool::CurseKind::Doom)) << "every curse kind, None aside";
	for (size_t i = 0; i < std::size(EveryCurse); i++) {
		const uint32_t a = oracool::CurseCastHue(EveryCurse[i]);
		EXPECT_NE(a, 0U) << oracool::CurseName(EveryCurse[i]) << " has no colour";
		for (size_t j = i + 1; j < std::size(EveryCurse); j++) {
			const uint32_t b = oracool::CurseCastHue(EveryCurse[j]);
			const int distance = std::abs(Red(a) - Red(b)) + std::abs(Green(a) - Green(b)) + std::abs(Blue(a) - Blue(b));
			EXPECT_GE(distance, 60) << oracool::CurseName(EveryCurse[i]) << " and " << oracool::CurseName(EveryCurse[j]) << " look alike";
		}
	}
}

// The colours fit the curses: Amplify Damage blood red, Frailty bone, Bane green rot, Lower Resist cyan, Dim Vision violet,
// Doom orange, Death Mark near black.
TEST(OracoolNecro369, TheCurseColoursFitTheirCurses)
{
	using oracool::CurseKind;
	EXPECT_EQ(oracool::CurseCastHue(CurseKind::AmplifyDamage), oracool::hue::BloodRed);
	EXPECT_EQ(oracool::CurseCastHue(CurseKind::Frailty), oracool::hue::BoneWhite);
	const uint32_t bane = oracool::CurseCastHue(CurseKind::Bane);
	EXPECT_GT(Green(bane), 2 * Red(bane));
	const uint32_t cyan = oracool::CurseCastHue(CurseKind::LowerResist);
	EXPECT_GT(Blue(cyan), 2 * Red(cyan));
	EXPECT_GT(Green(cyan), 2 * Red(cyan));
	const uint32_t violet = oracool::CurseCastHue(CurseKind::DimVision);
	EXPECT_GT(Blue(violet), Red(violet));
	EXPECT_GT(Red(violet), Green(violet));
	const uint32_t doom = oracool::CurseCastHue(CurseKind::Doom);
	EXPECT_GT(Red(doom), 2 * Green(doom));
	EXPECT_GT(Green(doom), Blue(doom));
	const uint32_t mark = oracool::CurseCastHue(CurseKind::DeathMark);
	EXPECT_LT(Red(mark), Red(oracool::hue::BloodRed)) << "darker than Amplify Damage's red";
	EXPECT_LT(Green(mark) + Blue(mark), 64);
	// Every curse spell finds its kind, so the ring is tinted for each; Soul Harvest lays none and draws no ring.
	EXPECT_EQ(oracool::CurseKindOf(SpellID::AmplifyDamage), CurseKind::AmplifyDamage);
	EXPECT_EQ(oracool::CurseKindOf(SpellID::NecroWeaken), CurseKind::Weaken);
	EXPECT_EQ(oracool::CurseKindOf(SpellID::NecroIronMaiden), CurseKind::IronMaiden);
	EXPECT_EQ(oracool::CurseKindOf(SpellID::Doom), CurseKind::Doom);
	EXPECT_EQ(oracool::CurseKindOf(SpellID::SoulHarvest), CurseKind::None);
}

// Only the Fire Golem trades its type's death cry for a sound of its own; every other body cries its own.
TEST(OracoolNecro369, OnlyTheFireGolemDiesWithAnotherSound)
{
	oracool::ForgetMinions();
	for (const size_t id : { size_t { 0 }, size_t { MaxEnemyMonsters }, size_t { MaxMonsters - 1 } })
		EXPECT_EQ(oracool::MinionDeathSfx(Monsters[id]), SFX_NONE) << id;
}
