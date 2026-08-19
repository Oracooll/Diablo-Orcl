/**
 * @file oracool/item_tiers.h
 *
 * Oracool: the four BASE tiers - Diablo II's Normal/Exceptional/Elite, one rung longer and named
 * for the four difficulties (user, 2026-08-19).
 *
 * ## Base tier is not item quality
 *
 * Two independent axes, and keeping them independent is the whole point:
 *
 *  - QUALITY - basic, magic, rare, set, unique, primal. What was rolled ON the item.
 *  - BASE TIER - Normal, Nightmare, Hell, Torment. What the item IS underneath: a Short Sword, or a
 *    Short Sword forged for Hell.
 *
 * Every quality can appear on every tier. A Torment basic and a Normal primal are both findable, and
 * neither is a contradiction - which is why nothing ever stops dropping (user, 2026-08-19: "nothing
 * stops dropping anywhere because we will introduce salvaging").
 *
 * ## Why this is an ITEM property, not 670 new base rows
 *
 * The obvious reading of D2 is three more copies of every base in AllItemsList - roughly 670 rows,
 * an enum that triples, and every _item_indexes bound in the codebase to re-audit. This does the
 * same job as a per-item byte instead: the base row stays one row, and the tier scales its numbers
 * as the item is generated.
 *
 * What that buys: no save-index churn (a stored IDidx still means what it meant), no new sprites to
 * cut, no risk of a table and an enum drifting apart, and set and unique items get tiers for free
 * rather than needing a copy of each per tier. What it costs: the base list in a shop reads as one
 * entry per base rather than four - which is what the tier NAME on the item is for.
 */
#pragma once

#include <cstdint>

#include "DiabloUI/ui_flags.hpp"
#include "utils/stdcompat/string_view.hpp"

namespace devilution {
struct Item;
enum class OracoolItemTier : uint8_t;
} // namespace devilution

