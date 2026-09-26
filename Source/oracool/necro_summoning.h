#pragma once
/**
 * @file oracool/necro_summoning.h
 *
 * The Necromancer's Summoning page (Plan - The Necromancer, phase N5): the eighteen rows that raise, shape and
 * command the army of oracool/minions.h, from the corpses of oracool/corpses.h.
 *
 * The actives are cast through the RfA-12 door (rfa12_actives.cpp's CastOnce hands anything it does not know to
 * CastNecromancerSummoning), so their animation, aim, mana or Essence and fizzle-refund are the engine's. The five
 * passives are tree investments read at the moment they matter: the masteries when a body is raised, Summon Resist
 * when a blow lands on one, Lasting Bond when a Revived is timed.
 */

#include <string>

#include "engine/point.hpp"
#include "oracool/class_tree.h"
#include "spelldat.h"

namespace devilution {

struct Player;

namespace oracool {

/** @brief Casts one of the page's actives at rank @p rank. False fizzles the cast and refunds it. */
bool CastNecromancerSummoning(Player &player, SpellID spell, Point target, int rank);
/** @brief Whether @p spell is one of this page's actives. */
bool IsNecromancerSummoning(SpellID spell);
/** @brief Army of the Dead's pulses and the like: once a tick per player. */
void ProcessNecromancerSummoningTick(Player &player);
/** @brief A new game. */
void ClearNecromancerSummoningState();

/** @brief Tooltip lines for summoning @p spell at @p rank for @p player (masteries read): count, life, damage, armour, duration. */
std::string NecroSummoningFactsAt(const Player &player, SpellID spell, int rank);

/**
 * @brief Tooltip lines for the tree passives and Passive Skills page rows whose RULE lives in the Necromancer's modules (necro_summoning.cpp, curses.cpp, corpses.cpp, minions.cpp), at
 * @p points (1 for a Passive Skills page row): the main effect with its numbers, from the same named
 * helpers the rule reads. Empty for any other row. See oracool/skill_facts.h for the two rules.
 */
std::string NecroPassiveFactsAt(const Player &player, ClassTreeSkill skill, int points);

/** @brief How many of a group a hero of this rank may keep: 1 at rank 1, one more every three ranks, @p cap at most. */
int RaisedCountAtRank(int rank, int cap);

} // namespace oracool
} // namespace devilution
