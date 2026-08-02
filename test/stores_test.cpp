#include <array>

#include <gtest/gtest.h>

#include "items.h"
#include "stores.h"

using namespace devilution;

namespace {

TEST(Stores, SmithConsumablesListsFourInfinitePepinPotionsBeforeWitchStock)
{
	InitStores();
	for (devilution::Item &item : witchitem)
		item.clear();
	for (devilution::Item &item : healitem)
		item.clear();

	InitializeItem(witchitem[3], IDI_MANA);
	InitializeItem(healitem[7], IDI_RESURRECT);

	ASSERT_EQ(GetSmithConsumablesStockCountForTest(), 5);
	constexpr std::array<item_misc_id, 4> ExpectedPepinPotions = {
		IMISC_HEAL,
		IMISC_FULLHEAL,
		IMISC_REJUV,
		IMISC_FULLREJUV,
	};
	for (size_t i = 0; i < ExpectedPepinPotions.size(); ++i) {
		EXPECT_EQ(GetSmithConsumablesStockMiscIdForTest(i), ExpectedPepinPotions[i]);
		EXPECT_TRUE(IsSmithConsumablesStockFromPepinForTest(i));
		UpdateSmithConsumablesStockAfterPurchaseForTest(i);
		ASSERT_EQ(GetSmithConsumablesStockCountForTest(), 5);
		for (size_t j = 0; j < ExpectedPepinPotions.size(); ++j)
			EXPECT_EQ(GetSmithConsumablesStockMiscIdForTest(j), ExpectedPepinPotions[j]);
	}

	EXPECT_EQ(GetSmithConsumablesStockMiscIdForTest(4), IMISC_MANA);
	EXPECT_FALSE(IsSmithConsumablesStockFromPepinForTest(4));
	UpdateSmithConsumablesStockAfterPurchaseForTest(4);
	EXPECT_EQ(GetSmithConsumablesStockCountForTest(), 4);
}

TEST(Stores, AddStoreHoldRepair_magic)
{
	devilution::Item *item;

	item = &storehold[0];

	item->_iMaxDur = 60;
	item->_iDurability = item->_iMaxDur;
	item->_iMagical = ITEM_QUALITY_MAGIC;
	item->_iIdentified = true;
	item->_ivalue = 2000;
	item->_iIvalue = 19000;

	for (int i = 1; i < item->_iMaxDur; i++) {
		item->_ivalue = 2000;
		item->_iIvalue = 19000;
		item->_iDurability = i;
		storenumh = 0;
		AddStoreHoldRepair(item, 0);
		EXPECT_EQ(1, storenumh);
		EXPECT_EQ(95 * (item->_iMaxDur - i) / 2, item->_ivalue);
	}

	item->_iDurability = 59;
	storenumh = 0;
	item->_ivalue = 500;
	item->_iIvalue = 30; // To cheap to repair
	AddStoreHoldRepair(item, 0);
	EXPECT_EQ(0, storenumh);
	EXPECT_EQ(30, item->_iIvalue);
	EXPECT_EQ(500, item->_ivalue);
}

TEST(Stores, AddStoreHoldRepair_normal)
{
	devilution::Item *item;

	item = &storehold[0];

	item->_iMaxDur = 20;
	item->_iDurability = item->_iMaxDur;
	item->_iMagical = ITEM_QUALITY_NORMAL;
	item->_iIdentified = true;
	item->_ivalue = 2000;
	item->_iIvalue = item->_ivalue;

	for (int i = 1; i < item->_iMaxDur; i++) {
		item->_ivalue = 2000;
		item->_iIvalue = item->_ivalue;
		item->_iDurability = i;
		storenumh = 0;
		AddStoreHoldRepair(item, 0);
		EXPECT_EQ(1, storenumh);
		EXPECT_EQ(50 * (item->_iMaxDur - i), item->_ivalue);
	}

	item->_iDurability = 19;
	storenumh = 0;
	item->_ivalue = 10; // less than 1 per dur
	item->_iIvalue = item->_ivalue;
	AddStoreHoldRepair(item, 0);
	EXPECT_EQ(1, storenumh);
	EXPECT_EQ(1, item->_ivalue);
	EXPECT_EQ(1, item->_iIvalue);
}
} // namespace
