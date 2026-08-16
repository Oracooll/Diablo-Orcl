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
	/**
	 * Amethyst's armour effect - D2 gives it dexterity there, and nothing else uses this. APPENDED
	 * rather than filed beside the other armour fields: every row below is positional, so inserting
	 * a field mid-struct would silently re-read each rune's trailing numbers as something else.
	 */
	int armorDexterity;
};

constexpr GemData Gems[] = {
	// The gems keep this fork's own host-dependent tuning (only the RUNES were mandated to D2).
	// Every row here is the NORMAL quality of its type; the other four qualities are the same row
	// scaled by GemQualityPercent, which is why the ladder needed no second table.
	//
	// Amethyst: the eye. Weapon +8 to-hit; armor +5 dexterity; shield +10 AC. (D2 gives Amethyst
	// attack rating in weapons and dexterity in armour, which both exist here.)
	{ IDI_ORACOOL_GEM_AMETHYST_NORMAL, 0, 0, 0, 0, 8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 10, 0, 0, 0, 5 },
	// Diamond: the ward. Weapon +3 flat damage; armor +6 to all three resists; shield +10 to all.
	// (D2's Diamond is damage-to-undead in weapons, which has no channel here, so it is flat.)
	{ IDI_ORACOOL_GEM_DIAMOND_NORMAL, 0, 0, 0, 0, 0, 3, 6, 6, 6, 0, 0, 10, 10, 10, 0, 0, 0, 0, 0 },
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

const GemData *FindGemRow(uint16_t idx)
{
	for (const GemData &gem : Gems) {
		if (gem.idx == idx)
			return &gem;
	}
	return nullptr;
}

/**
 * @brief The (type, quality) -> item index table.
 *
 * Explicit rather than arithmetic because the indices are NOT contiguous: the five gems that
 * shipped first are the normal quality of their type and keep their original indices, and the
 * thirty that completed the ladder were appended after them. Item indices are positional save
 * format, so the older five could not be moved to make the block tidy.
 */
constexpr uint16_t GemIndexTable[GemTypeCount][GemQualityCount] = {
	// Chipped, Flawed, Normal, Flawless, Perfect
	{ IDI_ORACOOL_GEM_AMETHYST_CHIPPED, IDI_ORACOOL_GEM_AMETHYST_FLAWED, IDI_ORACOOL_GEM_AMETHYST_NORMAL,
	    IDI_ORACOOL_GEM_AMETHYST_FLAWLESS, IDI_ORACOOL_GEM_AMETHYST_PERFECT },
	{ IDI_ORACOOL_GEM_DIAMOND_CHIPPED, IDI_ORACOOL_GEM_DIAMOND_FLAWED, IDI_ORACOOL_GEM_DIAMOND_NORMAL,
	    IDI_ORACOOL_GEM_DIAMOND_FLAWLESS, IDI_ORACOOL_GEM_DIAMOND_PERFECT },
	{ IDI_ORACOOL_GEM_EMERALD_CHIPPED, IDI_ORACOOL_GEM_EMERALD_FLAWED, IDI_ORACOOL_GEM_EMERALD,
	    IDI_ORACOOL_GEM_EMERALD_FLAWLESS, IDI_ORACOOL_GEM_EMERALD_PERFECT },
	{ IDI_ORACOOL_GEM_RUBY_CHIPPED, IDI_ORACOOL_GEM_RUBY_FLAWED, IDI_ORACOOL_GEM_RUBY,
	    IDI_ORACOOL_GEM_RUBY_FLAWLESS, IDI_ORACOOL_GEM_RUBY_PERFECT },
	{ IDI_ORACOOL_GEM_SAPPHIRE_CHIPPED, IDI_ORACOOL_GEM_SAPPHIRE_FLAWED, IDI_ORACOOL_GEM_SAPPHIRE,
	    IDI_ORACOOL_GEM_SAPPHIRE_FLAWLESS, IDI_ORACOOL_GEM_SAPPHIRE_PERFECT },
	{ IDI_ORACOOL_GEM_TOPAZ_CHIPPED, IDI_ORACOOL_GEM_TOPAZ_FLAWED, IDI_ORACOOL_GEM_TOPAZ,
	    IDI_ORACOOL_GEM_TOPAZ_FLAWLESS, IDI_ORACOOL_GEM_TOPAZ_PERFECT },
	{ IDI_ORACOOL_GEM_SKULL_CHIPPED, IDI_ORACOOL_GEM_SKULL_FLAWED, IDI_ORACOOL_GEM_SKULL,
	    IDI_ORACOOL_GEM_SKULL_FLAWLESS, IDI_ORACOOL_GEM_SKULL_PERFECT },
};

} // namespace

uint16_t GemIndexFor(GemType type, GemQuality quality)
{
	return GemIndexTable[static_cast<size_t>(type)][static_cast<size_t>(quality)];
}

bool GemTypeAndQuality(uint16_t gemIdx, GemType &type, GemQuality &quality)
{
	for (size_t t = 0; t < GemTypeCount; t++) {
		for (size_t q = 0; q < GemQualityCount; q++) {
			if (GemIndexTable[t][q] == gemIdx) {
				type = static_cast<GemType>(t);
				quality = static_cast<GemQuality>(q);
				return true;
			}
		}
	}
	return false;
}

bool IsPerfectGem(uint16_t gemIdx)
{
	GemType type;
	GemQuality quality;
	return GemTypeAndQuality(gemIdx, type, quality) && quality == GemQuality::Perfect;
}

