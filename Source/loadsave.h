/**
 * @file loadsave.h
 *
 * Interface of save game functionality.
 */
#pragma once

#include <cstdint>
#include <vector>

#include "pfile.h"
#include "player.h"
#include "utils/attributes.h"

namespace devilution {

/** @brief The stash file this game could not read: nothing may go into the stash, which will not be written. */
extern bool StashFileRefused;

extern DVL_API_FOR_TEST bool gbIsHellfireSaveGame;
extern DVL_API_FOR_TEST uint8_t giNumberOfLevels;

void RemoveInvalidItem(Item &pItem);
_item_indexes RemapItemIdxFromDiablo(_item_indexes i);
_item_indexes RemapItemIdxToDiablo(_item_indexes i);
_item_indexes RemapItemIdxFromSpawn(_item_indexes i);
_item_indexes RemapItemIdxToSpawn(_item_indexes i);
bool IsHeaderValid(uint32_t magicNumber);
void LoadHotkeys();
/** @brief Loads the "heroitems" sidecar of the given save slot - a parameter so the hero-select
 * preview can read OTHER slots without touching gSaveNumber. */
/**
 * @brief Loads the hero's worn, packed and belt items from the slot's "heroitems" record.
 * @return false when the record exists but is of another item format or the wrong length - the caller decides
 *   (the real load path stops with a message; the hero-select preview shows the hero without gear). No record
 *   at all is a fresh hero and returns true.
 */
bool LoadHeroItems(Player &player, uint32_t saveNumber);
/**
 * @brief Remove invalid inventory items from the inventory grid
 * @param player The player to remove invalid items from
 */
void RemoveEmptyInventory(Player &player);

/**
 * @brief Load game state
 * @param firstflag Can be set to false if we are simply reloading the current game
 */
void LoadGame(bool firstflag);
void SaveHotkeys(SaveWriter &saveWriter, const Player &player);
void SaveHeroItems(SaveWriter &saveWriter, Player &player);
void SaveGameData(SaveWriter &saveWriter);
void SaveGame();
void SaveLevel(SaveWriter &saveWriter);
void LoadLevel();

/** @brief Whether the current level (currlevel, or setlvlnum on a set level) has a temp or perm save to load. */
bool LevelSaveExists();
void ConvertLevels(SaveWriter &saveWriter);
void LoadStash();
void SaveStash(SaveWriter &stashWriter);

/**
 * @brief Loads the Oracool Tabbed Inventory's 9 extra backpack pages for the current hero save,
 * if any exist; call alongside LoadHeroItems. A missing, future-versioned, or corrupt file is
 * handled gracefully: every extra tab simply stays empty, exactly like an old pre-feature save
 * or a character that never stored anything in a tab. Takes the save slot for the same reason
 * LoadHeroItems does - the hero-select preview reads slots other than the selected one.
 */
void LoadInventoryTabs(Player &player, uint32_t saveNumber);
/** @brief Whether this game's extra-page file was there but could not be read: the pages are empty and locked. */
extern bool InvTabsFileRefused;
/** @brief Saves the Oracool Tabbed Inventory's 9 extra backpack pages; call alongside SaveHeroItems. */
void SaveInventoryTabs(SaveWriter &saveWriter, const Player &player);

/** @brief One item's record as SaveItem writes it, raw - the item-format tests. */
DVL_API_FOR_TEST std::vector<uint8_t> SaveItemBytesForTest(const Item &item);
/** @brief Reads one record written in item format @p format, under the same scope the three loaders use. */
DVL_API_FOR_TEST bool LoadItemBytesForTest(const std::vector<uint8_t> &bytes, uint8_t format, Item &item);
/** @brief The format LoadItemData would read with now - back to today's after every loader. */
DVL_API_FOR_TEST uint8_t LoadingItemFormatForTest();

} // namespace devilution