namespace devilution::oracool {

/**
 * @brief The four base tiers, one per difficulty block of the area ladder.
 *
 * The mapping is deliberately one-to-one with oracool/area_level.h's 24-floor blocks, so the rule
 * is learnable in one sentence: the tier you find is the difficulty you are in.
 */
enum class BaseItemTier : uint8_t {
	Normal = 0,
	Nightmare = 1,
	Hell = 2,
	Torment = 3,
	LAST = Torment,
};

constexpr int BaseItemTierCount = 4;

/** @brief The highest tier an item level may produce. ilvl 1-24 -> Normal, 25-48 -> Nightmare, ... */
BaseItemTier HighestTierForItemLevel(int itemLevel);

/**
 * @brief The tier for an item generated at @p itemLevel from @p seed.
 *
 * Weighted toward the deepest tier the level allows, but never exclusively: a lower tier is a
 * perfectly good find, and once salvaging exists it is raw material rather than litter.
 *
 * Derived from the seed by HASHING it rather than by drawing from the seeded stream, and that is
 * load-bearing. Every generated item is reconstructible from its seed - RecreateItem and UnPackItem
 * replay the whole sequence - so a draw inserted here would shift every affix roll after it and
 * silently change what existing seeds produce. Hashing gives a per-item answer that is stable, and
 * costs the sequence nothing.
 */
BaseItemTier TierForItem(int itemLevel, uint32_t seed);

/** @brief "Normal", "Nightmare", "Hell", "Torment" - untranslated; the caller runs it through _(). */
const char *TierName(BaseItemTier tier);

/**
 * @brief The colour the tier line wears in the item description (user, 2026-08-19).
 *
 * White, blue, yellow, gold - the same four the difficulties are read in everywhere else, so the
 * colour alone identifies the tier before the word is read.
 */
UiFlags TierColor(BaseItemTier tier);

/**
 * @brief The word that goes in front of the base's name - "Jagged Short Sword".
 *
 * Empty for Normal: a Short Sword is a Short Sword, and the un-prefixed name is what makes the
 * prefixed ones read as something more.
 */
const char *TierNamePrefix(BaseItemTier tier);

/**
 * @brief Scales @p item's base numbers - damage, armour, requirements, durability and value - to
 * @p tier, and records it on the item.
 *
 * Called once, immediately after the base row has been copied onto the item and BEFORE any affix
 * rolls, so affixes stack on top of the tiered base exactly as they do on a Normal one.
 */
void ApplyBaseTier(Item &item, BaseItemTier tier);

/**
 * @brief The banded qlvl of a base item - the ilvl a drop needs before this base can appear.
 *
 * The authored ItemData::iMinMLvl is vanilla's ladder, 1-51, written against monster levels that
 * ignored difficulty entirely. The area ladder runs to 96 and its design target is that the LAST
 * base becomes available at Hell/Hell (alvl 61-64), so the authored numbers are regrouped onto a
 * 1-60 scale here rather than rewritten in the table.
 *
 * A mapping rather than a rescale, and the difference matters: items land ON band edges, so many
 * bases share a qlvl and a floor opens a GROUP of them at once. That is how D2 reads - a depth
 * unlocks a shelf, not one sword.
 *
 * ## The replay seam
 *
 * The shared droppable pool is part of the SAVE FORMAT: UnPackItem rebuilds a dungeon item's index
 * by replaying its seed through the pool walk, and RecreateTownItem replays the vendor pools the same
 * way. Banding the filter unconditionally therefore changed what an existing seed rebuilt, and twenty
 * net-pack validation tests said so.
 *
 * So items.cpp carries one flag - ReplayingStoredItemSeed, set by an RAII guard at RecreateItem's
 * entry - and every pool filter asks PoolQlvl(), which answers with the AUTHORED qlvl during a replay
 * and the banded one during fresh generation. Old seeds rebuild exactly as they did; new drops use
 * the ladder.
 */
int BandedQlvl(int authoredQlvl);

/**
 * @brief The configured tier chance for @p itemLevel, in PER MILLE.
 *
 * The INI keeps the master knob (Rare/Buffed Unique/Primal Item Drop Chance); this shapes it against
 * depth, so the same setting means "rare at the top of the curve" rather than one flat number from
 * Cathedral 1 to Torment Crypt. Per mille rather than percent because the early-band primal chance
 * is a fraction of one percent and would round away to never.
 *
 * At the default settings (rare 20, buffed unique 10, primal 5) the curve is roughly:
 *
 *   band       rare   buffed unique   primal
 *   1-24        2.0%       1.0%         0%
 *   25-48       6.0%       2.5%        0.25%
 *   49-72      14.0%       5.0%        0.75%
 *   73-96      22.0%       8.0%        2.0%
 */
int QualityChancePerMille(OracoolItemTier quality, int itemLevel, int configuredPercent);

/**
 * @brief The ilvl a vendor's stock is generated at: their own level, lifted by the difficulty.
 *
 * Vendor level is the deepest floor visited, clamped to 6-16, and it has never known which
 * difficulty the game is on - so Adria in Torment offered exactly what Adria in Normal did. Adding
 * the difficulty block puts shops on the same ladder as the dungeon: 6-16 in Normal, 30-40 in
 * Nightmare, 54-64 in Hell, 78-88 in Torment, which is one tier band per difficulty.
 */
int VendorItemLevel(int vendorLevel);

/**
 * @brief Stamps a vendor item's ilvl and, on a chance, gives it a base tier.
 *
 * A CHANCE rather than the dungeon's flat weighting (user, 2026-08-19: "make vendors have a chance
 * to offer tiered gear related to the difficulty of the game"). A shop is a reliable, repeatable
 * source; if every Torment shelf were Torment-tier the dungeon would stop being where gear comes
 * from. The chance itself is the INI's Vendor Tiered Stock Chance.
 *
 * Derived from @p seed by hashing, salted differently from TierForItem so that "is it tiered" and
 * "which tier" are independent questions rather than the same roll read twice.
 *
 * Called on FRESH stock only, never from the Recreate* twins. Those rebuild an item from a stored
 * seed for the multiplayer pack, which has no room to carry a tier; single-player never keeps a
 * recreated item, since the save holds every stat. Same rule the dungeon path follows.
 */
void ApplyVendorTier(Item &item, int vendorLevel, uint32_t seed, int maxValue);

/**
 * @brief Stamps a vendor item's ilvl WITHOUT touching its stats, for use before GetItemAttrs.
 *
 * Books pick their spell inside GetItemAttrs, and the band gate there reads the item's ilvl - so a
 * shop book needs its ilvl before that call, not after it. Everything else can wait for
 * ApplyVendorTier, which stamps the same number again.
 */
void StampVendorItemLevel(Item &item, int vendorLevel);

/** @brief Whether @p item is something base tiers apply to at all: worn gear, not a potion. */
bool CanCarryBaseTier(const Item &item);

} // namespace devilution::oracool
