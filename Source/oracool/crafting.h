/**
 * @file oracool/crafting.h
 *
 * Oracool: Megaplan Phase 1 - crafting, the recipe half of the Horadric Cube with none of the
 * cube: no item takes up a backpack slot to be one.
 *
 * EIGHTEEN recipes (Punch Sockets joined on 2026-09-13), and exactly ONE place they run: Levski's Roar, the monument in town (user,
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

// Nineteen since 2026-09-19: Cleanse Shards (recipe 18), the one way out of an imbuement (decision D9).
// Eighteen since 2026-09-13: Punch Sockets (recipe 17). Both books - Levski's and the burger menu's Crafting
// window - walk this count and the name/inputs table below, so a recipe added here is listed in both.
/**
 * Levski's Cube (2026-09-20, the Cube plan, decisions D5/D7/D8): the recipe book is split between
 * HOSTS. The nineteen recipes the Roar had go to the three artisans as the Roadmap's cards proposed
 * (Griswold the gear, Ogden the stones and sockets, Gillian the charms, sets, magic and shards); the
 * Cube keeps only the Horadric and Kanai additions - rejuvenation, the four Diablo II crafts, and
 * Kanai's Work of Cathan (a level requirement removed). Every recipe belongs to exactly one host and
 * the window shows one host's book at a time.
 */
enum class TransmuteHost : uint8_t {
	Cube,
	Smith,
	Tavern,
	Barmaid,
};

constexpr int RejuvenationRecipe = 19;
constexpr int FullRejuvenationRecipe = 20;
constexpr int UnbindLevelRecipe = 21;
constexpr int CraftBloodRecipe = 22;
constexpr int CraftCasterRecipe = 23;
constexpr int CraftHitPowerRecipe = 24;
constexpr int CraftSafetyRecipe = 25;
/**
 * Kanai's Darkness of Radament (2026-09-20, the user's verdict on the Kanai's Cube Recipes page: "Make it work the way
 * you suggest"): the salvage materials climb and descend a LADDER - White Scales, Magic Powder, Rare Fibres, Set
 * Engravings, Unique Encrustments, Primal Vines - three of one kind refine to one of the tier above, one breaks down
 * to two of the tier below (a tier's material is rarer than the one under it, so the ratios keep salvage worth
 * doing). Both on Griswold's book beside the salvage plates; Ethereal Imbueities stand outside the ladder.
 */
constexpr int RefineMaterialsRecipe = 26;
constexpr int BreakDownMaterialsRecipe = 27;
constexpr int CraftingRecipeCount = 28;

/** @brief Which host's book @p recipe is in. */
TransmuteHost HostOfRecipe(int recipe);
bool RecipeBelongsTo(int recipe, TransmuteHost host);
/** @brief The window title for a host ("Levski's Cube", "Griswold's Forge", ...). */
const char *TransmuteHostTitle(TransmuteHost host);
/** @brief FirstReadyLevskiRecipe restricted to one host's book. */
int FirstReadyLevskiRecipeFor(const Item *grid, TransmuteHost host);

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
