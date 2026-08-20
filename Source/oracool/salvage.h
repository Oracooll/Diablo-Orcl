/**
 * @file oracool/salvage.h
 *
 * Oracool: breaking unwanted gear down into the seven materials.
 *
 * User request, 2026-08-20: "add salvage all whites, magic, rare, uniques, primal, set, ethereal
 * buttons on levski ui."
 *
 * ## The seven buckets PARTITION the item space
 *
 * Every salvageable item belongs to exactly one bucket, so the seven buttons between them consume
 * each item once and no item twice. That is not the only design available - an ethereal rare could
 * plausibly yield both Rare Fibres and an Ethereal Imbueity - but a partition is the one the
 * BUTTONS demand: with overlap, "Salvage all rares" and "Salvage all ethereal" would each claim the
 * same item, and which one you pressed first would silently change what you got.
 *
 * The order below is the priority order, most specific first: ethereal beats set beats tier beats
 * plain quality. An ethereal item is an ethereal item whatever else it also is.
 */
#pragma once

#include <cstdint>

namespace devilution {
struct Item;
struct Player;
} // namespace devilution

namespace devilution::oracool {

/** @brief The seven, in the order the user listed both the buttons and the orb colours. */
enum class SalvageTier : uint8_t {
	White,
	Magic,
	Rare,
	Unique,
	Primal,
	Set,
	Ethereal,
	LAST = Ethereal,
};
constexpr int SalvageTierCount = 7;

/** @brief The button's label - "Whites", "Magic", ... - for the Levski panel. */
const char *SalvageTierName(SalvageTier tier);

/** @brief The material index this tier breaks down into. */
uint16_t SalvageMaterialFor(SalvageTier tier);

/**
 * @brief Whether @p item can be broken down at all.
 *
 * Equipment only. Materials, gems, runes, charms, potions, scrolls, books, gold and quest items all
 * decline - a system that could eat your runes because you mis-clicked is not worth having.
 */
bool IsSalvageable(const Item &item);

/** @brief Which bucket @p item falls in. Only meaningful when IsSalvageable(item). */
SalvageTier SalvageTierOf(const Item &item);

/** @brief How many materials @p item yields. Scales with what it was, never below one. */
int SalvageYield(const Item &item);

/**
 * @brief Breaks every backpack item of @p tier down, and puts the materials back in the pack.
 *
 * @return the number of ITEMS consumed, which is what the message line reports. Returns 0 without
 * touching anything when the pack has no room for the materials, so a full pack cannot silently
 * destroy gear.
 */
int SalvageAllInBackpack(Player &player, SalvageTier tier);

} // namespace devilution::oracool
