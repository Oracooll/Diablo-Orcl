/**
 * @file oracool/aura_field.h
 *
 * Oracool: Megaplan Phase 3.4 - the monster-facing aura pass.
 *
 * Every aura in the game so far points INWARD: it reaches the character sheet through the bonus
 * providers in oracool/stat_sheet.h and never touches anything else. A whole family of skills point
 * outward instead - Conviction strips the enemy, Sanctuary repels the undead, Holy Freeze chills
 * them, the Barbarian's warcries frighten them - and every one of those rows has been sitting inert
 * across four class trees waiting for a way to say "and it does something to THEM".
 *
 * ## Why this is a query and not a per-tick mutation
 *
 * The obvious implementation walks nearby monsters each tick and writes to them - drop their
 * resistance, raise their armour, set a flag. That is a trap this project has already paid for
 * twice (see the audit lifetime checklist): `Monster::resistance` and friends are SAVED, so anything
 * written into them has to be un-written when the aura stops, when the player walks away, when the
 * character dies, when the level unloads - and every one of those is a place to forget.
 *
 * So nothing is stored. An aura field is computed FROM the player's live state - which aura is lit,
 * how many points are in it, where the player is standing - at the moment a question is asked. Walk
 * out of range and the answer changes by itself, because there was never any state to restore. It
 * is the same move the bonus providers made for the character sheet, pointed the other way.
 *
 * ## What it hooks
 *
 * `Monster::isImmune` and `Monster::isResistant` are the entire resistance seam - two functions,
 * four call sites - which is why Conviction is the flagship consumer. Repulsion rides
 * `MonsterGoal::Retreat`, the same channel `M_FallenFear` has always used for the Fallen.
 */
#pragma once

#include <cstdint>

#include "engine/point.hpp"

namespace devilution {
struct Monster;
struct Player;
} // namespace devilution

namespace devilution::oracool {

/**
 * @brief How far an aura reaches, in tiles, for @p points invested.
 *
 * Deliberately modest and slow-growing: an aura that covers the screen stops being a thing you
 * position yourself with and becomes a thing you forget you have on.
 */
int AuraRadiusForPoints(int points);

/**
 * @brief Whether @p monster stands inside the local player's lit Conviction, and how deep.
 *
 * @return invested points, or 0 when Conviction is not lit, not unlocked, or @p monster is out of
 * range. Zero is the "no field here" answer, so callers can use it as a plain boolean too.
 */
int ConvictionPointsOn(const Monster &monster);

/**
 * @brief @p resistances as Conviction of @p points leaves them. Pure, so it is testable alone.
 *
 * The ladder, and why it never makes anything outright vulnerable that was immune: plain
 * resistances are stripped FIRST, and only then are immunities stepped down INTO plain resistances.
 * So a shallow Conviction neutralises a resistant monster, and a deep one turns an immune monster
 * into a merely resistant one. An immunity is a design statement about a monster; a strong aura
 * should erode it, not delete it.
 */
uint16_t ConvictionAdjusted(uint16_t resistances, int points);

/**
 * @brief @p monster's resistance bits as they stand right now, Conviction included.
 *
 * THE accessor for "what does this monster actually resist" - `Monster::isImmune` and
 * `Monster::isResistant` both go through it, which is the whole resistance seam. Returns the
 * monster's own bits untouched when no field is on it, so the ordinary case costs one comparison.
 */
uint16_t EffectiveResistances(const Monster &monster);

/**
 * @brief Per-tick outward work for the local player's lit aura. Called from ProcessClassTreeTick.
 *
 * Only auras that must PUSH (repulsion) live here; anything a monster can be ASKED about at the
 * point of use belongs in a query above instead, because a query cannot go stale.
 */
void ProcessOutwardAura(Player &player);

} // namespace devilution::oracool
