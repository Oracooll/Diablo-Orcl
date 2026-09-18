#pragma once
/**
 * @file oracool/necro_items.h
 *
 * The Necromancer's three item families (Plan - The Necromancer, section 6; phase N9; decisions D6, D9, D10):
 * wands, scythes and shrunken heads, eight bases each, level 1 to 40.
 *
 * They are ordinary bases in AllItemsList (IDI_ORACOOL_NECRO_*) - but NOT in the seeded drop pool: that pool is
 * replayed from item seeds by the pack tests and the multiplayer wire, and growing it re-identified every fixture
 * item (measured, 2026-09-18: a book came back a scythe). So, like the tier gear and the sets, they drop through a
 * hook of their own (items.cpp TrySpawnNecroBase) and reach Griswold through OracoolGearBasesFor, Adria through
 * StockOracoolFixedItems. Their kinds are the engine's: a wand is a one-handed ItemType::Mace (the mace sheets, and the shield swap on them), a
 * scythe a two-handed ItemType::Axe (the axe sheets), a head an ItemType::Shield in the shield hand (the LIGHT
 * shield's look, decision D11). What makes them a family is the index range, asked here.
 *
 * Shrunken heads are HIS ALONE (D10): the pool leaves them out unless the hero is a Necromancer, Adria stocks
 * them only for him, and CanUseItem refuses them to everyone else - so one that is dropped by a Necromancer and
 * picked up by another hero is carried, red, and never worn.
 *
 * Icons and ground tumbles are the vanilla maces', axes' and small shields' until RfA-17's batch B arrives.
 */

#include <cstdint>

namespace devilution {

struct Item;
struct Player;

namespace oracool {

constexpr int NecroBasesPerFamily = 8;

bool IsNecroWandIdx(int idx);
bool IsNecroScytheIdx(int idx);
bool IsNecroHeadIdx(int idx);
/** @brief Any of the twenty-four. */
bool IsNecroBaseIdx(int idx);
bool IsNecroHeadItem(const Item &item);

/** @brief Whether @p player may wear @p item at all, class considered - the heads are the Necromancer's. */
bool ClassMayUseItem(const Player &player, const Item &item);
/** @brief Swift Harvesting: the passive is slotted and a wand or scythe is in hand. */
bool SwiftHarvestingApplies(const Player &player);
/** @brief Whether a shrunken head may be generated right now: the hero of this game is a Necromancer. */
bool NecroHeadsMayDrop();

} // namespace oracool
} // namespace devilution
