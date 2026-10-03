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
#include "engine/direction.hpp"
#include "engine/displacement.hpp"
#include "misdat.h"
#include "monstdat.h"
#include "monster.h" // AnimStruct, MonsterGraphic: the Fire Golem's sheet
#include "oracool/missile_tint.h"

namespace devilution {

struct Missile;
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
/** @brief Whether SummonMinion's search round @p near (SpawnBody's rings and sight rule) has a free tile (round 73 audit). */
bool MinionTileNear(Point near);
/** @brief Whether a record is free for one more minion - asked before a summon spends something it cannot give back. */
bool MinionRecordFree();
/** @brief Dismisses every minion of @p owner, or only those of one group. Bodies on this level die where they stand. */
void DismissMinions(Player &owner);
void DismissMinions(Player &owner, MinionGroup group);

/** @brief Living minions of @p owner in @p group, on this level or waiting to re-form. */
int MinionCount(const Player &owner, MinionGroup group);
int MinionCount(const Player &owner);
/** @brief @p owner's minions standing on this floor now - not the waiting records a town visit leaves (round 66 audit). */
int LivingMinionCount(const Player &owner);
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
/** @brief Dark Mending's glow on @p monster (v1.12.211): 1 as it is healed, fading to 0 over a second; 0 for any other. */
double MinionMendGlow(const Monster &monster);
/** @brief For @p ticks every minion of @p owner strikes @p percent harder and hurries after its prey. */
void FrenzyMinions(Player &owner, int ticks, int percent);
/** @brief How far from the cursor Unholy Offering reaches for a minion (round 48 audit). */
constexpr int UnholyOfferingReach = 3;

/**
 * @brief Unmakes the minion of @p owner nearest @p tile, within UnholyOfferingReach and in the owner's sight. Its current life in
 * 1/64 points, or 0 if there was none.
 */
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

// ---- how the army is drawn (the Necromancer Skill Cards page, 2026-10-03) --------------------------------------

/** @brief A body's colours on the 32-bit screen: a tint of missile_tint.h and its hue. Tint::None draws it as it is. */
struct MinionLook {
	Tint tint = Tint::None;
	uint32_t rgb = 0;
};

/**
 * @brief A golem's material over the vanilla golem's sprite: Clay in clay, Blood in blood red, Iron in iron with a
 * glint running over it at random (Tint::Sheen). The Fire Golem wears no tint: its body is the Elemental's run sheet,
 * fire-coloured as drawn (GetMinionAnim, v1.12.364).
 */
MinionLook GolemLook(GolemKind kind);
/** @brief The hue of a Skeletal Mage's element, by the missile it shoots: fire, lightning, poison, or bone (Arrow). */
uint32_t MageElementHue(MissileID missile);
/** @brief A Skeletal Mage's body: the regular skeleton in its element's hue, a subtle cycle of that hue on it (Tint::Shimmer). */
MinionLook MageLook(MissileID missile);
/** @brief What @p monster wears as drawn: its golem material or mage element; Tint::None for anything else. */
MinionLook MinionBodyLook(const Monster &monster);
/** @brief Whether @p monster is a living minion under Frenzy of the Dead now - it wears the frenzy ring (aura_ground.h). */
bool IsMinionFrenzied(const Monster &monster);

// ---- the Fire Golem: the Hellfire Elemental's body (the user's LUNGE pick, v1.12.364) ----------------------------
//
// "We must use Elemental sprite but we need to reduce its movement speed a bit and we need to come up with some
// attacking animation." The golem monster stays the logic carrier (its blow, its burn, fire healing it); every one of
// its six animations binds the Elemental's run sheet instead (missiles\firerun1..8: eight facings of twelve frames,
// 96 px wide) through GetMinionAnim, which monster_scale.cpp's GetScaledAnim asks beside the companions' sheets.

/** The run sheet's frames per facing (misdat.cpp, MissileGraphicID::Elemental, AnimLen_12). */
constexpr int FireGolemSheetFrames = 12;
/** Its walk: twelve frames at two ticks each, 24 ticks a tile against the vanilla golem's 16 frames at one - 2/3 the pace. */
constexpr int FireGolemWalkTicksPerFrame = 2;
/** Its attack: the run loop at one tick a frame, the vanilla golem's 12 ticks, so the blow lands on the tick it always did. */
constexpr int FireGolemAttackTicksPerFrame = 1;
/** How far the lunge carries the body toward its target at the peak, in tenths of a tile: half a tile. */
constexpr int FireGolemLungeTenths = 5;

/** @brief The frames and ticks per frame the Fire Golem's @p graphic binds - the run sheet, slowed for a walk or a stand. */
struct FireGolemTiming {
	int8_t frames;
	int8_t rate;
};
FireGolemTiming FireGolemAnimTiming(MonsterGraphic graphic);
/** @brief Whether @p monster is a Necromancer's Fire Golem (its record says so). */
bool IsFireGolem(const Monster &monster);
/**
 * @brief The animation @p monster binds for @p graphic in place of its type's, or nullptr: the Fire Golem's run sheet,
 * while the Elemental's art is loaded. Asked by GetScaledAnim, i.e. by both of the engine's sprite binders.
 */
const AnimStruct *GetMinionAnim(const Monster &monster, MonsterGraphic graphic);
/**
 * @brief The lunge's share of its full reach at @p progress (0..AnimationInfo::baseValueFraction through the attack):
 * sin(pi * p), easing out to 1 at the middle (the hit frame) and back to 0 at both ends - symmetric about the middle.
 */
double FireGolemLungeFraction(int progress);
/** @brief The lunge as a screen offset toward @p facing at @p progress: up to half a tile, 0 at the start and the end. */
Displacement FireGolemLungeOffset(Direction facing, int progress);
/** @brief Where @p monster is drawn off its tile beyond its walk: the Fire Golem's lunge during an attack, else nothing. */
Displacement MinionLungeOffset(const Monster &monster);
/** @brief Whether @p monster's body is not drawn: a Fire Golem dying is all explosion (FireGolemDeathArt), no body. */
bool MinionBodyHidden(const Monster &monster);
/** @brief Whether @p monster is drawn at full brightness, as the Elemental missile is: the Fire Golem burns in the dark. */
bool MinionDrawsUnlit(const Monster &monster);
/** @brief The splash at the Fire Golem's hit frame, on its target: hit_fire.png when delivered, the magma ball's burst if not. */
MissileGraphicID FireGolemStrikeArt();
/** @brief The flare the Fire Golem rises in: the Apocalypse boom (newexp), a fire burst about one tile across. */
constexpr MissileGraphicID FireGolemSummonArt = MissileGraphicID::ApocalypseBoom;
/** @brief The Fire Golem's end: the explosion the Elemental spell ends in (bigexp). */
constexpr MissileGraphicID FireGolemDeathArt = MissileGraphicID::BigExplosion;
/** @brief Plays @p art on @p tile for @p playerId as a still sheet (AddArtEffect); the Fire Golem's three bursts go through here. */
Missile *AddFireGolemBurst(Point tile, MissileGraphicID art, int playerId);
/** @brief A minion's attack reached its hit frame (monster.cpp MonsterAttack): the Fire Golem's splash bursts on the target. */
void OnMinionStrikeFrame(const Monster &minion);
/** @brief A minion began to die (monster.cpp MonsterDeath): the Fire Golem bursts in the Elemental's explosion. */
void OnMinionDeathStarted(const Monster &minion);

// ---- the golems' rise and stand (dev note, 2026-10-03: "we need to introduce the golem's buildup animation when summoned for
// all golems, and we also need to choose a proper frame to use as stand still frame when they are not moving") ----------

/**
 * The frame of the vanilla golem's 16-frame walk a standing golem holds: both feet planted side by side, arms down. The
 * golem has no stand sheet and stood on its walk, walking on the spot.
 */
constexpr int8_t GolemStandFrame = 4;
/** @brief Whether @p monster, a golem body, stands on GolemStandFrame - every golem but the Fire Golem, whose flames move. */
bool GolemHoldsStillFrame(const Monster &monster);
/**
 * @brief A monster finished its special stand (monster.cpp MonsterSpecialStand). A golem summoned by the Necromancer rises
 * out of the floor through the vanilla golem's build-up sheet (golems) - the Fire Golem too, molten orange, and it bursts
 * into its Elemental's flame here, at the end of the rise.
 */
void OnMinionRisen(const Monster &monster);
/** @brief Whether @p monster leaves no corpse when its death ends: the Fire Golem burns away to nothing. */
bool MinionLeavesNoBody(const Monster &monster);
/**
 * @brief The sound @p monster dies with in place of its monster type's death cry: the Fire Golem's explosion (LS_NESTXPLD).
 * SFX_NONE for every other body, which cries its own (PlayEffect, MonsterSound::Death) - Unholy Offering's victims included.
 */
_sfx_id MinionDeathSfx(const Monster &monster);

/** @brief The army panel, under the companions' (or in its place). */
void DrawMinionHud(const Surface &out);
/** @brief A click on the panel's header cycles the shared stance. */
bool HandleMinionHudClick(Point mouse);
/** @brief Whether @p mouse is over the army header - HUD, not the world behind it. */
bool IsPointOverMinionHeader(Point mouse);

} // namespace oracool
} // namespace devilution
