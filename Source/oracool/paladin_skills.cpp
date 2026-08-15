#include "oracool/paladin_skills.h"

#include <array>

#include "cursor.h" // pcursmonst - what the targeting rule is measured against
#include "engine/backbuffer_state.hpp"
#include "monster.h"
#include "oracool/oracool.h"
#include "player.h"
#include "spells.h"
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
//
// The range column is the user's targeting rule made per-skill (2026-08-15): a click on a monster
// further away than this is a MOVE, not a cast. Melee skills take MeleeSkillRangeTiles; the four
// that strike at a distance take the full MaxSkillRangeTiles, except Charge, which is deliberately
// shorter - it closes the gap on foot, and a dash from the far edge of the screen would read as a
// teleport rather than as a charge.
constexpr std::array<PaladinSkillData, PaladinSkillCount> Skills { {
	{ N_("Charge"), N_("Charges at enemies delivering a deadly blow."), SpellID::Charge, 8, false, 12, 10 },
	{ N_("Zeal"), N_("Strikes up to five times in the time of one swing, spread across nearby enemies."),
	    SpellID::Zeal, MeleeSkillRangeTiles, false, 6, 1 },
	{ N_("Hammer of Faith"), N_("A splash damage melee attack."), SpellID::HammerOfFaith,
	    MeleeSkillRangeTiles, false, 10, 5 },
	{ N_("Blessed Shield"), N_("Hurl a blessed shield at a crowd of enemies to eradicate them."),
	    SpellID::BlessedShield, MaxSkillRangeTiles, true, 20, 10 },
	{ N_("Fist of the Heavens"),
	    N_("A divine fist descends from the sky, causing splash damage to enemies nearby."),
	    SpellID::FistOfTheHeavens, MaxSkillRangeTiles, false, 24, 15 },
	{ N_("Shield Bash"), N_("Bash an enemy with your shield, stunning them in the process."),
	    SpellID::ShieldBash, MeleeSkillRangeTiles, true, 8, 3 },
	{ N_("Blessed Hammer"),
	    N_("A divine hammer spirals outward from you, hurting every enemy it touches."),
	    SpellID::BlessedHammer, MaxSkillRangeTiles, false, 16, 8 },
} };
static_assert([] {
	for (const PaladinSkillData &skill : Skills) {
		if (skill.rangeTiles < 1 || skill.rangeTiles > MaxSkillRangeTiles)
			return false;
	}
	return true;
}(),
    "a skill's range is outside 1..MaxSkillRangeTiles - see the 640x480 derivation in paladin_skills.h");
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

std::optional<PaladinSkill> PaladinSkillForSpell(SpellID spell)
{
	if (!IsValidSpell(spell))
		return std::nullopt;
	for (size_t i = 0; i < Skills.size(); i++) {
		if (Skills[i].spellId == spell)
			return static_cast<PaladinSkill>(i);
	}
	return std::nullopt;
}

bool IsPaladinSkillTargetInRange(const Player &player, PaladinSkill skill)
{
	if (pcursmonst == -1)
		return false;
	const Monster &monster = Monsters[pcursmonst];
	return player.position.tile.WalkingDistance(monster.position.tile) <= GetPaladinSkillData(skill).rangeTiles;
}

bool IsPaladinSkillImplemented(PaladinSkill skill)
{
	// All seven, as of 2026-08-15. Charge rides oracool/furious_charge.cpp; Zeal, Hammer of Faith and
	// Shield Bash ride the shared melee hook in oracool/paladin_melee.cpp; the other three are cast
	// from oracool/paladin_ranged.cpp.
	//
	// Kept rather than deleted now that it always returns true: it is the one place that states which
	// skills have mechanics, and the next skill added will start out without any.
	(void)skill;
	return true;
}

bool ClassHasPaladinSkills(const Player &player)
{
	// HeroClass::Warrior, NOT a HeroClass::Paladin - there isn't one. See this file's header, and
	// the identical note in auras.cpp: reading the class by its DISPLAYED name compiles everywhere
	// and silently never matches.
	return player._pClass == HeroClass::Warrior;
}

bool HasShieldEquipped(const Player &player)
{
	// Oracool bug fix (2026-08-15): BOTH hands. This used to read INVLOC_HAND_RIGHT alone, which is
	// where a shield USUALLY ends up but not where it must be: shields are ILOC_ONEHAND (itemdat.cpp
	// - Buckler, Small Shield, Large Shield and every tier above them), and CheckInvPaste's
	// ILOC_ONEHAND case puts a one-handed item in whichever hand slot it was dropped on. Drop a
	// shield on the left slot and it stays there.
	//
	// The engine's own armour-class and block-chance code has always checked both hands
	// (items.cpp's CalcPlrItemVals) - this was the odd one out, and the cost of being wrong was
	// silent: Shield Bash and Blessed Shield are gated on this, so both simply vanished from the
	// Abilities window for a player whose shield happened to sit on the left.
	return player.InvBody[INVLOC_HAND_LEFT]._itype == ItemType::Shield
	    || player.InvBody[INVLOC_HAND_RIGHT]._itype == ItemType::Shield;
}

bool IsPaladinSkillUnlocked(const Player &player, PaladinSkill skill)
{
	if (!ClassHasPaladinSkills(player))
		return false;
	const PaladinSkillData &data = GetPaladinSkillData(skill);
	// The shield sits alongside the level gate rather than being checked at the cast, so a skill that
	// cannot be used is GREY and inert on the sheet whichever requirement is unmet - the player is
	// told before they click, not after (user rule, 2026-08-15: "else - skill is inactivated").
	if (data.requiresShield && !HasShieldEquipped(player))
		return false;
	return player._pLevel >= data.minLevel;
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
