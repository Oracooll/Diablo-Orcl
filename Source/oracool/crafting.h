/**
 * @file oracool/crafting.h
 *
 * Oracool: Megaplan Phase 1 - crafting, the recipe half of the Horadric Cube with none of the
 * cube: it is a WINDOW (opened from the belt's burger menu), not an item taking up a slot.
 *
 * Three launch recipes, all pure material transmutes over the MAIN backpack (tab 1 - the pages
 * you can see are the pages the forge reads):
 *
 *   1. Refine Gems:     three identical gems          -> one of the next quality
 *   2. Ascend Runes:    two identical runes         -> the next rune up (El->Tir->Ral->Ort->Sol)
 *   3. Rework Charms:   two charms of any kind      -> a random charm
 *
 * Together they close the socket economy's loop: surplus gems become runes (the chase), doubled
 * runes climb the ladder (Ancient's Pledge needs Ral, four ascensions from El), and dud charms
 * reroll. Materials are only consumed when the craft fully succeeds - the output's backpack slot
 * is the FIRST check, not the last.
 */
#pragma once

#include <string>

namespace devilution {
struct Item;
struct Player;
} // namespace devilution

namespace devilution::oracool {

constexpr int CraftingRecipeCount = 9;

/**
 * @brief Whether recipe @p index needs the MONUMENT rather than the backpack.
 *
 * The first three recipes turn N small things into one small thing, which the backpack's Craft path
 * handles generically. Everything after them TRANSFORMS an item in place - frees its sockets,
 * rerolls it, ennobles it - and there is no generic "one in, one changed" shape to hang that on, so
 * they live on the grid where the caller can see every slot at once.
 *
 * The burger Crafting window asks this and skips the ones that answer true. Before it did,
 * "Free the Sockets" sat in that window permanently greyed out with nothing saying where it
 * actually lived - it had no backpack case at all, and CanCraft therefore always answered false.
 */
bool CraftingRecipeUsesGrid(int index);

/**
 * @brief Levski's Roar runs its recipes against the MONUMENT'S 3x3 grid rather than the backpack.
 *
 * Same recipes, different larder. The grid is nine Items owned by oracool/levski_roar.cpp and
 * never persisted, so these take a raw array rather than a Player - which also keeps them testable
 * without building a character.
 */
bool CanCraftFromLevskiGrid(const Item *grid, int index);
/** @brief The lowest-numbered recipe the grid can currently run, or -1 for none. */
int FirstReadyLevskiRecipe(const Item *grid);
/**
 * @brief Runs the first ready recipe over @p grid (nine slots), consuming and producing in place.
 * Returns a line for the event log, or empty when nothing was ready.
 */
std::string TransmuteLevskiGrid(Item *grid);

/** @brief Display name for recipe @p index, untranslated (callers wrap in _()). */
const char *CraftingRecipeName(int index);

/** @brief The inputs line, e.g. "3 gems of one kind". Untranslated. */
const char *CraftingRecipeInputs(int index);

/** @brief Whether @p player's main backpack currently holds recipe @p index's materials. */
bool CanCraft(const Player &player, int index);

/**
 * @brief Executes recipe @p index: verifies materials AND output room first, then consumes and
 * produces. Returns the crafted item's display name on success, empty on refusal (missing
 * materials or a full backpack - never a partial consume).
 */
std::string Craft(Player &player, int index);

} // namespace devilution::oracool
