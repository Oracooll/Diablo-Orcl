/**
 * @file oracool/paladin_ranged.h
 *
 * Oracool: the Paladin skills that strike at a distance rather than riding a swing - Fist of the
 * Heavens, Blessed Shield, Blessed Hammer.
 *
 * Separate from oracool/paladin_melee.h because they answer a different question at a different
 * moment: the melee three are asked "this swing landed, do you add anything?" from inside DoAttack,
 * while these are CAST (user, 2026-09-11: "Blessed Hammer needs to act as spell"). The click queues a
 * real spell, the hero plays a spell animation at cast speed, and CastSpell hands the skill here at
 * the animation's cast frame.
 */
#pragma once

#include "engine/point.hpp"
#include "oracool/paladin_skills.h"

namespace devilution {

struct Missile;
struct Player;

namespace oracool {

/**
 * @brief Casts @p skill at @p target, and reports whether it did anything.
 *
 * Returns false for a skill this module does not implement, which is how the caller knows to fall
 * back to a plain swing rather than leaving the button inert. That fallback is the same rule the
 * rest of these skills follow: the ability never "does nothing".
 *
 * Charges mana only on success, and only once the effect is committed. Called from CastSpell at the
 * cast frame, so the mana leaves when the skill does.
 */
bool CastRangedPaladinSkill(Player &player, PaladinSkill skill, Point target);

/** @brief Whether @p skill is one of the three that are cast: Fist of the Heavens, Blessed Shield, Blessed Hammer. */
bool IsCastPaladinSkill(PaladinSkill skill);

/**
 * @brief Whether @p skill could be cast right now: one of the three, unlocked, affordable, and with room
 * for its missile.
 *
 * Asked at the CLICK, so a refusal still falls back to a swing before any animation starts.
 * CastRangedPaladinSkill asks again at the cast frame, which is where the mana actually leaves.
 */
bool CanStartRangedPaladinSkill(const Player &player, PaladinSkill skill);

/**
 * @brief The cast animation the three are drawn with - Blessed Hammer fire, Blessed Shield magic, Fist
 * of the Heavens lightning (user, 2026-09-11). nullopt for every other spell, which keeps its element's.
 */
std::optional<MagicType> PaladinCastAnimation(SpellID spell);

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

/**
 * @brief A player's Blessed Shield launches with its own cue INSTEAD of the generic cast sound its
 * missile row borrows (IS_CAST2). False - so the row's sound plays - for anything else.
 */
bool PlayPaladinMissileSound(const Missile &missile);

/** @brief Blessed Shield's burst, where the thrown shield lands. Nothing sounded there before. */
void PlayBlessedShieldImpactSound(const Missile &missile);

/** @brief Fist of the Heavens', Blessed Shield's and Blessed Hammer's facts, one per line. For the tooltip. */
std::string PaladinRangedFactsAt(PaladinSkill skill, int rank);

} // namespace oracool
} // namespace devilution
