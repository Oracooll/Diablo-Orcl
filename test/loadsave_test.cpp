#include "loadsave.h"

#include <gtest/gtest.h>

#include "init.h"
#include "items.h"
#include "levels/gendung.h"
#include "menu.h"
#include "multi.h"
#include "options.h"
#include "pfile.h"
#include "player.h"
#include "qol/stash.h"
#include "spells.h"
#include "utils/file_util.h"
#include "utils/paths.h"

#include "isolated_pref_path.hpp"

namespace devilution {
namespace {

class LoadSaveOracoolItemExtensionsTest : public ::testing::Test {
public:
	void TearDown() override
	{
		// A passing test takes its directory with it; a failing one keeps the archive to be read.
		DropIsolatedPrefPath();
	}

	void SetUp() override
	{
		UseIsolatedPrefPath();
		gbVanilla = false;
		gbIsHellfire = false;
		gbIsMultiplayer = false;
		gbIsHellfireSaveGame = false;
		leveltype = DTYPE_TOWN;
		giNumberOfLevels = 17;
		gSaveNumber = 0;

		RemoveFile((paths::PrefPath() + "single_0.sv").c_str());

		// CreatePlayer (called inside pfile_ui_save_create) has different, crash-prone
		// side effects when &player == MyPlayer, so MyPlayer must not alias Players[0]
		// (leftover from a previous test in this fixture) at the moment it runs.
		MyPlayer = nullptr;
		Players.resize(2);

		// Starting gear ON for this fixture. These are save round-trip tests, and they need an item to
		// tag and reload; Naked Heroes (default ON since 2026-08-19) would hand back an empty bag. The
		// option is read at creation, which pfile_ui_save_create performs below.
		sgOptions.Oracool.nakedHeroes.SetValue(false);
	}
};

void SetFullOracoolTierData(Item &item)
{
	item._iOracoolTier = OracoolItemTier::Primal;
	item._iOracoolPerfectRoll = true;
	item._iOracoolAffixCount = Item::MaxOracoolAffixes;
	for (int i = 0; i < Item::MaxOracoolAffixes; i++)
		item._iOracoolAffixes[i] = OracoolAffix { static_cast<item_effect_type>(IPL_STR + i), 10 + i, 20 + i };
}

// Older item formats still read (audit, 2026-09-27: nothing loaded a format-13 or format-14 record, so the size maths
// and the version gates were untested). A record is written today, then cut back to what each older format held: 14
// lacks v15's three workshop bytes at the end, and 13 also lacks v14's four cold-resistance bytes. Each loads with the
// missing fields at their defaults and everything after them in place - and the format goes back to today's after.
TEST(LoadSaveItemFormats, Formats13And14StillReadAndTheFormatComesBack)
{
	Item item;
	item._itype = ItemType::Ring;
	item.IDidx = IDI_ROCK;
	item._iSeed = 0x12345678;
	item._iPLCR = 17;
	item._iPLMana = 5 << 6;
	item._iOracoolRiftTier = 9;
	item._iOracoolRerolls = 3;
	item._iOracoolRemovals = 2;
	item._iOracoolLockedAffix = -1;

	const std::vector<uint8_t> v15 = SaveItemBytesForTest(item);
	const uint8_t today = LoadingItemFormatForTest();
	ASSERT_EQ(today, 15);

	Item loaded;
	ASSERT_TRUE(LoadItemBytesForTest(v15, 15, loaded));
	EXPECT_EQ(loaded._iPLCR, 17);
	EXPECT_EQ(loaded._iOracoolRerolls, 3);
	EXPECT_EQ(LoadingItemFormatForTest(), today);

	std::vector<uint8_t> v14(v15.begin(), v15.end() - 3);
	loaded = {};
	ASSERT_TRUE(LoadItemBytesForTest(v14, 14, loaded));
	EXPECT_EQ(loaded._iPLCR, 17);
	EXPECT_EQ(loaded._iOracoolRiftTier, 9) << "the field before the cut is in place";
	EXPECT_EQ(loaded._iOracoolRerolls, 0);
	EXPECT_EQ(loaded._iOracoolLockedAffix, -1);
	EXPECT_EQ(LoadingItemFormatForTest(), today) << "the loader's scope put today's format back";

	// Where _iPLCR sits: the one place two records differing only in it differ.
	Item other = item;
	other._iPLCR = 18;
	const std::vector<uint8_t> otherBytes = SaveItemBytesForTest(other);
	ASSERT_EQ(otherBytes.size(), v15.size());
	size_t at = 0;
	while (at < v15.size() && v15[at] == otherBytes[at])
		at++;
	ASSERT_LT(at, v15.size());
	std::vector<uint8_t> v13 = v14;
	v13.erase(v13.begin() + static_cast<std::ptrdiff_t>(at), v13.begin() + static_cast<std::ptrdiff_t>(at) + 4);
	loaded = {};
	ASSERT_TRUE(LoadItemBytesForTest(v13, 13, loaded));
	EXPECT_EQ(loaded._iPLCR, 0);
	EXPECT_EQ(loaded._iPLMana, 5 << 6) << "the field after the missing one is read in place";
	EXPECT_EQ(loaded._iOracoolRiftTier, 9);
	EXPECT_EQ(LoadingItemFormatForTest(), today);

	EXPECT_FALSE(LoadItemBytesForTest(v15, 12, loaded)) << "12 is refused";
}

TEST_F(LoadSaveOracoolItemExtensionsTest, RoundTripsFullyPopulatedTieredItem)
{
	_uiheroinfo info {};
	info.heroclass = HeroClass::Warrior;
	ASSERT_TRUE(pfile_ui_save_create(&info));

	Player &creator = Players[0];
	MyPlayer = &creator;

	ASSERT_GT(creator._pNumInv, 0) << "a freshly created Warrior should start with at least one inventory item to tag";
	SetFullOracoolTierData(creator.InvList[0]);
	// Item format 15: the Mystic's counters and lock ride the item, so the menu no longer resets them (2026-09-27).
	creator.InvList[0]._iOracoolRerolls = 3;
	creator.InvList[0]._iOracoolRemovals = 2;
	creator.InvList[0]._iOracoolLockedAffix = 4;
	const uint32_t seed = creator.InvList[0]._iSeed;
	const uint16_t createInfo = creator.InvList[0]._iCreateInfo;
	const _item_indexes idx = creator.InvList[0].IDidx;

	pfile_write_hero();

	Player &loaded = Players[1];
	pfile_read_player_from_save(0, loaded);

	ASSERT_GT(loaded._pNumInv, 0);
	Item *restored = nullptr;
	for (int i = 0; i < loaded._pNumInv; i++) {
		if (loaded.InvList[i].keyAttributesMatch(seed, idx, createInfo))
			restored = &loaded.InvList[i];
	}
	ASSERT_NE(restored, nullptr) << "matching item was not found in the reloaded inventory";

	EXPECT_TRUE(restored->hasOracoolTier());
	EXPECT_EQ(restored->_iOracoolTier, OracoolItemTier::Primal);
	EXPECT_TRUE(restored->_iOracoolPerfectRoll);
	ASSERT_EQ(restored->_iOracoolAffixCount, Item::MaxOracoolAffixes);
	for (int i = 0; i < Item::MaxOracoolAffixes; i++) {
		EXPECT_EQ(restored->_iOracoolAffixes[i].type, static_cast<item_effect_type>(IPL_STR + i));
		EXPECT_EQ(restored->_iOracoolAffixes[i].param1, 10 + i);
		EXPECT_EQ(restored->_iOracoolAffixes[i].param2, 20 + i);
	}
	EXPECT_EQ(restored->_iOracoolRerolls, 3);
	EXPECT_EQ(restored->_iOracoolRemovals, 2);
	EXPECT_EQ(restored->_iOracoolLockedAffix, 4);
}

// CHARACTERISATION, not a regression test - and the distinction is the point of the comment.
//
// It pins the contract single-player now states outright: the stored item record wins, and the seed
// replay in UnPackItem is a FALLBACK for when there is no stored record, not a gatekeeper for one.
//
// The item is the historically destructive combination on purpose: an Oracool index wearing a
// CF_SMITH town stamp. That stamp sends a replay through RecreateTownItem, which re-derives the
// item's INDEX by walking the droppable-item pool - a pool every Oracool item is excluded from - so
// the replay cannot produce this charm and must produce something else.
//
// This test passes both before and after LoadMatchingItems was simplified, and that is the finding
// rather than a weakness in the test: the guard it removed (`unpackedItem._iSeed != heroItem._iSeed`)
// could never fire, because every Recreate* path copies the seed across verbatim. The replay's
// result was already being overwritten for every non-empty item. Verified by reintroducing the
// guard and watching this still pass.
TEST_F(LoadSaveOracoolItemExtensionsTest, StoredRecordWinsOverAnUnreplayableItem)
{
	_uiheroinfo info {};
	info.heroclass = HeroClass::Warrior;
	ASSERT_TRUE(pfile_ui_save_create(&info));

	Player &creator = Players[0];
	MyPlayer = &creator;
	ASSERT_GT(creator._pNumInv, 0);

	Item &planted = creator.InvList[0];
	InitializeItem(planted, IDI_ORACOOL_CHARM_VIGOR);
	planted._iSeed = 0x5EEDF00D;
	// The poisonous stamp: "a normal item bought from Griswold at level 5". The replay path obeys it
	// and rebuilds the index from the pool, which cannot produce an Oracool charm.
	planted._iCreateInfo = 5 | CF_SMITH;
	planted._iIdentified = true;
	planted._iStatFlag = true;

	pfile_write_hero();

	Player &loaded = Players[1];
	pfile_read_player_from_save(0, loaded);

	ASSERT_GT(loaded._pNumInv, 0);
	bool found = false;
	for (int i = 0; i < loaded._pNumInv; i++) {
		if (loaded.InvList[i].IDidx == IDI_ORACOOL_CHARM_VIGOR)
			found = true;
	}
	EXPECT_TRUE(found) << "the stored record was discarded and the seed replay's substitute kept";
}

// No item on the freshly created character carries an Oracool tier, so SaveOracoolItemExtensions
// must skip writing "heroitemsext" entirely - exactly what an old, pre-feature save looks like.
// Loading must leave every item at its default (untiered) state without error.
TEST_F(LoadSaveOracoolItemExtensionsTest, AbsentExtensionFileLeavesItemsUntiered)
{
	_uiheroinfo info {};
	info.heroclass = HeroClass::Warrior;
	ASSERT_TRUE(pfile_ui_save_create(&info));

	Player &creator = Players[0];
	MyPlayer = &creator;
	pfile_write_hero();

	Player &loaded = Players[1];
	pfile_read_player_from_save(0, loaded);

	ASSERT_GT(loaded._pNumInv, 0);
	for (int i = 0; i < loaded._pNumInv; i++)
		EXPECT_FALSE(loaded.InvList[i].hasOracoolTier());
}

// Oracool Tabbed Inventory: an item stored in one of the 9 extra backpack pages must survive a
// full save/load round trip, and must not leak into tab 1 or any other extra tab.
TEST_F(LoadSaveOracoolItemExtensionsTest, RoundTripsExtraTabItems)
{
	_uiheroinfo info {};
	info.heroclass = HeroClass::Warrior;
	ASSERT_TRUE(pfile_ui_save_create(&info));

	Player &creator = Players[0];
	MyPlayer = &creator;

	// Tab index 2 (displayed as tab 3) holds one item; every other tab stays empty.
	Item tabItem;
	tabItem._itype = ItemType::Misc;
	tabItem._iSeed = 0x11223344;
	tabItem._iCreateInfo = 0x2200;
	tabItem.IDidx = IDI_ROCK;
	creator.InvTabList[2][0] = tabItem;
	creator.InvTabGrid[2][0] = 1;
	creator._pNumInvTab[2] = 1;

	pfile_write_hero();

	Player &loaded = Players[1];
	pfile_read_player_from_save(0, loaded);

	ASSERT_EQ(loaded._pNumInvTab[2], 1);
	EXPECT_EQ(loaded.InvTabGrid[2][0], 1);
	EXPECT_EQ(loaded.InvTabList[2][0]._itype, ItemType::Misc);
	EXPECT_EQ(loaded.InvTabList[2][0].IDidx, IDI_ROCK);
	EXPECT_TRUE(loaded.InvTabList[2][0].keyAttributesMatch(0x11223344, IDI_ROCK, 0x2200));

	for (int t = 0; t < Player::NumExtraInventoryTabs; t++) {
		if (t == 2)
			continue;
		EXPECT_EQ(loaded._pNumInvTab[t], 0) << "tab index " << t << " should still be empty";
	}
}

// Reproduces a real reported bug: an item stored in an extra tab, saved once, then removed
// (e.g. picked up and dropped) and saved again, must NOT reappear on the next load. MPQ
// archives are updated in place rather than rewritten from scratch each save, so the first
// save's "heroinvtabs" entry - unless explicitly deleted - would still be sitting in the
// archive for the second load to read back, resurrecting an item that no longer exists.
TEST_F(LoadSaveOracoolItemExtensionsTest, RemovedExtraTabItemDoesNotReappearAfterASecondSave)
{
	_uiheroinfo info {};
	info.heroclass = HeroClass::Warrior;
	ASSERT_TRUE(pfile_ui_save_create(&info));

	Player &creator = Players[0];
	MyPlayer = &creator;

	// First save: tab index 3 (displayed as tab 5) holds one item.
	creator.InvTabList[3][0]._itype = ItemType::Misc;
	creator.InvTabList[3][0].IDidx = IDI_ROCK;
	creator.InvTabGrid[3][0] = 1;
	creator._pNumInvTab[3] = 1;
	pfile_write_hero();

	// The item is picked up and dropped (or sold, or consumed) - the tab is empty again.
	creator.InvTabList[3][0].clear();
	creator.InvTabGrid[3][0] = 0;
	creator._pNumInvTab[3] = 0;
	pfile_write_hero();

	Player &loaded = Players[1];
	pfile_read_player_from_save(0, loaded);

	EXPECT_EQ(loaded._pNumInvTab[3], 0) << "the removed item must not resurrect from a stale heroinvtabs entry";
}

// A tiered item (Rare/Buffed Unique/Primal) stored in an extra tab must keep its tier/affix
// data across a save/load round trip, exactly like one stored in the original backpack -
// SaveItem/LoadItemData carry every item's tier/affix data directly now, so this needs no
// container-specific wiring at all; InvTabList round-trips through the exact same two
// functions as InvList/InvBody/SpdList.
TEST_F(LoadSaveOracoolItemExtensionsTest, RoundTripsTieredItemStoredInExtraTab)
{
	_uiheroinfo info {};
	info.heroclass = HeroClass::Warrior;
	ASSERT_TRUE(pfile_ui_save_create(&info));

	Player &creator = Players[0];
	MyPlayer = &creator;

	Item tabItem;
	tabItem._itype = ItemType::Misc;
	tabItem._iSeed = 0x99887766;
	tabItem._iCreateInfo = 0x1100;
	tabItem.IDidx = IDI_ROCK;
	SetFullOracoolTierData(tabItem);
	creator.InvTabList[5][0] = tabItem;
	creator.InvTabGrid[5][0] = 1;
	creator._pNumInvTab[5] = 1;

	pfile_write_hero();

	Player &loaded = Players[1];
	pfile_read_player_from_save(0, loaded);

	ASSERT_EQ(loaded._pNumInvTab[5], 1);
	Item &restored = loaded.InvTabList[5][0];
	EXPECT_TRUE(restored.hasOracoolTier());
	EXPECT_EQ(restored._iOracoolTier, OracoolItemTier::Primal);
	EXPECT_TRUE(restored._iOracoolPerfectRoll);
	ASSERT_EQ(restored._iOracoolAffixCount, Item::MaxOracoolAffixes);
	for (int i = 0; i < Item::MaxOracoolAffixes; i++) {
		EXPECT_EQ(restored._iOracoolAffixes[i].type, static_cast<item_effect_type>(IPL_STR + i));
		EXPECT_EQ(restored._iOracoolAffixes[i].param1, 10 + i);
	}
}

// Dropped ground items, the Stash, and Tabbed Inventory's extra tabs all round-trip through
// the exact same SaveItem/LoadItemData functions InvBody/InvList/SpdList already use (see
// SaveDroppedItems/LoadDroppedItems, SaveStash/LoadStash, SaveInventoryTabs/LoadInventoryTabs) -
// there's no separate per-container matching step left to test in isolation the way the old
// "heroitemsext" sidecar mechanism needed. RoundTripsFullyPopulatedTieredItem above is the
// authoritative proof that SaveItem/LoadItemData's tier/affix fields round-trip correctly;
// every other container inherits that same guarantee for free. Ground-item persistence
// specifically isn't unit-testable without exposing loadsave.cpp's internal SaveHelper/
// LoadHelper machinery just for a test, so it's covered by manual QA instead (see
// Testing-Guide.md).

// No extra tab has ever been touched on a freshly created character, so SaveInventoryTabs must
// skip writing "heroinvtabs" entirely - exactly what an old, pre-feature save looks like.
TEST_F(LoadSaveOracoolItemExtensionsTest, AbsentInvTabsFileLeavesTabsEmpty)
{
	_uiheroinfo info {};
	info.heroclass = HeroClass::Warrior;
	ASSERT_TRUE(pfile_ui_save_create(&info));

	Player &creator = Players[0];
	MyPlayer = &creator;
	pfile_write_hero();

	Player &loaded = Players[1];
	pfile_read_player_from_save(0, loaded);

	for (int t = 0; t < Player::NumExtraInventoryTabs; t++)
		EXPECT_EQ(loaded._pNumInvTab[t], 0);
}

// Defensive clamp on the write side: an out-of-range in-memory affix count (which should never
// happen via normal code paths, but must not be trusted blindly) must not corrupt the stream or
// crash - only the actual MaxOracoolAffixes-sized array contents are ever persisted.
TEST_F(LoadSaveOracoolItemExtensionsTest, OutOfRangeInMemoryCountIsClampedOnSave)
{
	_uiheroinfo info {};
	info.heroclass = HeroClass::Warrior;
	ASSERT_TRUE(pfile_ui_save_create(&info));

	Player &creator = Players[0];
	MyPlayer = &creator;

	ASSERT_GT(creator._pNumInv, 0);
	SetFullOracoolTierData(creator.InvList[0]);
	creator.InvList[0]._iOracoolAffixCount = 200; // corrupt in-memory value
	const uint32_t seed = creator.InvList[0]._iSeed;
	const uint16_t createInfo = creator.InvList[0]._iCreateInfo;
	const _item_indexes idx = creator.InvList[0].IDidx;

	pfile_write_hero();

	Player &loaded = Players[1];
	pfile_read_player_from_save(0, loaded);

	Item *restored = nullptr;
	for (int i = 0; i < loaded._pNumInv; i++) {
		if (loaded.InvList[i].keyAttributesMatch(seed, idx, createInfo))
			restored = &loaded.InvList[i];
	}
	ASSERT_NE(restored, nullptr);
	EXPECT_LE(restored->_iOracoolAffixCount, Item::MaxOracoolAffixes);
}

TEST_F(LoadSaveOracoolItemExtensionsTest, StashRoundTripsTieredItem)
{
	Stash = {};
	Item tieredItem;
	tieredItem._itype = ItemType::Misc;
	tieredItem._iSeed = 0xABCD1234;
	tieredItem._iCreateInfo = 0x55AA;
	tieredItem.IDidx = IDI_ROCK;
	SetFullOracoolTierData(tieredItem);
	Stash.stashList.push_back(tieredItem);
	Stash.dirty = true;

	sfile_write_stash();

	Stash = {};
	LoadStash();

	ASSERT_EQ(Stash.stashList.size(), 1u);
	EXPECT_TRUE(Stash.stashList[0].hasOracoolTier());
	EXPECT_EQ(Stash.stashList[0]._iOracoolTier, OracoolItemTier::Primal);
	EXPECT_TRUE(Stash.stashList[0]._iOracoolPerfectRoll);
	ASSERT_EQ(Stash.stashList[0]._iOracoolAffixCount, Item::MaxOracoolAffixes);
}

/**
 * The hotkey chunk is a FALLBACK, not the truth (user, 2026-09-02: "make hotkeys remembered over
 * games. i dont want to set hotkeys every new game. this is PER CHARACTER setting.").
 *
 * It lives in the game save, which V1 never continues from, so what it holds is whichever session
 * last wrote it. The bindings themselves ride the hero file - HeroChunkSpellHotkeys and friends -
 * and ApplyHeroChunks has already put them in the player by the time LoadHotkeys runs. Vanilla's
 * version cleared the array before reading, which threw those away and refilled from the stale
 * chunk; every new game the F-keys came back as some older set.
 *
 * Both halves of the new rule are asserted, because dropping the second would look like a fix and
 * would silently retire the chunk instead: a slot the hero file filled keeps what it has, and a slot
 * it left empty still takes what the chunk offers.
 */
TEST_F(LoadSaveOracoolItemExtensionsTest, LoadHotkeysFillsEmptySlotsWithoutOverwritingTheHeroFiles)
{
	_uiheroinfo info {};
	info.heroclass = HeroClass::Warrior;
	ASSERT_TRUE(pfile_ui_save_create(&info));

	Player &player = Players[0];
	MyPlayer = &player;

	// An earlier session's game save: two bindings, written exactly as SaveGame writes them.
	std::fill(player._pSplHotKey, player._pSplHotKey + NumHotkeys, SpellID::Invalid);
	std::fill(player._pSplTHotKey, player._pSplTHotKey + NumHotkeys, SpellType::Invalid);
	player._pSplHotKey[0] = SpellID::Firebolt;
	player._pSplTHotKey[0] = SpellType::Spell;
	player._pSplHotKey[1] = SpellID::Healing;
	player._pSplTHotKey[1] = SpellType::Spell;
	{
		SaveWriter writer(paths::PrefPath() + "single_0.sv");
		SaveHotkeys(writer, player);
	}

	// This session, after the hero file has been applied: slot 0 is bound to something else, slot 1
	// is empty.
	player._pSplHotKey[0] = SpellID::Flash;
	player._pSplTHotKey[0] = SpellType::Spell;
	player._pSplHotKey[1] = SpellID::Invalid;
	player._pSplTHotKey[1] = SpellType::Invalid;
	// The hero knows Healing: since 2026-09-27 the fallback restores only a binding the hero can use (HeroHasBinding).
	player._pMemSpells |= GetSpellBitmask(SpellID::Healing);

	LoadHotkeys();

	EXPECT_EQ(player._pSplHotKey[0], SpellID::Flash)
	    << "the game save overwrote a binding the hero file had already restored - the reported bug";
	EXPECT_EQ(player._pSplHotKey[1], SpellID::Healing)
	    << "an empty slot did not take the game save's binding, so the chunk is being ignored rather than used as a fallback";

	// And a binding the hero cannot use is not put back (audit, 2026-09-27): the fallback restored refunded skills.
	player._pMemSpells &= ~GetSpellBitmask(SpellID::Healing);
	player._pSplHotKey[1] = SpellID::Invalid;
	player._pSplTHotKey[1] = SpellType::Invalid;
	LoadHotkeys();
	EXPECT_EQ(player._pSplHotKey[1], SpellID::Invalid) << "a spell the hero does not know came back on a key";
}

} // namespace
} // namespace devilution
