/**
 * @file oracool_rage_test.cpp
 *
 * The Barbarian's Rage (v1.12.002): the user's generator/spender picks, the pool's bounds, the drain,
 * and the skill check that asks for Rage instead of mana.
 */

#include <gtest/gtest.h>

#include "oracool/rage.h"
#include "player.h"
#include "spells.h"

using namespace devilution;

namespace {

devilution::Player &FreshBarbarian()
{
	Players.resize(1);
	devilution::Player &player = Players[0];
	player._pClass = HeroClass::Barbarian;
	player._pHitPoints = 100 << 6;
	player._pMaxHP = 100 << 6;
	player._pmode = PM_STAND;
	oracool::ResetRage(player);
	return player;
}

} // namespace

// The user's ledger, 2026-09-13. Every Barbarian active is exactly one of the two - never both,
// never neither - and carries the number the user picked.
TEST(OracoolRage, EveryBarbarianActiveIsTheUsersPick)
{
	const std::pair<SpellID, int> generators[] = {
		{ SpellID::Bash, 6 }, { SpellID::Backhand, 6 }, { SpellID::Cleave, 6 }, { SpellID::DoubleSwing, 6 },
		{ SpellID::Stun, 7 }, { SpellID::Concentrate, 6 }, { SpellID::Frenzy, 6 }, { SpellID::ClaspOfRuin, 6 },
		{ SpellID::BerserkBlow, 6 },
	};
	const std::pair<SpellID, int> spenders[] = {
		{ SpellID::AncestralCall, 30 }, { SpellID::LeapAttack, 14 }, { SpellID::Rend, 9 }, { SpellID::SplitRanks, 9 },
		{ SpellID::BattleCommand, 10 }, { SpellID::BattleCry, 10 }, { SpellID::BattleOrders, 10 }, { SpellID::Bloodcall, 10 },
		{ SpellID::Earthquake, 10 }, { SpellID::EarthshakerCry, 10 }, { SpellID::FindItem, 10 }, { SpellID::FindPotion, 10 },
		{ SpellID::GrimWard, 10 }, { SpellID::GroundStomp, 10 }, { SpellID::HammerOfTheAncients, 10 }, { SpellID::Howl, 10 },
		{ SpellID::Intimidate, 10 }, { SpellID::IronWill, 10 }, { SpellID::Leap, 10 }, { SpellID::RallyingCry, 10 },
		{ SpellID::SeismicSlam, 10 }, { SpellID::Shout, 10 }, { SpellID::Taunt, 10 }, { SpellID::ThreateningShout, 10 },
		{ SpellID::WarCry, 10 }, { SpellID::Whirlwind, 10 },
	};
	for (const auto &[spell, gain] : generators) {
		EXPECT_EQ(oracool::RageGain(spell), gain) << "spell " << static_cast<int>(spell);
		EXPECT_EQ(oracool::RageCost(spell), 0) << "a generator must cost nothing: spell " << static_cast<int>(spell);
	}
	for (const auto &[spell, cost] : spenders) {
		EXPECT_EQ(oracool::RageCost(spell), cost) << "spell " << static_cast<int>(spell);
		EXPECT_EQ(oracool::RageGain(spell), 0) << "a spender must generate nothing: spell " << static_cast<int>(spell);
	}
}

TEST(OracoolRage, OnlyTheBarbarianUsesRage)
{
	EXPECT_TRUE(oracool::ClassUsesRage(HeroClass::Barbarian));
	for (HeroClass other : { HeroClass::Warrior, HeroClass::Rogue, HeroClass::Sorcerer, HeroClass::Monk, HeroClass::Bard })
		EXPECT_FALSE(oracool::ClassUsesRage(other));
}

TEST(OracoolRage, GeneratorsFillAndSpendersDrainWithinThePool)
{
	devilution::Player &player = FreshBarbarian();
	EXPECT_EQ(player._pRage, 0);
	EXPECT_EQ(oracool::MaxRage(player), oracool::BaseMaxRage);

	EXPECT_TRUE(oracool::CanPaySkill(player, SpellID::Bash)) << "a generator is always affordable";
	EXPECT_FALSE(oracool::CanPaySkill(player, SpellID::Whirlwind)) << "a spender needs Rage in the pool";

	oracool::SettleSkill(player, SpellID::Stun);
	EXPECT_EQ(player._pRage, 7);
	oracool::SettleSkill(player, SpellID::Bash);
	EXPECT_EQ(player._pRage, 13);
	EXPECT_TRUE(oracool::CanPaySkill(player, SpellID::Whirlwind));
	EXPECT_FALSE(oracool::CanPaySkill(player, SpellID::LeapAttack)) << "13 Rage is one short of 14";

	oracool::SettleSkill(player, SpellID::Whirlwind);
	EXPECT_EQ(player._pRage, 3);

	for (int i = 0; i < 40; i++)
		oracool::SettleSkill(player, SpellID::Frenzy);
	EXPECT_EQ(player._pRage, oracool::BaseMaxRage) << "the pool never overfills";
}

TEST(OracoolRage, TheSkillCheckAsksForRageNotMana)
{
	devilution::Player &player = FreshBarbarian();
	player._pMana = 1000 << 6; // mana in plenty must not buy a Barbarian skill
	EXPECT_EQ(CheckSpell(player, SpellID::AncestralCall, SpellType::Skill, /*manaonly=*/true), SpellCheckResult::Fail_NoMana);
	oracool::GainRage(player, 30);
	EXPECT_EQ(CheckSpell(player, SpellID::AncestralCall, SpellType::Skill, /*manaonly=*/true), SpellCheckResult::Success);
}

TEST(OracoolRage, RageDrainsOnlyAfterThreeQuietSeconds)
{
	devilution::Player &player = FreshBarbarian();
	oracool::GainRage(player, 20);
	for (int i = 0; i < oracool::RageDecayDelayTicks; i++)
		oracool::ProcessRageTick(player);
	EXPECT_EQ(player._pRage, 20) << "no drain inside the delay";

	for (int i = 0; i < oracool::RageDecayIntervalTicks * 4; i++)
		oracool::ProcessRageTick(player);
	EXPECT_EQ(player._pRage, 16) << "one point per interval once it starts";

	oracool::GainRage(player, 1); // combat again: the clock restarts
	for (int i = 0; i < oracool::RageDecayDelayTicks; i++)
		oracool::ProcessRageTick(player);
	EXPECT_EQ(player._pRage, 17);
}

TEST(OracoolRage, ANewLevelStartsEmpty)
{
	devilution::Player &player = FreshBarbarian();
	oracool::GainRage(player, 50);
	oracool::ResetRage(player);
	EXPECT_EQ(player._pRage, 0);
	EXPECT_EQ(player._pRageIdleTicks, 0);
}
