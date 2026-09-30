#pragma once
/**
 * @file oracool/corpses.h
 *
 * Corpses a skill can use (Plan - The Necromancer, section 5.3; phase N5).
 *
 * The engine's `dCorpse` knows where a body lies and which sprite to draw, not what it was. Raise Skeleton, Raise
 * Skeletal Mage and Revive need the rest: the type, so Revive can bring it back as it was; its numbers, so the
 * Revived hit like the living did. So every death of an ENEMY on the floor is written here as well - at the moment
 * its corpse sprite is laid down - into a table of at most CorpseTableSize per level (the user's "pool of 100").
 * Taking one removes the sprite too: a raised corpse is gone from the floor.
 *
 * Per level and never saved. A floor revisited has its corpse sprites but no table; the dead there are not usable,
 * which is also what keeps a stale table from raising a body on the wrong floor.
 */

#include <cstdint>
#include <optional>

#include "engine/point.hpp"
#include "monstdat.h"

namespace devilution {

struct Monster;

namespace oracool {

constexpr int CorpseTableSize = 100;

struct Corpse {
	Point position;
	_monster_id type;
	/** The dead one's own numbers - what a Revived gets back. Life in whole points. */
	int level;
	int maxLife;
	int minDamage;
	int maxDamage;
	int toHit;
	int armorClass;
	/** A unique, a boss, the Golem, a hidden type: usable for a skeleton, never for Revive. */
	bool revivable;
};

/** @brief Writes @p monster into the table as it dies. Minions and the already-full table are not written. */
void RecordCorpse(const Monster &monster);
/** @brief The nearest usable corpse within @p radius of @p tile, TAKEN: gone from the table and from the floor. */
std::optional<Corpse> TakeCorpseNear(Point tile, int radius, bool forRevive);
/** @brief As TakeCorpseNear, only a corpse in a clear line from @p seenFrom (round 38: the explosions). */
std::optional<Corpse> TakeCorpseNearSeen(Point tile, int radius, Point seenFrom, bool forRevive = false);
/** @brief Whether a corpse in a clear line from @p seenFrom lies within @p radius of @p tile (round 41: Raise and Revive). */
bool CorpseNearSeen(Point tile, int radius, Point seenFrom, bool forRevive);
/** @brief Whether one is there, without taking it - for the cursor and the cast's fizzle. */
bool CorpseNear(Point tile, int radius, bool forRevive);
int CorpseCount();
/** @brief A level is being torn down or built: nothing on the last floor is usable on this one. */
void ClearCorpses();

} // namespace oracool
} // namespace devilution
