/**
 * @file oracool/corpses.cpp
 *
 * See corpses.h.
 */
#include "oracool/corpses.h"

#include <array>

#include "dead.h"
#include "multi.h"
#include "engine.h"
#include "levels/gendung.h"
#include "monster.h"
#include "oracool/minions.h"

namespace devilution::oracool {

namespace {

std::array<Corpse, CorpseTableSize> Table;
int Count = 0;

bool Revivable(const Monster &monster)
{
	if (monster.isUnique() || monster.type().type == MT_GOLEM)
		return false;
	// The story monsters and the ones the variant system already keeps out: what could not be spawned twice
	// should not be raised either.
	if (monster.data().availability == MonsterAvailability::Never)
		return false;
	return IsNoneOf(monster.type().type, MT_DIABLO, MT_NAKRUL, MT_DEFILER);
}

} // namespace

void RecordCorpse(const Monster &monster)
{
	if (Count >= CorpseTableSize || IsMinion(monster) || monster.isPlayerMinion())
		return;
	Corpse &corpse = Table[static_cast<size_t>(Count++)];
	corpse.position = monster.position.tile;
	corpse.type = monster.type().type;
	corpse.level = static_cast<int>(monster.level(sgGameInitInfo.nDifficulty));
	corpse.maxLife = std::max(monster.maxHitPoints >> 6, 1);
	corpse.minDamage = monster.minDamage;
	corpse.maxDamage = monster.maxDamage;
	corpse.toHit = static_cast<int>(monster.toHit(sgGameInitInfo.nDifficulty));
	corpse.armorClass = monster.armorClass;
	corpse.revivable = Revivable(monster);
}

namespace {

int NearestIndex(Point tile, int radius, bool forRevive)
{
	int best = -1;
	int bestDistance = 0;
	for (int i = 0; i < Count; i++) {
		const Corpse &corpse = Table[static_cast<size_t>(i)];
		if (forRevive && !corpse.revivable)
			continue;
		const int distance = tile.WalkingDistance(corpse.position);
		if (distance > radius)
			continue;
		if (best < 0 || distance < bestDistance) {
			best = i;
			bestDistance = distance;
		}
	}
	return best;
}

} // namespace

std::optional<Corpse> TakeCorpseNear(Point tile, int radius, bool forRevive)
{
	const int index = NearestIndex(tile, radius, forRevive);
	if (index < 0)
		return std::nullopt;
	const Corpse taken = Table[static_cast<size_t>(index)];
	Table[static_cast<size_t>(index)] = Table[static_cast<size_t>(--Count)];
	// The floor's sprite goes with it - unless another body has since been laid on the same tile, which keeps its own.
	if (InDungeonBounds(taken.position)) {
		bool another = false;
		for (int i = 0; i < Count; i++)
			another = another || Table[static_cast<size_t>(i)].position == taken.position;
		if (!another)
			dCorpse[taken.position.x][taken.position.y] = 0;
	}
	return taken;
}

bool CorpseNear(Point tile, int radius, bool forRevive)
{
	return NearestIndex(tile, radius, forRevive) >= 0;
}

int CorpseCount()
{
	return Count;
}

void ClearCorpses()
{
	Count = 0;
}

} // namespace devilution::oracool
