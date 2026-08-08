/**
 * @file warrior_splash.h
 *
 * Oracool: user request - a Warrior's melee attack also damages monsters near the one actually
 * struck. Falls off with tile distance: full damage at 1 tile, 50% at 2 tiles, 25% at 3 tiles,
 * each an additional ring layered on top of the closer ones rather than a replacement. The
 * configurable range option controls how many rings apply (0 disables entirely, up to 3).
 */
#pragma once

#include "monster.h"
#include "player.h"

namespace devilution::oracool {

/**
 * @brief True if this player's next landed melee hit should splash - Warrior class, the option
 * is above 0, and this is a single-player game.
 */
bool IsWarriorSplashDamageEnabled(const Player &player);

/**
 * @brief Applies falloff-scaled damage to every other hittable monster within the configured
 * range of primaryTarget's tile. primaryDamage is the fixed-point (1/64) damage just dealt to
 * primaryTarget by the landed hit that triggered this. Deliberately calls the low-level
 * ApplyMonsterDamage/M_StartHit/M_StartKill primitives directly rather than re-running the full
 * melee-hit path, so splash targets don't re-roll to-hit or re-trigger life/mana-steal effects.
 */
void ApplyWarriorSplashDamage(Player &player, Monster &primaryTarget, int primaryDamage);

} // namespace devilution::oracool
