#include "oracool/skill_facts.h"

#include "spells.h" // IsValidSpell
#include "oracool/cold.h"
#include "oracool/melee_skills.h"
#include "oracool/paladin_skills.h"
#include "oracool/rogue_arrows.h"
#include "oracool/warcries.h"

namespace devilution::oracool {

std::string SkillFactsAt(SpellID spell, int rank)
{
	if (!IsValidSpell(spell))
		return {};
	std::string out;
	const auto add = [&out](const std::string &s) {
		if (s.empty())
			return;
		if (!out.empty())
			out += '\n';
		out += s;
	};
	if (const std::optional<PaladinSkill> skill = PaladinSkillForSpell(spell); skill.has_value())
		add(PaladinSkillFactsAt(*skill, rank));
	if (const std::optional<ClassMeleeSkill> skill = ClassMeleeSkillForSpell(spell); skill.has_value())
		add(MeleeSkillFactsAt(*skill, rank));
	if (IsWarcry(spell))
		add(WarcryFactsAt(spell, rank));
	if (const std::optional<RogueArrow> arrow = RogueArrowForSpell(spell); arrow.has_value())
		add(RogueArrowFactsAt(*arrow, rank));
	if (IsColdSpell(spell))
		add(ColdSpellFactsAt(spell, rank));
	return out;
}

} // namespace devilution::oracool
