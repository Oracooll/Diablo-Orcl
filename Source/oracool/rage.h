#pragma once
/**
 * @file oracool/rage.h
 *
 * The Barbarian's Rage (user, 2026-09-13: "if he is to not use mana at all then his skill cant cost
 * mana as well. so we must go D3 road here").
 *
 * The Barbarian has no mana. His second orb holds Rage instead: a pool of 100 (120 with Animosity)
 * that starts empty on every level, fills a fixed amount each time a GENERATOR skill lands a blow,
 * and is spent by SPENDER skills, which cannot be used without it. Out of combat it drains away -
 * after three seconds with no Rage gained or spent, one point every quarter second.
 *
 * Which skill is which, and by how much, is the user's own ledger (the Barbarian Rage artifact,
 * picks read 2026-09-13) - see RageGain and RageCost. Passives are neither.
 *
 * Every other class keeps mana, and the pay helpers below take the mana path for them unchanged, so
 * the skill modules ask one question - "can this hero pay for this skill" - whatever the resource.
 *
 * Whole points, not the 1/64 fixed point life and mana are kept in: nothing earns a fraction of Rage.
 * Transient - never saved; a level always starts at zero.
 */

#include <string>

#include "spelldat.h"

namespace devilution {

struct Player;
enum class HeroClass : uint8_t;

namespace oracool {

/** The pool before passives. */
constexpr int BaseMaxRage = 100;
/** What Animosity adds to it. */
constexpr int AnimosityRage = 20;
/** Ticks with no Rage gained or spent before the pool starts to drain. Three seconds. */
constexpr int RageDecayDelayTicks = 60;
/** Ticks per point drained once it does. Four points a second. */
constexpr int RageDecayIntervalTicks = 5;

/** @brief Whether this class runs on Rage rather than mana. The Barbarian alone. */
bool ClassUsesRage(HeroClass heroClass);
bool UsesRage(const Player &player);

/** @brief Rage one landed use of @p spell generates. 0 for anything that is not a generator. */
int RageGain(SpellID spell);
/** @brief Rage @p spell costs. 0 for anything that is not a spender. */
int RageCost(SpellID spell);

int MaxRage(const Player &player);

/** @brief Adds @p points, clamped to the pool. Counts as combat: the drain clock restarts. */
void GainRage(Player &player, int points);
/** @brief Empties the pool and resets the drain clock - every level entry. */
void ResetRage(Player &player);
/** @brief The out-of-combat drain. Called once per game tick from ProcessClassTreeTick. */
void ProcessRageTick(Player &player);

/**
 * @brief Whether @p player can use @p spell right now: enough Rage for a Rage user's spender, enough
 * mana for everyone else. A generator is always affordable.
 */
bool CanPaySkill(const Player &player, SpellID spell);

/**
 * @brief Settles one use of @p spell that did something: a spender's cost is taken, a generator's
 * Rage is granted; for a mana user, the mana price is paid. Callers invoke it only when the skill
 * landed, which is what makes Rage "for every hit".
 */
void SettleSkill(Player &player, SpellID spell);

/**
 * @brief The resource line of a skill's tooltip at @p level: "Rage Cost: 10", "Generates 6 Rage",
 * or "Mana Cost: 5" for a mana user. Empty when there is nothing to say.
 */
std::string SkillResourceLine(const Player &player, SpellID spell, int level);

} // namespace oracool
} // namespace devilution
