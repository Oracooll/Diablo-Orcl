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

#include <string>

#include "engine/point.hpp"
#include "oracool/class_tree.h"
#include "spelldat.h"

namespace devilution {
struct Player;
struct Monster;
struct Missile;
} // namespace devilution

namespace devilution::oracool {

/**
 * @brief Tooltip lines for the tree passives and Passive Skills page rows whose RULE lives in passives.cpp, at
 * @p points (1 for a Passive Skills page row): the main effect with its numbers, from the same named
 * helpers the rule reads. Empty for any other row. See oracool/skill_facts.h for the two rules.
 */
std::string PassiveFactsAt(const Player &player, ClassTreeSkill skill, int points);

/**
 * @brief Whether @p skill is live on @p player: implemented, its class, unlocked, and either slotted
 * (a Passive Skills page row) or invested (a Diablo II passive). The single definition of "on".
 */
bool PassiveActive(const Player &player, ClassTreeSkill skill);

/** @brief Net change to a blow @p player is about to take, in percent; negative is less. */
int PassiveDamageTakenPercent(const Player &player, DamageType damageType);

/**
 * @brief Net change to a blow @p player is about to deal @p target, in percent. @p melee: a swing rather than a missile.
 * @p burst: a weapon's fire or lightning burst - melee, but not the blow the per-blow counters (Cadence, Counterstroke) belong to.
 */
int PassiveDamageDealtPercent(const Player &player, const Monster &target, bool melee, bool burst = false);
/** @brief The part of that sum that holds against every target (Glass Cannon), for the sheet's ranges. */
int PassiveUnconditionalDamagePercent(const Player &player);

/** @brief Dodge standing, Evade moving: a melee blow that would have landed slips instead. */
bool PassiveEvadesMelee(const Player &player);
/** @brief The chance, in percent, that PassiveEvadesMelee slips a blow - Dodge standing, Evade @p walking. No roll. */
int PassiveMeleeSlipChance(const Player &player, bool walking);

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
/** @p burst: a weapon cleave's side blow (round 70 audit) - the per-blow passives pay, the swing's counters (Counterstroke,
 *  Cadence, Combination Strike, Mythic Rhythm, the shared rules) wait for the front blow. */
void OnPassiveHit(Player &player, const Monster &target, int damage, bool melee, bool burst = false);

/**
 * @brief Juggernaut: a stagger @p player would take is shrugged off half the time; one that lands has
 * a 30% chance to heal a fifth of life, once every ten seconds. Stagecraft: never while a song plays.
 * Asked by StartPlrHit.
 */
bool PassiveShrugsOffStagger(Player &player);

// ---- the all-heroes sweep (2026-09-14) ----

/** @brief A missile @p player's spell or arrow landed on @p target - Paralysis, Temporal Flux, Thrill of the Hunt, the element marks. */
void OnPassiveMissileHit(Player &player, const Monster &target, int damage, DamageType damageType, bool arrow, bool sharedRulesDone = false);
/** @brief Any monster's death, whoever killed it (MonsterDeath): Life from Death. */
void OnPassiveMonsterDied(Player &player, const Monster &monster);
/** @brief A blow reached @p player, before any shield absorbs it: Galvanizing Ward's clock restarts. */
void OnPassiveStruck(Player &player);

/** @brief @p player blocked a blow - Insurmountable, Renewal, Counterstroke. Asked by StartPlrBlock. */
void OnPassiveBlock(Player &player);

/** @brief @p player just lost @p damage life (1/64 units) - Galvanizing Ward, Illusionist. Asked by ApplyPlrDamage. */
/** @p lifeLost: what the blow actually took (round 87 audit); -1 reads @p damage. */
void OnPassiveDamaged(Player &player, int damage, int lifeLost = -1);

/** @brief Block chance added by Hold Your Ground and Reed in the Wind, in percent. */
int PassiveBlockBonus(const Player &player);

/** @brief Iron Maiden: share of a melee blow returned to the attacker, in percent, on top of Thorns. */
int PassiveThornsPercent(const Player &player);

/** @brief The bursts of speed - Illusionist, Tactical Advantage, Hot Pursuit - in percent. Asked by MovementSpeedBonusPercent. */
int PassiveMoveSpeedBonus(const Player &player);

/** @brief Change to a blow @p monster deals the local player - Numbing Traps, Dissonance - in percent. */
int PassiveMonsterDamagePercent(const Monster &monster);

/** @brief Change to @p spell's mana price - Chant of Resonance - in percent. */
int PassiveManaCostPercent(const Player &player, SpellID spell);

/** @brief Custom Engineering: spell levels a rune trap @p player sets gains. Asked by AddRune. */
int PassiveRuneLevelBonus(const Player &player);

/** @brief Custom Engineering: whether this rune is set without being used up. Asked by ConsumeScroll. */
bool PassiveSparesRune(const Player &player);

/** @brief @p player loosed arrows this frame toward @p target - Grenadier counts them. Asked by DoRangeAttack. */
void OnPassiveArrowLoosed(Player &player, Point target);

/** @brief Change to one named skill's damage - Blunt, Towering Shield - in percent. */
int PassiveSkillDamagePercent(const Player &player, SpellID spell);

/** @brief Juggernaut: how much shorter a slow on @p player runs, in percent. Asked by SlowPlayer. */
int PassiveSlowShortenPercent(const Player &player);

/** @brief Inspiring Presence: a warcry blessing's length, in percent of its normal length. Asked by the warcries' StartBuff. */
int PassiveWarcryDurationPercent(const Player &player);

/** @brief @p player spent @p cost mana (1/64 units) on a skill or spell. */
void OnPassiveManaSpent(Player &player, int cost);

/** @brief @p player's blow killed @p monster. */
void OnPassiveMonsterKilled(Player &player, const Monster &monster);
/** @brief A corpse was used by one of the Necromancer's skills (oracool/corpses.h) - Fueled by Death. */
void OnPassiveCorpseConsumed(Player &player);

/** @brief Fleet Footed: the Monk runs. Asked by IsClassTreeRunActive. */
bool PassiveRunActive(const Player &player);

/** @brief One game tick of the clocks. */
void ProcessPassivesTick(Player &player);

/** @brief Empties every clock, stack and mark - a full reset, for tests. */
void ClearPassiveState();
/**
 * @brief Empties the per-monster marks only - the slots are about to be handed to other monsters. Called on every level
 * load. The hero's own clocks walk down the stairs with him (audit, 2026-09-27): the level load used to empty them too,
 * so the stairs reset Cheat Death's minute, Final Service, Rathma's Shield and the Rampage stacks. A new game empties
 * them through ClearPassiveClocks.
 */
void ClearPassiveMarks();
/** @brief @p monster's element marks gone, when its slot is freed or reused - the next occupant must not inherit them. */
void ClearPassiveMarksForMonster(const Monster &monster);

/** @brief Empties one player's clocks - the save's cooldown among them. Called where a new game clears the cold armour, so a cooldown cannot carry from the last character to this one. */
void ClearPassiveClocks(Player &player);

} // namespace devilution::oracool
