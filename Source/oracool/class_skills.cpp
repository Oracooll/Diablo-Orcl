#include "oracool/class_skills.h"

#include "oracool/class_tree.h"
#include "oracool/paladin_skills.h"
#include "panels/spell_book.hpp" // AbilityFKeyCount
#include "player.h"
#include "spells.h"

namespace devilution {
namespace oracool {

bool IsClassSkill(SpellID spell)
{
	for (const SpellID skill : ClassSkills) {
		if (skill == spell)
			return true;
	}
	return false;
}

SpellMask AllClassSkillsBitmask()
{
	SpellMask mask;
	for (const SpellID skill : ClassSkills)
		mask |= GetSpellBitmask(skill);
	return mask;
}

SpellMask InnateSpellsBitmask(const Player &player)
{
	// The six vanilla class skills are NOT granted (user, 2026-08-19: "i created a new paladin hero
	// and he spawned with Repair Skill assigned to RMB. i told you to retire the vanilla skill from
	// every possible appearance in the game. for all hero classes").
	//
	// They were handed to every class from birth back when the Abilities window had a Class Skills
	// sheet to list them on. That sheet is gone - oracool/class_tree is the skill system now - so all
	// the grant did was put Item Repair on a new character's right button and leave six vanilla rows
	// in the speedbook. AllClassSkillsBitmask is still exported for the debug "give me everything"
	// command, which is a different question from what a character owns.
	//
	// Nothing else breaks: repairing, identifying, recharging and disarming are town services, and
	// the tree carries every skill a class is meant to have.
	SpellMask mask;
	// Oracool: the Paladin's skills are not class skills and not book spells - they are earned by
	// character level - but each still has to be IN a mask to be selectable, because the speedbook,
	// the Abilities window and the skill wells all list what the masks say the player has. So they
	// are granted here rather than being a fourth kind of thing.
	//
	// Recomputed on every call, and the callers run at creation, on load AND on level-up, which is
	// what makes each grant appear the moment its level is reached rather than on the next reload.
	//
	// UNLOCKED IS NOT ENOUGH: the tree row must also have a point in it (user, 2026-08-27, on
	// finding Shield Bash still listed after refunding its only point - "there is a gold background
	// next to TP spell icon [...] It reads Shield Bash! Why? Makes no sense").
	//
	// IsPaladinSkillUnlocked answers level (the shield is a use check since 2026-09-27). It has
	// never known about the class tree, so a skill was in this mask from the moment its level gate
	// opened whether or not the player had spent anything on it - and the mask is what the speedbook,
	// the quick list and the wells all read as "you have this".
	//
	// The symptom was invisible until two things changed on the same day: Smite moved to level 1
	// (v1.9.65), so it entered the mask at character creation rather than at 8, and the quick list
	// lists a masked skill under SPELLS when the tree section has skipped it for having no points.
	// It then drew with no icon, because a tree skill has no vanilla spell art to fall back on.
	//
	// A zero-point row is exactly the state the Red plate already describes: "earned and spendable,
	// but nothing invested yet - so the skill exists and does nothing". Being in this mask is the
	// difference between existing and doing something.
	for (size_t i = 0; i < PaladinSkillCount; i++) {
		const auto skill = static_cast<PaladinSkill>(i);
		if (!IsPaladinSkillUnlocked(player, skill))
			continue;
		const SpellID spellId = GetPaladinSkillData(skill).spellId;
		// A skill with no tree row for this class keeps the old rule - the row is what carries the
		// investment, so where there is none there is nothing extra to ask.
		const ClassTreeSkill row = ClassTreeSkillForSpell(player._pClass, spellId);
		if (row != ClassTreeSkill::None && ClassTreeInvestment(player, row) <= 0)
			continue;
		mask |= GetSpellBitmask(spellId);
	}

	// Oracool audit (2026-09-03): AND EVERY OTHER CASTABLE TREE ROW, by the same rule. The nine
	// rounds of the inert-skill plan gave the Sorceress, Rogue, Barbarian, Bard and Monk some
	// ninety rows with a SpellID - the cold page, the bow and melee pages, the cries, the javelins -
	// and every one of them was selectable only if it sat in a mask, and nothing put it in one.
	// The Paladin's seven were granted above because they came first; the rule was never widened.
	// So a Barbarian could invest in Bash, read its sentence in the Abilities window, and never
	// ready it: the picker, the speedbook and the wells all list what the masks say.
	//
	// The same three conditions as the Paladin's: the row is this class's, is unlocked, and has a
	// point in it. Plus one: a row that rides a BOOK spell (Charm on Berserk, Sonic Barrier on Mana
	// Shield) is retired from investment and learned from the book instead, so it is not innate.
	for (size_t i = 0; i < ClassTreeSkillCount; i++) {
		const auto row = static_cast<ClassTreeSkill>(i);
		const ClassTreeSkillData &data = GetClassTreeSkillData(row);
		if (data.heroClass != player._pClass || !data.implemented || data.kind != ClassTreeKind::Active)
			continue;
		const SpellID spellId = ClassTreeSpellId(row); // the accessor, which also answers for borrowed rows
		if (spellId == SpellID::Invalid || IsClassTreeRowRetiredAsSpell(row))
			continue;
		if (!IsClassTreeSkillUnlocked(player, row) || ClassTreeInvestment(player, row) <= 0)
			continue;
		mask |= GetSpellBitmask(spellId);
	}
	return mask;
}

void RefreshInnateSpells(Player &player)
{
	player._pAblSpells = InnateSpellsBitmask(player);

	// A slot still holding a skill the character no longer has must be let go, or the well draws it
	// and a click tries to cast it (user, 2026-08-27: "the smite icons remains on rmb slot. it need
	// to disappear and be replaced with something else like regular/fist attack. to apply for all
	// skill if their skill points are removed completely").
	//
	// Cleared to Invalid rather than to a named attack: Invalid IS the basic attack on both buttons -
	// see attack_skills.h - so the well falls back to the fist or the sword by itself, and this does
	// not have to know which of the two the player last chose.
	const auto clearIfLost = [&player](SpellID &spell, SpellType &type) {
		if (type == SpellType::Skill && IsValidSpell(spell)
		    && (player._pAblSpells & GetSpellBitmask(spell)) == 0) {
			spell = SpellID::Invalid;
			type = SpellType::Invalid;
		}
	};
	clearIfLost(player._pRSpell, player._pRSplType);
	clearIfLost(player._pLRSpell, player._pLRSplType);
	// The F-key bindings too. A hotkey pointing at a refunded skill is the same fault one keystroke
	// further away, and it survives into the save.
	// All twelve slots, not the eight the Abilities window shows: Quick Cast reads 0-11 (round 6 audit, v1.12.231).
	for (size_t i = 0; i < NumHotkeys; i++) {
		clearIfLost(player._pSplHotKey[i], player._pSplTHotKey[i]);
		clearIfLost(player._pSplLHotKey[i], player._pSplLTHotKey[i]);
	}
}

} // namespace oracool
} // namespace devilution
