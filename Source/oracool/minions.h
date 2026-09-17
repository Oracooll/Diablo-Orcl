#pragma once
/**
 * @file oracool/minions.h
 *
 * The army (Plan - The Necromancer, phase N4; user, D4: "increase this 4 number to a lot more - as many as in D2").
 *
 * A MINION is an ordinary monster in an ordinary `Monsters[]` slot that fights for a hero. The engine already had
 * most of what that needs: `Monster::isPlayerMinion()` is a FLAG test (MFLAG_GOLEM without MFLAG_BERSERK), every
 * damage path and the cursor already spare such a monster, enemies already turn on one that stands beside them,
 * and the Companion brain (monster.cpp `CompanionAi`) already fights from a set of orders rather than from a slot.
 * What was tied to the four golem slots was the BOOKKEEPING - who owns a body, where it stands, what happens to it
 * on the stairs - and that is what lives here, for up to MaxMinionBodies (monster.h) bodies.
 *
 * Differences from a Companion (oracool/companion.h), all deliberate:
 *  - a minion wears a MONSTER's sprites and swings a monster's blow (its own min/max damage and to-hit), where a
 *    companion wears hero sheets and deals its owner's blow;
 *  - a minion is not timed. It lasts until it is killed or dismissed;
 *  - minions come in GROUPS with a count, not as named individuals. The panel says "Skeletons 6", and a stance is
 *    shared with the companions' (the J key).
 *
 * The bodies belong to the level; the RECORDS do not. Leaving a level keeps each record and its body's remaining
 * life, and the army re-forms around its owner on the next floor that has monsters at all (not town). Nothing is
 * saved: a game always starts without an army.
 *
 * The pool is ABOVE the enemies' 200 (MaxEnemyMonsters), so an army never costs a floor its monsters.
 */

#include <cstddef>
#include <cstdint>

#include "engine/point.hpp"
#include "engine/surface.hpp"
#include "monstdat.h"

namespace devilution {

struct Monster;
struct Player;

namespace oracool {

struct CompanionOrders;

/** @brief The kinds of body an army is counted in. Decision D7: 8 Skeletons, 8 Mages, 1 Golem, 10 Revived. */
enum class MinionGroup : uint8_t {
	Skeleton,
	Mage,
	Golem,
	Revived,
};
constexpr size_t MinionGroupCount = 4;

/** @brief The most bodies of @p group one hero may keep - the ceiling a skill's rank climbs toward. */
int MinionGroupCap(MinionGroup group);
const char *MinionGroupName(MinionGroup group);

/** @brief What a summoning skill asks for. Life in whole points; the rest as a monster carries them. */
struct MinionSpec {
	MinionGroup group;
	_monster_id type;
	int life;
	int minDamage;
	int maxDamage;
	int toHit;
	int armorClass;
};

/**
 * @brief Raises one minion for @p owner on a free tile near @p near. False if the group or the pool is full, the
 * level has no monsters (town), the sprites cannot be loaded, or there is no room to stand.
 */
bool SummonMinion(Player &owner, const MinionSpec &spec, Point near);
/** @brief Dismisses every minion of @p owner, or only those of one group. Bodies on this level die where they stand. */
void DismissMinions(Player &owner);
void DismissMinions(Player &owner, MinionGroup group);

/** @brief Living minions of @p owner in @p group, on this level or waiting to re-form. */
int MinionCount(const Player &owner, MinionGroup group);
int MinionCount(const Player &owner);
/** @brief Monster slots minion bodies hold right now, the dying included - what AddMonster leaves out of the enemies' count. */
size_t ActiveMinionBodies();

bool IsMinion(const Monster &monster);
/** @brief Whose it is, or null - so its kills are its owner's. */
const Player *MinionOwner(const Monster &monster);

/** @brief Its orders for this tick, in the Companion brain's terms. False if @p monster is not a living minion. */
bool GetMinionOrders(const Monster &monster, CompanionOrders &orders);
/** @brief Whether its owner may walk through it (it takes the tile the hero leaves), as a companion does. */
bool MinionMakesWay(const Player &player, const Monster &monster);
/**
 * @brief The thinking budget: an idle minion looks for something to do on one tick in three, its own third, so an
 * army of thirty costs what ten cost. A walking or fighting minion is never held up - its steps and blows are the
 * engine's, tick by tick.
 */
bool MinionThinksThisTick(const Monster &monster, uint32_t tick);

/** @brief A new game: no army. The records are statics and would follow one hero into the next. */
void ForgetMinions();
/** @brief The level is being torn down: every body is gone, every record keeps its life and waits. */
void OnMinionLevelLoad();
/**
 * @brief The level is about to be STORED (loadsave.cpp SaveLevel): every minion body is taken off it now, the living
 * keeping their life in their records. Must run before the monsters are written.
 */
void WithdrawMinionsForLevelSave();
/** @brief A monster slot is being handed back (monster.cpp DeleteMonster): if a minion lived there, it is no more. */
void OnMonsterSlotFreed(size_t monsterId);
/** @brief Once a tick for @p owner: re-forms the waiting part of the army around him, a few bodies a tick. */
void ProcessMinions(Player &owner);

/** @brief The army panel, under the companions' (or in its place). */
void DrawMinionHud(const Surface &out);
/** @brief A click on the panel's header cycles the shared stance. */
bool HandleMinionHudClick(Point mouse);

} // namespace oracool
} // namespace devilution
