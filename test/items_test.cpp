#include <algorithm>
#include <cstdlib>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "items.h"
#include "items/validation.h"
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

TEST(Item, IsStackableConsumable_ExcludesRunesAndEquipment)
{
	EXPECT_FALSE(MakeItem(ICLASS_MISC, IMISC_RUNEF, IDI_RUNEOFSTONE).isStackableConsumable());
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
TEST_F(RareItemTest, GetRareItemAffixes_AlwaysProducesAtLeastOnePrefixAndSuffix)
{
	for (int trial = 0; trial < 200; trial++) {
		Item item = MakeItem(ICLASS_WEAPON, IMISC_NONE, IDI_WARRIOR, false, ItemType::Sword);
		GetRareItemAffixes(Players[0], item, 1, 30, AffixItemType::Weapon, false);
		EXPECT_GE(item._iOracoolPrefixCount, 1) << "trial " << trial;
		EXPECT_GE(item._iOracoolSuffixCount, 1) << "trial " << trial;
	}
}

// Reproduces the user-reported bug: a jewelry item (rings/amulets - AffixItemType::Misc, a much
// smaller affix pool than weapons/armor) in a narrow level window used to starve the forced
// minimum, letting Rare items ship with fewer than the guaranteed 1 prefix + 1 suffix. The
// minAffixesPerSlot loop must ignore level limits regardless of item type or window width.
TEST_F(RareItemTest, GetRareItemAffixes_AlwaysProducesAtLeastOnePrefixAndSuffixForJewelryInNarrowLevelWindow)
{
	for (int trial = 0; trial < 200; trial++) {
		Item item = MakeItem(ICLASS_MISC, IMISC_RING, IDI_WARRIOR, false, ItemType::Ring);
		GetRareItemAffixes(Players[0], item, 1, 1, AffixItemType::Misc, false, /*ignoreLevelLimits=*/false);
		EXPECT_GE(item._iOracoolPrefixCount, 1) << "trial " << trial;
		EXPECT_GE(item._iOracoolSuffixCount, 1) << "trial " << trial;
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
	EXPECT_EQ(item.getTextColor(), UiFlags::ColorYellow);
	// DebugSpawnItem (the "drop {name}" debug console command) finds items purely by a
	// lowercased substring match against _iIName, so this is also what guarantees "drop rare"
	// actually works - ASCII lowercasing preserves substring containment, so proving the
	// mixed-case name here is sufficient proof for the lowercase search path too.
	EXPECT_NE(std::string(item._iIName).find("Rare"), std::string::npos);
}

using BuffedUniqueItemTest = RareItemTest;

// Buffed Unique's stated minimum: at least two prefixes and two suffixes.
TEST_F(BuffedUniqueItemTest, GetBuffedUniqueItemAffixes_AlwaysProducesAtLeastTwoPrefixesAndTwoSuffixes)
{
	for (int trial = 0; trial < 200; trial++) {
		Item item = MakeItem(ICLASS_WEAPON, IMISC_NONE, IDI_WARRIOR, false, ItemType::Sword);
		GetBuffedUniqueItemAffixes(Players[0], item, 1, 30, AffixItemType::Weapon, false);
		EXPECT_GE(item._iOracoolPrefixCount, 2) << "trial " << trial;
		EXPECT_GE(item._iOracoolSuffixCount, 2) << "trial " << trial;
	}
}

// Reproduces the exact user-reported bug: a Buffed Unique ring dropped with only 1 affix total,
// well under the guaranteed minimum of 2 prefixes + 2 suffixes. Root cause was identical to the
// earlier Primal narrow-window bug, just never fixed for Rare/Buffed Unique at the time: only
// perfectRoll forced ignoreLevelLimits, so a jewelry item (a much smaller affix pool than
// weapons/armor) combined with a narrow level window could still starve the forced minimum.
TEST_F(BuffedUniqueItemTest, GetBuffedUniqueItemAffixes_AlwaysProducesAtLeastTwoPrefixesAndTwoSuffixesForJewelryInNarrowLevelWindow)
{
	for (int trial = 0; trial < 200; trial++) {
		Item item = MakeItem(ICLASS_MISC, IMISC_RING, IDI_WARRIOR, false, ItemType::Ring);
		GetBuffedUniqueItemAffixes(Players[0], item, 1, 1, AffixItemType::Misc, false, /*ignoreLevelLimits=*/false);
		EXPECT_GE(item._iOracoolPrefixCount, 2) << "trial " << trial;
		EXPECT_GE(item._iOracoolSuffixCount, 2) << "trial " << trial;
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
	// Display name is "Unique {base}", not "Buffed Unique {base}", per the roadmap's naming spec.
	EXPECT_NE(std::string(item._iIName).find("Unique"), std::string::npos);
	EXPECT_EQ(std::string(item._iIName).find("Buffed"), std::string::npos);
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
	EXPECT_EQ(item.getTextColor(), UiFlags::ColorOrange);
	EXPECT_NE(std::string(item._iIName).find("Primal"), std::string::npos);
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
	EXPECT_EQ(item.getTextColor(), UiFlags::ColorYellow);
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
#endif

} // namespace devilution
