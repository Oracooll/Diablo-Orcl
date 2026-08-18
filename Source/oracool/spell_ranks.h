/**
 * @file oracool/spell_ranks.h
 *
 * Oracool: the Rule of Rangs, and the character level a spell demands before it can be learned.
 *
 * Two rules, user-authored 2026-08-19, and they compose:
 *
 *  1. LEARNING. Every book spell sits in one of SIX bands - level 1, 6, 12, 18, 24 or 30 - the same
 *     six tiers the class trees use. Band 1 was added on 2026-08-19 ("i want sorc to be able to
 *     obtain spells right away. i just forgot about lvl 1 tier"): without it the cheapest spells
 *     were locked away from a level-1 character who had just found their first book. Below the
 *     band the spell cannot be learned at all, and its book cannot be read.
 *
 *  2. THE RULE OF RANGS. "Each Rang Level requires +1 lvl compared to previous rang." So rank 1 of a
 *     spell wants its band's level, rank 2 wants one more, rank 3 one more again. A level-6 spell
 *     reaches rank 10 at character level 15, and rank 94 at level 99.
 *
 * The second rule is what replaces the old flat cap: there is no ceiling on a skill or a spell any
 * more (MaxSkillInvestment is the whole pool a character can ever earn), but every rank past the
 * first costs a character level, so depth is paid for in the same currency breadth is.
 *
 * The bands are derived from each spell's own sBookLvl - the dungeon depth its book drops at, which
 * is the game's own statement of how advanced the spell is - and then written out explicitly here,
 * so retuning one spell is a one-line edit rather than a change to a formula that moves forty.
 */
#pragma once

#include "spelldat.h"

namespace devilution {
struct Player;
} // namespace devilution

namespace devilution::oracool {

/** @brief The six bands, in ascending order. Exported so the sheets can group by them. */
constexpr int SpellLevelBands[] = { 1, 6, 12, 18, 24, 30 };
constexpr int SpellLevelBandCount = 6;

/**
 * @brief The character level @p spell demands before it can be learned at all, or 0 if it is not a
 * book spell (class-tree skills carry their own minLevel, and the innate abilities have none).
 */
int SpellRequiredLevel(SpellID spell);

/**
 * @brief The character level needed to hold @p rank of @p spell - the Rule of Rangs applied.
 *
 * @p rank is 1-based: rank 1 is the first point in it. Ranks below 1 answer the same as rank 1.
 */
int SpellRankRequiredLevel(SpellID spell, int rank);

/** @brief The Rule of Rangs on a bare requirement, for callers that carry their own (the trees). */
constexpr int RankRequiredLevel(int baseLevel, int rank)
{
	return baseLevel + (rank > 1 ? rank - 1 : 0);
}

/**
 * @brief The ilvl a BOOK of @p spell needs before it can be generated - its qlvl, in D2 terms.
 *
 * Derived from the spell's own band so the two rules cannot drift: a book you could not read is a
 * book that should not have dropped (user, 2026-08-19: "i dont want to find book of apocalips on
 * Cathedral LVL1 in Normal"). Apocalypse is band 30, so its book needs ilvl 52 - Hell Cathedral.
 */
int SpellBookItemLevel(SpellID spell);

/** @brief Whether @p player is high enough to learn @p spell from a book at all. */
bool CanLearnSpell(const Player &player, SpellID spell);

/**
 * @brief Whether @p player may raise @p spell's book level to @p rank by reading a book.
 *
 * The book gate and the Rule of Rangs in one question, which is exactly what reading a book asks.
 */
bool CanReadSpellBookTo(const Player &player, SpellID spell, int rank);

} // namespace devilution::oracool
