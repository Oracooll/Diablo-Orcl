/**
 * @file level_requirement.h
 *
 * Oracool: the character level an item asks for (2026-09-20, the Level Requirements plan, every
 * decision on it taken as recommended).
 *
 * Vanilla Diablo gates an item by strength, magic and dexterity only. This is Diablo II's rule on
 * this fork's own tables:
 *
 *   required level = max( the base's level (its material tier's floor, plus 12 per base tier:
 *                             Normal / Nightmare / Hell / Torment),
 *                         ceil(3/4 x the highest affix's level - the level it rolls at),
 *                         a unique's own UIMinLvl,
 *                         a set piece's own level,
 *                         the highest socketed rune's or gem's level (Diablo II's ladders) )
 *                    - 3 per Shard of Ease, floor 1.
 *
 * DERIVED, never stored: every term is already on the item, so nothing changes in any save format
 * and the number can never disagree with the item. It is asked in the three places the stat
 * requirements are asked (Player::CanUseItem, CalcSelfItems, the item panel) and nowhere else.
 */
#pragma once

#include <cstdint>

namespace devilution {
struct Item;
} // namespace devilution

namespace devilution::oracool {

/** @brief The level @p item asks for; 0 for anything that asks none (potions, scrolls, gold...). */
int RequiredLevel(const Item &item);

/** @brief Diablo II's ratio: the level an affix that rolls at @p affixLevel asks for (three quarters, rounded up). */
int AffixRequiredLevel(int affixLevel);

/** @brief Diablo II's rune ladder: El 11, Eld 11, Tir 13 ... Zod 69; 0 for a non-rune index. */
int RuneRequiredLevel(uint16_t itemIndex);

/** @brief Diablo II's gems: Chipped 1, Flawed 5, Normal 12, Flawless 15, Perfect 18; 0 for a non-gem index. */
int GemRequiredLevel(uint16_t itemIndex);

/** @brief The level a socketed stone raises its host to (rune, gem, or a jewel's drop level); 0 for none. */
int SocketedStoneLevel(uint16_t itemIndex);

/** @brief The level the base alone asks: the material tier's floor plus the base tier's step. */
int BaseRequiredLevel(const Item &item);

/** @brief The level the item's affixes alone ask (the highest of them, through AffixRequiredLevel). */
int AffixesRequiredLevel(const Item &item);

/** @brief Levels taken off by Shards of Ease (3 each). */
int LevelRequirementReduction(const Item &item);

/** @brief 12 - what each base tier above Normal adds. */
constexpr int BaseTierLevelStep = 12;
/** @brief 3 - what each Shard of Ease takes off. */
constexpr int EaseLevelStep = 3;

} // namespace devilution::oracool
