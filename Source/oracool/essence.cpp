/**
 * @file oracool/essence.cpp
 *
 * See essence.h.
 */
#include "oracool/essence.h"

#include <algorithm>

#include "engine/backbuffer_state.hpp"
#include "oracool/class_tree.h"
#include "oracool/passives.h"
#include "player.h"

namespace devilution::oracool {

namespace {

/** One tick's refill in 1/64 points: the whole pool over the refill time, rounded UP so "within 20 seconds" holds. */
int RefillPerTick(const Player &player)
{
	const int ticks = EssenceRefillSeconds * EssenceTicksPerSecond;
	return ((MaxEssence(player) << 6) + ticks - 1) / ticks;
}

} // namespace

bool ClassUsesEssence(HeroClass heroClass)
{
	return heroClass == HeroClass::Necromancer;
}

bool UsesEssence(const Player &player)
{
	return ClassUsesEssence(player._pClass);
}

int MaxEssence(const Player &player)
{
	if (!UsesEssence(player))
		return 0;
	// Overwhelming Essence (N8): a fifth more.
	if (PassiveActive(player, ClassTreeSkill::OverwhelmingEssence))
		return BaseMaxEssence + BaseMaxEssence / 5;
	return BaseMaxEssence;
}

int CurrentEssence(const Player &player)
{
	return player._pEssence >> 6;
}

int EssenceCost(SpellID spell)
{
	// The prices proposed on 2026-09-17 and carried in the skill ledger: the corpse skills 10, Revive 35; the curses (N7) 25.
	switch (spell) {
	case SpellID::CorpseExplosion:
	case SpellID::PoisonExplosion:
		return 10;
	case SpellID::NecroRevive:
		return 35;
	case SpellID::AmplifyDamage:
	case SpellID::DimVision:
	case SpellID::NecroWeaken:
	case SpellID::Frailty:
	case SpellID::NecroIronMaiden:
	case SpellID::Terror:
	case SpellID::Bane:
	case SpellID::Confuse:
	case SpellID::LifeTap:
	case SpellID::Attract:
	case SpellID::Decrepify:
	case SpellID::DeathMark:
	case SpellID::LowerResist:
	case SpellID::Doom:
		return 25; // the curses (N7); Soul Harvest GIVES Essence and is priced in mana
	default:
		return 0;
	}
}

bool HasEssence(const Player &player, int points)
{
	if (points <= 0)
		return true;
	return UsesEssence(player) && player._pEssence >= (points << 6);
}

void SpendEssence(Player &player, int points)
{
	if (points <= 0 || !UsesEssence(player))
		return;
	player._pEssence = std::max(player._pEssence - (points << 6), 0);
	RedrawComponent(PanelDrawComponent::Mana);
}

void GainEssence(Player &player, int points)
{
	if (points <= 0 || !UsesEssence(player))
		return;
	player._pEssence = std::min(player._pEssence + (points << 6), MaxEssence(player) << 6);
	RedrawComponent(PanelDrawComponent::Mana);
}

void ResetEssence(Player &player)
{
	player._pEssence = 0;
}

void ProcessEssenceTick(Player &player)
{
	if (!UsesEssence(player)) {
		player._pEssence = 0;
		return;
	}
	if (player._pHitPoints <= 0 || player._pmode == PM_DEATH)
		return;
	const int full = MaxEssence(player) << 6;
	if (player._pEssence >= full) {
		player._pEssence = full; // a pool that shrank under it must not stay overfull
		return;
	}
	const int before = player._pEssence >> 6;
	player._pEssence = std::min(player._pEssence + RefillPerTick(player), full);
	if ((player._pEssence >> 6) != before)
		RedrawComponent(PanelDrawComponent::Mana);
}

} // namespace devilution::oracool
