#include "oracool/gems.h"

#include <fmt/format.h>

#include <algorithm>

#include "inv.h"
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
	// Weapon host.
	int weaponFireMin = 0, weaponFireMax = 0;
	int weaponLightMin = 0, weaponLightMax = 0;
	int weaponToHit = 0;
	int weaponDamageMod = 0;
	int weaponMana = 0;
	/** Weapon host only - Skull's leech substitute, resolved per kill in GemLifePerKill (which
	 * scales it by quality, unlike the flat rune fields below). */
	int lifePerKill = 0;
	// Armor host (body, helm, and the worn accessories).
	int armorFireRes = 0, armorLightRes = 0, armorMagicRes = 0;
	int armorHitPoints = 0; // in whole HP; converted to <<6 fixed point at apply time
	int armorMana = 0;      // whole mana; same fixed-point conversion
	int armorBonusAc = 0;
	int armorStrength = 0;
	int armorDexterity = 0;
	int armorToHit = 0;
	int armorMagicFind = 0;
	// Shield host.
	int shieldFireRes = 0, shieldLightRes = 0, shieldMagicRes = 0;
	int shieldBonusAc = 0;
	int shieldMana = 0;
	bool shieldThorns = false; // ItemSpecialEffect::Thorns - a flag, so it does NOT quality-scale
	// Every host (the runes' D2 mechanics; unscaled - runes have no quality ladder).
	int lightRadius = 0;
	int manaPerKill = 0;     // Tir's +2, granted by GrantRuneKillMana on each kill
	int damageReduction = 0; // armor and shield hosts - Sol's flat -7 damage taken

	// Sockets v2 additions. D2's full 33-rune sheet needs channels the five launch runes never
	// touched: a percentage damage roll, the four attributes and the two find stats as
	// every-host effects, and the engine's own special-effect flags, which is where D2's
	// knockback, attack speed, hit recovery, block speed, steal and demon damage actually live.
	int weaponDamagePercent = 0;
	int allStrength = 0, allMagic = 0, allDexterity = 0, allVitality = 0;
	int allMagicFind = 0, allGoldFind = 0;
	ItemSpecialEffect weaponFlags = ItemSpecialEffect::None;
	ItemSpecialEffect armorFlags = ItemSpecialEffect::None;
	ItemSpecialEffect shieldFlags = ItemSpecialEffect::None;

	/**
	 * @brief Hel: the host's own strength/magic/dexterity requirements, reduced by this percent.
	 *
	 * Not a totals field, because requirements are not a total - they are the item's own numbers,
	 * compared against the player's in Player::CanUseItem. Read there and in the requirement line
	 * of the item panel through EffectiveRequirement, so nothing is ever written back into the
	 * item and the reduction cannot compound across recalculations.
	 */
	int requirementPercentReduction = 0;
	/** @brief Zod: the host cannot lose durability. Read where durability is spent, same rule. */
	bool indestructible = false;
};

