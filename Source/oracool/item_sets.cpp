#include "oracool/item_sets.h"

#include <cstring>

#include "utils/language.h"

namespace devilution::oracool {

// The tables themselves. GENERATED - see tools/GenItemSets.ps1 and the note in item_sets.h.
#include "oracool/item_sets_data.inc"

static_assert(sizeof(ItemSets) / sizeof(ItemSets[0]) == ItemSetCount,
    "ItemSetCount disagrees with the generated table - re-run tools/GenItemSets.ps1 and update the header");
static_assert(sizeof(ItemSetItems) / sizeof(ItemSetItems[0]) == ItemSetItemCount,
    "ItemSetItemCount disagrees with the generated table");
static_assert(sizeof(ItemSetBonuses) / sizeof(ItemSetBonuses[0]) == ItemSetBonusCount,
    "ItemSetBonusCount disagrees with the generated table");

const SetItemDefinition *FindSetItem(string_view itemId)
{
	// Linear over 94 rows. Not sorted, because the generator's order IS the set order and grouping
	// each set's items together is what makes firstItem/itemCount a range rather than a scatter.
	for (const SetItemDefinition &item : ItemSetItems) {
		if (itemId == item.id)
			return &item;
	}
	return nullptr;
}

const ItemSetDefinition *FindItemSetOwning(string_view itemId)
{
	const SetItemDefinition *item = FindSetItem(itemId);
	if (item == nullptr)
		return nullptr;
	const auto index = static_cast<int>(item - ItemSetItems);
	for (const ItemSetDefinition &set : ItemSets) {
		if (index >= set.firstItem && index < set.firstItem + set.itemCount)
			return &set;
	}
	return nullptr;
}

const SetBonusDefinition *ActiveSetBonus(const ItemSetDefinition &set, int wornPieces)
{
	const SetBonusDefinition *best = nullptr;
	for (int i = 0; i < set.bonusCount; i++) {
		const SetBonusDefinition &rung = ItemSetBonuses[set.firstBonus + i];
		// The highest rung whose requirement is met. The table is in ascending order, but this does
		// not lean on that - a rung inserted out of order would still resolve correctly.
		if (rung.pieces <= wornPieces && (best == nullptr || rung.pieces > best->pieces))
			best = &rung;
	}
	return best;
}

int CountLivePowers(const ItemPower *powers, size_t count)
{
	int live = 0;
	for (size_t i = 0; i < count; i++) {
		if (powers[i].type != IPL_INVALID)
			live++;
	}
	return live;
}

} // namespace devilution::oracool
