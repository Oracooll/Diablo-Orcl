#include "oracool/imbuement.h"

#include <algorithm>
#include <cstdlib>

#include <fmt/format.h>

#include "items.h"
#include "oracool/stat_sheet.h"
#include "player.h"
#include "utils/language.h"

namespace devilution::oracool {

namespace {

constexpr ShardDefinition Shards[] = {
#include "oracool/shards_kinds.inc"
};
static_assert(sizeof(Shards) / sizeof(Shards[0]) == ShardKindCount, "shards_kinds.inc and ShardKind disagree on the count");

/** Per-shard values - decision D6, the table as proposed. Tuned to a cap of twenty. */
constexpr int StatStep = 1;
constexpr int WardingStep = 2;
constexpr int FuryStep = 1;
constexpr int FortuneStep = 2;
constexpr int AvariceStep = 3;
constexpr int BloodStep = 5;
constexpr int SpiritStep = 5;
constexpr int KeennessStep = 2;
constexpr int PrecisionStep = 2;
constexpr int FlameMin = 1, FlameMax = 2;
constexpr int SparkMin = 1, SparkMax = 3;
constexpr int BulwarkStep = 2;
constexpr int StoneStep = 1;
constexpr int SingleResistStep = 3;
constexpr int RadianceStep = 1;
constexpr int ArcanaStep = 1;
constexpr int RefinementPercent = 3;
constexpr int TemperingStep = 10;
constexpr int EaseStep = 3;

bool IsGear(const Item &item)
{
	if (item.isEmpty())
		return false;
	if (item._iClass != ICLASS_WEAPON && item._iClass != ICLASS_ARMOR)
		return false;
	// The socketable families are ICLASS_MISC and so already excluded, but they are named here as
	// well because "a shard cannot go into a jewel" is a rule worth being able to find.
	if (IsOracoolShardIdx(item.IDidx) || IsOracoolGemIdx(item.IDidx) || IsOracoolRuneIdx(item.IDidx)
	    || IsOracoolJewelIdx(item.IDidx) || IsOracoolSalvageIdx(item.IDidx))
		return false;
	return true;
}

/** Whether Ease still has anything to take: at least one requirement that is not yet at zero. */
bool EaseHasWork(const Item &item)
{
	const int cut = ShardRequirementReduction(item);
	return (item._iMinStr > 0 && item._iMinStr - cut > 0)
	    || (item._iMinMag > 0 && item._iMinMag - cut > 0)
	    || (item._iMinDex > 0 && item._iMinDex - cut > 0);
}

/** @p percent of @p value, rounded away from zero so a small affix still moves. */
int Scaled(int value, int percent)
{
	if (value == 0 || percent == 0)
		return 0;
	const int scaled = value * percent;
	// At least one point whenever there is anything to scale: round-half-up alone left every affix
	// under 17 at zero for a 3% shard, while the shard's line promised "every affix" (audit, 2026-09-19).
	const int magnitude = std::max(1, (std::abs(scaled) + 50) / 100);
	return scaled < 0 ? -magnitude : magnitude;
}

} // namespace

const ShardDefinition &ShardDef(ShardKind kind)
{
	return Shards[static_cast<int>(kind)];
}

const ShardDefinition *FindShardByItem(int idx)
{
	for (const ShardDefinition &def : Shards) {
		if (def.itemIndex == idx)
			return &def;
	}
	return nullptr;
}

// The ledger and the item must agree on the cap, or RestoreImbuements writes past the ledger's array.
static_assert(MaxImbuementKinds == Item::MaxOracoolImbuements, "ImbuementLedger::kinds must hold every imbuement an Item can");

int ShardLimit(ShardKind kind)
{
	const uint8_t limit = ShardDef(kind).limit;
	return limit == 0 ? Item::MaxOracoolImbuements : limit;
}

int ShardCountOfKind(const Item &item, ShardKind kind)
{
	int n = 0;
	for (int i = 0; i < item._iOracoolImbueCount && i < Item::MaxOracoolImbuements; i++) {
		if (item._iOracoolImbuements[i] == static_cast<uint8_t>(kind))
			n++;
	}
	return n;
}

bool CanReceiveShard(const Item &target)
{
	return IsGear(target) && target._iOracoolImbueCount < Item::MaxOracoolImbuements;
}

bool CanReceiveShardKind(const Item &target, ShardKind kind)
{
	if (!CanReceiveShard(target))
		return false;
	if (ShardCountOfKind(target, kind) >= ShardLimit(kind))
		return false;
	switch (kind) {
	case ShardKind::Tempering:
		// Durability an indestructible item does not have - and never INTO the sentinel: 255 means
		// "cannot break", and _iMaxDur is a byte in the packed hero record, so the durability affix
		// caps at 254 (items.cpp, IPL_DUR) and a shard that would cross that line is refused outright
		// rather than clamped, so the player is not sold a shard that does less than its line says.
		// Both sentinels: a unique's indestructibility lives in _iMaxDur, a Zod rune's in _iDurability
		// alone (ApplyZodToHost leaves _iMaxDur so extraction can put the item back). Adding ten to a
		// _iDurability of 255 made the item destructible with the rune still listed (audit, 2026-09-19).
		return target._iMaxDur != DUR_INDESTRUCTIBLE && target._iDurability != DUR_INDESTRUCTIBLE
		    && target._iMaxDur + TemperingStep < DUR_INDESTRUCTIBLE;
	case ShardKind::Ease:
		return EaseHasWork(target);
	default:
		return true;
	}
}

bool TryImbue(const Player & /*player*/, Item &target, const Item &held)
{
	if (held.isEmpty())
		return false;
	const ShardDefinition *def = FindShardByItem(held.IDidx);
	if (def == nullptr)
		return false;
	if (!CanReceiveShardKind(target, def->kind))
		return false;

	target._iOracoolImbuements[target._iOracoolImbueCount] = static_cast<uint8_t>(def->kind);
	target._iOracoolImbueCount++;
	if (def->kind == ShardKind::Tempering) {
		// Durability is an item field, not a sheet total, so it moves now - and the current
		// durability moves with it, so the shard is felt at once rather than at the next repair.
		target._iMaxDur += TemperingStep;
		target._iDurability += TemperingStep;
	}
	return true;
}

std::string ImbueCountLine(const Item &item)
{
	// Shown on anything that HAS taken a shard, and on anything that COULD - a player needs to know
	// how much room is left before they spend, and an item silently at its cap is the one thing
	// this line exists to prevent.
	if (item._iOracoolImbueCount == 0 && !CanReceiveShard(item))
		return {};
	return fmt::format(fmt::runtime(_("Imbued: {:d} / {:d}")), item._iOracoolImbueCount, Item::MaxOracoolImbuements);
}

std::string ImbueBreakdownLine(const Item &item)
{
	if (item._iOracoolImbueCount == 0)
		return {};
	std::string out;
	for (int k = 0; k < ShardKindCount; k++) {
		const ShardKind kind = static_cast<ShardKind>(k);
		const int n = ShardCountOfKind(item, kind);
		if (n == 0)
			continue;
		if (!out.empty())
			out += ", ";
		// "Shard of Strength" -> "Strength": the word after the last space.
		const std::string name(_(ShardDef(kind).name));
		const size_t space = name.rfind(' ');
		out += space == std::string::npos ? name : name.substr(space + 1);
		out += fmt::format(" x{:d}", n);
	}
	return out;
}

const char *ShardLine(int idx)
{
	const ShardDefinition *def = FindShardByItem(idx);
	return def != nullptr ? def->line : "";
}

void ApplyImbuementsToTotals(const Item &item, ItemBonusTotals &totals)
{
	if (item.isEmpty() || !item._iStatFlag || item._iOracoolImbueCount == 0)
		return;

	int refinement = 0;
	for (int i = 0; i < item._iOracoolImbueCount && i < Item::MaxOracoolImbuements; i++) {
		const uint8_t raw = item._iOracoolImbuements[i];
		if (raw > static_cast<uint8_t>(ShardKind::LAST))
			continue;
		switch (static_cast<ShardKind>(raw)) {
		case ShardKind::Strength: totals.strength += StatStep; break;
		case ShardKind::Dexterity: totals.dexterity += StatStep; break;
		case ShardKind::Magic: totals.magic += StatStep; break;
		case ShardKind::Vitality: totals.vitality += StatStep; break;
		case ShardKind::Warding:
			totals.fireResist += WardingStep;
			totals.lightningResist += WardingStep;
			totals.magicResist += WardingStep;
			break;
		case ShardKind::Fury: totals.damageMod += FuryStep; break;
		case ShardKind::Fortune: totals.magicFind += FortuneStep; break;
		case ShardKind::Avarice: totals.goldFind += AvariceStep; break;
		// Life and mana ride in 64ths, as IPL_LIFE and IPL_MANA write them (items.cpp SaveItemPower).
		case ShardKind::Blood: totals.hitPoints += BloodStep << 6; break;
		case ShardKind::Spirit: totals.mana += SpiritStep << 6; break;
		case ShardKind::Keenness: totals.bonusDamage += KeennessStep; break;
		case ShardKind::Precision: totals.bonusToHit += PrecisionStep; break;
		case ShardKind::Flame:
			totals.fireMin += FlameMin;
			totals.fireMax += FlameMax;
			totals.flags |= ItemSpecialEffect::FireDamage;
			break;
		case ShardKind::Spark:
			totals.lightningMin += SparkMin;
			totals.lightningMax += SparkMax;
			totals.flags |= ItemSpecialEffect::LightningDamage;
			break;
		case ShardKind::Bulwark: totals.bonusArmor += BulwarkStep; break;
		// IPL_GETHIT stores damage TAKEN as a negative, so less taken is a subtraction here too.
		case ShardKind::Stone: totals.getHit -= StoneStep; break;
		case ShardKind::Ember: totals.fireResist += SingleResistStep; break;
		case ShardKind::Storm: totals.lightningResist += SingleResistStep; break;
		case ShardKind::Veil: totals.magicResist += SingleResistStep; break;
		case ShardKind::Radiance: totals.lightRadius += RadianceStep; break;
		case ShardKind::Arcana: totals.spellLevelAdd += ArcanaStep; break;
		case ShardKind::Refinement: refinement++; break;
		case ShardKind::Tempering:
		case ShardKind::Ease:
			break; // item-local: durability and requirements, read elsewhere
		}
	}

	if (refinement == 0)
		return;
	// Refinement: +3% per shard of the item's OWN affix totals - decision D5. The item's contribution
	// to the sheet is recomputed here in isolation; base damage and armour are left out (they are the
	// base, not an affix), and so are the flags and granted spells, which have no magnitude to scale.
	// Gems, set bonuses and runewords come through their own providers and are never touched.
	ItemBonusTotals own;
	own.AddItem(item);
	const int percent = RefinementPercent * refinement;
	totals.bonusDamage += Scaled(own.bonusDamage, percent);
	totals.bonusToHit += Scaled(own.bonusToHit, percent);
	totals.bonusArmor += Scaled(own.bonusArmor, percent);
	totals.strength += Scaled(own.strength, percent);
	totals.magic += Scaled(own.magic, percent);
	totals.dexterity += Scaled(own.dexterity, percent);
	totals.vitality += Scaled(own.vitality, percent);
	totals.fireResist += Scaled(own.fireResist, percent);
	totals.lightningResist += Scaled(own.lightningResist, percent);
	totals.magicResist += Scaled(own.magicResist, percent);
	totals.damageMod += Scaled(own.damageMod, percent);
	totals.getHit += Scaled(own.getHit, percent);
	totals.lightRadius += Scaled(own.lightRadius, percent);
	totals.hitPoints += Scaled(own.hitPoints, percent);
	totals.mana += Scaled(own.mana, percent);
	totals.spellLevelAdd += Scaled(own.spellLevelAdd, percent);
	totals.enhancedAccuracy += Scaled(own.enhancedAccuracy, percent);
	totals.fireMin += Scaled(own.fireMin, percent);
	totals.fireMax += Scaled(own.fireMax, percent);
	totals.lightningMin += Scaled(own.lightningMin, percent);
	totals.lightningMax += Scaled(own.lightningMax, percent);
	totals.magicFind += Scaled(own.magicFind, percent);
	totals.goldFind += Scaled(own.goldFind, percent);
	totals.moveSpeed += Scaled(own.moveSpeed, percent);
	totals.fastCast += Scaled(own.fastCast, percent);
}

int ShardRequirementReduction(const Item &item)
{
	return EaseStep * ShardCountOfKind(item, ShardKind::Ease);
}

int ShardDurabilityBonus(const Item &item)
{
	return TemperingStep * ShardCountOfKind(item, ShardKind::Tempering);
}

ImbuementLedger CaptureImbuements(const Item &item)
{
	ImbuementLedger ledger;
	ledger.count = std::min<uint8_t>(item._iOracoolImbueCount, Item::MaxOracoolImbuements);
	for (int i = 0; i < ledger.count; i++)
		ledger.kinds[i] = item._iOracoolImbuements[i];
	return ledger;
}

void RestoreImbuements(Item &item, const ImbuementLedger &ledger)
{
	if (ledger.count == 0 || !IsGear(item))
		return;
	item._iOracoolImbueCount = ledger.count;
	for (int i = 0; i < ledger.count; i++)
		item._iOracoolImbuements[i] = ledger.kinds[i];
	// The rebuilt item came out of InitializeItem with its base durability; Tempering goes back on -
	// capped below the sentinel like the affix, because the rebuilt base may be larger than the old one.
	const int bonus = ShardDurabilityBonus(item);
	if (bonus > 0 && item._iMaxDur != DUR_INDESTRUCTIBLE) {
		const int granted = std::min(bonus, DUR_INDESTRUCTIBLE - 1 - item._iMaxDur);
		if (granted > 0) {
			item._iMaxDur += granted;
			// Not over a Zod rune's sentinel (audit, 2026-09-19): 255 + 10 is a destructible 265.
			if (item._iDurability != DUR_INDESTRUCTIBLE)
				item._iDurability += granted;
		}
	}
}

void StripImbuements(Item &item)
{
	const int bonus = ShardDurabilityBonus(item);
	if (bonus > 0 && item._iMaxDur != DUR_INDESTRUCTIBLE) {
		item._iMaxDur = std::max(1, item._iMaxDur - bonus);
		// A Zod rune's 255 is a sentinel, not a durability; clamping it to the max would turn the
		// rune off on a Cleanse (audit, 2026-09-19).
		if (item._iDurability != DUR_INDESTRUCTIBLE)
			item._iDurability = std::min(item._iDurability, item._iMaxDur);
	}
	item._iOracoolImbueCount = 0;
	item._iOracoolImbuements.fill(0);
}

} // namespace devilution::oracool
