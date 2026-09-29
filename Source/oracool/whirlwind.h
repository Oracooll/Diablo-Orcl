/**
 * @file oracool/whirlwind.h
 *
 * Oracool: the Barbarian's Whirlwind, redesigned as his signature move (user, the Barbarian Skill Cards page, 2026-09-29:
 * "It should only be assignable to right mouse button and be cast infinitely while rage is available and the hero must
 * follow cursor position. ... use the magic casting spritesheet to animate rotation AND the magic cloud around the hero
 * while he is gliding over the tiles. He is constantly swinging while rotating. Swinging is not animated but every
 * landed swing produces the holybolt explosion animation and the impact sound. Rage is constantly depleted at 5 rage per
 * second while whirlwind is active. Whirlwind must move at run speed of hero and must not stand still").
 *
 * - Held, not clicked: pressing the right button with Whirlwind readied starts the spin, releasing it ends it
 *   (decided 2026-09-29). It also ends when the Rage runs out, on death, and on leaving the level.
 * - While it spins the hero glides toward the cursor at run speed through the engine's own walk (so walls, doors and
 *   monsters block him the way they block a walk); on the cursor's tile he keeps going the way he was heading.
 * - He is drawn with his magic cast sheet, its facing turning a step every WhirlwindTurnTicks - the rotation - and its
 *   frames playing on - the cloud.
 * - Every WhirlwindStrikeTicks (0.25 s, decided 2026-09-29) each enemy beside him takes a real blow (to-hit roll and
 *   all) at WhirlwindDamagePercent of a normal one. Each that lands shows a quarter-size Holy Bolt burst, pale warm; the
 *   Impact cue sounds once for the strike.
 * - WhirlwindRagePerSecond Rage is drained while it spins; its Rage cost is only what it takes to start.
 */
#pragma once

#include <optional>

#include "engine/clx_sprite.hpp"
#include "spelldat.h"

namespace devilution {

struct Player;

namespace oracool {

/** Ticks between the spin's strikes: four a second. */
constexpr int WhirlwindStrikeTicks = 5;
/** Rage the spin drains a second. */
constexpr int WhirlwindRagePerSecond = 5;
/** Ticks per step of the spin's turn through the eight facings. */
constexpr int WhirlwindTurnTicks = 1;

/** @brief Whether @p spell may only sit on the right button - Whirlwind, which is held there. */
bool WhirlwindRightButtonOnly(SpellID spell);

/** @brief Whether @p player is spinning. The local player only. */
bool IsWhirlwinding(const Player &player);

/**
 * @brief The right button went down with Whirlwind readied: start spinning, if there is Rage to start with and this is
 * not town. False when it did not start.
 */
bool StartWhirlwind(Player &player);

/** @brief Ends the spin - the step under way finishes, then he stands. */
void StopWhirlwind(Player &player);

/** @brief Drops the spin without touching the player - a level change, a new game. */
void ResetWhirlwind();

/** @brief The spin's tick: the held button, the Rage, the glide and the strikes. Once per game tick, the local player. */
void ProcessWhirlwindTick(Player &player);

/** @brief What a blow of the spin deals, in percent of a normal one, at @p rank: 66%, +5% a rank. */
int WhirlwindDamagePercent(int rank);

/** @brief The sprite to draw @p player with while the spin lasts - the magic cast sheet, turning - or none. */
std::optional<ClxSprite> WhirlwindSprite(const Player &player);

} // namespace oracool
} // namespace devilution
