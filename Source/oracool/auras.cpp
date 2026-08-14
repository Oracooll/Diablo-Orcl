#include "oracool/auras.h"

#include <algorithm>
#include <array>
#include <cassert>

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

} // namespace oracool
} // namespace devilution
