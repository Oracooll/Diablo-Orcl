/**
 * @file oracool/imbuement.h
 *
 * Oracool: Imbuement Shards - the Mystic Orbs' replacement (2026-09-19; plan in
 * 01-Project-Overview/Plan - Imbuement Shards.md, decisions on the Imbuement Shards artifact).
 *
 * A shard is a consumable dropped onto a piece of gear. Three properties, two of them the orbs' and
 * the third the reason the orbs were replaced rather than renamed:
 *
 *  - **Fixed value, never rolled.** A shard is a known quantity the player plans around.
 *  - **Many per item, one cap for every kind** (MaxOracoolImbuements, twenty - decision D2). The cap
 *    is what makes a shard a decision: twenty into one weapon finishes it, the twenty-first has to go
 *    somewhere else. A few kinds stop earlier (D3) because twenty of them would be another item.
 *  - **Recorded, not baked.** The item carries a LEDGER of the kinds it took (Item::_iOracoolImbuements);
 *    the effect is computed from that ledger every time the sheet is totalled, through the same
 *    bonus-source seam sockets and set bonuses use (ApplyImbuementsToTotals, the "imbuements"
 *    provider in stat_sheet.cpp). Nothing is written into the item's stat fields, so a rebuild or a
 *    reroll keeps the shards, the tooltip can list them, and the crafting refusal the orbs needed is
 *    gone. Two kinds act on the ITEM rather than the sheet - Tempering (maximum durability, an item
 *    field) and Ease (the requirements, read through EffectiveRequirement) - and Refinement scales
 *    the item's own affix totals; all three are still derived from the ledger, never stored twice.
 *
 * Permanent by decision D9, with one way out: the Cleanse recipe at Levski's Roar strips every shard
 * from an item and returns none (StripImbuements).
 *
 * The twenty-four kinds and their item indices come out of tools/GenImbuementShards.ps1 (shards_kinds.inc).
 * The first eight sit on the orbs' old item indices - positional save format - and the sixteen new ones
 * are appended after the Necromancer's bases, which is why IsOracoolShardIdx is two ranges.
 */
#pragma once

#include <array>
#include <cstdint>
#include <string>

#include "itemdat.h"

namespace devilution {
struct Item;
struct Player;
namespace oracool {
struct ItemBonusTotals;
}
} // namespace devilution

namespace devilution::oracool {

/** @brief The kinds, in the generator's order. The value is what the ledger stores, so it is save format: append only. */
enum class ShardKind : uint8_t {
	Strength,
	Dexterity,
	Magic,
	Vitality,
	Warding,
	Fury,
	Fortune,
	Avarice,
	Blood,
	Spirit,
	Keenness,
	Precision,
	Flame,
	Spark,
	Bulwark,
	Stone,
	Ember,
	Storm,
	Veil,
	Radiance,
	Arcana,
	Refinement,
	Tempering,
	Ease,
	LAST = Ease,
};
constexpr int ShardKindCount = static_cast<int>(ShardKind::LAST) + 1;

/** @brief One row of shards_kinds.inc. `limit` 0 means the item cap; `band` is the qlvl the drop walk gates on. */
struct ShardDefinition {
	ShardKind kind;
	int itemIndex;
	const char *name;
	const char *line;
	uint8_t limit;
	uint8_t band;
};

/** @brief The definition for @p kind. */
const ShardDefinition &ShardDef(ShardKind kind);

/** @brief The definition whose item is @p idx, or null if @p idx is not a shard. */
const ShardDefinition *FindShardByItem(int idx);

/** @brief The per-item limit for @p kind, resolved: the cap where the table says 0. */
int ShardLimit(ShardKind kind);

/** @brief How many shards of @p kind @p item carries. */
int ShardCountOfKind(const Item &item, ShardKind kind);

/** @brief Whether @p target is gear that could take a shard at all, ignoring which. */
bool CanReceiveShard(const Item &target);

/**
 * @brief Whether @p target can take one more shard of @p kind: room under the cap, room under the
 * kind's own limit, and the kind's own sense - Tempering declines an indestructible item, Ease an item
 * whose requirements are already gone.
 */
bool CanReceiveShardKind(const Item &target, ShardKind kind);

/**
 * @brief Applies @p held (a shard) to @p target. False if either side declines; nothing changes then.
 *
 * Records the kind in the ledger. Tempering also raises the item's maximum durability at once, because
 * durability is an item field and not a sheet total; everything else is read from the ledger later.
 */
bool TryImbue(const Player &player, Item &target, const Item &held);

/** @brief "Imbued: 3 / 20" for @p item, or empty if it has taken none and cannot. */
std::string ImbueCountLine(const Item &item);

/** @brief "Strength x3, Refinement x2" - every kind on @p item with its count, or empty if none. */
std::string ImbueBreakdownLine(const Item &item);

/** @brief The description line of the shard item @p idx (untranslated), or empty if it is not a shard. */
const char *ShardLine(int idx);

/** @brief Feeds @p item's ledger into @p totals - the provider's per-item step. Refinement scales @p item's own affix totals. */
void ApplyImbuementsToTotals(const Item &item, ItemBonusTotals &totals);

/** @brief Ease: how many points come off each of @p item's requirements. */
int ShardRequirementReduction(const Item &item);

/** @brief Tempering: how much maximum durability @p item's ledger adds. */
int ShardDurabilityBonus(const Item &item);

/**
 * @brief A copy of an item's ledger, for the crafting recipes that rebuild an item from a bare
 * InitializeItem: capture before, restore after, and the shards survive the rebuild (decision D9's
 * permanence cuts both ways - a rebuild may not delete them either).
 */
/** @brief The ledger's capacity; imbuement.cpp asserts it equals Item::MaxOracoolImbuements. */
constexpr int MaxImbuementKinds = 20;
struct ImbuementLedger {
	uint8_t count = 0;
	std::array<uint8_t, MaxImbuementKinds> kinds {};
};
ImbuementLedger CaptureImbuements(const Item &item);
void RestoreImbuements(Item &item, const ImbuementLedger &ledger);

/** @brief The Cleanse recipe: empties the ledger and takes Tempering's durability back with it. */
void StripImbuements(Item &item);

/** @brief Calls @p fn(itemIndex) for every shard kind, in ShardKind order - the drop walk's island walk. */
template <typename Fn>
void ForEachShardItem(Fn fn)
{
	for (int k = 0; k < ShardKindCount; k++)
		fn(ShardDef(static_cast<ShardKind>(k)).itemIndex);
}

} // namespace devilution::oracool
