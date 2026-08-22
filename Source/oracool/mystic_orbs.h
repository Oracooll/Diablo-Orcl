/**
 * @file oracool/mystic_orbs.h
 *
 * Oracool: D2MXL-to-ORCL Phase 1 - Mystic Orbs.
 *
 * A consumable that adds ONE fixed small stat to an item, permanently, with a hard cap on how many
 * any single item can take. Median XL's most distinctive mechanic, and the one that changes what a
 * player does with gear they already own rather than with gear they might find.
 *
 * ## Three properties, all of them decisions
 *
 *  - **The cap is per ITEM, not per orb type.** Six into one weapon and it is finished; the seventh
 *    has to go somewhere else. That is what makes an orb a decision rather than an accumulator, and
 *    it is why _iOracoolOrbCount counts ORBS rather than tracking which kinds were used.
 *  - **The value is FIXED, never rolled.** An orb is a known quantity, so a player can plan around
 *    it. A rolled orb is an affix wearing a different name.
 *  - **It is permanent.** Levski's Roar already sells gambling - the whole reroll ladder - and orbs
 *    are deliberately the opposite of it.
 *
 * ## Why the values are small
 *
 * An orb is worth roughly a third of an affix. Six should be a real upgrade to a good base and
 * never a substitute for finding a better item, because the moment orbing beats finding, the drop
 * tables stop mattering and every zone's treasure class becomes decoration.
 *
 * ## What it cost
 *
 * One byte on Item and a save format bump (8 -> 9). Everything else this fork adds per-item is
 * DERIVED - from the item's seed, or the monster's - and derived state needs no storage and cannot
 * disagree with itself. An orb count is a player decision, and there is nowhere to recompute a
 * decision from. That byte is the one irreversible thing in this phase, and Phase 3's growing
 * charms are planned to ride it rather than pay for a second bump.
 */
#pragma once

#include <cstdint>
#include <string>

#include "itemdat.h"

namespace devilution {
struct Item;
struct Player;
} // namespace devilution

namespace devilution::oracool {

/**
 * @brief How many orbs one item can ever take.
 *
 * Six, matching MaxItemSockets - a cap a player can hold in their head is worth more than a tuned
 * one they cannot, and this fork already asks them to remember that number.
 */
constexpr uint8_t MaxOrbsPerItem = 6;

/** @brief Whether @p idx is one of the eight Mystic Orbs. */
bool IsMysticOrbIdx(int idx);

/** @brief The power @p orbIdx grants, or a power of type IPL_INVALID if it is not an orb. */
ItemPower MysticOrbPower(int orbIdx);

/** @brief @p orbIdx's own description line, untranslated. Empty if it is not an orb. */
const char *MysticOrbLine(int orbIdx);

/**
 * @brief Whether an orb can be applied to @p target at all, ignoring which orb it is.
 *
 * Gear only, and gear with room left. Socketables, potions, gold and quest items decline - an orb
 * on a rune would be a stat nothing reads.
 */
bool CanReceiveMysticOrb(const Item &target);

/**
 * @brief Applies @p held to @p target. False if either the orb or the target is ineligible.
 *
 * Goes through ApplyItemPower - the public door onto SaveItemPower that the item sets already use -
 * so an orb's stat lands in exactly the field, with exactly the sign and flag semantics, that every
 * other item's stats land in. Reimplementing the switch here is how an orb would come to grant a
 * subtly different +3 Strength from the one an affix grants.
 */
bool TryApplyMysticOrb(const Player &player, Item &target, const Item &held);

/** @brief The "Mystic Orbs: 3 / 6" line for @p item, or empty if it has taken none and cannot. */
std::string MysticOrbCountLine(const Item &item);

/** @brief The first and last orb item indices, for the drop walk. */
int FirstMysticOrbIdx();
int LastMysticOrbIdx();

} // namespace devilution::oracool
