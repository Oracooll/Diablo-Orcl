/**
 * @file oracool/treasure_class.h
 *
 * Oracool: Phase 5 - treasure classes. What a PLACE is worth farming for.
 *
 * ## The problem
 *
 * Every drop hook in this fork was flat. The socketable roll was 3% gem, 1% charm, 2% rune, 1%
 * jewel on every monster in the game, and the named-set roll was 3% everywhere. Depth changed which
 * items were *eligible* - BandedQlvl gates a Radiant jewel out of the Church - but it never changed
 * what a floor was FOR. Two floors at the same area level were interchangeable, and the only reason
 * to prefer one place over another was how fast you could clear it.
 *
 * That is the gap a treasure class closes: a table attached to a place, saying what that place
 * gives more of than anywhere else.
 *
 * ## What it does NOT do, and why
 *
 * A treasure class here does not touch the BASE ITEM pool. It cannot: `GetItemIndexForDroppableItem`
 * is replayed from an item's seed by `UnPackItem` to recover its index, which makes that pool part
 * of the save format - the constraint recorded on every Oracool drop hook. A zone that changed
 * which swords were in the pool would transform every existing item in every existing save.
 *
 * So a class redistributes the hooks that were already additive: the socketable families and the
 * named-set rate. That is enough to make the Caves the place to farm runes and Hell the place to
 * farm jewels, which is the whole point, and it costs nothing in the save.
 *
 * ## Zones and bosses
 *
 * Two axes, multiplied rather than tabulated separately. The ZONE picks the table - what kind of
 * thing this place gives. The MONSTER picks a multiplier - an ordinary monster 1x, a champion 2x, a
 * unique 4x - so a champion in the Caves is a better rune chance than a champion anywhere else AND
 * a better rune chance than an ordinary Caves monster, without a second table to keep in step.
 *
 * Deliberately a multiplier and not a guarantee. A boss that always drops a socketable turns
 * farming into a loop with no roll in it, and the moment that is true the class stops being a
 * reason to go somewhere and becomes a chore.
 */
#pragma once

#include <cstdint>

#include "levels/gendung.h"

namespace devilution {
struct Monster;
enum _difficulty : uint8_t;
} // namespace devilution

namespace devilution::oracool {

/**
 * @brief What a place gives more of than anywhere else.
 *
 * The four family weights are RELATIVE shares of the socketable draw, not percentages - they are
 * summed and the draw is taken against the total, so a table can be re-tuned without every row
 * having to still add up to a hundred. A zero weight means that family does not drop here at all,
 * which is how the shallow floors keep jewels out.
 */
struct TreasureClass {
	/** @brief Untranslated display name, for the wiki and the event log. */
	const char *name;
	/** @brief Chance per kill, in percent, that ANY socketable drops. */
	int socketablePercent;
	int gemWeight;
	int runeWeight;
	int jewelWeight;
	int charmWeight;
	/** @brief Chance per kill, in percent, of a named set piece. */
	int setPercent;
};

/** @brief The four socketable families, in the order the weights are listed. */
enum class SocketableFamily : uint8_t {
	Gem,
	Rune,
	Jewel,
	Charm,
};

/** @brief The class for @p dungeon. Never null - town included, which gives nothing. */
const TreasureClass &TreasureClassFor(dungeon_type dungeon);

/** @brief The class for the floor the game is currently on. */
const TreasureClass &CurrentTreasureClass();

/**
 * @brief What @p monster multiplies its zone's rates by: 1 ordinary, 2 champion, 4 unique.
 *
 * The reason a champion is worth walking across the room for. Kept as a multiplier on the zone's
 * own numbers rather than a table of its own so that re-tuning a zone re-tunes its bosses with it.
 */
int TreasureBonusFor(const Monster &monster);

/**
 * @brief What @p difficulty multiplies a class's rates by, as a PERCENTAGE. 100 on Normal.
 *
 * The gap this closes (v1.9.16): a treasure class is chosen by DUNGEON TYPE, and a re-run walks the
 * same twenty-four floors. So the Cathedral in Torment paid exactly what the Cathedral in Normal
 * paid - the item LEVEL rose with the area level, so drops got better, but they did not get more
 * frequent, and the socket economy in particular was no denser on the fourth run than the first.
 * That is the definition of "Normal with bigger numbers".
 *
 * A percentage rather than a fourth column on every table, so re-tuning a zone does not mean
 * re-tuning it four times, and so the difficulty ladder is one list a reader can take in.
 */
int DifficultyTreasureScale(_difficulty difficulty);

/** @brief @p percent scaled by the current difficulty and clamped to 100. */
int ScaleRateForDifficulty(int percent);

/**
 * @brief Which family a roll of @p roll lands on, given @p tc's weights.
 *
 * @param roll a value in [0, total weight), drawn by the caller
 *
 * Pure, so the distribution can be tested without killing anything. Returns Gem for a table whose
 * weights are all zero, which cannot happen for a real table and is a defined answer rather than an
 * out-of-range read if one ever does.
 */
SocketableFamily FamilyForRoll(const TreasureClass &tc, int roll);

/** @brief The sum of @p tc's four family weights. */
int TotalFamilyWeight(const TreasureClass &tc);

} // namespace devilution::oracool
