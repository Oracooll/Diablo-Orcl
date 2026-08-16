#include "oracool/gems.h"

#include <fmt/format.h>

#include "items.h"
#include "oracool/stat_sheet.h"
#include "utils/language.h"

namespace devilution::oracool {

namespace {

/**
 * @brief One gem's three host-dependent effects. Kept as plain numbers routed onto
 * ItemBonusTotals fields rather than IPL affixes - gems are fixed, unconditional, and stack, and
 * the totals struct is exactly the right vocabulary for that.
 */
struct GemData {
	uint16_t idx;
	// Weapon: elemental damage min/max or flat bonuses.
	int weaponFireMin, weaponFireMax;
	int weaponLightMin, weaponLightMax;
	int weaponToHit;
	int weaponDamageMod;
	// Armor (body/helm/worn) and shield: resist or defensive numbers; shield gets the bigger roll.
	int armorFireRes, armorLightRes, armorMagicRes;
	int armorHitPoints; // in whole HP; converted to <<6 fixed point at apply time
	int armorBonusAc;
	int shieldFireRes, shieldLightRes, shieldMagicRes;
	int shieldBonusAc;
};

constexpr GemData Gems[] = {
	// Ruby: fire. Weapon +2-6 fire damage; armor +12 FR; shield +20 FR.
	{ IDI_ORACOOL_GEM_RUBY, 2, 6, 0, 0, 0, 0, 12, 0, 0, 0, 0, 20, 0, 0, 0 },
	// Sapphire: lightning. Weapon +1-8 lightning; armor +12 LR; shield +20 LR.
	{ IDI_ORACOOL_GEM_SAPPHIRE, 0, 0, 1, 8, 0, 0, 0, 12, 0, 0, 0, 0, 20, 0, 0 },
	// Topaz: precision and warding. Weapon +10 to-hit; armor +12 MR; shield +20 MR.
	{ IDI_ORACOOL_GEM_TOPAZ, 0, 0, 0, 0, 10, 0, 0, 0, 12, 0, 0, 0, 0, 20, 0 },
	// Emerald: force. Weapon +4 flat damage; armor +6 AC; shield +10 AC.
	{ IDI_ORACOOL_GEM_EMERALD, 0, 0, 0, 0, 0, 4, 0, 0, 0, 0, 6, 0, 0, 0, 10 },
	// Skull: vitality torn from the dead. Weapon +2 flat damage; armor +15 HP; shield +10 HP...
	// no - shields guard, so a Skull there is +8 AC. Life belongs on the body.
	{ IDI_ORACOOL_GEM_SKULL, 0, 0, 0, 0, 0, 2, 0, 0, 0, 15, 0, 0, 0, 0, 8 },
	// The runes: individually smaller than gems on purpose - a rune alone is a down payment on the
	// runeword it belongs to (oracool/runewords.h), and pricing it above a gem would make every
	// completed word strictly free power on top of the better socketable.
	{ IDI_ORACOOL_RUNE_EL, 0, 0, 0, 0, 5, 0, 0, 0, 0, 0, 2, 0, 0, 0, 2 },
	{ IDI_ORACOOL_RUNE_TIR, 0, 0, 0, 0, 0, 2, 0, 0, 0, 5, 0, 0, 0, 0, 3 },
	{ IDI_ORACOOL_RUNE_RAL, 1, 4, 0, 0, 0, 0, 8, 0, 0, 0, 0, 10, 0, 0, 0 },
	{ IDI_ORACOOL_RUNE_ORT, 0, 0, 1, 6, 0, 0, 0, 8, 0, 0, 0, 0, 10, 0, 0 },
	{ IDI_ORACOOL_RUNE_SOL, 0, 0, 0, 0, 0, 3, 0, 0, 0, 8, 0, 0, 0, 0, 4 },
};

const GemData *FindGem(uint16_t gemIdx)
{
	for (const GemData &gem : Gems) {
		if (gem.idx == gemIdx)
			return &gem;
	}
	return nullptr;
}

} // namespace

SocketHost SocketHostForItemType(ItemType hostType)
{
	switch (hostType) {
	case ItemType::Sword:
	case ItemType::Axe:
	case ItemType::Bow:
	case ItemType::Mace:
	case ItemType::Staff:
		return SocketHost::Weapon;
	case ItemType::Shield:
		return SocketHost::Shield;
	default:
		return SocketHost::Armor;
	}
}

bool CanItemHaveSockets(const Item &item)
{
	if (item.isEmpty() || item._iMagical != ITEM_QUALITY_NORMAL || item.hasOracoolTier())
		return false;
	switch (item._itype) {
	case ItemType::Sword:
	case ItemType::Axe:
	case ItemType::Bow:
	case ItemType::Mace:
	case ItemType::Staff:
	case ItemType::Shield:
	case ItemType::LightArmor:
	case ItemType::MediumArmor:
	case ItemType::HeavyArmor:
	case ItemType::Helm:
	case ItemType::Shoulders:
	case ItemType::Bracers:
	case ItemType::Gloves:
	case ItemType::Belt:
	case ItemType::Legs:
	case ItemType::Boots:
		return true;
	default:
		return false;
	}
}

void ApplyGemToTotals(uint16_t gemIdx, SocketHost host, ItemBonusTotals &totals)
{
	const GemData *gem = FindGem(gemIdx);
	if (gem == nullptr)
		return;
	switch (host) {
	case SocketHost::Weapon:
		totals.fireMin += gem->weaponFireMin;
		totals.fireMax += gem->weaponFireMax;
		totals.lightningMin += gem->weaponLightMin;
		totals.lightningMax += gem->weaponLightMax;
		totals.bonusToHit += gem->weaponToHit;
		totals.damageMod += gem->weaponDamageMod;
		break;
	case SocketHost::Shield:
		totals.fireResist += gem->shieldFireRes;
		totals.lightningResist += gem->shieldLightRes;
		totals.magicResist += gem->shieldMagicRes;
		totals.bonusArmor += gem->shieldBonusAc;
		break;
	case SocketHost::Armor:
		totals.fireResist += gem->armorFireRes;
		totals.lightningResist += gem->armorLightRes;
		totals.magicResist += gem->armorMagicRes;
		totals.hitPoints += gem->armorHitPoints << 6; // HP fields run in <<6 fixed point
		totals.bonusArmor += gem->armorBonusAc;
		break;
	}
}

std::string GemSocketLine(uint16_t gemIdx, SocketHost host)
{
	const GemData *gem = FindGem(gemIdx);
	if (gem == nullptr)
		return {};
	const char *name = AllItemsList[gemIdx].iName;
	switch (host) {
	case SocketHost::Weapon:
		if (gem->weaponFireMax > 0)
			return fmt::format(fmt::runtime(_("{:s}: +{:d}-{:d} fire damage")), _(name), gem->weaponFireMin, gem->weaponFireMax);
		if (gem->weaponLightMax > 0)
			return fmt::format(fmt::runtime(_("{:s}: +{:d}-{:d} lightning damage")), _(name), gem->weaponLightMin, gem->weaponLightMax);
		if (gem->weaponToHit > 0)
			return fmt::format(fmt::runtime(_("{:s}: +{:d}% to hit")), _(name), gem->weaponToHit);
		return fmt::format(fmt::runtime(_("{:s}: +{:d} damage")), _(name), gem->weaponDamageMod);
	case SocketHost::Shield:
		if (gem->shieldFireRes > 0)
			return fmt::format(fmt::runtime(_("{:s}: +{:d}% fire resist")), _(name), gem->shieldFireRes);
		if (gem->shieldLightRes > 0)
			return fmt::format(fmt::runtime(_("{:s}: +{:d}% lightning resist")), _(name), gem->shieldLightRes);
		if (gem->shieldMagicRes > 0)
			return fmt::format(fmt::runtime(_("{:s}: +{:d}% magic resist")), _(name), gem->shieldMagicRes);
		return fmt::format(fmt::runtime(_("{:s}: +{:d} armor")), _(name), gem->shieldBonusAc);
	case SocketHost::Armor:
		if (gem->armorFireRes > 0)
			return fmt::format(fmt::runtime(_("{:s}: +{:d}% fire resist")), _(name), gem->armorFireRes);
		if (gem->armorLightRes > 0)
			return fmt::format(fmt::runtime(_("{:s}: +{:d}% lightning resist")), _(name), gem->armorLightRes);
		if (gem->armorMagicRes > 0)
			return fmt::format(fmt::runtime(_("{:s}: +{:d}% magic resist")), _(name), gem->armorMagicRes);
		if (gem->armorHitPoints > 0)
			return fmt::format(fmt::runtime(_("{:s}: +{:d} life")), _(name), gem->armorHitPoints);
		return fmt::format(fmt::runtime(_("{:s}: +{:d} armor")), _(name), gem->armorBonusAc);
	}
	return {};
}

bool TrySocketGem(Item &target, const Item &held)
{
	if (held.isEmpty() || (!IsOracoolGemIdx(held.IDidx) && !IsOracoolRuneIdx(held.IDidx)))
		return false;
	if (!target.hasOpenSocket())
		return false;
	for (uint16_t &socket : target._iSocketed) {
		if (socket == Item::EmptySocket) {
			socket = static_cast<uint16_t>(held.IDidx);
			return true;
		}
	}
	return false;
}

} // namespace devilution::oracool
