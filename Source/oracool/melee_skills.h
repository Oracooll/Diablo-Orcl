/**
 * @file oracool/melee_skills.h
 *
 * Oracool, Round 4 of the inert-skill plan (2026-09-03): the melee skills - the Barbarian's nine
 * and the Monk's eight - as one mechanism.
 *
 * THE MECHANISM is oracool/paladin_melee.h's, generalised. A readied melee skill is SWUNG: the click
 * becomes the ordinary attack with a latch naming the skill, PlrHitMonst asks the latch for a damage
 * multiplier, and DoAttack asks it once per swing for whatever else the skill does - a second blow,
 * a stagger, a shove, a strike at everything around. The Paladin's own three keep their own latch
 * and their own hook; they were written first and they work, and folding them in would be a rewrite
 * for no row.
 *
 * Every skill here is data on top of that: a damage percentage, an extra-strike count and share, and
 * one of a few reactions. Which is what makes seventeen rows cost about as much as the Paladin's
 * three did.
 *
 *   Bash              +30% (+10/rank), knocks back
 *   Stun              staggers for a second and a half (+0.2s/rank); uniques exempt
 *   Double Swing      a second blow at 75% (+5/rank)
 *   Concentrate       +50% (+10/rank)
 *   Frenzy            a second blow at 100%, both +0/+10/rank
 *   Whirlwind         every swing also strikes all eight neighbours at 66% (+5/rank)
 *   Berserk           +100% (+20/rank)
 *   Leap              vault to the cursor, four tiles (+1 every three ranks)
 *   Leap Attack       adjacent: +50% (+10/rank); otherwise the leap
 *   Sweeping Reed     also strikes the two tiles beside the target, at 100%
 *   Breaking Current  +33%, staggers for a second; uniques exempt
 *   Vaulting Strike   Leap Attack, for the Monk
 *   Wheel of Heaven   Whirlwind, for the Monk
 *   Seven Reeds       three blows (+1 every three ranks, seven at most) at 60%
 *   Open Palm         +20% (+10/rank), knocks back
 *   Hundred Fists     four blows (+1 every two ranks, seven at most) at 50%
 *   Radiant Palm      +20% (+10/rank); a kill erupts, dealing the blow again to every neighbour
 *
 * Deliberately NOT built, and the rows say so: Concentrate's uninterruptibility, Berserk's defence
 * penalty, Frenzy's speed, Whirlwind's travel. Each is a second mechanism serving one row.
 */
#pragma once

#include <optional>

#include "engine/point.hpp"
#include "spelldat.h"

namespace devilution {
struct Player;
struct Monster;
} // namespace devilution

namespace devilution::oracool {

enum class ClassMeleeSkill : uint8_t {
	Bash,
	Leap,
	DoubleSwing,
	Stun,
	LeapAttack,
	Concentrate,
	Frenzy,
	Whirlwind,
	Berserk,
	SweepingReed,
	BreakingCurrent,
	VaultingStrike,
	WheelOfHeaven,
	SevenReeds,
	OpenPalm,
	HundredFists,
	RadiantPalm,
	// Round 7: the Rogue's thrusts. No javelin or spear exists here, so they are what a thrust IS -
	// a swing with a rule on it - and Lightning Bolt and Lightning Fury, the thrown ones, are spells.
	Jab,
	PowerStrike,
	Impale,
	ChargedStrike,
	Fend,
	LightningStrike,
	// Round 8: the Paladin's Sacrifice - a heavy blow that wounds the striker.
	Sacrifice,
};

/** @brief The melee skill @p spell is, if it is one. */
std::optional<ClassMeleeSkill> ClassMeleeSkillForSpell(SpellID spell);

/** @brief The spell @p skill is readied and priced as. */
SpellID ClassMeleeSkillSpell(ClassMeleeSkill skill);

/** @brief Whether @p skill moves the character rather than (or before) striking. */
bool IsLeapSkill(ClassMeleeSkill skill);

/** @brief How far @p player's leap reaches at the skill's rank, in tiles. */
int LeapRangeTiles(const Player &player, ClassMeleeSkill skill);

/** @brief Records which melee skill the swing now being launched was thrown with. Same rules as ArmMeleeSkill. */
void ArmClassMeleeSkill(std::optional<ClassMeleeSkill> skill);

/** @brief The melee skill the swing being resolved was thrown with, if any. */
std::optional<ClassMeleeSkill> ArmedClassMeleeSkill();

/**
 * @brief The armed skill's damage bonus on the swing being resolved, in percent. Zero when nothing
 * is armed, the swing is not the local player's, or the skill cannot be paid for.
 *
 * Asked by PlrHitMonst for every blow, including the extra blows a skill adds - so a Double Swing's
 * second blow carries Double Swing's bonus, which is the arithmetic the rows describe.
 */
int ClassMeleeSkillDamagePercent(const Player &player);

/**
 * @brief Everything the armed skill does beyond the swing's own blow. Called once per swing by
 * DoAttack at the hit frame, after the front target has been resolved.
 *
 * @p front is the monster in front of the player, or null; @p frontHit whether the swing landed on
 * it; @p frontDamage what it dealt. Returns true if the skill struck anything, so the caller can
 * count the swing as a hit for weapon wear.
 *
 * Mana is charged here, once per swing, and only when the skill did something - the rule every
 * skill in this fork follows.
 */
bool ApplyClassMeleeSkillOnSwing(Player &player, Monster *front, bool frontHit, int frontDamage);

/**
 * @brief The leap: moves @p player toward @p target, up to the skill's range, through the engine's
 * own teleport. Charges the mana. False if there was nowhere to land.
 */
bool LeapToward(Player &player, ClassMeleeSkill skill, Point target);

/** @brief One sentence for the Abilities window, untranslated. "" for a spell that is not a melee skill. */
const char *ClassMeleeSkillDescription(SpellID spell);

/** @brief What @p skill does at @p rank, one fact per line: damage bonus, strikes, stun, range, sweep. For the tooltip. */
std::string MeleeSkillFactsAt(ClassMeleeSkill skill, int rank);

} // namespace devilution::oracool
