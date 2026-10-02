#pragma once
/**
 * @file oracool/companion.h
 *
 * Companions - the Golem rebuilt as an engine feature of its own (user, 2026-09-14: "keep this version of golem we are
 * building as a separate engine feature called Companion ... to make it feel like D3 phalanx or Ancients companions").
 *
 * WHAT A COMPANION IS. A definition (look, attack, role, ability, and how life, resistance, damage and duration grow
 * with the skill's level) and an instance (owner, rank, time left, life carried between levels). Four skills call
 * them:
 *
 * | Skill                     | Companions                               | Fights with                          |
 * |---------------------------|------------------------------------------|--------------------------------------|
 * | Valkyrie (Rogue)          | the Valkyrie                             | bow; Volley                          |
 * | Ancestral Call (Barbarian)| Korlic, Talic and Madawc, together       | melee; Whirlwind; thrown hammers     |
 * | Spirit Guardian (Monk)    | the Spirit Guardian                      | melee; holds and taunts enemies      |
 * | Decoy (Rogue)             | a blue ghost of the caster               | nothing; draws every blow near it    |
 *
 * HOW THEY LIVE IN THE ENGINE. In a dungeon a companion's body is one of the golem slots, Monsters[0..MAX_PLRS). A
 * single-player game has one player, so slots 1-3 belong to nobody: companions take those, and slot 0 stays the Golem
 * spell's. The body keeps the Golem's data row and level type; everything else is the companion's:
 *
 * - drawn in the hero sheets its definition names (GetCompanionAnim, asked by GetScaledAnim), with a real stand;
 * - its own brain (CompanionAi in monster.cpp asks GetCompanionOrders and PickCompanionTarget): a place in formation
 *   around the owner, a leash, a regroup beside the owner past it, focus on what the owner strikes, then the enemy
 *   nearest the owner, abilities on a cooldown, and a stance the player sets (J, or a click on the panel);
 * - its blows and arrows are the OWNER's, at a percent of the owner's own damage, so kills and experience are the
 *   hero's, and its arrows copy the hero's fire or lightning arrows;
 * - life and resistances from its level (CompanionDamageTaken, asked by ApplyMonsterDamage); it does not flinch;
 * - a timed life - it leaves with a flourish when the time is up, keeps its life and time across level changes, and
 *   falls for good if its life runs out;
 * - the owner walks through it, and it takes the tile the owner left (CompanionMakesWay);
 * - guards and decoys draw the attention of what comes near (CompanionTauntTarget, asked by UpdateEnemy).
 *
 * In town no monster can stand (town's dMonster holds the townsfolk), so there a companion is drawn beside the players
 * and follows, never fighting - ProcessTownCompanions and DrawTownCompanions.
 */

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>

#include "engine/point.hpp"
#include "engine/surface.hpp"
#include "monster.h"
#include "spelldat.h"

namespace devilution {
struct Player;
} // namespace devilution

