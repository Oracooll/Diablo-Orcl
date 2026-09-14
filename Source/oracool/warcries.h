/**
 * @file oracool/warcries.h
 *
 * Oracool, Round 6 of the inert-skill plan (2026-09-03): the timed shouts and songs.
 *
 * ONE MECHANISM, TWO VOCABULARIES, as the plan put it. A cry is cast like a spell - it has a
 * SpellID, a mana price, the cast animation and sound - and its missile (MissileID::Warcry, one for
 * all of them) does nothing but call CastWarcry and delete itself. What the cry then does is one of
 * three things:
 *
 *   A TIMED BUFF on the caster - Shout, Battle Orders, Battle Command, Purifying Breath,
 *   Vengeance, Slow Missiles, Tranquility. Kept in a per-player table with a tick count; the sheet
 *   ones feed ItemBonusTotals through ApplyWarcryBuffsToTotals and force a sheet recompute when
 *   they start and when they end, so a buff cannot outlive its clock.
 *
 *   A TIMED DEBUFF on the monsters that heard it - Battle Cry, Inner Sight. Kept per monster,
 *   asked about at the point of use: the monster's armour by every player to-hit roll, its damage
 *   and aim by MonsterAttackPlayer.
 *
 *   AN IMMEDIATE REACTION - Howl and Daze (retreat, the channel Sanctuary already pushes on),
 *   Taunt (wake and target the caster), War Cry, Lullaby, Bard's Shout, Sound Shock, Temple Bell
 *   (stun, some with damage).
 *
 * The Bard's three song-auras and the Paladin's Holy Freeze are the same vocabulary held rather
 * than shouted: they are Kind::Aura rows, lit through the existing aura toggle, and the monster-side
 * queries below fold them in beside the timed debuffs - Discord strips armour, Weaken blunts aim
 * and chills, Dirge of Dread weakens and repels, Holy Freeze chills.
 */
#pragma once

#include "engine/point.hpp"
#include "oracool/class_tree.h"
#include "spelldat.h"

namespace devilution {
struct Player;
struct Monster;
struct Missile;
struct AddMissileParameter;
} // namespace devilution

namespace devilution::oracool {

struct ItemBonusTotals;

/** @brief Whether @p spell is a cry - one of the seventeen this module casts. */
bool IsWarcry(SpellID spell);

/** @brief The cry's row in the tree, for its rank and its class. Skill::None for a spell that is not a cry. */
ClassTreeSkill WarcrySkill(SpellID spell);

/**
 * @brief Performs @p spell for @p player at its current rank. False if it had nothing to do -
 * nothing in earshot, or a buff the caster already carries at full length - in which case the
 * cast fizzles and costs nothing.
 */
bool CastWarcry(Player &player, SpellID spell);

/** @brief The same, aimed: Conversion wants the tile under the cursor. The rest ignore @p target. */
bool CastWarcry(Player &player, SpellID spell, Point target);

/** @brief Ticks left on @p player's @p spell buff; 0 when not carried. */
int WarcryBuffTicks(const Player &player, SpellID spell);

/** @brief The sheet buffs - Shout, Battle Orders, Battle Command, Purifying Breath, Vengeance - into the totals. */
void ApplyWarcryBuffsToTotals(const Player &player, ItemBonusTotals &totals);

/** @brief Slow Missiles: an arrow that would have struck @p player turns aside instead. Rolled per arrow. */
bool SlowMissilesTurnsAside(const Player &player);

/** @brief Lays a timed debuff on @p monster: @p damagePercent and @p armorPercent (negative) for @p ticks. The deeper of two overlapping debuffs wins, and the longer. */
void DebuffMonster(const Monster &monster, int ticks, int damagePercent, int armorPercent);

/** @brief What @p monster's damage is right now, in percent of its own; negative is weakened. Battle Cry, Dirge of Dread. */
int MonsterDebuffDamagePercent(const Monster &monster);

/** @brief What @p monster's armour is right now, in percent of its own; negative is stripped. Battle Cry, Inner Sight, Discord. */
int MonsterDebuffArmorPercent(const Monster &monster);

/** @brief Points off @p monster's chance to hit a player. Weaken. */
int MonsterDebuffToHit(const Monster &monster);

/** @brief @p monster's armour as every player to-hit roll should see it: the pack aura's, then the debuffs. */
int EffectiveMonsterArmor(const Monster &monster);

/** @brief One game tick: buffs run down, the held auras and Tranquility do their per-tick work. */
void ProcessWarcriesTick(Player &player);

/** @brief Whether any warcry blessing is on @p player - Inspiring Presence's mend asks. */
bool AnyWarcryBuffActive(const Player &player);

/** @brief Empties the monsters' side - debuffs, conversions, wards. Called where the chill table is cleared, once per level. */
void ClearWarcries();

/**
 * @brief Forgets everything the cries knew about @p monster's SLOT.
 *
 * The debuff table is keyed by monster index and nothing else, and the engine reuses an index the
 * moment its monster is deleted - skeletons, golems, doppelgangers. Without this a monster raised
 * into a dead one's slot inherited its remaining Battle Cry, and an expiring Conversion cleared
 * MFLAG_BERSERK | MFLAG_GOLEM on whoever stood there by then (external audit, 2026-09-06: WCR-01).
 * Called from InitMonster, which every creation path goes through, and from DeleteMonster.
 */
void ClearWarcryStateForMonster(const Monster &monster);

/** @brief Empties @p player's own buffs, recomputing the sheet if one was on it. Called where a new game clears the cold armour. */
void ClearWarcryBuffs(Player &player);

/** @brief One sentence for the Abilities window, untranslated. "" for a spell that is not a cry. */
const char *WarcryDescription(SpellID spell);

/** @brief The missile every cry is cast as: calls CastWarcry and is gone. */
void AddWarcry(Missile &missile, AddMissileParameter &parameter);

/** @brief What @p spell does at @p rank, one fact per line: radius, duration, magnitude, chance. For the tooltip. */
std::string WarcryFactsAt(SpellID spell, int rank);

} // namespace devilution::oracool
