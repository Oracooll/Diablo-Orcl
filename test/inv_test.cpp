#include <algorithm>

#include <gtest/gtest.h>

#include "control.h"
#include "cursor.h"
#include "diablo.h"
#include "inv.h"
#include "oracool/auto_save.h"
#include "oracool/inventory_layout.h"
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
		// A FRESH player, not whatever the previous test left in the vector: resize keeps an
		// existing element, so belt, gold and grids leaked between cases and a shuffled run of
		// this binary failed on inherited state (external audit, 2026-09-06: QA-02).
		Players[0] = Player {};
		MyPlayer = &Players[0];
		InspectPlayer = MyPlayer; // otherwise IsInspectingPlayer() is true and every inventory click is refused
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

// Test that the scroll is not used in the inventory for each invalid condition (town casting is
// intentionally excluded from "invalid" - see the Oracool comment below).
TEST_F(InvTest, UseScroll_from_inventory_invalid_conditions)
{
	// Empty the belt to prevent using a scroll from the belt
	for (int i = 0; i < MaxBeltItems; i++) {
		MyPlayer->SpdList[i].clear();
	}

	// Adjust inventory size
	MyPlayer->_pNumInv = 5;

	// Oracool: user request - every spell (and thus every scroll) is now castable in town;
	// missiles.cpp's CheckMissileCol suppresses the resulting damage instead of blocking the cast.
	set_up_scroll(MyPlayer->InvList[2], SpellID::Firebolt);
	leveltype = DTYPE_TOWN;
	EXPECT_TRUE(CanUseScroll(*MyPlayer, SpellID::Firebolt));

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

	// Oracool: user request - every spell (and thus every scroll) is now castable in town;
	// missiles.cpp's CheckMissileCol suppresses the resulting damage instead of blocking the cast.
	set_up_scroll(MyPlayer->SpdList[2], SpellID::Firebolt);
	leveltype = DTYPE_TOWN;
	EXPECT_TRUE(CanUseScroll(*MyPlayer, SpellID::Firebolt));

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
// Oracool: picked-up gold now goes straight to the shared Stash pool in single-player instead of
// the inventory - the inventory-placement machinery (AddGoldToInventory et al.) stays in place
// unchanged for multiplayer and for a possible future withdraw-to-inventory feature.
TEST_F(InvTest, GoldAutoPlace_SinglePlayerGoesToStash)
{
	SNetInitializeProvider(SELCONN_LOOPBACK, nullptr);
	gbIsMultiplayer = false;
	clear_inventory();
	Stash.gold = 0;

	MyPlayer->HoldItem._itype = ItemType::Gold;
	MyPlayer->HoldItem._ivalue = 4900;

	EXPECT_TRUE(GoldAutoPlace(*MyPlayer, MyPlayer->HoldItem));
	EXPECT_EQ(MyPlayer->HoldItem._ivalue, 0);
	EXPECT_EQ(Stash.gold, 4900);
	EXPECT_EQ(MyPlayer->_pNumInv, 0) << "gold should not land in inventory in single-player anymore";
}

// Multiplayer has no shared Stash concept, so it keeps the original inventory-placement behavior
// exactly as before this change.
TEST_F(InvTest, GoldAutoPlace_MultiplayerStillUsesInventory)
{
	SNetInitializeProvider(SELCONN_LOOPBACK, nullptr);
	gbIsMultiplayer = true;
	// MaxGold is a global the single-player tests raise to 100,000,000 through CalcPlrInv; under a
	// shuffled run it leaked into this multiplayer case and the pile absorbed everything
	// (external audit, 2026-09-06: QA-02). Multiplayer's cap, explicitly.
	MaxGold = GOLD_MAX_LIMIT;
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

	gbIsMultiplayer = false;
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

// Regression test: a dungeon/monster-dropped potion (SetupItem sets _iIdentified=false,
// items.cpp) must still merge with an otherwise-identical vendor-bought or starting one
// (_iIdentified=true) - identification is functionally meaningless for these always-
// ITEM_QUALITY_NORMAL items, and canStackWith() no longer compares it. Previously this pair
// would land in a separate slot instead of merging.
TEST_F(InvTest, MergeStackableItemIntoInventory_identifiedStateMismatchStillMerges)
{
	SNetInitializeProvider(SELCONN_LOOPBACK, nullptr);
	clear_inventory();
	gbIsMultiplayer = false;

	MyPlayer->InvList[0] = MakeStackablePotion(IDI_HEAL, /*identified=*/true, 5);
	MyPlayer->InvGrid[0] = 1;
	MyPlayer->_pNumInv = 1;

	Item incoming = MakeStackablePotion(IDI_HEAL, /*identified=*/false, 1);
	EXPECT_TRUE(AutoPlaceItemInInventory(*MyPlayer, incoming, true));
	EXPECT_EQ(MyPlayer->InvList[0].stackCount(), 6); // merged into the existing stack
	EXPECT_EQ(MyPlayer->_pNumInv, 1);                // no separate slot needed
}

/**
 * Conservation tests (external audit, 2026-08-25).
 *
 * The merge functions moved exactly ONE unit however many the incoming stack held, and callers read
 * the boolean as "the source is dealt with" and discarded the rest. Fifty gems plus a forty stack
 * produced fifty-one.
 *
 * The three tests above passed throughout, because every one of them arrives with a stack of ONE -
 * the single size at which "+1" is the right answer. These count UNITS, and they arrive with more
 * than one.
 */
int TotalUnitsOf(const Player &player, const Item &like)
{
	int total = 0;
	for (int i = 0; i < player._pNumInv; i++) {
		if (player.InvList[i].canStackWith(like))
			total += player.InvList[i].stackCount();
	}
	for (int i = 0; i < MaxBeltItems; i++) {
		if (!player.SpdList[i].isEmpty() && player.SpdList[i].canStackWith(like))
			total += player.SpdList[i].stackCount();
	}
	return total;
}

/**
 * External audit, 2026-08-25: the gold accumulators were ints and their totals are not.
 *
 * MaxGold is 100,000,000 and the backpack has 70 cells, so an EMPTY backpack means RoomForGold
 * answers 7,000,000,000 - more than three times INT_MAX. That is the commonest inventory state in
 * the game, and it was signed overflow: undefined behaviour, not a large number.
 */
/**
 * MaxGold is set by CalcPlrInv, not a constant: vanilla's 5,000 becomes GoldStackSaveLimit
 * (100,000,000) for a single-player character, which V1 always is. A test that left it at 5,000
 * would be measuring a configuration this game never runs in - and would have reported no problem.
 */
struct RaisedGoldCap {
	RaisedGoldCap() { MaxGold = GoldStackSaveLimit; }
	~RaisedGoldCap() { MaxGold = previous; }
	int previous = MaxGold;
};

TEST_F(InvTest, RoomForGold_EmptyBackpackDoesNotOverflow)
{
	RaisedGoldCap singlePlayerCap;
	clear_inventory();
	const int64_t room = RoomForGold();
	EXPECT_EQ(room, static_cast<int64_t>(InventoryGridCells) * MaxGold)
	    << "an empty backpack should offer every cell's worth of gold";
	EXPECT_GT(room, static_cast<int64_t>(std::numeric_limits<int>::max()))
	    << "the value this test exists for no longer exceeds an int - has MaxGold or the grid changed?";
}

TEST_F(InvTest, CalculateGold_SaturatesInsteadOfWrapping)
{
	RaisedGoldCap singlePlayerCap;
	clear_inventory();
	// Twenty-five full stacks is 2.5 billion, past INT_MAX. The old int accumulator wrapped
	// NEGATIVE, so a very rich character read as being in debt.
	constexpr int Stacks = 25;
	for (int i = 0; i < Stacks; i++) {
		MyPlayer->InvList[i] = {};
		MakeGoldStack(MyPlayer->InvList[i], MaxGold);
		MyPlayer->InvGrid[i] = static_cast<int8_t>(i + 1);
	}
	MyPlayer->_pNumInv = Stacks;

	const int gold = CalculateGold(*MyPlayer);
	EXPECT_EQ(gold, std::numeric_limits<int>::max())
	    << "a total past INT_MAX must saturate, not wrap";
	EXPECT_GT(gold, 0) << "the total wrapped negative";
}

TEST_F(InvTest, MergeStackable_FortyOntoFiftyKeepsNinety)
{
	SNetInitializeProvider(SELCONN_LOOPBACK, nullptr);
	clear_inventory();
	gbIsMultiplayer = false;
	for (auto &beltItem : MyPlayer->SpdList)
		beltItem.clear();

	MyPlayer->InvList[0] = MakeStackablePotion(IDI_HEAL, true, 50);
	MyPlayer->InvGrid[0] = 1;
	MyPlayer->_pNumInv = 1;

	Item incoming = MakeStackablePotion(IDI_HEAL, true, 40);
	ASSERT_EQ(TotalUnitsOf(*MyPlayer, incoming), 50);

	EXPECT_TRUE(AutoPlaceItemInInventory(*MyPlayer, incoming, true));
	EXPECT_EQ(TotalUnitsOf(*MyPlayer, incoming), 90)
	    << "units destroyed merging a 40 stack onto a 50 stack";
}

TEST_F(InvTest, MergeStackable_OverflowSpillsInsteadOfVanishing)
{
	SNetInitializeProvider(SELCONN_LOOPBACK, nullptr);
	clear_inventory();
	gbIsMultiplayer = false;
	for (auto &beltItem : MyPlayer->SpdList)
		beltItem.clear();

	// 95 held, 20 arriving: 4 top the stack up to the 99 cap and 16 must land in a second slot.
	MyPlayer->InvList[0] = MakeStackablePotion(IDI_HEAL, true, Item::MaxStackCount - 4);
	MyPlayer->InvGrid[0] = 1;
	MyPlayer->_pNumInv = 1;

	Item incoming = MakeStackablePotion(IDI_HEAL, true, 20);
	EXPECT_TRUE(AutoPlaceItemInInventory(*MyPlayer, incoming, true));
	EXPECT_EQ(TotalUnitsOf(*MyPlayer, incoming), Item::MaxStackCount - 4 + 20)
	    << "the overflow past MaxStackCount was destroyed instead of spilling to a new slot";
	EXPECT_EQ(MyPlayer->InvList[0].stackCount(), Item::MaxStackCount);
}

// External audit, 2026-09-06 (INV-01, P1): a 95-stack in a FULL backpack and 20 arriving, with room
// in an extra tab. The merge topped the stack up to 99 and then the fallback was handed the whole
// 20 again - 119 units from 115. The remainder, and only the remainder, goes to the tab.
TEST_F(InvTest, MergeStackable_RemainderNotOriginalGoesToTheExtraTab)
{
	SNetInitializeProvider(SELCONN_LOOPBACK, nullptr);
	clear_inventory();
	gbIsMultiplayer = false;
	for (auto &beltItem : MyPlayer->SpdList)
		beltItem.clear();

	MyPlayer->InvList[0] = MakeStackablePotion(IDI_HEAL, true, Item::MaxStackCount - 4);
	MyPlayer->_pNumInv = 1;
	for (int8_t &cell : MyPlayer->InvGrid)
		cell = 1; // every backpack cell taken - the stack is item 1 and the rest may as well be too

	Item incoming = MakeStackablePotion(IDI_HEAL, true, 20);
	EXPECT_TRUE(AutoPlaceItemInInventory(*MyPlayer, incoming, true));
	EXPECT_EQ(MyPlayer->InvList[0].stackCount(), Item::MaxStackCount);
	ASSERT_EQ(MyPlayer->_pNumInvTab[0], 1) << "the remainder should have landed in the first extra tab";
	EXPECT_EQ(MyPlayer->InvTabList[0][0].stackCount(), 16) << "the tab received the ORIGINAL 20, not the 16 left after merging";
	EXPECT_EQ(TotalUnitsOf(*MyPlayer, incoming) + MyPlayer->InvTabList[0][0].stackCount(), Item::MaxStackCount - 4 + 20)
	    << "units were created";
}

// External audit, 2026-09-06 (INV-01): the same 95 + 20 with EVERY destination full. The merge used
// to commit 4 units and then return false, and the caller kept the 20-stack: 4 units from nothing.
// A false return must leave every container exactly as it was.
TEST_F(InvTest, MergeStackable_FailedPlacementChangesNothing)
{
	SNetInitializeProvider(SELCONN_LOOPBACK, nullptr);
	clear_inventory();
	gbIsMultiplayer = false;
	for (auto &beltItem : MyPlayer->SpdList)
		beltItem.clear();

	MyPlayer->InvList[0] = MakeStackablePotion(IDI_HEAL, true, Item::MaxStackCount - 4);
	MyPlayer->_pNumInv = 1;
	for (int8_t &cell : MyPlayer->InvGrid)
		cell = 1;
	for (auto &tab : MyPlayer->InvTabGrid)
		for (int8_t &cell : tab)
			cell = 1;

	Item incoming = MakeStackablePotion(IDI_HEAL, true, 20);
	EXPECT_FALSE(AutoPlaceItemInInventory(*MyPlayer, incoming, true));
	EXPECT_EQ(MyPlayer->InvList[0].stackCount(), Item::MaxStackCount - 4) << "a failed placement still merged into the stack";
	EXPECT_EQ(MyPlayer->_pNumInvTab[0], 0);
	EXPECT_EQ(MyPlayer->_pNumInv, 1);
}

// External audit, 2026-09-06 (INV-01): the belt's half. A 95-stack in slot 1, the other three real
// slots full of something else, 20 arriving: the merge committed 4 and then found no slot for the
// 16 - false, with the belt already changed and the caller about to put all 20 in the backpack.
TEST_F(InvTest, MergeStackable_FailedBeltPlacementChangesNothing)
{
	SNetInitializeProvider(SELCONN_LOOPBACK, nullptr);
	clear_inventory();
	gbIsMultiplayer = false;
	for (auto &beltItem : MyPlayer->SpdList)
		beltItem.clear();
	MyPlayer->SpdList[1] = MakeStackablePotion(IDI_HEAL, true, Item::MaxStackCount - 4);
	MyPlayer->SpdList[2] = MakeStackablePotion(IDI_MANA, true, Item::MaxStackCount);
	MyPlayer->SpdList[3] = MakeStackablePotion(IDI_MANA, true, Item::MaxStackCount);
	MyPlayer->SpdList[4] = MakeStackablePotion(IDI_MANA, true, Item::MaxStackCount);

	Item incoming = MakeStackablePotion(IDI_HEAL, true, 20);
	EXPECT_FALSE(AutoPlaceItemInBelt(*MyPlayer, incoming, true));
	EXPECT_EQ(MyPlayer->SpdList[1].stackCount(), Item::MaxStackCount - 4) << "a failed belt placement still merged into the stack";
	for (int i = 2; i <= 4; i++)
		EXPECT_EQ(MyPlayer->SpdList[i].stackCount(), Item::MaxStackCount);

	// And with a slot free it is all-or-nothing the other way: 4 merge, 16 take the slot.
	MyPlayer->SpdList[4].clear();
	EXPECT_TRUE(AutoPlaceItemInBelt(*MyPlayer, incoming, true));
	EXPECT_EQ(MyPlayer->SpdList[1].stackCount(), Item::MaxStackCount);
	EXPECT_EQ(MyPlayer->SpdList[4].stackCount(), 16);
}

TEST_F(InvTest, MergeStackableItemIntoBelt_mergesIntoExistingStack)
{
	SNetInitializeProvider(SELCONN_LOOPBACK, nullptr);
	gbIsMultiplayer = false;
	for (auto &beltItem : MyPlayer->SpdList)
		beltItem.clear();

	// Oracool HUD overhaul: belt slot 0 is the Menu button and slot 5 the Town Portal button;
	// only 1-4 hold items (oracool::IsRealBeltItemSlot). This used to seed slot 0 and expect the
	// merge there, which now correctly refuses - auto-place skips the button slots entirely.
	MyPlayer->SpdList[1] = MakeStackablePotion(IDI_HEAL, true, 5);

	Item incoming = MakeStackablePotion(IDI_HEAL, true, 1);
	EXPECT_TRUE(AutoPlaceItemInBelt(*MyPlayer, incoming, true));
	EXPECT_EQ(MyPlayer->SpdList[1].stackCount(), 6);
	EXPECT_TRUE(MyPlayer->SpdList[0].isEmpty()) << "the Menu button slot must never take an item";
	EXPECT_TRUE(MyPlayer->SpdList[2].isEmpty()) << "no second belt slot used";
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

// Oracool user request: 1x1 items (potions here) sort to the bottom row(s) instead of wherever
// the plain top-down first-fit scan happens to land them.
TEST_F(InvTest, SortInventoryBySellValue_OneByOneItemsGoToBottomRow)
{
	SNetInitializeProvider(SELCONN_LOOPBACK, nullptr);
	gbIsMultiplayer = false;
	clear_inventory();

	MyPlayer->InvList[0] = MakeStackablePotion(IDI_HEAL, true, 1);
	MyPlayer->InvList[0]._ivalue = 100;
	MyPlayer->InvList[0]._iIvalue = 100;
	MyPlayer->InvGrid[0] = 1;

	MyPlayer->InvList[1] = MakeStackablePotion(IDI_HEAL, true, 1);
	MyPlayer->InvList[1]._ivalue = 50;
	MyPlayer->InvList[1]._iIvalue = 50;
	MyPlayer->InvGrid[1] = 2;

	MyPlayer->_pNumInv = 2;

	SortInventoryBySellValue(*MyPlayer);

	ASSERT_EQ(MyPlayer->_pNumInv, 2);
	constexpr int BottomRowFirstSlot = InventoryGridCells - 10;
	for (int slot = 0; slot < InventoryGridCells; slot++) {
		if (MyPlayer->InvGrid[slot] != 0)
			EXPECT_GE(slot, BottomRowFirstSlot) << "1x1 item at slot " << slot << " should be in the bottom row";
	}
}

TEST_F(InvTest, CheckInventorySortButtonClick_HitsButtonAndSorts)
{
	SNetInitializeProvider(SELCONN_LOOPBACK, nullptr);
	gbIsMultiplayer = false;
	clear_inventory();

	InitializeItem(MyPlayer->InvList[0], IDI_WARRIOR);
	MyPlayer->InvList[0]._iMagical = ITEM_QUALITY_NORMAL;
	MyPlayer->InvList[0]._ivalue = 40;
	MyPlayer->InvGrid[0] = 1;
	InitializeItem(MyPlayer->InvList[1], IDI_WARRIOR);
	MyPlayer->InvList[1]._iMagical = ITEM_QUALITY_NORMAL;
	MyPlayer->InvList[1]._ivalue = 400;
	MyPlayer->InvGrid[1] = 2;
	MyPlayer->_pNumInv = 2;

	// Oracool V1: SORT is a text button in the panel's footer - it spent a while as the tab row's
	// last position - and the inventory window owns its own rect rather than sitting inside
	// UiPanels::Inventory.
	const Rectangle sortRect = oracool::GetSortButtonRect();
	MousePosition = oracool::GetInventoryPanelRect().position
	    + Displacement { sortRect.position.x + sortRect.size.width / 2,
		    sortRect.position.y + sortRect.size.height / 2 };

	EXPECT_TRUE(CheckInventorySortButtonClick(MousePosition));
	EXPECT_EQ(MyPlayer->InvList[0]._ivalue, 400);
}

// Oracool (2026-08-11): the sort control stopped being a setting - it is a standard V1 feature,
// present whenever the game is single-player. Multiplayer is now the only case that hides it.
TEST_F(InvTest, CheckInventorySortButtonClick_MissesInMultiplayer)
{
	clear_inventory();
	gbIsMultiplayer = true;

	// Oracool V1: SORT is a text button in the panel's footer - it spent a while as the tab row's
	// last position - and the inventory window owns its own rect rather than sitting inside
	// UiPanels::Inventory.
	const Rectangle sortRect = oracool::GetSortButtonRect();
	MousePosition = oracool::GetInventoryPanelRect().position
	    + Displacement { sortRect.position.x + sortRect.size.width / 2,
		    sortRect.position.y + sortRect.size.height / 2 };

	EXPECT_FALSE(CheckInventorySortButtonClick(MousePosition));

	gbIsMultiplayer = false;
}

/**
 * Oracool (2026-08-13, user request): every tab position is a storage page.
 *
 * This test used to pin the opposite - that the LAST position must not be claimed as a tab, because
 * it was the SORT button and a tab hit-test claiming it swallowed the click before SORT could see it
 * ("SORT doesn't sort, nor feedbacks on click. I think clicks on it land on ghost tab 10"). SORT has
 * moved to a text button in the footer, so the row now opens all ten pages - and the tenth, which had
 * storage behind it all along and no way to be viewed, is finally reachable.
 *
 * The ordering contract it was really guarding still matters and is still checked below: whatever the
 * tab hit-test does, it must not consume a click meant for SORT.
 */
TEST_F(InvTest, EveryTabPositionOpensAStoragePage)
{
	clear_inventory();

	for (int tab = 0; tab < oracool::TabCount; tab++) {
		ActiveInventoryTab = -1;
		const Rectangle r = oracool::GetTabRect(tab);
		const Point centre = oracool::GetInventoryPanelRect().position
		    + Displacement { r.position.x + r.size.width / 2, r.position.y + r.size.height / 2 };

		EXPECT_TRUE(CheckInventoryTabClick(centre)) << "tab position " << tab << " is not clickable";
		EXPECT_EQ(ActiveInventoryTab, tab) << "tab position " << tab << " opened the wrong page";
	}

	// One page per position, and the storage to back it: tab 0 is the vanilla backpack, 1..9 index
	// InvTabList. A tab position with no page behind it would run off that array.
	EXPECT_EQ(oracool::TabCount, Player::NumExtraInventoryTabs + 1);
}

TEST_F(InvTest, TabClickDoesNotClaimTheSortButton)
{
	clear_inventory();
	ActiveInventoryTab = 3;

	const Rectangle sortRect = oracool::GetSortButtonRect();
	const Point sortCentre = oracool::GetInventoryPanelRect().position
	    + Displacement { sortRect.position.x + sortRect.size.width / 2,
		    sortRect.position.y + sortRect.size.height / 2 };

	EXPECT_FALSE(CheckInventoryTabClick(sortCentre))
	    << "the SORT button was claimed as a tab, which swallows the click before SORT sees it";
	EXPECT_EQ(ActiveInventoryTab, 3) << "clicking SORT changed the open tab";
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
// Oracool Tabbed Inventory: regression test for a real bug found via user report ("the tavern
// sign doesn't go in tabs 2-10" / "magic rock item same bug"). Quest items used to be blocked
// from extra tabs entirely, because the quest-progression code that looks for them only ever
// scanned InvList - a quest item filed into an extra tab would have been invisible to every turn-in
// check in the game. That scanning gap is fixed now (InventoryPlayerItemsRange, inv_iterators.hpp,
// flattens InvList and every extra tab into one iteration), so quest items are placed into an
// extra tab exactly like any other non-gold item once the backpack is full.
TEST_F(InvTest, TabbedInventory_AutoPlaceItemInInventory_SendsQuestItemsToExtraTabsWhenBackpackIsFull)
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

	EXPECT_TRUE(placed);
	EXPECT_EQ(MyPlayer->_pNumInvTab[0], 1) << "the quest item should land in the first extra tab, same as any other item, since the backpack is full";
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
	// The InvList branch ends in Player::RemoveInvItem, which network-syncs the removal
	// (CMD_DELINVITEMS) when the owner is MyPlayer. Without a provider that call dereferences a
	// null connection - the test only passed because an earlier test in the binary had already
	// initialised one, so it crashed whenever ctest ran it under its own --gtest_filter. The
	// extra-tab branch needs no provider (single-player only, no packet format for it).
	SNetInitializeProvider(SELCONN_LOOPBACK, nullptr);
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

// Oracool Tabbed Inventory: regression test for a real bug found via user report ("check every
// item, npc, algorithm... which would at some point interact with vanilla inventory"). Several
// gameplay algorithms (quest turn-ins, stat-flag refreshes, shrine effects) are built on
// InventoryPlayerItemsRange, which previously only ever scanned InvList - an item placed in an
// extra tab was invisible to all of them. HasInventoryItemWithId/RemoveInventoryItemById now find
// and correctly remove a match stored in any extra tab, not just the original backpack.
TEST_F(InvTest, RemoveInventoryItemById_FindsAndRemovesItemStoredInExtraTab)
{
	clear_inventory();
	MyPlayer->InvTabList[2][0]._itype = ItemType::Misc;
	MyPlayer->InvTabList[2][0].IDidx = IDI_HEAL;
	MyPlayer->InvTabGrid[2][0] = 1;
	MyPlayer->_pNumInvTab[2] = 1;

	EXPECT_TRUE(HasInventoryItemWithId(*MyPlayer, IDI_HEAL));

	EXPECT_TRUE(RemoveInventoryItemById(*MyPlayer, IDI_HEAL));

	EXPECT_EQ(MyPlayer->_pNumInvTab[2], 0);
	EXPECT_TRUE(MyPlayer->InvTabList[2][0].isEmpty());
	EXPECT_FALSE(HasInventoryItemWithId(*MyPlayer, IDI_HEAL));
}

// User report (2026-08-19): "i could not CTRL+Click these 5 gold stacks in my inv grid into my
// stash." Non-gold items moved; gold did not.
//
// This walks the whole in-game chain for a gold stack - hover to set pcursinvitem, then the click
// router - because bisecting proved every part of it correct in isolation and the earlier version
// of this test failed for three separate harness reasons, each of which looked like the bug:
//
//   1. no SNetInitializeProvider, so Player::RemoveInvItem's NetSendCmdParam1 faulted;
//   2. InspectPlayer unset, so IsInspectingPlayer() was true and CheckInvItem refused every click
//      at its first line - which reproduced the reported symptom exactly, and was not the reason;
//   3. ItemInvSnds / pcursinvitem / IsStashOpen not exported to tests.
//
// It passes. So CheckInvHLight, CheckInvItem, TransferItemToStash and AutoPlaceItemInStash are all
// correct for gold, and whatever fails in the real game is UPSTREAM of CheckInvItem - in
// LeftMouseDown's routing or the modifier state it is handed. Keeping the test means that half can
// never quietly regress while the real cause is hunted.
TEST_F(InvTest, CtrlClickTransfersGoldStackFromBackpackToStash)
{
	SNetInitializeProvider(SELCONN_LOOPBACK, nullptr);
	clear_inventory();
	Stash.gold = 325000;
	IsStashOpen = true;
	MyPlayer->HoldItem.clear();

	MyPlayer->InvList[0]._itype = ItemType::Gold;
	MyPlayer->InvList[0]._ivalue = 65000;
	MyPlayer->_pNumInv = 1;
	MyPlayer->InvGrid[0] = 1;

	MousePosition = GetPanelPosition(UiPanels::Inventory, InvRect[SLOTXY_INV_FIRST].position)
	    + Displacement { InventorySlotSizeInPixels.width / 2, InventorySlotSizeInPixels.height / 2 };
	pcursinvitem = CheckInvHLight();
	ASSERT_EQ(pcursinvitem, INVITEM_INV_FIRST) << "the gold stack must be hoverable";

	CheckInvItem(/*isShiftHeld=*/false, /*isCtrlHeld=*/true);

	EXPECT_EQ(Stash.gold, 390000) << "the stack's value should have joined the stash pool";
	EXPECT_EQ(MyPlayer->_pNumInv, 0) << "the stack should have left the backpack";
	EXPECT_EQ(MyPlayer->InvGrid[0], 0) << "the backpack cell should be free again";
}

// User report (2026-08-19): "the area where the hover and ctrl+click works is just very tiny. only
// a few px somewhere around the top part of the grid boxes."
//
// Pins the invariant the whole backpack depends on: every pixel of a cell's own rect must resolve
// to that cell. CheckInvHLight breaks at the FIRST rect containing the point and scans the thirteen
// equipment slots before the backpack, so an equipment rect overlapping the grid shows up here as a
// cell whose lower rows answer with someone else's slot - which is exactly the reported signature.
TEST_F(InvTest, EveryBackpackCellIsLiveAcrossItsWholeRect)
{
	SNetInitializeProvider(SELCONN_LOOPBACK, nullptr);
	clear_inventory();

	for (int cell = 0; cell < InventoryGridCells; cell++) {
		MyPlayer->InvList[0]._itype = ItemType::Misc;
		MyPlayer->InvList[0].IDidx = IDI_ROCK;
		MyPlayer->_pNumInv = 1;
		for (int8_t &c : MyPlayer->InvGrid)
			c = 0;
		MyPlayer->InvGrid[cell] = 1;

		const Rectangle rect = InvRect[SLOTXY_INV_FIRST + cell];
		const Point corners[4] = {
			rect.position + Displacement { 1, 1 },
			rect.position + Displacement { rect.size.width - 2, 1 },
			rect.position + Displacement { 1, rect.size.height - 2 },
			rect.position + Displacement { rect.size.width - 2, rect.size.height - 2 },
		};
		for (const Point &corner : corners) {
			MousePosition = corner + Displacement { oracool::GetInventoryPanelRect().position.x,
				               oracool::GetInventoryPanelRect().position.y };
			EXPECT_EQ(CheckInvHLight(), INVITEM_INV_FIRST)
			    << "cell " << cell << " dead at panel-relative (" << corner.x << "," << corner.y << ")";
		}
	}
}

// Oracool bug fix (2026-08-19): AddGoldToInventory's two placement loops were written for the
// vanilla 10x4 grid (`i = 39..30`, then `y = 2..0`), so cells 40-69 of the 10x7 backpack were
// unreachable and gold could never be auto-placed below the fourth row.
//
// Multiplayer, because single-player sends picked-up gold straight to the stash pool (GoldAutoPlace)
// and never reaches these loops.
TEST_F(InvTest, AddGoldToInventory_UsesTheWholeSevenRowGrid)
{
	SNetInitializeProvider(SELCONN_LOOPBACK, nullptr);
	gbIsMultiplayer = true;
	clear_inventory();

	// Block every cell of the first four rows, which is all the old loops could ever see.
	for (int cell = 0; cell < 4 * InventorySizeInSlots.width; cell++)
		MyPlayer->InvGrid[cell] = -1;

	const int leftOver = AddGoldToInventory(*MyPlayer, 500);

	EXPECT_EQ(leftOver, 0) << "rows 5-7 are free, so the gold must fit";
	EXPECT_GT(MyPlayer->_pNumInv, 0) << "a gold stack should have been created";

	bool placedBelowRowFour = false;
	for (int cell = 4 * InventorySizeInSlots.width; cell < InventoryGridCells; cell++) {
		if (MyPlayer->InvGrid[cell] > 0)
			placedBelowRowFour = true;
	}
	EXPECT_TRUE(placedBelowRowFour) << "the gold must land in the rows the old loops could not reach";

	gbIsMultiplayer = false;
}

/**
 * @brief The anchor the right-click equip swap places the displaced item at.
 *
 * User, 2026-09-12: "the current item being replaced to take the same spot in the inventory the new
 * item just occupied". CheckInvCut finds that spot with ActiveInvAnchorSlotOf, and this is the one
 * piece of arithmetic in the change that can be wrong by exactly one row and fail SILENTLY:
 * AddItemToInvGrid marks an item's BOTTOM-left cell with the positive list index, while every
 * placement helper takes a TOP-left anchor. Off by a row, the placement just fails its fit check and
 * the displaced item goes to the far end of the bag - the very thing the change exists to stop.
 *
 * So the premise is asserted here too, not only the result.
 */
TEST_F(InvTest, ActiveInvAnchorSlotOfFindsTheTopLeftCellNotTheBottomLeft)
{
	ActiveInventoryTab = 0;

	// A LOCAL player, deliberately not MyPlayer and not Players[n]. Both of the known backpack-test
	// traps are in play here and this test hit each of them in turn:
	//
	//   - MyPlayer faults. AddItemToInvGrid calls NetSendCmdChInvItem when the player it is handed IS
	//     MyPlayer, which is an access violation under the harness (0xc0000005).
	//   - Players[1] is out of range. The harness sizes that vector to one entry, so indexing 1
	//     asserts inside std::vector rather than giving a spare player.
	//
	// A stack Player is neither, but it starts full of garbage - so every array this exercise reads
	// is zeroed by hand, SpdList included, because AutoPlaceItemInInventorySlot ends in CalcScrolls
	// and that walks the belt as well as the bag.
	Player player {};
	for (int i = 0; i < InventoryGridCells; i++) {
		player.InvList[i] = {};
		player.InvGrid[i] = 0;
	}
	for (Item &beltItem : player.SpdList)
		beltItem = {};
	player._pNumInv = 0;

	Item twoByTwo {};
	InitializeItem(twoByTwo, IDI_GREYSUIT);
	ASSERT_EQ(GetInventorySize(twoByTwo), Size(2, 2))
	    << "this test only means something if the fixture item spans two rows";

	// Top-left at row 1, column 1, so it covers cells 11, 12, 21 and 22.
	constexpr int Anchor = 11;
	const int pitch = InventorySizeInSlots.width;
	ASSERT_TRUE(AutoPlaceItemInInventorySlot(player, Anchor, twoByTwo, true));
	ASSERT_EQ(player._pNumInv, 1);

	EXPECT_EQ(player.InvGrid[Anchor + pitch], 1) << "the BOTTOM-left cell carries the positive index";
	EXPECT_EQ(player.InvGrid[Anchor], -1) << "the top-left cell carries the negative index";

	EXPECT_EQ(ActiveInvAnchorSlotOf(player, 1), Anchor)
	    << "the anchor must be the cell the item was PLACED at, not the cell the grid marks positive";

	// A one-cell item is the degenerate case, where the two cells are the same one.
	Item oneByOne {};
	InitializeItem(oneByOne, IDI_GOLD);
	ASSERT_EQ(GetInventorySize(oneByOne), Size(1, 1));
	ASSERT_TRUE(AutoPlaceItemInInventorySlot(player, 5, oneByOne, true));
	EXPECT_EQ(ActiveInvAnchorSlotOf(player, 2), 5);

	// And an index nothing in the grid references has no anchor, rather than cell 0.
	EXPECT_EQ(ActiveInvAnchorSlotOf(player, 7), -1);
}

// A 4-socket Canticle showed no gold socket rings on hover (user, 2026-09-12). It was reported as a
// problem with 2x2 shields, and it had nothing to do with size or socket count: the backpack draw
// loop decided "hovered" from pcursinvitem alone, and CheckInvHLight deliberately leaves that at -1
// for an item on an extra tab, recording the hover in pcursinvtabidx/pcursinvtabitem instead. So
// from tab 2 onward no item was ever hovered - no rings, no stones, no outline - and the later tabs
// are exactly where the big two-by-two items end up, which is what made it look size-shaped.
TEST_F(InvTest, EveryTabSItemCanBeHoveredNotOnlyTheFirstTabS)
{
	const int savedTab = ActiveInventoryTab;
	const int8_t savedItem = pcursinvitem;
	const int8_t savedTabIdx = pcursinvtabidx;
	const int8_t savedTabItem = pcursinvtabitem;

	// Tab 0 answers from pcursinvitem, as it always did.
	ActiveInventoryTab = 0;
	pcursinvitem = 3 + INVITEM_INV_FIRST;
	pcursinvtabidx = -1;
	pcursinvtabitem = -1;
	EXPECT_TRUE(IsActiveInvItemHovered(3));
	EXPECT_FALSE(IsActiveInvItemHovered(4));

	// On tab 0 the tab-cursor pair must not be consulted at all.
	pcursinvitem = -1;
	pcursinvtabidx = 0;
	pcursinvtabitem = 3;
	EXPECT_FALSE(IsActiveInvItemHovered(3)) << "tab 0 must read pcursinvitem, not the tab cursor";

	// The regression: an extra tab, where pcursinvitem is -1 BY DESIGN. Item 3 on tab 2 (index 1)
	// is hovered and must say so.
	ActiveInventoryTab = 2;
	pcursinvitem = -1;
	pcursinvtabidx = 1;
	pcursinvtabitem = 3;
	EXPECT_TRUE(IsActiveInvItemHovered(3)) << "an extra tab's item is never hovered - no socket rings, no outline";
	EXPECT_FALSE(IsActiveInvItemHovered(4)) << "only the item under the cursor";

	// The tab index has to match too, or tab 3 would light up tab 2's hover.
	ActiveInventoryTab = 3;
	EXPECT_FALSE(IsActiveInvItemHovered(3)) << "a hover on tab 2 must not draw on tab 3";

	// Nothing hovered anywhere.
	ActiveInventoryTab = 2;
	pcursinvtabidx = -1;
	pcursinvtabitem = -1;
	EXPECT_FALSE(IsActiveInvItemHovered(3));

	ActiveInventoryTab = savedTab;
	pcursinvitem = savedItem;
	pcursinvtabidx = savedTabIdx;
	pcursinvtabitem = savedTabItem;
}
} // namespace
} // namespace devilution
