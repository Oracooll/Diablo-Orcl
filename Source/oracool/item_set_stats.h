/**
 * @file oracool/item_set_stats.h
 *
 * The keyword vocabulary the fifteen delivered item sets are written in, and what each keyword
 * means to THIS engine.
 *
 * The sets arrive as data - one set-data.json per set under Resources/02. Oracooll Assets/01. Used/item-sets -
 * and their stats are written in a vocabulary of their own: 107 keywords, from "strength" to
 * "wearer_direct_damage_taken_by_target_per_spent". Roughly a third of them name something this
 * engine already has a channel for. The rest name bespoke set mechanics that nothing here can do.
 *
 * This table is the one place that judgement is recorded. Every keyword in the delivered data has a
 * row, and a row says one of three things:
 *
 *   Power    - it compiles to an ItemPower (an IPL_* effect), and the item really does it.
 *   Approx   - it compiles to an ItemPower that is CLOSE but not exact. The note says how it differs.
 *   Inert    - listed, described, and does nothing. The note says what it would have needed.
 *
 * The inert rows are deliberate and follow the rule the class tree established: an ability the
 * engine has no channel for is listed and described honestly rather than hidden or silently
 * dropped. A player reading "Cinderbrand" on a five-piece bonus should find it in the game's own
 * text; they should not find it quietly missing, and they should not find a bright number that does
 * nothing.
 *
 * @see oracool/class_tree.h for the same rule applied to skills.
 */
#pragma once

#include <cstddef>
#include <cstdint>

#include "itemdat.h"
#include "utils/stdcompat/string_view.hpp"

namespace devilution::oracool {

/** @brief How faithfully a set-data keyword survives the trip into this engine. */
enum class SetStatFidelity : uint8_t {
	/** Exact: the engine does what the keyword says. */
	Power,
	/** Close: the engine does something adjacent. `note` says what was traded away. */
	Approx,
	/** Nothing: no channel exists. `note` says what building it would take. */
	Inert,
};

struct SetStatMapping {
	/** @brief The keyword exactly as it appears in set-data.json, before the colon. */
	const char *keyword;
	SetStatFidelity fidelity;
	/** @brief The effect it compiles to, or IPL_INVALID when inert. */
	item_effect_type power;
	/**
	 * @brief For Approx, what the difference is. For Inert, what it would need.
	 *
	 * Untranslated and developer-facing: this is the record of a decision, not player text. What the
	 * player sees is the item's own description.
	 */
	const char *note;
};

/**
 * @brief Every keyword the sets use - the 107 delivered, plus ten this fork added.
 *
 * The nine are channels the ENGINE already had and this table had simply never named: all
 * attributes at once, armour against demons/undead, fire and lightning and multiple arrows, half
 * trap damage, and life/mana steal. They exist because the delivered ladders were largely written
 * in mechanics this engine cannot do, and the rungs had to be re-authored out of things it can -
 * see item_set_bonus_overrides.txt. Adding a keyword is the intended way to widen that palette;
 * inventing a number for an inert one is not.
 *
 * The tenth, faster_cast_rate (2026-09-11), is a channel the fork added itself: IPL_FASTCAST.
 */
constexpr size_t SetStatMappingCount = 117;

extern DVL_API_FOR_TEST const SetStatMapping SetStatMappings[SetStatMappingCount];

/**
 * @brief The row for @p keyword, or nullptr if the vocabulary does not contain it.
 *
 * A nullptr means the data has grown a keyword this table has not been told about, which is a
 * content error rather than a runtime one - the loader should refuse it loudly rather than silently
 * dropping the stat.
 */
const SetStatMapping *FindSetStat(string_view keyword);

/** @brief Whether the keyword resolves to something the player will actually feel. */
bool IsSetStatLive(string_view keyword);

} // namespace devilution::oracool
