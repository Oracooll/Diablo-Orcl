/**
 * @file oracool/stat_sheet.h
 *
 * Oracool: Megaplan Phase 0.4 - the bonus-source aggregation seam.
 *
 * CalcPlrItemVals used to inline ONE bonus source: the equipped-items loop. Every system the
 * megaplan adds wants to feed the same totals - sockets and their gems, runewords, charms working
 * from the backpack, set bonuses, auras, and eventually a hireling's own equipment - and without a
 * seam each of them would have been one more surgery on that monolith. This module is the seam:
 *
 *   - ItemBonusTotals is everything the accumulation loop summed, name for name.
 *   - A BonusProvider contributes to the totals when its condition holds. The condition hook is
 *     deliberate: "IF three pieces of the set are worn THEN +X" is the entire future set-bonus
 *     system expressed as a provider flavour rather than a new mechanism.
 *   - BonusContext carries the OWNER. Per-entity by design, not per-player: a hireling is a second
 *     sheet fed by its own providers, not a special case inside the player's.
 *
 * The class-specific derived math (Rogue/Monk/Bard/Barbarian damage mods, resist clamps, HP/mana
 * multipliers) deliberately stays in CalcPlrItemVals - it is not a bonus SOURCE, it is what the
 * game does with the totals, and it differs per class, not per source.
 *
 * Adding a source: append a BonusProvider to the Providers table in stat_sheet.cpp. Order is
 * irrelevant - every contribution is additive/OR-ing, so providers commute.
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
 * @brief Every quantity the equipment loop accumulates, one field per vanilla local.
 * lightRadius is the DELTA against the base of 10, which stays in CalcPlrItemVals.
 */
struct ItemBonusTotals {
	int minDamage = 0;
	int maxDamage = 0;
	int armor = 0;
	int bonusDamage = 0;
	int bonusToHit = 0;
	int bonusArmor = 0;
	ItemSpecialEffect flags = ItemSpecialEffect::None;
	ItemSpecialEffectHf damAcFlags = ItemSpecialEffectHf::None;
	int strength = 0;
	int magic = 0;
	int dexterity = 0;
	int vitality = 0;
	SpellMask spells;
	int fireResist = 0;
	int lightningResist = 0;
	int magicResist = 0;
	int damageMod = 0;
	int getHit = 0;
	int lightRadius = 0;
	int hitPoints = 0;
	int mana = 0;
	int spellLevelAdd = 0;
	int enhancedAccuracy = 0;
	int fireMin = 0;
	int fireMax = 0;
	int lightningMin = 0;
	int lightningMax = 0;
	/** @brief Phase 1: Magic Find - % chance for a plain drop to upgrade to a Rare-tier roll.
	 * Consumed in the UNSEEDED drop tail (ApplyMagicAndGoldFindToDrop), never in seeded setup. */
	int magicFind = 0;
	/** @brief Phase 1: Gold Find - % increase on dropped gold piles. Same unseeded consumption. */
	int goldFind = 0;
	/** @brief Movement Speed +X%: the items' IPL_MOVESPEED affixes plus the Paladin's Vigor by rank (2026-09-07). */
	int moveSpeed = 0;

	/**
	 * @brief Accumulates one item with the vanilla loop's exact semantics: nothing from an empty
	 * or requirement-failed (_iStatFlag false) item; base damage/AC/granted-spell always; the
	 * bonus block only when normal-quality or identified; the percentage-of-own-AC computation
	 * with its sign fallback.
	 */
	void AddItem(const Item &item);
};

/** @brief Who the totals are being computed FOR. A hireling passes itself here one day. */
struct BonusContext {
	const Player *owner;
};

/**
 * @brief One source of bonuses. `isActive` null means always-on; otherwise the provider is
 * consulted first - the condition hook that set bonuses ("three pieces worn?") ride on.
 */
struct BonusProvider {
	const char *name;
	bool (*isActive)(const BonusContext &ctx);
	void (*apply)(const BonusContext &ctx, ItemBonusTotals &totals);
};

/** @brief Runs every registered provider whose condition holds, in table order. */
void AccumulateBonuses(const BonusContext &ctx, ItemBonusTotals &totals);

/**
 * @brief Every non-zero field of @p totals, in player-facing words, joined by @p separator.
 *
 * Written so a tooltip can say what something GIVES without a second table to say it with. The
 * caller zeroes a totals, runs the same apply function the game runs, and hands the result here -
 * so what is printed is what is applied, by construction, and the two cannot drift. That is the
 * whole reason this exists rather than a per-skill description string: this codebase has been
 * bitten three times by a table that described an effect the code did not have (see
 * oracool/unique_affixes.h), and a tooltip is the worst place to be wrong, because the player will
 * believe it.
 *
 * Empty when nothing moved, which is the honest answer for a skill whose effect this struct cannot
 * carry - the flags, the procs, the bespoke behaviour. Those keep their authored sentence.
 */
std::string DescribeBonusTotals(const ItemBonusTotals &totals, const char *separator = ", ");

} // namespace devilution::oracool
