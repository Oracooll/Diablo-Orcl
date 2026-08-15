#include "oracool/paladin_melee.h"

#include "items.h"
#include "monster.h"
#include "oracool/oracool.h"
#include "player.h"

namespace devilution::oracool {

namespace {

std::optional<PaladinSkill> ArmedSkill;

/**
 * @brief Most enemies Zeal's swing carries to, beside the one actually struck.
 *
 * FIVE, from the skill's own description - "Hits up to five adjacent enemies in a rapid succession".
 * One tile of reach is eight surrounding squares, so without this a swing in a crowd could carry to
 * eight and the description would be a lie.
 */
constexpr int MaxZealTargets = 5;

/**
 * @brief What Hammer of Faith's splash does, as a percentage of the blow that landed.
 *
 * Half. It is ONE hammer blow whose force carries, not a second attack on each neighbour - full
 * damage all round would make it strictly better than Zeal at every count of enemies, and the two
 * are meant to be different answers rather than a worse one and a better one.
 */
constexpr int HammerOfFaithSplashPercent = 50;

/**
 * @brief How long Shield Bash holds a monster, in game ticks.
 *
 * The clock runs at 20 ticks a second, so this is about a second and a quarter - long enough to step
 * away or line up the next blow, short enough that it is not a substitute for killing the thing.
 * Shield Bash adds no damage of its own; the stun IS the skill, which is also why it does not scale.
 */
constexpr int ShieldBashStunTicks = 25;

/** @brief Applies @p damage to @p monster, killing it or staggering it as the total decides. */
void StrikeMonster(Player &player, Monster &monster, int damage)
{
	ApplyMonsterDamage(DamageType::Physical, monster, damage);
	if ((monster.hitPoints >> 6) <= 0)
		M_StartKill(monster, player);
	else
		M_StartHit(monster, player, damage);
}

/**
 * @brief Collects up to @p maxTargets monsters within one tile of @p centre, excluding @p centre.
 *
 * Gathered BEFORE anything is applied, for two reasons that have both bitten this code: the mana is
 * charged only if the swing actually carries to someone, so the count has to be known first; and
 * killing a monster mid-scan reorders ActiveMonsters underneath the loop.
 */
int GatherAdjacent(const Monster &centre, Monster **out, int maxTargets)
{
	int found = 0;
	for (size_t i = 0; i < ActiveMonsterCount && found < maxTargets; i++) {
		Monster &other = Monsters[ActiveMonsters[i]];
		if (&other == &centre || !other.isPossibleToHit())
			continue;
		if (other.position.tile.WalkingDistance(centre.position.tile) != 1)
			continue;
		out[found++] = &other;
	}
	return found;
}

/** @brief Zeal - the swing carries to up to five neighbours at full force, one after another. */
void ApplyZeal(Player &player, Monster &primaryTarget, int hitDamage)
{
	Monster *targets[MaxZealTargets] = {};
	const int found = GatherAdjacent(primaryTarget, targets, MaxZealTargets);
	if (found == 0)
		return;
	if (!SpendPaladinSkillMana(player, PaladinSkill::Zeal))
		return; // ran dry between the caller's check and here

	for (int i = 0; i < found; i++)
		StrikeMonster(player, *targets[i], hitDamage);
}

/**
 * @brief Hammer of Faith - one blow, and everything around the target takes half of it.
 *
 * The difference from Zeal is the SHAPE, not the machinery: no cap on how many neighbours are
 * caught, because a hammer's impact does not count heads, and half damage rather than full, because
 * they are catching the shockwave rather than the hammer.
 */
void ApplyHammerOfFaith(Player &player, Monster &primaryTarget, int hitDamage)
{
	const int splashDamage = hitDamage * HammerOfFaithSplashPercent / 100;
	if (splashDamage <= 0)
		return;

	// Eight is every square touching the target - the whole ring, since this one does not cap.
	Monster *targets[8] = {};
	const int found = GatherAdjacent(primaryTarget, targets, 8);
	if (found == 0)
		return;
	if (!SpendPaladinSkillMana(player, PaladinSkill::HammerOfFaith))
		return;

	for (int i = 0; i < found; i++)
		StrikeMonster(player, *targets[i], splashDamage);
}

/** @brief Shield Bash - no extra damage, but the target loses its next second and a bit. */
void ApplyShieldBash(Player &player, Monster &primaryTarget)
{
	if (!HasShieldEquipped(player))
		return;
	// Nothing to stun on a corpse, and charging for it would break the rule that mana follows effect.
	if ((primaryTarget.hitPoints >> 6) <= 0)
		return;
	if (!SpendPaladinSkillMana(player, PaladinSkill::ShieldBash))
		return;

	StunMonster(primaryTarget, ShieldBashStunTicks);
}

} // namespace

void ArmMeleeSkill(std::optional<PaladinSkill> skill)
{
	ArmedSkill = skill;
}

std::optional<PaladinSkill> ArmedMeleeSkill()
{
	return ArmedSkill;
}

bool HasShieldEquipped(const Player &player)
{
	return player.InvBody[INVLOC_HAND_RIGHT]._itype == ItemType::Shield;
}

void ApplyMeleeSkillOnHit(Player &player, Monster &primaryTarget, int hitDamage)
{
	if (!ArmedSkill.has_value() || hitDamage <= 0)
		return;
	// The mana and level gates in one call, so a Paladin who cannot pay simply swings normally -
	// which is the same "still does something" rule the rest of these skills follow.
	if (!CanUsePaladinSkill(player, *ArmedSkill))
		return;

	switch (*ArmedSkill) {
	case PaladinSkill::Zeal:
		ApplyZeal(player, primaryTarget, hitDamage);
		break;
	case PaladinSkill::HammerOfFaith:
		ApplyHammerOfFaith(player, primaryTarget, hitDamage);
		break;
	case PaladinSkill::ShieldBash:
		ApplyShieldBash(player, primaryTarget);
		break;
	// Charge is intercepted before the swing (oracool/furious_charge.cpp); the three that throw
	// something are missiles and never reach a melee hook.
	case PaladinSkill::Charge:
	case PaladinSkill::BlessedShield:
	case PaladinSkill::FistOfTheHeavens:
	case PaladinSkill::BlessedHammer:
		break;
	}
}

} // namespace devilution::oracool
