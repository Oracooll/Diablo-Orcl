#include "oracool/class_skills.h"

#include "oracool/paladin_skills.h"
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

uint64_t AllClassSkillsBitmask()
{
	uint64_t mask = 0;
	for (const SpellID skill : ClassSkills)
		mask |= GetSpellBitmask(skill);
	return mask;
}

uint64_t InnateSpellsBitmask(const Player &player)
{
	uint64_t mask = AllClassSkillsBitmask();
	// Oracool: the Paladin's skills are not class skills and not book spells - they are earned by
	// character level - but each still has to be IN a mask to be selectable, because the speedbook,
	// the Abilities window and the skill wells all list what the masks say the player has. So they
	// are granted here rather than being a fourth kind of thing.
	//
	// Recomputed on every call, and the callers run at creation, on load AND on level-up, which is
	// what makes each grant appear the moment its level is reached rather than on the next reload.
	for (size_t i = 0; i < PaladinSkillCount; i++) {
		const auto skill = static_cast<PaladinSkill>(i);
		if (IsPaladinSkillUnlocked(player, skill))
			mask |= GetSpellBitmask(GetPaladinSkillData(skill).spellId);
	}
	return mask;
}

} // namespace oracool
} // namespace devilution
