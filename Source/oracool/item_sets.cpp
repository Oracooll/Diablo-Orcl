#include "oracool/item_sets.h"

#include <cstring>

#include "engine/random.hpp"
#include "items.h"
#include "inv.h"
#include "player.h"
#include "qol/stash.h"
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
	// AMULET and RING resolve now (2026-08-21). The old note said they needed "a droppable
	// ring/amulet row with a named IDI_ constant", because the vanilla rows are anonymous and the
	// code could not name them - but naming them means inserting into _item_indexes, and item
	// indices are positional save format.
	//
	// ItemMiscIdIdx answers the same question without touching the enum: it finds the first
	// DROPPABLE row carrying a misc id. It is also now bounded and returns IDI_NONE on a miss
	// (v1.8.94), which is exactly the -1 this function already means by "no base", so the failure
	// path needs no special handling.
	//
	// That unlocks 19 of the 21 unspawnable pieces - 11 amulets and 8 rings.
	if (slot == "amulet") return ItemMiscIdIdx(IMISC_AMULET);
	if (slot == "ring") return ItemMiscIdIdx(IMISC_RING);
	// RELIC and CLOAK are re-slotted onto locations that already exist (user decision, 2026-08-21),
	// rather than the fork growing two new equipment slots for two items.
	//
	// The obvious mapping - relic to amulet, cloak to shoulders - does NOT work, and the reason is
	// worth recording because it is invisible from the slot names. Both items belong to Leoric's
	// Fallen Court, which ALREADY has an amulet and a shoulders piece, and each of those locations
	// holds one item. Re-slotting there would leave the set still capped at eleven worn pieces: the
	// two extra pieces would be buildable, findable, and permanently unwearable together with the
	// ones they duplicate.
	//
	// BRACERS and LEGS are the only two equipment locations that set leaves free, so they are the
	// only two that actually raise its ceiling. That is the whole reason for this pairing, and it is
	// why "reliquary on the forearm" and "shroud over the legs" are a fit chosen by the equipment
	// grid rather than by the fiction. If either slot is ever wanted by a future set piece, this
	// mapping is what has to move.
	if (slot == "relic") return IDI_ORACOOL_BRACERS;
	if (slot == "cloak") return IDI_ORACOOL_LEGS;
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
	// DISTINCT pieces, not equipped items (audit, 2026-08-17). The first version counted every
	// equipped item belonging to the set, so two copies of the same set ring - one in each ring
	// slot - counted as two pieces. On a set with one missing item that reads as complete: the
	// ladder's top rung lights, and the completion stinger rings, for a set the player does not
	// have. Walking the set's own definitions and asking "is THIS piece worn" cannot double-count,
	// because a piece is worn or it is not, however many copies are equipped.
	int worn = 0;
	for (int i = 0; i < set.itemCount; i++) {
		if (IsSetPieceWorn(player, ItemSetItems[set.firstItem + i]))
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
		const int worn = WornSetPieces(player, set);
		for (int i = 0; i < set.bonusCount; i++) {
			const SetBonusDefinition *rung = &ItemSetBonuses[set.firstBonus + i];
			// CUMULATIVE: every rung the wearer has reached, not just the top one. See the header
			// for why the "highest only" reading was wrong - it made a sixth piece take away the
			// five rungs below it.
			if (rung->pieces > worn)
				continue;

			// A rung's stats go onto a SCRATCH item, and the scratch item then goes through the same
			// ItemBonusTotals::AddItem every worn item goes through.
			//
			// The alternative - a switch mapping each IPL_ onto a totals field - would be a second
			// copy of SaveItemPower's switch, and the two would drift the first time either gained a
			// case. This way a set bonus is, by construction, worth exactly what the same stats would
			// be worth on a piece of equipment.
			//
			// The scratch carries no base damage, armour or granted spell, so AddItem contributes
			// only its bonus block; _iStatFlag and _iIdentified are what let that block count at all.
			Item scratch {};
			scratch._itype = ItemType::Misc;
			scratch._iIdentified = true;
			scratch._iStatFlag = true;
			// Armour is the one stat that cannot ride the scratch item, because IPL_ACP is a
			// PERCENTAGE of the item's own armour and a bonus rung has no item of its own. Routed
			// through ApplyItemPower it would reach AddItem as `0 * pct / 100`, which the sign
			// fallback there turns into exactly 1 point - so "Deep Foundation"'s +12 armour was
			// worth +1, and every armour rung authored after it would have been worth +1 too.
			//
			// On a rung the declared number is read as flat armour points instead. That is the only
			// reading available (the engine has no flat-armour power at all) and it is the one the
			// data means.
			int flatArmor = 0;
			bool anyOnScratch = false;
			for (const ItemPower &power : rung->powers) {
				if (power.type == IPL_INVALID)
					continue;
				if (power.type == IPL_ACP) {
					flatArmor += power.param1;
					continue;
				}
				ApplyItemPower(player, scratch, power);
				anyOnScratch = true;
			}
			if (anyOnScratch)
				totals.AddItem(scratch);
			totals.bonusArmor += flatArmor;
		}
	}
}

