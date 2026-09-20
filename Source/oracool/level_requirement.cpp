#include "oracool/level_requirement.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <string_view>

#include "itemdat.h"
#include "items.h"
#include "oracool/gems.h"
#include "oracool/imbuement.h"
#include "oracool/item_sets.h"
#include "utils/language.h"

namespace devilution::oracool {

namespace {

/**
 * @brief Diablo II's rune requirements in ladder order, El to Zod: 11, 11, then two more a step.
 *
 * The fork's 33 runes are Diablo II's 33 in the same order (RuneAtLadderPosition), so the table is
 * positional. Taken exactly (decision D4): a rune is worn at the level it was worn at in Diablo II.
 */
constexpr std::array<int, MaxRuneLadder> RuneLevelByLadder = {
	11, 11, 13, 13, 15, 15, 17, 19, 21, 23, 25, 27, 29, 31, 33, 35, 37,
	39, 41, 43, 45, 47, 49, 51, 53, 55, 57, 59, 61, 63, 65, 67, 69
};

/** @brief Diablo II's gems by quality: Chipped, Flawed, Normal, Flawless, Perfect. */
int GemLevelByQuality(GemQuality quality)
{
	switch (quality) {
	case GemQuality::Chipped: return 1;
	case GemQuality::Flawed: return 5;
	case GemQuality::Normal: return 12;
	case GemQuality::Flawless: return 15;
	case GemQuality::Perfect: return 18;
	}
	return 0;
}

/**
 * @brief The nine material tiers' floors (decision D2): the lowest monster level each tier drops
 * from, read off the base table on 2026-09-19. A base is recognised by the first word of its name,
 * the same rule the givebset debug command uses - there is no material enum.
 */
struct MaterialFloor {
	std::string_view word;
	int floor;
};
constexpr std::array<MaterialFloor, 9> MaterialFloors = { {
    { "Leather", 1 }, { "Iron", 4 }, { "Steel", 7 }, { "Crusader", 10 }, { "Bone", 1 },
    { "Royal", 16 }, { "Obsidian", 19 }, { "Infernal", 22 }, { "Diamond", 25 },
} };

int MaterialFloorFor(const Item &item)
{
	if (item.IDidx < 0 || item.IDidx > IDI_LAST)
		return 0;
	const ItemData &data = AllItemsList[item.IDidx];
	// A base the dungeon never drops - a class's starting kit (the Warrior's Short Sword carries a
	// drop level of 2), a quest item, a unique's own row - has no drop level worth the name; a fresh
	// hero must be able to hold what the game handed him. A unique's own number is asked elsewhere.
	if (data.iRnd == IDROP_NEVER)
		return 1;
	// The UNTRANSLATED name: the material words are the table's English ones, and a translated
	// "Iron Helm" would miss its floor and ask its drop level instead (audit, 2026-09-20).
	const std::string_view name = data.iName != nullptr ? data.iName : "";
	for (const MaterialFloor &m : MaterialFloors) {
		if (name.size() > m.word.size() && name.substr(0, m.word.size()) == m.word && name[m.word.size()] == ' ')
			return m.floor;
	}
	// Everything outside the nine materials - vanilla's bases, the class weapons, the Necromancer's
	// heads - asks its own drop level. Rings and amulets have 1 and ask only what their affixes ask.
	return std::max<int>(1, data.iMinMLvl);
}

/** @brief The lowest level at which an affix row with this power whose range covers @p roll appears. */
int TableAffixLevel(const PLStruct *table, item_effect_type type, int roll)
{
	int best = -1;
	int fallback = -1;
	for (int j = 0; table[j].power.type != IPL_INVALID; j++) {
		if (table[j].power.type != type)
			continue;
		const int level = table[j].PLMinLvl;
		if (fallback < 0 || level < fallback)
			fallback = level;
		if (roll >= table[j].power.param1 && roll <= table[j].power.param2 && (best < 0 || level < best))
			best = level;
	}
	return best >= 0 ? best : fallback;
}

/**
 * @brief The level an affix rolls at. The item's record keeps the power and the ROLLED value (the
 * OracoolAffix's param1; its param2 is the row's price multiplier, not a range), never the row; the
 * lowest-level row of that power whose range covers the roll is the honest reading, and the lowest
 * row of that power when none does (a magic item's vanilla pair carries no value here at all).
 */
int AffixLevelFor(item_effect_type type, int roll)
{
	if (type == IPL_INVALID)
		return 0;
	int level = -1;
	for (const PLStruct *table : { ItemPrefixes, ItemSuffixes }) {
		const int found = TableAffixLevel(table, type, roll);
		if (found >= 0 && (level < 0 || found < level))
			level = found;
	}
	const int pool = OracoolPoolAffixMinLevel(type, roll, roll);
	if (pool >= 0 && (level < 0 || pool < level))
		level = pool;
	return std::max(0, level);
}

bool AsksALevel(const Item &item)
{
	if (item.isEmpty())
		return false;
	if (item.IDidx < 0 || item.IDidx > IDI_LAST)
		return false;
	const ItemData &data = AllItemsList[item.IDidx];
	// Worn or wielded gear, and charms (which carry affixes and sit in the pack, as Diablo II's do).
	// Potions, scrolls, books, gold, gems, runes and every other consumable ask nothing.
	if (data.iLoc != ILOC_UNEQUIPABLE && data.iLoc != ILOC_BELT)
		return true;
	return IsOracoolCharmIdx(item.IDidx);
}

} // namespace

int AffixRequiredLevel(int affixLevel)
{
	if (affixLevel <= 0)
		return 0;
	return (affixLevel * 3 + 3) / 4; // ceil(3/4 x level)
}

int RuneRequiredLevel(uint16_t itemIndex)
{
	if (!IsOracoolRuneIdx(itemIndex))
		return 0;
	for (size_t p = 0; p < RuneLadderSize() && p < RuneLevelByLadder.size(); p++) {
		if (RuneAtLadderPosition(p) == itemIndex)
			return RuneLevelByLadder[p];
	}
	return 0;
}

int GemRequiredLevel(uint16_t itemIndex)
{
	GemType type;
	GemQuality quality;
	if (!IsOracoolGemIdx(itemIndex) || !GemTypeAndQuality(itemIndex, type, quality))
		return 0;
	return GemLevelByQuality(quality);
}

int BaseRequiredLevel(const Item &item)
{
	if (!AsksALevel(item))
		return 0;
	if (IsOracoolCharmIdx(item.IDidx))
		return 1; // a charm's base is nothing; its affixes are the whole of it
	const int tierStep = static_cast<int>(item._iOracoolBaseTier) * BaseTierLevelStep;
	return std::max(1, MaterialFloorFor(item) + tierStep);
}

int AffixesRequiredLevel(const Item &item)
{
	int highest = 0;
	for (uint8_t i = 0; i < item._iOracoolAffixCount && i < item._iOracoolAffixes.size(); i++) {
		const OracoolAffix &affix = item._iOracoolAffixes[i];
		highest = std::max(highest, AffixLevelFor(affix.type, affix.param1));
	}
	// A magic item of vanilla's shape carries its pair here rather than in the list.
	if (item._iOracoolAffixCount == 0) {
		if (item._iPrePower != IPL_INVALID)
			highest = std::max(highest, AffixLevelFor(item._iPrePower, 0));
		if (item._iSufPower != IPL_INVALID)
			highest = std::max(highest, AffixLevelFor(item._iSufPower, 0));
	}
	return AffixRequiredLevel(highest);
}

int LevelRequirementReduction(const Item &item)
{
	return ShardCountOfKind(item, ShardKind::Ease) * EaseLevelStep;
}

int RequiredLevel(const Item &item)
{
	if (!AsksALevel(item))
		return 0;
	// Kanai's Work of Cathan (Levski's Cube, 2026-09-20): unbound for good.
	if (item._iOracoolLevelFree)
		return 1;
	int level = BaseRequiredLevel(item);
	level = std::max(level, AffixesRequiredLevel(item));

	// A unique's own number (decision D6): the level it needs to roll is the level it needs to wear.
	if (item._iMagical == ITEM_QUALITY_UNIQUE && item._iUid >= 0 && static_cast<size_t>(item._iUid) < UniqueItemCount)
		level = std::max(level, static_cast<int>(UniqueItems[item._iUid].UIMinLvl));

	// A set piece's own number, which was a roll and shelf gate only until now.
	if (item._iOracoolTier == OracoolItemTier::Set) {
		if (const SetItemDefinition *piece = FindSetItemByCursor(item._iCurs); piece != nullptr)
			level = std::max(level, piece->requiredLevel);
	}

	// Sockets: a rune or gem raises the host to its own level; a runeword therefore asks its
	// highest rune. Anything else socketed (a jewel) asks the level it drops from.
	for (uint8_t s = 0; s < item._iSocketCount && s < Item::MaxItemSockets; s++) {
		const uint16_t idx = item._iSocketed[s];
		if (idx == Item::EmptySocket || idx > IDI_LAST)
			continue;
		int socketed = std::max(RuneRequiredLevel(idx), GemRequiredLevel(idx));
		if (socketed == 0 && IsOracoolJewelIdx(idx))
			socketed = std::max<int>(1, AllItemsList[idx].iMinMLvl);
		level = std::max(level, socketed);
	}

	level -= LevelRequirementReduction(item);
	return std::max(1, level);
}

} // namespace devilution::oracool
