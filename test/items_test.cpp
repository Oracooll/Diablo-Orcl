#include <algorithm>
#include <cstdlib>
#include <map>
#include <set>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "engine/random.hpp"
#include "items.h"
#include "items/validation.h"
#include "oracool/smart_loot.h"
#include "monstdat.h"
#include "options.h"
#include "player.h"

namespace devilution {
namespace {

Item MakeItem(item_class iclass, item_misc_id miscId, _item_indexes idx, bool identified = true, ItemType itype = ItemType::Misc)
{
	Item item;
	item._itype = itype; // Item::isEmpty()/clear() key on this; every real item has a non-None type.
	item._iClass = iclass;
	item._iMiscId = miscId;
	item.IDidx = idx;
	item._iIdentified = identified;
	return item;
}

} // namespace

// Oracool: OptionEntryTormentMultiplier stores the 1.1-5.0 (0.1-step) Torment multiplier as raw
// tenths internally (an OptionEntryInt-family class can only store plain ints), reformatting it
// to a one-decimal float on both the read side (*option, GetTormentDifficultyMultiplier()) and
// the options-menu display side (GetListDescription) - this verifies both directions agree and
// that GetListDescription's index-to-value mapping lines up with how the entries were added
// (11, 12, ..., 50 in strict ascending order, so index i -> value 11+i).
TEST(OptionEntryTormentMultiplier, StoresAndDisplaysAsOneDecimalMultiplier)
{
	auto &option = sgOptions.Oracool.tormentDifficultyMultiplier;
	const int original = option.ValueTenths();

	option.SetValue(20);
	EXPECT_EQ(option.ValueTenths(), 20);
	EXPECT_FLOAT_EQ(*option, 2.0f);
	EXPECT_FLOAT_EQ(GetTormentDifficultyMultiplier(), 2.0f);
	EXPECT_EQ(option.GetListDescription(9), "2.0"); // index 9 -> value 11+9 = 20 -> "2.0"

	option.SetValue(11);
	EXPECT_FLOAT_EQ(*option, 1.1f);
	EXPECT_EQ(option.GetListDescription(0), "1.1");

	option.SetValue(50);
	EXPECT_FLOAT_EQ(*option, 5.0f);
	EXPECT_EQ(option.GetListDescription(39), "5.0");

	option.SetValue(original);
}

TEST(Item, StackCount_DefaultsToOne)
{
	Item item;
	EXPECT_EQ(item.stackCount(), 1);
}

TEST(Item, StackCount_SetAndGet)
{
	Item item;
	item.setStackCount(37);
	EXPECT_EQ(item.stackCount(), 37);
}

TEST(Item, StackCount_ClampsAtMax)
{
	Item item;
	item.setStackCount(500);
	EXPECT_EQ(item.stackCount(), Item::MaxStackCount);
}

TEST(Item, StackCount_ClampsAtMin)
{
	Item item;
	item.setStackCount(0);
	EXPECT_EQ(item.stackCount(), 1);
	item.setStackCount(-5);
	EXPECT_EQ(item.stackCount(), 1);
}

TEST(Item, StackCount_DoesNotDisturbHellfireFlag)
{
	Item item;
	item.dwBuff |= CF_HELLFIRE;
	item.setStackCount(42);
	EXPECT_EQ(item.stackCount(), 42);
	EXPECT_NE(item.dwBuff & CF_HELLFIRE, 0u);
}

TEST(Item, IsStackableConsumable_Potions)
{
	EXPECT_TRUE(MakeItem(ICLASS_MISC, IMISC_HEAL, IDI_HEAL).isStackableConsumable());
	EXPECT_TRUE(MakeItem(ICLASS_MISC, IMISC_FULLMANA, IDI_FULLMANA).isStackableConsumable());
	EXPECT_TRUE(MakeItem(ICLASS_MISC, IMISC_ELIXVIT, IDI_GLDNELIX).isStackableConsumable());
}

TEST(Item, IsStackableConsumable_ScrollsBooksOilsSpecElix)
{
	EXPECT_TRUE(MakeItem(ICLASS_MISC, IMISC_SCROLL, IDI_PORTAL).isStackableConsumable());
	EXPECT_TRUE(MakeItem(ICLASS_MISC, IMISC_BOOK, IDI_BOOK1).isStackableConsumable());
	EXPECT_TRUE(MakeItem(ICLASS_MISC, IMISC_OILOF, IDI_OIL).isStackableConsumable());
	EXPECT_TRUE(MakeItem(ICLASS_MISC, IMISC_SPECELIX, IDI_SPECELIX).isStackableConsumable());
}

TEST(Item, IsStackableConsumable_ExcludesQuestItems)
{
	EXPECT_FALSE(MakeItem(ICLASS_QUEST, IMISC_NONE, IDI_ROCK).isStackableConsumable());
	EXPECT_FALSE(MakeItem(ICLASS_QUEST, IMISC_NONE, IDI_MAPOFDOOM).isStackableConsumable());
}

TEST(Item, IsStackableConsumable_ExcludesArenaPotion)
{
	EXPECT_FALSE(MakeItem(ICLASS_MISC, IMISC_ARENAPOT, IDI_ARENAPOT).isStackableConsumable());
}

// Hellfire's trap runes STACK since 2026-09-05 (user: "why aren't you stacking the hellfire runes") -
// a use takes one off the stack like a scroll's does - and different runes stay apart. Equipment
// never stacks.
TEST(Item, IsStackableConsumable_IncludesRunesExcludesEquipment)
{
	EXPECT_TRUE(MakeItem(ICLASS_MISC, IMISC_RUNEF, IDI_RUNEOFSTONE).isStackableConsumable());
	EXPECT_FALSE(MakeItem(ICLASS_MISC, IMISC_RUNEF, IDI_RUNEOFSTONE).canStackWith(MakeItem(ICLASS_MISC, IMISC_RUNEL, IDI_RUNEOFSTONE)));
	EXPECT_FALSE(MakeItem(ICLASS_WEAPON, IMISC_NONE, IDI_WARRIOR, true, ItemType::Sword).isStackableConsumable());
}

// Item::clear() only resets _itype (isEmpty()'s check); it deliberately leaves every
// other field, including _iMiscId/_iClass/IDidx, as stale leftover data from whatever
// last occupied the slot. isStackableConsumable() must not be fooled by that leftover
// data into treating an empty slot as a real, mergeable item.
TEST(Item, IsStackableConsumable_ExcludesClearedItemDespiteStaleMatchingFields)
{
	Item item = MakeItem(ICLASS_MISC, IMISC_HEAL, IDI_HEAL);
	ASSERT_TRUE(item.isStackableConsumable());

	item.clear();

	EXPECT_TRUE(item.isEmpty());
	EXPECT_FALSE(item.isStackableConsumable());
}

TEST(Item, CanStackWith_ClearedSlotWithStaleMatchingFieldsDoesNotStack)
{
	Item incoming = MakeItem(ICLASS_MISC, IMISC_HEAL, IDI_HEAL);
	Item clearedSlot = MakeItem(ICLASS_MISC, IMISC_HEAL, IDI_HEAL);
	clearedSlot.clear();

	EXPECT_FALSE(clearedSlot.canStackWith(incoming));
	EXPECT_FALSE(incoming.canStackWith(clearedSlot));
}

TEST(Item, CanStackWith_SameItemSameIdentifiedState)
{
	Item a = MakeItem(ICLASS_MISC, IMISC_HEAL, IDI_HEAL, true);
	Item b = MakeItem(ICLASS_MISC, IMISC_HEAL, IDI_HEAL, true);
	EXPECT_TRUE(a.canStackWith(b));
}

// Regression test for a real bug: canStackWith() used to also require _iIdentified equality,
// but _iIdentified is set inconsistently across generation paths for stackable consumables
// (SetupItem, items.cpp, unconditionally sets it false for monster/floor drops, only flipped
// true afterward if Auto Identify Drops is on; vendor stock and starting gear set it true
// directly) even though identification is functionally meaningless for these always-
// ITEM_QUALITY_NORMAL items. A dungeon-dropped potion could silently refuse to stack with an
// otherwise-identical vendor-bought or starting one. Fixed by dropping _iIdentified from the
// comparison entirely - same kind, same spell is sufficient regardless of identified state.
TEST(Item, CanStackWith_DifferentIdentifiedStateStillStacks)
{
	Item a = MakeItem(ICLASS_MISC, IMISC_HEAL, IDI_HEAL, true);
	Item b = MakeItem(ICLASS_MISC, IMISC_HEAL, IDI_HEAL, false);
	EXPECT_TRUE(a.canStackWith(b));
}

TEST(Item, CanStackWith_DifferentBaseItemDoesNotStack)
{
	Item a = MakeItem(ICLASS_MISC, IMISC_HEAL, IDI_HEAL);
	Item b = MakeItem(ICLASS_MISC, IMISC_FULLHEAL, IDI_FULLHEAL);
	EXPECT_FALSE(a.canStackWith(b));
}

// Regression test for a real bug: AllItemsList carries two separate _item_indexes for
// several potions - one reserved for vendor stock and starting gear (e.g. Pepin's Healing
// potions, Adria's Mana potions, a new character's starting belt), another that monster/
// floor drops actually use - both displaying as the identical potion with no visible
// difference to the player. canStackWith() used to key on IDidx and would refuse to merge
// them; it must key on _iMiscId (the field that actually determines the potion's kind)
// instead, so a vendor-bought or starting potion stacks with an otherwise-identical one
// found on the ground.
TEST(Item, CanStackWith_SameKindDifferentUnderlyingIDidxStillStacks)
{
	Item vendorStock = MakeItem(ICLASS_MISC, IMISC_HEAL, IDI_HEAL, true);
	Item dungeonDrop = MakeItem(ICLASS_MISC, IMISC_HEAL, IDI_MANA /* stand-in for the other Diablo item table's duplicate index */, true);
	EXPECT_TRUE(vendorStock.canStackWith(dungeonDrop));
	EXPECT_TRUE(dungeonDrop.canStackWith(vendorStock));
}

// _iMiscId alone is IMISC_SCROLL for every spell scroll, so _iSpell must still be checked -
// otherwise the IDidx fix above would incorrectly let a Scroll of Firebolt stack with a
// Scroll of Identify just because both are "IMISC_SCROLL."
TEST(Item, CanStackWith_DifferentScrollSpellsDoNotStack)
{
	Item identifyScroll = MakeItem(ICLASS_MISC, IMISC_SCROLL, IDI_PORTAL, true);
	identifyScroll._iSpell = SpellID::Identify;
	Item fireboltScroll = MakeItem(ICLASS_MISC, IMISC_SCROLL, IDI_PORTAL, true);
	fireboltScroll._iSpell = SpellID::Firebolt;

	EXPECT_FALSE(identifyScroll.canStackWith(fireboltScroll));
}

TEST(Item, CanStackWith_NonStackableTypeNeverStacks)
{
	Item a = MakeItem(ICLASS_QUEST, IMISC_NONE, IDI_ROCK);
	Item b = MakeItem(ICLASS_QUEST, IMISC_NONE, IDI_ROCK);
	EXPECT_FALSE(a.canStackWith(b));
}

// Items at 0 durability: a broken (_iOracoolBroken) equipped item must contribute nothing to the
// player's stats, checked in CalcSelfItems before bonuses are ever added (not just invalidated
// afterward, since that pass only removes an already-added bonus).
TEST(CalcPlrInv, BrokenItemContributesNoStatBonus)
{
	Players.resize(1);
	devilution::Player &player = Players[0];
	MyPlayer = &player;
	gbIsMultiplayer = false;
	for (Item &item : player.InvBody)
		item.clear();
	player._pBaseStr = 20;
	player._pLevel = 1;

	Item &weapon = player.InvBody[INVLOC_HAND_LEFT];
	InitializeItem(weapon, IDI_WARRIOR);
	weapon._iMagical = ITEM_QUALITY_NORMAL;
	weapon._iIdentified = true;
	weapon._iPLStr = 10;
	weapon._iMaxDur = 40;
	weapon._iDurability = 40;

	CalcPlrInv(player, false);
	EXPECT_EQ(player._pStrength, 30) << "an intact item's Str bonus should apply";
	EXPECT_TRUE(weapon._iStatFlag);

	// Broken the way the game breaks it (BreakOrRemoveEquipment): emptied AND flagged. The flag alone,
	// with durability left, now reads as an item that was mended since (2026-09-11).
	weapon._iDurability = 0;
	weapon._iOracoolBroken = true;
	CalcPlrInv(player, false);
	EXPECT_EQ(player._pStrength, 20) << "a broken item's Str bonus must not apply";
	EXPECT_FALSE(weapon._iStatFlag);
}

class RareItemTest : public ::testing::Test {
public:
	void SetUp() override
	{
		Players.resize(1);
	}
};

// The description-panel line shown under the belt row must be distinct per tier, matching the
// user-reported bug where Buffed Unique and Primal both incorrectly showed "rare item".
TEST(GetOracoolTierPanelLabel, ReturnsDistinctWordingPerTier)
{
	EXPECT_EQ(GetOracoolTierPanelLabel(OracoolItemTier::Rare), "rare item");
	EXPECT_EQ(GetOracoolTierPanelLabel(OracoolItemTier::BuffedUnique), "unique item");
	EXPECT_EQ(GetOracoolTierPanelLabel(OracoolItemTier::Primal), "primal item");
}

// A common weapon type across a wide, low-difficulty level window should always come away
// with at least one prefix and one suffix - Rare's stated minimum identity requirement.
TEST_F(RareItemTest, GetRareItemAffixes_AlwaysProducesAtLeastTwoAffixes)
{
	for (int trial = 0; trial < 200; trial++) {
		Item item = MakeItem(ICLASS_WEAPON, IMISC_NONE, IDI_WARRIOR, false, ItemType::Sword);
		GetRareItemAffixes(Players[0], item, 1, 30, AffixItemType::Weapon, false);
		EXPECT_GE(item._iOracoolPrefixCount + item._iOracoolSuffixCount, 2) << "trial " << trial;
	}
}

// Reproduces the user-reported bug: a jewelry item (rings/amulets - AffixItemType::Misc, a much
// smaller affix pool than weapons/armor) in a narrow level window used to starve the forced
// minimum, letting Rare items ship with fewer than the guaranteed two affixes. The
// minAffixesPerSlot loop must ignore level limits regardless of item type or window width.
TEST_F(RareItemTest, GetRareItemAffixes_AlwaysProducesAtLeastTwoAffixesForJewelryInNarrowLevelWindow)
{
	for (int trial = 0; trial < 200; trial++) {
		Item item = MakeItem(ICLASS_MISC, IMISC_RING, IDI_WARRIOR, false, ItemType::Ring);
		GetRareItemAffixes(Players[0], item, 1, 1, AffixItemType::Misc, false, /*ignoreLevelLimits=*/false);
		EXPECT_GE(item._iOracoolPrefixCount + item._iOracoolSuffixCount, 2) << "trial " << trial;
	}
}

TEST_F(RareItemTest, GetRareItemAffixes_NeverDuplicatesAnAffixType)
{
	for (int trial = 0; trial < 200; trial++) {
		Item item = MakeItem(ICLASS_WEAPON, IMISC_NONE, IDI_WARRIOR, false, ItemType::Sword);
		GetRareItemAffixes(Players[0], item, 1, 30, AffixItemType::Weapon, false);

		std::vector<item_effect_type> seen;
		for (int i = 0; i < item._iOracoolPrefixCount; i++)
			seen.push_back(item._iOracoolPrefixes[i].type);
		for (int i = 0; i < item._iOracoolSuffixCount; i++)
			seen.push_back(item._iOracoolSuffixes[i].type);

		std::sort(seen.begin(), seen.end());
		EXPECT_EQ(std::adjacent_find(seen.begin(), seen.end()), seen.end()) << "trial " << trial << " had a duplicate affix type";
	}
}

// Confirms the confirmed "weighted toward fewer" distribution shape: 2 total affixes should
// be common, 4 total should be comparatively rare. Loose statistical bounds over many trials
// to avoid flakiness rather than asserting exact percentages.
TEST_F(RareItemTest, GetRareItemAffixes_AffixCountDistributionIsWeightedTowardFewer)
{
	int twoAffixCount = 0;
	int fourAffixCount = 0;
	constexpr int Trials = 1000;
	for (int trial = 0; trial < Trials; trial++) {
		Item item = MakeItem(ICLASS_WEAPON, IMISC_NONE, IDI_WARRIOR, false, ItemType::Sword);
		GetRareItemAffixes(Players[0], item, 1, 30, AffixItemType::Weapon, false);
		int total = item._iOracoolPrefixCount + item._iOracoolSuffixCount;
		if (total == 2)
			twoAffixCount++;
		else if (total == 4)
			fourAffixCount++;
	}
	EXPECT_GT(twoAffixCount, fourAffixCount * 2) << "2-affix Rares should be substantially more common than 4-affix Rares";
	EXPECT_GT(twoAffixCount, Trials / 4) << "2-affix Rares should still be a large share of all Rares generated";
}

TEST_F(RareItemTest, GetRareItemAffixes_TagsItemAsRare)
{
	Item item = MakeItem(ICLASS_WEAPON, IMISC_NONE, IDI_WARRIOR, false, ItemType::Sword);
	GetRareItemAffixes(Players[0], item, 1, 30, AffixItemType::Weapon, false);
	EXPECT_TRUE(item.hasOracoolTier());
	EXPECT_EQ(item._iOracoolTier, OracoolItemTier::Rare);
	EXPECT_EQ(item._iMagical, ITEM_QUALITY_MAGIC);
	EXPECT_EQ(item.getTextColor(), UiFlags::ColorYellow3); // YL-3 since 2026-09-07
	// The name comes from the pool now (v1.11.101), so it no longer carries the tier word - the
	// tier is carried by _iOracoolTier, the tooltip Tier line and the name colour. "giverare" was
	// never name-driven either: DebugSpawnTieredItem filters on _iOracoolTier. What the name must
	// still be is TWO WORDS, generated and non-empty.
	EXPECT_EQ(std::count(std::begin(item._iIName), std::end(item._iIName), 0x20), 1) << item._iIName;
}

using BuffedUniqueItemTest = RareItemTest;

// Buffed Unique's stated minimum: at least two prefixes and two suffixes.
TEST_F(BuffedUniqueItemTest, GetBuffedUniqueItemAffixes_AlwaysProducesAtLeastFourAffixes)
{
	for (int trial = 0; trial < 200; trial++) {
		Item item = MakeItem(ICLASS_WEAPON, IMISC_NONE, IDI_WARRIOR, false, ItemType::Sword);
		GetBuffedUniqueItemAffixes(Players[0], item, 1, 30, AffixItemType::Weapon, false);
		EXPECT_GE(item._iOracoolPrefixCount + item._iOracoolSuffixCount, 4) << "trial " << trial;
	}
}

// Reproduces the exact user-reported bug: a Buffed Unique ring dropped with only 1 affix total,
// well under the guaranteed minimum of four affixes. Root cause was identical to the
// earlier Primal narrow-window bug, just never fixed for Rare/Buffed Unique at the time: only
// perfectRoll forced ignoreLevelLimits, so a jewelry item (a much smaller affix pool than
// weapons/armor) combined with a narrow level window could still starve the forced minimum.
TEST_F(BuffedUniqueItemTest, GetBuffedUniqueItemAffixes_AlwaysProducesAtLeastFourAffixesForJewelryInNarrowLevelWindow)
{
	for (int trial = 0; trial < 200; trial++) {
		Item item = MakeItem(ICLASS_MISC, IMISC_RING, IDI_WARRIOR, false, ItemType::Ring);
		GetBuffedUniqueItemAffixes(Players[0], item, 1, 1, AffixItemType::Misc, false, /*ignoreLevelLimits=*/false);
		EXPECT_GE(item._iOracoolPrefixCount + item._iOracoolSuffixCount, 4) << "trial " << trial;
	}
}

// Buffed Unique's stated maximum: at most three prefixes and three suffixes (the hard cap
// Item::MaxOracoolAffixesPerSlot already enforces).
TEST_F(BuffedUniqueItemTest, GetBuffedUniqueItemAffixes_NeverExceedsThreePrefixesOrSuffixes)
{
	for (int trial = 0; trial < 200; trial++) {
		Item item = MakeItem(ICLASS_WEAPON, IMISC_NONE, IDI_WARRIOR, false, ItemType::Sword);
		GetBuffedUniqueItemAffixes(Players[0], item, 1, 30, AffixItemType::Weapon, false);
		EXPECT_LE(item._iOracoolPrefixCount, Item::MaxOracoolAffixesPerSlot) << "trial " << trial;
		EXPECT_LE(item._iOracoolSuffixCount, Item::MaxOracoolAffixesPerSlot) << "trial " << trial;
	}
}

TEST_F(BuffedUniqueItemTest, GetBuffedUniqueItemAffixes_NeverDuplicatesAnAffixType)
{
	for (int trial = 0; trial < 200; trial++) {
		Item item = MakeItem(ICLASS_WEAPON, IMISC_NONE, IDI_WARRIOR, false, ItemType::Sword);
		GetBuffedUniqueItemAffixes(Players[0], item, 1, 30, AffixItemType::Weapon, false);

		std::vector<item_effect_type> seen;
		for (int i = 0; i < item._iOracoolPrefixCount; i++)
			seen.push_back(item._iOracoolPrefixes[i].type);
		for (int i = 0; i < item._iOracoolSuffixCount; i++)
			seen.push_back(item._iOracoolSuffixes[i].type);

		std::sort(seen.begin(), seen.end());
		EXPECT_EQ(std::adjacent_find(seen.begin(), seen.end()), seen.end()) << "trial " << trial << " had a duplicate affix type";
	}
}

// Same "weighted toward fewer" shape confirmed for Rare: 4 total affixes (the 2+2 minimum)
// should be common, 6 total should be comparatively rare.
TEST_F(BuffedUniqueItemTest, GetBuffedUniqueItemAffixes_AffixCountDistributionIsWeightedTowardFewer)
{
	int fourAffixCount = 0;
	int sixAffixCount = 0;
	constexpr int Trials = 1000;
	for (int trial = 0; trial < Trials; trial++) {
		Item item = MakeItem(ICLASS_WEAPON, IMISC_NONE, IDI_WARRIOR, false, ItemType::Sword);
		GetBuffedUniqueItemAffixes(Players[0], item, 1, 30, AffixItemType::Weapon, false);
		int total = item._iOracoolPrefixCount + item._iOracoolSuffixCount;
		if (total == 4)
			fourAffixCount++;
		else if (total == 6)
			sixAffixCount++;
	}
	EXPECT_GT(fourAffixCount, sixAffixCount * 2) << "4-affix Buffed Uniques should be substantially more common than 6-affix ones";
	EXPECT_GT(fourAffixCount, Trials / 4) << "4-affix Buffed Uniques should still be a large share of all Buffed Uniques generated";
}

TEST_F(BuffedUniqueItemTest, GetBuffedUniqueItemAffixes_TagsItemAsBuffedUniqueAndNamesItUnique)
{
	Item item = MakeItem(ICLASS_WEAPON, IMISC_NONE, IDI_WARRIOR, false, ItemType::Sword);
	GetBuffedUniqueItemAffixes(Players[0], item, 1, 30, AffixItemType::Weapon, false);
	EXPECT_TRUE(item.hasOracoolTier());
	EXPECT_EQ(item._iOracoolTier, OracoolItemTier::BuffedUnique);
	EXPECT_EQ(item._iMagical, ITEM_QUALITY_MAGIC);
	EXPECT_EQ(item.getTextColor(), UiFlags::ColorWhitegold);
	// Pool-named since v1.11.101 - two words, no tier label. See the Rare case above.
	EXPECT_EQ(std::count(std::begin(item._iIName), std::end(item._iIName), 0x20), 1) << item._iIName;

}

using PrimalItemTest = RareItemTest;

// Primal's affix count is fixed, not a min/max range: always exactly 3 prefixes + 3 suffixes.
TEST_F(PrimalItemTest, GetPrimalItemAffixes_AlwaysProducesExactlyThreePrefixesAndThreeSuffixes)
{
	for (int trial = 0; trial < 200; trial++) {
		Item item = MakeItem(ICLASS_WEAPON, IMISC_NONE, IDI_WARRIOR, false, ItemType::Sword);
		GetPrimalItemAffixes(Players[0], item, 1, 30, AffixItemType::Weapon, false);
		EXPECT_EQ(item._iOracoolPrefixCount, Item::MaxOracoolAffixesPerSlot) << "trial " << trial;
		EXPECT_EQ(item._iOracoolSuffixCount, Item::MaxOracoolAffixesPerSlot) << "trial " << trial;
	}
}

// Reproduces the user-reported bug: a narrow, low-level window (e.g. from a low monster level
// drop) used to starve the forced-minimum candidate loop, letting Primal items ship with fewer
// than 6 total affixes. perfectRoll must force ignoreLevelLimits so the count guarantee holds
// regardless of the level window the caller passes in.
TEST_F(PrimalItemTest, GetPrimalItemAffixes_AlwaysProducesExactlyThreePrefixesAndThreeSuffixesInNarrowLevelWindow)
{
	for (int trial = 0; trial < 200; trial++) {
		Item item = MakeItem(ICLASS_WEAPON, IMISC_NONE, IDI_WARRIOR, false, ItemType::Sword);
		GetPrimalItemAffixes(Players[0], item, 1, 1, AffixItemType::Weapon, false, /*ignoreLevelLimits=*/false);
		EXPECT_EQ(item._iOracoolPrefixCount, Item::MaxOracoolAffixesPerSlot) << "trial " << trial;
		EXPECT_EQ(item._iOracoolSuffixCount, Item::MaxOracoolAffixesPerSlot) << "trial " << trial;
	}
}

TEST_F(PrimalItemTest, GetPrimalItemAffixes_NeverDuplicatesAnAffixType)
{
	for (int trial = 0; trial < 200; trial++) {
		Item item = MakeItem(ICLASS_WEAPON, IMISC_NONE, IDI_WARRIOR, false, ItemType::Sword);
		GetPrimalItemAffixes(Players[0], item, 1, 30, AffixItemType::Weapon, false);

		std::vector<item_effect_type> seen;
		for (int i = 0; i < item._iOracoolPrefixCount; i++)
			seen.push_back(item._iOracoolPrefixes[i].type);
		for (int i = 0; i < item._iOracoolSuffixCount; i++)
			seen.push_back(item._iOracoolSuffixes[i].type);

		std::sort(seen.begin(), seen.end());
		EXPECT_EQ(std::adjacent_find(seen.begin(), seen.end()), seen.end()) << "trial " << trial << " had a duplicate affix type";
	}
}

// Every affix on a Primal item must be a "perfect roll" - its stored value must equal the
// maximum end of some matching ItemPrefixes[]/ItemSuffixes[] table entry's own small roll range
// (power.param2), not that entry's price-scaling maxVal (a much larger number used only to scale
// the item's gold value - see the OracoolAffix.param1 fix in GetTieredItemAffixes/items.h).
// Checked against "any matching entry" rather than a specific index, since some power types
// have multiple table entries (different tiers/level requirements) sharing the same type.
// IPL_TARGAC (the suffix-only piercing/puncturing/bashing trio) bit-shifts its stored
// param1/param2 before rolling in non-Hellfire mode (see SaveItemPower) - mirrored here rather
// than assuming gbIsHellfire's test-time value.
TEST_F(PrimalItemTest, GetPrimalItemAffixes_EveryAffixIsRolledAtItsMaximum)
{
	for (int trial = 0; trial < 50; trial++) {
		Item item = MakeItem(ICLASS_WEAPON, IMISC_NONE, IDI_WARRIOR, false, ItemType::Sword);
		GetPrimalItemAffixes(Players[0], item, 1, 30, AffixItemType::Weapon, false);

		for (int i = 0; i < item._iOracoolPrefixCount; i++) {
			bool matchedMax = false;
			for (int j = 0; ItemPrefixes[j].power.type != IPL_INVALID; j++) {
				if (ItemPrefixes[j].power.type == item._iOracoolPrefixes[i].type && ItemPrefixes[j].power.param2 == item._iOracoolPrefixes[i].param1) {
					matchedMax = true;
					break;
				}
			}
			EXPECT_TRUE(matchedMax) << "trial " << trial << " prefix " << i << " was not rolled at its own table's maximum";
		}
		for (int i = 0; i < item._iOracoolSuffixCount; i++) {
			bool matchedMax = false;
			for (int j = 0; ItemSuffixes[j].power.type != IPL_INVALID; j++) {
				if (ItemSuffixes[j].power.type != item._iOracoolSuffixes[i].type)
					continue;
				int expectedMax = ItemSuffixes[j].power.param2;
				if (!gbIsHellfire && ItemSuffixes[j].power.type == IPL_TARGAC)
					expectedMax = 3 << expectedMax;
				if (expectedMax == item._iOracoolSuffixes[i].param1) {
					matchedMax = true;
					break;
				}
			}
			EXPECT_TRUE(matchedMax) << "trial " << trial << " suffix " << i << " was not rolled at its own table's maximum";
		}
	}
}

// Every affix selected must be beneficial (PLOk) - a "perfect roll" curse would be nonsensical.
TEST_F(PrimalItemTest, GetPrimalItemAffixes_NeverSelectsANonBeneficialAffix)
{
	for (int trial = 0; trial < 200; trial++) {
		Item item = MakeItem(ICLASS_WEAPON, IMISC_NONE, IDI_WARRIOR, false, ItemType::Sword);
		GetPrimalItemAffixes(Players[0], item, 1, 30, AffixItemType::Weapon, false);

		for (int i = 0; i < item._iOracoolPrefixCount; i++) {
			bool anyOk = false;
			for (int j = 0; ItemPrefixes[j].power.type != IPL_INVALID; j++) {
				if (ItemPrefixes[j].power.type == item._iOracoolPrefixes[i].type && ItemPrefixes[j].PLOk) {
					anyOk = true;
					break;
				}
			}
			EXPECT_TRUE(anyOk) << "trial " << trial << " prefix " << i << " has no PLOk-eligible table entry";
		}
	}
}

TEST_F(PrimalItemTest, GetPrimalItemAffixes_TagsItemAsPrimalWithPerfectRollAndOrangeColor)
{
	Item item = MakeItem(ICLASS_WEAPON, IMISC_NONE, IDI_WARRIOR, false, ItemType::Sword);
	GetPrimalItemAffixes(Players[0], item, 1, 30, AffixItemType::Weapon, false);
	EXPECT_TRUE(item.hasOracoolTier());
	EXPECT_EQ(item._iOracoolTier, OracoolItemTier::Primal);
	EXPECT_TRUE(item._iOracoolPerfectRoll);
	EXPECT_EQ(item._iMagical, ITEM_QUALITY_MAGIC);
	EXPECT_EQ(item.getTextColor(), UiFlags::ColorBeige2); // BE-2 since 2026-09-07
	// Pool-named since v1.11.101 - two words, no tier label.
	EXPECT_EQ(std::count(std::begin(item._iIName), std::end(item._iIName), 0x20), 1) << item._iIName;
}

// ForcePerfectAffixRoll must never leak past the call that set it - confirm a normal
// (non-Primal) generation right after a Primal one can still roll below its type's maximum.
TEST_F(PrimalItemTest, GetRareItemAffixes_AfterPrimalGeneration_DoesNotLeakPerfectRollFlag)
{
	Item primalItem = MakeItem(ICLASS_WEAPON, IMISC_NONE, IDI_WARRIOR, false, ItemType::Sword);
	GetPrimalItemAffixes(Players[0], primalItem, 1, 30, AffixItemType::Weapon, false);

	bool sawNonMaxRoll = false;
	for (int trial = 0; trial < 50 && !sawNonMaxRoll; trial++) {
		Item item = MakeItem(ICLASS_WEAPON, IMISC_NONE, IDI_WARRIOR, false, ItemType::Sword);
		GetRareItemAffixes(Players[0], item, 1, 30, AffixItemType::Weapon, false);
		for (int i = 0; i < item._iOracoolPrefixCount && !sawNonMaxRoll; i++) {
			for (int j = 0; ItemPrefixes[j].power.type != IPL_INVALID; j++) {
				if (ItemPrefixes[j].power.type == item._iOracoolPrefixes[i].type && ItemPrefixes[j].maxVal != item._iOracoolPrefixes[i].param1) {
					sawNonMaxRoll = true;
					break;
				}
			}
		}
	}
	EXPECT_TRUE(sawNonMaxRoll) << "Rare rolls should not all be forced to maximum after a Primal (perfect-roll) generation ran - the flag may have leaked";
}

TEST(Item, CalcOracoolTieredItemValue_SumsAddAndAppliesMultTotal)
{
	Item item = MakeItem(ICLASS_WEAPON, IMISC_NONE, IDI_WARRIOR, false, ItemType::Sword);
	item._ivalue = 100;

	CalcOracoolTieredItemValue(item, /*addTotal=*/10, /*multTotal=*/0);

	EXPECT_EQ(item._iIvalue, 10);
}

TEST(Item, CalcOracoolTieredItemValue_NeverProducesLessThanOne)
{
	Item item = MakeItem(ICLASS_WEAPON, IMISC_NONE, IDI_WARRIOR, false, ItemType::Sword);
	item._ivalue = 100;

	CalcOracoolTieredItemValue(item, /*addTotal=*/-50, /*multTotal=*/0);

	EXPECT_GE(item._iIvalue, 1);
}

// Regression test for the exact user-reported bug: a Buffed Unique "Crown of the Eagle" showed
// "Resist Lightning: +10290%", "+800% armor", "+1000 to dexterity" - PLVal's price-scaled output
// (routinely in the thousands, see GetTieredItemAffixes) was being stored in OracoolAffix.param1
// and displayed directly instead of the actual small rolled stat. Every real affix in the game's
// prefix/suffix tables rolls well under this ceiling (the largest is DAMP's 175 at the very top
// end) - anything anywhere near a thousand can only mean the price-scaled value leaked back in.
TEST_F(BuffedUniqueItemTest, GetBuffedUniqueItemAffixes_StoredAffixValuesStayInRealStatRange)
{
	constexpr int PlausibleStatCeiling = 300;
	for (int trial = 0; trial < 200; trial++) {
		Item item = MakeItem(ICLASS_WEAPON, IMISC_NONE, IDI_WARRIOR, false, ItemType::Sword);
		GetBuffedUniqueItemAffixes(Players[0], item, 1, 30, AffixItemType::Weapon, false);
		for (int i = 0; i < item._iOracoolPrefixCount; i++) {
			EXPECT_LE(std::abs(item._iOracoolPrefixes[i].param1), PlausibleStatCeiling)
			    << "trial " << trial << " prefix " << i << " looks like a price value, not a stat roll";
		}
		for (int i = 0; i < item._iOracoolSuffixCount; i++) {
			EXPECT_LE(std::abs(item._iOracoolSuffixes[i].param1), PlausibleStatCeiling)
			    << "trial " << trial << " suffix " << i << " looks like a price value, not a stat roll";
		}
	}
}

// Regression test for the self-heal repair feature: reproduces the user's follow-up "Crystal
// Amulet" screenshot exactly - a Magic Resist affix stored as 1500 (the White prefix's price-range
// maxVal) instead of 20 (White's own small-range maxVal, since it was a Primal perfect roll).
TEST(Item, RepairOracoolAffixesIfCorrupted_FixesAPriceValueBackToItsRealRoll)
{
	Item item = MakeItem(ICLASS_WEAPON, IMISC_NONE, IDI_WARRIOR, false, ItemType::Sword);
	item._iOracoolTier = OracoolItemTier::Primal;
	item._iOracoolPrefixCount = 1;
	item._iOracoolPrefixes[0] = OracoolAffix { IPL_MAGICRES, 1500, 5 };

	EXPECT_TRUE(RepairOracoolAffixesIfCorrupted(item));
	EXPECT_EQ(item._iOracoolPrefixes[0].param1, 20);
}

TEST(Item, RepairOracoolAffixesIfCorrupted_LeavesAnAlreadyCorrectValueAlone)
{
	Item item = MakeItem(ICLASS_WEAPON, IMISC_NONE, IDI_WARRIOR, false, ItemType::Sword);
	item._iOracoolTier = OracoolItemTier::Rare;
	item._iOracoolPrefixCount = 1;
	item._iOracoolPrefixes[0] = OracoolAffix { IPL_MAGICRES, 20, 5 };

	EXPECT_FALSE(RepairOracoolAffixesIfCorrupted(item));
	EXPECT_EQ(item._iOracoolPrefixes[0].param1, 20);
}

TEST(Item, RepairOracoolAffixesIfCorrupted_IsANoOpForItemsWithoutAnOracoolTier)
{
	Item item = MakeItem(ICLASS_WEAPON, IMISC_NONE, IDI_WARRIOR, false, ItemType::Sword);
	item._iOracoolPrefixCount = 1;
	item._iOracoolPrefixes[0] = OracoolAffix { IPL_MAGICRES, 1500, 5 };

	EXPECT_FALSE(RepairOracoolAffixesIfCorrupted(item));
	EXPECT_EQ(item._iOracoolPrefixes[0].param1, 1500);
}

// Same fix, exercised on a suffix this time (the "+1000 to Dexterity" line from the same
// screenshot - the "dexterity" suffix's price maxVal of 1000 instead of its own small-range
// maxVal of 5).
TEST(Item, RepairOracoolAffixesIfCorrupted_AlsoFixesSuffixes)
{
	Item item = MakeItem(ICLASS_WEAPON, IMISC_NONE, IDI_WARRIOR, false, ItemType::Sword);
	item._iOracoolTier = OracoolItemTier::Primal;
	item._iOracoolSuffixCount = 1;
	item._iOracoolSuffixes[0] = OracoolAffix { IPL_DEX, 1000, 2 };

	EXPECT_TRUE(RepairOracoolAffixesIfCorrupted(item));
	EXPECT_EQ(item._iOracoolSuffixes[0].param1, 5);
}

// Oracool regression test: reproduces the user's bug report of a Rare item appearing to have
// "two identical affixes." PrintOracoolAffixPower must use each affix's own stored param1, not
// the item's single shared accumulated field (item._iPLStr here) - otherwise two distinct affix
// types that both happen to add into the same field would both render the combined total instead
// of their own individual contribution, looking like a duplicate line.
TEST(Item, PrintOracoolAffixPower_UsesEachAffixsOwnValueNotTheItemsSharedField)
{
	Item item = MakeItem(ICLASS_WEAPON, IMISC_NONE, IDI_WARRIOR, true, ItemType::Staff);
	item._iPLStr = 99; // simulates SaveItemPower having accumulated multiple affixes into this field

	const OracoolAffix strAffix { IPL_STR, 3, 0 };
	const OracoolAffix attribsAffix { IPL_ATTRIBS, 5, 0 };

	const std::string strText(PrintOracoolAffixPower(strAffix, item));
	EXPECT_NE(strText.find("3"), std::string::npos) << "should show this affix's own +3, not the item's accumulated _iPLStr (99)";
	EXPECT_EQ(strText.find("99"), std::string::npos);

	// IPL_ATTRIBS also shares _iPLStr (and _iPLMag/_iPLDex/_iPLVit) with plain IPL_STR/MAG/DEX/VIT -
	// same collision class as above (see OE-058's ALLRES/ATTRIBS fix), so it must use its own
	// param1 too rather than falling back to PrintItemPower's shared-field read.
	const std::string attribsText(PrintOracoolAffixPower(attribsAffix, item));
	EXPECT_NE(attribsText.find("5"), std::string::npos) << "should show this affix's own +5, not the item's accumulated _iPLStr (99)";
	EXPECT_EQ(attribsText.find("99"), std::string::npos);

	// IPL_TOHIT_DAMP is a genuinely still-uncovered compound type (its to-hit component can't be
	// reconstructed from param1 alone - see the fix comment above PrintOracoolAffixPower) - confirming
	// the PrintItemPower fallback path still works for it rather than crashing or returning empty.
	const OracoolAffix tohitDampAffix { IPL_TOHIT_DAMP, 5, 0 };
	const std::string tohitDampText(PrintOracoolAffixPower(tohitDampAffix, item));
	EXPECT_FALSE(tohitDampText.empty());
}

// Oracool bug fix (OE-058): user report - a Rare Amulet's tooltip showed "Resist All: +63%" (should
// have been +21%, since the item also had a separate "Resist Fire: +42%" affix - 42+21=63 - the
// IPL_ALLRES line was falling back to PrintItemPower, which reads the item's shared _iPLFR field
// covering every fire-resistance-contributing affix combined, not just this one's own roll).
TEST(Item, PrintOracoolAffixPower_AllResDoesNotCollideWithAnotherResistAffixOnTheSameItem)
{
	Item item = MakeItem(ICLASS_WEAPON, IMISC_NONE, IDI_WARRIOR, true, ItemType::Staff);
	item._iPLFR = 63; // simulates SaveItemPower having accumulated both affixes' rolls (42 + 21) here

	const OracoolAffix allResAffix { IPL_ALLRES, 21, 0 };
	const std::string allResText(PrintOracoolAffixPower(allResAffix, item));
	EXPECT_NE(allResText.find("21"), std::string::npos) << "should show this affix's own +21, not the item's combined _iPLFR (63)";
	EXPECT_EQ(allResText.find("63"), std::string::npos);
}

// Oracool bug fix (OE-058): user report - a Rare Amulet's "+9 to Strength" line was actually a
// Strength-draining curse (-9), but displayed with a "+" because the curse-flavored simple cases
// shared one case block with their positive counterpart and always printed the unsigned roll
// magnitude. Every "_CURSE" type must apply the opposite sign SaveItemPower actually applies to the
// item (IPL_GETHIT/IPL_GETHIT_CURSE are the one intentionally inverted pair - see SaveItemPower).
TEST(Item, PrintOracoolAffixPower_CurseFlavoredAffixesDisplayANegativeValue)
{
	Item item = MakeItem(ICLASS_WEAPON, IMISC_NONE, IDI_WARRIOR, true, ItemType::Staff);

	const OracoolAffix strCurseAffix { IPL_STR_CURSE, 9, 0 };
	const std::string strCurseText(PrintOracoolAffixPower(strCurseAffix, item));
	EXPECT_NE(strCurseText.find("-9"), std::string::npos) << strCurseText;

	const OracoolAffix getHitAffix { IPL_GETHIT, 5, 0 };
	const std::string getHitText(PrintOracoolAffixPower(getHitAffix, item));
	EXPECT_NE(getHitText.find("-5"), std::string::npos) << getHitText << " (IPL_GETHIT itself reduces damage taken, so it's a negative delta)";

	const OracoolAffix getHitCurseAffix { IPL_GETHIT_CURSE, 5, 0 };
	const std::string getHitCurseText(PrintOracoolAffixPower(getHitCurseAffix, item));
	EXPECT_NE(getHitCurseText.find("+5"), std::string::npos) << getHitCurseText << " (IPL_GETHIT_CURSE increases damage taken, so it's a positive delta)";
}

TEST(Item, GetTextColor_RareTierIsYellowRegardlessOfMagicalQuality)
{
	Item item = MakeItem(ICLASS_WEAPON, IMISC_NONE, IDI_WARRIOR, true, ItemType::Sword);
	item._iMagical = ITEM_QUALITY_MAGIC;
	item._iOracoolTier = OracoolItemTier::Rare;
	EXPECT_EQ(item.getTextColor(), UiFlags::ColorYellow3); // YL-3 since 2026-09-07
}

TEST(Item, HasOracoolTier_FalseByDefault)
{
	Item item = MakeItem(ICLASS_ARMOR, IMISC_NONE, IDI_ROCK, true, ItemType::LightArmor);
	EXPECT_FALSE(item.hasOracoolTier());
}

TEST(Item, HasOracoolTier_TrueWhenTierSetAndNotEmpty)
{
	Item item = MakeItem(ICLASS_ARMOR, IMISC_NONE, IDI_ROCK, true, ItemType::LightArmor);
	item._iOracoolTier = OracoolItemTier::Rare;
	EXPECT_TRUE(item.hasOracoolTier());
}

// Item::clear() only resets _itype (isEmpty()'s check); it deliberately leaves every
// other field, including _iOracoolTier, as stale leftover data from whatever last
// occupied the slot. hasOracoolTier() must not be fooled by that leftover data into
// treating an empty slot as a genuinely tiered item - the same bug class fixed for
// isStackableConsumable() earlier this session.
TEST(Item, HasOracoolTier_ExcludesClearedItemDespiteStaleTierData)
{
	Item item = MakeItem(ICLASS_ARMOR, IMISC_NONE, IDI_ROCK, true, ItemType::LightArmor);
	item._iOracoolTier = OracoolItemTier::Primal;
	ASSERT_TRUE(item.hasOracoolTier());

	item.clear();

	EXPECT_TRUE(item.isEmpty());
	EXPECT_FALSE(item.hasOracoolTier());
}

// User-reported bug: uniques purchased from Griswold's Unique Items shop (OE-004) vanished
// with "sent an invalid packet" the instant they were dropped on the ground, even though they
// behaved normally in every other respect. Root cause: CreateUniqueVendorItem (items.cpp)
// stamps its stock's _iCreateInfo with CF_UNIQUE alongside CF_SMITH, so stores.cpp can later
// tell a Unique-Shop purchase apart from a real monster-dropped Unique for resale/pricing - but
// vanilla's IsCreationFlagComboValid treats any town item carrying more than one flag as
// invalid outright, and never anticipated CF_UNIQUE being one of those flags. This combo must
// be explicitly tolerated.
TEST(IsCreationFlagComboValid, AllowsUniqueVendorItemComboFlag)
{
	EXPECT_TRUE(IsCreationFlagComboValid(CF_UNIQUE | CF_SMITH));
}

// The exclusion above must stay narrowly scoped to CF_UNIQUE - an item claiming to be from two
// different real vendors at once is still an invalid combination and must still be rejected,
// whether or not CF_UNIQUE is also set.
TEST(IsCreationFlagComboValid, StillRejectsMultipleRealTownFlags)
{
	EXPECT_FALSE(IsCreationFlagComboValid(CF_SMITH | CF_WITCH));
	EXPECT_FALSE(IsCreationFlagComboValid(CF_SMITH | CF_WITCH | CF_UNIQUE));
}

// End-to-end guard covering the exact construction CreateUniqueVendorItem uses
// (`max(UIMinLvl, 1) | CF_UNIQUE | CF_SMITH`) across the real UniqueItems table's UIMinLvl
// range (1-27), through both checks IsPItemValid actually runs for a town item in sequence -
// this is what determines whether dropping a Unique-Shop item on the ground survives.
TEST(IsCreationFlagComboValid, GriswoldUniqueShopItemSurvivesFullTownValidationAcrossRealUniqueLevelRange)
{
	for (int level = 1; level <= 27; level++) {
		uint16_t creationFlags = static_cast<uint16_t>(level) | CF_UNIQUE | CF_SMITH;
		ASSERT_TRUE(IsCreationFlagComboValid(creationFlags)) << "level " << level;
		ASSERT_TRUE(IsTownItemValid(creationFlags)) << "level " << level;
	}
}

#ifdef _DEBUG
// Reproduces the exact user-reported bug: the "drop {name}" debug console command
// (DebugSpawnItem) picked a uniformly random level in [1, 63] purely to select which item to
// generate, with no regard for whether the network layer's loopback validation would ever
// accept that level for a plain dungeon item. IsDungeonItemValid accepts a level either via an
// exact match against some real monster's level, or via a ceiling fallback (<=30 for
// non-Hellfire items); anything above that ceiling with no exact match is rejected. A rejected
// item was placed in the world locally (so it could still be seen, picked up, and equipped) but
// vanished with "sent an invalid packet" the moment it was actually dropped back onto the
// ground. Find one such invalid level dynamically instead of hardcoding it, since the exact set
// of real monster levels is data, not something this test should assume.
TEST(WouldSurviveNetworkValidation, RejectsDungeonLevelBeyondFallbackCeilingWithNoMonsterMatch)
{
	int invalidLevel = -1;
	for (int level = 31; level <= 63; level++) {
		if (!IsDungeonItemValid(static_cast<uint16_t>(level), 0)) {
			invalidLevel = level;
			break;
		}
	}
	ASSERT_NE(invalidLevel, -1) << "expected at least one level in 31-63 with no matching monster level - "
	                               "if this fails the underlying bug this test protects against may no longer be reachable";

	Item item;
	item._iCreateInfo = static_cast<uint16_t>(invalidLevel);
	item.dwBuff = 0;
	EXPECT_FALSE(WouldSurviveNetworkValidation(item, IDI_ROCK));
}

// The ilvls this fork's OWN drops carry above Normal, and the rules still reject them - which is
// correct for the caller these rules exist for (a remote player's packed equipment) and is why the
// single-player fix lives in IsPItemValid instead of here.
//
// Items generate at 2 * ItemsGetCurrlevel(), and this fork made that the AREA level
// (items.cpp:534), +16 per difficulty block. So a Nightmare sarcophagus stamps 2 * (floor + 16) and
// the Skeleton King's crown 38 | CF_UPER15. Both sailed past vanilla's ceiling of 30, and
// IsPItemValid then printed "Player '...' sent an invalid packet." and skipped DeltaPutItem - the
// drop was on the floor and absent from the delta, so it vanished on level re-entry (user,
// 2026-09-12: the Skeleton King on Nightmare, then a sarcophagus).
//
// These assertions are the PREMISE of that bug. If one ever flips, the early-out in IsPItemValid is
// no longer the only thing standing between the player and silent item loss, and this file should be
// revisited rather than the failure explained away.
TEST(WouldSurviveNetworkValidation, TheForkOwnDropLevelsAreStillRejectedByTheVanillaRules)
{
	EXPECT_FALSE(IsDungeonItemValid(34, 0)) << "a Nightmare sarcophagus drop, 2 * (floor + 16)";
	EXPECT_FALSE(IsUniqueMonsterItemValid(38, 0)) << "the Skeleton King's crown on Nightmare";

	// And the reason it never bit on Normal: those levels stay inside the ceiling.
	EXPECT_TRUE(IsDungeonItemValid(2 * 3, 0)) << "the same sarcophagus on Normal, floor 3";
}

// A plain dungeon item at a low level always survives validation regardless of any exact
// monster-level match, since it falls within IsDungeonItemValid's ceiling fallback - this is
// the common case that must keep working.
TEST(WouldSurviveNetworkValidation, AcceptsLowDungeonLevelRegardlessOfMonsterMatch)
{
	Item item;
	item._iCreateInfo = 1;
	item.dwBuff = 0;
	EXPECT_TRUE(WouldSurviveNetworkValidation(item, IDI_ROCK));
}

// The fix in DebugSpawnItem retries with a fresh random level whenever the generated item
// wouldn't survive network validation. Sweeping every representable level (the full 6-bit
// CF_LEVEL range) and confirming the predicate agrees exactly with IsDungeonItemValid for plain
// dungeon items (no town/unique-monster/Hellfire-book flags) guards against the two checks
// drifting apart if either is changed later.
TEST(WouldSurviveNetworkValidation, AgreesWithIsDungeonItemValidAcrossFullLevelRange)
{
	for (int level = 0; level <= 63; level++) {
		Item item;
		item._iCreateInfo = static_cast<uint16_t>(level);
		item.dwBuff = 0;
		EXPECT_EQ(WouldSurviveNetworkValidation(item, IDI_ROCK), IsDungeonItemValid(item._iCreateInfo, item.dwBuff)) << "level " << level;
	}
}

// Oracool: user report ("still only the same items... everytime the same set of items drops")
// after the give*set commands grew a material-tier prefix. This pins the selection function those
// commands resolve through, in a plain Diablo game, so "the prefix picks the right base item" is
// a tested fact rather than a code-reading assertion - the first version of these commands
// shipped with a selection that could structurally only ever return the leather tier, and that
// also "read correct".
TEST(FirstBaseItemForEquipLocation, PrefixSelectsTieredBaseItems)
{
	gbIsHellfire = false;
	gbIsSpawn = false;

	// Empty prefix: unchanged first-in-table behavior - leather for the worn slots.
	EXPECT_EQ(FirstBaseItemForEquipLocation(ILOC_GLOVES), IDI_ORACOOL_GLOVES);
	EXPECT_EQ(FirstBaseItemForEquipLocation(ILOC_SHOULDERS), IDI_ORACOOL_SHOULDERS);

	// Material prefixes reach every tier, including past the leather items in table order.
	EXPECT_EQ(FirstBaseItemForEquipLocation(ILOC_GLOVES, "bone"), IDI_ORACOOL_BONE_GLOVES);
	EXPECT_EQ(FirstBaseItemForEquipLocation(ILOC_GLOVES, "iron"), IDI_ORACOOL_IRON_GLOVES);
	EXPECT_EQ(FirstBaseItemForEquipLocation(ILOC_GLOVES, "diamond"), IDI_ORACOOL_DIAMOND_GLOVES);
	EXPECT_EQ(FirstBaseItemForEquipLocation(ILOC_HELM, "diamond"), IDI_ORACOOL_DIAMOND_HELM);
	EXPECT_EQ(FirstBaseItemForEquipLocation(ILOC_HELM, "iron"), IDI_ORACOOL_HELM); // "Iron Helm" IS the iron-tier helm
	// Name collision with vanilla's own index-59 "Leather Armor", which sits far earlier in the
	// table: Oracool's item must still win, or `give*set leather` hands back the old icon.
	EXPECT_EQ(FirstBaseItemForEquipLocation(ILOC_ARMOR, "leather"), IDI_ORACOOL_LEATHER_ARMOR);
	EXPECT_EQ(FirstBaseItemForEquipLocation(ILOC_ONEHAND, "steel"), IDI_ORACOOL_STEEL_SHIELD);
	EXPECT_EQ(FirstBaseItemForEquipLocation(ILOC_WAIST, "obsidian"), IDI_ORACOOL_OBSIDIAN_BELT);
	EXPECT_EQ(FirstBaseItemForEquipLocation(ILOC_LEGS, "infernal"), IDI_ORACOOL_INFERNAL_LEGS);
	EXPECT_EQ(FirstBaseItemForEquipLocation(ILOC_BOOTS, "royal"), IDI_ORACOOL_ROYAL_BOOTS);
	EXPECT_EQ(FirstBaseItemForEquipLocation(ILOC_BRACERS, "crusader"), IDI_ORACOOL_CRUSADER_BRACERS);

	// The caller lowercases the user's text; the item-name side is lowercased inside the
	// function itself, so a mixed-case table name ("Bone Gloves") still matches.
	EXPECT_EQ(FirstBaseItemForEquipLocation(ILOC_GLOVES, "bo"), IDI_ORACOOL_BONE_GLOVES);

	// No item of that material at the slot: IDI_NONE, reported by the command as a missing slot
	// rather than silently substituting a different tier.
	EXPECT_EQ(FirstBaseItemForEquipLocation(ILOC_AMULET, "bone"), IDI_NONE);
	EXPECT_EQ(FirstBaseItemForEquipLocation(ILOC_RING, "diamond"), IDI_NONE);
	EXPECT_EQ(FirstBaseItemForEquipLocation(ILOC_HELM, "leather"), IDI_NONE);
	EXPECT_EQ(FirstBaseItemForEquipLocation(ILOC_GLOVES, "adamantite"), IDI_NONE);
}
#endif


// Batches 21 and 22 (2026-09-12): twelve ground tumbles, keyed on what the item IS rather than on
// its cursor id - because they have to serve 250 uniques and 94 set pieces as well as the bases,
// and an id list that long is 344 chances to point an item at the wrong sprite.
//
// So this asserts the RULE, not a list. The case a list could not express is the last one: a unique
// built on a cloak base tumbles like a cloak without being named anywhere.
TEST(OracoolDropTumbles, TheTumbleFollowsTheItemsShapeNotItsIcon)
{
	constexpr int8_t FirstNewTumble = 51;

	// The six worn slots each answer from _iLoc, and each answers differently - the whole point,
	// since before this they were one flopping piece of leather.
	std::set<int> wornAnims;
	for (const item_equip_type loc : { ILOC_GLOVES, ILOC_BOOTS, ILOC_BRACERS, ILOC_WAIST, ILOC_LEGS, ILOC_SHOULDERS }) {
		Item item {};
		item._itype = ItemType::LightArmor;
		item._iLoc = loc;
		item.IDidx = IDI_ORACOOL_SHOULDERS;
		const int8_t anim = GetItemDropAnimIndexFor(item);
		EXPECT_GE(anim, FirstNewTumble) << "equip location " << static_cast<int>(loc) << " still borrows a vanilla tumble";
		EXPECT_TRUE(wornAnims.insert(anim).second)
		    << "equip location " << static_cast<int>(loc) << " shares a tumble with another slot";
	}
	EXPECT_EQ(wornAnims.size(), 6u);

	// The exotic bases answer from the BASE's UITYPE. Pairs that share a shape must share a sheet,
	// and each pair must differ from the others.
	const auto animFor = [](_item_indexes idx, bool asUnique) {
		Item item {};
		item._itype = AllItemsList[idx].itype;
		item._iLoc = AllItemsList[idx].iLoc;
		item.IDidx = idx;
		item._iCurs = AllItemsList[idx].iCurs;
		if (asUnique)
			item._iMagical = ITEM_QUALITY_UNIQUE;
		return GetItemDropAnimIndexFor(item);
	};

	const int8_t cloak = animFor(IDI_ORACOOL_UNQBASE_CLOAK, false);
	const int8_t relic = animFor(IDI_ORACOOL_UNQBASE_RELIC, false);
	const int8_t spear = animFor(IDI_ORACOOL_UNQBASE_SPEAR, false);
	const int8_t lute = animFor(IDI_ORACOOL_UNQBASE_WAR_LUTE, false);
	const int8_t quiver = animFor(IDI_ORACOOL_UNQBASE_WAR_QUIVER, false);
	const int8_t focus = animFor(IDI_ORACOOL_UNQBASE_CANTICLE, false);

	// Same shape, same sheet.
	EXPECT_EQ(animFor(IDI_ORACOOL_UNQBASE_BATTLE_CLOAK, false), cloak) << "both cloaks are cloth";
	EXPECT_EQ(animFor(IDI_ORACOOL_UNQBASE_RELIQUARY, false), relic) << "a reliquary is a casket like a relic";
	EXPECT_EQ(animFor(IDI_ORACOOL_UNQBASE_PIKE, false), spear) << "a pike is a polearm like a spear";
	EXPECT_EQ(animFor(IDI_ORACOOL_UNQBASE_ARCANE_FOCUS, false), focus) << "both are a bound book";

	// Six distinct exotic shapes, all of them new sheets.
	std::set<int> exotic { cloak, relic, spear, lute, quiver, focus };
	EXPECT_EQ(exotic.size(), 6u) << "two exotic bases collapsed onto one tumble";
	for (const int anim : exotic)
		EXPECT_GE(anim, FirstNewTumble) << "an exotic base still borrows a vanilla tumble";

	// Twelve sheets, twelve reachable tumbles. One nothing maps to would ship and never draw.
	std::set<int> all = wornAnims;
	all.insert(exotic.begin(), exotic.end());
	EXPECT_EQ(all.size(), 12u) << "one of the twelve new tumbles is unreachable";

	// THE CASE AN ID LIST CANNOT EXPRESS: a unique on one of these bases keeps the base's shape,
	// with no unique named anywhere in the mapping. Same for all six.
	EXPECT_EQ(animFor(IDI_ORACOOL_UNQBASE_CLOAK, true), cloak);
	EXPECT_EQ(animFor(IDI_ORACOOL_UNQBASE_RELIC, true), relic);
	EXPECT_EQ(animFor(IDI_ORACOOL_UNQBASE_SPEAR, true), spear);
	EXPECT_EQ(animFor(IDI_ORACOOL_UNQBASE_WAR_LUTE, true), lute);
	EXPECT_EQ(animFor(IDI_ORACOOL_UNQBASE_WAR_QUIVER, true), quiver);
	EXPECT_EQ(animFor(IDI_ORACOOL_UNQBASE_CANTICLE, true), focus);

	// And nothing vanilla is dragged in.
	Item rock {};
	rock._itype = ItemType::Misc;
	rock._iLoc = ILOC_UNEQUIPABLE;
	rock.IDidx = IDI_ROCK;
	rock._iCurs = AllItemsList[IDI_ROCK].iCurs;
	EXPECT_LT(GetItemDropAnimIndexFor(rock), FirstNewTumble) << "a vanilla item picked up a fork tumble";
}

// User report, 2026-09-12: "something isnt ok with rares store - many many item types are missing
// from it no matter how many times i refresh."
//
// It was real and it was measurable. GenerateCuratedShelf(Rare) picked its base through
// RndSmithItem at VendorStockLevel(), which is clamp(l + 2, 6, 16) - a ceiling of 16 - while the
// pool gate compares BandedQlvl, whose steps run 14 -> 17 -> 20. So nothing above authored qlvl 15
// was reachable at ANY character level: 72 of 87 eligible bases, and HEAVY ARMOUR 0 of 5. On top of
// that SmithItemOk drops rings, amulets and staves, so a rare ring could not exist.
//
// The base pick is the premium pool now. This test is the user's symptom, stated as an assertion:
// roll the shelf's generator many times and the kinds that were unreachable must actually turn up.
// It is a sampling test by necessity - the pool is not exposed - so it uses a fixed seed and a
// generous budget rather than pretending to be exhaustive.
TEST(OracoolRareShelf, TheRarePoolReachesTheKindsTheOldCeilingLockedOut)
{
	Players.resize(1);
	Player &player = Players[0];
	MyPlayer = &player;
	gbIsMultiplayer = false;
	gbIsHellfire = true;
	player = {};
	player._pLevel = 40; // deep character: the old ceiling ignored this entirely, which was the bug
	player._pBaseStr = player._pStrength = 250;
	player._pBaseDex = player._pDexterity = 250;
	player._pBaseMag = player._pMagic = 250;
	player._pBaseVit = player._pVitality = 250;

	SetRndSeed(0x5EED1234);

	std::set<ItemType> kinds;
	int made = 0;
	int oracoolGear = 0;
	// The live generator's own budget is CuratedShelfCapacity * 8 = 320 attempts for 40 slots. A
	// larger budget here because this is asking "is it REACHABLE", not "does one shelf contain it".
	for (int i = 0; i < 4000; i++) {
		Item item;
		if (!CreateRareVendorItem(player, item, 16))
			continue;
		made++;
		kinds.insert(item._itype);
		if (item.IDidx >= IDI_ORACOOL_SHOULDERS && item.IDidx <= IDI_ORACOOL_SPECTRAL_HELM)
			oracoolGear++;
		EXPECT_EQ(item._iOracoolTier, OracoolItemTier::Rare)
		    << "the shelf only accepts items that actually took the Rare tier";
	}
	ASSERT_GT(made, 200) << "the generator produced almost nothing - the pool or the tier roll is broken";

	// The kind that was 0 of 5 before, and the whole point of the report.
	EXPECT_TRUE(kinds.count(ItemType::HeavyArmor) > 0)
	    << "no heavy armour in " << made << " rolls - the old qlvl-16 ceiling is back";

	// Jewellery, which SmithItemOk excluded outright, so a rare ring was impossible.
	EXPECT_TRUE(kinds.count(ItemType::Ring) > 0 || kinds.count(ItemType::Amulet) > 0)
	    << "no rings or amulets in " << made << " rolls - the pool is back to Griswold's basic filter";

	// And the breadth the user was actually missing. Eight distinct kinds is well short of what the
	// premium pool offers and well clear of what the old ceiling allowed, so this fails on a
	// regression without being brittle about exactly which kinds a seed happens to draw.
	EXPECT_GE(kinds.size(), 8u) << "only " << kinds.size() << " item kinds reachable across " << made << " rolls";

	// And the fork's own gear, which the Basic and Magic tabs stock and this shelf never could (user,
	// 2026-09-13: "None of oracool items make it there"). A third of the rolls aim at it, so hundreds
	// should land; the bound is loose on purpose.
	EXPECT_GT(oracoolGear, made / 10) << "only " << oracoolGear << " Oracool gear pieces in " << made
	                                  << " rare rolls - the Rare shelf is back to the droppable pool alone";
}

} // namespace devilution

