#include <algorithm>
#include <array>

#include <gtest/gtest.h>

#include "items.h"
#include "options.h"
#include "player.h"
#include "qol/stash.h"
#include "storm/storm_net.hpp"
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
TEST(Stores, SmithConsumablesBuy_AfterClearingSlotWithStaleMatchingData_ItemIsActuallyPlaced)
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

// Oracool Tabbed Inventory: Griswold's Sell Items list previously only ever scanned InvList and
// the belt, so an item moved into one of the 9 extra tabs was invisible to him - it never
// appeared in the sell list at all, even though it was a perfectly ordinary sellable item.
TEST(Stores, SmithSell_ListsItemStoredInExtraTab)
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
TEST(Stores, StorytellerIdentify_ListsAndIdentifiesItemStoredInExtraTab)
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

TEST(Stores, SmithSell_StackedConsumable_PricedByQuantity)
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
TEST(Stores, WitchSell_SortsByPriceHighestFirst)
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

// The Repair list used to keep whatever fixed order StartSmithRepair populated it in (equipped
// slots first, then inventory in slot order), unlike the Sell list which already sorts by price.
// It should now sort by repair cost, descending - AddStoreHoldRepair already overwrites _iIvalue
// with the computed cost before storing the entry, so sorting on that field ranks by "what you'd
// pay to fix this," not the item's own value.
TEST(Stores, SmithRepair_SortsByRepairCostDescending)
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
TEST(Stores, SmithSell_CleaverUniqueIsSellableDespiteQuestIdRange)
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
TEST(Stores, SmithSell_OtherQuestRangeUniquesAreSellableToo)
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
TEST(Stores, SmithBuy_EmptyStock_StaysOnBuyScreenInsteadOfBouncingOut)
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
TEST(Stores, SmithSell_FourItemPage_SellAllRowNotHijackedByPremiumRedirect)
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
		item._iPrePower = IPL_LIGHTRES;
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

// User request: "Sort Stash" (Gillian's dialog) should sort by item category (Weapons, Armor,
// Helms, Shields, Jewelry, then everything else), descending price within each category.
TEST(Stores, SortStash_OrdersByCategoryThenDescendingPrice)
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
} // namespace
