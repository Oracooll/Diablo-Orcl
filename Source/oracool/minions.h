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
#include "misdat.h"
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

/** @brief The golems, each a material and a habit (oracool/necro_summoning.cpp gives the numbers). */
enum class GolemKind : uint8_t {
	None,
	Clay,  // its blows slow
	Blood, // what it takes heals it and its owner
	Iron,  // returns a share of what it takes
	Fire,  // burns what stands beside it; fire heals it
};

/** @brief What a summoning skill asks for. Life in whole points; the rest as a monster carries them. */
struct MinionSpec {
	MinionGroup group;
	_monster_id type;
	int life;
	int minDamage;
	int maxDamage;
	int toHit;
	int armorClass;
	/** Null: it fights with its blade. Anything else: it shoots this from range. */
	MissileID missile = MissileID::Null;
	GolemKind golem = GolemKind::None;
	/** 0 lasts; otherwise the body falls apart when the ticks run out (the Revived). */
	int ticksLeft = 0;
	/** 0: its own colours; otherwise every colour to this palette ramp (RampTranslation) - the golems' materials. */
	uint8_t ramp = 0;
};

/**
 * @brief Raises one minion for @p owner on a free tile near @p near. False if the group or the pool is full, the
 * level has no monsters (town), the sprites cannot be loaded, or there is no room to stand.
 */
bool SummonMinion(Player &owner, const MinionSpec &spec, Point near);
/** @brief Whether a record is free for one more minion - asked before a summon spends something it cannot give back. */
bool MinionRecordFree();
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

// ---- what the skills and the engine ask of the army ----------------------------------------------------------

/** @brief Every waiting or straying minion of @p owner is placed beside him now. How many moved. */
int GatherMinions(Player &owner);
/** @brief Every minion of @p owner within @p radius of him regains @p percent of its life. How many were healed. */
int HealMinions(Player &owner, int radius, int percent);
/** @brief For @p ticks every minion of @p owner strikes @p percent harder and hurries after its prey. */
void FrenzyMinions(Player &owner, int ticks, int percent);
/** @brief Unmakes the minion of @p owner nearest @p tile. Its full life in 1/64 points, or 0 if there was none. */
int SacrificeMinion(Player &owner, Point tile);

// ---- the army's numbers, one place each: the rules below and the tooltips (necro_summoning.cpp) both read them ----

/** Gather the Dead: a minion farther than this from its owner is called; it lands within GatherPlaceRadius. */
constexpr int GatherLeaveRadius = 2;
constexpr int GatherPlaceRadius = 4;
/** The Clay Golem's landed blow chills for this many ticks (2 s). */
constexpr int ClayGolemChillTicks = 40;
/** The Blood Golem: 1/N of the damage it deals heals itself, and 1/N heals its owner. */
constexpr int BloodGolemShareDivisor = 4;
/** The Iron Golem returns 1/N of every blow it takes. */
constexpr int IronGolemReturnDivisor = 3;
/** The Fire Golem: once every FireGolemPulseTicks, 1/N of a blow as fire to everything beside it; fire heals it 1/N. */
constexpr int FireGolemPulseTicks = 20;
constexpr int FireGolemBurnDivisor = 2;
constexpr int FireGolemFireHealDivisor = 2;
/** Grisly Tribute (N8): 1/N of every minion blow heals the owner. */
constexpr int GrislyTributeDivisor = 10;
/** Aberrant Animator (N8): any minion returns 1/N of a blow it takes. */
constexpr int AberrantAnimatorDivisor = 5;

/** @brief Summon Resist at @p points: the share of fire, lightning and magic damage a minion shrugs off, in percent (0 at 0 points). */
int SummonResistPercent(int points);

/** @brief A blow landing on a minion (monster.cpp ApplyMonsterDamage): Summon Resist, and fire feeding a Fire Golem. */
int MinionDamageTaken(const Monster &monster, DamageType type, int damage);
/** @brief What a minion's blows are multiplied by right now - Frenzy of the Dead. 100 for the rest. */
int MinionDamagePercent(const Monster &monster);
/** @brief A minion's blow struck @p target for @p damage (monster.cpp MonsterAttackMonster): the golems' habits. */
void OnMinionBlow(Monster &minion, Monster &target, int damage);
/** @brief A blow struck a minion (monster.cpp MonsterAttackMonster): the Iron Golem gives a share back. */
void OnMinionStruck(Monster &minion, Monster &attacker, int damage);

/** @brief The army panel, under the companions' (or in its place). */
void DrawMinionHud(const Surface &out);
/** @brief A click on the panel's header cycles the shared stance. */
bool HandleMinionHudClick(Point mouse);

} // namespace oracool
} // namespace devilution
