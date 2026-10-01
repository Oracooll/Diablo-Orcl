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

#include <optional>
#include <utility>

#include "engine/point.hpp"
#include "misdat.h" // DamageType
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
 * @brief The cast animation the three are drawn with - Blessed Hammer and Blessed Shield magic, Fist of
 * the Heavens lightning (user, 2026-09-11; the Hammer was fire until its damage became magic the same
 * day). nullopt for every other spell, which keeps its element's.
 */
std::optional<MagicType> PaladinCastAnimation(SpellID spell);

/**
 * @brief The per-hit damage @p skill does for @p player, at both ends of the weapon roll - what the cast
 * itself rolls (the weapon, its bonuses, then the skill's percentage), for the character sheet
 * (2026-09-11). Fist of the Heavens answers with its lightning ring (round 55 audit). nullopt for any other skill.
 */
std::optional<std::pair<int, int>> PaladinCastDamageRange(const Player &player, PaladinSkill skill);

/** @brief The damage type @p skill deals - that of the missile doing its damage. nullopt for any other skill. */
std::optional<DamageType> PaladinCastDamageType(PaladinSkill skill);

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

// Their damage at a skill level, as a percentage of weapon damage (user, 2026-09-12: "i approve all
// sugestions on the paladin skills") - they were fixed before, and a point bought nothing.
/** @brief Fist of the Heavens' blast on the target's own tile: 150%, +10 points a level. */
constexpr int FistCentrePercentAt(int rank)
{
	return 150 + 10 * ((rank < 1 ? 1 : rank) - 1);
}
/** @brief Each spark of its ring: 60%, +4 points a level - the ring keeps its 150 : 60 share of the fist. */
constexpr int FistNovaPercentAt(int rank)
{
	return 60 + 4 * ((rank < 1 ? 1 : rank) - 1);
}
/** @brief Blessed Shield's first strike: 125%, +8 points a level. The bounces carry 75% and 50% of it. */
constexpr int BlessedShieldPercentAt(int rank)
{
	return 125 + 8 * ((rank < 1 ? 1 : rank) - 1);
}
/** @brief Blessed Hammer on each tile it crosses: 60%, +6 points a level. */
constexpr int BlessedHammerPercentAt(int rank)
{
	return 60 + 6 * ((rank < 1 ? 1 : rank) - 1);
}

/** @brief Fist of the Heavens', Blessed Shield's and Blessed Hammer's facts, one per line. For the tooltip. */
std::string PaladinRangedFactsAt(PaladinSkill skill, int rank);

} // namespace oracool
} // namespace devilution
