/**
 * @file oracool_passive_sweep_test.cpp
 *
 * The all-heroes passive sweep (v1.12.005): which rows are built, which stay inert and say why, and
 * the new hooks answering only while their passive is on.
 */

#include <gtest/gtest.h>

#include <set>
#include <string>

#include "levels/gendung.h"
#include "misdat.h"
#include "monster.h"
#include "oracool/class_tree.h"
#include "oracool/companion.h"
#include "oracool/hidden_classes.h"
#include "oracool/passives.h"
#include "oracool/rage.h"
#include "oracool/rfa12_actives.h"
#include "oracool/skill_points.h"
#include "oracool/weapon_throw.h"
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
		// The Necromancer arrived after the sweep (2026-09-17) with every row inert; his passives are built in his own phases.
		if (data.heroClass == HeroClass::Necromancer) {
			EXPECT_FALSE(data.implemented) << data.name;
		} else if (stillInert.count(data.name) != 0) {
			EXPECT_FALSE(data.implemented) << data.name;
		} else {
			EXPECT_TRUE(data.implemented) << data.name << " was left unbuilt by the sweep";
		}
		EXPECT_EQ(description.find("Not yet built") != std::string::npos, !data.implemented) << data.name;
	}
}

// The census notes (2026-09-14): the ten rows the user asked to be adjusted to the engine, all built.
TEST(OracoolCensusNotes, TheTenNotedSkillsAreBuiltTheWayTheNotesAsked)
{
	for (ClassTreeSkill skill : { ClassTreeSkill::DoubleThrow, ClassTreeSkill::ThrowingMastery, ClassTreeSkill::StaticField,
	         ClassTreeSkill::ThunderStorm, ClassTreeSkill::Meteor, ClassTreeSkill::Decoy, ClassTreeSkill::PoisonJavelin,
	         ClassTreeSkill::PlagueJavelin, ClassTreeSkill::CustomEngineering, ClassTreeSkill::Grenadier }) {
		const oracool::ClassTreeSkillData &data = oracool::GetClassTreeSkillData(skill);
		EXPECT_TRUE(data.implemented) << data.name;
	}
	EXPECT_EQ(oracool::ClassTreeSpellId(ClassTreeSkill::DoubleThrow), SpellID::WeaponThrow) << "Double Throw became the single throw";
	EXPECT_STREQ(oracool::GetClassTreeSkillData(ClassTreeSkill::DoubleThrow).name, "Weapon Throw");
	EXPECT_EQ(oracool::ClassTreeSpellId(ClassTreeSkill::Meteor), SpellID::Meteor);
	EXPECT_EQ(oracool::ClassTreeSpellId(ClassTreeSkill::Decoy), SpellID::Decoy);
	EXPECT_EQ(oracool::ClassTreeSpellId(ClassTreeSkill::PoisonJavelin), SpellID::PoisonJavelin);
	EXPECT_EQ(oracool::ClassTreeSpellId(ClassTreeSkill::PlagueJavelin), SpellID::PlagueJavelin);
	EXPECT_EQ(oracool::GetClassTreeSkillData(ClassTreeSkill::StaticField).kind, oracool::ClassTreeKind::Aura) << "like Holy Fire";
	EXPECT_EQ(oracool::GetClassTreeSkillData(ClassTreeSkill::ThunderStorm).kind, oracool::ClassTreeKind::Aura);
	EXPECT_EQ(oracool::RageCost(SpellID::WeaponThrow), 10) << "a Barbarian skill needs a Rage role";
}

TEST(OracoolCensusNotes, CustomEngineeringStrengthensRunesOnlyWhenSlotted)
{
	devilution::Player &player = FreshHero(HeroClass::Rogue);
	EXPECT_EQ(oracool::PassiveRuneLevelBonus(player), 0);
	ASSERT_TRUE(oracool::SetPassiveSlot(player, 0, ClassTreeSkill::CustomEngineering));
	EXPECT_EQ(oracool::PassiveRuneLevelBonus(player), 3);
	ClearSlots(player);
}

