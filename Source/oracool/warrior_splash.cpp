#include "oracool/warrior_splash.h"

#include "options.h"
#include "oracool/oracool.h"
#include "oracool/paladin_skills.h"

namespace devilution::oracool {

// Oracool: user decision (2026-08-11) - splash damage left the settings list to become an
// acquirable skill rather than an INI toggle. User request (2026-08-15) reopens it as ZEAL, with a
// level 6 gate and 2 mana a hit; both numbers live in oracool/paladin_skills.h so the Abilities
// window's row and the code that charges for it read the same table. The range a learned rank grants
// stays here, because it is a property of this mechanic rather than of the skill's price.
constexpr int LearnedSplashRange = 1;

/**
 * @brief Most enemies one swing carries to, beside the one actually struck.
 *
 * FIVE, from the skill's own description - "Hits up to five adjacent enemies in a rapid succession".
 * Range 1 is eight surrounding tiles, so without this a swing in a crowd could carry to eight and
 * the description would be a lie. Rings are collected nearest-first below so that if the range ever
 * grows, the cap spends itself on the closest enemies - the ones taking full damage.
 */
constexpr int MaxZealTargets = 5;

bool IsWarriorSplashDamageEnabled(const Player &player)
{
	// Includes the mana check, so a Paladin below 2 mana simply swings normally.
	return CanUsePaladinSkill(player, PaladinSkill::Zeal);
}

void ApplyWarriorSplashDamage(Player &player, Monster &primaryTarget, int primaryDamage)
{
	const int range = LearnedSplashRange;
	if (range <= 0 || primaryDamage <= 0)
		return;

	// Gathered before anything is applied, for two reasons: the mana is charged only if the swing
	// actually carries to someone (a lone enemy costs nothing), and killing a monster mid-scan can
	// reorder ActiveMonsters underneath the loop.
	Monster *targets[MaxZealTargets] = {};
	int damage[MaxZealTargets] = {};
	int found = 0;

	for (int ring = 1; ring <= range && found < MaxZealTargets; ring++) {
		// Each ring is a flat percentage of the primary hit's own damage, not a re-roll -
		// ring 1 (adjacent) is full damage, ring 2 half, ring 3 a quarter.
		int splashDamage = primaryDamage;
		if (ring == 2)
			splashDamage = primaryDamage / 2;
		else if (ring == 3)
			splashDamage = primaryDamage / 4;
		if (splashDamage <= 0)
			continue;

		for (size_t i = 0; i < ActiveMonsterCount && found < MaxZealTargets; i++) {
			Monster &other = Monsters[ActiveMonsters[i]];
			if (&other == &primaryTarget || !other.isPossibleToHit())
				continue;
			if (other.position.tile.WalkingDistance(primaryTarget.position.tile) != ring)
				continue;

			targets[found] = &other;
			damage[found] = splashDamage;
			found++;
		}
	}

	if (found == 0)
		return;
	if (!SpendPaladinSkillMana(player, PaladinSkill::Zeal))
		return; // ran dry between the caller's check and here

	for (int i = 0; i < found; i++) {
		Monster &other = *targets[i];
		ApplyMonsterDamage(DamageType::Physical, other, damage[i]);
		if ((other.hitPoints >> 6) <= 0)
			M_StartKill(other, player);
		else
			M_StartHit(other, player, damage[i]);
	}
}

} // namespace devilution::oracool
