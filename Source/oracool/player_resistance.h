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
 *   2. **Floor at zero.** See below - this is a deliberate departure.
 *   3. **The soft cap.** Up to 75 a point of resistance is a point. Past it each point counts for
 *      1/`SoftCapDivisor`, up to a hard ceiling of 90. Reaching 90 therefore costs 75 + 45 = 120 raw
 *      points on Normal, and 180 on Hell.
 *
 * ## Why the floor stays at zero
 *
 * D2, which this borrows from, lets resistances go NEGATIVE, so an unprepared character on Hell takes
 * amplified elemental damage. That works mechanically here too - `resper` is applied as
 * `damage - damage * resper / 100` in missiles.cpp, so a negative value amplifies with no further
 * change. It is not done, because it is a much larger balance swing than the one being asked for:
 * every character who has not built for resistance becomes glass on Nightmare, including on the way
 * there. The stated goal is "resistance gear matters at endgame", and the penetration alone delivers
 * that. Removing the floor is a one-line change in ApplyResistanceCurve if it is ever wanted.
 *
 * ## Save format
 *
 * Untouched, and this is not luck. The three fields are `int8_t` and hold only the FINAL value, which
 * this bounds to [0, 90]; the raw total has always been a local `int` inside CalcPlrItemVals. Old
 * characters load and are simply recomputed, as they are on every equipment change anyway.
 */
#pragma once

#include "player.h"
#include "utils/attributes.h"

namespace devilution::oracool {

/** @brief Where returns start diminishing. The value vanilla used as a hard cap. */
constexpr int ResistanceSoftCap = MaxResistance;

/** @brief The ceiling no amount of gear passes. */
constexpr int ResistanceHardCap = 90;

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
 * @return a value in [0, ResistanceHardCap].
 */
DVL_API_FOR_TEST int ApplyResistanceCurve(int raw, _difficulty difficulty);

} // namespace devilution::oracool
