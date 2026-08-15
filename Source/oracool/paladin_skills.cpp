#include "oracool/paladin_skills.h"

#include <array>

#include "engine/backbuffer_state.hpp"
#include "oracool/oracool.h"
#include "player.h"
#include "utils/language.h"

namespace devilution {
namespace oracool {

namespace {

// Order must match the PaladinSkill enum, which in turn matches ui\paladin_skill_icons.png.
//
// Levels and costs are the user's (2026-08-15), not derived from anything: Zeal at 6 for 2 mana a
// hit, Charge at 12 for 10 a use. The shape of that is worth keeping visible - Zeal is the cheap
// thing you lean on constantly and Charge the expensive one you open with, which is why Zeal's cost
// is charged per swing and Charge's per launch.
constexpr std::array<PaladinSkillData, PaladinSkillCount> Skills { {
	{ N_("Charge"), N_("Charges at enemies delivering a deadly blow."), 12, 10 },
	{ N_("Zeal"), N_("Hits up to five adjacent enemies in a rapid succession."), 6, 2 },
} };
static_assert(Skills.size() == static_cast<size_t>(PaladinSkill::LAST) + 1,
    "a PaladinSkill was added without its data row - the two are indexed by each other");

/**
 * @brief Mana is stored in 1/64ths; the costs above are in whole points.
 *
 * Same shift GetManaAmount (spells.cpp) applies to every spell's sManaCost. Doing it here rather
 * than writing 640 and 128 in the table keeps the table readable as the numbers the user gave.
 */
constexpr int ManaFixedPointShift = 6;

int ManaCostFixedPoint(PaladinSkill skill)
{
	return GetPaladinSkillData(skill).manaCost << ManaFixedPointShift;
}

} // namespace

const PaladinSkillData &GetPaladinSkillData(PaladinSkill skill)
{
	const auto index = static_cast<size_t>(skill);
	return Skills[index < Skills.size() ? index : 0];
}

bool ClassHasPaladinSkills(const Player &player)
{
	// HeroClass::Warrior, NOT a HeroClass::Paladin - there isn't one. See this file's header, and
	// the identical note in auras.cpp: reading the class by its DISPLAYED name compiles everywhere
	// and silently never matches.
	return player._pClass == HeroClass::Warrior;
}

bool IsPaladinSkillUnlocked(const Player &player, PaladinSkill skill)
{
	if (!ClassHasPaladinSkills(player))
		return false;
	return player._pLevel >= GetPaladinSkillData(skill).minLevel;
}

bool CanUsePaladinSkill(const Player &player, PaladinSkill skill)
{
	if (!IsSinglePlayer() || !IsPaladinSkillUnlocked(player, skill))
		return false;
	return player._pMana >= ManaCostFixedPoint(skill);
}

bool SpendPaladinSkillMana(Player &player, PaladinSkill skill)
{
	// Re-checks rather than trusting the caller: this is the only place mana leaves the player for a
	// skill, so a caller that forgets CanUsePaladinSkill still cannot drive the orb negative.
	if (!CanUsePaladinSkill(player, skill))
		return false;

	const int cost = ManaCostFixedPoint(skill);
	// Both, exactly as CastSpell does (spells.cpp): _pMana is the current pool and _pManaBase the
	// unmodified one items adjust from. Moving only the first would have the difference reappear the
	// next time anything recalculated the character's stats.
	player._pMana -= cost;
	player._pManaBase -= cost;
	RedrawComponent(PanelDrawComponent::Mana);
	return true;
}

} // namespace oracool
} // namespace devilution
