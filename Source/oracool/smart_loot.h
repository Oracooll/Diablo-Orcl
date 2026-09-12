/**
 * @file smart_loot.h
 *
 * Oracool: Smart Loot - drops lean toward the class that found them.
 */
#pragma once

#include "itemdat.h"

namespace devilution {
struct Item;
struct Player;
} // namespace devilution

namespace devilution::oracool {

/**
 * @brief How many candidates an aimed drop generates, keeping the one that suits the class best.
 *
 * Best-of-N rather than reject-until-acceptable, deliberately. A rejection loop needs a threshold
 * ("is this good enough?"), and a threshold is a number nobody can tune honestly: set it high and
 * the loop runs out of attempts and ships the last candidate anyway, set it low and it never fires.
 * Best-of-N needs no threshold, always improves, and cannot fail.
 */
constexpr int SmartLootCandidates = 3;

/**
 * @brief Whether this drop is aimed at the player's class at all.
 *
 * Roughly four drops in five, matching Diablo III's Loot 2.0 tuning. The remaining fifth is left
 * completely alone on purpose (user, 2026-09-13: "any item should be able to drop with a any hero
 * class, just drop percentage should favour more usable one") - every item stays reachable by every
 * class, and the world keeps surprising you.
 */
bool SmartLootShouldAimThisDrop();

/**
 * @brief Whether Smart Loot has any opinion about @p item.
 *
 * Equipment only. Gold, potions, scrolls, books and quest items serve every class the same, and
 * re-rolling them would be pure churn.
 */
bool SmartLootConsiders(const Item &item);

/**
 * @brief How well @p item suits @p player's class. Higher is better; never negative.
 *
 * Weighted by the class's own stat profile, so nothing here hardcodes "Sorcerers want staves".
 */
int SmartLootScore(const Item &item, const Player &player);

/**
 * @brief The same judgement applied to a BASE item index, before anything is rolled onto it.
 *
 * The chest and barrel path chooses its index first, so there is no finished item to score there.
 */
int SmartLootScoreForBase(_item_indexes idx, const Player &player);

} // namespace devilution::oracool
