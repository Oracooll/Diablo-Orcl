/**
 * @file oracool/rfa12_effects.h
 *
 * Oracool (2026-09-13): the rules behind the RfA-12 skills that need no spell slot - the Paladin's
 * sixteen new auras, the Bard's eleven new songs and the twenty-one new passives.
 *
 * Their sheet NUMBERS live in class_tree.cpp's ApplyAura and ApplyPassive with every other sheet
 * number, and their per-rank level-up stat in LevelUpStats[]. This file is for the part that is a RULE
 * - a question the engine asks at one moment - exactly as oracool/passives.h is for the Round 5
 * passives, and each answer below is asked from exactly one engine site:
 *
 *   Rfa12DamageDealtPercent    PlrHitMonst, MonsterMHit      Bane of Evil, Dominion, Retaliation,
 *                                                            Dead Ground, Deadeye
 *   Rfa12MonsterDamagePercent  MonsterAttackPlayer           Dominion
 *   Rfa12MonsterArmorCutPercent EffectiveMonsterArmor        Condemnation
 *   Rfa12DamageTakenPercent    ApplyPlrDamage                Battle Hardened
 *   Rfa12BlockBonus            MonsterAttackPlayer,          Staff Parry, Brace
 *                              MonsterMHit
 *   Rfa12GrantsBlock           CalcPlrInv                    Brace
 *   PlayerIgnoresKnockback     MonsterAttackPlayer           Immovable, Heavy Foot
 *   PlayerHoldsAgainstHit      StartPlrHit                   Anthem of Valor, Grip of Iron
 *   MonsterRegenBlocked        ProcessMonsters               Lasting Wounds, Deep Wounds
 *   MonsterMayNotice           ProcessMonsters               Nocturne, Soft Tread
 *   MonsterScented             DrawMonsterHelper             Scent of Blood
 *   Rfa12ReachTarget           DoAttack                      Long Reach
 *   TitheTakesCorpse           the death animation's end     Tithe of Ash
 *   OnRfa12Hit / Struck /      DoAttack, MonsterMHit,        the clocks and the procs
 *   PlayerDamaged / Killed     MonsterAttackPlayer, death
 *   ProcessRfa12Tick           ProcessClassTreeTick          pulses, regeneration, bleeding, wakes
 *
 * The state is file-local and per player or per monster slot, so it OUTLIVES THE GAME unless cleared:
 * ClearRfa12State runs where the chill table is cleared, and ClearRfa12StateForMonster wherever a
 * monster slot is created or deleted, beside the warcries' own.
 */
#pragma once

#include <string>

#include "engine/point.hpp"
#include "oracool/class_tree.h"
#include "spelldat.h"

namespace devilution {
struct Player;
struct Monster;
} // namespace devilution

