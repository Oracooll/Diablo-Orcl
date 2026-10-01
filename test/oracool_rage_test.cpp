/**
 * @file oracool_rage_test.cpp
 *
 * The Barbarian's Rage (v1.12.002): the user's generator/spender picks, the pool's bounds, the drain,
 * and the skill check that asks for Rage instead of mana.
 */

#include <gtest/gtest.h>

#include <string>

#include "misdat.h"
#include "oracool/class_tree.h"
#include "oracool/melee_skills.h"
#include "oracool/passives.h"
#include "oracool/rage.h"
#include "oracool/rfa12_actives.h"
#include "oracool/whirlwind.h"
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
		{ SpellID::WarCry, 10 },
		// Whirlwind: 5 to START the spin, which then drains 5 a second (oracool/whirlwind.h, 2026-09-29).
		{ SpellID::Whirlwind, 5 },
		{ SpellID::WeaponThrow, 5 }, // dev note, 2026-09-29
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

	oracool::SettleSkill(player, SpellID::BattleCry);
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

// User, 2026-09-14: five seconds of lingering fury after the last swing at a monster, then one point
// a second - and none of it while the swinging goes on.
TEST(OracoolRage, RageHoldsThroughCombatAndThenDrainsOneASecond)
{
	devilution::Player &player = FreshBarbarian();
	EXPECT_EQ(oracool::RageCalmDelayTicks, 100) << "five seconds at 20 ticks";
	EXPECT_EQ(oracool::RageDecayIntervalTicks, 20) << "one point a second";
	oracool::GainRage(player, 20);

	// Swinging at monsters every half second for a minute: not a point lost.
	for (int i = 0; i < 1200; i++) {
		if (i % 10 == 0)
			oracool::NoteRageCombat(player);
		oracool::ProcessRageTick(player);
	}
	EXPECT_EQ(player._pRage, 20) << "the pool drained while the fighting went on";

	// Stopped: the five-second fury, then the first point goes after one more second.
	oracool::NoteRageCombat(player);
	for (int i = 0; i < oracool::RageCalmDelayTicks + oracool::RageDecayIntervalTicks - 1; i++)
		oracool::ProcessRageTick(player);
	EXPECT_EQ(player._pRage, 20) << "drained inside the five-second fury";
	oracool::ProcessRageTick(player);
	EXPECT_EQ(player._pRage, 19);
	for (int i = 0; i < oracool::RageDecayIntervalTicks * 4; i++)
		oracool::ProcessRageTick(player);
	EXPECT_EQ(player._pRage, 15) << "one point a second once calm";

	// Back into a fight: the drain stops at once.
	oracool::NoteRageCombat(player);
	for (int i = 0; i < oracool::RageCalmDelayTicks; i++)
		oracool::ProcessRageTick(player);
	EXPECT_EQ(player._pRage, 15);
}

TEST(OracoolRage, EveryLandedBlowOfAGeneratorEarnsRage)
{
	devilution::Player &player = FreshBarbarian();
	oracool::SettleSkill(player, SpellID::DoubleSwing, 2);
	EXPECT_EQ(player._pRage, 12) << "both blows of a Double Swing earn";
	oracool::SettleSkill(player, SpellID::Cleave, 3);
	EXPECT_EQ(player._pRage, 30) << "a Cleave through three earns three times";
	oracool::SettleSkill(player, SpellID::Bash, 0);
	EXPECT_EQ(player._pRage, 30) << "a blow that struck nothing earns nothing";
}

TEST(OracoolRage, SpendingIsNotFighting)
{
	devilution::Player &player = FreshBarbarian();
	oracool::GainRage(player, 50);
	for (int i = 0; i < oracool::RageCalmDelayTicks - 1; i++)
		oracool::ProcessRageTick(player);
	oracool::SettleSkill(player, SpellID::Shout, 0); // a shout into an empty room
	EXPECT_EQ(player._pRage, 40);
	for (int i = 0; i < oracool::RageDecayIntervalTicks + 1; i++)
		oracool::ProcessRageTick(player);
	EXPECT_EQ(player._pRage, 39) << "a shout restarted the calm clock";
}

// User, 2026-09-14: "move unforgiving passive to lvl10 slot and develop it".
TEST(OracoolRage, UnforgivingIsTheLevelTenPassive)
{
	EXPECT_EQ(oracool::PassiveSkillRequiredLevel(oracool::ClassTreeSkill::Unforgiving), 10);
	EXPECT_TRUE(oracool::GetClassTreeSkillData(oracool::ClassTreeSkill::Unforgiving).implemented);
	EXPECT_EQ(oracool::PassiveSkillRequiredLevel(oracool::ClassTreeSkill::InspiringPresence), 30) << "the two swapped cells";
	EXPECT_EQ(oracool::PassiveSkillRequiredLevel(oracool::ClassTreeSkill::Rampage), 36) << "the last cell did not move";
}

// The Barbarian's passive page, fully built (2026-09-14).
TEST(OracoolRage, EveryBarbarianPassiveOnThePageIsBuilt)
{
	for (oracool::ClassTreeSkill skill : { oracool::ClassTreeSkill::PoundOfFlesh, oracool::ClassTreeSkill::WeaponsMaster,
	         oracool::ClassTreeSkill::InspiringPresence, oracool::ClassTreeSkill::BerserkerRage, oracool::ClassTreeSkill::NoEscape,
	         oracool::ClassTreeSkill::Juggernaut, oracool::ClassTreeSkill::EarthenMight, oracool::ClassTreeSkill::Unforgiving }) {
		const oracool::ClassTreeSkillData &data = oracool::GetClassTreeSkillData(skill);
		EXPECT_TRUE(data.implemented) << data.name;
		EXPECT_EQ(std::string(data.description).find("Not yet built"), std::string::npos) << data.name;
	}
}

