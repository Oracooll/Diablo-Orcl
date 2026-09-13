/**
 * @file oracool/player_resistance.h
 *
 * Oracool: the resistance soft cap and the per-difficulty penetration.
 *
 * ## What this replaces
 *
 * Vanilla clamps each of the player's three resistances to `[0, MaxResistance]` - a HARD cap at 75 -
 * at the end of CalcPlrItemVals, and that is the whole system. Every point past 75 is discarded, and
 * the difficulty a character is playing on does not enter into it. So resistance gear stops mattering
 * the moment a character reaches 75 in a school, which for a Barbarian (who gains her level in all
 * three) happens on the way to the endgame without wearing anything at all.
 *
 * ## The curve
 *
 * Three steps, in this order, and the order is the design:
 *
 *   1. **Penetration.** The difficulty's penalty is subtracted from the RAW total, before any cap.
 *      This is what makes gear matter: on Hell a character needs +60 just to stand where a Normal
 *      character stands for free. Applying it after the cap instead would make the penalty a flat
 *      tax that no amount of gear could answer, which is the opposite of the intent.
 *   2. **Floor at -100.** Resistance can go NEGATIVE, as in D2 - see below.
 *   3. **The soft cap.** Up to 75 a point of resistance is a point. Past it each point counts for
 *      1/`SoftCapDivisor`, up to a hard ceiling of 90. Reaching 90 therefore costs 75 + 45 = 120 raw
 *      points on Normal, and 180 on Hell.
 *
 * ## Negative resistance (since v1.11.127)
 *
 * Until 2026-09-13 the value floored at zero, on the argument that negative resistance was a larger
 * balance swing than asked for. The user asked for it: "just like in D2 allow resists to go below 0 if
 * hero lacks resist affixes". So a hero with no resistance on Torment stands at -90 and takes 190% of
 * every fire, lightning and magic hit; on Nightmare, -30 and 130%.
 *
 * `resper` is applied as `damage - damage * resper / 100`, so a negative value amplifies by itself -
 * but the three sites that apply it (two in missiles.cpp, the burning cross in objects.cpp) only did so
 * for `resper > 0`, and now test `!= 0`. The floor is D2's own -100: double damage at worst.
 *
 * ## Save format
 *
 * Untouched. The three fields are `int8_t` and hold only the FINAL value, which this bounds to
 * [-100, 90], inside the type; the raw total has always been a local `int` inside CalcPlrItemVals.
 * Old characters load and are simply recomputed, as they are on every equipment change anyway.
 */
#pragma once

#include "player.h"
#include "utils/attributes.h"

namespace devilution::oracool {

/** @brief Where returns start diminishing. The value vanilla used as a hard cap. */
constexpr int ResistanceSoftCap = MaxResistance;

/** @brief The ceiling no amount of gear passes. */
constexpr int ResistanceHardCap = 90;

/** @brief The lowest a resistance can fall: D2's -100, which doubles an elemental hit. */
constexpr int ResistanceFloor = -100;

/** @brief Points of raw resistance per point earned above the soft cap. */
constexpr int ResistanceSoftCapDivisor = 3;

/**
 * @brief What each difficulty subtracts from a raw resistance total, before any cap.
 *
 * Evenly spaced rather than copied from D2's 0/-40/-100, for two reasons: this fork has FOUR
 * difficulties where D2 has three, and a regular ladder is easier to reason about when the telemetry
 * CSV is eventually read back and these get tuned against real play.
 */
constexpr int ResistancePenaltyPerDifficulty[] = { 0, 30, 60, 90 };

/** @brief The penalty for @p difficulty. Out-of-range values get the Normal penalty of none. */
DVL_API_FOR_TEST int ResistancePenaltyFor(_difficulty difficulty);

/**
 * @brief Turns a raw resistance total into the value stored on the player.
 *
 * @param raw the summed total from items, class bonuses and spell flags - unbounded.
 * @return a value in [ResistanceFloor, ResistanceHardCap] - negative when the difficulty's penalty
 * exceeds what the hero wears.
 */
DVL_API_FOR_TEST int ApplyResistanceCurve(int raw, _difficulty difficulty);

} // namespace devilution::oracool