namespace devilution {

namespace {

constexpr int8_t SmartLootTestLevel = 30;

_item_indexes DrawTestEquipment()
{
	return RndEquipmentForMonsterLevel(SmartLootTestLevel);
}

_item_indexes DrawTestEquipmentIn(item_equip_type slot)
{
	return RndEquipmentForMonsterLevel(SmartLootTestLevel, slot);
}

/** @brief Which main stat a base leans on, or -1 for none or a tie. 0 Str, 1 Mag, 2 Dex. */
int LeaningOf(_item_indexes idx)
{
	const ItemData &data = AllItemsList[idx];
	const int s = data.iMinStr;
	const int m = data.iMinMag;
	const int d = data.iMinDex;
	if (s > m && s > d)
		return 0;
	if (m > s && m > d)
		return 1;
	if (d > s && d > m)
		return 2;
	return -1;
}

} // namespace

/**
 * Smart Loot, measured on the REAL drop pools rather than a simulated one.
 *
 * The first test drew uniformly from a hand-built list of equipment, which is why it passed while
 * the shipped code starved chests of gold, re-rolled mostly into nothing, inflated rarity and pushed
 * drops toward the deepest bases. This drives RndEquipmentForMonsterLevel and SmartLootAimBase - the
 * two functions the drop sites actually call.
 *
 * User decisions it holds the code to (2026-09-13): no item is locked out of any class; Smart Loot
 * does not change rarity; only main stats count.
 */
TEST(OracoolSmartLoot, AimsTheBaseOnTheRealPoolsAndLeavesGoldRarityAndOtherClassesAlone)
{
	Players.resize(4);
	MyPlayer = &Players[0];
	gbIsMultiplayer = false;
	gbIsHellfire = true;
	Player &sorcerer = Players[1];
	Player &barbarian = Players[2];
	Player &bard = Players[3];
	sorcerer = {};
	barbarian = {};
	bard = {};
	sorcerer._pClass = HeroClass::Sorcerer;
	barbarian._pClass = HeroClass::Barbarian;
	bard._pClass = HeroClass::Bard;

	// 1. GOLD AND CONSUMABLES PASS THROUGH UNTOUCHED, and consume no randomness doing it.
	_item_indexes potion = IDI_NONE;
	for (std::underlying_type_t<_item_indexes> i = IDI_GOLD; i <= IDI_LAST; i++) {
		if (AllItemsList[i].iMiscId == IMISC_HEAL && AllItemsList[i].iRnd != IDROP_NEVER) {
			potion = static_cast<_item_indexes>(i);
			break;
		}
	}
	ASSERT_NE(potion, IDI_NONE);
	const std::vector<_item_indexes> passThroughs { IDI_GOLD, potion };
	for (_item_indexes passThrough : passThroughs) {
		bool drew = false;
		SetRndSeed(77);
		const int expected = GenerateRnd(100000);
		SetRndSeed(77);
		const _item_indexes result = oracool::SmartLootAimBase(passThrough, sorcerer, [&drew](item_equip_type) {
			drew = true;
			return IDI_NONE;
		});
		EXPECT_EQ(result, passThrough) << "a non-equipment drop was replaced";
		EXPECT_FALSE(drew) << "a non-equipment drop drew candidates";
		EXPECT_EQ(GenerateRnd(100000), expected) << "a non-equipment drop consumed randomness";
	}

	// 2. MULTIPLAYER IS LEFT ALONE - aiming at MyPlayer would aim at whoever is local, not the finder.
	gbIsMultiplayer = true;
	for (int i = 0; i < 200; i++) {
		const _item_indexes first = DrawTestEquipment();
		bool drew = false;
		const _item_indexes result = oracool::SmartLootAimBase(first, sorcerer, [&drew](item_equip_type) {
			drew = true;
			return IDI_NONE;
		});
		EXPECT_EQ(result, first);
		EXPECT_FALSE(drew);
	}
	gbIsMultiplayer = false;

	// 3. THE CANDIDATE POOL IS EQUIPMENT ONLY - no nothing, no gold, no potions to waste a candidate on.
	SetRndSeed(0x5EED);
	for (int i = 0; i < 3000; i++)
		ASSERT_TRUE(oracool::SmartLootIsEquipmentBase(DrawTestEquipment()));

	// 4. THE SCORE IS A SHARE: which stats a base asks for, never how much.
	std::vector<_item_indexes> strengthOnly;
	for (std::underlying_type_t<_item_indexes> i = IDI_GOLD; i <= IDI_LAST; i++) {
		const auto idx = static_cast<_item_indexes>(i);
		if (!oracool::SmartLootIsEquipmentBase(idx))
			continue;
		const ItemData &data = AllItemsList[i];
		if (data.iMinStr > 0 && data.iMinMag == 0 && data.iMinDex == 0)
			strengthOnly.push_back(idx);
		if (data.iMinStr == 0 && data.iMinMag == 0 && data.iMinDex == 0) {
			// Rings, amulets, light gear: neutral for everyone, so neither favoured nor starved.
			EXPECT_EQ(oracool::SmartLootScoreForBase(idx, barbarian), oracool::SmartLootNeutralScore) << data.iName;
			EXPECT_EQ(oracool::SmartLootScoreForBase(idx, sorcerer), oracool::SmartLootNeutralScore) << data.iName;
		}
		// The Bard wants all three equally, so nothing it can find scores differently.
		EXPECT_NEAR(oracool::SmartLootScoreForBase(idx, bard), oracool::SmartLootNeutralScore, 1) << data.iName;
	}
	ASSERT_GE(strengthOnly.size(), 2u);
	for (_item_indexes idx : strengthOnly) {
		EXPECT_EQ(oracool::SmartLootScoreForBase(idx, barbarian), oracool::SmartLootScoreForBase(strengthOnly.front(), barbarian))
		    << AllItemsList[idx].iName << " - a bigger requirement scored higher, which pushes drops up the item ladder";
	}

	// 5. THE LIFT, on the real pool: how often a drop leans on the class's own stat.
	constexpr int Draws = 12000;
	const auto aimedLeaning = [](const Player &player, int leaning) {
		int hits = 0;
		for (int i = 0; i < Draws; i++) {
			const _item_indexes idx = oracool::SmartLootAimBase(DrawTestEquipment(), player, DrawTestEquipmentIn);
			if (LeaningOf(idx) == leaning)
				hits++;
		}
		return static_cast<double>(hits) / Draws;
	};
	const auto blindLeaning = [](int leaning) {
		int hits = 0;
		for (int i = 0; i < Draws; i++) {
			if (LeaningOf(DrawTestEquipment()) == leaning)
				hits++;
		}
		return static_cast<double>(hits) / Draws;
	};
	SetRndSeed(0x10072013);
	const double blindMagic = blindLeaning(1);
	const double blindStrength = blindLeaning(0);
	const double sorcererMagic = aimedLeaning(sorcerer, 1);
	const double barbarianStrength = aimedLeaning(barbarian, 0);
	const double bardMagic = aimedLeaning(bard, 1);
	ASSERT_GT(blindMagic, 0.0) << "no magic-leaning base at this level - the test proves nothing";
	EXPECT_GT(sorcererMagic, blindMagic * 1.5) << "blind " << blindMagic << " aimed " << sorcererMagic;
	EXPECT_GT(barbarianStrength, blindStrength * 1.2) << "blind " << blindStrength << " aimed " << barbarianStrength;
	EXPECT_NEAR(bardMagic, blindMagic, blindMagic * 0.25) << "the generalist drops moved - aiming is not flat for it";

	// 6. THE SLOT MIX DOES NOT MOVE. Aiming picks which base fills the slot the blind roll chose, never
	// the slot itself, so a Barbarian finds as many rings as anyone. Scoring across slots starved them:
	// a neutral ring loses to almost every Strength base, and jewellery fell to a third of its share.
	const auto slotShares = [](const Player *player) {
		std::map<item_equip_type, double> shares;
		for (int i = 0; i < Draws; i++) {
			_item_indexes idx = DrawTestEquipment();
			if (player != nullptr)
				idx = oracool::SmartLootAimBase(idx, *player, DrawTestEquipmentIn);
			shares[AllItemsList[idx].iLoc] += 1.0 / Draws;
		}
		return shares;
	};
	const std::map<item_equip_type, double> blindSlots = slotShares(nullptr);
	ASSERT_GT(blindSlots.count(ILOC_RING) + blindSlots.count(ILOC_AMULET), 0u) << "no jewellery at this level - the check proves nothing";
	const std::vector<const Player *> slotProbes { &barbarian, &sorcerer };
	for (const Player *player : slotProbes) {
		std::map<item_equip_type, double> aimedSlots = slotShares(player);
		for (const auto &[slot, blindShare] : blindSlots) {
			const double tolerance = std::max(0.01, blindShare * 0.2);
			EXPECT_NEAR(aimedSlots[slot], blindShare, tolerance)
			    << "slot " << static_cast<int>(slot) << " moved for class " << static_cast<int>(player->_pClass);
		}
	}

	// 7. NOTHING IS LOCKED OUT: every base a blind draw finds with any regularity, an aimed draw finds too.
	SetRndSeed(4242);
	std::map<_item_indexes, int> blind;
	for (int i = 0; i < 40000; i++)
		blind[DrawTestEquipment()]++;
	std::set<_item_indexes> aimedSeen;
	for (int i = 0; i < 40000; i++)
		aimedSeen.insert(oracool::SmartLootAimBase(DrawTestEquipment(), sorcerer, DrawTestEquipmentIn));
	for (const auto &[idx, count] : blind) {
		if (count < 40)
			continue;
		EXPECT_TRUE(aimedSeen.count(idx) != 0) << AllItemsList[idx].iName << " became unreachable for the Sorcerer";
	}
}

/**
 * The unified affix pool: one pool, any combination, within each tier's limit.
 *
 * User direction (2026-09-13): "We call all possible item bonuses affixes and an item can have any combo
 * of them within its limit of affixes." Before this, a Rare always rolled at least one prefix and one
 * suffix and at most two of either, a magic item rolled vanilla's one-prefix-one-suffix shape, and
 * Movement Speed and Faster Cast arrived from drop-tail rolls outside every limit.
 *
 * What stays limited is STORAGE - three prefixes and three suffixes - and the assertions say so.
 */
TEST_F(RareItemTest, UnifiedAffixes_AnyCombinationWithinTheLimitAndMovementSpeedIsAnAffix)
{
	MyPlayer = &Players[0];
	const bool wasHellfire = gbIsHellfire;
	gbIsHellfire = true;
	SetRndSeed(0x0AFF1C5);

	// RARE: every split the storage allows, including ones the old per-slot guarantee made impossible.
	int emptyTable = 0;
	int threeFromOneTable = 0;
	int moveSpeed = 0;
	int fastCast = 0;
	for (int trial = 0; trial < 3000; trial++) {
		Item item = MakeItem(ICLASS_ARMOR, IMISC_NONE, IDI_ORACOOL_HELM, false, ItemType::Helm);
		GetRareItemAffixes(Players[0], item, 1, 50, AffixItemType::Armor, false);
		const int prefixes = item._iOracoolPrefixCount;
		const int suffixes = item._iOracoolSuffixCount;
		ASSERT_GE(prefixes + suffixes, 2) << "trial " << trial;
		ASSERT_LE(prefixes + suffixes, 4) << "trial " << trial;
		ASSERT_LE(prefixes, Item::MaxOracoolAffixesPerSlot);
		ASSERT_LE(suffixes, Item::MaxOracoolAffixesPerSlot);
		if (prefixes == 0 || suffixes == 0)
			emptyTable++;
		if (prefixes == 3 || suffixes == 3)
			threeFromOneTable++;
		for (int i = 0; i < suffixes; i++) {
			if (item._iOracoolSuffixes[i].type == IPL_MOVESPEED)
				moveSpeed++;
			if (item._iOracoolSuffixes[i].type == IPL_FASTCAST)
				fastCast++;
		}
	}
	EXPECT_GT(emptyTable, 0) << "a Rare never rolled all its affixes from one table - the pool is still split";
	EXPECT_GT(threeFromOneTable, 0) << "a Rare never rolled three from one table - the old two-per-slot ceiling is back";
	EXPECT_GT(moveSpeed, 0) << "Movement Speed never rolled on a Rare helm - it is not in the pool";
	EXPECT_GT(fastCast, 0) << "Faster Cast never rolled on a Rare helm - it is not in the pool";

	// A SWORD takes neither: the pool rows keep the item types the drop tail allowed.
	for (int trial = 0; trial < 1500; trial++) {
		Item item = MakeItem(ICLASS_WEAPON, IMISC_NONE, IDI_WARRIOR, false, ItemType::Sword);
		GetRareItemAffixes(Players[0], item, 1, 50, AffixItemType::Weapon, false);
		for (int i = 0; i < item._iOracoolSuffixCount; i++) {
			ASSERT_NE(item._iOracoolSuffixes[i].type, IPL_MOVESPEED) << "a sword rolled Movement Speed";
			ASSERT_NE(item._iOracoolSuffixes[i].type, IPL_FASTCAST) << "a sword rolled Faster Cast";
		}
	}

	// MAGIC: one or two affixes, never more; pool rows live in the record and agree with the field.
	int twoAffixes = 0;
	int magicMoveSpeed = 0;
	for (int trial = 0; trial < 6000; trial++) {
		Item item = MakeItem(ICLASS_ARMOR, IMISC_NONE, IDI_ORACOOL_HELM, false, ItemType::Helm);
		GetItemPower(Players[0], item, 1, 50, AffixItemType::Armor, false);
		const int used = OracoolAffixesUsed(item);
		ASSERT_LE(used, 2) << "a magic item rolled " << used << " affixes";
		if (item._iMagical == ITEM_QUALITY_MAGIC)
			ASSERT_GE(used, 1);
		if (used == 2)
			twoAffixes++;
		int fromRecord = 0;
		for (int i = 0; i < item._iOracoolSuffixCount; i++) {
			if (item._iOracoolSuffixes[i].type == IPL_MOVESPEED)
				fromRecord += item._iOracoolSuffixes[i].param1;
			else if (item._iOracoolSuffixes[i].type == IPL_MOVESPEED_CURSE)
				fromRecord -= item._iOracoolSuffixes[i].param1;
		}
		ASSERT_EQ(fromRecord, item._iPLMoveSpeed) << "the record and the field disagree, and the loader re-derives from the record";
		if (item._iPLMoveSpeed > 0) {
			magicMoveSpeed++;
			// Pool affixes are PRICED; the drop-tail roll added a stat and no value.
			EXPECT_GT(item._iIvalue, 0) << "a magic helm carrying Movement Speed is worth nothing";
		}
	}
	EXPECT_GT(twoAffixes, 0);
	EXPECT_GT(magicMoveSpeed, 0) << "Movement Speed never rolled on a magic helm";

	// ONLY GOOD means no curse, pool rows included.
	for (int trial = 0; trial < 3000; trial++) {
		Item item = MakeItem(ICLASS_ARMOR, IMISC_NONE, IDI_ORACOOL_HELM, false, ItemType::Helm);
		GetItemPower(Players[0], item, 1, 50, AffixItemType::Armor, true);
		ASSERT_GE(item._iPLMoveSpeed, 0) << "an only-good roll took the Movement Speed curse";
	}

	gbIsHellfire = wasHellfire;
}
} // namespace devilution
