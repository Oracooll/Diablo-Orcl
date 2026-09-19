/**
 * @file oracool_necro_items_test.cpp
 *
 * The Necromancer's three item families (phase N9, v1.12.039): eight bases each in a contiguous run, kept out of the
 * seeded pool, the heads his alone, the six uniques on bases that exist.
 */

#include <gtest/gtest.h>

#include "itemdat.h"
#include "items.h"
#include "oracool/necro_items.h"
#include "player.h"

using namespace devilution;

TEST(OracoolNecroItems, ThreeFamiliesOfEightInOneRun)
{
	EXPECT_EQ(IDI_ORACOOL_NECRO_WAND_LAST - IDI_ORACOOL_NECRO_WAND_FIRST + 1, oracool::NecroBasesPerFamily);
	EXPECT_EQ(IDI_ORACOOL_NECRO_SCYTHE_LAST - IDI_ORACOOL_NECRO_SCYTHE_FIRST + 1, oracool::NecroBasesPerFamily);
	EXPECT_EQ(IDI_ORACOOL_NECRO_HEAD_LAST - IDI_ORACOOL_NECRO_HEAD_FIRST + 1, oracool::NecroBasesPerFamily);
	// The heads were the enum's tail until 2026-09-19; the sixteen new Imbuement Shard kinds follow them now.
	EXPECT_EQ(IDI_ORACOOL_SHARD_BLOOD, IDI_ORACOOL_NECRO_HEAD_LAST + 1);
	EXPECT_EQ(IDI_LAST, IDI_ORACOOL_SHARD_EASE);
	int lastLevel = 0;
	for (int i = IDI_ORACOOL_NECRO_WAND_FIRST; i <= IDI_ORACOOL_NECRO_HEAD_LAST; i++) {
		const ItemData &data = AllItemsList[static_cast<size_t>(i)];
		EXPECT_TRUE(oracool::IsNecroBaseIdx(i));
		EXPECT_EQ(data.iRnd, IDROP_REGULAR) << data.iName;
		EXPECT_NE(data.iItemId, UITYPE_NONE) << data.iName << " needs a unique type of its own";
		if (oracool::IsNecroWandIdx(i)) {
			EXPECT_EQ(data.itype, ItemType::Mace);
			EXPECT_EQ(data.iLoc, ILOC_ONEHAND);
		} else if (oracool::IsNecroScytheIdx(i)) {
			EXPECT_EQ(data.itype, ItemType::Axe);
			EXPECT_EQ(data.iLoc, ILOC_TWOHAND);
		} else {
			EXPECT_TRUE(oracool::IsNecroHeadIdx(i));
			EXPECT_EQ(data.itype, ItemType::Shield);
			EXPECT_EQ(data.iClass, ICLASS_ARMOR);
		}
		// A ladder within each family: the level never falls.
		const bool familyStart = i == IDI_ORACOOL_NECRO_WAND_FIRST || i == IDI_ORACOOL_NECRO_SCYTHE_FIRST || i == IDI_ORACOOL_NECRO_HEAD_FIRST;
		if (!familyStart)
			EXPECT_GE(data.iMinMLvl, lastLevel) << data.iName;
		lastLevel = data.iMinMLvl;
	}
	EXPECT_FALSE(oracool::IsNecroBaseIdx(IDI_ORACOOL_SETBASE_MACE));
	EXPECT_FALSE(oracool::IsNecroBaseIdx(IDI_LAST + 1));
}

TEST(OracoolNecroItems, TheSixUniquesStandOnBasesThatExist)
{
	int found = 0;
	for (size_t u = 0; u < UniqueItemCount; u++) {
		const UniqueItem &unique = UniqueItems[u];
		bool onNecroBase = false;
		for (int i = IDI_ORACOOL_NECRO_WAND_FIRST; i <= IDI_ORACOOL_NECRO_HEAD_LAST; i++)
			onNecroBase = onNecroBase || AllItemsList[static_cast<size_t>(i)].iItemId == unique.UIItemId;
		if (onNecroBase)
			found++;
	}
	EXPECT_EQ(found, 6);
}

TEST(OracoolNecroItems, TheHeadsAreHisAlone)
{
	Players.resize(1);
	devilution::Player &player = Players[0];
	player = {};
	player._pClass = HeroClass::Sorcerer;
	devilution::Item head {};
	head._itype = ItemType::Shield;
	head.IDidx = IDI_ORACOOL_NECRO_HEAD_FIRST;
	EXPECT_TRUE(oracool::IsNecroHeadItem(head));
	EXPECT_FALSE(oracool::ClassMayUseItem(player, head));
	player._pClass = HeroClass::Necromancer;
	EXPECT_TRUE(oracool::ClassMayUseItem(player, head));
	devilution::Item wand {};
	wand._itype = ItemType::Mace;
	wand.IDidx = IDI_ORACOOL_NECRO_WAND_FIRST;
	player._pClass = HeroClass::Warrior;
	EXPECT_TRUE(oracool::ClassMayUseItem(player, wand)) << "wands and scythes are anyone's";
	MyPlayer = &player;
	EXPECT_FALSE(oracool::NecroHeadsMayDrop());
	player._pClass = HeroClass::Necromancer;
	EXPECT_TRUE(oracool::NecroHeadsMayDrop());
}
