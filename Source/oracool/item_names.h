/**
 * @file item_names.h
 *
 * Oracool: the item name pool - two evocative words, generated on the fly from the item's own seed.
 */
#pragma once

#include <cstdint>
#include <string>

namespace devilution::oracool {

/**
 * @brief A two-word name for a rolled item, derived from @p seed.
 *
 * D3 style, at the user's direction (2026-09-13): "we must generate a pool of name affixes to make
 * items sound more interesting and generate their names on the fly in real time", and "we can have
 * that as well" for names "from a per-type name pool unrelated to what rolled".
 *
 * Diablo II builds a magic item's name out of the affixes that rolled - "Garnet Cap of the Tiger" -
 * which is why it can only ever carry one prefix and one suffix: a six-affix item has no name. Once
 * the budget went flat and D3-style (see OracoolAffixBudget), the naming had to follow, because
 * there is no longer a prefix and a suffix to build a name out of.
 *
 * DERIVED, NEVER ROLLED. The name is hashed out of the seed and consumes no randomness at all. That
 * is not a detail: SetupAllItems is replayed from a stored seed to rebuild every dungeon item on
 * load, and a single extra GenerateRnd call inside that replay shifts every roll after it - which
 * is exactly how a "Helm of harmony" once came back as a "Great Helm of haste". Hashing the seed
 * instead means the name is perfectly reproducible and costs the replay nothing.
 */
std::string GenerateOracoolItemName(uint32_t seed);

/** @brief Sizes of the two word tables, for the audit test to walk them. */
size_t OracoolNameAdjectiveCount();
size_t OracoolNameNounCount();

/** @brief One word from each table, by index - for the audit test. */
const char *OracoolNameAdjective(size_t index);
const char *OracoolNameNoun(size_t index);

} // namespace devilution::oracool
