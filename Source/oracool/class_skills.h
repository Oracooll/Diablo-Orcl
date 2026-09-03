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

struct Player;

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
SpellMask AllClassSkillsBitmask();

/**
 * @brief Everything @p player has WITHOUT learning it: the six class skills, plus Charge once its
 * level gate opens.
 *
 * This is what `_pAblSpells` should be set to. Charge is neither a class skill nor a book spell, but
 * it has to live in a mask to be selectable at all - the speedbook and the skill wells list what the
 * masks contain - so it is folded in here rather than becoming a third mechanism.
 */
SpellMask InnateSpellsBitmask(const Player &player);

/**
 * @brief Recomputes `_pAblSpells` and releases any button or hotkey now holding a lost skill.
 *
 * Call after anything that changes what the character HAS: a level, a refund, a point spent. The
 * mask alone is not enough - a readied slot keeps its SpellID, so a skill refunded to zero would go
 * on being drawn in the well and cast from it (user, 2026-08-27).
 */
void RefreshInnateSpells(Player &player);

} // namespace oracool
} // namespace devilution
