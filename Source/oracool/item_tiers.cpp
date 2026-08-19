#include "oracool/item_tiers.h"

#include <algorithm>
#include <string>

#include "items.h"
#include "oracool/area_level.h"
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
	// One tier per 24-floor difficulty block, straight off the area ladder.
	const int block = std::clamp((std::max(itemLevel, 1) - 1) / AreaFloorCount, 0, BaseItemTierCount - 1);
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
		return UiFlags::ColorUiGold;
	case BaseItemTier::Torment:
		return UiFlags::ColorWhitegold;
	case BaseItemTier::Normal:
		break;
	}
	return UiFlags::ColorWhite;
}

const char *TierNamePrefix(BaseItemTier tier)
{
	switch (tier) {
	case BaseItemTier::Nightmare:
		return N_("Jagged");
	case BaseItemTier::Hell:
		return N_("Cruel");
	case BaseItemTier::Torment:
		return N_("Primeval");
	case BaseItemTier::Normal:
		break;
	}
	return "";
}

int BandedQlvl(int authoredQlvl)
{
	// 99 is the table's "never drops" sentinel, not a level - it must stay above every ilvl the
	// ladder can produce, so it is passed through untouched.
	if (authoredQlvl >= 99)
		return authoredQlvl;

	// Seventeen groups across the authored 1-51, landing on the 1-60 ladder. Read the left column as
	// "authored up to N" and the right as the ilvl that opens the group.
	static constexpr struct {
		int authoredMax;
		int banded;
	} Bands[] = {
		{ 3, 1 }, { 6, 5 }, { 9, 9 }, { 12, 13 }, { 15, 17 }, { 18, 21 },
		{ 21, 25 }, { 24, 29 }, { 27, 33 }, { 30, 37 }, { 33, 41 }, { 36, 45 },
		{ 39, 49 }, { 42, 52 }, { 45, 55 }, { 48, 57 }, { 51, 60 },
	};
	for (const auto &band : Bands) {
		if (authoredQlvl <= band.authoredMax)
			return band.banded;
	}
	return 60; // anything authored past the vanilla ladder still tops out at Hell/Hell
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

	// configured% x scale% -> per mille: (c/100) * (s/100) * 1000 = c * s / 10.
	return configuredPercent * scale / 10;
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
		item._iMaxDur = ScaleByte(item._iMaxDur, scale.durability);
		item._iDurability = std::min(item._iDurability, item._iMaxDur);
	}

	item._ivalue = ScaleByPercent(item._ivalue, scale.value);
	item._iIvalue = ScaleByPercent(item._iIvalue, scale.value);

	// The name carries the tier so it can be read in the inventory grid, not only in the popup.
	// Prepended to BOTH names: _iName is what an unidentified item shows and _iIName the identified
	// one, and a tier is a property of the object rather than something identifying reveals.
	const string_view prefix = _(TierNamePrefix(tier));
	if (prefix.empty())
		return;
	// std::string, not a char buffer. The first version copied into a char[64] and then handed that
	// array to string_view, whose ARRAY overload takes the whole 64 bytes - trailing NULs included -
	// so the rebuilt name was 64 bytes long before it was truncated back to 64, losing the tail. The
	// pack tests caught it as "Brutal Sword of gore" for what should have been a Long Sword.
	const std::string baseName = item._iName;
	const std::string baseIName = item._iIName;
	*BufCopy(item._iName, prefix, " ", baseName) = '\0';
	*BufCopy(item._iIName, prefix, " ", baseIName) = '\0';
}

} // namespace devilution::oracool
