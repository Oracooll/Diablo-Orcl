/**
 * @file oracool_essence_test.cpp
 *
 * The Necromancer's Essence (phase N2, v1.12.033): a second pool beside mana - 100 points, empty to full
 * within 20 seconds, his alone, and paid through the same door as mana and Rage.
 */

#include <gtest/gtest.h>

#include "oracool/curses.h"
#include "oracool/essence.h"
#include "oracool/rage.h"
#include "monster.h"
#include "player.h"

using namespace devilution;

namespace {

devilution::Player &FreshHero(HeroClass heroClass)
{
	Players.resize(1);
	devilution::Player &player = Players[0];
	player = {};
	player._pClass = heroClass;
	player._pHitPoints = 100 << 6;
	player._pMaxHP = 100 << 6;
	player._pmode = PM_STAND;
	oracool::ResetEssence(player);
	return player;
}

} // namespace

TEST(OracoolEssence, OnlyTheNecromancerHasAPoolAndHeKeepsHisMana)
{
	for (const HeroClass heroClass : { HeroClass::Warrior, HeroClass::Rogue, HeroClass::Sorcerer, HeroClass::Monk, HeroClass::Bard, HeroClass::Barbarian })
		EXPECT_FALSE(oracool::ClassUsesEssence(heroClass));
	EXPECT_TRUE(oracool::ClassUsesEssence(HeroClass::Necromancer));
	EXPECT_FALSE(oracool::ClassUsesRage(HeroClass::Necromancer)) << "Essence is beside mana, not instead of it";
	EXPECT_EQ(oracool::MaxEssence(FreshHero(HeroClass::Necromancer)), 100);
	EXPECT_EQ(oracool::MaxEssence(FreshHero(HeroClass::Sorcerer)), 0);
}

// The user's numbers: "pool of 100. fills within 20 seconds."
TEST(OracoolEssence, ItFillsFromEmptyWithinTwentySecondsAndNotMuchSooner)
{
	devilution::Player &player = FreshHero(HeroClass::Necromancer);
	EXPECT_EQ(oracool::CurrentEssence(player), 0);
	int ticks = 0;
	while (oracool::CurrentEssence(player) < 100 && ticks < 10000) {
		oracool::ProcessEssenceTick(player);
		ticks++;
	}
	EXPECT_LE(ticks, 20 * 20);
	EXPECT_GE(ticks, 19 * 20);
	// Full stays full.
	for (int i = 0; i < 100; i++)
		oracool::ProcessEssenceTick(player);
	EXPECT_EQ(oracool::CurrentEssence(player), 100);
}

TEST(OracoolEssence, TheDeadRefillNothingAndOtherClassesNeverHoldAny)
{
	devilution::Player &dead = FreshHero(HeroClass::Necromancer);
	dead._pHitPoints = 0;
	for (int i = 0; i < 400; i++)
		oracool::ProcessEssenceTick(dead);
	EXPECT_EQ(oracool::CurrentEssence(dead), 0);

	devilution::Player &sorcerer = FreshHero(HeroClass::Sorcerer);
	sorcerer._pEssence = 50 << 6; // as if left behind by another hero in this slot
	oracool::ProcessEssenceTick(sorcerer);
	EXPECT_EQ(sorcerer._pEssence, 0);
	EXPECT_FALSE(oracool::HasEssence(sorcerer, 1));
	EXPECT_TRUE(oracool::HasEssence(sorcerer, 0));
}

TEST(OracoolEssence, SpendingAndGainingStayInsideThePool)
{
	devilution::Player &player = FreshHero(HeroClass::Necromancer);
	oracool::GainEssence(player, 60);
	EXPECT_EQ(oracool::CurrentEssence(player), 60);
	EXPECT_TRUE(oracool::HasEssence(player, 60));
	EXPECT_FALSE(oracool::HasEssence(player, 61));
	oracool::SpendEssence(player, 25);
	EXPECT_EQ(oracool::CurrentEssence(player), 35);
	oracool::SpendEssence(player, 500);
	EXPECT_EQ(oracool::CurrentEssence(player), 0);
	oracool::GainEssence(player, 500);
	EXPECT_EQ(oracool::CurrentEssence(player), 100);
}

// The prices carried in the skill ledger (2026-09-17): the corpse skills 10, Revive 35, every curse 25, and nothing else -
// every other spell (Soul Harvest included, which GIVES Essence) still asks the Necromancer for mana.
TEST(OracoolEssence, OnlyTheCorpseSkillsReviveAndTheCursesArePricedInEssence)
{
	int priced = 0;
	for (int i = 0; i <= static_cast<int>(SpellID::LAST); i++) {
		const auto spell = static_cast<SpellID>(i);
		const int cost = oracool::EssenceCost(spell);
		if (cost > 0)
			priced++;
		if (IsAnyOf(spell, SpellID::CorpseExplosion, SpellID::PoisonExplosion))
			EXPECT_EQ(cost, 10) << i;
		else if (spell == SpellID::NecroRevive)
			EXPECT_EQ(cost, 35);
		else if (oracool::IsNecromancerCurse(spell) && spell != SpellID::SoulHarvest)
			EXPECT_EQ(cost, 25) << i;
		else
			EXPECT_EQ(cost, 0) << i;
	}
	EXPECT_EQ(priced, 3 + 14);
}

// The Curses page (phase N7): fourteen curse kinds behind fifteen actives, nothing cursed until something is cast.
TEST(OracoolCurses, FourteenKindsAndACleanSlate)
{
	int curses = 0;
	for (int i = 0; i <= static_cast<int>(SpellID::LAST); i++) {
		if (oracool::IsNecromancerCurse(static_cast<SpellID>(i)))
			curses++;
	}
	EXPECT_EQ(curses, 15) << "fourteen curses and Soul Harvest";
	oracool::ClearAllCurses();
	for (size_t id = 0; id < MaxMonsters; id++)
		EXPECT_EQ(oracool::CurseOn(Monsters[id]), oracool::CurseKind::None);
	EXPECT_STREQ(oracool::CurseName(oracool::CurseKind::Doom), "Doom");
	EXPECT_EQ(oracool::CurseLureTarget(Monsters[0]), -1);
}
