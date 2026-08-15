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
// Zeal's and Charge's numbers are the user's (2026-08-15), not derived from anything: Zeal at 6 for
// 2 mana a hit, Charge at 12 for 10 a use. The shape of that is worth keeping visible - Zeal is the
// cheap thing you lean on constantly and Charge the expensive one you open with, which is why Zeal's
// cost is charged per swing and Charge's per launch.
//
// The other five arrived the same day as art plus one line of description each, with no numbers
// attached, so THEIR levels and costs are placeholders I chose - see IsPaladinSkillImplemented. They
// are a ladder rather than five guesses: the two shield moves bracket the melee ones, and the two
// that call something down from outside the Paladin's own reach sit at the top. Each is one edit to
// change, and none of them is load-bearing until the skill has mechanics to gate.
//
//   Zeal 6 | Shield Bash 8 | Hammer of Faith 10 | Charge 12 | Blessed Hammer 16 |
//   Blessed Shield 20 | Fist of the Heavens 24
constexpr std::array<PaladinSkillData, PaladinSkillCount> Skills { {
	{ N_("Charge"), N_("Charges at enemies delivering a deadly blow."), 12, 10 },
	{ N_("Zeal"), N_("Hits up to five adjacent enemies in a rapid succession."), 6, 2 },
	{ N_("Hammer of Faith"), N_("A splash damage melee attack."), 10, 5 },
	{ N_("Blessed Shield"), N_("Hurl a blessed shield at a crowd of enemies to eradicate them."), 20, 10 },
	{ N_("Fist of the Heavens"),
	    N_("A divine fist descends from the sky, causing splash damage to enemies nearby."), 24, 15 },
	{ N_("Shield Bash"), N_("Bash an enemy with your shield, stunning them in the process."), 8, 3 },
	{ N_("Blessed Hammer"),
	    N_("A divine hammer spirals outward from you, hurting every enemy it touches."), 16, 8 },
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

bool IsPaladinSkillImplemented(PaladinSkill skill)
{
	// Charge rides oracool/furious_charge.cpp and Zeal oracool/warrior_splash.cpp. The five added on
	// 2026-08-15 have no mechanics module of their own yet.
	return skill == PaladinSkill::Charge || skill == PaladinSkill::Zeal;
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
