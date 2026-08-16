#include "oracool/paladin_tree.h"

#include <algorithm>
#include <array>
#include <cassert>

#include <fmt/format.h>

#include "engine/backbuffer_state.hpp"
#include "oracool/event_log.h"
#include "oracool/paladin_skills.h"
#include "oracool/skill_points.h"
#include "oracool/stat_sheet.h"
#include "player.h"
#include "utils/language.h"

namespace devilution::oracool {

namespace {

using Skill = PaladinTreeSkill;
using Page = PaladinTreePage;
using Kind = PaladinTreeKind;

// Diablo II's own tier requirements. Six rows per page, and every skill sits on one of them.
constexpr int TierLevels[] = { 1, 6, 12, 18, 24, 30 };

/**
 * @brief The tree, in icon-strip order.
 *
 * Descriptions state D2's effect first, then this engine's adaptation where the two differ. The
 * `implemented` flag is the honest one: false means the row is listed and described but contributes
 * nothing, which the UI shows rather than hides.
 */
const PaladinTreeSkillData Skills[PaladinTreeSkillCount] = {
	// ------------------------------ Combat Skills ------------------------------
	{ N_("Sacrifice"),
	    N_("Strike for heavy bonus damage and wound yourself for a share of it. Not yet built in this engine."),
	    Page::Combat, 0, 0, Kind::Active, SpellID::Invalid, false },
	{ N_("Smite"),
	    N_("Bash with your shield: it always connects and briefly stuns. A shield is mandatory."),
	    Page::Combat, 0, 1, Kind::Active, SpellID::Invalid, true },
	{ N_("Holy Bolt"),
	    N_("A bolt of holy energy that sears the undead. Uses this engine's own Holy Bolt, so invested points raise its spell level."),
	    Page::Combat, 0, 2, Kind::Active, SpellID::HolyBolt, true },
	{ N_("Zeal"),
	    N_("Strike several times in one furious burst. Each invested pair of points adds a strike, up to five."),
	    Page::Combat, 1, 0, Kind::Active, SpellID::Invalid, true },
	{ N_("Charge"),
	    N_("Rush an enemy and land a running blow."),
	    Page::Combat, 1, 1, Kind::Active, SpellID::Invalid, true },
	{ N_("Vengeance"),
	    N_("Adds fire, lightning and cold damage to your attack. Not yet built; this engine also has no cold damage."),
	    Page::Combat, 2, 0, Kind::Active, SpellID::Invalid, false },
	{ N_("Blessed Hammer"),
	    N_("Looses a spinning hammer that wheels outward through anything in its path."),
	    Page::Combat, 3, 2, Kind::Active, SpellID::Invalid, true },
	{ N_("Conversion"),
	    N_("Turns an enemy to your side for a time. Not yet built: this engine has no charmed-monster state."),
	    Page::Combat, 4, 1, Kind::Active, SpellID::Invalid, false },
	{ N_("Fist of the Heavens"),
	    N_("Calls down a bolt from the sky, which bursts into holy energy where it lands."),
	    Page::Combat, 5, 2, Kind::Active, SpellID::Invalid, true },

	// ----------------------------- Offensive Auras -----------------------------
	{ N_("Might"),
	    N_("Increases the damage you deal."),
	    Page::OffensiveAuras, 0, 0, Kind::Aura, SpellID::Invalid, true },
	{ N_("Holy Fire"),
	    N_("Wreathes your weapon in flame, adding fire damage to every blow."),
	    Page::OffensiveAuras, 1, 1, Kind::Aura, SpellID::Invalid, true },
	{ N_("Thorns"),
	    N_("Returns damage to whatever strikes you. This engine's thorns is a flat return, so points light it rather than growing it."),
	    Page::OffensiveAuras, 1, 2, Kind::Aura, SpellID::Invalid, true },
	{ N_("Blessed Aim"),
	    N_("Steadies your hand, raising your chance to hit."),
	    Page::OffensiveAuras, 2, 0, Kind::Aura, SpellID::Invalid, true },
	{ N_("Concentration"),
	    N_("Raises damage and steadies you against interruption, shrugging off the flinch when struck."),
	    Page::OffensiveAuras, 3, 0, Kind::Aura, SpellID::Invalid, true },
	{ N_("Holy Freeze"),
	    N_("Chills nearby enemies and adds cold damage. Inert here: this engine has no cold damage and no slow."),
	    Page::OffensiveAuras, 3, 1, Kind::Aura, SpellID::Invalid, false },
	{ N_("Holy Shock"),
	    N_("Charges your weapon, adding lightning damage to every blow."),
	    Page::OffensiveAuras, 4, 1, Kind::Aura, SpellID::Invalid, true },
	{ N_("Sanctuary"),
	    N_("Harms and repels nearby undead, ignoring their armour. Inert here: it needs the monster-facing pass."),
	    Page::OffensiveAuras, 4, 2, Kind::Aura, SpellID::Invalid, false },
	{ N_("Fanaticism"),
	    N_("Drives you to strike faster, harder and truer."),
	    Page::OffensiveAuras, 5, 0, Kind::Aura, SpellID::Invalid, true },
	{ N_("Conviction"),
	    N_("Strips nearby enemies of armour and resistance. Inert here: it needs the monster-facing pass."),
	    Page::OffensiveAuras, 5, 2, Kind::Aura, SpellID::Invalid, false },

	// ----------------------------- Defensive Auras -----------------------------
	{ N_("Prayer"),
	    N_("Mends your wounds steadily as you walk."),
	    Page::DefensiveAuras, 0, 0, Kind::Aura, SpellID::Invalid, true },
	{ N_("Resist Fire"),
	    N_("Hardens you against fire."),
	    Page::DefensiveAuras, 0, 1, Kind::Aura, SpellID::Invalid, true },
	{ N_("Defiance"),
	    N_("Raises your armour class."),
	    Page::DefensiveAuras, 1, 0, Kind::Aura, SpellID::Invalid, true },
	{ N_("Resist Cold"),
	    N_("Hardens you against cold. This engine has no cold, so it wards against magic instead - its third resistance."),
	    Page::DefensiveAuras, 1, 1, Kind::Aura, SpellID::Invalid, true },
	{ N_("Cleansing"),
	    N_("Shortens poison and curses. Inert here: this engine tracks no duration for either."),
	    Page::DefensiveAuras, 2, 2, Kind::Aura, SpellID::Invalid, false },
	{ N_("Resist Lightning"),
	    N_("Hardens you against lightning."),
	    Page::DefensiveAuras, 2, 1, Kind::Aura, SpellID::Invalid, true },
	{ N_("Vigor"),
	    N_("Quickens your stride: you run instead of walking, wherever you are, without the toggle."),
	    Page::DefensiveAuras, 3, 0, Kind::Aura, SpellID::Invalid, true },
	{ N_("Meditation"),
	    N_("Restores your mana steadily as you walk."),
	    Page::DefensiveAuras, 4, 0, Kind::Aura, SpellID::Invalid, true },
	{ N_("Redemption"),
	    N_("Consumes the fallen for life and mana. Inert here: it needs the corpse-handling pass."),
	    Page::DefensiveAuras, 5, 1, Kind::Aura, SpellID::Invalid, false },
	{ N_("Salvation"),
	    N_("Wards you against fire, lightning and magic alike."),
	    Page::DefensiveAuras, 5, 2, Kind::Aura, SpellID::Invalid, true },
};

/** @brief An aura's index into Player::_pPaladinAuraInvestment, or -1 if it is not an aura. */
int AuraSlot(Skill skill)
{
	if (skill < Skill::FIRST_AURA || skill > Skill::LAST)
		return -1;
	return static_cast<int>(skill) - static_cast<int>(Skill::FIRST_AURA);
}

/**
 * @brief The shape every scaled aura uses: the first point buys @p base, each one after adds
 * @p perPoint. Returns 0 for an aura with nothing invested, which is what makes an unpaid aura
 * unlightable rather than a free buff.
 */
int Scaled(int points, int base, int perPoint)
{
	if (points <= 0)
		return 0;
	return base + perPoint * (points - 1);
}

} // namespace

const PaladinTreeSkillData &GetPaladinTreeSkillData(Skill skill)
{
	const auto index = static_cast<size_t>(skill);
	assert(index < PaladinTreeSkillCount);
	return Skills[index];
}

SpellID PaladinTreeSpellId(Skill skill)
{
	// Resolved here rather than stored in the table: five of these slots are owned by
	// oracool/paladin_skills.h, and reading that module's table during THIS one's static
	// initialisation would be an initialisation-order gamble across translation units. Asking at
	// call time also means a slot that moves there moves here with no edit.
	switch (skill) {
	case Skill::Smite:
		return GetPaladinSkillData(PaladinSkill::ShieldBash).spellId;
	case Skill::Zeal:
		return GetPaladinSkillData(PaladinSkill::Zeal).spellId;
	case Skill::Charge:
		return GetPaladinSkillData(PaladinSkill::Charge).spellId;
	case Skill::BlessedHammer:
		return GetPaladinSkillData(PaladinSkill::BlessedHammer).spellId;
	case Skill::FistOfTheHeavens:
		return GetPaladinSkillData(PaladinSkill::FistOfTheHeavens).spellId;
	default:
		// Holy Bolt names an engine spell directly; everything else has no slot yet.
		return skill > Skill::LAST ? SpellID::Invalid : Skills[static_cast<size_t>(skill)].spellId;
	}
}

int PaladinTreeTierMinLevel(int tier)
{
	if (tier < 0 || tier >= static_cast<int>(std::size(TierLevels)))
		return 1;
	return TierLevels[tier];
}

string_view GetPaladinTreePageName(Page page)
{
	switch (page) {
	case Page::Combat:
		return _("Combat Skills");
	case Page::OffensiveAuras:
		return _("Offensive Auras");
	case Page::DefensiveAuras:
		return _("Defensive Auras");
	}
	return {};
}

bool ClassHasPaladinTree(const Player &player)
{
	// HeroClass::Warrior, not a HeroClass::Paladin - there isn't one. Oracool renames the Warrior
	// in display data only; see the same note in oracool/paladin_skills.cpp.
	return player._pClass == HeroClass::Warrior;
}

bool IsPaladinTreeSkillUnlocked(const Player &player, Skill skill)
{
	if (skill > Skill::LAST || !ClassHasPaladinTree(player))
		return false;
	return player._pLevel >= PaladinTreeTierMinLevel(GetPaladinTreeSkillData(skill).tier);
}

int PaladinTreeInvestment(const Player &player, Skill skill)
{
	if (skill > Skill::LAST)
		return 0;
	const PaladinTreeSkillData &data = GetPaladinTreeSkillData(skill);
	// A skill with a slot stores its points where GetSpellLevel will find them - see the file
	// comment. Only the auras need the tree's own array.
	if (PaladinTreeSpellId(skill) != SpellID::Invalid)
		return player._pSkillInvestment[static_cast<size_t>(PaladinTreeSpellId(skill))];
	const int slot = AuraSlot(skill);
	return slot < 0 ? 0 : player._pPaladinAuraInvestment[slot];
}

bool CanInvestPaladinTreePoint(const Player &player, Skill skill)
{
	return player._pUnspentSkillPoints > 0
	    && IsPaladinTreeSkillUnlocked(player, skill)
	    && PaladinTreeInvestment(player, skill) < MaxTreeInvestment;
}

bool InvestPaladinTreePoint(Player &player, Skill skill)
{
	if (!CanInvestPaladinTreePoint(player, skill))
		return false;
	const PaladinTreeSkillData &data = GetPaladinTreeSkillData(skill);
	player._pUnspentSkillPoints--;
	if (PaladinTreeSpellId(skill) != SpellID::Invalid) {
		player._pSkillInvestment[static_cast<size_t>(PaladinTreeSpellId(skill))]++;
	} else {
		player._pPaladinAuraInvestment[AuraSlot(skill)]++;
	}
	if (&player == MyPlayer) {
		LogEvent(fmt::format("{:s} raised to {:d}", std::string(_(data.name)),
		             PaladinTreeInvestment(player, skill)),
		    UiFlags::ColorWhitegold);
	}
	return true;
}

Skill GetActivePaladinAura(const Player &player)
{
	const auto skill = static_cast<Skill>(player._pOracoolActiveAura);
	if (skill < Skill::FIRST_AURA || skill > Skill::LAST)
		return Skill::None;
	return skill;
}

bool TogglePaladinAura(Player &player, Skill skill)
{
	if (AuraSlot(skill) < 0 || !IsPaladinTreeSkillUnlocked(player, skill))
		return false;
	const bool switchingOff = GetActivePaladinAura(player) == skill;
	// An aura with nothing invested has no strength to give, so lighting it would be a no-op that
	// LOOKED like it worked. Switching one off is always allowed.
	if (!switchingOff && PaladinTreeInvestment(player, skill) <= 0)
		return false;
	player._pOracoolActiveAura = static_cast<uint8_t>(switchingOff ? Skill::None : skill);
	if (&player == MyPlayer) {
		const char *name = GetPaladinTreeSkillData(skill).name;
		LogEvent(switchingOff
		        ? fmt::format("{:s} fades", std::string(_(name)))
		        : fmt::format("{:s} burns", std::string(_(name))),
		    UiFlags::ColorWhitegold);
	}
	return true;
}

void ApplyPaladinAuraToTotals(const Player &player, ItemBonusTotals &totals)
{
	const Skill aura = GetActivePaladinAura(player);
	if (aura == Skill::None || !IsPaladinTreeSkillUnlocked(player, aura))
		return;
	const int p = PaladinTreeInvestment(player, aura);
	if (p <= 0)
		return;

	switch (aura) {
	case Skill::Might:
		totals.bonusDamage += Scaled(p, 20, 10);
		break;
	case Skill::HolyFire:
		totals.fireMin += Scaled(p, 2, 1);
		totals.fireMax += Scaled(p, 6, 4);
		break;
	case Skill::Thorns:
		// This engine's thorns is a flag with a flat 1-3 return (monster.cpp), not a percentage,
		// so points light the aura rather than growing it. A proportional return is its own pass.
		totals.flags |= ItemSpecialEffect::Thorns;
		break;
	case Skill::BlessedAim:
		totals.bonusToHit += Scaled(p, 15, 7);
		break;
	case Skill::Concentration:
		totals.bonusDamage += Scaled(p, 15, 8);
		// "Uninterruptible" has no flag of its own; the fastest hit-recovery tier is the nearest
		// thing the engine has to shrugging off the flinch.
		totals.flags |= ItemSpecialEffect::FastestHitRecovery;
		break;
	case Skill::HolyShock:
		totals.lightningMin += 1;
		totals.lightningMax += Scaled(p, 10, 6);
		break;
	case Skill::Fanaticism:
		totals.flags |= ItemSpecialEffect::FastAttack;
		totals.bonusToHit += Scaled(p, 10, 5);
		totals.bonusDamage += Scaled(p, 10, 5);
		break;
	case Skill::ResistFire:
		totals.fireResist += Scaled(p, 15, 4);
		break;
	case Skill::Defiance:
		totals.bonusArmor += Scaled(p, 25, 12);
		break;
	case Skill::ResistCold:
		// The documented remap: no cold in this engine, so the cold ward is a magic ward.
		totals.magicResist += Scaled(p, 15, 4);
		break;
	case Skill::ResistLightning:
		totals.lightningResist += Scaled(p, 15, 4);
		break;
	case Skill::Salvation:
		totals.fireResist += Scaled(p, 10, 3);
		totals.lightningResist += Scaled(p, 10, 3);
		totals.magicResist += Scaled(p, 10, 3);
		break;
	default:
		// Prayer, Meditation and Vigor act elsewhere (the per-tick hook and the walk animation);
		// Holy Freeze, Sanctuary, Conviction, Cleansing and Redemption are inert - see their rows.
		break;
	}
}

bool IsPaladinVigorActive(const Player &player)
{
	const Skill aura = GetActivePaladinAura(player);
	return aura == Skill::Vigor && PaladinTreeInvestment(player, aura) > 0
	    && IsPaladinTreeSkillUnlocked(player, aura);
}

void ProcessPaladinAuraTick(Player &player)
{
	const Skill aura = GetActivePaladinAura(player);
	if (aura == Skill::None || !IsPaladinTreeSkillUnlocked(player, aura))
		return;
	const int p = PaladinTreeInvestment(player, aura);
	if (p <= 0)
		return;

	// Both regenerations are expressed in whole points per tick against the <<6 fixed point the
	// life and mana fields use, so one invested point is a slow trickle rather than a heal button.
	if (aura == Skill::Prayer) {
		if (player._pHitPoints >= player._pMaxHP)
			return;
		const int heal = Scaled(p, 2, 2);
		player._pHitPoints = std::min(player._pHitPoints + heal, player._pMaxHP);
		player._pHPBase = std::min(player._pHPBase + heal, player._pMaxHPBase);
		RedrawComponent(PanelDrawComponent::Health);
		return;
	}
	if (aura == Skill::Meditation) {
		if (player._pMana >= player._pMaxMana || HasAnyOf(player._pIFlags, ItemSpecialEffect::NoMana))
			return;
		const int gain = Scaled(p, 2, 2);
		player._pMana = std::min(player._pMana + gain, player._pMaxMana);
		player._pManaBase = std::min(player._pManaBase + gain, player._pMaxManaBase);
		RedrawComponent(PanelDrawComponent::Mana);
	}
}

size_t BuildPaladinTreePage(Page page, Skill *out)
{
	size_t count = 0;
	for (int tier = 0; tier < static_cast<int>(std::size(TierLevels)); tier++) {
		for (int column = 0; column < 3; column++) {
			for (size_t i = 0; i < PaladinTreeSkillCount; i++) {
				const PaladinTreeSkillData &data = Skills[i];
				if (data.page == page && data.tier == tier && data.column == column)
					out[count++] = static_cast<Skill>(i);
			}
		}
	}
	return count;
}

std::string PaladinTreeEffectLine(const Player &player, Skill skill)
{
	const PaladinTreeSkillData &data = GetPaladinTreeSkillData(skill);
	const int p = PaladinTreeInvestment(player, skill);
	std::string out = fmt::format(fmt::runtime(_("Points: {:d} of {:d}")), p, MaxTreeInvestment);
	out += "\n" + fmt::format(fmt::runtime(_("Requires level {:d}")), PaladinTreeTierMinLevel(data.tier));
	if (!data.implemented)
		out += "\n" + std::string(_("No effect yet"));
	else if (data.kind == Kind::Aura && p == 0)
		out += "\n" + std::string(_("Invest a point to light it"));
	return out;
}

} // namespace devilution::oracool
