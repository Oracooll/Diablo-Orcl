/**
 * @file warrior_splash.h
 *
 * Oracool: the Paladin skill **ZEAL** - a melee attack that also damages monsters near the one
 * actually struck. Falls off with tile distance: full damage at 1 tile, 50% at 2, 25% at 3, each an
 * additional ring layered on top of the closer ones rather than a replacement.
 *
 * The file is still called warrior_splash because that is what the mechanic was before it was a
 * skill, and because the class really is HeroClass::Warrior underneath its "Paladin" display name -
 * renaming the file would make it look like it belonged to some other class. Its price and level
 * gate are NOT here: they live in oracool/paladin_skills.h with the rest of the skill's data.
 */
#pragma once

#include "monster.h"
#include "player.h"

namespace devilution::oracool {

/**
 * @brief True if this player's next landed melee hit should carry - Paladin, single-player, level 6
 * or above, AND holding at least Zeal's 2 mana. Below that it simply swings normally.
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
