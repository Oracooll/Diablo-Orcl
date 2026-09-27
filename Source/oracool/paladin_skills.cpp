#include "oracool/paladin_skills.h"

#include <array>

#include "cursor.h" // pcursmonst - what the targeting rule is measured against
#include "engine/backbuffer_state.hpp"
#include "monster.h"
#include "oracool/oracool.h"
#include "missiles.h"
#include "oracool/passives.h"
#include "player.h"
#include "spells.h"
#include "utils/language.h"
#include <fmt/format.h>
#include "oracool/paladin_melee.h"
#include "oracool/paladin_ranged.h"
#include "oracool/furious_charge.h"

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
//   Zeal 6 | Shield Bash 8 | Hammer of Faith 12 | Charge 12 | Blessed Shield 18 |
//   Blessed Hammer 18 | Fist of the Heavens 30
//
// ALL SEVEN now appear on the class tree's Combat Skills page, where the row's TIER carries a level
// of its own (oracool::ClassTreeTierMinLevel: 1, 6, 12, 18, 24, 30, 36). That gave each skill two
// level gates, and the window showed whichever one the sheet you happened to be looking at asked.
// IsClassTreeSkillUnlocked defers to this table now, which settles it - and four of the placeholder
// levels moved onto their tiers so that deferral costs nothing rather than leaving the tree quietly
// stricter than the skill:
//
//   Blessed Hammer      16 -> 18   (tier 3)   2026-08-16
//   Fist of the Heavens 24 -> 30   (tier 5)   2026-08-16
//   Hammer of Faith     10 -> 12   (tier 2)   2026-08-16, when it was given a tree row
//   Blessed Shield      20 -> 18   (tier 3)   2026-08-16, when it was given a tree row
//
// Each was moved to the NEAREST tier level, so no skill travelled further than it had to.
//
// The last two followed on 2026-08-26, reported from play: "there is a bug with Smite skill - is it
// requiring lvl 8 for some reason?!"
//
//   Shield Bash (Smite)  8 -> 1    (tier 0)
//   Charge              12 -> 6    (tier 1)
//
// Leaving those two "stricter than their tier" had been a deliberate choice, and it was the wrong
// one. The tier is not an internal detail: it is the row's POSITION on the Combat Skills page, and
// the page tells the player in so many words that the top tier opens at level 1. A skill sitting
// there and refusing until level 8 does not read as balance, it reads as broken - which is exactly
// how it was reported.
//
// So the tier is now the only level gate any of these skills has, and this table agrees with it
// rather than competing. EveryBorrowedPaladinSkillMatchesItsTreeTier pins that in the test suite,
// because two tables agreeing today is not the same as two tables that cannot drift.
//
// The shield requirement is untouched: that is a real condition the player controls, checked
// alongside the level and shown the same way.
//
// The range column is the user's targeting rule made per-skill (2026-08-15): a click on a monster
// further away than this is a MOVE, not a cast. Melee skills take MeleeSkillRangeTiles; the four
// that strike at a distance take the full MaxSkillRangeTiles, except Charge, which is deliberately
// shorter - it closes the gap on foot, and a dash from the far edge of the screen would read as a
// teleport rather than as a charge.
constexpr std::array<PaladinSkillData, PaladinSkillCount> Skills { {
	{ N_("Charge"), N_("Charges at enemies delivering a deadly blow, +20% damage per level."), SpellID::Charge, 8, false, 6, 10 },
	// The ladder is SKILL levels now (user, 2026-08-30) - strikes at 1, 3 and 5, accuracy at every
	// level and onwards alone. Says "skill level" out loud because the same numbers read as
	// character levels 6, 8 and 10, and a player looking at the wrong one would think it lied.
	{ N_("Zeal"), N_("Strikes up to four times in the time of one swing, spread across nearby enemies. Skill levels 1, 3 and 5 each add a strike, and every skill level adds +1% chance to hit."),
	    SpellID::Zeal, MeleeSkillRangeTiles, false, 6, 1 },
	{ N_("Hammer of Faith"), N_("A splash damage melee attack: half the blow to everything around the target, +2% per level."), SpellID::HammerOfFaith,
	    MeleeSkillRangeTiles, false, 12, 5 },
	{ N_("Blessed Shield"), N_("Hurl a blessed shield at a crowd of enemies to eradicate them."),
	    SpellID::BlessedShield, MaxSkillRangeTiles, true, 18, 10 },
	{ N_("Fist of the Heavens"),
	    N_("A divine fist descends from the sky, causing splash damage to enemies nearby."),
	    SpellID::FistOfTheHeavens, MaxSkillRangeTiles, false, 30, 15 },
	{ N_("Shield Bash"), N_("Bash an enemy with your shield, +15% damage per level, stunning them in the process."),
	    SpellID::ShieldBash, MeleeSkillRangeTiles, true, 1, 3 },
	{ N_("Blessed Hammer"),
	    N_("A divine hammer spirals outward from you, hurting every enemy it touches."),
	    SpellID::BlessedHammer, MaxSkillRangeTiles, false, 18, 8 },
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
	// A BROKEN shield does not count, which is the rule everywhere else in the game: a broken item
	// is left equipped rather than destroyed (BreakOrRemoveEquipment) and contributes nothing at
	// all - CalcPlrItemVals clears its _iStatFlag before a single bonus is added, so it grants no
	// armour and no block.
	//
	// This asked only about the item's TYPE, so Shield Bash and Blessed Shield stayed available on
	// a shield that had stopped being one in every way that matters (external audit, 2026-08-25).
	// The two skills are ABOUT the shield; a shield giving nothing should not power them.
	const auto usableShield = [](const Item &item) {
		return item._itype == ItemType::Shield && !item._iOracoolBroken;
	};
	return usableShield(player.InvBody[INVLOC_HAND_LEFT])
	    || usableShield(player.InvBody[INVLOC_HAND_RIGHT]);
}

bool IsPaladinSkillUnlocked(const Player &player, PaladinSkill skill)
{
	if (!ClassHasPaladinSkills(player))
		return false;
	// Level only. The shield is a USE requirement, checked in CanUsePaladinSkill (dev note, 2026-09-27:
	// "skill that require shield to operate must not require shield to level up, only to operate"), so
	// Smite and Blessed Shield take points and sit on a button without one, and refuse until it is held.
	return player._pLevel >= GetPaladinSkillData(skill).minLevel;
}

bool CanUsePaladinSkill(const Player &player, PaladinSkill skill)
{
	if (!IsSinglePlayer() || !IsPaladinSkillUnlocked(player, skill))
		return false;
	if (GetPaladinSkillData(skill).requiresShield && !HasShieldEquipped(player))
		return false;
	return player._pMana >= ManaCostFixedPoint(skill);
}

bool MissilePoolHasRoom()
{
	// Audit finding, 2026-08-26: the three ranged Paladin skills spent mana and THEN called
	// AddMissile, discarding its result. AddMissile returns nullptr when the pool is full, so a
	// busy screen - the exact moment a player reaches for Fist of the Heavens - took the mana
	// and cast nothing. Asked before the mana is spent instead.
	return Missiles.size() < Missiles.max_size();
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
	oracool::OnPassiveManaSpent(player, cost);
	RedrawComponent(PanelDrawComponent::Mana);
	return true;
}

std::string PaladinSkillFactsAt(PaladinSkill skill, int rank)
{
	// Range and the shield gate from this table; what the blow does from the module that lands it.
	const PaladinSkillData &data = GetPaladinSkillData(skill);
	std::string out;
	const auto line = [&out](const std::string &s) {
		if (!out.empty())
			out += '\n';
		out += s;
	};
	if (data.rangeTiles <= 1)
		line(std::string(_("Range: melee")));
	else
		line(fmt::format(fmt::runtime(_("Range: {:d} tiles")), data.rangeTiles));
	if (data.requiresShield)
		line(std::string(_("Requires a shield")));
	const std::string melee = PaladinMeleeFactsAt(skill, rank);
	if (!melee.empty())
		line(melee);
	const std::string ranged = PaladinRangedFactsAt(skill, rank);
	if (!ranged.empty())
		line(ranged);
	if (skill == PaladinSkill::Charge)
		line(FuriousChargeFacts(rank));
	return out;
}

} // namespace oracool
} // namespace devilution
