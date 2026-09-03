/**
 * @file oracool/cold.h
 *
 * Oracool, Round 2 of the inert-skill plan (2026-09-03): the Sorceress's cold line, in one place.
 *
 * Round 1 gave the engine DamageType::Cold, a chill, and Ice Bolt. This module is everything the
 * other eight rows on that page share, so that each of them is a few lines of data on top of it
 * rather than a copy of the last one:
 *
 *   - which SpellIDs are cold, and what each does on a hit (chill, freeze, or a shatter around it);
 *   - the damage of every cold spell at a given rank, from ONE function that both the missile and
 *     the Abilities window's tooltip read, because a sheet that reports a number the game does not
 *     use is indistinguishable from a broken sheet (this project has paid for that three times);
 *   - Cold Mastery, the passive that pierces cold resistance;
 *   - the three armours - Frozen, Shiver, Chilling - which are one mechanism with three reactions:
 *     a shell the caster wears for a while, and something that happens to whatever hits them.
 *
 * What is NOT here: the missiles' own motion, which lives in missiles.cpp beside every other
 * missile's, and the chill table itself, which is oracool/chill.h.
 */
#pragma once

#include <cstdint>

#include "spelldat.h"

namespace devilution {
struct Player;
struct Monster;
struct Missile;
} // namespace devilution

namespace devilution::oracool {

/** @brief Whether @p spell is one of the cold line's castable spells (Cold Mastery is a passive and is not). */
bool IsColdSpell(SpellID spell);

/** @brief The armours, which are cold spells that are also a state the caster is in. */
bool IsColdArmourSpell(SpellID spell);

/**
 * @brief Damage of @p spell at rank @p spellLevel for @p player, as a min..max range.
 *
 * Both outputs are -1 for a cold spell that does no direct damage of its own - the armours, and
 * Frozen Orb, whose damage is its bolts'. THE function: GetDamageAmtAtLevel calls it for the
 * tooltip and every cold missile's Add function calls it for the hit.
 */
void ColdSpellDamage(const Player &player, SpellID spell, int spellLevel, int &minDamage, int &maxDamage);

/**
 * @brief What a cold hit does to the monster it lands on, applied by MonsterMHit for every cold
 * missile so the next cold row inherits it without a line.
 *
 * Ice Bolt and the arrows chill; Ice Blast and Glacial Spike freeze; Frost Nova and Blizzard chill
 * for longer. The durations grow with the missile's spell level. A unique is never frozen, only
 * chilled - a boss that stands still for a rank-twenty Ice Blast is a boss with no fight in it.
 */
void ApplyColdHit(MissileID type, int spellLevel, Monster &monster);

/**
 * @brief A number that changes on every cast of an armour, so an armour's missile can tell whether
 * it is still the current one. Two casts of Frozen Armor would otherwise both tick the same state.
 */
int ColdArmourCastSerial(const Player &player);

/**
 * @brief The frozen tint: every palette index mapped onto the cool blue-grey ramp by brightness.
 *
 * Rebuilt from the live palette on each call rather than cached, because the palette is the
 * level's and this is 256 comparisons - cheaper than remembering which level built it last.
 */
uint8_t *ColdTRN();

/**
 * @brief Cold Mastery's whole effect: how much of the resistance penalty a cold hit keeps.
 *
 * A resisted hit is normally quartered (`dam >>= 2`). Mastery hands back part of that: the return
 * value is the DIVISOR to use instead - 4 with no mastery, 2 from rank 3, 1 (no penalty at all)
 * from rank 6. Asked by MonsterMHit only for DamageType::Cold.
 */
int ColdResistanceDivisor(const Player &player);

/** @brief Cold Mastery's damage side: a percentage bonus on every cold hit, 6 per rank. */
int ColdMasteryDamagePercent(const Player &player);

// ---------------------------------------------------------------------------------------------
// The armours
// ---------------------------------------------------------------------------------------------

/** @brief Which armour @p player wears right now, or SpellID::Invalid. */
SpellID ActiveColdArmour(const Player &player);

/**
 * @brief Puts @p spell on @p player for @p ticks at @p spellLevel, replacing any armour already worn.
 *
 * Replacing, not stacking: the three are one slot, exactly as in the game they come from, and
 * casting Shiver Armor over Frozen Armor is how you change your mind.
 */
void CastColdArmour(Player &player, SpellID spell, int spellLevel, int ticks);

/** @brief Ages the armour by one tick and takes it off when it runs out. Called once per tick by the armour's own missile. */
void TickColdArmour(Player &player);

/** @brief Takes the armour off now - a new game, a death. */
void ClearColdArmour(Player &player);
void ClearAllColdArmours();

/**
 * @brief The armour's reaction to @p monster landing a MELEE blow on @p player.
 *
 * Frozen Armor freezes the attacker; Shiver Armor chills it and deals cold damage; Chilling Armor
 * chills it and answers with an Ice Bolt. Called from MonsterAttackPlayer after the blow lands.
 */
void OnColdArmourStruckInMelee(Player &player, Monster &monster);

/**
 * @brief Chilling Armor's reaction to a RANGED hit: an Ice Bolt at the shooter. The other two
 * armours do nothing here, which is the difference between them.
 */
void OnColdArmourStruckAtRange(Player &player, Monster &monster);

/** @brief The frame of the shell to draw over @p player this instant, or -1 for no armour. */
int ColdArmourShellFrame(const Player &player);

/** @brief One sentence per cold spell for the Abilities window, untranslated. "" for a spell that is not cold. */
const char *ColdSpellDescription(SpellID spell);

} // namespace devilution::oracool
