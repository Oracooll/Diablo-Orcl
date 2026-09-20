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
int SalvageAllInBackpack(Player &player, SalvageTier tier, int *materialsMade = nullptr);
// @p materialsMade, when given, receives how many materials were placed in the pack (the Salvage window's message, 2026-09-21).

/**
 * @brief Whether @p player holds anything of @p tier anywhere in the backpack.
 *
 * Shares SalvageAllInBackpack's walk over EVERY page, so the lit state of a Levski button and what
 * pressing it actually consumes cannot disagree. They did once: both read the displayed tab only.
 */
bool AnySalvageableInBackpack(const Player &player, SalvageTier tier);

/** @brief The Charm of Salvaging that arms @p tier. */
uint16_t SalvageCharmFor(SalvageTier tier);

/** @brief Which tier the Charm of Salvaging @p charmIdx arms. Only call for a salvage charm. */
SalvageTier SalvageTierOfCharm(uint16_t charmIdx);

/**
 * @brief The one-line description a Charm of Salvaging shows in its item popup.
 *
 * Lives here rather than in charms.cpp because the tier names and the material names it quotes are
 * this file's tables; charms.cpp calls through.
 */
const char *SalvageCharmEffectLine(uint16_t charmIdx);

/**
 * @brief Charms of Salvaging: converts @p item to material AT PICKUP, before it reaches the pack.
 *
 * User request, 2026-08-20: "when I put them in inv everytime I pick an item it automatically turns
 * into salvaged material instead of a regular item. save inv slots and time."
 *
 * Only ACTIVE charms count - a salvage charm obeys the same CharmActiveCap as every other charm,
 * so at most three tiers can be armed at once and arming a fourth means retiring one. That cap is
 * also the safety rail: without it a full backpack of charms would silently eat everything.
 *
 * The conversion is destructive and the player never sees the item, so it is NOT silent - it names
 * what was consumed and what it became in the event log. That matters most for the tiers you might
 * regret: a Primal charm eats every Primal, including one you would have kept.
 *
 * @return true when @p item was replaced in place by a material stack; the caller then continues
 * its normal pickup path with the material. False leaves @p item untouched.
 */
bool TrySalvageOnPickup(Player &player, const Item &item);

} // namespace devilution::oracool
