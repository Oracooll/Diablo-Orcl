/**
 * @file smart_loot.h
 *
 * Oracool: Smart Loot - drops lean toward the class that found them.
 */
#pragma once

#include <function_ref.hpp>

#include "itemdat.h"

namespace devilution {
struct Player;
} // namespace devilution

namespace devilution::oracool {

/**
 * @brief How many base items an aimed drop considers, keeping the one that suits the class best.
 *
 * Best-of-N rather than reject-until-acceptable: a rejection loop needs a "good enough" threshold
 * nobody can tune honestly, while best-of-N needs none, always improves, and can never turn into a
 * filter - a candidate is kept whenever nothing better turns up.
 */
constexpr int SmartLootCandidates = 3;

/**
 * @brief The score of a base that asks for no main stat at all - rings, amulets, most light gear.
 *
 * Exactly what a perfectly balanced class scores for anything, so a base like this sits in the
 * middle of every class's range rather than at the bottom of it.
 */
constexpr int SmartLootNeutralScore = 333;

/**
 * @brief Whether this drop is aimed at all - roughly four in five, Diablo III Loot 2.0's split.
 *
 * The remaining fifth is untouched (user, 2026-09-13: "any item should be able to drop with a any
 * hero class, just drop percentage should favour more usable one").
 */
bool SmartLootShouldAimThisDrop();

/** @brief Whether @p idx is a base Smart Loot has an opinion about: weapons, armour, rings, amulets. */
bool SmartLootIsEquipmentBase(_item_indexes idx);

/**
 * @brief How well base @p idx suits @p player's class, 0..1000.
 *
 * MAIN STATS ONLY (user, 2026-09-13: "not even FCR as all classes will profit from it. Main stats
 * mainly"), and scored as a SHARE of what the base asks for, not an amount - see the definition.
 */
int SmartLootScoreForBase(_item_indexes idx, const Player &player);

/**
 * @brief The base item this drop should actually use.
 *
 * Returns @p first untouched unless the game is single-player, @p first is equipment, and the aim
 * roll succeeds; then asks @p drawCandidate for SmartLootCandidates - 1 more bases IN THE SAME SLOT
 * as @p first and keeps the best-scoring one.
 *
 * It chooses WHICH BASE fills a slot and nothing else:
 *
 * - Not the SLOT. The blind roll already decided helm, ring or two-handed weapon, and that decision
 *   stands. Scoring across slots starved a Barbarian of jewellery - a ring asks for no main stat, so
 *   almost every Strength base outscored it, and rings and amulets fell to a third of their share.
 *   Diablo III's Smart Loot is the same shape: it biases what is ON an item, not which slot drops.
 * - Not the QUALITY. Magic, rare, unique and primal are rolled afterwards, exactly once, by the
 *   ordinary generation path (user, 2026-09-13: Smart Loot must not change rarity). The first
 *   version generated whole items and kept the best-scoring; tiered items carry more stat affixes,
 *   so they won, and aiming quietly inflated rarity.
 *
 * @param drawCandidate Returns a droppable base for the given slot.
 */
_item_indexes SmartLootAimBase(_item_indexes first, const Player &player, tl::function_ref<_item_indexes(item_equip_type)> drawCandidate);

} // namespace devilution::oracool
