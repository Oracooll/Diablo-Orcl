#include "oracool/rage.h"

#include <algorithm>

#include <fmt/format.h>

#include "engine/backbuffer_state.hpp"
#include "oracool/class_tree.h"
#include "oracool/passives.h"
#include "player.h"
#include "spells.h"
#include "utils/language.h"

namespace devilution::oracool {

bool ClassUsesRage(HeroClass heroClass)
{
	return heroClass == HeroClass::Barbarian;
}

bool UsesRage(const Player &player)
{
	return ClassUsesRage(player._pClass);
}

int RageGain(SpellID spell)
{
	// The user's generators, 2026-09-13.
	switch (spell) {
	case SpellID::Bash:
	case SpellID::Backhand:
	case SpellID::Cleave:
	case SpellID::DoubleSwing:
	case SpellID::Concentrate:
	case SpellID::Frenzy:
	case SpellID::ClaspOfRuin:
	case SpellID::BerserkBlow:
		return 6;
	case SpellID::Stun:
		return 7;
	default:
		return 0;
	}
}

int RageCost(SpellID spell)
{
	// The user's spenders, 2026-09-13.
	switch (spell) {
	case SpellID::AncestralCall:
		return 30;
	case SpellID::LeapAttack:
		return 14;
	case SpellID::Rend:
	case SpellID::SplitRanks:
		return 9;
	case SpellID::BattleCommand:
	case SpellID::BattleCry:
	case SpellID::BattleOrders:
	case SpellID::Bloodcall:
	case SpellID::Earthquake:
	case SpellID::EarthshakerCry:
	case SpellID::FindItem:
	case SpellID::FindPotion:
	case SpellID::GrimWard:
	case SpellID::GroundStomp:
	case SpellID::HammerOfTheAncients:
	case SpellID::Howl:
	case SpellID::Intimidate:
	case SpellID::IronWill:
	case SpellID::Leap:
	case SpellID::RallyingCry:
	case SpellID::SeismicSlam:
	case SpellID::Shout:
	case SpellID::Taunt:
	case SpellID::ThreateningShout:
	case SpellID::WarCry:
	case SpellID::Whirlwind:
		return 10;
	default:
		return 0;
	}
}

int MaxRage(const Player &player)
{
	return BaseMaxRage + (PassiveActive(player, ClassTreeSkill::Animosity) ? AnimosityRage : 0);
}

void GainRage(Player &player, int points)
{
	if (points <= 0 || !UsesRage(player))
		return;
	player._pRage = std::clamp(player._pRage + points, 0, MaxRage(player));
	player._pRageIdleTicks = 0;
	RedrawComponent(PanelDrawComponent::Mana);
}

void ResetRage(Player &player)
{
	player._pRage = 0;
	player._pRageIdleTicks = 0;
}

void ProcessRageTick(Player &player)
{
	if (!UsesRage(player))
		return;
	// A passive that shrinks the pool (Animosity unassigned) must not leave it overfull.
	player._pRage = std::min(player._pRage, MaxRage(player));
	if (player._pRage <= 0) {
		player._pRageIdleTicks = 0;
		return;
	}
	player._pRageIdleTicks++;
	if (player._pRageIdleTicks > RageDecayDelayTicks
	    && (player._pRageIdleTicks - RageDecayDelayTicks) % RageDecayIntervalTicks == 0) {
		player._pRage--;
		RedrawComponent(PanelDrawComponent::Mana);
	}
}

bool CanPaySkill(const Player &player, SpellID spell)
{
	if (UsesRage(player))
		return player._pRage >= RageCost(spell);
	return player._pMana >= GetManaAmount(player, spell);
}

void SettleSkill(Player &player, SpellID spell)
{
	if (UsesRage(player)) {
		if (const int cost = RageCost(spell); cost > 0) {
			player._pRage = std::max(player._pRage - cost, 0);
			player._pRageIdleTicks = 0;
			// Bloodthirst returns half of what is spent as life. The passive speaks in the 1/64 units
			// of the mana it was written for; a point of Rage stands in for a point of mana.
			OnPassiveManaSpent(player, cost << 6);
			RedrawComponent(PanelDrawComponent::Mana);
		}
		GainRage(player, RageGain(spell));
		return;
	}
	const int cost = GetManaAmount(player, spell);
	player._pMana -= cost;
	player._pManaBase -= cost;
	OnPassiveManaSpent(player, cost);
	RedrawComponent(PanelDrawComponent::Mana);
}

std::string SkillResourceLine(const Player &player, SpellID spell, int level)
{
	if (UsesRage(player)) {
		if (const int cost = RageCost(spell); cost > 0)
			return fmt::format(fmt::runtime(_("Rage Cost: {:d}")), cost);
		if (const int gain = RageGain(spell); gain > 0)
			return fmt::format(fmt::runtime(_("Generates {:d} Rage")), gain);
		return {};
	}
	return fmt::format(fmt::runtime(_("Mana Cost: {:d}")), GetManaAmountAtLevel(player, spell, level) >> 6);
}

} // namespace devilution::oracool