// User, 2026-09-14, Companions. The Valkyrie's numbers are the user's own: "30 sec on level 1 and up 5 sec every
// level", "up to 1000hp at lvl 20 and more onwards", "she should reach res 90% rather soon".
TEST(OracoolCompanion, TheValkyrieGrowsTheWayTheUserAsked)
{
	using oracool::CompanionKind;
	const oracool::CompanionStats first = oracool::CompanionStatsAt(CompanionKind::Valkyrie, 1);
	EXPECT_EQ(first.seconds, 30);
	EXPECT_EQ(oracool::CompanionStatsAt(CompanionKind::Valkyrie, 2).seconds, 35);
	EXPECT_EQ(first.hitPoints, 150);
	EXPECT_EQ(oracool::CompanionStatsAt(CompanionKind::Valkyrie, 20).hitPoints, 1000);
	EXPECT_GT(oracool::CompanionStatsAt(CompanionKind::Valkyrie, 25).hitPoints, 1000) << "and more onwards";
	EXPECT_LT(oracool::CompanionStatsAt(CompanionKind::Valkyrie, 10).elementalResist, 90);
	EXPECT_EQ(oracool::CompanionStatsAt(CompanionKind::Valkyrie, 11).elementalResist, 90) << "rather soon";
	EXPECT_EQ(oracool::CompanionStatsAt(CompanionKind::Valkyrie, 40).elementalResist, 90) << "and never past it";
	EXPECT_EQ(first.damagePercent, 50);
	EXPECT_EQ(oracool::CompanionStatsAt(CompanionKind::Decoy, 10).damagePercent, 0) << "a decoy strikes no one";
	EXPECT_EQ(oracool::CompanionAttackOf(CompanionKind::Valkyrie), oracool::CompanionAttack::Bow);
	EXPECT_EQ(oracool::CompanionAttackOf(CompanionKind::Talic), oracool::CompanionAttack::Melee);
}

// User, 2026-09-14: "an error occured when timer ran out on the valkyrie" - assertion monster.enemy < MAX_PLRS. A
// released body stops targeting monsters, so its enemy must name a player again.
TEST(OracoolCompanion, ALetGoBodyTargetsNoMonster)
{
	const Point holdingCell { 1, 0 }; // monster.cpp's GolemHoldingCell, which the test DLL cannot see
	devilution::Monster &slot = Monsters[1];
	// Monster cannot be copied; keep what the release touches.
	const auto savedFlags = slot.flags;
	const auto savedEnemy = slot.enemy;
	const auto savedEnemyPosition = slot.enemyPosition;
	const auto savedPosition = slot.position;
	const auto savedMode = slot.mode;
	slot.position.tile = holdingCell; // nothing on the map to clear
	slot.flags |= MFLAG_TARGETS_MONSTER;
	slot.enemy = 57;
	ReleaseCompanionBody(slot);
	EXPECT_EQ(slot.flags & MFLAG_TARGETS_MONSTER, 0u);
	EXPECT_LT(slot.enemy, MAX_PLRS) << "ProcessMonsters asserts a player index when no monster is targeted";
	EXPECT_EQ(Point(slot.position.tile), holdingCell);
	slot.flags = savedFlags;
	slot.enemy = savedEnemy;
	slot.enemyPosition = savedEnemyPosition;
	slot.position = savedPosition;
	slot.mode = savedMode;
}

