#include <algorithm>
#include <array>
#include <limits>

#include <gtest/gtest.h>

#include "items.h"
#include "options.h"
#include "player.h"
#include "qol/stash.h"
#include "storm/storm_net.hpp"
#include "stores.h"
#include "oracool/shop_grid.h"

using namespace devilution;

namespace {

/**
 * @brief Resets every global a store test can touch, so test ORDER cannot change a result.
 *
 * External audit of v1.9.88, finding 9. Run shuffled (`--gtest_shuffle --gtest_repeat=3`),
 * Sold_BuyBackChargesTheSalePriceNotTheItemValue failed on two iterations of three and passed on
 * the third. It cleared InvList and SpdList but not the EXTRA INVENTORY TABS, and
 * StorytellerIdentify_ListsAndIdentifiesItemStoredInExtraTab leaves an item in one - so the sell
 * list held two items where the test asserted one.
 *
 * A fixture rather than a helper each test remembers to call, because "remembers to call" is exactly
 * what failed: that test DID reset the containers its author had in mind. The ones it did not think
 * of are the ones a fixture covers.
 */
class StoresTest : public ::testing::Test {
public:
	void SetUp() override
	{
		gbIsMultiplayer = false;
		Players.resize(1);
		MyPlayer = &Players[0];

		// Every container a sell/repair list walks: the backpack and its grid, the belt, the worn
		// slots, all nine extra tabs, and the item in hand.
		for (int i = 0; i < InventoryGridCells; i++)
			MyPlayer->InvList[i].clear();
		for (int8_t &cell : MyPlayer->InvGrid)
			cell = 0;
		MyPlayer->_pNumInv = 0;
		for (auto &beltItem : MyPlayer->SpdList)
			beltItem.clear();
		for (auto &worn : MyPlayer->InvBody)
			worn.clear();
		MyPlayer->InvTabList = {};
		MyPlayer->InvTabGrid = {};
		MyPlayer->_pNumInvTab = {};
		MyPlayer->HoldItem.clear();

		Stash = {};
		// Clears the vendor arrays, the buyback shelf, the curated shelves and the service cursor.
		InitStores();
	}
};

TEST_F(StoresTest,SmithConsumablesListsFourInfinitePepinPotionsBeforeWitchStock)
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

// Regression test for a real bug: Item::clear() only resets _itype (which isEmpty()
// checks) and deliberately leaves every other field вЂ” including _iMiscId/_iClass/IDidx
// вЂ” as stale leftover data, since vanilla code always fully overwrites a cleared slot
// via assignment rather than reading its other fields. Before the fix, a cleared
// inventory/belt slot whose stale _iMiscId happened to match the item being purchased
// looked like a valid merge target to isStackableConsumable()/canStackWith(), so the
// merge pass silently "absorbed" the purchase into a slot the engine considers empty
// instead of ever copying the real item in: gold was spent and the item vanished. This
// reproduced the user's report exactly ("if I don't own a single consumable, some
// purchases don't appear") because a belt/inventory slot only carries this kind of
// stale-but-matching leftover data right after the player's last item of that type was
// used up вЂ” CreatePlayer's starting belt item plus the explicit .clear() calls below
// recreate exactly that condition.
TEST_F(StoresTest,SmithConsumablesBuy_AfterClearingSlotWithStaleMatchingData_ItemIsActuallyPlaced)
{
	SNetInitializeProvider(SELCONN_LOOPBACK, nullptr);
	Players.resize(1);
	CreatePlayer(Players[0], HeroClass::Warrior);
	MyPlayer = &Players[0];
	gbIsMultiplayer = false;
	MyPlayer->_pGold = 100000;
	for (int i = 0; i < InventoryGridCells; i++) {
		MyPlayer->InvList[i].clear();
		MyPlayer->InvGrid[i] = 0;
	}
	MyPlayer->_pNumInv = 0;
	for (auto &beltItem : MyPlayer->SpdList)
		beltItem.clear();

	InitStores();
	for (devilution::Item &item : witchitem)
		item.clear();
	for (devilution::Item &item : healitem)
		item.clear();

	StartStore(TalkID::SmithConsumables);

	int goldBefore = MyPlayer->_pGold;

	ASSERT_TRUE(SimulateSmithConsumablesPurchaseForTest(0)) << "Purchase did not reach the confirm screen (probe reported no room?)";

	EXPECT_LT(MyPlayer->_pGold, goldBefore) << "Gold was not spent";

	bool foundInInv = false;
	for (int i = 0; i < MyPlayer->_pNumInv; i++) {
		if (MyPlayer->InvList[i]._iMiscId == IMISC_HEAL)
			foundInInv = true;
	}
	bool foundOnBelt = false;
	for (auto &beltItem : MyPlayer->SpdList) {
		if (!beltItem.isEmpty() && beltItem._iMiscId == IMISC_HEAL)
			foundOnBelt = true;
	}
	EXPECT_TRUE(foundInInv || foundOnBelt) << "Purchased Potion of Healing is missing from both inventory and belt";
}

// Ctrl+right click buys a STACK of a restocking potion (user, 2026-09-13: "purchase a stack of up to
// 99 of these, limited by amount of available money, or free slots in the belt/inv grid").
TEST_F(StoresTest,CtrlRightClickBuysAPotionStackLimitedByGoldAndCappedAt99)
{
	SNetInitializeProvider(SELCONN_LOOPBACK, nullptr);
	Players.resize(1);
	CreatePlayer(Players[0], HeroClass::Warrior);
	MyPlayer = &Players[0];
	gbIsMultiplayer = false;
	const auto clearCarried = [] {
		for (int i = 0; i < InventoryGridCells; i++) {
			MyPlayer->InvList[i].clear();
			MyPlayer->InvGrid[i] = 0;
		}
		MyPlayer->_pNumInv = 0;
		for (auto &beltItem : MyPlayer->SpdList)
			beltItem.clear();
	};
	const auto healingCarried = [] {
		int units = 0;
		for (int i = 0; i < MyPlayer->_pNumInv; i++) {
			if (MyPlayer->InvList[i]._iMiscId == IMISC_HEAL)
				units += MyPlayer->InvList[i].stackCount();
		}
		for (auto &beltItem : MyPlayer->SpdList) {
			if (!beltItem.isEmpty() && beltItem._iMiscId == IMISC_HEAL)
				units += beltItem.stackCount();
		}
		return units;
	};
	clearCarried();

	InitStores();
	for (devilution::Item &item : witchitem)
		item.clear();
	for (devilution::Item &item : healitem)
		item.clear();
	StartStore(TalkID::SmithConsumables);

	// Entry 0 is Pepin's restocking Potion of Healing (see the test above).
	const std::vector<oracool::ShopSlot> stock = GetShopStock(TalkID::SmithConsumables);
	ASSERT_FALSE(stock.empty());
	ASSERT_EQ(stock[0].item->_iMiscId, IMISC_HEAL);
	const int price = stock[0].price;
	ASSERT_GT(price, 0);

	// Gold for exactly thirty: thirty, and every coin of it spent.
	Stash.gold = 0;
	MyPlayer->_pGold = price * 30;
	EXPECT_EQ(ShopBuyPotionStack(TalkID::SmithConsumables, stock[0].index), 30);
	EXPECT_EQ(healingCarried(), 30) << "the stack did not arrive whole";
	EXPECT_EQ(TotalPlayerGold(), 0u) << "thirty potions should cost thirty prices";

	// Gold for far more: one stack, capped at 99.
	clearCarried();
	MyPlayer->_pGold = price * 500;
	EXPECT_EQ(ShopBuyPotionStack(TalkID::SmithConsumables, stock[0].index), devilution::Item::MaxStackCount);
	EXPECT_EQ(healingCarried(), devilution::Item::MaxStackCount);
	EXPECT_EQ(TotalPlayerGold(), static_cast<uint32_t>(price * (500 - devilution::Item::MaxStackCount)));

	// Not enough for one: the gesture declines, and the ordinary purchase is left to say so.
	clearCarried();
	MyPlayer->_pGold = price - 1;
	EXPECT_EQ(ShopBuyPotionStack(TalkID::SmithConsumables, stock[0].index), -1);
	EXPECT_EQ(healingCarried(), 0);

	// A one-off entry is never multiplied: Adria's own stock past her three pinned slots.
	InitializeItem(witchitem[5], IDI_MANA);
	MyPlayer->_pGold = 100000;
	EXPECT_EQ(ShopBuyPotionStack(TalkID::WitchBuy, 5), -1) << "a potion that sells out was bought as a stack";
}

// Oracool Tabbed Inventory: Griswold's Sell Items list previously only ever scanned InvList and
// the belt, so an item moved into one of the 9 extra tabs was invisible to him - it never
// appeared in the sell list at all, even though it was a perfectly ordinary sellable item.
TEST_F(StoresTest,SmithSell_ListsItemStoredInExtraTab)
{
	Players.resize(1);
	MyPlayer = &Players[0];
	gbIsMultiplayer = false;

	for (int i = 0; i < InventoryGridCells; i++)
		MyPlayer->InvList[i].clear();
	MyPlayer->_pNumInv = 0;
	for (auto &beltItem : MyPlayer->SpdList)
		beltItem.clear();

	MyPlayer->InvTabList[2][0].clear();
	InitializeItem(MyPlayer->InvTabList[2][0], IDI_HEAL);
	MyPlayer->InvTabList[2][0]._iIdentified = true;
	MyPlayer->InvTabGrid[2][0] = 1;
	MyPlayer->_pNumInvTab[2] = 1;

	StartStore(TalkID::SmithSell);

	bool foundTabItem = false;
	for (int i = 0; i < storenumh; i++) {
		if (storehold[i]._iMiscId == IMISC_HEAL)
			foundTabItem = true;
	}
	EXPECT_TRUE(foundTabItem) << "the extra-tab item should be listed for sale, just like an item in the original backpack or belt";
}

// Oracool Tabbed Inventory: regression test for a real bug - Cain's "identify an item" list only
// ever scanned InvBody/InvList, so an unidentified item moved into an extra tab never showed up
// to be identified at all (matching the user's report that Cain "doesn't see unidentified items
// in tabs 2-10"). Separately, using a Scroll of Identify directly on such an item silently
// consumed the scroll without identifying anything, since pcursinvitem is deliberately -1 for
// extra-tab hovers - that path is covered by CheckIdentify's tabIdx parameter instead (items.cpp),
// which has no test-friendly seam here but is exercised by the same ResolveInvOrTabItem helper
// StorytellerIdentifyItem now shares the same storehTabIdx-driven resolution with.
TEST_F(StoresTest,StorytellerIdentify_ListsAndIdentifiesItemStoredInExtraTab)
{
	Players.resize(1);
	MyPlayer = &Players[0];
	gbIsMultiplayer = false;

	for (int i = 0; i < InventoryGridCells; i++)
		MyPlayer->InvList[i].clear();
	MyPlayer->_pNumInv = 0;
	for (auto &beltItem : MyPlayer->SpdList)
		beltItem.clear();
	for (auto &item : MyPlayer->InvBody)
		item.clear();

	MyPlayer->InvTabList[2][0].clear();
	InitializeItem(MyPlayer->InvTabList[2][0], IDI_HEAL);
	MyPlayer->InvTabList[2][0]._iMagical = ITEM_QUALITY_MAGIC;
	MyPlayer->InvTabList[2][0]._iIdentified = false;
	MyPlayer->InvTabGrid[2][0] = 1;
	MyPlayer->_pNumInvTab[2] = 1;

	StartStore(TalkID::StorytellerIdentify);

	int foundAt = -1;
	for (int i = 0; i < storenumh; i++) {
		if (!storehold[i]._iIdentified)
			foundAt = i;
	}
	ASSERT_NE(foundAt, -1) << "the unidentified extra-tab item should be listed for identification, just like one in the original backpack";

	SimulateStorytellerIdentifyForTest(static_cast<size_t>(foundAt));

	EXPECT_TRUE(MyPlayer->InvTabList[2][0]._iIdentified) << "identifying the listed entry should mark the real extra-tab item identified, not silently do nothing";
}

// Cain's list walked vanilla's seven worn slots; an unidentified piece on the fork's other six never appeared (audit,
// 2026-09-27).
TEST_F(StoresTest,StorytellerIdentify_ListsAndIdentifiesAPieceWornInAnAddedSlot)
{
	InitializeItem(MyPlayer->InvBody[INVLOC_BOOTS], IDI_HEAL);
	MyPlayer->InvBody[INVLOC_BOOTS]._iMagical = ITEM_QUALITY_MAGIC;
	MyPlayer->InvBody[INVLOC_BOOTS]._iIdentified = false;

	StartStore(TalkID::StorytellerIdentify);

	int foundAt = -1;
	for (int i = 0; i < storenumh; i++) {
		if (!storehold[i]._iIdentified)
			foundAt = i;
	}
	ASSERT_NE(foundAt, -1) << "the boots should be on Cain's list";
	SimulateStorytellerIdentifyForTest(static_cast<size_t>(foundAt));
	EXPECT_TRUE(MyPlayer->InvBody[INVLOC_BOOTS]._iIdentified);
}

// The lists held 48 - vanilla's backpack and belt - and the sell list stopped filling there before it sorted by price,
// so the 49th sellable item on and everything on later pages never showed (audit, 2026-09-27).
TEST_F(StoresTest,SmithSell_ListsEverySellableItemPastVanillasFortyEight)
{
	constexpr int Count = 60;
	for (int n = 0; n < Count; n++) {
		const int tab = n / InventoryGridCells;
		const int index = n % InventoryGridCells;
		devilution::Item &item = MyPlayer->InvTabList[tab][index];
		InitializeItem(item, IDI_HEAL);
		item._iIdentified = true;
		MyPlayer->InvTabGrid[tab][index] = static_cast<int8_t>(index + 1);
		MyPlayer->_pNumInvTab[tab] = index + 1;
	}
	ASSERT_GT(StoreHoldCapacity, Count);

	StartStore(TalkID::SmithSell);

	EXPECT_EQ(storenumh, Count);
}

// A new hero met the last hero's Wirt: InitStores left his two grids, and SpawnBoy restocks only on an empty first
// slot or a higher tier (audit, 2026-09-27).
TEST_F(StoresTest,InitStores_EmptiesWirtsShopAndGambleGrids)
{
	InitializeItem(boyitems[0], IDI_HEAL);
	InitializeItem(gambleitems[0], IDI_HEAL);

	InitStores();

	for (const devilution::Item &item : boyitems)
		EXPECT_TRUE(item.isEmpty());
	for (const devilution::Item &item : gambleitems)
		EXPECT_TRUE(item.isEmpty());
}

TEST_F(StoresTest,SmithSell_StackedConsumable_PricedByQuantity)
{
	Players.resize(1);
	MyPlayer = &Players[0];
	gbIsMultiplayer = false;

	MyPlayer->InvList[0].clear();
	InitializeItem(MyPlayer->InvList[0], IDI_HEAL);
	MyPlayer->InvList[0]._iIdentified = true;
	MyPlayer->InvList[0].setStackCount(40);
	MyPlayer->_pNumInv = 1;
	for (auto &beltItem : MyPlayer->SpdList)
		beltItem.clear();

	const int unitValue = MyPlayer->InvList[0]._ivalue;
	const int expectedUnitPrice = std::max(unitValue / 4, 1);

	StartStore(TalkID::SmithSell);

	ASSERT_GE(storenumh, 1);
	EXPECT_EQ(storehold[0]._ivalue, expectedUnitPrice * 40);
	EXPECT_EQ(storehold[0]._iIvalue, expectedUnitPrice * 40);
}

// The Witch's sell list previously never sorted by price at all, unlike Griswold's - PopulateSellList
// is now shared by both, so highest-price-first sorting always applies uniformly to each.
TEST_F(StoresTest,WitchSell_SortsByPriceHighestFirst)
{
	Players.resize(1);
	MyPlayer = &Players[0];
	gbIsMultiplayer = false;

	for (int i = 0; i < InventoryGridCells; i++)
		MyPlayer->InvList[i].clear();
	InitializeItem(MyPlayer->InvList[0], IDI_HEAL);
	MyPlayer->InvList[0]._iIdentified = true;
	MyPlayer->InvList[0].setStackCount(1);
	InitializeItem(MyPlayer->InvList[1], IDI_HEAL);
	MyPlayer->InvList[1]._iIdentified = true;
	MyPlayer->InvList[1].setStackCount(40);
	MyPlayer->_pNumInv = 2;
	for (auto &beltItem : MyPlayer->SpdList)
		beltItem.clear();

	StartStore(TalkID::WitchSell);

	ASSERT_GE(storenumh, 2);
	EXPECT_GE(storehold[0]._iIvalue, storehold[1]._iIvalue) << "items should be listed highest price first";
}

// The Sold tab (v1.9.28). Selling used to be the whole point of this screen; it is a record of what
// the vendor already bought now, offered back at the price they paid. Two things have to hold for
// that to be true, and neither is visible from the transaction code alone: the sale has to REACH the
// list whichever door it came in by, and the buyback has to charge exactly what was paid rather than
// the item's worth - which for a magic item is several times higher.
TEST_F(StoresTest,Sold_BuyBackChargesTheSalePriceNotTheItemValue)
{
	Players.resize(1);
	MyPlayer = &Players[0];
	gbIsMultiplayer = false;
	InitStores();
	Stash = {};

	for (int i = 0; i < InventoryGridCells; i++)
		MyPlayer->InvList[i].clear();
	for (auto &beltItem : MyPlayer->SpdList)
		beltItem.clear();
	InitializeItem(MyPlayer->InvList[0], IDI_HEAL);
	MyPlayer->InvList[0]._iIdentified = true;
	MyPlayer->InvList[0].setStackCount(1);
	MyPlayer->_pNumInv = 1;

	ASSERT_TRUE(GetShopStock(TalkID::SmithSell).empty()) << "the Sold tab should start empty";

	StartStore(TalkID::SmithSell);
	ASSERT_EQ(storenumh, 1);
	const int salePrice = storehold[0]._iIvalue;
	ASSERT_GT(salePrice, 0);
	// Through the Sell all button, which is a public door onto the same StoreSellItemAt every other
	// sale path ends in - including the drag onto the panel, which has no headless equivalent.
	ShopActivateAction(TalkID::SmithSell, GetSellAllLineForTest());

	const std::vector<oracool::ShopSlot> sold = GetShopStock(TalkID::SmithSell);
	ASSERT_EQ(sold.size(), 1u) << "a sale did not reach the Sold tab";
	EXPECT_EQ(sold[0].price, salePrice) << "the Sold tab is not offering it back at what was paid";

	// One gold short of the sale price must refuse. This is how the CHARGE is pinned without
	// running the placement: a buyback priced off _ivalue - which for anything magical is several
	// times the sale price - would refuse here too, but so would one priced correctly, so the
	// matching "exactly the sale price is enough" half is what makes the pair meaningful. That half
	// cannot run headless: placing the item calls into the network layer, which is not up in a test
	// binary, and no existing store test executes a persisted placement either.
	Stash.gold = salePrice - 1;
	MyPlayer->_pGold = 0;
	ShopBuyBack(0);
	EXPECT_EQ(GetShopStock(TalkID::SmithSell).size(), 1u) << "an unaffordable buyback consumed the entry";
	EXPECT_EQ(TotalPlayerGold(), static_cast<uint32_t>(salePrice - 1)) << "an unaffordable buyback still charged";

	// And the gate opens at exactly the sale price, not above it - checked through the same
	// predicate the buyback uses rather than by running it.
	Stash.gold = salePrice;
	EXPECT_GE(TotalPlayerGold(), static_cast<uint32_t>(sold[0].price))
	    << "the sale price is not affordable with exactly the gold the sale paid";
}

// A Torment-tier magic item is valued in the millions; 30 x value x wear overflowed an int within a few points of wear
// (audit, 2026-09-27) - a negative price no hero could pay, which also stopped Repair All.
TEST_F(StoresTest,RepairPriceFor_DoesNotOverflowOnAMillionGoldItem)
{
	devilution::Item *item = &storehold[0];
	item->_iMagical = ITEM_QUALITY_MAGIC;
	item->_iIdentified = true;
	item->_ivalue = 2000000;
	item->_iIvalue = 2000000;
	item->_iMaxDur = 150;
	item->_iDurability = 0;
	storenumh = 0;
	AddStoreHoldRepair(item, 0);
	ASSERT_EQ(storenumh, 1);
	EXPECT_EQ(storehold[0]._ivalue, 300000);
}

TEST_F(StoresTest,AddStoreHoldRepair_magic)
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

TEST_F(StoresTest,AddStoreHoldRepair_normal)
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

// The Repair list used to keep whatever fixed order StartSmithRepair populated it in (equipped
// slots first, then inventory in slot order), unlike the Sell list which already sorts by price.
// It should now sort by repair cost, descending - AddStoreHoldRepair already overwrites _iIvalue
// with the computed cost before storing the entry, so sorting on that field ranks by "what you'd
// pay to fix this," not the item's own value.
TEST_F(StoresTest,SmithRepair_SortsByRepairCostDescending)
{
	Players.resize(1);
	MyPlayer = &Players[0];
	gbIsMultiplayer = false;

	for (auto &item : MyPlayer->InvBody)
		item.clear();
	for (int i = 0; i < InventoryGridCells; i++)
		MyPlayer->InvList[i].clear();
	MyPlayer->_pNumInv = 3;

	auto setupDamagedItem = [](devilution::Item &item, int maxDur, int durability, int value) {
		// Item::clear() only resets _itype; a real base item is initialized via
		// InitializeItem first so _iName/_iIName and everything else ScrollSmithSell's
		// display formatting reads are actually valid, then just the durability/value
		// fields relevant to this test are overridden.
		InitializeItem(item, IDI_WARRIOR);
		item._iMagical = ITEM_QUALITY_NORMAL;
		item._iMaxDur = maxDur;
		item._iDurability = durability;
		item._ivalue = value;
		item._iIvalue = value;
	};

	// Cheapest repair first, most expensive last, deliberately out of order.
	setupDamagedItem(MyPlayer->InvList[0], 40, 39, 2000); // small dur deficit -> cheap
	setupDamagedItem(MyPlayer->InvList[1], 40, 4, 2000);  // large dur deficit -> expensive
	setupDamagedItem(MyPlayer->InvList[2], 40, 20, 2000); // moderate dur deficit

	StartStore(TalkID::SmithRepair);

	ASSERT_EQ(storenumh, 3);
	EXPECT_GE(storehold[0]._iIvalue, storehold[1]._iIvalue);
	EXPECT_GE(storehold[1]._iIvalue, storehold[2]._iIvalue);
}

// User-reported bug: a found/looted "The Butcher's Cleaver" unique couldn't be sold to Griswold.
// Root cause: vanilla Diablo happens to define IDI_CLEAVER == IDI_FIRSTQUEST, so SmithSellOk's
// quest-item-range exclusion (meant for real quest deliverables like the Rock or Anvil) caught
// the Cleaver too, even though it's an ordinary lootable/sellable unique. Fixed with an explicit
// carve-out for IDI_CLEAVER.
TEST_F(StoresTest,SmithSell_CleaverUniqueIsSellableDespiteQuestIdRange)
{
	Players.resize(1);
	MyPlayer = &Players[0];
	gbIsMultiplayer = false;
	MyPlayer->InvTabList = {};
	MyPlayer->InvTabGrid = {};
	MyPlayer->_pNumInvTab = {};

	for (int i = 0; i < InventoryGridCells; i++)
		MyPlayer->InvList[i].clear();
	InitializeItem(MyPlayer->InvList[0], IDI_CLEAVER);
	MyPlayer->InvList[0]._iMagical = ITEM_QUALITY_UNIQUE;
	MyPlayer->InvList[0]._iIdentified = true;
	MyPlayer->InvList[0]._iIvalue = 3650;
	MyPlayer->InvList[0]._iCreateInfo = 0; // a real dungeon drop, not a Unique Shop item (no CF_SMITH)
	MyPlayer->_pNumInv = 1;
	for (auto &beltItem : MyPlayer->SpdList)
		beltItem.clear();

	StartStore(TalkID::SmithSell);

	ASSERT_EQ(storenumh, 1) << "the Cleaver should appear in Griswold's sell list";
	EXPECT_EQ(storehold[0].IDidx, IDI_CLEAVER);
}

// User bug report: "i cant sell unique items i was awarded from NPCs to griswold. They don't
// appear in the SELL ITEMS list." Root cause was the same quest-ID-range check that once blocked
// the Cleaver - several other genuine Unique items (Harlequin Crest among them) also happen to
// use a base-item slot inside IDI_FIRSTQUEST..IDI_LASTQUEST, purely as an artifact of vanilla's
// item table ordering, and were still being blocked despite Cleaver's own fix.
TEST_F(StoresTest,SmithSell_OtherQuestRangeUniquesAreSellableToo)
{
	Players.resize(1);
	MyPlayer = &Players[0];
	gbIsMultiplayer = false;
	MyPlayer->InvTabList = {};
	MyPlayer->InvTabGrid = {};
	MyPlayer->_pNumInvTab = {};

	for (int i = 0; i < InventoryGridCells; i++)
		MyPlayer->InvList[i].clear();
	InitializeItem(MyPlayer->InvList[0], IDI_HARCREST);
	MyPlayer->InvList[0]._iMagical = ITEM_QUALITY_UNIQUE;
	MyPlayer->InvList[0]._iIdentified = true;
	MyPlayer->InvList[0]._iIvalue = 5000;
	MyPlayer->InvList[0]._iCreateInfo = 0;
	MyPlayer->_pNumInv = 1;
	for (auto &beltItem : MyPlayer->SpdList)
		beltItem.clear();

	StartStore(TalkID::SmithSell);

	ASSERT_EQ(storenumh, 1) << "the Harlequin Crest should appear in Griswold's sell list";
	EXPECT_EQ(storehold[0].IDidx, IDI_HARCREST);
}

// User-reported bug: re-entering Griswold's "Buy Basic Items" screen after buying out his
// entire stock bounced the player back out to the store menu instead of just showing an empty
// list. StartStore's TalkID::SmithBuy case used to special-case an empty smithitem[] by calling
// StoreESC(), which - since stextflag was set to TalkID::SmithBuy right before that call -
// resolves to StartStore(TalkID::Smith), leaving stextflag as TalkID::Smith rather than
// TalkID::SmithBuy. StartSmithBuy() itself already renders correctly with zero items (just a
// header line and a Back button), so the special case was unnecessary.
TEST_F(StoresTest,SmithBuy_EmptyStock_StaysOnBuyScreenInsteadOfBouncingOut)
{
	for (devilution::Item &item : smithitem)
		item.clear();

	StartStore(TalkID::SmithBuy);

	EXPECT_EQ(stextflag, TalkID::SmithBuy) << "an empty Buy Basic Items list should render normally, not back out to the store menu";
}

// User-reported bug, third strike in this click-routing area: "again Sell All is not working."
// Clicking Sell All on a sell page with exactly FOUR items opened the last item's single-item
// confirmation instead. The Premium Refresh/Refresh-Until redirects on Back's row were gated only
// on "does that line index have text" - and on a four-item sell page the last item's second
// attribute line lands on the exact line PremiumRefreshLine() names, so the Premium branch
// hijacked the click before the Sell-All branch was ever consulted. Intermittent by list shape
// (one to three items leave that line empty), which is why it kept coming back. Every redirect is
// now gated on its own screen's stextflag; this test pins the four-item layout that armed the trap.
TEST_F(StoresTest,SmithSell_FourItemPage_SellAllRowNotHijackedByPremiumRedirect)
{
	Players.resize(1);
	MyPlayer = &Players[0];
	gbIsMultiplayer = false;
	MyPlayer->InvTabList = {};
	MyPlayer->InvTabGrid = {};
	MyPlayer->_pNumInvTab = {};

	for (int i = 0; i < InventoryGridCells; i++)
		MyPlayer->InvList[i].clear();
	for (auto &beltItem : MyPlayer->SpdList)
		beltItem.clear();

	// A plain droppable shield base, found by walking the table rather than hardcoding an index -
	// SmithSellOk rejects quest-range ids for non-unique items, so IDI_HARCREST (used by the
	// neighbouring test, which marks its item unique) would be silently filtered out here.
	devilution::_item_indexes shieldIdx = IDI_NONE;
	for (std::underlying_type_t<devilution::_item_indexes> i = IDI_GOLD; i <= IDI_LAST; i++) {
		if (AllItemsList[i].itype == ItemType::Shield && AllItemsList[i].iRnd != IDROP_NEVER) {
			shieldIdx = static_cast<devilution::_item_indexes>(i);
			break;
		}
	}
	ASSERT_NE(shieldIdx, IDI_NONE);

	// Four identified magic shields: each renders a name line plus TWO attribute lines, which is
	// what pushes the fourth item's tail onto PremiumRefreshLine()'s row.
	for (int i = 0; i < 4; i++) {
		devilution::Item &item = MyPlayer->InvList[i];
		InitializeItem(item, shieldIdx);
		item._iMagical = ITEM_QUALITY_MAGIC;
		item._iIdentified = true;
		// The affix on the item's one list, where every affix lives since 2026-09-25 (it was the vanilla prefix field).
		item._iOracoolAffixes[0] = OracoolAffix { IPL_LIGHTRES, 51, 0 };
		item._iOracoolAffixCount = 1;
		item._iPLLR = 51;
		item._iAC = 2;
		item._iDurability = 6;
		item._iMaxDur = 16;
		item._ivalue = 100;
		item._iIvalue = 4312 - i; // distinct, harmless
		item._iCreateInfo = 0;
	}
	MyPlayer->_pNumInv = 4;

	Stash = {};
	StartStore(TalkID::SmithSell);
	// StartStore sets stextflag only after populating, and the Sell all button is added by the
	// per-frame rescroll that gates on it - which a headless test must request explicitly.
	RescrollStoreForTest();
	ASSERT_EQ(storenumh, 4);

	// The trap this bug depended on: the last item's text reaching PremiumRefreshLine()'s row.
	// If a layout change ever un-arms it, this test needs a new arrangement, not deletion.
	ASSERT_TRUE(StoreLineHasTextForTest(GetPremiumRefreshLineForTest()))
	    << "test setup no longer reproduces the four-item layout the bug depended on";

	// The routing itself, which is where the bug lived: a click in the right zone of Back's row
	// must resolve to Sell All's line. Before the fix this returned PremiumRefreshLine() - the
	// Premium branch hijacked it because the fourth item's text made that line non-empty.
	const int uiLeft = 0;
	const int rightZoneX = uiLeft + 616 - 20;
	EXPECT_EQ(ResolveBackRowClickLine(rightZoneX, uiLeft), GetSellAllLineForTest())
	    << "right-zone click on Back's row was hijacked away from Sell All";

	// And the left zone must be Back, NOT Sell All - on this screen PremiumRefreshUntilLine()
	// numerically coincides with SmithSellAllLine(), so before the fix a click near Back's left
	// edge would have sold everything with no confirmation.
	EXPECT_NE(ResolveBackRowClickLine(uiLeft + 24 + 20, uiLeft), GetSellAllLineForTest())
	    << "left-zone click on Back's row must not trigger Sell All";

	// Then the dispatch: activating Sell All must run Sell All, not a single-item confirm.
	//
	// Sell is a GRID screen since v1.9.26 (oracool/shop_grid.h), so Sell All is a footer button
	// rather than a text row and StoreEnter no longer reaches it - it now activates whatever the
	// grid cursor is on. The button still carries the same line index, which is what identifies it
	// to the same handler, so the dispatch under test is unchanged; only the door is.
	const std::vector<oracool::ShopAction> actions = GetShopActions(TalkID::SmithSell);
	ASSERT_EQ(actions.size(), 1u) << "the Sell screen no longer offers Sell all";
	ASSERT_EQ(actions[0].line, GetSellAllLineForTest());
	ShopActivateAction(TalkID::SmithSell, actions[0].line);

	EXPECT_NE(stextflag, TalkID::Confirm)
	    << "Sell All click opened a single-item confirmation - the Premium redirect hijacked it";
	int remaining = 0;
	for (int i = 0; i < InventoryGridCells; i++) {
		if (!MyPlayer->InvList[i].isEmpty())
			remaining++;
	}
	EXPECT_EQ(remaining, 0) << "Sell All should have sold every listed item";
}

// Sell all from every Griswold tab (user, 2026-09-13: "in griswold shops - any screen that has refresh button
// reduce its width in half and add a second button next to it SELL ALL. i want to be able to sell all items
// from any tab of griswold shop").
TEST_F(StoresTest,SellAllIsOfferedOnEveryGriswoldTabAndStaysOnIt)
{
	SNetInitializeProvider(SELCONN_LOOPBACK, nullptr);
	sgOptions.Oracool.shopStockRefresh.SetValue(true);

	const auto sellAllOn = [](TalkID tab) -> const oracool::ShopAction * {
		static std::vector<oracool::ShopAction> actions;
		actions = GetShopActions(tab);
		for (const oracool::ShopAction &action : actions) {
			if (std::string_view(action.label) == "Sell all")
				return &action;
		}
		return nullptr;
	};
	for (TalkID tab : { TalkID::SmithBuy, TalkID::SmithPremiumBuy, TalkID::SmithUniqueBuy, TalkID::SmithRareBuy,
	         TalkID::SmithSetBuy, TalkID::SmithConsumables }) {
		EXPECT_NE(sellAllOn(tab), nullptr) << "Griswold tab " << static_cast<int>(tab) << " offers no Sell all";
	}

	// Beside Refresh, not in front of it: the button sits next to the one it halves.
	std::vector<oracool::ShopAction> basic = GetShopActions(TalkID::SmithBuy);
	ASSERT_EQ(basic.size(), 2u);
	EXPECT_EQ(std::string_view(basic[0].label), "Refresh");
	EXPECT_EQ(std::string_view(basic[1].label), "Sell all");
	// And never on a line a refresh dispatches on.
	EXPECT_NE(basic[1].line, basic[0].line);

	// Two plain shields in the pack, sold from the Basic tab.
	devilution::_item_indexes shieldIdx = IDI_NONE;
	for (std::underlying_type_t<devilution::_item_indexes> i = IDI_GOLD; i <= IDI_LAST; i++) {
		if (AllItemsList[i].itype == ItemType::Shield && AllItemsList[i].iRnd != IDROP_NEVER) {
			shieldIdx = static_cast<devilution::_item_indexes>(i);
			break;
		}
	}
	ASSERT_NE(shieldIdx, IDI_NONE);
	for (int i = 0; i < 2; i++) {
		devilution::Item &item = MyPlayer->InvList[i];
		InitializeItem(item, shieldIdx);
		item._iIdentified = true;
		item._iCreateInfo = 0;
	}
	MyPlayer->_pNumInv = 2;
	MyPlayer->_pGold = 0;

	StartStore(TalkID::SmithBuy);
	const oracool::ShopAction *sellAll = sellAllOn(TalkID::SmithBuy);
	ASSERT_NE(sellAll, nullptr);
	ShopActivateAction(TalkID::SmithBuy, sellAll->line);

	EXPECT_EQ(stextflag, TalkID::SmithBuy) << "Sell all moved the player off the tab it was pressed on";
	int remaining = 0;
	for (int i = 0; i < InventoryGridCells; i++) {
		if (!MyPlayer->InvList[i].isEmpty())
			remaining++;
	}
	EXPECT_EQ(remaining, 0) << "Sell all from the Basic tab left items in the pack";
	EXPECT_GT(TotalPlayerGold(), 0u) << "Sell all paid nothing";
}

// User request: "Sort Stash" (Gillian's dialog) should sort by item category (Weapons, Armor,
// Helms, Shields, Jewelry, then everything else), descending price within each category.
TEST_F(StoresTest,SortStash_OrdersByCategoryThenDescendingPrice)
{
	Players.resize(1);
	MyPlayer = &Players[0];
	gbIsMultiplayer = false;
	Stash = {};

	// `curs` is item_cursor_graphic, not int8_t. It was int8_t, which silently truncated every id
	// above 127 - ICURS_LEATHER_ARMOR (135) arrived as -121. That round-tripped back to 135 only
	// because Item::_iCurs was itself uint8_t; once it widened to uint16_t the same -121 became
	// 65415, GetInvItemSize read far out of bounds, and the sort crashed with a divide by zero.
	auto makeItem = [](ItemType itype, item_cursor_graphic curs, int value) {
		devilution::Item item;
		item._itype = itype;
		item._iCurs = curs;
		item._iMagical = ITEM_QUALITY_NORMAL;
		item._ivalue = value;
		return item;
	};

	// Inserted out of both category and price order.
	devilution::Item ring = makeItem(ItemType::Ring, ICURS_RING, 100);
	devilution::Item cheapSword = makeItem(ItemType::Sword, ICURS_SHORT_SWORD, 40);
	devilution::Item armor = makeItem(ItemType::LightArmor, ICURS_LEATHER_ARMOR, 200);
	devilution::Item pricySword = makeItem(ItemType::Sword, ICURS_SHORT_SWORD, 400);

	Stash.stashList = { ring, cheapSword, armor, pricySword };

	SortStash(*MyPlayer);

	ASSERT_EQ(Stash.stashList.size(), 4u);
	// Weapons before Armor before Jewelry; descending price within Weapons.
	EXPECT_EQ(Stash.stashList[0]._itype, ItemType::Sword);
	EXPECT_EQ(Stash.stashList[0]._ivalue, 400);
	EXPECT_EQ(Stash.stashList[1]._itype, ItemType::Sword);
	EXPECT_EQ(Stash.stashList[1]._ivalue, 40);
	EXPECT_EQ(Stash.stashList[2]._itype, ItemType::LightArmor);
	EXPECT_EQ(Stash.stashList[3]._itype, ItemType::Ring);

	// Every item should have landed somewhere on page 0's grid (nothing lost in the re-sort).
	int placedCount = 0;
	for (const auto &row : Stash.stashGrids[0])
		for (StashStruct::StashCell cell : row)
			if (cell != 0)
				placedCount++;
	EXPECT_GT(placedCount, 0);
}

// A paid service cursor left behind by a closed shop must NEVER become the vanilla class skill.
//
// External audit of v1.9.92, finding 1, against my own v1.9.92 fix. That fix made the paid predicate
// require an open shop and then reconciled the leftover cursor once a game-logic tick. I argued that
// reconciling beat a list of exit sites, and as a BACKSTOP it does - but SDL drains a whole queued
// event batch before game logic runs, so a close and a click land together and the tick has not
// happened yet. In that window `pcurs` is still CURSOR_REPAIR while the paid predicate is already
// false, and TryIconCurs fell through to vanilla Repair - which reduces _iMaxDur permanently.
//
// This pins the decision the click path now makes BEFORE that fallback, with no tick in between.
TEST_F(StoresTest, StaleServiceCursorIsConsumedRatherThanRunAsTheVanillaSkill)
{
	ArmShopRepairCursor();
	ASSERT_TRUE(IsAnyShopServiceCursorArmed()) << "arming did not record the raw state";

	// The shop closes. No UpdateStoreState() call - that is the whole point: this is the window
	// between the close event and the next game-logic tick.
	stextflag = TalkID::None;

	EXPECT_FALSE(IsShopRepairCursorArmed())
	    << "the PAID path must be dead the moment the shop is gone - it takes gold";
	ASSERT_TRUE(IsAnyShopServiceCursorArmed())
	    << "the raw state must survive, or the click path cannot tell a shop cursor from a skill one";

	EXPECT_TRUE(ConsumeStaleShopServiceCursor())
	    << "the click was not recognised as belonging to a closed shop, so it would reach vanilla Repair";
	EXPECT_FALSE(IsAnyShopServiceCursorArmed()) << "consuming it must clear the state";

	// A second click has nothing left to consume, so an ordinary Repair SKILL still works - the
	// guard must not swallow legitimate skill use.
	EXPECT_FALSE(ConsumeStaleShopServiceCursor())
	    << "with no shop state armed this must stand aside and let the class skill run";
}

// The Recharge twin, against _iMaxCharges.
TEST_F(StoresTest, StaleRechargeCursorIsConsumedRatherThanRunAsTheVanillaSkill)
{
	ArmShopRechargeCursor();
	ASSERT_TRUE(IsAnyShopServiceCursorArmed());

	stextflag = TalkID::None;

	EXPECT_FALSE(IsShopRechargeCursorArmed());
	EXPECT_TRUE(ConsumeStaleShopServiceCursor());
	EXPECT_FALSE(IsAnyShopServiceCursorArmed());
}

/**
 * @brief Leaves the backpack with exactly @p headroom gold of space and the Stash pool full.
 *
 * Every cell holds gold: all but one at MaxGold, so they take nothing more, and one short by
 * exactly @p headroom. With Stash.gold at its INT_MAX cap the single-player pool contributes
 * nothing either, so the sale-fit gate has one number to compare against and the test controls it.
 */
void FillBackpackLeavingGoldHeadroom(int headroom)
{
	devilution::Player &player = *MyPlayer;
	InitializeItem(player.InvList[0], IDI_GOLD);
	player.InvList[0]._ivalue = MaxGold;
	InitializeItem(player.InvList[1], IDI_GOLD);
	player.InvList[1]._ivalue = MaxGold - headroom;
	player._pNumInv = 2;
	for (int8_t &cell : player.InvGrid)
		cell = 1; // InvList[0], which is full
	player.InvGrid[0] = 2;
	Stash.gold = std::numeric_limits<int>::max();
}

/**
 * @brief The sale-fit gate is fed the sale PRICE, not the item's value.
 *
 * External audit of v1.9.97, finding 3. StoreGoldFit used to read `item._iIvalue` itself, which is
 * the sale price only on a storehold display copy and the item's full value everywhere else. For an
 * ordinary item that made the gate four times too strict - annoying, never lossy. For a STACK it
 * made it far too lax, because GetItemSellValue multiplies a stackable consumable by its count: a
 * 99-potion stack was approved against one potion's value and then paid at ninety-nine quarters of
 * it.
 *
 * The headroom below sits deliberately between the two numbers, so the gate must answer differently
 * for each. Passing the wrong one is not a near miss here; it is the whole verdict.
 */
TEST_F(StoresTest, SaleFitGateAnswersOnThePriceNotTheItemValue)
{
	constexpr int Headroom = 50000;
	FillBackpackLeavingGoldHeadroom(Headroom);

	devilution::Item stack = {};
	InitializeItem(stack, IDI_HEAL);
	stack._ivalue = stack._iIvalue = 40000;
	stack.setStackCount(99);

	const int price = GetItemSellValue(stack);
	ASSERT_GT(price, Headroom) << "the stack must not fit";
	ASSERT_LE(stack._iIvalue, Headroom) << "the old number must fit, or the test proves nothing";

	EXPECT_TRUE(StoreGoldFitForTest(stack._iIvalue, /*itemFreeingCells=*/nullptr))
	    << "the value the gate used to read";
	EXPECT_FALSE(StoreGoldFitForTest(price, /*itemFreeingCells=*/nullptr))
	    << "the price the sale actually credits";
}

/** @brief The same discrepancy through the gesture that has it: selling the held stack. */
TEST_F(StoresTest, SellingAHeldStackThatCannotBePaidForIsRefused)
{
	constexpr int Headroom = 50000;
	FillBackpackLeavingGoldHeadroom(Headroom);

	devilution::Player &player = *MyPlayer;
	InitializeItem(player.HoldItem, IDI_HEAL);
	player.HoldItem._ivalue = player.HoldItem._iIvalue = 40000;
	player.HoldItem.setStackCount(99);
	const devilution::Item held = player.HoldItem;

	// Adria's Buy tab: a shop screen whose vendor takes Misc items, which is what a potion is.
	stextflag = TalkID::WitchBuy;

	EXPECT_FALSE(ShopSellHeldItem()) << "the sale was approved and could not be paid for";
	EXPECT_FALSE(player.HoldItem.isEmpty()) << "the stack left the cursor anyway";
	EXPECT_EQ(player.HoldItem.stackCount(), held.stackCount());
	EXPECT_EQ(Stash.gold, std::numeric_limits<int>::max()) << "the pool was credited for a refused sale";
}

/**
 * @brief Buying one premium item restocks one slot, and never resurrects a trimmed one.
 *
 * External audit of v1.9.97, finding 1. A purchase used to call SpawnPremium, whose refill branch
 * fills EVERY empty vanilla slot whenever numpremium is below the array size - so the holes the
 * one-page trim had deliberately made all came back at once, and the shelf grew past a page without
 * the player pressing Refresh.
 */
TEST_F(StoresTest, RestockingOnePremiumSlotDoesNotRefillTheTrimmedOnes)
{
	// A real player and a live net provider, because the purchase below places the item into the
	// backpack and that path sends a net command - the same setup
	// SmithConsumablesBuy_AfterClearingSlotWithStaleMatchingData_ItemIsActuallyPlaced needs.
	SNetInitializeProvider(SELCONN_LOOPBACK, nullptr);
	Players.resize(1);
	CreatePlayer(Players[0], HeroClass::Warrior);
	MyPlayer = &Players[0];
	gbIsMultiplayer = false;
	for (int i = 0; i < InventoryGridCells; i++) {
		MyPlayer->InvList[i].clear();
		MyPlayer->InvGrid[i] = 0;
	}
	MyPlayer->_pNumInv = 0;

	// A shelf of body armour - six cells each - so thirty of them cannot fit one page and
	// the trim has real work to do.
	for (devilution::Item &item : premiumitems) {
		item = {};
		InitializeItem(item, IDI_ORACOOL_LEATHER_ARMOR);
		item._iCurs = ICURS_FULL_PLATE_MAIL; // 2x3 cells: thirty of these cannot fit a 10x16 page
		item._iIdentified = true;
		item._iStatFlag = true;
	}
	RecountPremiumStock();
	ASSERT_EQ(numpremium, SMITH_PREMIUM_ITEMS);

	oracool::TrimShopStockToOnePage(TalkID::SmithPremiumBuy);
	RecountPremiumStock();
	ASSERT_TRUE(oracool::ShopStockFitsOnePage(TalkID::SmithPremiumBuy));

	std::array<bool, SMITH_PREMIUM_ITEMS> wasEmpty {};
	int sold = -1;
	int trimmed = 0;
	for (int i = 0; i < SMITH_PREMIUM_ITEMS; i++) {
		wasEmpty[i] = premiumitems[i].isEmpty();
		if (wasEmpty[i])
			trimmed++;
		else if (sold < 0)
			sold = i;
	}
	ASSERT_GT(trimmed, 0) << "nothing was trimmed, so there is no hole to resurrect";
	ASSERT_GE(sold, 0) << "nothing survived the trim, so there is nothing to buy";

	// Through the PURCHASE, not through the restock helper directly: the defect was in what the
	// purchase called, so a test that calls the replacement helper itself would pass with the
	// defect still in place.
	MyPlayer->_pGold = std::numeric_limits<int>::max() / 2;
	stextflag = TalkID::SmithPremiumBuy;
	devilution::Item bought = premiumitems[sold];
	SimulateSmithPremiumBuyForTest(/*visible index of the first surviving slot=*/0, bought);
	ASSERT_TRUE(premiumitems[sold].isEmpty() || premiumitems[sold]._iSeed != bought._iSeed)
	    << "the purchase did not go through, so nothing below is being tested";

	for (int i = 0; i < SMITH_PREMIUM_ITEMS; i++) {
		if (i == sold)
			continue;
		EXPECT_EQ(premiumitems[i].isEmpty(), wasEmpty[i])
		    << "slot " << i << " changed occupancy, and only the sold slot may";
	}
}

/**
 * @brief Supplies is materialised, and Pepin's potions survive materialising it.
 *
 * External audit of v1.9.97, finding 2. Adria's array was trimmed against her OWN page, where it
 * has the grid to itself; Supplies shows Pepin's four potions first, so her last few items fell off
 * that shelf while staying alive in witchitem - a reserve that surfaced the moment a visible
 * Supplies item was bought. The trim now runs against Supplies, the tighter of the two pages, and
 * refuses to clear the protected potions while doing it.
 */
TEST_F(StoresTest, SuppliesHasNoHiddenReserveBehindPepinsPotions)
{
	// Adria's whole array full of body armour, which is far more than one page - so
	// the trim has to discard, and the potions are in the way while it does.
	for (devilution::Item &item : witchitem) {
		item = {};
		InitializeItem(item, IDI_ORACOOL_LEATHER_ARMOR);
		item._iIdentified = true;
		item._iStatFlag = true;
	}
	TrimWitchStockToOnePageForTest();

	int potions = 0;
	for (const oracool::ShopSlot &slot : GetShopStock(TalkID::SmithConsumables)) {
		if (slot.neverTrim)
			potions++;
	}
	EXPECT_GT(potions, 0) << "the protected prefix vanished";

	EXPECT_TRUE(oracool::ShopStockFitsOnePage(TalkID::SmithConsumables))
	    << "Supplies still holds stock it cannot show";
	// Supplies is strictly the smaller page, so settling it settles Adria's own tab too.
	EXPECT_TRUE(oracool::ShopStockFitsOnePage(TalkID::WitchBuy))
	    << "Adria's own tab holds stock it cannot show";

	// Trimming again must be a fixed point: it must not eat into the potions on a second pass.
	TrimWitchStockToOnePageForTest();
	int potionsAfter = 0;
	for (const oracool::ShopSlot &slot : GetShopStock(TalkID::SmithConsumables)) {
		if (slot.neverTrim)
			potionsAfter++;
	}
	EXPECT_EQ(potionsAfter, potions) << "a second trim cleared Pepin's potions";
}
} // namespace