TEST(OracoolRage, JuggernautAndInspiringPresenceAnswerOnlyWhenSlotted)
{
	devilution::Player &player = FreshBarbarian();
	player._pLevel = 36;
	for (int slot = 0; slot < static_cast<int>(oracool::PassiveSlotCount); slot++)
		oracool::ClearPassiveSlot(player, slot);
	EXPECT_EQ(oracool::PassiveSlowShortenPercent(player), 0);
	EXPECT_EQ(oracool::PassiveWarcryDurationPercent(player), 100);

	ASSERT_TRUE(oracool::SetPassiveSlot(player, 0, oracool::ClassTreeSkill::Juggernaut));
	ASSERT_TRUE(oracool::SetPassiveSlot(player, 1, oracool::ClassTreeSkill::InspiringPresence));
	EXPECT_EQ(oracool::PassiveSlowShortenPercent(player), 50);
	EXPECT_EQ(oracool::PassiveWarcryDurationPercent(player), 200);

	for (int slot = 0; slot < static_cast<int>(oracool::PassiveSlotCount); slot++)
		oracool::ClearPassiveSlot(player, slot);
}

TEST(OracoolRage, ANewLevelStartsEmpty)
{
	devilution::Player &player = FreshBarbarian();
	oracool::GainRage(player, 50);
	oracool::ResetRage(player);
	EXPECT_EQ(player._pRage, 0);
	EXPECT_EQ(player._pRageIdleTicks, 0);
}

// Whirlwind, held on the right button (the Barbarian Skill Cards page, 2026-09-29): its blow share, its button, its price.
TEST(OracoolRage, WhirlwindIsHeldOnTheRightButton)
{
	EXPECT_EQ(oracool::WhirlwindDamagePercent(1), 66);
	EXPECT_EQ(oracool::WhirlwindDamagePercent(5), 86) << "+5% a rank";
	// A share of the whole blow, not a term of the pool (round 58 audit): the sheet's readied line reads it as a swing.
	{
		devilution::Player player {};
		EXPECT_EQ(oracool::ClassMeleeSkillBonusPercentFor(player, SpellID::Whirlwind), 0) << "below zero read as not a swing";
		EXPECT_EQ(oracool::ClassMeleeSkillSharePercentFor(player, SpellID::Whirlwind), oracool::WhirlwindDamagePercent(1));
		EXPECT_EQ(oracool::ClassMeleeSkillSharePercentFor(player, SpellID::Bash), 100);
		EXPECT_EQ(oracool::ClassMeleeSkillSharePercent(player), 100) << "nothing armed, nothing spinning: the whole blow";
	}
	EXPECT_TRUE(oracool::RightButtonOnly(SpellID::Whirlwind));
	EXPECT_TRUE(oracool::RightButtonOnly(SpellID::Earthquake)) << "the Barbarian Skill Cards page, 2026-09-29";
	EXPECT_FALSE(oracool::RightButtonOnly(SpellID::Bash));
	// The dev notes of the same day: Leap, Ground Stomp, Rend and every active on the Warcries page are spells.
	for (const SpellID spell : { SpellID::Leap, SpellID::GroundStomp, SpellID::Rend, SpellID::Howl, SpellID::FindPotion,
	         SpellID::BattleCry, SpellID::BattleCommand, SpellID::WarCry, SpellID::ThreateningShout, SpellID::EarthshakerCry })
		EXPECT_TRUE(oracool::RightButtonOnly(spell)) << "spell " << static_cast<int>(spell);
	for (const SpellID spell : { SpellID::LeapAttack, SpellID::Frenzy, SpellID::WeaponThrow, SpellID::Cleave })
		EXPECT_FALSE(oracool::RightButtonOnly(spell)) << "spell " << static_cast<int>(spell);
	EXPECT_FALSE(oracool::IsRfa12Melee(SpellID::Rend)) << "Rend is cast round him now, not swung";
	EXPECT_EQ(oracool::WhirlwindStrikeTicks, 5) << "four strikes a second, at 20 ticks a second";
	EXPECT_EQ(oracool::WhirlwindRagePerSecond, 5);
	devilution::Player &player = FreshBarbarian();
	EXPECT_FALSE(oracool::IsWhirlwinding(player)) << "nothing spins until the right button is held";
	EXPECT_FALSE(oracool::CanPaySkill(player, SpellID::Whirlwind)) << "it takes Rage to start";
}

// Absolute Zero cools for 30 seconds after a cast (user, 2026-10-01); a hero who never cast it can cast it.
TEST(OracoolCooldowns, AFreshHeroHasNoCooldown)
{
	Players.resize(1);
	devilution::Player &player = Players[0];
	oracool::ClearRfa12PlayerBuffs(player);
	EXPECT_EQ(oracool::Rfa12CooldownTicksLeft(player, SpellID::AbsoluteZero), 0);
	EXPECT_FLOAT_EQ(oracool::Rfa12CooldownProgress(player, SpellID::AbsoluteZero), 1.0F);
	EXPECT_FLOAT_EQ(oracool::Rfa12CooldownProgress(player, SpellID::Bash), 1.0F) << "a skill with no cooldown is always ready";
}

// Every missile sheet's draw shift is held whole (round 59 audit: Absolute Zero's 224 wrapped to -32 in an int8 and the vortex
// stood four tiles to the Sorcerer's right).
TEST(OracoolCooldowns, AbsoluteZeroSheetIsCentredOnHim)
{
	const MissileFileData &data = GetMissileSpriteData(MissileGraphicID::AbsoluteZero);
	EXPECT_EQ(data.animWidth, 512);
	EXPECT_EQ(data.animWidth2, (512 - 64) / 2);
}
