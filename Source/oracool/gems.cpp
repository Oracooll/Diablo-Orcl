#include "oracool/gems.h"

#include <fmt/format.h>

#include "items.h"
#include "oracool/stat_sheet.h"
#include "player.h"
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
	// D2 rune mechanics (user directive 2026-08-16: "make them same as D2"):
	int lightRadius;      // every host - El's +1
	int manaPerKill;      // every host - Tir's +2, granted by GrantRuneKillMana on each kill
	int damageReduction;  // armor and shield hosts - Sol's flat -7 damage taken
};

constexpr GemData Gems[] = {
	// The gems keep this fork's own host-dependent tuning (only the RUNES were mandated to D2).
	// Ruby: fire. Weapon +2-6 fire damage; armor +12 FR; shield +20 FR.
	{ IDI_ORACOOL_GEM_RUBY, 2, 6, 0, 0, 0, 0, 12, 0, 0, 0, 0, 20, 0, 0, 0, 0, 0, 0 },
	// Sapphire: lightning. Weapon +1-8 lightning; armor +12 LR; shield +20 LR.
	{ IDI_ORACOOL_GEM_SAPPHIRE, 0, 0, 1, 8, 0, 0, 0, 12, 0, 0, 0, 0, 20, 0, 0, 0, 0, 0 },
	// Topaz: precision and warding. Weapon +10 to-hit; armor +12 MR; shield +20 MR.
	{ IDI_ORACOOL_GEM_TOPAZ, 0, 0, 0, 0, 10, 0, 0, 0, 12, 0, 0, 0, 0, 20, 0, 0, 0, 0 },
	// Emerald: force. Weapon +4 flat damage; armor +6 AC; shield +10 AC.
	{ IDI_ORACOOL_GEM_EMERALD, 0, 0, 0, 0, 0, 4, 0, 0, 0, 0, 6, 0, 0, 0, 10, 0, 0, 0 },
	// Skull: vitality torn from the dead. Weapon +2 flat damage; armor +15 HP; shield +8 AC.
	{ IDI_ORACOOL_GEM_SKULL, 0, 0, 0, 0, 0, 2, 0, 0, 0, 15, 0, 0, 0, 0, 8, 0, 0, 0 },
	// The runes, to Diablo II's own sheet (user directive). Two adaptations where the engines
	// differ, both documented rather than fudged silently:
	//   - El's +50 Attack Rating maps to +5% to-hit (D2's own ~10 AR : 1% convention).
	//   - Sol's "+9 to minimum damage" becomes +9 flat damage: D1 has no min-only channel, and
	//     pushing only the min can cross the max on small weapons, which D1's damage roll
	//     (GenerateRnd over max-min) does not survive.
	// El: +5% to-hit in weapons / +15 defense elsewhere; +1 light radius everywhere.
	{ IDI_ORACOOL_RUNE_EL, 0, 0, 0, 0, 5, 0, 0, 0, 0, 0, 15, 0, 0, 0, 15, 1, 0, 0 },
	// Tir: +2 mana after each kill, in every host.
	{ IDI_ORACOOL_RUNE_TIR, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0 },
	// Ral: adds 5-30 fire damage / Fire Resist +30% armor, +35% shield.
	{ IDI_ORACOOL_RUNE_RAL, 5, 30, 0, 0, 0, 0, 30, 0, 0, 0, 0, 35, 0, 0, 0, 0, 0, 0 },
	// Ort: adds 1-50 lightning damage / Lightning Resist +30% armor, +35% shield.
	{ IDI_ORACOOL_RUNE_ORT, 0, 0, 1, 50, 0, 0, 0, 30, 0, 0, 0, 0, 35, 0, 0, 0, 0, 0 },
	// Sol: +9 damage in weapons / Damage Reduced by 7 in armor and shields.
	{ IDI_ORACOOL_RUNE_SOL, 0, 0, 0, 0, 0, 9, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 7 },
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
	// Host-independent effects (the D2 rune column): light radius everywhere; Sol's flat damage
	// reduction only where D2 grants it, armor and shields. getHit is the vanilla damage-taken
	// channel - negative is beneficial, CalcPlrItemVals shifts it into <<6 fixed point.
	totals.lightRadius += gem->lightRadius;
	if (host != SocketHost::Weapon)
		totals.getHit -= gem->damageReduction;
}

