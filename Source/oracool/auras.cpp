#include "oracool/auras.h"

#include <algorithm>
#include <array>
#include <cassert>

#include <fmt/format.h>

#include "oracool/event_log.h"
#include "oracool/stat_sheet.h"
#include "player.h"
#include "utils/language.h"

namespace devilution {
namespace oracool {

namespace {

// Order must match the Aura enum, which in turn matches ui\aura_icons.png. The static_assert below
// catches a row added here without one added there.
//
// Descriptions are one sentence each and deliberately short - the Abilities window wraps them to
// two lines beside a 38px icon, and a third line would push the row height past what fits
// comfortably. Where the design doc revised an effect after first proposing it (Defense, Defiance,
// Resistance and Aura Mastery all got refined once the one-active-aura rule was settled), the
// REVISED wording is what appears here.
const AuraData AuraTable[AuraCount] = {
	// Tier I - Initiate (level 1+) are spread across the sheet rather than grouped, because the
	// sheet's order is the artist's and the tiers are the designer's. GetAuraTierName groups them
	// at display time instead of reordering the icons.
	{ N_("Might"), N_("Increases the Paladin's physical weapon damage."), AuraTier::Initiate },
	{ N_("Defense"), N_("Increases Armor Class."), AuraTier::Initiate },
	{ N_("Vigor"), N_("Increases walking speed and reduces recovery time after being hit."), AuraTier::Initiate },
	{ N_("Fanaticism"), N_("Increases attack speed and Chance to Hit, with a smaller damage bonus."), AuraTier::Champion },
	{ N_("Regeneration"), N_("Slowly regenerates Life while active."), AuraTier::Initiate },
	{ N_("Resistance"), N_("Resist All: increases Fire, Lightning and Magic Resistance."), AuraTier::Initiate },

	{ N_("Defiance"), N_("Greatly increases Armor Class, Block Chance and hit recovery."), AuraTier::Templar },
	{ N_("Holy Fire"), N_("Adds Fire damage to attacks and periodically burns nearby enemies."), AuraTier::Templar },
	{ N_("Holy Shock"), N_("Adds Lightning damage to attacks and periodically shocks nearby enemies."), AuraTier::Crusader },
	{ N_("Holy Freeze"), N_("Adds Cold damage to attacks and periodically slows nearby enemies."), AuraTier::Crusader },
	{ N_("Conviction"), N_("Nearby enemies suffer reduced Armor Class and resistances."), AuraTier::Champion },
	{ N_("Sanctuary"), N_("Damages nearby undead and improves damage dealt to them."), AuraTier::Champion },

	{ N_("Retribution"), N_("Returns part of the melee damage received back to the attacker."), AuraTier::Crusader },
	{ N_("Purge"), N_("Improves damage and Chance to Hit against demons and undead."), AuraTier::Crusader },
	{ N_("Life Aura"), N_("Increases maximum Life."), AuraTier::Crusader },
	{ N_("Shield Aura"), N_("Improves Block Chance and reduces damage taken while blocking."), AuraTier::Templar },
	{ N_("Focus"), N_("Improves Chance to Hit and reduces interruption when attacking."), AuraTier::Templar },
	{ N_("Aura Mastery"), N_("Moderate bonuses to Armor Class, Chance to Hit, Resist All and recovery."), AuraTier::Champion },

	{ N_("Cleanse"), N_("Reduces the duration and damage of poison and other lingering effects."), AuraTier::Initiate },
	{ N_("Blessing"), N_("Improves Chance to Hit and damage, especially against demons and undead."), AuraTier::Templar },
	{ N_("Swiftness"), N_("Improves movement speed and attack recovery."), AuraTier::Templar },
	{ N_("Endurance"), N_("Increases survivability and slightly reduces physical damage taken."), AuraTier::Crusader },
	{ N_("Vigilance"), N_("Improves light radius, trap detection and defence against ranged attacks."), AuraTier::Champion },
	{ N_("Righteousness"), N_("Improves physical damage and adds holy damage against demons and undead."), AuraTier::Champion },
};

static_assert(sizeof(AuraTable) / sizeof(AuraTable[0]) == AuraCount,
    "Aura table and AuraCount disagree - the icon strip has one cell per entry");

} // namespace

const AuraData &GetAuraData(Aura aura)
{
	assert(static_cast<size_t>(aura) < AuraCount);
	return AuraTable[static_cast<size_t>(aura)];
}

Aura GetAuraAtDisplayIndex(size_t index)
{
	assert(index < AuraCount);

	// Built once. The order depends only on the table, which is a compile-time constant, so
	// rebuilding it per frame - the Abilities window asks for every visible row every draw - would
	// be pure waste.
	static const std::array<Aura, AuraCount> DisplayOrder = [] {
		std::array<Aura, AuraCount> order {};
		for (size_t i = 0; i < AuraCount; i++)
			order[i] = static_cast<Aura>(i);
		// Tier first, name second. Sorting on the tier's minimum level rather than on the enum
		// value keeps this correct if the tiers are ever renumbered or reordered.
		std::stable_sort(order.begin(), order.end(), [](Aura a, Aura b) {
			const int levelA = GetAuraTierMinLevel(GetAuraData(a).tier);
			const int levelB = GetAuraTierMinLevel(GetAuraData(b).tier);
			if (levelA != levelB)
				return levelA < levelB;
			return string_view(_(GetAuraData(a).name)) < string_view(_(GetAuraData(b).name));
		});
		return order;
	}();

	return DisplayOrder[index];
}

int GetAuraTierMinLevel(AuraTier tier)
{
	switch (tier) {
	case AuraTier::Initiate:
		return 1;
	case AuraTier::Templar:
		return 8;
	case AuraTier::Crusader:
		return 16;
	case AuraTier::Champion:
		return 24;
	}
	return 1;
}

string_view GetAuraTierName(AuraTier tier)
{
	switch (tier) {
	case AuraTier::Initiate:
		return _("Initiate");
	case AuraTier::Templar:
		return _("Templar");
	case AuraTier::Crusader:
		return _("Crusader");
	case AuraTier::Champion:
		return _("Champion");
	}
	return {};
}

bool IsAuraUnlocked(const Player &player, Aura aura)
{
	if (!ClassHasAuras(player))
		return false;
	return player._pLevel >= GetAuraTierMinLevel(GetAuraData(aura).tier);
}

bool ClassHasAuras(const Player &player)
{
	// HeroClass::Warrior, NOT a HeroClass::Paladin - there isn't one. Oracool renames the Warrior
	// to "Paladin" in its display data only (playerdat.cpp's className), leaving the enum, the
	// sprite folder ("warrior") and every save field on the original name. Reading the class by its
	// displayed name is the trap here: it compiles as HeroClass::Warrior everywhere else in the
	// codebase and would silently never match.
	return player._pClass == HeroClass::Warrior;
}

Aura GetActiveAura(const Player &player)
{
	const auto aura = static_cast<Aura>(player._pOracoolActiveAura);
	if (aura > Aura::LAST)
		return Aura::None;
	return aura;
}

bool ToggleAura(Player &player, Aura aura)
{
	if (aura > Aura::LAST || !IsAuraUnlocked(player, aura))
		return false;
	const bool switchingOff = GetActiveAura(player) == aura;
	player._pOracoolActiveAura = static_cast<uint8_t>(switchingOff ? Aura::None : aura);
	if (&player == MyPlayer) {
		LogEvent(switchingOff
		        ? fmt::format("{:s} aura extinguished", std::string(_(GetAuraData(aura).name)))
		        : fmt::format("{:s} aura burning", std::string(_(GetAuraData(aura).name))),
		    UiFlags::ColorWhitegold);
	}
	return true;
}

void ApplyAuraToTotals(Aura aura, int characterLevel, ItemBonusTotals &totals)
{
	// The fourteen accumulator-shaped auras from the implementation plan, scaled on character
	// level until Stage 2 gives auras levels of their own. Percentages ride the same channels item
	// affixes use (bonusDamage/bonusToHit/bonusArmor are percent; resists are percent and clamped
	// by CalcPlrItemVals; hitPoints runs in <<6 fixed point; getHit is beneficial-negative).
	const int clvl = characterLevel;
	switch (aura) {
	case Aura::Might:
		totals.bonusDamage += 20 + clvl;
		break;
	case Aura::Defense:
		totals.bonusArmor += 20 + clvl;
		break;
	case Aura::Fanaticism:
		totals.bonusToHit += 10;
		totals.bonusDamage += 10 + clvl / 2;
		totals.flags |= ItemSpecialEffect::FastAttack;
		break;
	case Aura::Resistance:
		totals.fireResist += 10 + clvl / 2;
		totals.lightningResist += 10 + clvl / 2;
		totals.magicResist += 10 + clvl / 2;
		break;
	case Aura::Defiance:
		totals.bonusArmor += 40 + clvl;
		break;
	case Aura::HolyFire:
		totals.fireMin += 1 + clvl / 4;
		totals.fireMax += 2 + clvl / 2;
		break;
	case Aura::HolyShock:
		totals.lightningMin += 1;
		totals.lightningMax += 2 + clvl;
		break;
	case Aura::LifeAura:
		totals.hitPoints += (10 + 2 * clvl) << 6;
		break;
	case Aura::Focus:
		totals.bonusToHit += 15 + clvl / 2;
		break;
	case Aura::AuraMastery:
		totals.bonusToHit += 10;
		totals.bonusArmor += 10;
		totals.fireResist += 10;
		totals.lightningResist += 10;
		totals.magicResist += 10;
		break;
	case Aura::Blessing:
		totals.bonusToHit += 10 + clvl / 2;
		totals.bonusDamage += 10 + clvl / 2;
		break;
	case Aura::Endurance:
		totals.vitality += 5 + clvl / 4;
		totals.getHit -= 1 + clvl / 8;
		break;
	case Aura::Vigilance:
		totals.lightRadius += 2;
		break;
	case Aura::Righteousness:
		totals.bonusDamage += 30 + clvl;
		break;
	default:
		// Vigor, Regeneration, Holy Freeze, Conviction, Sanctuary, Retribution, Purge, Shield
		// Aura, Cleanse, Swiftness: their mechanics belong to later stages (pulses, monster-facing
		// queries, movement) and an aura must never half-work - so they are silent here, and the
		// Auras sheet's hover text still describes what they WILL do.
		break;
	}
}

} // namespace oracool
} // namespace devilution
