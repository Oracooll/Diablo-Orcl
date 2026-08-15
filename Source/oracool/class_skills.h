/**
 * @file oracool/class_skills.h
 *
 * Oracool: the six innate class skills, now shared by every class.
 *
 * Vanilla gives each class exactly one - Item Repair to the Warrior, Trap Disarm to the Rogue and so
 * on - and the Abilities window listed whichever one you had among your spells. User request
 * (2026-08-15): all six, to everyone, from birth, on a sheet of their own.
 *
 * They stay SpellIDs and stay in _pAblSpells, so nothing about how they are cast, drawn or saved
 * changes; only the size of the set does. That also means the change needs no save migration:
 * InitPlayer rewrites _pAblSpells from scratch every time a character is loaded, so an existing
 * character picks the other five up simply by being loaded once.
 */
#pragma once

#include <cstddef>
#include <cstdint>

#include "spelldat.h"

namespace devilution {
namespace oracool {

constexpr size_t ClassSkillCount = 6;

/**
 * @brief The six, in the order the Class Skills sheet lists them.
 *
 * Ordered as the classes are in HeroClass rather than alphabetically, so the sheet reads the same
 * way the character-select list does.
 */
constexpr SpellID ClassSkills[ClassSkillCount] = {
	SpellID::ItemRepair,    // Warrior / Paladin
	SpellID::TrapDisarm,    // Rogue
	SpellID::StaffRecharge, // Sorcerer
	SpellID::Search,        // Monk
	SpellID::Identify,      // Bard
	SpellID::Rage,          // Barbarian
};

/** @brief True if @p spell is one of the six. */
bool IsClassSkill(SpellID spell);

/**
 * @brief The _pAblSpells mask granting all six.
 *
 * Replaces the single-skill mask both CreatePlayer and InitPlayer used to build. Computed rather
 * than written as a literal so adding a seventh here needs nothing else changed.
 */
uint64_t AllClassSkillsBitmask();

} // namespace oracool
} // namespace devilution
