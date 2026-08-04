#include <algorithm>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "items.h"
#include "items/validation.h"
#include "monstdat.h"
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

TEST(Item, CanStackWith_DifferentIdentifiedStateDoesNotStack)
{
	Item a = MakeItem(ICLASS_MISC, IMISC_HEAL, IDI_HEAL, true);
	Item b = MakeItem(ICLASS_MISC, IMISC_HEAL, IDI_HEAL, false);
	EXPECT_FALSE(a.canStackWith(b));
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
// maximum end of some matching ItemPrefixes[]/ItemSuffixes[] table entry's declared range.
// Checked against "any matching entry" rather than a specific index, since some power types
// have multiple table entries (different tiers/level requirements) sharing the same type.
TEST_F(PrimalItemTest, GetPrimalItemAffixes_EveryAffixIsRolledAtItsMaximum)
{
	for (int trial = 0; trial < 50; trial++) {
		Item item = MakeItem(ICLASS_WEAPON, IMISC_NONE, IDI_WARRIOR, false, ItemType::Sword);
		GetPrimalItemAffixes(Players[0], item, 1, 30, AffixItemType::Weapon, false);

		for (int i = 0; i < item._iOracoolPrefixCount; i++) {
			bool matchedMax = false;
			for (int j = 0; ItemPrefixes[j].power.type != IPL_INVALID; j++) {
				if (ItemPrefixes[j].power.type == item._iOracoolPrefixes[i].type && ItemPrefixes[j].maxVal == item._iOracoolPrefixes[i].param1) {
					matchedMax = true;
					break;
				}
			}
			EXPECT_TRUE(matchedMax) << "trial " << trial << " prefix " << i << " was not rolled at a table maxVal";
		}
		for (int i = 0; i < item._iOracoolSuffixCount; i++) {
			bool matchedMax = false;
			for (int j = 0; ItemSuffixes[j].power.type != IPL_INVALID; j++) {
				if (ItemSuffixes[j].power.type == item._iOracoolSuffixes[i].type && ItemSuffixes[j].maxVal == item._iOracoolSuffixes[i].param1) {
					matchedMax = true;
					break;
				}
			}
			EXPECT_TRUE(matchedMax) << "trial " << trial << " suffix " << i << " was not rolled at a table maxVal";
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

TEST(Item, CalcOracoolTieredItemValue_SumsAllStoredAffixContributions)
{
	Item item = MakeItem(ICLASS_WEAPON, IMISC_NONE, IDI_WARRIOR, false, ItemType::Sword);
	item._ivalue = 100;
	item._iOracoolPrefixCount = 2;
	item._iOracoolPrefixes[0] = OracoolAffix { IPL_TOHIT, 5, 0 };
	item._iOracoolPrefixes[1] = OracoolAffix { IPL_STR, 3, 0 };
	item._iOracoolSuffixCount = 1;
	item._iOracoolSuffixes[0] = OracoolAffix { IPL_FIRERES, 2, 0 };

	CalcOracoolTieredItemValue(item);

	// addTotal = 5+3+2 = 10, multTotal = 0, so v = 10.
	EXPECT_EQ(item._iIvalue, 10);
}

TEST(Item, CalcOracoolTieredItemValue_NeverProducesLessThanOne)
{
	Item item = MakeItem(ICLASS_WEAPON, IMISC_NONE, IDI_WARRIOR, false, ItemType::Sword);
	item._ivalue = 100;
	item._iOracoolPrefixCount = 1;
	item._iOracoolPrefixes[0] = OracoolAffix { IPL_TOHIT_CURSE, -50, 0 };

	CalcOracoolTieredItemValue(item);

	EXPECT_GE(item._iIvalue, 1);
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
