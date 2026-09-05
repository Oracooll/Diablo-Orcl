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
#include <vector>

#include "itemdat.h"
#include "items.h"        // ItemSpecialEffect - a word's flag bonuses
#include "oracool/gems.h" // SocketHost - what a word's runes do in its host

namespace devilution {
struct Item;
} // namespace devilution

namespace devilution::oracool {

struct ItemBonusTotals;

/**
 * @brief The ten non-jewelry equipment slots a runeword can form in.
 *
 * Wider than gems.h's three-way SocketHost on purpose: a gem's EFFECT only needs to know whether
 * it sits in a weapon, a shield or armour, but a runeword is a named thing that belongs to a slot,
 * and "a belt word" and "a helm word" should not be interchangeable. Rings and amulets are absent
 * deliberately - they hold one socket, and a one-rune word is just a socketed rune.
 */
enum class RunewordHost : uint8_t {
	Weapon,
	Shield,
	Body,
	Helm,
	Shoulders,
	Bracers,
	Gloves,
	Belt,
	Legs,
	Boots,
	None,
};

/** @brief Which slot @p item forms words in, or RunewordHost::None for anything that cannot. */
RunewordHost RunewordHostForItemType(ItemType type);

struct RunewordDefinition {
	const char *name;
	/** @brief Which of the ten slots the word forms in - RunewordHost as uint8_t, so the generated
	 * table stays constexpr-friendly. */
	uint8_t host;
	uint8_t runeCount;
	/** @brief IDI_ORACOOL_RUNE_* in required order, zero-padded. Six because that is the largest
	 * socket count any host can carry (a 2x3 footprint), and a word must fill its host exactly. */
	uint16_t runes[6];
	// The word's own bonuses, applied ON TOP of each rune's individual socket effect.
	int bonusDamagePercent;
	int damageMod;
	int toHit;
	int allResists;
	int bonusAc;
	int spellLevels;
	int mana; // whole points
	int hitPoints; // whole points
	// THE SECOND HALF (2026-09-05, user: "all runewords provide only 3 additional affixes. is that
	// it?" - it was). The engine's own channels, so a word can carry what Diablo II's words carried
	// in spirit: speed, steal, knockback and thorns as flags; the four attributes; the find chances;
	// flat damage reduction; light; a single resistance where a word favours one element.
	ItemSpecialEffect flags;
	int strength;
	int dexterity;
	int magic;
	int vitality;
	int magicFind; // percent
	int goldFind;  // percent
	int damageReduction; // flat, off every hit taken
	int lightRadius;
	int fireResist;
	int lightningResist;
	int magicResist;
};

/**
 * @brief Every bonus the word grants on its own, one line each, in the item panel's voice:
 * "Damage +12%", "Faster attack", "Strength +10". Feeds the item panel and the runeword book, so
 * the two cannot describe one word two ways.
 */
std::vector<std::string> RunewordBonusLines(const RunewordDefinition &word);

/** @brief The socket-effect group a word's host uses - what its runes do when set in that host. */
SocketHost RunewordSocketHost(RunewordHost host);

/** @brief How many words the table holds - the wiki and the tests both ask. */
size_t RunewordCount();
/** @brief The word at @p index, for enumeration. Null past the end. */
const RunewordDefinition *RunewordAt(size_t index);

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
