/**
 * @file oracool_passive_sweep_test.cpp
 *
 * The all-heroes passive sweep (v1.12.005): which rows are built, which stay inert and say why, and
 * the new hooks answering only while their passive is on.
 */

#include <gtest/gtest.h>

#include <set>
#include <string>

#include "oracool/class_tree.h"
#include "oracool/hidden_classes.h"
#include "oracool/passives.h"
#include "player.h"
#include "spells.h"

using namespace devilution;
using oracool::ClassTreeSkill;

namespace {

devilution::Player &FreshHero(HeroClass heroClass)
{
	Players.resize(1);
	devilution::Player &player = Players[0];
	player._pClass = heroClass;
	player._pLevel = 36;
	player._pHitPoints = 100 << 6;
	player._pMaxHP = 100 << 6;
	for (int slot = 0; slot < static_cast<int>(oracool::PassiveSlotCount); slot++)
		oracool::ClearPassiveSlot(player, slot);
	return player;
}

void ClearSlots(devilution::Player &player)
{
	for (int slot = 0; slot < static_cast<int>(oracool::PassiveSlotCount); slot++)
		oracool::ClearPassiveSlot(player, slot);
}

} // namespace

// Every Passive Skills row still inert names the reason the engine cannot carry it, and nothing built
// still says "Not yet built".
TEST(OracoolPassiveSweep, EveryInertPassiveSaysWhyAndEveryBuiltOneDoesNot)
{
	const std::set<std::string> stillInert = {
		"Custom Engineering", "Grenadier",
		"Sustain", "Encore", "Countermelody", "Improvisation", "Refrain", "Timbre", "Virtuoso", "Overture",
		"Reverberation", "Boon of Bul-Kathos", "Ballistics",
	};
	for (size_t i = 0; i <= static_cast<size_t>(ClassTreeSkill::LAST); i++) {
		const auto skill = static_cast<ClassTreeSkill>(i);
		if (!oracool::IsPassiveSkillRow(skill) && oracool::GetClassTreeSkillData(skill).page != oracool::RetiredFromTreePage)
			continue;
		const oracool::ClassTreeSkillData &data = oracool::GetClassTreeSkillData(skill);
		if (data.kind != oracool::ClassTreeKind::Passive)
			continue;
		const std::string description = data.description;
		if (stillInert.count(data.name) != 0) {
			EXPECT_FALSE(data.implemented) << data.name;
		} else {
			EXPECT_TRUE(data.implemented) << data.name << " was left unbuilt by the sweep";
		}
		EXPECT_EQ(description.find("Not yet built") != std::string::npos, !data.implemented) << data.name;
	}
}

// User, 2026-09-14: "remove this class from our mod ... Hide them, dont remove them."
TEST(OracoolHiddenClass, TheBardAndHisInstrumentsAreHiddenNotDeleted)
{
	EXPECT_TRUE(oracool::IsClassHidden(HeroClass::Bard));
	for (HeroClass shown : { HeroClass::Warrior, HeroClass::Rogue, HeroClass::Sorcerer, HeroClass::Monk, HeroClass::Barbarian })
		EXPECT_FALSE(oracool::IsClassHidden(shown));

	EXPECT_TRUE(oracool::IsHiddenItemIdx(IDI_ORACOOL_UNQBASE_WAR_LUTE));
	EXPECT_TRUE(oracool::IsHiddenItemIdx(IDI_ORACOOL_UNQBASE_CANTICLE));
	EXPECT_FALSE(oracool::IsHiddenItemIdx(IDI_ORACOOL_UNQBASE_ARCANE_FOCUS)) << "shares the Canticle's tumble, not its fate";
	EXPECT_FALSE(oracool::IsHiddenItemIdx(IDI_ORACOOL_UNQBASE_SPEAR));

	// Hidden, not deleted: the rows are still there for the class's return.
	EXPECT_STREQ(AllItemsList[IDI_ORACOOL_UNQBASE_WAR_LUTE].iName, "War Lute");
	EXPECT_EQ(oracool::GetClassTreeSkillData(ClassTreeSkill::MelodyOfLife).heroClass, HeroClass::Bard);
}

TEST(OracoolPassiveSweep, HoldYourGroundAddsBlockOnlyWhenSlotted)
{
	devilution::Player &player = FreshHero(HeroClass::Warrior); // the Paladin
	EXPECT_EQ(oracool::PassiveBlockBonus(player), 0);
	ASSERT_TRUE(oracool::SetPassiveSlot(player, 0, ClassTreeSkill::HoldYourGround));
	EXPECT_EQ(oracool::PassiveBlockBonus(player), 20);
	EXPECT_EQ(oracool::PassiveThornsPercent(player), 0);
	ASSERT_TRUE(oracool::SetPassiveSlot(player, 1, ClassTreeSkill::IronMaiden));
	EXPECT_EQ(oracool::PassiveThornsPercent(player), 50);
	ASSERT_TRUE(oracool::SetPassiveSlot(player, 2, ClassTreeSkill::Blunt));
	EXPECT_EQ(oracool::PassiveSkillDamagePercent(player, SpellID::BlessedHammer), 25);
	EXPECT_EQ(oracool::PassiveSkillDamagePercent(player, SpellID::BlessedShield), 0);
	ClearSlots(player);
}

TEST(OracoolPassiveSweep, ChantOfResonanceHalvesOnlyTheMantras)
{
	devilution::Player &player = FreshHero(HeroClass::Monk);
	EXPECT_EQ(oracool::PassiveManaCostPercent(player, SpellID::MantraOfEvasion), 0);
	ASSERT_TRUE(oracool::SetPassiveSlot(player, 0, ClassTreeSkill::ChantOfResonance));
	EXPECT_EQ(oracool::PassiveManaCostPercent(player, SpellID::MantraOfEvasion), -50);
	EXPECT_EQ(oracool::PassiveManaCostPercent(player, SpellID::MantraOfClarity), -50);
	EXPECT_EQ(oracool::PassiveManaCostPercent(player, SpellID::Firebolt), 0);
	ClearSlots(player);
}

TEST(OracoolPassiveSweep, AGreatBlowGivesTheIllusionistABurstOfSpeed)
{
	devilution::Player &player = FreshHero(HeroClass::Sorcerer);
	oracool::ClearPassiveClocks(player);
	ASSERT_TRUE(oracool::SetPassiveSlot(player, 0, ClassTreeSkill::Illusionist));
	oracool::OnPassiveDamaged(player, 5 << 6); // 5% of 100 life: not enough
	EXPECT_EQ(oracool::PassiveMoveSpeedBonus(player), 0);
	oracool::OnPassiveDamaged(player, 20 << 6); // 20%
	EXPECT_EQ(oracool::PassiveMoveSpeedBonus(player), 50);
	oracool::ClearPassiveClocks(player);
	ClearSlots(player);
}
