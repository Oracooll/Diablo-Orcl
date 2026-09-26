/**
 * @file oracool/skill_facts.h
 *
 * What a skill DOES at a rank, as lines for the tooltip: strikes, range, duration, stun, chance,
 * splash - "all the stuff that happens in the back of the engine during triggering" (user,
 * 2026-09-05). Each module that runs a skill answers for its own skills, beside its own formula
 * (MeleeSkillFactsAt, WarcryFactsAt, PaladinSkillFactsAt, RogueArrowFactsAt, ColdSpellFactsAt,
 * Rfa12ActiveFactsAt, CurseFactsAt, NecroSummoningFactsAt); this only routes a SpellID to the right
 * one, so a fact can never be written in two places.
 *
 * THE TWO RULES (user, 2026-09-26): every skill, spell and aura gives a main effect plus something
 * that grows with each level, and its tooltip states both with numbers. The numbers live in the
 * Current / Next Level blocks these functions fill - a description's prose never shows the current
 * value, so it does not count. test/oracool_skill_rules_test.cpp holds every row to it.
 */
#pragma once

#include <string>

#include "spelldat.h"

namespace devilution {
struct Player;
} // namespace devilution

namespace devilution::oracool {

/** @brief The facts of @p spell at @p rank, newline-separated; empty for a spell no module describes. */
std::string SkillFactsAt(SpellID spell, int rank);

/**
 * @brief SkillFactsAt, plus the facts only @p player can answer: the RfA-12 actives (some read the
 * hero's passives), the Necromancer's curses and summons (masteries), and a book spell's per-level
 * terms (some also read character level).
 */
std::string SkillFactsAt(const Player &player, SpellID spell, int rank);

/**
 * @brief A book spell's hidden per-level terms - bolt count, leap radius, duration, width, charges -
 * which a spell level changes but no Damage line shows. Empty for a spell with none.
 */
std::string BookSpellFactsAt(const Player &player, SpellID spell, int level);

/**
 * @brief Everything @p spell does at @p level for @p player, as tooltip lines: the resource line
 * (mana, Rage, Essence), Damage or Heals, then SkillFactsAt. The one block both the Spells sheet
 * and the Abilities window print, so a spell reads the same on both.
 */
std::string SpellLevelLines(const Player &player, SpellID spell, int level);

} // namespace devilution::oracool
