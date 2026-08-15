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
	// Oracool: Charge is not a class skill and not a book spell - it is earned at character level 12
	// - but it still has to be IN a mask to be selectable, because the speedbook and the skill wells
	// list what the masks say the player has. So it is granted here rather than being a fourth kind
	// of thing.
	//
	// Recomputed on every call, and the callers run at creation, on load AND on level-up, which is
	// what makes the grant appear the moment level 12 is reached rather than on the next reload.
	if (IsPaladinSkillUnlocked(player, PaladinSkill::Charge))
		mask |= GetSpellBitmask(SpellID::Charge);
	return mask;
}

} // namespace oracool
} // namespace devilution
