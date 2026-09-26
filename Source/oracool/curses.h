#pragma once
/**
 * @file oracool/curses.h
 *
 * Monster curses (Plan - The Necromancer, section 5.1; phase N7): the Curses page, paid in Essence.
 *
 * ONE curse per monster: a kind, a rank, an owner and a clock. A new curse replaces the old - except Doom, which
 * a weaker curse cannot displace. Cast as an area at the cursor (radius 2, wider with Wide Malice), or on one
 * monster for Attract and Death Mark. Duration 8 s + 1 s a rank, longer with Curse Mastery.
 *
 * What a curse does is asked at the engine's seams, never pushed:
 *  - damage TAKEN  (monster.cpp ApplyMonsterDamage)   Amplify Damage, Lower Resist, Decrepify, Doom; Frailty's floor
 *  - damage DEALT  (oracool/warcries DebuffMonster)    Weaken, Decrepify - the warcries' own debuff, reused
 *  - a blow landed on the hero or a minion (MonsterAttackPlayer / MonsterAttackMonster)   Iron Maiden
 *  - a blow landed ON the cursed monster (player.cpp / missiles.cpp / OnMinionBlow)        Life Tap
 *  - noticing the hero (rfa12_effects MonsterMayNotice)                                    Dim Vision
 *  - the AI step (monster.cpp ProcessMonsters)                                             Terror
 *  - the target pick (monster.cpp UpdateEnemy)                                             Attract
 *  - death (monster.cpp MonsterDeath)                                                      Death Mark, Essence Tap
 *  - the clock (ProcessCursesTick)                                                         Bane's rot, Confuse's turning
 * Confuse turns the monster the way the Barbarian's cry does: MFLAG_BERSERK | MFLAG_GOLEM for the duration.
 *
 * A marker over the head says which curse (DrawCurseMarker): a sigil from ui/curse_markers.png, or a lettered chip if it fails to load.
 * Per level, per slot, never saved.
 */

#include <cstdint>
#include <string>

#include "engine/point.hpp"
#include "engine/surface.hpp"
#include "oracool/class_tree.h"
#include "spelldat.h"

namespace devilution {

struct Monster;
struct Player;
enum class DamageType : uint8_t;

namespace oracool {

enum class CurseKind : uint8_t {
	None,
	AmplifyDamage,
	DimVision,
	Weaken,
	Frailty,
	IronMaiden,
	Terror,
	Bane,
	Confuse,
	LifeTap,
	Attract,
	Decrepify,
	DeathMark,
	LowerResist,
	Doom,
};

/** @brief Tooltip lines for curse @p spell at @p rank for @p player (Curse Mastery, Wide Malice): duration, radius, effect. */
std::string CurseFactsAt(const Player &player, SpellID spell, int rank);
/**
 * @brief Tooltip lines for the curse passives whose rule lives here - Curse Mastery, Essence Tap, Wide Malice, and
 * the Passive Skills page's Eternal Torment - at @p points. Empty for any other row. (NecroPassiveFactsAt calls it.)
 */
std::string CursePassiveFactsAt(ClassTreeSkill skill, int points);

/** Dim Vision: a blinded monster notices only a hero within this many tiles (rfa12_effects MonsterMayNotice). */
constexpr int DimVisionSightTiles = 1;
/** @brief Death Mark's burst, in percent of the dead one's life at @p rank - Corpse Explosion's own share (rfa12_actives). */
int CorpseBurstPercent(int rank);
/** @brief Casts one of the page's actives at rank @p rank. False fizzles the cast and refunds it. */
bool CastNecromancerCurse(Player &player, SpellID spell, Point target, int rank);
bool IsNecromancerCurse(SpellID spell);

CurseKind CurseOn(const Monster &monster);
/** @brief Living cursed monsters within @p radius of @p centre - Spreading Malediction. */
int CursedMonstersNear(Point centre, int radius);
const char *CurseName(CurseKind kind);

// ---- the seams ----------------------------------------------------------------------------------------------------
/** @brief @p damage of @p type about to land on @p monster, after its curse. */
int CurseDamageTaken(const Monster &monster, DamageType type, int damage);
/** @brief Frailty: whether a monster now at @p hitPoints of @p maxHitPoints simply dies. */
bool CurseFinishes(const Monster &monster, int hitPoints, int maxHitPoints);
/** @brief Iron Maiden: @p monster landed a blow for @p damage on the hero or a minion; it takes a multiple back. */
void OnCursedMonsterDealtBlow(Monster &monster, int damage);
/** @brief Life Tap: a blow for @p damage landed on @p monster by @p player, or by a minion of @p player's. */
void OnCursedMonsterStruck(const Monster &monster, Player &player, Monster *minion, int damage);
/** @brief Dim Vision: the monster sees only what stands beside it. */
bool CursedMonsterBlinded(const Monster &monster);
/** @brief Terror: took its step away from the hero this tick (so the AI is skipped). Uniques never run. */
bool CursedMonsterFlees(Monster &monster);
/** @brief Attract: the cursed monster within 8 of @p monster that draws its attention, or -1. */
int CurseLureTarget(const Monster &monster);
/** @brief The cursed monster died: Death Mark bursts it, Essence Tap pays its owner. */
void OnCursedMonsterDeath(const Monster &monster);

/** @brief Once a tick per player: clocks, Bane, Confuse's release. */
void ProcessCursesTick(Player &player);
/** @brief The slot is handed back, the level is gone, or the game is new. */
void ClearCurseForMonster(const Monster &monster);
void ClearAllCurses();

/** @brief The chip over a cursed monster's head, @p anchor being the top-centre of its sprite. */
void DrawCurseMarker(const Surface &out, const Monster &monster, Point anchor);

} // namespace oracool
} // namespace devilution
