#include "oracool/paladin_ranged.h"

#include "engine/random.hpp"
#include "missiles.h"
#include "monster.h"
#include "oracool/paladin_melee.h" // HasShieldEquipped
#include "player.h"

namespace devilution::oracool {

namespace {

/**
 * @brief One roll of the character's own weapon damage, in whole hit points.
 *
 * Oracool: user decision (2026-08-15) - these skills scale off the WEAPON, not off character level,
 * so gear matters and they keep a melee feel even when they strike at a distance.
 *
 * Mirrors the first lines of the engine's own melee roll (player.cpp's PlrHitMonst): the weapon's
 * min..max, then the item's percentage bonus, its flat bonus, and the character's damage modifier.
 * Deliberately WITHOUT the two things that follow it there - the Warrior/Barbarian double-damage
 * roll and the weapon-type modifiers - because those describe a swing connecting, and none of these
 * skills is a swing.
 *
 * Whole hit points rather than the 1/64ths the player's own pools use, matching what every missile
 * puts in _midam; CheckMissileCol does the shift.
 */
int RollWeaponDamage(const Player &player)
{
	const int minDamage = player._pIMinDam;
	const int maxDamage = std::max(player._pIMaxDam, minDamage);
	int damage = GenerateRnd(maxDamage - minDamage + 1) + minDamage;
	damage += damage * player._pIBonusDam / 100;
	damage += player._pIBonusDamMod;
	damage += player._pDamageMod;
	return std::max(damage, 1);
}

/**
 * @brief Drops one explosion on @p tile carrying @p damage.
 *
 * MissileID::ApocalypseBoom is the engine's own one-tile blast: it plants itself where it is told,
 * runs its animation, and damages whatever is standing there exactly once. Apocalypse builds its
 * whole effect by scattering these over an area, so using them the same way is reuse of a pattern,
 * not a workaround - and it is what "reuse existing art now, swap later" (user, 2026-08-15) buys.
 */
void DropBlast(const Player &player, Point tile, int damage, int spellLevel)
{
	if (!InDungeonBounds(tile))
		return;
	AddMissile(tile, tile, Direction::South, MissileID::ApocalypseBoom, TARGET_MONSTERS,
	    player.getId(), damage, spellLevel);
}

/** @brief Fist of the Heavens' blast on the target's own tile, as a percentage of weapon damage. */
constexpr int FistCentrePercent = 150;
/** @brief And on each of the eight squares around it - it is the edge of the impact, not the fist. */
constexpr int FistSplashPercent = 75;

/** @brief Blessed Shield's damage to the enemy it is thrown at. */
constexpr int BlessedShieldPrimaryPercent = 125;
/** @brief And to each enemy it carries on to. */
constexpr int BlessedShieldCarryPercent = 100;
/** @brief How many further enemies one throw can reach. */
constexpr int BlessedShieldCarryTargets = 4;
/** @brief How far from the first enemy the shield will look for the rest of the crowd, in tiles. */
constexpr int BlessedShieldCarryRange = 3;

/** @brief A divine fist lands on the target and shakes the ground around it. */
bool CastFistOfTheHeavens(Player &player, Point target, int spellLevel)
{
	const int damage = RollWeaponDamage(player);
	if (!SpendPaladinSkillMana(player, PaladinSkill::FistOfTheHeavens))
		return false;

	DropBlast(player, target, damage * FistCentrePercent / 100, spellLevel);
	// The eight squares touching it. Drawn as separate blasts rather than as one big one because the
	// engine has no big one - and because each then damages its own tile, which is the behaviour
	// "splash damage on enemies nearby" describes.
	for (int dy = -1; dy <= 1; dy++) {
		for (int dx = -1; dx <= 1; dx++) {
			if (dx == 0 && dy == 0)
				continue;
			DropBlast(player, target + Displacement { dx, dy }, damage * FistSplashPercent / 100, spellLevel);
		}
	}
	return true;
}

/**
 * @brief A shield thrown into a crowd: the enemy aimed at, then the nearest few around it.
 *
 * The carry targets are chosen by distance from the FIRST enemy rather than from the player, which
 * is what makes it read as a shield bouncing through a knot of monsters instead of as a second area
 * blast centred on the caster.
 */
bool CastBlessedShield(Player &player, Point target, int spellLevel)
{
	// No shield check here any more: requiresShield is part of IsPaladinSkillUnlocked, which
	// CanUsePaladinSkill already asked before this ran, so a shieldless Paladin never gets here.
	Monster *primary = FindMonsterAtPosition(target);
	if (primary == nullptr)
		return false;

	// Gathered before anything is applied: killing a monster mid-scan reorders ActiveMonsters, and
	// the mana must not be charged until the throw is committed.
	Monster *carried[BlessedShieldCarryTargets] = {};
	int found = 0;
	for (size_t i = 0; i < ActiveMonsterCount && found < BlessedShieldCarryTargets; i++) {
		Monster &other = Monsters[ActiveMonsters[i]];
		if (&other == primary || !other.isPossibleToHit())
			continue;
		if (other.position.tile.WalkingDistance(primary->position.tile) > BlessedShieldCarryRange)
			continue;
		carried[found++] = &other;
	}

	const int damage = RollWeaponDamage(player);
	if (!SpendPaladinSkillMana(player, PaladinSkill::BlessedShield))
		return false;

	DropBlast(player, primary->position.tile, damage * BlessedShieldPrimaryPercent / 100, spellLevel);
	for (int i = 0; i < found; i++)
		DropBlast(player, carried[i]->position.tile, damage * BlessedShieldCarryPercent / 100, spellLevel);
	return true;
}

/** @brief Blessed Hammer's damage, as a percentage of weapon damage, on each tile it crosses. */
constexpr int BlessedHammerPercent = 60;

/**
 * @brief A hammer that winds outward from the caster's own feet.
 *
 * Launched FROM the player rather than at the target - "spirals outward of you" - so the click's job
 * is only to say that a target was in range, which the caller has already checked. The spiral itself
 * is ProcessBlessedHammer's; see the note there for why it is the one missile that does not travel
 * on a velocity vector.
 *
 * The per-tile damage is the lowest of the three because the hammer crosses many tiles in one cast,
 * and each crossing is a hit.
 */
bool CastBlessedHammer(Player &player, int spellLevel)
{
	const int damage = RollWeaponDamage(player) * BlessedHammerPercent / 100;
	if (!SpendPaladinSkillMana(player, PaladinSkill::BlessedHammer))
		return false;

	AddMissile(player.position.tile, player.position.tile, player._pdir, MissileID::BlessedHammer,
	    TARGET_MONSTERS, player.getId(), std::max(damage, 1), spellLevel);
	return true;
}

} // namespace

bool CastRangedPaladinSkill(Player &player, PaladinSkill skill, Point target)
{
	if (!CanUsePaladinSkill(player, skill))
		return false;

	const int spellLevel = player.GetSpellLevel(GetPaladinSkillData(skill).spellId);
	switch (skill) {
	case PaladinSkill::FistOfTheHeavens:
		return CastFistOfTheHeavens(player, target, spellLevel);
	case PaladinSkill::BlessedShield:
		return CastBlessedShield(player, target, spellLevel);
	case PaladinSkill::BlessedHammer:
		return CastBlessedHammer(player, spellLevel);
	// The melee three and Charge never reach here.
	case PaladinSkill::Charge:
	case PaladinSkill::Zeal:
	case PaladinSkill::HammerOfFaith:
	case PaladinSkill::ShieldBash:
		break;
	}
	return false;
}

} // namespace devilution::oracool
