#include "oracool/warrior_splash.h"

#include "options.h"
#include "oracool/oracool.h"

namespace devilution::oracool {

// Oracool: user decision (2026-08-11) - splash damage left the settings list for the same reason
// Furious Charge did: it becomes an acquirable skill earned through level progression once the
// Skills system exists, not something toggled in an INI. The mechanics below are untouched and
// ready; only the gate is closed, and the range that a learned rank would grant is kept here.
constexpr int LearnedSplashRange = 1;

bool IsWarriorSplashDamageEnabled(const Player & /*player*/)
{
	return false;
}

void ApplyWarriorSplashDamage(Player &player, Monster &primaryTarget, int primaryDamage)
{
	const int range = LearnedSplashRange;
	if (range <= 0 || primaryDamage <= 0)
		return;

	for (size_t i = 0; i < ActiveMonsterCount; i++) {
		Monster &other = Monsters[ActiveMonsters[i]];
		if (&other == &primaryTarget || !other.isPossibleToHit())
			continue;

		const int dist = other.position.tile.WalkingDistance(primaryTarget.position.tile);
		if (dist < 1 || dist > range)
			continue;

		// Each ring is a flat percentage of the primary hit's own damage, not a re-roll -
		// ring 1 (adjacent) is full damage, ring 2 half, ring 3 a quarter.
		int splashDamage = primaryDamage;
		if (dist == 2)
			splashDamage = primaryDamage / 2;
		else if (dist == 3)
			splashDamage = primaryDamage / 4;
		if (splashDamage <= 0)
			continue;

		ApplyMonsterDamage(DamageType::Physical, other, splashDamage);
		if ((other.hitPoints >> 6) <= 0)
			M_StartKill(other, player);
		else
			M_StartHit(other, player, splashDamage);
	}
}

} // namespace devilution::oracool
