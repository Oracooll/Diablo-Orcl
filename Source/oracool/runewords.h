/**
 * @file oracool/runewords.h
 *
 * Oracool: Megaplan Phase 1 - runewords, D2's crown jewel, improved on one axis: the recipes are
 * DISCOVERABLE IN-GAME. Every rune's own description lists the runewords it belongs to, so the
 * knowledge drops with the runes themselves instead of living on a wiki.
 *
 * A runeword forms when a fully socketed, plain (NORMAL, tierless) item holds EXACTLY the right
 * rune sequence in order. The state is entirely DERIVED - recomputed from socket contents plus
 * host type whenever it is asked for - so it costs the save format nothing and can never desync
 * from the sockets that define it. On completion the item takes the runeword's name (that part is
 * persisted naturally, through the name field every save already carries).
 */
#pragma once

#include <string>

#include "itemdat.h"

namespace devilution {
struct Item;
} // namespace devilution

namespace devilution::oracool {

struct ItemBonusTotals;

struct RunewordDefinition {
	const char *name;
	/** @brief Which host category (gems.h's SocketHost) the word forms in. */
	uint8_t host; // SocketHost as uint8_t to keep the table constexpr-friendly
	uint8_t runeCount;
	uint16_t runes[3]; // IDI_ORACOOL_RUNE_* in required order
	// The word's own bonuses, applied ON TOP of each rune's individual socket effect.
	int bonusDamagePercent;
	int damageMod;
	int toHit;
	int allResists;
	int bonusAc;
	int spellLevels;
	int mana; // whole points
	int hitPoints; // whole points
};

/** @brief The runeword @p item currently completes, or nullptr. Fully derived, never stored. */
const RunewordDefinition *GetActiveRuneword(const Item &item);

/** @brief Applies @p word's bonuses onto @p totals. */
void ApplyRunewordToTotals(const RunewordDefinition &word, ItemBonusTotals &totals);

/** @brief Description lines: the runes' teaching text ("Runeword 'Steel': Tir El, in weapons")
 * for rune @p runeIdx - one string per word it appears in, joined with newlines. */
std::string RuneTeachingLines(uint16_t runeIdx);

/** @brief Called after a socket insertion: if @p item now completes a runeword, renames it and
 * returns true (caller plays feedback and recalculates). */
bool TryCompleteRuneword(Item &item);

} // namespace devilution::oracool
