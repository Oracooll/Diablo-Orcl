/**
 * @file oracool/crafting.h
 *
 * Oracool: Megaplan Phase 1 - crafting, the recipe half of the Horadric Cube with none of the
 * cube: no item takes up a backpack slot to be one.
 *
 * SEVENTEEN recipes, and exactly ONE place they run: Levski's Roar, the monument in town (user,
 * 2026-08-31 - "i want levski to be the only place recipies can produce an item. no crafting in
 * hero backpack"). The belt's burger menu still opens a Crafting book, but it is a READING window -
 * it lists the recipes and nothing there can mint an item.
 *
 * The backpack transmute path that ran the first three recipes was DELETED in v1.9.142 rather than
 * merely unhooked from its button. A one-place rule that a second code path can still quietly break
 * is not a rule; the only walks left are the grid ones, below TransmuteLevskiGrid.
 *
 * The recipes close the socket economy's loop: surplus gems become runes (the chase), doubled runes
 * climb the ladder (Ancient's Pledge needs Ral, four ascensions from El), dud charms reroll, and the
 * salvage materials buy the item transforms. Materials are only consumed when the craft fully
 * succeeds - room for the output is the FIRST check, not the last.
 */
#pragma once

#include <string>

namespace devilution {
struct Item;
struct Player;
} // namespace devilution

namespace devilution::oracool {

constexpr int CraftingRecipeCount = 17;

// CraftingRecipeUsesGrid and CraftingRecipeVenue stood here for one version. Both existed to say
// which of two venues a recipe belonged to, and there is only one venue now - every recipe uses the
// grid, so the predicate answered true for all seventeen and the venue line said the same words on
// every row. Removed with the backpack path they described.

/** @brief How many of its reagent recipe @p index charges, or 0 if it takes none. */
int CraftingRecipeReagentCount(int index);

/** @brief The item index of recipe @p index's reagent, or IDI_NONE if it takes none. */
int CraftingRecipeReagentItem(int index);

/**
 * @brief Runs recipe @p index over @p grid, or the auto-picked one when @p index is -1.
 *
 * The selectable form. TransmuteLevskiGrid is this with -1, kept because most callers and every
 * test want "run whatever is ready" - but the monument passes the player's own choice, because with
 * the tier ladder two recipes can want the same target and the same material at different costs and
 * no auto-pick can be the one they meant.
 *
 * A selected recipe that is NOT ready runs nothing and says so, rather than quietly falling back to
 * a different recipe - a fallback here would spend the wrong materials on the right item.
 */
std::string TransmuteLevskiGridWith(Item *grid, int index);

/**
 * @brief Levski's Roar runs its recipes against the MONUMENT'S grid rather than the backpack.
 *
 * Same recipes, different larder. The grid is `LevskiGridSlots` Items - 3 columns by 4 rows, 12 -
 * owned by oracool/levski_roar.cpp and never persisted, so these take a raw array rather than a
 * Player, which also keeps them testable without building a character.
 *
 * This said "3x3" and "nine Items" until 2026-09-12. `LevskiGridSlots` is the authority and is
 * derived from `LevskiGridColumns * LevskiGridRows` in levski_roar.h, so no caller was ever wrong -
 * only the prose.
 */
bool CanCraftFromLevskiGrid(const Item *grid, int index);
/** @brief The lowest-numbered recipe the grid can currently run, or -1 for none. */
int FirstReadyLevskiRecipe(const Item *grid);
/**
 * @brief Runs the first ready recipe over @p grid (`LevskiGridSlots` slots), consuming and
 * producing in place. Returns a line for the event log, or empty when nothing was ready.
 */
std::string TransmuteLevskiGrid(Item *grid);

/**
 * @brief Whether @p result is one of TransmuteLevskiGridWith's refusals - "not enough room to free
 * the stones" or "not enough room for the result" - which consume nothing. Anything else it returns
 * is a report of what it made.
 */
bool IsTransmuteRefusal(const std::string &result);

/** @brief Display name for recipe @p index, untranslated (callers wrap in _()). */
const char *CraftingRecipeName(int index);

/** @brief The inputs line, e.g. "3 gems of one kind". Untranslated. */
const char *CraftingRecipeInputs(int index);

} // namespace devilution::oracool
