#include "oracool/aura_field.h"

#include <algorithm>

#include "engine/points_in_rectangle_range.hpp"
#include "levels/gendung.h"
#include "monster.h"
#include "oracool/class_tree.h"
#include "oracool/monster_difficulty.h"
#include "player.h"

namespace devilution::oracool {

namespace {

using Skill = ClassTreeSkill;

/** @brief How far Sanctuary drives an undead when it repels it. Matches the Fallen's own flight. */
constexpr int RepelDistance = 4;

/**
 * @brief Points of Conviction before it starts breaking immunities rather than just resistances.
 *
 * Five of a possible twenty, so a player who merely dabbles neutralises resistant monsters, and one
 * who commits to it can finally hurt the immune ones. That threshold is the reason to keep pouring
 * points in past the radius growing.
 */
constexpr int ConvictionBreaksImmunityAt = 5;

/**
 * @brief The points in @p aura if the local player has it lit and usable, else 0.
 *
 * One place for the three conditions every outward aura shares, so a new consumer cannot forget
 * one: the player must exist and be on this level, the aura must be the lit one, and it must be
 * unlocked and paid for.
 */
int LitAuraPoints(Skill aura)
{
	if (MyPlayer == nullptr || !MyPlayer->isOnActiveLevel())
		return 0;
	const Player &player = *MyPlayer;
	if (GetActiveClassAura(player) != aura || !IsClassTreeSkillUnlocked(player, aura))
		return 0;
	return ClassTreeInvestment(player, aura);
}

/** @brief Whether @p monster is close enough to the local player for an aura of @p points. */
bool WithinAura(const Monster &monster, int points)
{
	return monster.position.tile.WalkingDistance(MyPlayer->position.tile) <= AuraRadiusForPoints(points);
}

} // namespace

int AuraRadiusForPoints(int points)
{
	if (points <= 0)
		return 0;
	// Four tiles at one point, one more per two points, capped at eight. Eight is about the point
	// where the field covers everything already on screen, past which positioning stops mattering.
	return std::min(4 + (points - 1) / 2, 8);
}

int ConvictionPointsOn(const Monster &monster)
{
	const int points = LitAuraPoints(Skill::Conviction);
	if (points <= 0)
		return 0;
	return WithinAura(monster, points) ? points : 0;
}

uint16_t ConvictionAdjusted(uint16_t resistances, int points)
{
	if (points <= 0)
		return resistances;
	// Strip the plain resistances FIRST, then step immunities down into plain resistances. Doing it
	// in this order is what keeps a deep Conviction from deleting an immunity outright: the newly
	// demoted bits arrive after the stripping has already happened, so the monster ends up
	// resistant rather than bare.
	uint16_t out = resistances & ~static_cast<uint16_t>(RESIST_MAGIC | RESIST_FIRE | RESIST_LIGHTNING);
	if (points >= ConvictionBreaksImmunityAt)
		out = DemoteImmunitiesToResistances(out);
	return out;
}

uint16_t EffectiveResistances(const Monster &monster)
{
	return ConvictionAdjusted(monster.resistance, ConvictionPointsOn(monster));
}

void ProcessOutwardAura(Player &player)
{
	if (&player != MyPlayer)
		return;

	const int sanctuary = LitAuraPoints(Skill::Sanctuary);
	if (sanctuary <= 0)
		return;

	// Repulsion has to PUSH, so unlike Conviction it cannot be a question asked at the point of
	// use - there is no such point. It rides MonsterGoal::Retreat, which is the same channel
	// M_FallenFear has always used, and which the monster AI clears by itself once the retreat
	// finishes. Re-set every tick while the undead is still in the field, so walking away ends it
	// without anything having to be undone.
	const int radius = AuraRadiusForPoints(sanctuary);
	const Rectangle field { player.position.tile, radius };
	for (const Point tile : PointsInRectangle(field)) {
		if (!InDungeonBounds(tile))
			continue;
		const int id = dMonster[tile.x][tile.y];
		if (id == 0)
			continue;
		Monster &monster = Monsters[std::abs(id) - 1];
		if ((monster.hitPoints >> 6) <= 0)
			continue;
		// Undead only - that is what a sanctuary is for, and it is the same test HolyBolt already
		// uses to decide what it may burn.
		if (monster.data().monsterClass != MonsterClass::Undead)
			continue;
		// A champion is frightened by nothing. Letting an aura walk a unique out of the room would
		// make the fight the player came for un-fightable.
		if (monster.isUnique())
			continue;
		monster.goal = MonsterGoal::Retreat;
		monster.goalVar1 = RepelDistance;
		monster.goalVar2 = static_cast<int>(GetDirection(player.position.tile, monster.position.tile));
	}
}

} // namespace devilution::oracool