constexpr GemData Gems[] = {
	// The seven gems, each on its Diablo II column (user directive 2026-08-18: "apply the D2
	// affixes to them"). Every row is the NORMAL quality of its type; the other four qualities are
	// the same row scaled by GemQualityPercent - Normal numbers are chosen so Perfect (x2) lands on
	// D2's own Perfect values. Designated initializers on purpose: the old positional rows were one
	// inserted field away from silently re-reading every trailing number as something else.
	//
	// Four substitutions where D2 leans on channels this engine lacks, documented rather than
	// fudged silently:
	//   - Diamond's weapon "+% damage vs undead" has no channel -> flat damage.
	//   - Emerald's weapon poison damage has no channel -> flat damage; its poison resists -> magic.
	//   - Sapphire is D2's COLD gem and D1 has no cold at all -> it becomes the mana gem, its D2
	//     armor identity (+38 mana at Perfect) extended to every host; cold resist -> magic resist.
	//   - Skull's steal percentages ride D1 flags that only exist at fixed 3%/5% and cannot scale,
	//     so the weapon gets +life per kill instead (same drain fantasy, quality-scaled), the armor
	//     gets life+mana (for "replenish life / regenerate mana"), and the shield keeps D2's thorns
	//     via the engine's own Thorns flag.
	//
	// Amethyst: W +8% to-hit (D2 attack rating); A +5 strength (P: +10 = D2); S +15 AC (P: +30 = D2).
	{ .idx = IDI_ORACOOL_GEM_AMETHYST_NORMAL, .weaponToHit = 8, .armorStrength = 5, .shieldBonusAc = 15 },
	// Diamond: W +3 damage (undead substitute); A +5% to-hit (D2 attack rating);
	// S +10 all resists (P: +20 ~= D2's 19).
	{ .idx = IDI_ORACOOL_GEM_DIAMOND_NORMAL, .weaponDamageMod = 3, .armorToHit = 5,
	    .shieldFireRes = 10, .shieldLightRes = 10, .shieldMagicRes = 10 },
	// Ruby: W 8-12 fire (D2's Normal ruby exactly); A +19 life (P: +38 = D2); S +20% FR (P: 40 = D2).
	{ .idx = IDI_ORACOOL_GEM_RUBY, .weaponFireMin = 8, .weaponFireMax = 12,
	    .armorHitPoints = 19, .shieldFireRes = 20 },
	// Sapphire: the mana gem (cold substitute) - W +10 mana; A +19 mana (P: +38 = D2);
	// S +12% magic resist and +10 mana.
	{ .idx = IDI_ORACOOL_GEM_SAPPHIRE, .weaponMana = 10, .armorMana = 19,
	    .shieldMagicRes = 12, .shieldMana = 10 },
	// Topaz: W 1-20 lightning (P: 2-40 ~= D2's 1-40); A +12% magic find (P: 24 = D2);
	// S +20% LR (P: 40 = D2).
	{ .idx = IDI_ORACOOL_GEM_TOPAZ, .weaponLightMin = 1, .weaponLightMax = 20,
	    .armorMagicFind = 12, .shieldLightRes = 20 },
	// Emerald: W +4 damage (poison substitute); A +5 dexterity (P: +10 = D2);
	// S +20% magic resist (poison resist substitute; P: 40 = D2).
	{ .idx = IDI_ORACOOL_GEM_EMERALD, .weaponDamageMod = 4, .armorDexterity = 5, .shieldMagicRes = 20 },
	// Skull: W +2 life per kill (leech substitute, quality-scaled); A +8 life +8 mana
	// (replenish/regenerate substitute); S thorns (D2's attacker-takes-damage) +4 AC.
	{ .idx = IDI_ORACOOL_GEM_SKULL, .lifePerKill = 2, .armorHitPoints = 8, .armorMana = 8,
	    .shieldBonusAc = 4, .shieldThorns = true },
	// The runes, to Diablo II's own sheet (user directive 2026-08-16). Two adaptations where the
	// engines differ, both documented rather than fudged silently:
	//   - El's +50 Attack Rating maps to +5% to-hit (D2's own ~10 AR : 1% convention).
	//   - Sol's "+9 to minimum damage" becomes +9 flat damage: D1 has no min-only channel, and
	//     pushing only the min can cross the max on small weapons, which D1's damage roll
	//     (GenerateRnd over max-min) does not survive.
	// El: +5% to-hit in weapons / +15 defense elsewhere; +1 light radius everywhere.
	{ .idx = IDI_ORACOOL_RUNE_EL, .weaponToHit = 5, .armorBonusAc = 15, .shieldBonusAc = 15, .lightRadius = 1 },
	// Tir: +2 mana after each kill, in every host.
	{ .idx = IDI_ORACOOL_RUNE_TIR, .manaPerKill = 2 },
	// Ral: adds 5-30 fire damage / Fire Resist +30% armor, +35% shield.
	{ .idx = IDI_ORACOOL_RUNE_RAL, .weaponFireMin = 5, .weaponFireMax = 30, .armorFireRes = 30, .shieldFireRes = 35 },
	// Ort: adds 1-50 lightning damage / Lightning Resist +30% armor, +35% shield.
	{ .idx = IDI_ORACOOL_RUNE_ORT, .weaponLightMin = 1, .weaponLightMax = 50, .armorLightRes = 30, .shieldLightRes = 35 },
	// Sol: +9 damage in weapons / Damage Reduced by 7 in armor and shields.
	{ .idx = IDI_ORACOOL_RUNE_SOL, .weaponDamageMod = 9, .damageReduction = 7 },
	// The other 28, in Diablo II's own order. GENERATED by tools/GenRunes.ps1 - every adaptation
	// where this engine lacks D2's channel is named in a comment on its own row.
#include "oracool/runes_effects.inc"
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

namespace {

/**
 * @brief The 33 runes in Diablo II's order.
 *
 * This list exists because the ENUM order is not the rune order and cannot be: the five that
 * shipped in v1.7.8 sit before the charms and the whole gem ladder, and the other 28 are appended
 * after all of it. Anything that means "the next rune up" - the crafting ladder, the drop walk -
 * has to come through here. Doing it with `index + 1` gave Eld -> Nef (skipping Tir) and walked
 * Sol straight into a charm.
 */
constexpr uint16_t RuneOrder[] = {
#include "oracool/runes_order.inc"
};
constexpr size_t RuneCount = sizeof(RuneOrder) / sizeof(RuneOrder[0]);
static_assert(RuneCount == 33, "Diablo II has 33 runes - regenerate with tools/GenRunes.ps1");

} // namespace

size_t RuneLadderSize() { return RuneCount; }

uint16_t RuneAtLadderPosition(size_t position)
{
	return position < RuneCount ? RuneOrder[position] : Item::EmptySocket;
}

uint16_t NextRune(uint16_t runeIdx)
{
	for (size_t i = 0; i + 1 < RuneCount; i++) {
		if (RuneOrder[i] == runeIdx)
			return RuneOrder[i + 1];
	}
	return runeIdx; // Zod, or not a rune at all - nothing above either.
}

bool IsTopRune(uint16_t runeIdx)
{
	return runeIdx == RuneOrder[RuneCount - 1];
}

int MaxSocketsForItem(const Item &item)
{
	// Sockets v2 (user directive 2026-08-19): "max number of sockets = number of 28x28px boxes the
	// item is made of". GetInventorySize already answers that in cells - it is the same number the
	// backpack grid uses to place the item, so a socket cap can never disagree with what the player
	// sees. Clamped to the record's own width, which is sized for the largest footprint (2x3).
	const Size size = GetInventorySize(item);
	return std::clamp(size.width * size.height, 0, Item::MaxItemSockets);
}

bool CanItemHaveSockets(const Item &item)
{
	if (item.isEmpty())
		return false;
	switch (item._itype) {
	case ItemType::Ring:
	case ItemType::Amulet:
		// Jewelry has no basic versions to roll on - a ring is magic or better by construction -
		// so for these two the basic-only rule would mean "never". They socket at any quality
		// instead; being 1x1 they get exactly one socket, which keeps them a choice rather than a
		// second equipment slot.
		break;
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
		// Everything else stays basic-only: a magic sword has already been rolled on, and letting
		// it socket too would leave the white sword with no role at all. The base TIER no longer
		// disqualifies anything, though - a Torment base is the better host, and excluding it made
		// the deeper item strictly worse raw material, which is backwards.
		if (item._iMagical != ITEM_QUALITY_NORMAL)
			return false;
		break;
	default:
		return false;
	}
	return MaxSocketsForItem(item) > 0;
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
	// ONE application of the quality percentage per number - the same single at() the tooltip in
	// GemSocketLine applies, so what the line promises is what the mechanics deliver. This block
	// shipped as at(at(...)) on every field but dexterity (external audit, 2026-08-17), squaring
	// the quality scale: a Perfect gem's 200% became 400%, so a ruby DISPLAYING 4-12 fire damage
	// was mechanically granting 8-24. The one single-wrapped field is what proved the doubling
	// was a copy-paste accident rather than a design.
	const auto at = [percent](int value) { return AtQuality(value, percent); };
	switch (host) {
	case SocketHost::Weapon:
		totals.fireMin += at(gem->weaponFireMin);
		totals.fireMax += at(gem->weaponFireMax);
		totals.lightningMin += at(gem->weaponLightMin);
		totals.lightningMax += at(gem->weaponLightMax);
		totals.bonusToHit += at(gem->weaponToHit);
		totals.damageMod += at(gem->weaponDamageMod);
		totals.mana += at(gem->weaponMana) << 6; // mana runs in <<6 fixed point
		totals.bonusDamage += at(gem->weaponDamagePercent);
		totals.flags |= gem->weaponFlags;
		// lifePerKill is deliberately absent here: it is an EVENT, not a stat - resolved per kill
		// in GemLifePerKill, the same shape as the runes' manaPerKill.
		break;
	case SocketHost::Shield:
		totals.fireResist += at(gem->shieldFireRes);
		totals.lightningResist += at(gem->shieldLightRes);
		totals.magicResist += at(gem->shieldMagicRes);
		totals.bonusArmor += at(gem->shieldBonusAc);
		totals.mana += at(gem->shieldMana) << 6;
		if (gem->shieldThorns)
			totals.flags |= ItemSpecialEffect::Thorns; // same route the class tree's thorns take
		totals.flags |= gem->shieldFlags;
		break;
	case SocketHost::Armor:
		totals.fireResist += at(gem->armorFireRes);
		totals.lightningResist += at(gem->armorLightRes);
		totals.magicResist += at(gem->armorMagicRes);
		totals.hitPoints += at(gem->armorHitPoints) << 6; // HP fields run in <<6 fixed point
		totals.mana += at(gem->armorMana) << 6;
		totals.bonusArmor += at(gem->armorBonusAc);
		totals.strength += at(gem->armorStrength);
		totals.dexterity += at(gem->armorDexterity);
		totals.bonusToHit += at(gem->armorToHit);
		totals.magicFind += at(gem->armorMagicFind);
		totals.flags |= gem->armorFlags;
		break;
	}
	// Host-independent effects (the D2 rune column): light radius everywhere; Sol's flat damage
	// reduction only where D2 grants it, armor and shields. getHit is the vanilla damage-taken
	// channel - negative is beneficial, CalcPlrItemVals shifts it into <<6 fixed point.
	totals.lightRadius += gem->lightRadius;
	if (host != SocketHost::Weapon)
		totals.getHit -= gem->damageReduction;

	// The every-host runes (Io, Lum, Ko, Fal, Lem, Ist). D2 grants these in any host, so they sit
	// outside the switch rather than being repeated three times - repeating them is how the three
	// arms drift apart.
	totals.strength += gem->allStrength;
	totals.magic += gem->allMagic;
	totals.dexterity += gem->allDexterity;
	totals.vitality += gem->allVitality;
	totals.magicFind += gem->allMagicFind;
	totals.goldFind += gem->allGoldFind;
}

int SocketRequirementReductionPercent(const Item &item)
{
	// Hel, summed over the host's own filled sockets and capped so a six-Hel two-hander cannot
	// drive a requirement to zero - the requirement is meant to be reduced, not deleted.
	if (item.isEmpty() || item._iSocketCount == 0)
		return 0;
	int percent = 0;
	for (const uint16_t socketed : item._iSocketed) {
		if (socketed == Item::EmptySocket)
			continue;
		const GemData *gem = FindGemRow(socketed);
		if (gem != nullptr)
			percent += gem->requirementPercentReduction;
	}
	return std::min(percent, 60);
}

bool SocketsMakeIndestructible(const Item &item)
{
	// Zod. One is enough, which is why this answers yes/no rather than summing anything.
	if (item.isEmpty() || item._iSocketCount == 0)
		return false;
	for (const uint16_t socketed : item._iSocketed) {
		if (socketed == Item::EmptySocket)
			continue;
		const GemData *gem = FindGemRow(socketed);
		if (gem != nullptr && gem->indestructible)
			return true;
	}
	return false;
}

int EffectiveRequirement(const Item &item, int baseRequirement)
{
	if (baseRequirement <= 0)
		return baseRequirement;
	const int reduction = SocketRequirementReductionPercent(item);
	if (reduction == 0)
		return baseRequirement;
	// Never below 1: a requirement that exists should still be a requirement.
	return std::max(1, baseRequirement * (100 - reduction) / 100);
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
		if (at(gem->weaponMana) > 0)
			add(fmt::format(fmt::runtime(_("+{:d} mana")), at(gem->weaponMana)));
		if (at(gem->lifePerKill) > 0)
			add(fmt::format(fmt::runtime(_("+{:d} life per kill")), at(gem->lifePerKill)));
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
		if (at(gem->shieldMana) > 0)
			add(fmt::format(fmt::runtime(_("+{:d} mana")), at(gem->shieldMana)));
		if (gem->shieldThorns)
			add(std::string(_("attackers take damage")));
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
		if (at(gem->armorMana) > 0)
			add(fmt::format(fmt::runtime(_("+{:d} mana")), at(gem->armorMana)));
		if (at(gem->armorBonusAc) > 0)
			add(fmt::format(fmt::runtime(_("+{:d} armor")), at(gem->armorBonusAc)));
		if (at(gem->armorStrength) > 0)
			add(fmt::format(fmt::runtime(_("+{:d} strength")), at(gem->armorStrength)));
		if (at(gem->armorDexterity) > 0)
			add(fmt::format(fmt::runtime(_("+{:d} dexterity")), at(gem->armorDexterity)));
		if (at(gem->armorToHit) > 0)
			add(fmt::format(fmt::runtime(_("+{:d}% to hit")), at(gem->armorToHit)));
		if (at(gem->armorMagicFind) > 0)
			add(fmt::format(fmt::runtime(_("+{:d}% magic find")), at(gem->armorMagicFind)));
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

int GemLifePerKill(const Player &player)
{
	// The Skull's leech substitute. Unlike RuneManaPerKill above, this resolves through
	// ResolveGem rather than FindGemRow: a Flawless or Perfect skull is its own item index that
	// only ResolveGem maps back to the type's Normal row and quality percent - FindGemRow would
	// silently answer 0 for every quality but Normal. Weapon hosts only, D2's own placement.
	int total = 0;
	for (const Item &item : player.InvBody) {
		if (item.isEmpty() || !item._iStatFlag)
			continue;
		if (SocketHostForItemType(item._itype) != SocketHost::Weapon)
			continue;
		for (const uint16_t socket : item._iSocketed) {
			if (socket == Item::EmptySocket)
				continue;
			int percent = 100;
			const GemData *gem = ResolveGem(socket, percent);
			if (gem != nullptr && gem->lifePerKill > 0)
				total += AtQuality(gem->lifePerKill, percent);
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
			ApplyZodToHost(target);
			return true;
		}
	}
	return false;
}

void ApplyZodToHost(Item &item)
{
	// Zod is the one rune written INTO the host rather than read off it, and deliberately so:
	// "indestructible" is not a total, it is a durability VALUE this engine already has - vanilla
	// uniques set exactly this. Ten separate durability-decrement sites all test for it, so
	// setting it once here is both cheaper and less fragile than a check at each.
	//
	// _iMaxDur is untouched, which is what lets extraction put the item back: restoring
	// _iDurability = _iMaxDur returns it to a full, destructible item.
	if (item.isEmpty() || item._iMaxDur == 0)
		return;
	if (SocketsMakeIndestructible(item))
		item._iDurability = DUR_INDESTRUCTIBLE;
}

} // namespace devilution::oracool
