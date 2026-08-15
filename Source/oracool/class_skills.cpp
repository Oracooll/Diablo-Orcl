#include "oracool/class_skills.h"

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

} // namespace oracool
} // namespace devilution
