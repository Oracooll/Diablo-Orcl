/**
 * @file oracool/paladin_ranged.h
 *
 * Oracool: the Paladin skills that strike at a distance rather than riding a swing - Fist of the
 * Heavens, Blessed Shield, Blessed Hammer.
 *
 * Separate from oracool/paladin_melee.h because they answer a different question at a different
 * moment: the melee three are asked "this swing landed, do you add anything?" from inside DoAttack,
 * while these are asked "the button was pressed, do something" from CheckPlrSpell, before any
 * animation exists.
 */
#pragma once

#include "engine/point.hpp"
#include "oracool/paladin_skills.h"

namespace devilution {

struct Player;

namespace oracool {

/**
 * @brief Casts @p skill at @p target, and reports whether it did anything.
 *
 * Returns false for a skill this module does not implement, which is how the caller knows to fall
 * back to a plain swing rather than leaving the button inert. That fallback is the same rule the
 * rest of these skills follow: the ability never "does nothing".
 *
 * Charges mana only on success, and only once the effect is committed.
 */
bool CastRangedPaladinSkill(Player &player, PaladinSkill skill, Point target);

/**
 * @brief Everything Fist of the Heavens does the moment the mace lands.
 *
 * Split out because the descent is a missile and the impact is not: ProcessFallingMace calls this on
 * the animation's last frame, so the blast, the sound and the mini-Nova all happen when the mace is
 * seen to hit the ground rather than when the button was pressed.
 *
 * @p damage is already the weapon roll for this cast; the mana was charged at the cast.
 */
void FistOfTheHeavensImpact(Player &player, Point target, int damage, int spellLevel);

/** @brief Fist of the Heavens', Blessed Shield's and Blessed Hammer's facts, one per line. For the tooltip. */
std::string PaladinRangedFactsAt(PaladinSkill skill, int rank);

} // namespace oracool
} // namespace devilution
