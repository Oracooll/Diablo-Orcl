#include "oracool/salvage.h"

#include <vector>

#include "inv.h"
#include "items.h"
#include "player.h"
#include "oracool/event_log.h"
#include "utils/language.h"
#include "utils/str_cat.hpp"
#include "DiabloUI/ui_flags.hpp"

namespace devilution::oracool {

namespace {

/**
 * @brief The material each bucket yields, in SalvageTier order.
 *
 * One row per tier rather than a switch, so the table and the enum are read side by side and a
 * missing case is a missing row - visible - rather than a silent fall-through to White Scales.
 */
constexpr uint16_t TierMaterial[SalvageTierCount] = {
	IDI_ORACOOL_SALVAGE_WHITE_SCALES,
	IDI_ORACOOL_SALVAGE_MAGIC_POWDER,
	IDI_ORACOOL_SALVAGE_RARE_FIBRES,
	IDI_ORACOOL_SALVAGE_UNIQUE_ENCRUSTMENTS,
	IDI_ORACOOL_SALVAGE_PRIMAL_VINES,
	IDI_ORACOOL_SALVAGE_SET_ENGRAVINGS,
	IDI_ORACOOL_SALVAGE_ETHEREAL_IMBUEITIES,
};

constexpr const char *TierName[SalvageTierCount] = {
	N_("Whites"), N_("Magic"), N_("Rare"), N_("Uniques"), N_("Primal"), N_("Set"), N_("Ethereal"),
};

/** @brief How many materials each bucket gives. Deeper tiers give more, so salvage keeps pace. */
constexpr int TierYield[SalvageTierCount] = { 1, 1, 2, 3, 4, 3, 2 };

} // namespace

const char *SalvageTierName(SalvageTier tier)
{
	return TierName[static_cast<int>(tier)];
}

uint16_t SalvageMaterialFor(SalvageTier tier)
{
	return TierMaterial[static_cast<int>(tier)];
}

bool IsSalvageable(const Item &item)
{
	if (item.isEmpty())
		return false;
	// Everything that is not gear declines. A salvage-all that could eat a stack of runes because
	// the wrong button was pressed is not worth having, and the socketable families are exactly the
	// things a player is hoarding for the crafting system this feeds.
	if (item._itype == ItemType::Gold)
		return false;
	if (item._iClass == ICLASS_QUEST)
		return false;
	if (item._iClass == ICLASS_MISC)
		return false;
	if (IsOracoolGemIdx(item.IDidx) || IsOracoolRuneIdx(item.IDidx) || IsOracoolSalvageIdx(item.IDidx))
		return false;
	// Weapons and armour, which is what is left.
	return item._iClass == ICLASS_WEAPON || item._iClass == ICLASS_ARMOR;
}

SalvageTier SalvageTierOf(const Item &item)
{
	// Most specific first - see the header. Ethereal wins over everything, because an ethereal item
	// is an ethereal item whatever else it also is, and because the alternative is two buttons
	// claiming the same object.
	if (item._iOracoolEthereal)
		return SalvageTier::Ethereal;
	if (item.hasOracoolTier()) {
		switch (item._iOracoolTier) {
		case OracoolItemTier::Set:
			return SalvageTier::Set;
		case OracoolItemTier::Primal:
			return SalvageTier::Primal;
		case OracoolItemTier::BuffedUnique:
			return SalvageTier::Unique;
		case OracoolItemTier::Rare:
			return SalvageTier::Rare;
		default:
			break;
		}
	}
	if (item._iMagical == ITEM_QUALITY_UNIQUE)
		return SalvageTier::Unique;
	if (item._iMagical == ITEM_QUALITY_MAGIC)
		return SalvageTier::Magic;
	return SalvageTier::White;
}

int SalvageYield(const Item &item)
{
	return TierYield[static_cast<int>(SalvageTierOf(item))];
}

int SalvageAllInBackpack(Player &player, SalvageTier tier)
{
	// Two passes, and the split matters. Removing from InvList compacts it - every index after the
	// removed one shifts down - so collecting first and removing afterwards, highest index first,
	// is what keeps the walk from skipping items.
	std::vector<int> victims;
	int materials = 0;
	for (int i = 0; i < GetActiveNumInv(player); i++) {
		const Item &item = GetActiveInvListItem(player, i);
		if (!IsSalvageable(item) || SalvageTierOf(item) != tier)
			continue;
		victims.push_back(i);
		materials += SalvageYield(item);
	}
	if (victims.empty())
		return 0;

	for (auto it = victims.rbegin(); it != victims.rend(); ++it)
		RemoveActiveInvItem(player, *it);

	// The materials go back into the space the gear just vacated, so a pack that was full enough to
	// need salvaging always has room for what salvaging produced. They stack, so a hundred White
	// Scales occupy one cell.
	Item material;
	InitializeItem(material, static_cast<_item_indexes>(SalvageMaterialFor(tier)));
	//
	// Overflow is close to impossible and is NOT silently swallowed. Each salvaged item frees at
	// least one cell, and the materials of one tier merge into a single stack until they pass 99 -
	// so running out needs a single press to yield more than 99 materials while freeing almost no
	// space. If it ever does happen the loss is real, so it says so in the log rather than leaving
	// the player to notice a short count.
	int placed = 0;
	for (; placed < materials; placed++) {
		Item one = material;
		if (!AutoPlaceItemInInventory(player, one, true))
			break;
	}
	if (placed < materials) {
		LogEvent(StrCat("Salvage: no room for ", materials - placed, " ",
		             _(AllItemsList[SalvageMaterialFor(tier)].iName), " - they were lost"),
		    UiFlags::ColorRed);
	}

	return static_cast<int>(victims.size());
}

} // namespace devilution::oracool