uint16_t NextGemQuality(uint16_t gemIdx)
{
	GemType type;
	GemQuality quality;
	if (!GemTypeAndQuality(gemIdx, type, quality) || quality == GemQuality::Perfect)
		return gemIdx;
	return GemIndexFor(type, static_cast<GemQuality>(static_cast<uint8_t>(quality) + 1));
}

int GemQualityPercent(GemQuality quality)
{
	switch (quality) {
	case GemQuality::Chipped:
		return 40;
	case GemQuality::Flawed:
		return 70;
	case GemQuality::Normal:
		return 100;
	case GemQuality::Flawless:
		return 145;
	case GemQuality::Perfect:
		return 200;
	}
	return 100;
}

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

namespace {

/**
 * @brief The effect row for @p idx, and the percentage its numbers are scaled by.
 *
 * Runes have one strength and answer 100. A gem answers its TYPE's row (the normal-quality
 * tuning) and its quality's percentage, which is the whole of how thirty-five gems run off seven
 * rows of numbers.
 */
const GemData *ResolveGem(uint16_t idx, int &percent)
{
	percent = 100;
	GemType type;
	GemQuality quality;
	if (GemTypeAndQuality(idx, type, quality)) {
		percent = GemQualityPercent(quality);
		return FindGemRow(GemIndexFor(type, GemQuality::Normal));
	}
	return FindGemRow(idx);
}

/** @brief @p value at @p percent, never rounding a real effect away to nothing. */
int AtQuality(int value, int percent)
{
	if (value == 0)
		return 0;
	const int scaled = value * percent / 100;
	return scaled > 0 ? scaled : 1;
}

} // namespace

void ApplyGemToTotals(uint16_t gemIdx, SocketHost host, ItemBonusTotals &totals)
{
	int percent = 100;
	const GemData *gem = ResolveGem(gemIdx, percent);
	if (gem == nullptr)
		return;
	const auto at = [percent](int value) { return AtQuality(value, percent); };
	switch (host) {
	case SocketHost::Weapon:
		totals.fireMin += at(at(gem->weaponFireMin));
		totals.fireMax += at(at(gem->weaponFireMax));
		totals.lightningMin += at(at(gem->weaponLightMin));
		totals.lightningMax += at(at(gem->weaponLightMax));
		totals.bonusToHit += at(at(gem->weaponToHit));
		totals.damageMod += at(at(gem->weaponDamageMod));
		break;
	case SocketHost::Shield:
		totals.fireResist += at(at(gem->shieldFireRes));
		totals.lightningResist += at(at(gem->shieldLightRes));
		totals.magicResist += at(at(gem->shieldMagicRes));
		totals.bonusArmor += at(at(gem->shieldBonusAc));
		break;
	case SocketHost::Armor:
		totals.fireResist += at(at(gem->armorFireRes));
		totals.lightningResist += at(at(gem->armorLightRes));
		totals.magicResist += at(at(gem->armorMagicRes));
		totals.hitPoints += at(at(gem->armorHitPoints)) << 6; // HP fields run in <<6 fixed point
		totals.bonusArmor += at(at(gem->armorBonusAc));
		totals.dexterity += at(gem->armorDexterity);
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
	int percent = 100;
	const GemData *gem = ResolveGem(gemIdx, percent);
	if (gem == nullptr)
		return {};
	const auto at = [percent](int value) { return AtQuality(value, percent); };
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
		if (at(gem->weaponFireMax) > 0)
			add(fmt::format(fmt::runtime(_("+{:d}-{:d} fire damage")), at(gem->weaponFireMin), at(gem->weaponFireMax)));
		if (at(gem->weaponLightMax) > 0)
			add(fmt::format(fmt::runtime(_("+{:d}-{:d} lightning damage")), at(gem->weaponLightMin), at(gem->weaponLightMax)));
		if (at(gem->weaponToHit) > 0)
			add(fmt::format(fmt::runtime(_("+{:d}% to hit")), at(gem->weaponToHit)));
		if (at(gem->weaponDamageMod) > 0)
			add(fmt::format(fmt::runtime(_("+{:d} damage")), at(gem->weaponDamageMod)));
		break;
	case SocketHost::Shield:
		if (at(gem->shieldFireRes) > 0)
			add(fmt::format(fmt::runtime(_("+{:d}% fire resist")), at(gem->shieldFireRes)));
		if (at(gem->shieldLightRes) > 0)
			add(fmt::format(fmt::runtime(_("+{:d}% lightning resist")), at(gem->shieldLightRes)));
		if (at(gem->shieldMagicRes) > 0)
			add(fmt::format(fmt::runtime(_("+{:d}% magic resist")), at(gem->shieldMagicRes)));
		if (at(gem->shieldBonusAc) > 0)
			add(fmt::format(fmt::runtime(_("+{:d} armor")), at(gem->shieldBonusAc)));
		break;
	case SocketHost::Armor:
		if (at(gem->armorFireRes) > 0)
			add(fmt::format(fmt::runtime(_("+{:d}% fire resist")), at(gem->armorFireRes)));
		if (at(gem->armorLightRes) > 0)
			add(fmt::format(fmt::runtime(_("+{:d}% lightning resist")), at(gem->armorLightRes)));
		if (at(gem->armorMagicRes) > 0)
			add(fmt::format(fmt::runtime(_("+{:d}% magic resist")), at(gem->armorMagicRes)));
		if (at(gem->armorHitPoints) > 0)
			add(fmt::format(fmt::runtime(_("+{:d} life")), at(gem->armorHitPoints)));
		if (at(gem->armorBonusAc) > 0)
			add(fmt::format(fmt::runtime(_("+{:d} armor")), at(gem->armorBonusAc)));
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
			const GemData *gem = FindGemRow(socket);
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