namespace devilution::oracool {

enum class CompanionKind : uint8_t {
	Valkyrie,
	Korlic,
	Talic,
	Madawc,
	SpiritGuardian,
	Decoy,
};
inline constexpr size_t CompanionKindCount = 6;

enum class CompanionAttack : uint8_t {
	None,
	Bow,
	Melee,
	Throw, // hurls its weapon from range, as a bow shoots - Madawc's hammer (2026-09-29)
	Whirl, // spins where it stands while anything is beside it, striking all of it - Talic's Whirlwind (2026-09-29)
};

enum class CompanionStance : uint8_t {
	Follow,     // keeps close, fights what comes near
	Hold,       // stays where it stands, fights what it can reach from there
	Aggressive, // ranges further and chases
	Passive,    // keeps close and never attacks
};

/** @brief A companion's numbers at a skill level. */
struct CompanionStats {
	int hitPoints;       // whole points
	int elementalResist; // percent off fire, lightning, magic, cold and acid
	int physicalResist;  // percent off physical blows
	int damagePercent;   // of the owner's own blow
	int seconds;         // how long it stays
};

CompanionStats CompanionStatsAt(CompanionKind kind, int rank);
/**
 * @brief Tooltip lines for @p spell's companions at @p rank - life, resistances, damage, ability, duration - from the
 * same numbers the companion lives by. Empty for a spell that calls none. See oracool/skill_facts.h for the two rules.
 */
std::string CompanionFactsAt(SpellID spell, int rank);
const char *CompanionName(CompanionKind kind);
CompanionAttack CompanionAttackOf(CompanionKind kind);
bool IsCompanionSpell(SpellID spell);

/** @brief The cast: calls (or refreshes) @p spell's companions at @p target. False where none can stand. */
bool SummonCompanions(Player &owner, SpellID spell, Point target, int rank);
bool HasCompanion(CompanionKind kind);
/** @brief A new game: every companion forgotten, nothing touched in the monster table. */
void ForgetCompanions();
/** @brief @p owner died: every companion of his goes, as his army does (round 82 audit). */
void DismissCompanions(const Player &owner);

// ---- engine hooks ------------------------------------------------------------------------------------------------

bool IsCompanion(const Monster &monster);
const AnimStruct *GetCompanionAnim(const Monster &monster, MonsterGraphic graphic);

/**
 * @brief Talic's Whirlwind (2026-09-29, "one to attack with whirlwind"): a whirling companion starts to spin when an enemy
 * is beside it, and spins on while one is - drawn as the hero's Whirlwind is, the magic cast sheet on its full-cloud
 * frames turning, with the circling blades; striking everything beside it four times a second. CompanionAi leaves a
 * spinning companion alone; ProcessCompanions runs the spin.
 */
void StartCompanionSpin(Monster &companion);
/** @brief Ends the spin at once - the stance holds it back, or its owner is out of its leash (audit, 2026-09-29). */
void StopCompanionSpin(Monster &companion);
bool IsCompanionSpinning(const Monster &companion);
/** @brief The sprite a spinning companion is drawn with instead of its own, or none. */
std::optional<ClxSprite> CompanionSpinSprite(const Monster &companion);
/** @brief The spin's circling blades: the far half (@p front false, before the body) or the near half (after). */
void DrawCompanionBlades(const Surface &out, const Monster &companion, Point foot, bool front);
/** @brief @p damage after the companion's resistances. */
int CompanionDamageTaken(const Monster &monster, DamageType type, int damage);
/** @brief The Golem spell is taking this slot: whatever companion stood in it waits for another. */
void ForgetCompanionInSlot(Monster &slot);
/** @brief Any level load: bodies and town figures are gone; companions with time left come back on the new level. */
void OnCompanionLevelLoad();
/** @brief Once a game tick, on every level: time, cooldowns, a fallen body, a return after a level change. */
void ProcessCompanions(Player &owner);
/** @brief @p player struck @p monster - the companions' focus. */
void NoteOwnerStruck(const Player &player, const Monster &monster);
/** @brief Marks a companion's blow or arrow in progress, so NoteOwnerStruck does not take it for the owner's. */
void SetCompanionBlowInFlight(bool inFlight);
/** @brief The slot of a guard or decoy that holds @p monster's attention, or -1. */
int CompanionTauntTarget(const Monster &monster);
/** @brief Whether @p player may walk onto @p monster's tile: her own companion, standing. */
bool CompanionMakesWay(const Player &player, const Monster &monster);
/** @brief @p player is stepping onto @p tile: her companion there moves to the tile she leaves. */
void CompanionsMakeWay(Player &player, Point tile);

// ---- the brain (CompanionAi in monster.cpp) ------------------------------------------------------------------

struct CompanionOrders {
	bool valid;
	Point owner;
	Point home; // its place in formation
	int leash;
	int settle;
	int regroup;
	bool attacks;
	int reach;
	CompanionAttack attack;
	/** What a ranged one shoots - a companion's arrow, a skeletal mage's bolt (oracool/minions.h). */
	MissileID missile;
};

enum class CompanionAct : uint8_t {
	None,
	Acted,  // the ability happened this tick
	Volley, // start the ranged attack; CompanionShot looses a volley
};

/** @brief Every companion and minion turns on @p monster for @p ticks - the Necromancer's Command the Dead. */
void FocusCompanionsOn(const Monster &monster, int ticks);
/** @brief DeleteMonster: a Command the Dead focus on slot @p id ends with its monster, before the slot is reused. */
void OnCompanionFocusSlotFreed(size_t id);
/** @brief Whether @p monster is the one Command the Dead turned every companion and minion on, still - the Commanded marker. */
bool IsCommandedTarget(const Monster &monster);
/**
 * @brief Every colour to one palette ramp by its brightness - a monster body in one material (the golems). The ramps
 * run light to dark from their base; @p lightest shifts toward the light end; @p keepShadow leaves the near-black.
 */
std::unique_ptr<uint8_t[]> RampTranslation(uint8_t ramp, int lightest, bool keepShadow);

/** @brief The shared stance's name, untranslated - the army's panel shows it too (oracool/minions.h). */
const char *CompanionStanceName();

CompanionOrders GetCompanionOrders(const Monster &companion);
Monster *PickCompanionTarget(const Monster &companion, const CompanionOrders &orders);
void AimCompanion(Monster &companion, const Monster &target);
CompanionAct TryCompanionAbility(Monster &companion, Monster &target);
/** @brief The 0-based frame of its attack at which the blow lands or the arrow leaves - its sheet's own. */
int CompanionActionFrame(const Monster &companion);
void CompanionMeleeHit(Monster &companion);
void CompanionShot(Monster &companion);
void OnCompanionRegrouped(const Monster &companion);

// ---- stance and HUD -----------------------------------------------------------------------------------------------

CompanionStance GetCompanionStance();
void CycleCompanionStance();
void AnnounceCompanionStance();

void ProcessTownCompanions();
void DrawTownCompanions(const Surface &out, Point tilePosition, Point targetBufferPosition);
/** @brief The companion panel under the clock: each companion's name, life and time, and the stance. */
void DrawCompanionHud(const Surface &out);
/** @brief A click on the panel's stance line cycles the stance. True if the click was the panel's. */
bool HandleCompanionHudClick(Point mouse);
/** @brief Whether @p mouse is over the companion header - HUD, not the world behind it. */
bool IsPointOverCompanionHeader(Point mouse);

} // namespace devilution::oracool
