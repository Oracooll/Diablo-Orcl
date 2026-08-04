#include <algorithm>

#include <gtest/gtest.h>

#include "control.h"
#include "cursor.h"
#include "diablo.h"
#include "inv.h"
#include "options.h"
#include "panels/ui_panels.hpp"
#include "player.h"
#include "qol/stash.h"
#include "storm/storm_net.hpp"

namespace devilution {
namespace {

class InvTest : public ::testing::Test {
public:
	void SetUp() override
	{
		Players.resize(1);
		MyPlayer = &Players[0];
		ActiveInventoryTab = 0;
		MyPlayer->InvTabList = {};
		MyPlayer->InvTabGrid = {};
		MyPlayer->_pNumInvTab = {};
	}
};

/* Set up a given item as a spell scroll, allowing for its usage. */
void set_up_scroll(Item &item, SpellID spell)
{
	pcurs = CURSOR_HAND;
	leveltype = DTYPE_CATACOMBS;
	MyPlayer->_pRSpell = static_cast<SpellID>(spell);
	item._itype = ItemType::Misc;
	item._iMiscId = IMISC_SCROLL;
	item._iSpell = spell;
}

/* Clear the inventory of MyPlayerId. */
void clear_inventory()
{
	for (int i = 0; i < InventoryGridCells; i++) {
		MyPlayer->InvList[i] = {};
		MyPlayer->InvGrid[i] = 0;
	}
	MyPlayer->_pNumInv = 0;
}

// Test that the scroll is used in the inventory in correct conditions
TEST_F(InvTest, UseScroll_from_inventory)
{
	set_up_scroll(MyPlayer->InvList[2], SpellID::Firebolt);
	MyPlayer->_pNumInv = 5;
	EXPECT_TRUE(CanUseScroll(*MyPlayer, SpellID::Firebolt));
}

// Test that the scroll is used in the belt in correct conditions
TEST_F(InvTest, UseScroll_from_belt)
{
	set_up_scroll(MyPlayer->SpdList[2], SpellID::Firebolt);
	EXPECT_TRUE(CanUseScroll(*MyPlayer, SpellID::Firebolt));
}

// A stacked scroll must decrement by one per use, and only vanish once its stack
// count reaches zero, instead of the slot disappearing on the first use.
TEST_F(InvTest, ConsumeScroll_stackedScroll_decrementsUntilEmpty)
{
	SNetInitializeProvider(SELCONN_LOOPBACK, nullptr);
	gbIsMultiplayer = false;
	clear_inventory();

	Item &scroll = MyPlayer->InvList[2];
	set_up_scroll(scroll, SpellID::Firebolt);
	scroll.setStackCount(3);
	MyPlayer->_pNumInv = 5;

	MyPlayer->executedSpell.spellId = SpellID::Firebolt;

	MyPlayer->executedSpell.spellFrom = INVITEM_INV_FIRST + 2;
	ConsumeScroll(*MyPlayer);
	ASSERT_FALSE(MyPlayer->InvList[2].isEmpty());
	EXPECT_EQ(MyPlayer->InvList[2].stackCount(), 2);

	MyPlayer->executedSpell.spellFrom = INVITEM_INV_FIRST + 2;
	ConsumeScroll(*MyPlayer);
	ASSERT_FALSE(MyPlayer->InvList[2].isEmpty());
	EXPECT_EQ(MyPlayer->InvList[2].stackCount(), 1);

	MyPlayer->executedSpell.spellFrom = INVITEM_INV_FIRST + 2;
	ConsumeScroll(*MyPlayer);
	EXPECT_TRUE(MyPlayer->InvList[2].isEmpty());
}

// Test that the scroll is not used in the inventory for each invalid condition
TEST_F(InvTest, UseScroll_from_inventory_invalid_conditions)
{
	// Empty the belt to prevent using a scroll from the belt
	for (int i = 0; i < MaxBeltItems; i++) {
		MyPlayer->SpdList[i].clear();
	}

	// Adjust inventory size
	MyPlayer->_pNumInv = 5;

	set_up_scroll(MyPlayer->InvList[2], SpellID::Firebolt);
	leveltype = DTYPE_TOWN;
	EXPECT_FALSE(CanUseScroll(*MyPlayer, SpellID::Firebolt));

	set_up_scroll(MyPlayer->InvList[2], SpellID::Firebolt);
	MyPlayer->_pRSpell = SpellID::Healing;
	EXPECT_FALSE(CanUseScroll(*MyPlayer, SpellID::Healing));

	set_up_scroll(MyPlayer->InvList[2], SpellID::Firebolt);
	MyPlayer->InvList[2]._iMiscId = IMISC_STAFF;
	EXPECT_FALSE(CanUseScroll(*MyPlayer, SpellID::Firebolt));

	set_up_scroll(MyPlayer->InvList[2], SpellID::Firebolt);
	MyPlayer->InvList[2].clear();
	EXPECT_FALSE(CanUseScroll(*MyPlayer, SpellID::Firebolt));
}

// Test that the scroll is not used in the belt for each invalid condition
TEST_F(InvTest, UseScroll_from_belt_invalid_conditions)
{
	// Disable the inventory to prevent using a scroll from the inventory
	MyPlayer->_pNumInv = 0;

	set_up_scroll(MyPlayer->SpdList[2], SpellID::Firebolt);
	leveltype = DTYPE_TOWN;
	EXPECT_FALSE(CanUseScroll(*MyPlayer, SpellID::Firebolt));

	set_up_scroll(MyPlayer->SpdList[2], SpellID::Firebolt);
	MyPlayer->_pRSpell = SpellID::Healing;
	EXPECT_FALSE(CanUseScroll(*MyPlayer, SpellID::Healing));

	set_up_scroll(MyPlayer->SpdList[2], SpellID::Firebolt);
	MyPlayer->SpdList[2]._iMiscId = IMISC_STAFF;
	EXPECT_FALSE(CanUseScroll(*MyPlayer, SpellID::Firebolt));

	set_up_scroll(MyPlayer->SpdList[2], SpellID::Firebolt);
	MyPlayer->SpdList[2].clear();
	EXPECT_FALSE(CanUseScroll(*MyPlayer, SpellID::Firebolt));
}

// Test gold calculation
TEST_F(InvTest, CalculateGold)
{
	MyPlayer->_pNumInv = 10;
	// Set up 4 slots of gold in the inventory
	MyPlayer->InvList[1]._itype = ItemType::Gold;
	MyPlayer->InvList[5]._itype = ItemType::Gold;
	MyPlayer->InvList[2]._itype = ItemType::Gold;
	MyPlayer->InvList[3]._itype = ItemType::Gold;
	// Set the gold amount to arbitrary values
	MyPlayer->InvList[1]._ivalue = 100;
	MyPlayer->InvList[5]._ivalue = 200;
	MyPlayer->InvList[2]._ivalue = 3;
	MyPlayer->InvList[3]._ivalue = 30;

	EXPECT_EQ(CalculateGold(*MyPlayer), 333);
}

// Test automatic gold placing
TEST_F(InvTest, GoldAutoPlace)
{
	SNetInitializeProvider(SELCONN_LOOPBACK, nullptr);

	// Empty the inventory
	clear_inventory();

	// Put gold into the inventory:
	// | 1000 | ... | ...
	MyPlayer->InvList[0]._itype = ItemType::Gold;
	MyPlayer->InvList[0]._ivalue = 1000;
	MyPlayer->_pNumInv = 1;
	// Put (max gold - 100) gold, which is 4900, into the player's hand
	MyPlayer->HoldItem._itype = ItemType::Gold;
	MyPlayer->HoldItem._ivalue = GOLD_MAX_LIMIT - 100;

	GoldAutoPlace(*MyPlayer, MyPlayer->HoldItem);
	// We expect the inventory:
	// | 5000 | 900 | ...
	EXPECT_EQ(MyPlayer->InvList[0]._ivalue, GOLD_MAX_LIMIT);
	EXPECT_EQ(MyPlayer->InvList[1]._ivalue, 900);
}

Item MakeStackablePotion(_item_indexes idx = IDI_HEAL, bool identified = true, int count = 1)
{
	Item item;
	item._itype = ItemType::Misc;
	item._iClass = ICLASS_MISC;
	item._iMiscId = IMISC_HEAL;
	item.IDidx = idx;
	item._iIdentified = identified;
	item.setStackCount(count);
	return item;
}

// Regression test for a real bug: with an empty inventory and belt, the merge pre-pass
// in AutoPlaceItemInBelt/AutoPlaceItemInInventory scanned every slot for a stack to merge
// into, including cleared (isEmpty()) slots whose stale leftover fields could look like a
// matching item. See items_test.cpp's IsStackableConsumable_ExcludesClearedItemDespiteStaleMatchingFields
// for the underlying Item-level fix.
TEST_F(InvTest, AutoPlaceItem_FreshPurchaseWithEmptyInventoryAndBelt_ItemIsActuallyPlaced)
{
	SNetInitializeProvider(SELCONN_LOOPBACK, nullptr);
	gbIsMultiplayer = false;
	clear_inventory();
	for (auto &beltItem : MyPlayer->SpdList)
		beltItem.clear();

	Item purchased = MakeStackablePotion(IDI_HEAL, true, 1);
	bool placedOnBelt = AutoPlaceItemInBelt(*MyPlayer, purchased, true);
	bool placedInInv = false;
	if (!placedOnBelt)
		placedInInv = AutoPlaceItemInInventory(*MyPlayer, purchased, true);

	EXPECT_TRUE(placedOnBelt || placedInInv) << "Item was not placed anywhere";
	if (placedOnBelt) {
		bool foundOnBelt = false;
		for (auto &beltItem : MyPlayer->SpdList) {
			if (!beltItem.isEmpty() && beltItem.IDidx == IDI_HEAL)
				foundOnBelt = true;
		}
		EXPECT_TRUE(foundOnBelt) << "AutoPlaceItemInBelt returned true but item is not actually in SpdList";
	} else if (placedInInv) {
		bool foundInInv = false;
		for (int i = 0; i < MyPlayer->_pNumInv; i++) {
			if (MyPlayer->InvList[i].IDidx == IDI_HEAL)
				foundInInv = true;
		}
		EXPECT_TRUE(foundInInv) << "AutoPlaceItemInInventory returned true but item is not actually in InvList";
	}
}

TEST_F(InvTest, MergeStackableItemIntoInventory_mergesIntoExistingStack)
{
	SNetInitializeProvider(SELCONN_LOOPBACK, nullptr);
	clear_inventory();
	gbIsMultiplayer = false;

	MyPlayer->InvList[0] = MakeStackablePotion(IDI_HEAL, true, 5);
	MyPlayer->InvGrid[0] = 1;
	MyPlayer->_pNumInv = 1;

	Item incoming = MakeStackablePotion(IDI_HEAL, true, 1);
	EXPECT_TRUE(AutoPlaceItemInInventory(*MyPlayer, incoming, true));
	EXPECT_EQ(MyPlayer->InvList[0].stackCount(), 6);
	EXPECT_EQ(MyPlayer->_pNumInv, 1); // no new slot was used
}

TEST_F(InvTest, MergeStackableItemIntoInventory_fallsBackToNewSlotWhenStackFull)
{
	SNetInitializeProvider(SELCONN_LOOPBACK, nullptr);
	clear_inventory();
	gbIsMultiplayer = false;

	MyPlayer->InvList[0] = MakeStackablePotion(IDI_HEAL, true, Item::MaxStackCount);
	MyPlayer->InvGrid[0] = 1;
	MyPlayer->_pNumInv = 1;

	Item incoming = MakeStackablePotion(IDI_HEAL, true, 1);
	EXPECT_TRUE(AutoPlaceItemInInventory(*MyPlayer, incoming, true));
	EXPECT_EQ(MyPlayer->InvList[0].stackCount(), Item::MaxStackCount); // untouched
	EXPECT_EQ(MyPlayer->_pNumInv, 2);                                  // placed in a new slot instead
}

TEST_F(InvTest, MergeStackableItemIntoInventory_identifiedStateMismatchDoesNotMerge)
{
	SNetInitializeProvider(SELCONN_LOOPBACK, nullptr);
	clear_inventory();
	gbIsMultiplayer = false;

	MyPlayer->InvList[0] = MakeStackablePotion(IDI_HEAL, /*identified=*/true, 5);
	MyPlayer->InvGrid[0] = 1;
	MyPlayer->_pNumInv = 1;

	Item incoming = MakeStackablePotion(IDI_HEAL, /*identified=*/false, 1);
	EXPECT_TRUE(AutoPlaceItemInInventory(*MyPlayer, incoming, true));
	EXPECT_EQ(MyPlayer->InvList[0].stackCount(), 5); // untouched
	EXPECT_EQ(MyPlayer->_pNumInv, 2);                // separate slot for the unidentified one
}

TEST_F(InvTest, MergeStackableItemIntoBelt_mergesIntoExistingStack)
{
	SNetInitializeProvider(SELCONN_LOOPBACK, nullptr);
	gbIsMultiplayer = false;
	for (auto &beltItem : MyPlayer->SpdList)
		beltItem.clear();

	MyPlayer->SpdList[0] = MakeStackablePotion(IDI_HEAL, true, 5);

	Item incoming = MakeStackablePotion(IDI_HEAL, true, 1);
	EXPECT_TRUE(AutoPlaceItemInBelt(*MyPlayer, incoming, true));
	EXPECT_EQ(MyPlayer->SpdList[0].stackCount(), 6);
	EXPECT_TRUE(MyPlayer->SpdList[1].isEmpty()); // no second belt slot used
}

// Belt Mod: consuming the last unit on a belt slot should refill it in one batch from
// a matching inventory stack, draining (and removing) that inventory slot.
TEST_F(InvTest, DecrementOrRemoveSpdBarItem_lastUnitConsumed_refillsFromMatchingInventoryStack)
{
	SNetInitializeProvider(SELCONN_LOOPBACK, nullptr);
	gbIsMultiplayer = false;
	clear_inventory();
	for (auto &beltItem : MyPlayer->SpdList)
		beltItem.clear();

	MyPlayer->SpdList[0] = MakeStackablePotion(IDI_HEAL, true, 1);
	MyPlayer->InvList[0] = MakeStackablePotion(IDI_HEAL, true, 10);
	MyPlayer->InvGrid[0] = 1;
	MyPlayer->_pNumInv = 1;

	DecrementOrRemoveSpdBarItem(*MyPlayer, 0);

	ASSERT_FALSE(MyPlayer->SpdList[0].isEmpty());
	EXPECT_EQ(MyPlayer->SpdList[0].stackCount(), 10);
	EXPECT_EQ(MyPlayer->_pNumInv, 0); // the drained inventory stack was fully removed
}

// Refill should keep drawing from subsequent matching inventory stacks (in scan order)
// until the belt slot hits the cap, partially draining the last stack it touches.
TEST_F(InvTest, RefillBeltSlotFromInventory_combinesMultipleMatchingStacksUpToCap)
{
	SNetInitializeProvider(SELCONN_LOOPBACK, nullptr);
	gbIsMultiplayer = false;
	clear_inventory();
	for (auto &beltItem : MyPlayer->SpdList)
		beltItem.clear();

	MyPlayer->InvList[0] = MakeStackablePotion(IDI_HEAL, true, 60);
	MyPlayer->InvGrid[0] = 1;
	MyPlayer->InvList[1] = MakeStackablePotion(IDI_HEAL, true, 60);
	MyPlayer->InvGrid[1] = 2;
	MyPlayer->_pNumInv = 2;

	RefillBeltSlotFromInventory(*MyPlayer, 0, IDI_HEAL, true);

	ASSERT_FALSE(MyPlayer->SpdList[0].isEmpty());
	EXPECT_EQ(MyPlayer->SpdList[0].stackCount(), Item::MaxStackCount);
	ASSERT_EQ(MyPlayer->_pNumInv, 1); // the first (fully-drained) stack was removed
	EXPECT_EQ(MyPlayer->InvList[0].stackCount(), 60 + 60 - Item::MaxStackCount);
}

// Only an inventory stack matching both IDidx and identified state should be used.
TEST_F(InvTest, RefillBeltSlotFromInventory_identifiedStateMismatchIsSkipped)
{
	SNetInitializeProvider(SELCONN_LOOPBACK, nullptr);
	gbIsMultiplayer = false;
	clear_inventory();
	for (auto &beltItem : MyPlayer->SpdList)
		beltItem.clear();

	MyPlayer->InvList[0] = MakeStackablePotion(IDI_HEAL, false, 10);
	MyPlayer->InvGrid[0] = 1;
	MyPlayer->_pNumInv = 1;

	RefillBeltSlotFromInventory(*MyPlayer, 0, IDI_HEAL, true);

	EXPECT_TRUE(MyPlayer->SpdList[0].isEmpty());
	EXPECT_EQ(MyPlayer->_pNumInv, 1); // untouched
}

// With no matching inventory stock anywhere, the belt slot simply stays empty.
TEST_F(InvTest, RefillBeltSlotFromInventory_noMatchingStock_leavesSlotEmpty)
{
	SNetInitializeProvider(SELCONN_LOOPBACK, nullptr);
	gbIsMultiplayer = false;
	clear_inventory();
	for (auto &beltItem : MyPlayer->SpdList)
		beltItem.clear();

	RefillBeltSlotFromInventory(*MyPlayer, 0, IDI_HEAL, true);

	EXPECT_TRUE(MyPlayer->SpdList[0].isEmpty());
}

// User-reported bug: Belt Mod's refill only ever scanned the real backpack (tab 1), so a
// matching stack sitting in one of the 9 Tabbed Inventory extra tabs was invisible to it -
// the belt slot stayed empty even though the player clearly had more of the potion.
TEST_F(InvTest, RefillBeltSlotFromInventory_fallsBackToExtraTabWhenTab1HasNoMatch)
{
	SNetInitializeProvider(SELCONN_LOOPBACK, nullptr);
	gbIsMultiplayer = false;
	clear_inventory();
	for (auto &beltItem : MyPlayer->SpdList)
		beltItem.clear();

	MyPlayer->InvTabList[2][0] = MakeStackablePotion(IDI_HEAL, true, 10);
	MyPlayer->InvTabGrid[2][0] = 1;
	MyPlayer->_pNumInvTab[2] = 1;

	RefillBeltSlotFromInventory(*MyPlayer, 0, IDI_HEAL, true);

	ASSERT_FALSE(MyPlayer->SpdList[0].isEmpty());
	EXPECT_EQ(MyPlayer->SpdList[0].stackCount(), 10);
	EXPECT_EQ(MyPlayer->_pNumInvTab[2], 0); // the drained extra-tab stack was fully removed
}

// The real backpack must still be drained first, in scan order, before any extra tab is
// touched at all - the extra tabs are only a fallback once tab 1 has nothing left to give.
TEST_F(InvTest, RefillBeltSlotFromInventory_combinesTab1AndExtraTabUpToCap)
{
	SNetInitializeProvider(SELCONN_LOOPBACK, nullptr);
	gbIsMultiplayer = false;
	clear_inventory();
	for (auto &beltItem : MyPlayer->SpdList)
		beltItem.clear();

	MyPlayer->InvList[0] = MakeStackablePotion(IDI_HEAL, true, 60);
	MyPlayer->InvGrid[0] = 1;
	MyPlayer->_pNumInv = 1;
	MyPlayer->InvTabList[2][0] = MakeStackablePotion(IDI_HEAL, true, 60);
	MyPlayer->InvTabGrid[2][0] = 1;
	MyPlayer->_pNumInvTab[2] = 1;

	RefillBeltSlotFromInventory(*MyPlayer, 0, IDI_HEAL, true);

	ASSERT_FALSE(MyPlayer->SpdList[0].isEmpty());
	EXPECT_EQ(MyPlayer->SpdList[0].stackCount(), Item::MaxStackCount);
	EXPECT_EQ(MyPlayer->_pNumInv, 0);                                            // tab 1's stack was fully drained and removed
	EXPECT_EQ(MyPlayer->InvTabList[2][0].stackCount(), 60 + 60 - Item::MaxStackCount); // the remainder stays in the extra tab
}

TEST_F(InvTest, WithdrawGoldOnlyDeductsPlacedAmount)
{
	SNetInitializeProvider(SELCONN_LOOPBACK, nullptr);
	clear_inventory();
	MaxGold = GoldStackSaveLimit;
	for (int8_t &cell : MyPlayer->InvGrid)
		cell = 1;

	MyPlayer->InvList[0]._itype = ItemType::Gold;
	MyPlayer->InvList[0]._ivalue = GoldStackSaveLimit - 100;
	MyPlayer->_pNumInv = 1;
	MyPlayer->_pGold = GoldStackSaveLimit - 100;
	Stash.gold = 30000;
	Stash.dirty = false;

	EXPECT_EQ(WithdrawGold(*MyPlayer, 30000), 100);
	EXPECT_EQ(MyPlayer->InvList[0]._ivalue, GoldStackSaveLimit);
	EXPECT_EQ(MyPlayer->_pGold, GoldStackSaveLimit);
	EXPECT_EQ(Stash.gold, 29900);
	EXPECT_TRUE(Stash.dirty);

	MaxGold = GOLD_MAX_LIMIT;
}

// Items at 0 durability: in single-player, a broken item stays equipped (0 durability, flagged
// _iOracoolBroken) instead of being hard-deleted, so it can later be repaired.
TEST_F(InvTest, BreakOrRemoveEquipment_SinglePlayer_MarksBrokenInsteadOfRemoving)
{
	gbIsMultiplayer = false;
	MyPlayer->InvBody[INVLOC_HAND_LEFT].clear();
	MyPlayer->InvBody[INVLOC_HAND_LEFT]._itype = ItemType::Sword;

	BreakOrRemoveEquipment(*MyPlayer, INVLOC_HAND_LEFT, false);

	EXPECT_FALSE(MyPlayer->InvBody[INVLOC_HAND_LEFT].isEmpty()) << "item should stay equipped, not be deleted";
	EXPECT_EQ(MyPlayer->InvBody[INVLOC_HAND_LEFT]._iDurability, 0);
	EXPECT_TRUE(MyPlayer->InvBody[INVLOC_HAND_LEFT]._iOracoolBroken);
}

// Multiplayer has no packet format for "broken but still equipped," so it keeps the vanilla
// hard-delete behavior unchanged.
TEST_F(InvTest, BreakOrRemoveEquipment_Multiplayer_RemovesItemAsBefore)
{
	SNetInitializeProvider(SELCONN_LOOPBACK, nullptr);
	gbIsMultiplayer = true;
	MyPlayer->InvBody[INVLOC_HAND_LEFT].clear();
	MyPlayer->InvBody[INVLOC_HAND_LEFT]._itype = ItemType::Sword;

	BreakOrRemoveEquipment(*MyPlayer, INVLOC_HAND_LEFT, false);

	EXPECT_TRUE(MyPlayer->InvBody[INVLOC_HAND_LEFT].isEmpty());

	gbIsMultiplayer = false;
}

// Test removing an item from inventory with no other items.
TEST_F(InvTest, RemoveInvItem)
{
	SNetInitializeProvider(SELCONN_LOOPBACK, nullptr);

	clear_inventory();
	// Put a two-slot misc item into the inventory:
	// | (item) | (item) | ... | ...
	MyPlayer->_pNumInv = 1;
	MyPlayer->InvGrid[0] = 1;
	MyPlayer->InvGrid[1] = -1;
	MyPlayer->InvList[0]._itype = ItemType::Misc;

	MyPlayer->RemoveInvItem(0);
	EXPECT_EQ(MyPlayer->InvGrid[0], 0);
	EXPECT_EQ(MyPlayer->InvGrid[1], 0);
	EXPECT_EQ(MyPlayer->_pNumInv, 0);
}

// Test removing an item from inventory with other items in it.
TEST_F(InvTest, RemoveInvItem_other_item)
{
	SNetInitializeProvider(SELCONN_LOOPBACK, nullptr);

	clear_inventory();
	// Put a two-slot misc item and a ring into the inventory:
	// | (item) | (item) | (ring) | ...
	MyPlayer->_pNumInv = 2;
	MyPlayer->InvGrid[0] = 1;
	MyPlayer->InvGrid[1] = -1;
	MyPlayer->InvList[0]._itype = ItemType::Misc;

	MyPlayer->InvGrid[2] = 2;
	MyPlayer->InvList[1]._itype = ItemType::Ring;

	MyPlayer->RemoveInvItem(0);
	EXPECT_EQ(MyPlayer->InvGrid[0], 0);
	EXPECT_EQ(MyPlayer->InvGrid[1], 0);
	EXPECT_EQ(MyPlayer->InvGrid[2], 1);
	EXPECT_EQ(MyPlayer->InvList[0]._itype, ItemType::Ring);
	EXPECT_EQ(MyPlayer->_pNumInv, 1);
}

// Diagnostic: reproduces the user's exact reported workflow through the real CheckInvPaste entry
// point (not just the accessor helpers), to find why placing an item into an extra tab fails.
TEST_F(InvTest, TabbedInventory_CheckInvPasteIntoExtraTab)
{
	SNetInitializeProvider(SELCONN_LOOPBACK, nullptr);
	gbIsMultiplayer = false;
	clear_inventory();

	InspectPlayer = MyPlayer;
	MyPlayer->HoldItem._itype = ItemType::Misc;
	MyPlayer->HoldItem._iClass = ICLASS_MISC;

	ActiveInventoryTab = 2; // extra tab index 1

	MousePosition = GetPanelPosition(UiPanels::Inventory, InvRect[SLOTXY_INV_FIRST].position) + Displacement { InventorySlotSizeInPixels.width / 2, InventorySlotSizeInPixels.height / 2 };
	CheckInvItem();

	EXPECT_TRUE(MyPlayer->HoldItem.isEmpty()) << "item should have left the cursor";
	EXPECT_EQ(MyPlayer->_pNumInvTab[1], 1) << "item should have landed in extra tab index 1";
	EXPECT_EQ(MyPlayer->InvTabList[1][0]._itype, ItemType::Misc);
}

// Diagnostic: the user specifically reported failure with Primal items (not plain items).
// Reproduces a real Primal item, generated the same way "drop primal" would, then attempts
// to paste it into an extra tab through the real CheckInvItem entry point.
TEST_F(InvTest, TabbedInventory_PastePrimalItemIntoExtraTab)
{
	SNetInitializeProvider(SELCONN_LOOPBACK, nullptr);
	gbIsMultiplayer = false;
	clear_inventory();

	InspectPlayer = MyPlayer;

	Item primalItem;
	primalItem._itype = ItemType::Sword;
	primalItem._iClass = ICLASS_WEAPON;
	primalItem.IDidx = IDI_WARRIOR;
	primalItem._iIdentified = false;
	primalItem._iCurs = ICURS_GREAT_SWORD; // realistic multi-cell weapon icon, not the 1x1 default
	GetPrimalItemAffixes(*MyPlayer, primalItem, 1, 30, AffixItemType::Weapon, false);
	ASSERT_TRUE(primalItem.hasOracoolTier());
	MyPlayer->HoldItem = primalItem;

	ActiveInventoryTab = 2; // extra tab index 1

	MousePosition = GetPanelPosition(UiPanels::Inventory, InvRect[SLOTXY_INV_FIRST].position) + Displacement { InventorySlotSizeInPixels.width / 2, InventorySlotSizeInPixels.height / 2 };
	CheckInvItem();

	EXPECT_TRUE(MyPlayer->HoldItem.isEmpty()) << "Primal item should have left the cursor";
	EXPECT_EQ(MyPlayer->_pNumInvTab[1], 1) << "Primal item should have landed in extra tab index 1";
	EXPECT_TRUE(MyPlayer->InvTabList[1][0].hasOracoolTier());
}

// User-reported gap: hovering an item stored in an extra tab showed no name/stats anywhere.
// Root cause: CheckInvHLight only ever read the real InvGrid/InvList (tab 1), so it always
// reported "nothing here" for an extra tab's contents. Fixed to read through the same active-tab
// accessors used elsewhere; pcursinvitem itself still stays -1 for an extra tab (legacy identify/
// drag code assumes tab-1 indices), but InfoString/InfoColor are now populated correctly.
TEST_F(InvTest, TabbedInventory_HoverInExtraTabShowsItemInfo)
{
	clear_inventory();
	InspectPlayer = MyPlayer;

	MyPlayer->InvTabList[1][0]._itype = ItemType::Misc;
	MyPlayer->InvTabList[1][0].IDidx = IDI_ROCK;
	MyPlayer->InvTabList[1][0]._iIdentified = true;
	MyPlayer->InvTabGrid[1][0] = 1;
	MyPlayer->_pNumInvTab[1] = 1;

	ActiveInventoryTab = 2; // extra tab index 1
	MousePosition = GetPanelPosition(UiPanels::Inventory, InvRect[SLOTXY_INV_FIRST].position) + Displacement { InventorySlotSizeInPixels.width / 2, InventorySlotSizeInPixels.height / 2 };

	int8_t hovered = CheckInvHLight();

	EXPECT_EQ(hovered, -1) << "extra-tab items don't use the tab-1 pcursinvitem index encoding";
	EXPECT_FALSE(InfoString.empty()) << "hovering an extra-tab item should still populate the info text";
	EXPECT_TRUE(ActiveTabItemHovered) << "DrawInfoBox needs this flag, since pcursinvitem == -1 here doesn't mean \"hovering nothing\"";
}

// Oracool inventory sort button: repacks tab 1 (and, on overflow, the extra tabs) by sell value,
// most valuable item first. With everything fitting comfortably in tab 1, this just verifies the
// resulting InvList order is value-descending.
TEST_F(InvTest, SortInventoryBySellValue_OrdersRelocatableItemsDescending)
{
	SNetInitializeProvider(SELCONN_LOOPBACK, nullptr);
	gbIsMultiplayer = false;
	clear_inventory();

	auto setupItem = [](Item &item, int ivalue) {
		InitializeItem(item, IDI_WARRIOR);
		item._iMagical = ITEM_QUALITY_NORMAL;
		item._ivalue = ivalue;
		item._iIvalue = ivalue;
	};

	setupItem(MyPlayer->InvList[0], 40);
	MyPlayer->InvGrid[0] = 1;
	setupItem(MyPlayer->InvList[1], 400);
	MyPlayer->InvGrid[1] = 2;
	setupItem(MyPlayer->InvList[2], 200);
	MyPlayer->InvGrid[2] = 3;
	MyPlayer->_pNumInv = 3;

	SortInventoryBySellValue(*MyPlayer);

	ASSERT_EQ(MyPlayer->_pNumInv, 3);
	EXPECT_EQ(MyPlayer->InvList[0]._ivalue, 400);
	EXPECT_EQ(MyPlayer->InvList[1]._ivalue, 200);
	EXPECT_EQ(MyPlayer->InvList[2]._ivalue, 40);
}

// Gold and quest items must never move - checked by grid cell (spatial position), not InvList
// array index, since removing an unrelated relocatable item can still shuffle a pinned item's
// underlying array slot via RemoveInvItem's own swap-compaction (that bookkeeping keeps the grid
// cell correctly pointing at wherever the pinned item's data ends up, so its on-screen position
// never changes even though its array index might).
TEST_F(InvTest, SortInventoryBySellValue_LeavesGoldAtItsGridPosition)
{
	SNetInitializeProvider(SELCONN_LOOPBACK, nullptr);
	gbIsMultiplayer = false;
	clear_inventory();

	MyPlayer->InvList[0]._itype = ItemType::Gold;
	MyPlayer->InvList[0]._ivalue = 500;
	MyPlayer->InvGrid[0] = 1;

	InitializeItem(MyPlayer->InvList[1], IDI_WARRIOR);
	MyPlayer->InvList[1]._iMagical = ITEM_QUALITY_NORMAL;
	MyPlayer->InvList[1]._ivalue = 400;
	MyPlayer->InvList[1]._iIvalue = 400;
	MyPlayer->InvGrid[1] = 2;

	MyPlayer->_pNumInv = 2;

	SortInventoryBySellValue(*MyPlayer);

	const int goldListIndex = abs(MyPlayer->InvGrid[0]) - 1;
	ASSERT_GE(goldListIndex, 0);
	EXPECT_EQ(MyPlayer->InvList[goldListIndex]._itype, ItemType::Gold);
	EXPECT_EQ(MyPlayer->InvList[goldListIndex]._ivalue, 500);
}

TEST_F(InvTest, CheckInventorySortButtonClick_HitsButtonAndSorts)
{
	SNetInitializeProvider(SELCONN_LOOPBACK, nullptr);
	gbIsMultiplayer = false;
	clear_inventory();
	sgOptions.Oracool.inventorySortButton.SetValue(true);

	InitializeItem(MyPlayer->InvList[0], IDI_WARRIOR);
	MyPlayer->InvList[0]._iMagical = ITEM_QUALITY_NORMAL;
	MyPlayer->InvList[0]._ivalue = 40;
	MyPlayer->InvGrid[0] = 1;
	InitializeItem(MyPlayer->InvList[1], IDI_WARRIOR);
	MyPlayer->InvList[1]._iMagical = ITEM_QUALITY_NORMAL;
	MyPlayer->InvList[1]._ivalue = 400;
	MyPlayer->InvGrid[1] = 2;
	MyPlayer->_pNumInv = 2;

	MousePosition = GetPanelPosition(UiPanels::Inventory, InventorySortButtonPosition) + Displacement { InventorySortButtonSize.width / 2, InventorySortButtonSize.height / 2 };

	EXPECT_TRUE(CheckInventorySortButtonClick(MousePosition));
	EXPECT_EQ(MyPlayer->InvList[0]._ivalue, 400);
}

TEST_F(InvTest, CheckInventorySortButtonClick_MissesWhenOptionDisabled)
{
	clear_inventory();
	sgOptions.Oracool.inventorySortButton.SetValue(false);

	MousePosition = GetPanelPosition(UiPanels::Inventory, InventorySortButtonPosition) + Displacement { InventorySortButtonSize.width / 2, InventorySortButtonSize.height / 2 };

	EXPECT_FALSE(CheckInventorySortButtonClick(MousePosition));

	sgOptions.Oracool.inventorySortButton.SetValue(true);
}

// User-reported gap: Ctrl+Click-to-stash only ever worked for tab 1, since TransferItemToStash
// is driven by pcursinvitem, which stays -1 for an extra tab's items (see CheckInvHLight). Fixed
// via a dedicated hit-test that bypasses pcursinvitem entirely for extra tabs.
TEST_F(InvTest, TabbedInventory_CtrlClickTransfersExtraTabItemToStash)
{
	clear_inventory();

	MyPlayer->InvTabList[1][0]._itype = ItemType::Misc;
	MyPlayer->InvTabList[1][0].IDidx = IDI_ROCK;
	MyPlayer->InvTabList[1][0]._iIdentified = true;
	MyPlayer->InvTabGrid[1][0] = 1;
	MyPlayer->_pNumInvTab[1] = 1;

	ActiveInventoryTab = 2; // extra tab index 1
	MousePosition = GetPanelPosition(UiPanels::Inventory, InvRect[SLOTXY_INV_FIRST].position) + Displacement { InventorySlotSizeInPixels.width / 2, InventorySlotSizeInPixels.height / 2 };

	EXPECT_TRUE(TryTransferHoveredActiveTabItemToStash(*MyPlayer));

	EXPECT_EQ(MyPlayer->_pNumInvTab[1], 0) << "item should have left the extra tab";
	EXPECT_EQ(MyPlayer->InvTabGrid[1][0], 0);

	const bool foundInStash = std::any_of(Stash.stashList.begin(), Stash.stashList.end(), [](const Item &item) {
		return !item.isEmpty() && item.IDidx == IDI_ROCK;
	});
	EXPECT_TRUE(foundInStash) << "item should have landed in the stash";
}

TEST_F(InvTest, TabbedInventory_CtrlClickOnTab1DefersToLegacyPath)
{
	clear_inventory();
	ActiveInventoryTab = 0;

	EXPECT_FALSE(TryTransferHoveredActiveTabItemToStash(*MyPlayer)) << "tab 1 has its own pcursinvitem-driven path and must not be handled here";
}

// Oracool Tabbed Inventory: a store purchase that can't fit in the original backpack (tab 1)
// must still succeed if any extra tab has room - vendors previously only ever checked tab 1,
// rejecting a purchase as "no room" even with 9 empty tabs sitting unused.
TEST_F(InvTest, TabbedInventory_AutoPlaceItemInExtraTabs_PlacesWhenTab1IsFull)
{
	clear_inventory();

	Item item;
	item._itype = ItemType::Misc;

	bool placed = AutoPlaceItemInExtraTabs(*MyPlayer, item, true);

	EXPECT_TRUE(placed);
	EXPECT_EQ(MyPlayer->_pNumInvTab[0], 1) << "should land in the first extra tab (tab 2)";
	EXPECT_EQ(MyPlayer->InvTabList[0][0]._itype, ItemType::Misc);
	EXPECT_EQ(MyPlayer->_pNumInv, 0) << "tab 1 (the real InvList) must be untouched by this fallback";
}

// The fallback must try every extra tab, not just the first, once earlier ones are full.
TEST_F(InvTest, TabbedInventory_AutoPlaceItemInExtraTabs_SkipsFullTabs)
{
	clear_inventory();

	// Fill extra tab index 0 (displayed as tab 2) completely with 1x1 items.
	for (int i = 0; i < InventoryGridCells; i++) {
		MyPlayer->InvTabList[0][i]._itype = ItemType::Misc;
		MyPlayer->InvTabGrid[0][i] = static_cast<int8_t>(i + 1);
	}
	MyPlayer->_pNumInvTab[0] = InventoryGridCells;

	Item item;
	item._itype = ItemType::Misc;

	bool placed = AutoPlaceItemInExtraTabs(*MyPlayer, item, true);

	EXPECT_TRUE(placed);
	EXPECT_EQ(MyPlayer->_pNumInvTab[1], 1) << "should skip the full tab and land in the next one (tab 3)";
}

// AutoPlaceItemInInventory itself must fall back to the extra tabs once tab 1 has no room - this
// is the single function every ground pickup (manual and auto-pickup), Stash withdrawal, and
// vendor purchase already funnels through, so fixing it here covers all of them at once.
TEST_F(InvTest, TabbedInventory_AutoPlaceItemInInventory_FallsBackToExtraTabsWhenTab1IsFull)
{
	clear_inventory();

	// Fill tab 1 completely with 1x1 items.
	for (int i = 0; i < InventoryGridCells; i++) {
		MyPlayer->InvList[i]._itype = ItemType::Misc;
		MyPlayer->InvGrid[i] = static_cast<int8_t>(i + 1);
	}
	MyPlayer->_pNumInv = InventoryGridCells;

	Item item;
	item._itype = ItemType::Misc;

	bool placed = AutoPlaceItemInInventory(*MyPlayer, item, true);

	EXPECT_TRUE(placed) << "ground pickup (and everything else routed through this function) should spill into an extra tab";
	EXPECT_EQ(MyPlayer->_pNumInvTab[0], 1);
}

// Gold and quest items must never auto-place into an extra tab, even as a fallback - they need
// to stay in tab 1 where _pGold bookkeeping and the existing quest-scanning code (which only
// ever looks at InvList) can find them. A full tab 1 should make placement fail outright for
// these, not silently redirect them somewhere unexpected.
TEST_F(InvTest, TabbedInventory_AutoPlaceItemInInventory_NeverSendsQuestItemsToExtraTabs)
{
	clear_inventory();

	for (int i = 0; i < InventoryGridCells; i++) {
		MyPlayer->InvList[i]._itype = ItemType::Misc;
		MyPlayer->InvGrid[i] = static_cast<int8_t>(i + 1);
	}
	MyPlayer->_pNumInv = InventoryGridCells;

	Item questItem;
	questItem._itype = ItemType::Misc;
	questItem._iClass = ICLASS_QUEST;

	bool placed = AutoPlaceItemInInventory(*MyPlayer, questItem, true);

	EXPECT_FALSE(placed);
	for (int t = 0; t < Player::NumExtraInventoryTabs; t++)
		EXPECT_EQ(MyPlayer->_pNumInvTab[t], 0) << "tab index " << t << " must stay empty";
}

// Oracool Tabbed Inventory: writing through the accessors while an extra tab is active must
// never touch tab 1's real InvList/InvGrid/_pNumInv, and switching back to tab 0 must see the
// original data untouched.
TEST_F(InvTest, TabbedInventory_ExtraTabAccessorsDoNotLeakIntoTab1)
{
	clear_inventory();
	MyPlayer->InvGrid[3] = 7; // sentinel value that must survive untouched
	MyPlayer->InvList[6]._itype = ItemType::Ring;

	ActiveInventoryTab = 2; // extra tab index 1
	GetActiveInvGridCell(*MyPlayer, 3) = 1;
	GetActiveInvListItem(*MyPlayer, 0)._itype = ItemType::Misc;
	GetActiveNumInv(*MyPlayer) = 1;

	EXPECT_EQ(MyPlayer->InvTabGrid[1][3], 1);
	EXPECT_EQ(MyPlayer->InvTabList[1][0]._itype, ItemType::Misc);
	EXPECT_EQ(MyPlayer->_pNumInvTab[1], 1);

	// Tab 1's real storage, and every other extra tab, must be completely unaffected.
	EXPECT_EQ(MyPlayer->InvGrid[3], 7);
	EXPECT_EQ(MyPlayer->InvList[6]._itype, ItemType::Ring);
	EXPECT_EQ(MyPlayer->_pNumInv, 0);
	EXPECT_EQ(MyPlayer->InvTabGrid[0][3], 0);

	ActiveInventoryTab = 0;
	EXPECT_EQ(&GetActiveInvGridCell(*MyPlayer, 3), &MyPlayer->InvGrid[3]);
	EXPECT_EQ(&GetActiveInvListItem(*MyPlayer, 0), &MyPlayer->InvList[0]);
	EXPECT_EQ(&GetActiveNumInv(*MyPlayer), &MyPlayer->_pNumInv);
}

// RemoveActiveInvItem must compact an extra tab's list exactly like Player::RemoveInvItem does
// for the real inventory - same test shape as RemoveInvItem_other_item, just against tab index 1.
TEST_F(InvTest, TabbedInventory_RemoveActiveInvItem_CompactsExtraTab)
{
	clear_inventory();
	ActiveInventoryTab = 2;
	auto &grid = MyPlayer->InvTabGrid[1];
	auto &list = MyPlayer->InvTabList[1];
	MyPlayer->_pNumInvTab[1] = 2;

	// Two-slot misc item followed by a ring, same layout as RemoveInvItem_other_item.
	grid[0] = 1;
	grid[1] = -1;
	list[0]._itype = ItemType::Misc;
	grid[2] = 2;
	list[1]._itype = ItemType::Ring;

	RemoveActiveInvItem(*MyPlayer, 0);

	EXPECT_EQ(grid[0], 0);
	EXPECT_EQ(grid[1], 0);
	EXPECT_EQ(grid[2], 1);
	EXPECT_EQ(list[0]._itype, ItemType::Ring);
	EXPECT_EQ(MyPlayer->_pNumInvTab[1], 1);

	// Tab 1's real inventory must be untouched by an extra-tab removal.
	EXPECT_EQ(MyPlayer->_pNumInv, 0);
}

// AddItemToActiveInvGrid must mark a multi-cell item's full footprint (anchor cell positive,
// every other covered cell negative) against the active extra tab, matching AddItemToInvGrid's
// convention for the real inventory.
TEST_F(InvTest, TabbedInventory_AddItemToActiveInvGrid_MarksMultiCellFootprintInExtraTab)
{
	clear_inventory();
	ActiveInventoryTab = 3; // extra tab index 2
	MyPlayer->_pNumInvTab[2] = 1;

	AddItemToActiveInvGrid(*MyPlayer, 0, 1, Size { 2, 2 });

	auto &grid = MyPlayer->InvTabGrid[2];
	EXPECT_EQ(grid[0], -1);
	EXPECT_EQ(grid[1], -1);
	EXPECT_EQ(grid[10], 1); // bottom-left cell is the anchor
	EXPECT_EQ(grid[11], -1);

	// Tab 1's real InvGrid must be untouched.
	for (int8_t cell : MyPlayer->InvGrid)
		EXPECT_EQ(cell, 0);
}

// Test removing an item from the belt
TEST_F(InvTest, RemoveSpdBarItem)
{
	SNetInitializeProvider(SELCONN_LOOPBACK, nullptr);

	// Clear the belt
	for (int i = 0; i < MaxBeltItems; i++) {
		MyPlayer->SpdList[i].clear();
	}
	// Put an item in the belt: | x | x | item | x | x | x | x | x |
	MyPlayer->SpdList[3]._itype = ItemType::Misc;

	MyPlayer->RemoveSpdBarItem(3);
	EXPECT_TRUE(MyPlayer->SpdList[3].isEmpty());
}

// Test removing a scroll from the inventory
TEST_F(InvTest, RemoveCurrentSpellScrollFromInventory)
{
	clear_inventory();

	// Put a firebolt scroll into the inventory
	MyPlayer->_pNumInv = 1;
	MyPlayer->executedSpell.spellId = SpellID::Firebolt;
	MyPlayer->executedSpell.spellFrom = INVITEM_INV_FIRST;
	MyPlayer->InvList[0]._itype = ItemType::Misc;
	MyPlayer->InvList[0]._iMiscId = IMISC_SCROLL;
	MyPlayer->InvList[0]._iSpell = SpellID::Firebolt;

	ConsumeScroll(*MyPlayer);
	EXPECT_EQ(MyPlayer->InvGrid[0], 0);
	EXPECT_EQ(MyPlayer->_pNumInv, 0);
}

// Test removing the first matching scroll from inventory
TEST_F(InvTest, RemoveCurrentSpellScrollFromInventoryFirstMatch)
{
	clear_inventory();

	// Put a firebolt scroll into the inventory
	MyPlayer->_pNumInv = 1;
	MyPlayer->executedSpell.spellId = SpellID::Firebolt;
	MyPlayer->executedSpell.spellFrom = 0; // any matching scroll
	MyPlayer->InvList[0]._itype = ItemType::Misc;
	MyPlayer->InvList[0]._iMiscId = IMISC_SCROLL;
	MyPlayer->InvList[0]._iSpell = SpellID::Firebolt;

	ConsumeScroll(*MyPlayer);
	EXPECT_EQ(MyPlayer->InvGrid[0], 0);
	EXPECT_EQ(MyPlayer->_pNumInv, 0);
}

// Test removing a scroll from the belt
TEST_F(InvTest, RemoveCurrentSpellScroll_belt)
{
	SNetInitializeProvider(SELCONN_LOOPBACK, nullptr);

	// Clear the belt
	for (int i = 0; i < MaxBeltItems; i++) {
		MyPlayer->SpdList[i].clear();
	}
	// Put a firebolt scroll into the belt
	MyPlayer->executedSpell.spellId = SpellID::Firebolt;
	MyPlayer->executedSpell.spellFrom = INVITEM_BELT_FIRST + 3;
	MyPlayer->SpdList[3]._itype = ItemType::Misc;
	MyPlayer->SpdList[3]._iMiscId = IMISC_SCROLL;
	MyPlayer->SpdList[3]._iSpell = SpellID::Firebolt;

	ConsumeScroll(*MyPlayer);
	EXPECT_TRUE(MyPlayer->SpdList[3].isEmpty());
}

// Test removing the first matching scroll from the belt
TEST_F(InvTest, RemoveCurrentSpellScrollFirstMatchFromBelt)
{
	SNetInitializeProvider(SELCONN_LOOPBACK, nullptr);

	// Clear the belt
	for (int i = 0; i < MaxBeltItems; i++) {
		MyPlayer->SpdList[i].clear();
	}
	// Put a firebolt scroll into the belt
	MyPlayer->executedSpell.spellId = SpellID::Firebolt;
	MyPlayer->executedSpell.spellFrom = 0; // any matching scroll
	MyPlayer->SpdList[3]._itype = ItemType::Misc;
	MyPlayer->SpdList[3]._iMiscId = IMISC_SCROLL;
	MyPlayer->SpdList[3]._iSpell = SpellID::Firebolt;

	ConsumeScroll(*MyPlayer);
	EXPECT_TRUE(MyPlayer->SpdList[3].isEmpty());
}

TEST_F(InvTest, ItemSize)
{
	Item testItem {};

	// Inventory sizes are currently determined by examining the sprite size
	// rune of stone and grey suit are adjacent in the sprite list so provide an easy check for off-by-one errors
	InitializeItem(testItem, IDI_RUNEOFSTONE);
	EXPECT_EQ(GetInventorySize(testItem), Size(1, 1));
	InitializeItem(testItem, IDI_GREYSUIT);
	EXPECT_EQ(GetInventorySize(testItem), Size(2, 2));

	// auric amulet is the first used hellfire sprite, but there's multiple unused sprites before it in the list.
	// unfortunately they're the same size so this is less valuable as a test.
	InitializeItem(testItem, IDI_AURIC);
	EXPECT_EQ(GetInventorySize(testItem), Size(1, 1));

	// gold is the last diablo sprite, off by ones will end up loading a 1x1 unused sprite from hellfire but maybe
	//  this'll segfault if we make a mistake in the future?
	InitializeItem(testItem, IDI_GOLD);
	EXPECT_EQ(GetInventorySize(testItem), Size(1, 1));
}

// RemoveMatchingInventoryOrExtraTabItem backs the ILOC_TWOHAND unequip-revert fix in
// CheckInvCut: AutoPlaceItemInInventory can silently fall back to an extra tab when tab 1 is
// full, so a caller undoing that placement can't assume it landed in InvList - it must be
// located by identity (seed + base item + create info) wherever it actually ended up.
TEST_F(InvTest, RemoveMatchingInventoryOrExtraTabItem_FindsItemInInvList)
{
	clear_inventory();
	MyPlayer->InvList[0]._itype = ItemType::Misc;
	MyPlayer->InvList[0]._iSeed = 0x1234;
	MyPlayer->InvList[0]._iCreateInfo = 0x50;
	MyPlayer->InvList[0].IDidx = IDI_ROCK;
	MyPlayer->InvGrid[0] = 1;
	MyPlayer->_pNumInv = 1;

	Item lookup;
	lookup._iSeed = 0x1234;
	lookup._iCreateInfo = 0x50;
	lookup.IDidx = IDI_ROCK;

	EXPECT_TRUE(RemoveMatchingInventoryOrExtraTabItem(*MyPlayer, lookup));
	EXPECT_EQ(MyPlayer->_pNumInv, 0);
}

TEST_F(InvTest, RemoveMatchingInventoryOrExtraTabItem_FindsItemInExtraTab)
{
	clear_inventory();

	MyPlayer->InvTabList[3][0]._itype = ItemType::Misc;
	MyPlayer->InvTabList[3][0]._iSeed = 0x5678;
	MyPlayer->InvTabList[3][0]._iCreateInfo = 0x60;
	MyPlayer->InvTabList[3][0].IDidx = IDI_ROCK;
	MyPlayer->InvTabGrid[3][0] = 1;
	MyPlayer->_pNumInvTab[3] = 1;

	Item lookup;
	lookup._iSeed = 0x5678;
	lookup._iCreateInfo = 0x60;
	lookup.IDidx = IDI_ROCK;

	EXPECT_TRUE(RemoveMatchingInventoryOrExtraTabItem(*MyPlayer, lookup));
	EXPECT_EQ(MyPlayer->_pNumInvTab[3], 0);
}

TEST_F(InvTest, RemoveMatchingInventoryOrExtraTabItem_NoMatch_ReturnsFalseAndTouchesNothing)
{
	clear_inventory();
	MyPlayer->InvList[0]._itype = ItemType::Misc;
	MyPlayer->InvList[0]._iSeed = 0x1111;
	MyPlayer->InvList[0]._iCreateInfo = 0x10;
	MyPlayer->InvList[0].IDidx = IDI_ROCK;
	MyPlayer->InvGrid[0] = 1;
	MyPlayer->_pNumInv = 1;

	Item lookup;
	lookup._iSeed = 0x9999; // does not match the item above
	lookup._iCreateInfo = 0x10;
	lookup.IDidx = IDI_ROCK;

	EXPECT_FALSE(RemoveMatchingInventoryOrExtraTabItem(*MyPlayer, lookup));
	EXPECT_EQ(MyPlayer->_pNumInv, 1);
}

} // namespace
} // namespace devilution