namespace devilution::oracool {

struct ItemBonusTotals;

/** @brief Net change to a blow @p player deals @p target, in percent. */
int Rfa12DamageDealtPercent(const Player &player, const Monster &target, bool melee);

/** @brief Net change to the blows @p monster deals the player, in percent; negative is weaker. */
int Rfa12MonsterDamagePercent(const Monster &monster);

/** @brief The share of @p monster's armour the local player's lit Condemnation strips, in percent. */
int Rfa12MonsterArmorCutPercent(const Monster &monster);

/** @brief Net change to a blow of @p type that @p player takes, in percent. */
int Rfa12DamageTakenPercent(const Player &player, DamageType type);

/** @brief Block chance, in percent, that @p player's skills add. */
int Rfa12BlockBonus(const Player &player);

/** @brief Whether a skill lets @p player block with what is in hand (Brace, with a spear or pike). */
bool Rfa12GrantsBlock(const Player &player);

/** @brief Whether a blow that knocks back cannot move @p player. */
bool PlayerIgnoresKnockback(const Player &player);

/** @brief Whether a blow does not interrupt @p player right now. The damage still lands. */
bool PlayerHoldsAgainstHit(const Player &player);

/** @brief Whether @p monster may not regenerate life this tick. */
bool MonsterRegenBlocked(const Monster &monster);

/** @brief Whether @p monster, standing in sight, may notice the local player this tick. */
bool MonsterMayNotice(const Monster &monster);

/** @brief Whether @p monster stays drawn out of the light: wounded by the Rogue, and not long gone. */
bool MonsterScented(const Monster &monster);

/** @brief Long Reach: the monster two tiles ahead when @p swingTile, the tile swung at, is empty. */
Monster *Rfa12ReachTarget(const Player &player, Point swingTile);

/** @brief Tithe of Ash: whether @p monster's corpse was taken, so none is left. Answers once. */
bool TitheTakesCorpse(const Monster &monster);

/** @brief A blow @p player landed on @p monster for @p damage (1/64 units). */
void OnRfa12Hit(Player &player, Monster &monster, int damage, bool melee);

/** @brief @p monster landed a melee blow on @p player. */
void OnRfa12Struck(Player &player, Monster &monster);

/** @brief @p monster's missile or spell landed on @p player for @p damage (1/64 units). Feedback. */
void OnRfa12MissileStruck(Player &player, Monster &monster, int damage);

/** @brief @p player just lost life and is still standing. */
void OnRfa12PlayerDamaged(Player &player);

/** @brief @p player's blow killed @p monster. */
void OnRfa12MonsterKilled(Player &player, const Monster &monster);

/** @brief How much sooner slows on @p player wear off, in percent - Sanctity's half. */
int Rfa12SlowShortenPercent(const Player &player);

/** @brief One game tick of the clocks, pulses and regeneration. */
void ProcessRfa12Tick(Player &player);

/** @brief The damage @p player would take, after a ward drinks its share (1/64 units). Chord of Warding. */
int Rfa12AbsorbDamage(Player &player, int damage);

/** @brief Whether a melee blow that would land on @p player misses instead. Mantra of Evasion. */
bool Rfa12EvadesMelee(const Player &player);

/** @brief Whether @p monster's plain resistances are stripped right now. Satire. */
bool Rfa12StripsResistances(const Monster &monster);

/** @brief Whether @p player's arrow passes @p monster by. Hunter's Claim. */
bool Rfa12ArrowIgnores(const Player &player, const Monster &monster);

/** @brief The extra cold damage @p monster takes, in percent. Frostbite. */
int Rfa12ColdDamagePercent(const Monster &monster);

/** @brief The RfA-12 actives' sheet buffs - Iron Will, Conduit, Saga, Astral Projection - into the totals. */
void ApplyRfa12BuffsToTotals(const Player &player, ItemBonusTotals &totals);

/** @brief Deep Wounds' bleed, for any skill that makes a monster bleed: @p perSecond in 1/64 units. */
void BleedMonster(const Monster &monster, int ticks, int perSecond);

/** @brief Stops @p monster regenerating for @p ticks. Lasting Wounds, Bitter Couplet. */
void BlockMonsterRegen(const Monster &monster, int ticks);

/** @brief Keeps @p monster drawn out of the light for @p ticks. Scent of Blood, Sonnet of Sight. */
void ScentMonster(const Monster &monster, int ticks);

/** @brief @p monster leaves no corpse when it dies. Tithe of Ash, Votive Strike. */
void TakeCorpseOf(const Monster &monster);

/** @brief Forgets every clock and mark. Called where the chill table is cleared. */
void ClearRfa12State();

/** @brief Forgets what was known about @p monster's slot. Called where the warcries' is. */
void ClearRfa12StateForMonster(const Monster &monster);

/** @brief Tooltip lines for what @p aura does off the sheet at @p points, or empty. */
std::string Rfa12AuraFactsAt(ClassTreeSkill aura, int points);

/** @brief Whether @p aura reaches the monsters around the player, so its radius means something. */
bool Rfa12AuraReachesMonsters(ClassTreeSkill aura);

} // namespace devilution::oracool
