#include "oracool/item_tiers.h"

#include <algorithm>
#include <string>

#include "items.h"
#include "oracool/area_level.h"
#include "multi.h"
#include "options.h"
#include "utils/language.h"
#include "utils/str_cat.hpp"
#include "utils/utf8.hpp"

namespace devilution::oracool {

namespace {

/**
 * @brief Per-tier multipliers, in percent.
 *
 * Damage and armour climb fastest, requirements more slowly - so a Torment base is a real upgrade
 * rather than a wall, and a character who finds one early can plausibly grow into it. Durability
 * barely moves: a tougher sword is not a sword you repair four times less often, and repair cost
 * already scales with value.
 *
 * Value is the steep one on purpose. It is what makes a low-tier find worth salvaging rather than
 * carrying to town, which is the economy the salvage materials will sit on.
 */
struct TierScale {
	int power;   // damage and armour
	int require; // strength/magic/dexterity minimums
	int durability;
	int value;
};

constexpr TierScale Scales[BaseItemTierCount] = {
	{ 100, 100, 100, 100 },  // Normal
	{ 180, 140, 125, 400 },  // Nightmare
	{ 290, 180, 150, 1200 }, // Hell
	{ 420, 220, 175, 3000 }, // Torment
};

/**
 * @brief How the roll spreads across the tiers a level allows, deepest first.
 *
 * Read as "of the tiers available here, take the top one 60 times in 100". At ilvl 1-24 only Normal
 * is available and the table collapses to it; by Torment all four are in play and the tail is what
 * keeps white items falling for the salvage economy.
 */
constexpr int TierWeights[BaseItemTierCount] = { 60, 25, 10, 5 };

int ScaleByPercent(int value, int percent)
{
	return (value * percent + 50) / 100;
}

uint8_t ScaleByte(uint8_t value, int percent)
{
	// Clamped to the byte the base table actually stores. Nothing in the vanilla data comes close -
	// the biggest max damage is 20 and the biggest AC 60 - but the clamp is what makes adding a
	// fifth tier later a balance question rather than a wrap-around bug.
	return static_cast<uint8_t>(std::min(ScaleByPercent(value, percent), 255));
}

} // namespace

BaseItemTier HighestTierForItemLevel(int itemLevel)
{
	// The four base tiers are spread across the first THREE difficulties, twelve rungs each, so the
	// deepest tier opens at ilvl 37 and is fully in reach by Hell/Hell's 48 (user, 2026-09-12:
	// "hell/hell should be the threshhold for reaching god tier items. everything should be droppable
	// by then"). Torment's sixteen rungs past it are for better ROLLS, not for new things - which is
	// exactly what QualityChancePerMille's top band gives. It was one tier per difficulty block.
	constexpr int RungsPerBaseTier = 12;
	const int block = std::clamp((std::max(itemLevel, 1) - 1) / RungsPerBaseTier, 0, BaseItemTierCount - 1);
	return static_cast<BaseItemTier>(block);
}

BaseItemTier TierForItem(int itemLevel, uint32_t seed)
{
	const int highest = static_cast<int>(HighestTierForItemLevel(itemLevel));
	int total = 0;
	for (int i = 0; i <= highest; i++)
		total += TierWeights[i];

	// Knuth's multiplicative hash with an xorshift finisher - enough mixing that neighbouring seeds
	// do not land on the same tier, and no state anywhere. See the header for why this must not be a
	// draw from the seeded stream.
	uint32_t hash = seed * 2654435761U;
	hash ^= hash >> 15;
	hash *= 2246822519U;
	hash ^= hash >> 13;
	int roll = static_cast<int>(hash % static_cast<uint32_t>(total));
	for (int i = 0; i <= highest; i++) {
		roll -= TierWeights[i];
		if (roll < 0)
			return static_cast<BaseItemTier>(highest - i);
	}
	return BaseItemTier::Normal;
}

const char *TierName(BaseItemTier tier)
{
	switch (tier) {
	case BaseItemTier::Nightmare:
		return N_("Nightmare");
	case BaseItemTier::Hell:
		return N_("Hell");
	case BaseItemTier::Torment:
		return N_("Torment");
	case BaseItemTier::Normal:
		break;
	}
	return N_("Normal");
}

UiFlags TierColor(BaseItemTier tier)
{
	switch (tier) {
	case BaseItemTier::Nightmare:
		return UiFlags::ColorBlue;
	case BaseItemTier::Hell:
		// Gold (user, 2026-09-26: "tier hell text should be gold"). ColorUiGold is the MENU's gold file and
		// drew blue in play; the books' gold is a value and reads the same on every palette.
		return UiFlags::ColorGold6;
	case BaseItemTier::Torment:
		return UiFlags::ColorSalmon; // "tier torment text should be salmon" (user, 2026-09-26)
	case BaseItemTier::Normal:
		break;
	}
	return UiFlags::ColorWhite;
}

int BandedQlvl(int authoredQlvl)
{
	// 99 is the table's "never drops" sentinel, not a level - it must stay above every ilvl the
	// ladder can produce, so it is passed through untouched.
	if (authoredQlvl >= 99)
		return authoredQlvl;

	// Seventeen groups across the authored 1-51, landing on the 1-48 ladder: the last group opens at
	// 48, Hell/Hell's own rung (user, 2026-09-12: "hell/hell should be the threshhold for reaching god
	// tier items. everything should be droppable by then"). A monster there carries alvl 45-48 as its
	// loot level, so the deepest base is in reach from that floor rather than from a difficulty past
	// it. The groups landed on 1-60 while the ladder ran to 96.
	//
	// Read the left column as "authored up to N" and the right as the ilvl that opens the group.
	static constexpr struct {
		int authoredMax;
		int banded;
	} Bands[] = {
		{ 3, 1 }, { 6, 4 }, { 9, 7 }, { 12, 10 }, { 15, 14 }, { 18, 17 },
		{ 21, 20 }, { 24, 23 }, { 27, 26 }, { 30, 30 }, { 33, 33 }, { 36, 36 },
		{ 39, 39 }, { 42, 42 }, { 45, 44 }, { 48, 46 }, { 51, 48 },
	};
	for (const auto &band : Bands) {
		if (authoredQlvl <= band.authoredMax)
			return band.banded;
	}
	return 48; // anything authored past the vanilla ladder still tops out at Hell/Hell
}

int QualityChancePerMille(OracoolItemTier quality, int itemLevel, int configuredPercent)
{
	if (configuredPercent <= 0)
		return 0;

	// Percent OF the configured chance, per band. The shape is the point: rare climbs steadily,
	// buffed unique more slowly, and primal starts at zero because a level-3 character finding a
	// perfect item would flatten the whole ladder in front of them.
	static constexpr int RareByBand[BaseItemTierCount] = { 10, 30, 70, 110 };
	static constexpr int BuffedUniqueByBand[BaseItemTierCount] = { 10, 25, 50, 80 };
	static constexpr int PrimalByBand[BaseItemTierCount] = { 0, 5, 15, 40 };

	const auto band = static_cast<size_t>(HighestTierForItemLevel(itemLevel));
	int scale = 0;
	switch (quality) {
	case OracoolItemTier::Rare:
		scale = RareByBand[band];
		break;
	case OracoolItemTier::BuffedUnique:
		scale = BuffedUniqueByBand[band];
		break;
	case OracoolItemTier::Primal:
		scale = PrimalByBand[band];
		break;
	default:
		return 0;
	}

	// Rares FIVE TIMES as often (user, 2026-09-13: "increase drop chance of rares 5 fold"). A factor
	// here rather than a new option default, because a saved diablo.ini keeps the old value.
	static constexpr int RareDropFactor = 5;
	if (quality == OracoolItemTier::Rare)
		scale *= RareDropFactor;

	// configured% x scale% -> per mille: (c/100) * (s/100) * 1000 = c * s / 10.
	return std::min(configuredPercent * scale / 10, 1000);
}

int VendorItemLevel(int vendorLevel)
{
	// The difficulty block, straight off the area ladder: sixteen rungs each since 2026-09-12, so
	// Normal is 1-16, Nightmare 17-32, Hell 33-48 and Torment 49-64. Read from the LADDER rather than
	// from the tier split, which is twelve rungs wide and no longer answers the same question.
	const int block = (AreaLevel(1, sgGameInitInfo.nDifficulty) - 1) / RungsPerDifficulty;
	return std::min(vendorLevel + RungsPerDifficulty * block, MaxAreaLevel);
}

void StampVendorItemLevel(Item &item, int vendorLevel)
{
	item._iOracoolItemLevel = static_cast<uint8_t>(std::clamp(VendorItemLevel(vendorLevel), 0, 255));
}

void ApplyVendorTier(Item &item, int vendorLevel, uint32_t seed, int maxValue)
{
	const int itemLevel = VendorItemLevel(vendorLevel);
	StampVendorItemLevel(item, vendorLevel);
	if (!CanCarryBaseTier(item))
		return;

	// Salted differently from TierForItem so the two questions are independent: a seed that says
	// "tiered" must not also decide WHICH tier by the same bits.
	uint32_t hash = (seed ^ 0x9E3779B9U) * 2654435761U;
	hash ^= hash >> 16;
	if (static_cast<int>(hash % 100U) >= *sgOptions.Oracool.vendorTieredStockChance)
		return; // Normal tier, which is what the base numbers already are

	const BaseItemTier tier = TierForItem(itemLevel, seed);

	// The vendor PRICE CAP, checked before the tier is applied rather than after.
	//
	// Every vendor generates in a retry loop that discards an item priced over its cap - and
	// SpawnOnePremium's loop is unbounded in Diablo, with a TODO in the engine warning it could spin
	// forever if nothing suitable can be generated. Torment multiplies value by thirty, so tiering
	// first and letting the loop reject would both skew shops toward the cheapest bases and push that
	// loop toward the failure its own comment predicts. Declining the tier keeps the item, the cap and
	// the loop all intact.
	if (maxValue > 0 && ScaleByPercent(item._ivalue, Scales[static_cast<size_t>(tier)].value) > maxValue)
		return;

	ApplyBaseTier(item, tier);
}

bool CanCarryBaseTier(const Item &item)
{
	// Worn gear only. A potion or a book has no damage, no armour and no requirements to scale, and
	// a "Cruel Book of Firebolt" would be a joke the game tells once.
	if (item.isEmpty())
		return false;
	return item._iLoc != ILOC_NONE && item._iLoc != ILOC_UNEQUIPABLE && item._iLoc != ILOC_BELT;
}

void ApplyBaseTier(Item &item, BaseItemTier tier)
{
	if (!CanCarryBaseTier(item))
		return;
	item._iOracoolBaseTier = static_cast<uint8_t>(tier);
	if (tier == BaseItemTier::Normal)
		return; // the numbers ARE Normal's - scaling by 100% would only risk a rounding drift

	const TierScale &scale = Scales[static_cast<size_t>(tier)];

	item._iMinDam = ScaleByPercent(item._iMinDam, scale.power);
	item._iMaxDam = ScaleByPercent(item._iMaxDam, scale.power);
	item._iAC = ScaleByPercent(item._iAC, scale.power);

	item._iMinStr = ScaleByte(item._iMinStr, scale.require);
	item._iMinMag = ScaleByte(item._iMinMag, scale.require);
	item._iMinDex = ScaleByte(item._iMinDex, scale.require);

	// Indestructible stays indestructible: 255 is a sentinel, not a quantity, and scaling it would
	// turn the game's own "cannot break" into a very large but finite number.
	if (item._iMaxDur != DUR_INDESTRUCTIBLE) {
		// NOT ScaleByte: its parameter is a uint8_t and _iMaxDur is an int, so anything already
		// above 255 - which an item that has been through a durability affix can be - was
		// truncated to its low eight bits on the way in, and 256 arrived as a zero. Clamped below
		// the sentinel, and never below 1, so scaling can shrink an item's lifespan but not end it.
		item._iMaxDur = std::clamp(ScaleByPercent(item._iMaxDur, scale.durability), 1, DUR_INDESTRUCTIBLE - 1);
		item._iDurability = std::clamp(item._iDurability, 0, item._iMaxDur);
	}

	item._ivalue = ScaleByPercent(item._ivalue, scale.value);
	item._iIvalue = ScaleByPercent(item._iIvalue, scale.value);

	// The NAME is deliberately left alone: nothing prepends a tier word to it. TierNamePrefix (Jagged / Cruel /
	// Primeval) was kept for callers that might want the word and none ever did; it was removed as dead code on
	// 2026-09-25 (tooltip audit), and the history has it.
	//
	// An earlier version did, so the tier could be read in the inventory grid. Two problems, both
	// caught by the pack tests. A magic item's name is composed as prefix + base + suffix against a
	// width budget, and the engine falls back to the base's SHORT name when it overflows - so
	// "Jagged Long Sword" came back as "Brutal Sword of gore", losing the tier AND the base. And on
	// a vendor item the affix name is built BEFORE this runs, giving "Lightning Jagged Maul": the
	// tier word wedged between the affix and the noun it belongs to.
	//
	// The coloured Tier line in the description carries it instead, which is where the request put
	// it (user, 2026-08-19: white / blue / yellow / gold).
}

} // namespace devilution::oracool
