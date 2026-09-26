#include "oracool/skill_facts.h"

#include <algorithm>

#include <fmt/format.h>

#include "missiles.h" // GetDamageAmtAtLevel, the book spells' per-level helpers
#include "monster.h"  // GolemStatsAt
#include "player.h"   // ManaShieldDamageReductionAtLevel
#include "spells.h"   // IsValidSpell, ChargedBoltCount
#include "oracool/cold.h"
#include "oracool/passives.h"     // PassiveRuneLevelBonus
#include "oracool/skill_points.h" // SpellHasBook
#include "oracool/melee_skills.h"
#include "oracool/paladin_skills.h"
#include "oracool/rage.h" // SkillResourceLine
#include "oracool/rfa12_actives.h"
#include "oracool/rogue_arrows.h"
#include "oracool/warcries.h"
#include "utils/language.h"

namespace devilution::oracool {

namespace {

void AddLine(std::string &out, const std::string &s)
{
	if (s.empty())
		return;
	if (!out.empty())
		out += '\n';
	out += s;
}

/** The game clock every missile duration is counted on. */
constexpr double TicksPerSecond = 20.0;

std::string DurationLine(int ticks)
{
	return fmt::format(fmt::runtime(_("Duration: {:.1f} s")), ticks / TicksPerSecond);
}

} // namespace

std::string SkillFactsAt(SpellID spell, int rank)
{
	if (!IsValidSpell(spell))
		return {};
	std::string out;
	const auto add = [&out](const std::string &s) { AddLine(out, s); };
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

std::string SkillFactsAt(const Player &player, SpellID spell, int rank)
{
	if (!IsValidSpell(spell))
		return {};
	std::string out = SkillFactsAt(spell, rank);
	if (IsRfa12Active(spell))
		AddLine(out, Rfa12ActiveFactsAt(player, spell, rank)); // routes the Necromancer's curses and summons too
	AddLine(out, BookSpellFactsAt(player, spell, rank));
	return out;
}

std::string BookSpellFactsAt(const Player &player, SpellID spell, int level)
{
	// Book spells only: a class skill or an RfA-12 active answers in its own module above.
	if (!SpellHasBook(spell))
		return {};
	// Every number below is the cast's own: each helper is what the missile (missiles.cpp),
	// CastSpell (spells.cpp), SpawnGolem (monster.cpp) or the damage path (player.cpp) calls.
	const int sl = std::max(level, 0);
	const int clvl = player._pLevel;
	std::string out;
	const auto add = [&out](const std::string &s) { AddLine(out, s); };
	const auto lingerLine = [](int spellLevel) {
		return fmt::format(fmt::runtime(_("Each strike lingers {:.2f} s")), LightningLingerTicks(spellLevel) / TicksPerSecond);
	};
	switch (spell) {
	case SpellID::Lightning:
		add(lingerLine(sl));
		break;
	case SpellID::ChainLightning:
		add(fmt::format(fmt::runtime(_("Leaps within {:d} tiles")), ChainLightningLeapRadius(sl)));
		add(lingerLine(sl)); // its bolts are Lightning's, at the same level
		break;
	case SpellID::ChargedBolt:
		add(fmt::format(fmt::runtime(_("Bolts: {:d}")), ChargedBoltCount(sl)));
		break;
	case SpellID::FireWall:
	case SpellID::RingOfFire: // its flames are Fire Wall segments at the ring's level
		add(DurationLine(FireWallDurationTicks(sl)));
		break;
	case SpellID::LightningWall:
		add(DurationLine(LightningWallDurationTicks(sl)));
		break;
	case SpellID::FlameWave:
		add(fmt::format(fmt::runtime(_("Width: {:d} tiles")), 2 * FlameWaveSideTiles(sl) + 1));
		break;
	case SpellID::HolyBolt:
		add(fmt::format(fmt::runtime(_("Bolt speed: {:d}%")), HolyBoltSpeedAtLevel(sl) * 100 / HolyBoltSpeedAtLevel(0)));
		break;
	case SpellID::Golem: {
		// The golem's blow is the Damage line (GetDamageAmtAtLevel reads the same GolemStatsAt).
		const GolemStats golem = GolemStatsAt(player, sl);
		add(fmt::format(fmt::runtime(_("Golem life: {:d}")), golem.maxHitPoints >> 6));
		add(fmt::format(fmt::runtime(_("Golem to-hit: {:d}")), golem.toHit));
	} break;
	case SpellID::Guardian:
		add(DurationLine(GuardianDurationTicks(sl, clvl)));
		break;
	case SpellID::StoneCurse:
		add(DurationLine(StoneCurseDurationTicks(sl)));
		break;
	case SpellID::RuneOfStone: // a Stone Curse at the rune's level, which Custom Engineering raises
		add(DurationLine(StoneCurseDurationTicks(sl + PassiveRuneLevelBonus(player))));
		break;
	case SpellID::RuneOfLight: // one Lightning Wall segment at the rune's level
		add(DurationLine(LightningWallDurationTicks(sl + PassiveRuneLevelBonus(player))));
		break;
	case SpellID::RuneOfFire:
	case SpellID::RuneOfNova:
	case SpellID::RuneOfImmolation:
		if (const int bonus = PassiveRuneLevelBonus(player); bonus > 0)
			add(fmt::format(fmt::runtime(_("The set rune strikes at spell level {:d}")), sl + bonus));
		break;
	case SpellID::Infravision:
		add(DurationLine(InfravisionDurationTicks(sl)));
		break;
	case SpellID::Etherealize:
		add(DurationLine(EtherealizeDurationTicks(sl)));
		break;
	case SpellID::Search:
		add(DurationLine(SearchDurationTicks(sl, clvl)));
		break;
	case SpellID::ManaShield:
		if (sl > 0) {
			const int divisor = ManaShieldDamageReductionAtLevel(sl);
			add(fmt::format(fmt::runtime(_("Blows reduced by 1/{:d} ({:d}%) before mana absorbs them")), divisor, 100 / divisor));
		}
		break;
	case SpellID::Mana: {
		int minAmount;
		int maxAmount;
		ManaSpellAmountRange(player, sl, minAmount, maxAmount);
		add(fmt::format(fmt::runtime(_("Restores: {:d} - {:d} mana")), minAmount, maxAmount));
	} break;
	case SpellID::Reflect:
		add(fmt::format(fmt::runtime(_("Charges: {:d}")), ReflectCharges(sl, clvl)));
		break;
	case SpellID::Berserk:
		add(fmt::format(fmt::runtime(_("Turned monster deals 120-129% damage, +{:d}")), BerserkDamageBonus(sl)));
		break;
	default:
		break;
	}
	return out;
}

std::string SpellLevelLines(const Player &player, SpellID spell, int level)
{
	std::string text;
	const auto add = [&text](const std::string &s) { AddLine(text, s); };
	// Mana FALLS as a spell levels - the adjustment is subtracted - so the next block's mana is a
	// reason to spend a point rather than a price for it, and D2 quotes it for the same reason.
	add(SkillResourceLine(player, spell, level)); // Mana Cost, or the Barbarian's Rage Cost / Generates
	if (spell == SpellID::BoneSpirit) {
		add(std::string(_("Damage: 1/3 of target's health")));
	} else {
		const bool heals = spell == SpellID::Healing || spell == SpellID::HealOther;
		int min = -1;
		int max = -1;
		GetDamageAmtAtLevel(spell, level, &min, &max);
		if (min != -1)
			add(fmt::format(fmt::runtime(heals ? _("Heals: {:d} - {:d}") : _("Damage: {:d} - {:d}")), min, max));
	}
	add(SkillFactsAt(player, spell, level)); // a cold spell's freeze, an arrow's count, a curse's duration
	return text;
}

} // namespace devilution::oracool
