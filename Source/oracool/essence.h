#pragma once
/**
 * @file oracool/essence.h
 *
 * The Necromancer's Essence (plan decisions D3 and D8, 2026-09-17): "Dual orb (Demon Hunter D3 style) -
 * Mana/Essence (dark green)"; "pool of 100. fills within 20 seconds."
 *
 * A SECOND pool beside his mana, not instead of it. Mana pays for what it pays for on everyone - raising the
 * dead, bone and poison. Essence pays for death magic: the curses and the corpse skills. It refills by itself,
 * steadily, fighting or not, and no potion touches it. A row is priced in one or the other, never both, and
 * which one is this module's table (EssenceCost) - so CanPaySkill / SettleSkill (oracool/rage.h) stay the
 * one door every skill pays through.
 *
 * Kept in the 1/64 fixed point life and mana use, unlike Rage: 100 points over 20 seconds at 20 ticks a
 * second is a quarter of a point a tick, and whole points cannot say that.
 *
 * Transient - never saved. A hero enters the game with an empty pool, and it is full before the first
 * staircase.
 */

#include <cstdint>

#include "spelldat.h"

namespace devilution {

struct Player;
enum class HeroClass : uint8_t;

namespace oracool {

/** The pool before passives, in whole points. */
constexpr int BaseMaxEssence = 100;
/** Seconds from empty to full. */
constexpr int EssenceRefillSeconds = 20;
/** Game ticks a second - the engine's fixed logic rate. */
constexpr int EssenceTicksPerSecond = 20;

/** @brief Whether this class carries an Essence pool beside its mana. The Necromancer alone. */
bool ClassUsesEssence(HeroClass heroClass);
bool UsesEssence(const Player &player);

/** @brief The pool's size in whole points. */
int MaxEssence(const Player &player);
/** @brief What is in it, in whole points, rounded down - what the orb and the sheet show. */
int CurrentEssence(const Player &player);

/** @brief Essence @p spell costs, in whole points. 0 for everything priced in mana - which at N2 is everything. */
int EssenceCost(SpellID spell);

/** @brief Whether the pool holds @p points. True for a class without one only when @p points is 0. */
bool HasEssence(const Player &player, int points);
/** @brief Takes @p points, never below empty. */
void SpendEssence(Player &player, int points);
/** @brief Adds @p points, never above full - Essence Tap, Soul Harvest and their like. */
void GainEssence(Player &player, int points);

/** @brief Empties the pool - a hero entering the game. */
void ResetEssence(Player &player);
/** @brief The refill. Called once per game tick from ProcessClassTreeTick. The dead refill nothing. */
void ProcessEssenceTick(Player &player);

} // namespace oracool
} // namespace devilution