std::string GemSocketLine(uint16_t gemIdx, SocketHost host)
{
	const GemData *gem = FindGem(gemIdx);
	if (gem == nullptr)
		return {};
	const char *name = AllItemsList[gemIdx].iName;
	// The D2 runes stack several effects on one host (El: armor AND light radius), so the line is
	// built from every nonzero field rather than the first one found.
	std::string parts;
	const auto add = [&parts](std::string piece) {
		if (!parts.empty())
			parts.append(", ");
		parts.append(std::move(piece));
	};
	switch (host) {
	case SocketHost::Weapon:
		if (gem->weaponFireMax > 0)
			add(fmt::format(fmt::runtime(_("+{:d}-{:d} fire damage")), gem->weaponFireMin, gem->weaponFireMax));
		if (gem->weaponLightMax > 0)
			add(fmt::format(fmt::runtime(_("+{:d}-{:d} lightning damage")), gem->weaponLightMin, gem->weaponLightMax));
		if (gem->weaponToHit > 0)
			add(fmt::format(fmt::runtime(_("+{:d}% to hit")), gem->weaponToHit));
		if (gem->weaponDamageMod > 0)
			add(fmt::format(fmt::runtime(_("+{:d} damage")), gem->weaponDamageMod));
		break;
	case SocketHost::Shield:
		if (gem->shieldFireRes > 0)
			add(fmt::format(fmt::runtime(_("+{:d}% fire resist")), gem->shieldFireRes));
		if (gem->shieldLightRes > 0)
			add(fmt::format(fmt::runtime(_("+{:d}% lightning resist")), gem->shieldLightRes));
		if (gem->shieldMagicRes > 0)
			add(fmt::format(fmt::runtime(_("+{:d}% magic resist")), gem->shieldMagicRes));
		if (gem->shieldBonusAc > 0)
			add(fmt::format(fmt::runtime(_("+{:d} armor")), gem->shieldBonusAc));
		break;
	case SocketHost::Armor:
		if (gem->armorFireRes > 0)
			add(fmt::format(fmt::runtime(_("+{:d}% fire resist")), gem->armorFireRes));
		if (gem->armorLightRes > 0)
			add(fmt::format(fmt::runtime(_("+{:d}% lightning resist")), gem->armorLightRes));
		if (gem->armorMagicRes > 0)
			add(fmt::format(fmt::runtime(_("+{:d}% magic resist")), gem->armorMagicRes));
		if (gem->armorHitPoints > 0)
			add(fmt::format(fmt::runtime(_("+{:d} life")), gem->armorHitPoints));
		if (gem->armorBonusAc > 0)
			add(fmt::format(fmt::runtime(_("+{:d} armor")), gem->armorBonusAc));
		break;
	}
	if (host != SocketHost::Weapon && gem->damageReduction > 0)
		add(fmt::format(fmt::runtime(_("damage taken -{:d}")), gem->damageReduction));
	if (gem->lightRadius > 0)
		add(fmt::format(fmt::runtime(_("+{:d} light radius")), gem->lightRadius));
	if (gem->manaPerKill > 0)
		add(fmt::format(fmt::runtime(_("+{:d} mana per kill")), gem->manaPerKill));
	return fmt::format(fmt::runtime(_("{:s}: {:s}")), _(name), parts);
}

int RuneManaPerKill(const Player &player)
{
	int total = 0;
	for (const Item &item : player.InvBody) {
		if (item.isEmpty() || !item._iStatFlag)
			continue;
		for (const uint16_t socket : item._iSocketed) {
			if (socket == Item::EmptySocket)
				continue;
			const GemData *gem = FindGem(socket);
			if (gem != nullptr)
				total += gem->manaPerKill;
		}
	}
	return total;
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
