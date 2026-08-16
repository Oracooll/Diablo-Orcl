#include "oracool/item_sets.h"

#include <cstring>

#include "engine/random.hpp"
#include "items.h"
#include "player.h"
#include "oracool/stat_sheet.h"
#include "utils/language.h"
#include "utils/str_cat.hpp"
#include "utils/utf8.hpp"

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

const SetItemDefinition *FindSetItemByCursor(int cursor)
{
	if (cursor < ICURS_ORACOOL_SET_ASHEN_HELM || cursor > ICURS_ORACOOL_SET_COURT_RELIQUARY)
		return nullptr;
	// The ids are contiguous and in table order by construction (the generator walks the items once
	// and numbers as it goes), so this is an index rather than a search. The bounds check above is
	// what makes that safe, and the static_assert below is what keeps it true.
	static_assert(ICURS_ORACOOL_SET_COURT_RELIQUARY - ICURS_ORACOOL_SET_ASHEN_HELM + 1 == static_cast<int>(ItemSetItemCount),
	    "the set cursor ids are no longer one contiguous run matching the item table");
	return &ItemSetItems[cursor - ICURS_ORACOOL_SET_ASHEN_HELM];
}

bool IsSetItem(const Item &item)
{
	return !item.isEmpty() && FindSetItemByCursor(item._iCurs) != nullptr;
}

int BaseItemForSetSlot(string_view slot)
{
	// The delivered slot words, mapped onto a base item that already exists with the right equip
	// location. The set piece overrides everything about the base that matters - name, icon,
	// footprint, requirements, stats - so the base's job is only to carry the correct ILOC_ and
	// item class into the equip checks.
	//
	// Three of the twelve have no home yet and are listed here saying so rather than silently
	// resolving to something close: "relic" and "cloak" are slots this fork has not built, and
	// "shoulders" exists but only one delivered item uses it.
	if (slot == "helm") return IDI_ORACOOL_HELM;
	if (slot == "torso") return IDI_ORACOOL_LEATHER_ARMOR;
	if (slot == "gloves") return IDI_ORACOOL_GLOVES;
	if (slot == "belt") return IDI_ORACOOL_BELT;
	if (slot == "boots") return IDI_ORACOOL_BOOTS;
	if (slot == "shoulders") return IDI_ORACOOL_SHOULDERS;
	if (slot == "off_hand") return IDI_ORACOOL_LEATHER_SHIELD;
	if (slot == "main_hand") return IDI_WARRIOR; // Short Sword: a plain one-handed weapon base
	// amulet, ring, relic and cloak have NO base yet, and -1 says so rather than resolving to
	// something close. The first two need a droppable ring/amulet row with a named IDI_ constant
	// (the vanilla ones are anonymous rows the code cannot reference); the last two are slots this
	// fork has not built.
	//
	// 21 of the 94 items sit here - amulet 11, ring 8, relic 1, cloak 1 - listed, described, and not
	// yet spawnable. Only Leoric's Fallen Court is badly hit (5 of its 13); every other set is short
	// by one or two. See the continuation plan in the vault.
	return -1;
}

void MakeSetItem(Item &item, const SetItemDefinition &def)
{
	// The base carried the equip location and item class in; everything that makes this the named
	// object rather than the base is overridden here.
	item._iCurs = static_cast<uint16_t>(def.cursor);
	// _iCreateInfo is deliberately LEFT ALONE. Zeroing it here was the cause of a naming bug -
	// Item::getName reads a zero there as "the roller never gave this a real name" and falls back to
	// the base item's. getName has a Set branch of its own now, so this is belt and braces, but there
	// was never a reason to clear the field either.
	item._iIdentified = true;
	item._iMagical = ITEM_QUALITY_UNIQUE;
	item._iOracoolTier = OracoolItemTier::Set;
	CopyUtf8(item._iIName, _(def.name), sizeof(item._iIName));
	CopyUtf8(item._iName, _(def.name), sizeof(item._iName));

	item._iMinDam = def.damageMin;
	item._iMaxDam = def.damageMax;
	item._iAC = def.armorMin == def.armorMax
	    ? def.armorMin
	    : def.armorMin + GenerateRnd(def.armorMax - def.armorMin + 1);
	item._iMinStr = static_cast<uint8_t>(def.requiredStrength);
	item._iMaxDur = static_cast<uint8_t>(def.durability);
	item._iDurability = item._iMaxDur;
	// 255 is DUR_INDESTRUCTIBLE, and the engine wants the flag as well as the number.
	if (def.durability == DUR_INDESTRUCTIBLE)
		item._iDurability = DUR_INDESTRUCTIBLE;

	// Through the engine's own applier, so a set item's stats land in exactly the fields every
	// other item's stats land in - no parallel path to keep in step with CalcPlrItemVals.
	for (const ItemPower &power : def.powers) {
		if (power.type == IPL_INVALID)
			continue;
		ApplyItemPower(*MyPlayer, item, power);
	}
}

int WornSetPieces(const Player &player, const ItemSetDefinition &set)
{
	int worn = 0;
	for (const Item &equipped : player.InvBody) {
		const SetItemDefinition *piece = IsSetItem(equipped) ? FindSetItemByCursor(equipped._iCurs) : nullptr;
		if (piece == nullptr)
			continue;
		const auto index = static_cast<int>(piece - ItemSetItems);
		if (index >= set.firstItem && index < set.firstItem + set.itemCount)
			worn++;
	}
	return worn;
}

bool AnySetBonusActive(const Player &player)
{
	for (const ItemSetDefinition &set : ItemSets) {
		if (ActiveSetBonus(set, WornSetPieces(player, set)) != nullptr)
			return true;
	}
	return false;
}

void ApplySetBonusesToTotals(const Player &player, ItemBonusTotals &totals)
{
	for (const ItemSetDefinition &set : ItemSets) {
		const SetBonusDefinition *rung = ActiveSetBonus(set, WornSetPieces(player, set));
		if (rung == nullptr)
			continue;

		// A rung's stats go onto a SCRATCH item, and the scratch item then goes through the same
		// ItemBonusTotals::AddItem every worn item goes through.
		//
		// The alternative - a switch mapping each IPL_ onto a totals field - would be a second copy
		// of SaveItemPower's switch, and the two would drift the first time either gained a case.
		// This way a set bonus is, by construction, worth exactly what the same stats would be worth
		// on a piece of equipment.
		//
		// The scratch carries no base damage, armour or granted spell, so AddItem contributes only
		// its bonus block; _iStatFlag and _iIdentified are what let that block count at all.
		Item scratch {};
		scratch._itype = ItemType::Misc;
		scratch._iIdentified = true;
		scratch._iStatFlag = true;
		bool anyLive = false;
		for (const ItemPower &power : rung->powers) {
			// A rung can be entirely inert - "Cinderbrand" is one proc and nothing else. It is still
			// EARNED and still named; it simply adds nothing here.
			if (power.type == IPL_INVALID)
				continue;
			ApplyItemPower(player, scratch, power);
			anyLive = true;
		}
		if (anyLive)
			totals.AddItem(scratch);
	}
}

} // namespace devilution::oracool
