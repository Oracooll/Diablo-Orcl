/**
 * @file oracool/charms.h
 *
 * Oracool: Megaplan Phase 1 - charms, with Diablo II's lesson learned.
 *
 * A charm is a passive that works from the BACKPACK. D2's mistake was letting the whole backpack
 * become a charm rack, taxing every pickup with inventory Tetris. The fix here is an ACTIVE CAP
 * rather than a physical pouch: the first CharmActiveCap charms found in the backpack (reading
 * order: tab 1 first, then the extra tabs) are live, every one after them is dead weight, and the
 * item description says which state a charm is in. The cap IS the pouch, economically - a
 * dedicated pouch UI can arrive later with its art, changing nothing underneath.
 *
 * Effects flow through the "charms" bonus provider in stat_sheet.cpp.
 */
#pragma once

#include <string>

#include "itemdat.h"

namespace devilution {
struct Item;
struct Player;
} // namespace devilution

namespace devilution::oracool {

struct ItemBonusTotals;

/** @brief How many charms may be live at once. */
constexpr int CharmActiveCap = 3;

/** @brief Applies charm @p charmIdx's effect. Unknown indices are inert. */
void ApplyCharmToTotals(uint16_t charmIdx, ItemBonusTotals &totals);

/** @brief The description line for @p charmIdx, e.g. "+20 life while in your backpack". */
std::string CharmEffectLine(uint16_t charmIdx);

/**
 * @brief Whether the charm at backpack list position @p invListIndex (tab -1 = the main backpack,
 * else the extra tab index) is within the active cap for @p player right now.
 */
bool IsCharmActive(const Player &player, int tabIndex, int invListIndex);

/** @brief The charm provider's whole walk, shared with the description code: calls @p visit for
 * every ACTIVE charm in reading order, stopping at the cap. */
void ForEachActiveCharm(const Player &player, void (*visit)(uint16_t charmIdx, void *context), void *context);

} // namespace devilution::oracool
