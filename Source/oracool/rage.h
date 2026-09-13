#pragma once
/**
 * @file oracool/rage.h
 *
 * The Barbarian's Rage (user, 2026-09-13: "if he is to not use mana at all then his skill cant cost
 * mana as well. so we must go D3 road here").
 *
 * The Barbarian has no mana. His second orb holds Rage instead: a pool of 100 (120 with Animosity)
 * that starts empty on every level, fills a fixed amount for every blow a GENERATOR skill lands, and
 * is spent by SPENDER skills, which cannot be used without it. It does not drain while he swings at
 * monsters; five seconds after the last swing it drains one point a second (2026-09-14) - or, with
 * Unforgiving, rises two a second instead.
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
/**
 * Ticks after the last swing at a monster before the pool starts to drain - the battle's lingering
 * fury (user, 2026-09-14: "once i stop swinging at monsters start a 5 seconds countdown timer").
 */
constexpr int RageCalmDelayTicks = 100;
/** Ticks per pulse once calm: one point drained - "slowly - 1 rage per 1 second". */
constexpr int RageDecayIntervalTicks = 20;
/** What Unforgiving adds per pulse instead of draining. */
constexpr int UnforgivingRagePerPulse = 2;

/** @brief Whether this class runs on Rage rather than mana. The Barbarian alone. */
bool ClassUsesRage(HeroClass heroClass);
bool UsesRage(const Player &player);

/** @brief Rage one landed use of @p spell generates. 0 for anything that is not a generator. */
int RageGain(SpellID spell);
/** @brief Rage @p spell costs. 0 for anything that is not a spender. */
int RageCost(SpellID spell);

int MaxRage(const Player &player);

/** @brief Adds @p points, clamped to the pool. Counts as combat: the calm clock restarts. */
void GainRage(Player &player, int points);
/**
 * @brief A swing at a monster - landed or not - or a blow that struck one. Restarts the calm clock,
 * so the pool does not drain while the fighting goes on. A swing at an empty tile is not combat.
 */
void NoteRageCombat(Player &player);
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
 * @brief Settles one use of @p spell that did something: a spender's cost is taken once, and a
 * generator grants its Rage for EACH of the @p landedBlows that struck a monster (user, 2026-09-14:
 * "make sure every hit that lands delivers rage") - a Double Swing that lands both blows earns twice.
 * For a mana user the mana price is paid and @p landedBlows is ignored.
 */
void SettleSkill(Player &player, SpellID spell, int landedBlows = 1);

/**
 * @brief The resource line of a skill's tooltip at @p level: "Rage Cost: 10", "Generates 6 Rage",
 * or "Mana Cost: 5" for a mana user. Empty when there is nothing to say.
 */
std::string SkillResourceLine(const Player &player, SpellID spell, int level);

} // namespace oracool
} // namespace devilution