TEST(OracoolCompanion, FourSkillsCallCompanionsAndTheStanceCycles)
{
	for (SpellID spell : { SpellID::Valkyrie, SpellID::AncestralCall, SpellID::SpiritGuardian, SpellID::Decoy })
		EXPECT_TRUE(oracool::IsCompanionSpell(spell)) << static_cast<int>(spell);
	EXPECT_FALSE(oracool::IsCompanionSpell(SpellID::Golem)) << "the Golem spell stays the engine's own";

	oracool::ForgetCompanions();
	EXPECT_FALSE(oracool::IsCompanion(Monsters[0]));
	EXPECT_EQ(oracool::GetCompanionAnim(Monsters[1], MonsterGraphic::Stand), nullptr);
	devilution::Monster local {};
	EXPECT_FALSE(oracool::IsCompanion(local)) << "a monster outside the table is never a companion";
	EXPECT_EQ(oracool::CompanionDamageTaken(local, DamageType::Fire, 640), 640) << "only a companion resists";
	EXPECT_FALSE(oracool::HasCompanion(oracool::CompanionKind::Valkyrie));

	EXPECT_EQ(oracool::GetCompanionStance(), oracool::CompanionStance::Follow);
	oracool::CycleCompanionStance();
	EXPECT_EQ(oracool::GetCompanionStance(), oracool::CompanionStance::Hold);
	oracool::CycleCompanionStance();
	oracool::CycleCompanionStance();
	oracool::CycleCompanionStance();
	EXPECT_EQ(oracool::GetCompanionStance(), oracool::CompanionStance::Follow) << "four stances, round again";
	oracool::ForgetCompanions();
}

// RfA-16 (2026-09-14): each census effect is its own drawn-only missile wearing its own sheet, and the throw
// picks the sword's spin with nothing in hand. (MissileArtLoaded reads MissileSpriteData, which the test DLL does
// not export, so it is not asked here.)
TEST(OracoolCensusNotes, TheCensusArtIsWiredToItsSkills)
{
	EXPECT_EQ(GetMissileData(MissileID::AcidJavelin).mFileNum, MissileGraphicID::AcidJavelin);
	EXPECT_EQ(GetMissileData(MissileID::AcidCloud).mFileNum, MissileGraphicID::AcidCloud);
	EXPECT_EQ(GetMissileData(MissileID::MeteorFall).mFileNum, MissileGraphicID::Meteor);
	EXPECT_EQ(GetMissileData(MissileID::MeteorImpact).mFileNum, MissileGraphicID::MeteorImpact);
	EXPECT_EQ(GetMissileData(MissileID::ThunderBolt).mFileNum, MissileGraphicID::ThunderBolt);

	devilution::Player &player = FreshHero(HeroClass::Barbarian);
	EXPECT_EQ(oracool::ThrownWeaponGraphic(player), MissileGraphicID::ThrownSword);
}

// User, 2026-09-14: "2 problems with valkyrie skill - the icon and the fact that it requires book!" It rode the
// Golem's book spell; its own id takes points and draws the class strip's glyph, not the legacy Golem icon.
TEST(OracoolCensusNotes, ValkyrieTakesSkillPointsNotBooks)
{
	EXPECT_EQ(oracool::ClassTreeSpellId(ClassTreeSkill::Valkyrie), SpellID::Valkyrie);
	EXPECT_FALSE(oracool::IsClassTreeRowRetiredAsSpell(ClassTreeSkill::Valkyrie));
	EXPECT_FALSE(oracool::SpellHasBook(SpellID::Valkyrie)) << "earned on the tree, never found";

	devilution::Player &rogue = FreshHero(HeroClass::Rogue);
	rogue._pUnspentSkillPoints = 1;
	EXPECT_TRUE(oracool::CanInvestClassTreePoint(rogue, ClassTreeSkill::Valkyrie));
	rogue._pUnspentSkillPoints = 0;
}

// User, 2026-09-14: "i tried casting valkyrie - game crashed." In town. Town and a quest's set level have no golem
// slot, so no summon may be spawned into one there.
TEST(OracoolCensusNotes, NoSummonWhereThereIsNoGolemSlot)
{
	const dungeon_type savedType = leveltype;
	const bool savedSet = setlevel;

	setlevel = false;
	leveltype = DTYPE_TOWN;
	EXPECT_FALSE(LevelHasGolemSlots()) << "town never runs InitGolems";
	leveltype = DTYPE_CATHEDRAL;
	EXPECT_TRUE(LevelHasGolemSlots());
	setlevel = true;
	EXPECT_FALSE(LevelHasGolemSlots()) << "a set level runs InitGolems but adds no slot";

	leveltype = savedType;
	setlevel = savedSet;
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
