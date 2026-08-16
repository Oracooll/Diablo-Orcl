/**
 * @file oracool/gems.h
 *
 * Oracool: Megaplan Phase 1 - gems and sockets, the raw-material half of the item endgame.
 *
 * The design imports Diablo II's soul with Diablo I's restraint:
 *   - Sockets roll ONLY on plain, tierless NORMAL-quality equipment (TryAddSocketsToDroppedItem),
 *     which makes "basic item" - the label that used to mean "vendor it" - the raw material of the
 *     socket economy, exactly the role white items played in D2's runeword culture.
 *   - Five gems (Ruby, Sapphire, Topaz, Emerald, Skull), one quality tier each for now, with
 *     HOST-DEPENDENT effects: what a gem does in a weapon differs from what it does in armor or a
 *     shield. That asymmetry is the whole fun of socketing decisions.
 *   - A socketed gem is permanent (no Hellforge yet - a possible crafting recipe later).
 *
 * The gems are identified by base-item index alone (IsOracoolGemIdx); effects live in the table
 * here and reach the character sheet through the "sockets" bonus provider in stat_sheet.cpp.
 */
#pragma once

#include <string>

#include "itemdat.h"

namespace devilution {
struct Item;
} // namespace devilution

namespace devilution::oracool {

struct ItemBonusTotals;

/** @brief The three effect groups a socket host falls into. */
enum class SocketHost : uint8_t {
	Weapon,
	Shield,
	/** Body armor, helms, and the six worn accessory types. */
	Armor,
};

/** @brief Which effect group @p hostType's sockets use. */
SocketHost SocketHostForItemType(ItemType hostType);

/** @brief Whether @p item may receive sockets at drop time: tierless NORMAL-quality equipment
 * of a socketable type (weapons, shields, body armor, helms, the worn accessories). */
bool CanItemHaveSockets(const Item &item);

/** @brief Applies gem @p gemIdx's effect for @p host onto @p totals. Unknown indices are inert. */
void ApplyGemToTotals(uint16_t gemIdx, SocketHost host, ItemBonusTotals &totals);

/** @brief One line of description for @p gemIdx socketed in @p host, e.g. "Ruby: +8 fire damage".
 * Translated and formatted; empty for an unknown index. */
std::string GemSocketLine(uint16_t gemIdx, SocketHost host);

/**
 * @brief The insertion rule, in one place: if @p held is a gem and @p target has an open socket,
 * the gem goes into the first empty slot and the function returns true (caller clears the cursor
 * and recalculates). False leaves both items untouched, and the paste proceeds as a normal swap.
 */
bool TrySocketGem(Item &target, const Item &held);

} // namespace devilution::oracool