bool IsSetPieceWorn(const Player &player, const SetItemDefinition &piece)
{
	for (const Item &equipped : player.InvBody) {
		if (equipped.isEmpty() || equipped._iCurs != piece.cursor)
			continue;
		// WORN is not the same as WORKING. Audit finding, 2026-08-26: this asked only whether a
		// piece with the right cursor was in a body slot, so a BROKEN set piece - zero durability,
		// _iOracoolBroken, contributing none of its own stats - still advanced the cumulative set
		// ladder, still fired the completion sound and still claimed the set milestone.
		//
		// A player could therefore be paid the full set bonus for a set they were not actually
		// wearing, which is the opposite of what breaking an item is supposed to cost. _iStatFlag
		// is the same test the item's own stats already answer to, so the piece and the set it
		// belongs to now agree about whether it counts.
		//
		// _iOracoolBroken and NOT _iStatFlag, deliberately, though the audit suggested the latter.
		// _iStatFlag is not a stored fact - it is COMPUTED during CalcPlrItemVals, and its first
		// assignment there (items.cpp) is literally `_iStatFlag = !_iOracoolBroken` before the
		// requirement checks refine it. Reading it from here would make set bonuses depend on
		// whether that pass had reached this item yet, which is an ordering hazard that fails in
		// the direction of silently losing bonuses - worse than the bug being fixed. The broken
		// flag is set once, at the moment of breaking, and is the same signal _iStatFlag derives
		// from.
		if (equipped._iOracoolBroken)
			continue;
		return true;
	}
	return false;
}

bool IsSetPieceHeld(const Player &player, const SetItemDefinition &piece)
{
	if (IsSetPieceWorn(player, piece))
		return true;
	// Every backpack page, not only the first. Audit finding, 2026-08-26: this walked InvList
	// directly, so a set piece stored in any of the nine extra tabs read as NOT OWNED - and this
	// predicate is what weights named-set drops toward the suit a player is actually collecting.
	// So parking a half-finished set in tab 2, which is exactly what the tabs are for, quietly
	// stopped the game helping you finish it.
	//
	// InventoryPlayerItemsRange already flattens InvList and all nine tabs into one walk; the
	// shared iterator the fix needs existed, this scan simply predated its use here.
	{
		const InventoryPlayerItemsRange carried { const_cast<Player &>(player) };
		for (const Item &item : carried) {
			if (!item.isEmpty() && item._iCurs == piece.cursor)
				return true;
		}
	}
	// The stash counts. It is where a set in progress actually lives - the backpack is for the run
	// you are on, and a player collecting a suit they cannot wear yet parks it in the chest.
	for (const Item &stored : Stash.stashList) {
		if (!stored.isEmpty() && stored._iCurs == piece.cursor)
			return true;
	}
	return false;
}

int HeldSetPieces(const Player &player, const ItemSetDefinition &set)
{
	// DISTINCT pieces, for the reason recorded on WornSetPieces: two copies of the same set ring
	// are one piece of progress, not two.
	int held = 0;
	for (int i = 0; i < set.itemCount; i++) {
		if (IsSetPieceHeld(player, ItemSetItems[set.firstItem + i]))
			held++;
	}
	return held;
}

int SetPieceDropWeight(int heldInSet, bool holdsThisPiece)
{
	constexpr int FloorWeight = 1;
	constexpr int PerHeldPiece = 3;
	if (heldInSet <= 0 || holdsThisPiece)
		return FloorWeight;
	return FloorWeight + heldInSet * PerHeldPiece;
}

const char *SetSlotDisplayName(string_view slot)
{
	if (slot == "helm") return N_("helm");
	if (slot == "torso") return N_("body");
	if (slot == "gloves") return N_("gloves");
	if (slot == "belt") return N_("belt");
	if (slot == "boots") return N_("boots");
	if (slot == "shoulders") return N_("shoulders");
	if (slot == "main_hand") return N_("main hand");
	if (slot == "off_hand") return N_("off hand");
	if (slot == "amulet") return N_("amulet");
	if (slot == "ring") return N_("ring");
	if (slot == "relic") return N_("relic");
	if (slot == "cloak") return N_("cloak");
	return N_("item");
}

int ForEachEarnedSetBonus(const Player &player, const ItemSetDefinition &set,
    void (*visit)(const SetBonusDefinition &rung, void *context), void *context)
{
	const int worn = WornSetPieces(player, set);
	int earned = 0;
	for (int i = 0; i < set.bonusCount; i++) {
		const SetBonusDefinition &rung = ItemSetBonuses[set.firstBonus + i];
		if (rung.pieces > worn)
			continue;
		earned++;
		if (visit != nullptr)
			visit(rung, context);
	}
	return earned;
}

} // namespace devilution::oracool
