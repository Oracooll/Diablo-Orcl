/**
 * @file oracool/passives.h
 *
 * Oracool, Round 5 of the inert-skill plan (2026-09-03): the passives the engine can already pay.
 *
 * A passive that is a NUMBER ON THE SHEET - armour, mana, a resistance, an attack-speed flag -
 * lives in class_tree.cpp's ApplyPassive, where every other sheet passive lives, and needs nothing
 * from this file. This file is for the passives that are a RULE rather than a number: they answer
 * a question the engine asks at one moment - "how much of this blow do I take", "does this arrow
 * carry on", "am I dead" - and that moment is a hook.
 *
 * Seven hooks, each asked from exactly one engine site, and every rule-passive is a case in one:
 *
 *   PassiveDamageTakenPercent   ApplyPlrDamage       Blur, Sixth Sense, Vigilant, Sword and Board,
 *                                                    Relentless, Unwavering Will
 *   PassiveDamageDealtPercent   PlrHitMonst,         Ruthless, Ambush, Brawler, Determination,
 *                               MonsterMHit          Steady Aim, Audacity, Power Hungry, Cold Blooded,
 *                                                    Cull the Weak, Relentless Assault, Single Out,
 *                                                    Rampage, Unwavering Will, Cadence
 *   PassiveEvadesMelee/Missile  MonsterAttackPlayer, Dodge, Evade, Avoid
 *                               PlayerMHit
 *   ArrowPierces                CheckMissileCol      Pierce
 *   PassiveCheatsDeath          ApplyPlrDamage       Indestructible, Nerves of Steel, Awareness,
 *                                                    Near Death Experience
 *   OnPassiveHit / ManaSpent /  DoAttack, MonsterMHit, Leech, Bloodthirst, Transcendence, Rampage,
 *   MonsterKilled               ConsumeSpell, death  Requiem, Cadence
 *   ProcessPassivesTick         ProcessClassTreeTick the clocks: stillness, stacks, cooldowns, Brooding
 *
 * The state the clocks need is file-local and per player index, and ClearPassiveState empties it
 * where the chill table is emptied - per level, which is where every other per-game static in this
 * fork learned it has to be cleared (see the "statics outlive the game" memory).
 */
#pragma once

#include "oracool/class_tree.h"
#include "spelldat.h"

namespace devilution {
struct Player;
struct Monster;
struct Missile;
} // namespace devilution

namespace devilution::oracool {

/**
 * @brief Whether @p skill is live on @p player: implemented, its class, unlocked, and either slotted
 * (a Passive Skills page row) or invested (a Diablo II passive). The single definition of "on".
 */
bool PassiveActive(const Player &player, ClassTreeSkill skill);

/** @brief Net change to a blow @p player is about to take, in percent; negative is less. */
int PassiveDamageTakenPercent(const Player &player, DamageType damageType);

/** @brief Net change to a blow @p player is about to deal @p target, in percent. @p melee: a swing rather than a missile. */
int PassiveDamageDealtPercent(const Player &player, const Monster &target, bool melee);

/** @brief Dodge standing, Evade moving: a melee blow that would have landed slips instead. */
bool PassiveEvadesMelee(const Player &player);

/** @brief Avoid: an arrow that would have landed slips instead. */
bool PassiveEvadesMissile(const Player &player);

/** @brief Pierce: whether @p missile, a player's arrow that just struck, carries on. */
bool ArrowPierces(Missile &missile);

/**
 * @brief The once-a-minute saves. Called when @p player's life has just reached zero; true if a
 * passive stood the character back up (life set, cooldown started), in which case the death is off.
 */
bool PassiveCheatsDeath(Player &player);

/** @brief A blow @p player landed on @p target for @p damage (in the engine's 1/64 life units). */
void OnPassiveHit(Player &player, const Monster &target, int damage, bool melee);

/** @brief @p player spent @p cost mana (1/64 units) on a skill or spell. */
void OnPassiveManaSpent(Player &player, int cost);

/** @brief @p player's blow killed @p monster. */
void OnPassiveMonsterKilled(Player &player, const Monster &monster);

/** @brief Fleet Footed: the Monk runs. Asked by IsClassTreeRunActive. */
bool PassiveRunActive(const Player &player);

/** @brief One game tick of the clocks. */
void ProcessPassivesTick(Player &player);

/** @brief Empties every clock and stack. Called where the chill table is cleared. */
void ClearPassiveState();

/** @brief Empties one player's clocks - the save's cooldown among them. Called where a new game clears the cold armour, so a cooldown cannot carry from the last character to this one. */
void ClearPassiveClocks(Player &player);

} // namespace devilution::oracool
