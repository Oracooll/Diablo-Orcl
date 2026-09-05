/**
 * @file oracool/skill_facts.h
 *
 * What a skill DOES at a rank, as lines for the tooltip: strikes, range, duration, stun, chance,
 * splash - "all the stuff that happens in the back of the engine during triggering" (user,
 * 2026-09-05). Each module that runs a skill answers for its own skills, beside its own formula
 * (MeleeSkillFactsAt, WarcryFactsAt, PaladinSkillFactsAt, RogueArrowFactsAt, ColdSpellFactsAt);
 * this only routes a SpellID to the right one, so a fact can never be written in two places.
 */
#pragma once

#include <string>

#include "spelldat.h"

namespace devilution::oracool {

/** @brief The facts of @p spell at @p rank, newline-separated; empty for a spell no module describes. */
std::string SkillFactsAt(SpellID spell, int rank);

} // namespace devilution::oracool
