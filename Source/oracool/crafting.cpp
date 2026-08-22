#include "oracool/crafting.h"

#include "oracool/gems.h"
#include "oracool/item_sets.h"
#include "oracool/levski_roar.h"

#include <fmt/format.h>

#include <algorithm>
#include <functional>
#include <vector>

#include "engine/random.hpp"
#include "inv.h"
#include "items.h"
#include "oracool/oracool.h"
#include "player.h"
#include "utils/language.h"

namespace devilution::oracool {

namespace {

/** @brief Indices (into InvList) of every main-backpack item satisfying @p matches. */
std::vector<int> FindMaterials(const Player &player, bool (*matches)(int idx))
{
	std::vector<int> found;
	for (int i = 0; i < player._pNumInv; i++) {
		if (!player.InvList[i].isEmpty() && matches(player.InvList[i].IDidx))
			found.push_back(i);
	}
	return found;
}

/**
 * @brief The largest same-IDidx group among @p indices, or empty if none reaches @p groupSize.
 * Recipes 1 and 2 both need "N of one KIND", not "N of the family".
 */
std::vector<int> LargestSameKindGroup(const Player &player, const std::vector<int> &indices, size_t groupSize)
{
	std::vector<int> best;
	for (const int anchor : indices) {
		std::vector<int> group;
		for (const int candidate : indices) {
			if (player.InvList[candidate].IDidx == player.InvList[anchor].IDidx)
				group.push_back(candidate);
		}
		if (group.size() >= groupSize && group.size() > best.size())
			best = std::move(group);
	}
	if (best.size() > groupSize)
		best.resize(groupSize);
	return best.size() >= groupSize ? best : std::vector<int> {};
}

bool IsGem(int idx) { return IsOracoolGemIdx(idx); }
bool IsRune(int idx) { return IsOracoolRuneIdx(idx); }
// Jewels get their own predicate rather than joining IsGem, and their own recipe rather than
// widening Refine Gems. Both would have been one-line changes and both would have been wrong: the
// gem recipe takes three of a (type, quality) pair and the jewel ladder is (family, grade), so a
// shared recipe would have to know which of two decompositions applied to the index in front of it.
bool IsJewel(int idx) { return IsOracoolJewelIdx(idx); }
// Stat charms only. A Charm of Salvaging is a charm structurally - it obeys the same active cap -
// but recipe 2 turns two charms into one random STAT charm, and letting a bought 40,000 gold Primal
// charm be consumed for a Charm of Vigor is a trap, not a recipe.
/**
 * @brief The charms "Rework Charms" may consume: the six BASIC stat charms and nothing else.
 *
 * This was `IsOracoolCharmIdx && !IsOracoolSalvageCharmIdx`, which was correct while those were the
 * only two families. It stopped being correct the moment the charm space grew, and silently: the
 * predicate kept compiling and kept saying yes to everything new.
 *
 * By v1.9.23 that meant the recipe would take two charms of any kind and hand back one random basic
 * charm - so a Chapel Reliquary, a guaranteed named-encounter reward that cannot be farmed for and
 * costs a Sealed Map to earn, could be fed in and come back as a Charm of Vigor. Growing charms the
 * same. A recipe for surplus was quietly a recipe for destroying the best items in the game.
 *
 * Written as an explicit LIST rather than as more exclusions, deliberately. Every "everything except
 * the ones I have thought of" predicate here has eventually been wrong; this one can only be wrong
 * by someone adding a basic charm and not adding it here, which is a change that makes them look at
 * this line.
 */
bool IsCharm(int idx)
{
	return idx == IDI_ORACOOL_CHARM_VIGOR || idx == IDI_ORACOOL_CHARM_EMBERS
	    || idx == IDI_ORACOOL_CHARM_STORMS || idx == IDI_ORACOOL_CHARM_FORTUNE
	    || idx == IDI_ORACOOL_CHARM_LUCK || idx == IDI_ORACOOL_CHARM_GREED;
}

/** @brief The materials a recipe would consume right now, empty when it cannot run. */
std::vector<int> MaterialsFor(const Player &player, int index)
{
	switch (index) {
	case 0: { // three identical gems - same type AND quality, perfect excluded
		std::vector<int> gems = FindMaterials(player, IsGem);
		gems.erase(std::remove_if(gems.begin(), gems.end(),
		                [&](int i) { return IsPerfectGem(static_cast<uint16_t>(player.InvList[i].IDidx)); }),
		    gems.end());
		return LargestSameKindGroup(player, gems, 3);
	}
	case 1: { // two identical runes, Zod excluded (it is the top of the ladder)
		std::vector<int> runes = FindMaterials(player, IsRune);
		runes.erase(std::remove_if(runes.begin(), runes.end(),
		                [&](int i) { return IsTopRune(static_cast<uint16_t>(player.InvList[i].IDidx)); }),
		    runes.end());
		return LargestSameKindGroup(player, runes, 2);
	}
	case 2: { // two charms of any kind
		std::vector<int> charms = FindMaterials(player, IsCharm);
		if (charms.size() < 2)
			return {};
		charms.resize(2);
		return charms;
	}
	case 4: { // three identical jewels - same family AND grade, Radiant excluded
		std::vector<int> jewels = FindMaterials(player, IsJewel);
		jewels.erase(std::remove_if(jewels.begin(), jewels.end(),
		                 [&](int i) { return IsTopJewel(static_cast<uint16_t>(player.InvList[i].IDidx)); }),
		    jewels.end());
		return LargestSameKindGroup(player, jewels, 3);
	}
	default:
		return {};
	}
}

/** @brief What recipe @p index produces, given the materials it will consume. */
_item_indexes OutputFor(const Player &player, int index, const std::vector<int> &materials)
{
	switch (index) {
	case 0: // the same gem, one quality better - Diablo II's own gem recipe
		return static_cast<_item_indexes>(NextGemQuality(static_cast<uint16_t>(player.InvList[materials[0]].IDidx)));
	case 1: // the next rune up from the consumed pair, by the LADDER - not by index
		return static_cast<_item_indexes>(NextRune(static_cast<uint16_t>(player.InvList[materials[0]].IDidx)));
	case 4: // the same jewel, one grade better
		return static_cast<_item_indexes>(NextJewelGrade(static_cast<uint16_t>(player.InvList[materials[0]].IDidx)));
	case 2: { // a random charm - the enum's two islands make this a pick-from-list
		constexpr _item_indexes CharmPool[] = {
			IDI_ORACOOL_CHARM_VIGOR, IDI_ORACOOL_CHARM_EMBERS, IDI_ORACOOL_CHARM_STORMS,
			IDI_ORACOOL_CHARM_FORTUNE, IDI_ORACOOL_CHARM_LUCK, IDI_ORACOOL_CHARM_GREED
		};
		return CharmPool[GenerateRnd(6)];
	}
	default:
		return IDI_NONE;
	}
}

} // namespace

const char *CraftingRecipeName(int index)
{
	switch (index) {
	case 0:
		return N_("Refine Gems");
	case 1:
		return N_("Ascend Runes");
	case 2:
		return N_("Rework Charms");
	case 3:
		return N_("Free the Sockets");
	case 4:
		return N_("Temper Jewels");
	case 5:
		return N_("Reforge Gear");
	case 6:
		return N_("Ennoble Rares");
	case 7:
		return N_("Recast Set Pieces");
	case 8:
		return N_("Recolour Gems");
	case 9:
		return N_("Enrich Magic");
	case 10:
		return N_("Consecrate Rares");
	case 11:
		return N_("Awaken Uniques");
	case 12:
		return N_("Reroll Rares");
	case 13:
		return N_("Reroll Uniques");
	case 14:
		return N_("Reroll Primals");
	case 15:
		return N_("Make Ethereal");
	case 16:
		return N_("Mend the Ethereal");
	default:
		return "";
	}
}

bool CraftingRecipeUsesGrid(int index)
{
	// 0-2 are the N-small-things-into-one-small-thing recipes the backpack path handles. Everything
	// from 3 up transforms an item in place.
	return index >= 3;
}

const char *CraftingRecipeInputs(int index)
{
	switch (index) {
	case 0:
		return N_("3 identical gems -> one of the next quality");
	case 1:
		return N_("2 identical runes -> the next rune up");
	case 2:
		return N_("2 charms of any kind -> a random charm");
	case 3:
		return N_("1 socketed item -> the item, emptied, and its stones back");
	case 4:
		return N_("3 identical jewels -> one of the next grade");
	case 5:
		return N_("1 magic or better item + 3 Unique Encrustments -> the same item, rerolled");
	case 6:
		return N_("1 rare item + 5 Rare Fibres -> a unique of the same kind");
	case 7:
		return N_("1 set piece + 3 Set Engravings -> a different piece of that set");
	case 8:
		return N_("1 gem + 2 Magic Powder -> another type, same quality");
	case 9:
		return N_("1 magic item + 4 Magic Powder -> the same item, rolled as a rare");
	case 10:
		return N_("1 rare item + 6 Set Engravings -> a set piece for that slot");
	case 11:
		return N_("1 unique item + 8 Unique Encrustments -> the same item, rolled as a primal");
	case 12:
		return N_("1 rare item + 3 Rare Fibres -> the same item, rare rolls taken again");
	case 13:
		return N_("1 unique item + 4 Unique Encrustments -> a different unique of the same kind");
	case 14:
		return N_("1 primal item + 6 Primal Vines -> the same item, primal rolls taken again");
	case 15:
		return N_("1 weapon or armour + 5 Ethereal Imbueities -> ethereal: +35%, half durability");
	case 16:
		return N_("1 damaged ethereal item + 12 Ethereal Imbueities -> fully repaired, still ethereal");
	default:
		return "";
	}
}

bool CanCraft(const Player &player, int index)
{
	return !MaterialsFor(player, index).empty();
}

std::string Craft(Player &player, int index)
{
	if (!IsSinglePlayer())
		return {};
	const std::vector<int> materials = MaterialsFor(player, index);
	if (materials.empty())
		return {};

	// The output is built and placed BEFORE anything is consumed: a full backpack refuses the
	// craft outright rather than eating materials it cannot pay for. (Consuming first would also
	// free a slot, but a craft that needs its own inputs' space to fit its output is a craft the
	// player can retry after making room - never one that half-executes.)
	Item crafted {};
	InitializeItem(crafted, OutputFor(player, index, materials));
	GenerateNewSeed(crafted);
	crafted._iIdentified = true;
	if (!AutoPlaceItemInInventory(player, crafted, /*persistItem=*/true))
		return {};

	// Consume from the highest InvList index down: RemoveInvItem compacts the list by moving the
	// last item into the vacated slot, so ascending-order removal would invalidate the later
	// indices in `materials` - descending order cannot.
	std::vector<int> toRemove = materials;
	std::sort(toRemove.begin(), toRemove.end(), std::greater<int>());
	for (const int invIndex : toRemove)
		player.RemoveInvItem(invIndex);

	// loadgfx false: crafting moves backpack contents (which the charm provider reads), never the
	// worn weapon whose sprites loadgfx would reload - and true would crash a headless caller.
	CalcPlrInv(player, false);
	return std::string(crafted.getName());
}


// ---------------------------------------------------------------------------------------------
// Levski's Roar - the same recipes, run against the monument's 3x3 grid instead of the backpack.
// ---------------------------------------------------------------------------------------------
//
// A separate set of walks rather than a shared one parameterised over "where the materials are":
// the backpack versions have to respect InvList's compaction rules and its grid, and the nine
// slots have neither. Trying to serve both from one function is how the compaction rule ends up
// being applied to an array that does not compact.

namespace {

constexpr int GridSlots = LevskiGridSlots;

/** @brief Indices of the grid slots holding an item that satisfies @p matches. */
std::vector<int> FindGridMaterials(const Item *grid, bool (*matches)(int idx))
{
	std::vector<int> found;
	for (int i = 0; i < GridSlots; i++) {
		if (!grid[i].isEmpty() && matches(grid[i].IDidx))
			found.push_back(i);
	}
	return found;
}

/** @brief The largest same-IDidx group among @p indices, or empty if none reaches @p groupSize. */
std::vector<int> LargestSameKindGridGroup(const Item *grid, const std::vector<int> &indices, size_t groupSize)
{
	std::vector<int> best;
	for (const int anchor : indices) {
		std::vector<int> group;
		for (const int candidate : indices) {
			if (grid[candidate].IDidx == grid[anchor].IDidx)
				group.push_back(candidate);
		}
		if (group.size() >= groupSize && group.size() > best.size())
			best = std::move(group);
	}
	if (best.size() > groupSize)
		best.resize(groupSize);
	return best.size() >= groupSize ? best : std::vector<int> {};
}

/**
 * @brief What each transform recipe charges. ONE table, read by the match and by the consume.
 *
 * The two halves would otherwise be a cost written twice - a recipe that matched on three
 * engravings and then consumed two would work perfectly and quietly hand out free crafts.
 *
 * The materials are the salvage tiers, and which tier pays for what is deliberate: you salvage
 * uniques to reforge, rares to ennoble, set pieces to recast. Each recipe is funded by the kind of
 * item it operates on, so the loop closes on itself.
 */
struct ReagentSpec {
	_item_indexes material;
	int count;
};

ReagentSpec ReagentFor(int recipe)
{
	switch (recipe) {
	case 5:
		return { IDI_ORACOOL_SALVAGE_UNIQUE_ENCRUSTMENTS, 3 };
	case 6:
		return { IDI_ORACOOL_SALVAGE_RARE_FIBRES, 5 };
	case 7:
		return { IDI_ORACOOL_SALVAGE_SET_ENGRAVINGS, 3 };
	case 8:
		return { IDI_ORACOOL_SALVAGE_MAGIC_POWDER, 2 };
	// The tier ladder (v1.9.18). Climbing costs more than rerolling at the same rung, which is the
	// whole shape of it: you can chase a better roll cheaply, or pay to change what the item IS.
	case 9:
		return { IDI_ORACOOL_SALVAGE_MAGIC_POWDER, 4 };
	case 10:
		return { IDI_ORACOOL_SALVAGE_SET_ENGRAVINGS, 6 };
	case 11:
		return { IDI_ORACOOL_SALVAGE_UNIQUE_ENCRUSTMENTS, 8 };
	case 12:
		return { IDI_ORACOOL_SALVAGE_RARE_FIBRES, 3 };
	case 13:
		return { IDI_ORACOOL_SALVAGE_UNIQUE_ENCRUSTMENTS, 4 };
	case 14:
		return { IDI_ORACOOL_SALVAGE_PRIMAL_VINES, 6 };
	case 15:
		return { IDI_ORACOOL_SALVAGE_ETHEREAL_IMBUEITIES, 5 };
	case 16:
		// The expensive one, and deliberately the most expensive recipe in the game. Ethereal is a
		// bargain - more of everything for half the lifespan and no smith will touch it - so a
		// repair removes the only price the item was paying. It should cost more than making one.
		return { IDI_ORACOOL_SALVAGE_ETHEREAL_IMBUEITIES, 12 };
	default:
		return { IDI_NONE, 0 };
	}
}

/**
 * @brief @p count slots holding @p materialIdx, or empty if the grid does not hold that many.
 *
 * Reagents STACK, so "three Unique Encrustments" may be one slot holding three or three slots
 * holding one. Both are spelled here, and the stack case returns the one slot - TransmuteLevskiGrid
 * decrements it rather than clearing it, which is the whole reason this returns slots and lets the
 * caller decide what consuming means.
 */
std::vector<int> FindGridReagents(const Item *grid, _item_indexes materialIdx, int count)
{
	std::vector<int> found;
	int have = 0;
	for (int i = 0; i < GridSlots && have < count; i++) {
		if (grid[i].isEmpty() || grid[i].IDidx != materialIdx)
			continue;
		found.push_back(i);
		have += grid[i].stackCount();
	}
	return have >= count ? found : std::vector<int> {};
}

/**
 * @brief Spends @p count of the reagent held across @p slots, emptying slots as they run dry.
 *
 * Stack-aware, because a reagent stack of five sitting in one slot has to be able to pay a cost of
 * three and leave two behind. Clearing the slot outright would silently confiscate the remainder.
 */
void ConsumeGridReagents(Item *grid, const std::vector<int> &slots, int count)
{
	int owed = count;
	for (const int slot : slots) {
		if (owed <= 0)
			break;
		const int taken = std::min(owed, grid[slot].stackCount());
		if (taken >= grid[slot].stackCount())
			grid[slot].clear();
		else
			grid[slot].setStackCount(grid[slot].stackCount() - taken);
		owed -= taken;
	}
}

/** @brief A magic-or-better item the reforge could reroll, or -1. */
int FindGridReforgeTarget(const Item *grid)
{
	for (int i = 0; i < GridSlots; i++) {
		const Item &item = grid[i];
		// Gear only, and only gear that HAS rolls to reroll. A white item has nothing to change,
		// and a socketable is not gear at all.
		if (item.isEmpty() || item._iMagical == ITEM_QUALITY_NORMAL)
			continue;
		if (IsOracoolGemIdx(item.IDidx) || IsOracoolRuneIdx(item.IDidx) || IsOracoolJewelIdx(item.IDidx)
		    || IsOracoolSalvageIdx(item.IDidx) || IsOracoolCharmIdx(item.IDidx))
			continue;
		// A completed runeword is NOT rerolled. Its stats come from the word, not from a seed, so
		// rerolling would silently strip it - and the runes are already inside it and would go too.
		if (item.socketedCount() > 0)
			continue;
		return i;
	}
	return -1;
}

/** @brief A rare item that some unique of its own kind could become, or -1. */
int FindGridEnnobleTarget(const Item *grid)
{
	for (int i = 0; i < GridSlots; i++) {
		const Item &item = grid[i];
		if (item.isEmpty() || item._iOracoolTier != OracoolItemTier::Rare)
			continue;
		if (item.socketedCount() > 0)
			continue;
		if (HasUniqueForBaseOf(item))
			return i;
	}
	return -1;
}

/** @brief A set piece whose set holds at least one OTHER piece, or -1. */
int FindGridSetPieceTarget(const Item *grid)
{
	for (int i = 0; i < GridSlots; i++) {
		if (grid[i].isEmpty() || !IsSetItem(grid[i]))
			continue;
		const SetItemDefinition *piece = FindSetItemByCursor(grid[i]._iCurs);
		if (piece == nullptr)
			continue;
		const ItemSetDefinition *set = FindItemSetOwning(piece->id);
		if (set != nullptr && set->itemCount > 1)
			return i;
	}
	return -1;
}

/**
 * @brief Every set piece whose slot suits equip location @p loc.
 *
 * Consecrate turns a rare into a set piece for the SAME SLOT, so a rare helm becomes a set helm.
 * The mapping goes through BaseItemForSetSlot - the set table's own slot word resolved to a base
 * item, whose ILOC is then compared - rather than a second slot-word table here, which is how the
 * two would come to disagree about what "off_hand" means.
 */
std::vector<const SetItemDefinition *> SetPiecesForLoc(item_equip_type loc)
{
	std::vector<const SetItemDefinition *> found;
	if (loc == ILOC_NONE || loc == ILOC_UNEQUIPABLE)
		return found;
	for (const ItemSetDefinition &set : ItemSets) {
		for (int i = 0; i < set.itemCount; i++) {
			const SetItemDefinition &piece = ItemSetItems[set.firstItem + i];
			const int base = BaseItemForSetSlot(piece.slot);
			if (base < 0)
				continue;
			if (AllItemsList[base].iLoc == loc)
				found.push_back(&piece);
		}
	}
	return found;
}

/** @brief Whether @p item is gear a tier recipe can act on at all. */
bool IsTierRecipeGear(const Item &item)
{
	if (item.isEmpty())
		return false;
	if (item._iClass != ICLASS_WEAPON && item._iClass != ICLASS_ARMOR)
		return false;
	// A socketed item is excluded from every reroll and every tier bump, for the reason Reforge
	// already records: its stats would come back without the stones inside it, and a completed
	// runeword's name comes from the word rather than from a seed. Empty it first.
	return item.socketedCount() == 0;
}

/** @brief The first grid item matching @p wanted, by the tier a recipe operates on. */
int FindGridItemOfTier(const Item *grid, OracoolItemTier wanted)
{
	for (int i = 0; i < GridSlots; i++) {
		if (IsTierRecipeGear(grid[i]) && grid[i]._iOracoolTier == wanted)
			return i;
	}
	return -1;
}

/** @brief A plain-or-magic item - no Oracool tier - the low recipes act on, or -1. */
int FindGridUntieredItem(const Item *grid)
{
	for (int i = 0; i < GridSlots; i++) {
		if (IsTierRecipeGear(grid[i]) && grid[i]._iOracoolTier == OracoolItemTier::None
		    && grid[i]._iMagical != ITEM_QUALITY_UNIQUE)
			return i;
	}
	return -1;
}

/**
 * @brief A unique item, or -1.
 *
 * BOTH senses of the word: a vanilla unique (`_iMagical == ITEM_QUALITY_UNIQUE`, a named object
 * with fixed powers) and this fork's BuffedUnique TIER (an ordinary item rolled heavily). They are
 * different things that a player calls the same thing, and a recipe that accepted only one of them
 * would refuse half the items its own name describes.
 */
int FindGridUniqueItem(const Item *grid)
{
	for (int i = 0; i < GridSlots; i++) {
		if (!IsTierRecipeGear(grid[i]))
			continue;
		if (grid[i]._iMagical == ITEM_QUALITY_UNIQUE || grid[i]._iOracoolTier == OracoolItemTier::BuffedUnique)
			return i;
	}
	return -1;
}

/** @brief Durable gear that is not ethereal yet, or -1. */
int FindGridEtherealTarget(const Item *grid)
{
	for (int i = 0; i < GridSlots; i++) {
		const Item &item = grid[i];
		if (item.isEmpty() || item._iOracoolEthereal)
			continue;
		if (item._iClass != ICLASS_WEAPON && item._iClass != ICLASS_ARMOR)
			continue;
		// The same eligibility MakeItemEthereal enforces. Checked here too so the recipe does not
		// offer itself on an item it would then decline - a Transmute that consumes nothing and
		// says nothing is the worst of both.
		if (item._iMaxDur == 0 || item._iMaxDur == DUR_INDESTRUCTIBLE)
			continue;
		return i;
	}
	return -1;
}

/** @brief An ethereal item with durability missing, or -1. */
int FindGridDamagedEthereal(const Item *grid)
{
	for (int i = 0; i < GridSlots; i++) {
		const Item &item = grid[i];
		if (item.isEmpty() || !item._iOracoolEthereal)
			continue;
		if (item._iMaxDur == 0 || item._iMaxDur == DUR_INDESTRUCTIBLE)
			continue;
		// Only a DAMAGED one. A full-durability ethereal offered this recipe would take twelve
		// imbueities for nothing.
		if (item._iDurability < item._iMaxDur)
			return i;
	}
	return -1;
}

/** @brief A rare whose slot some set piece could fill, or -1. */
int FindGridConsecrateTarget(const Item *grid)
{
	for (int i = 0; i < GridSlots; i++) {
		if (!IsTierRecipeGear(grid[i]) || grid[i]._iOracoolTier != OracoolItemTier::Rare)
			continue;
		if (!SetPiecesForLoc(grid[i]._iLoc).empty())
			return i;
	}
	return -1;
}

/** @brief Any gem, or -1. */
int FindGridGem(const Item *grid)
{
	for (int i = 0; i < GridSlots; i++) {
		if (!grid[i].isEmpty() && IsOracoolGemIdx(grid[i].IDidx))
			return i;
	}
	return -1;
}

/** @brief The single socketed item with at least one stone in it, or -1. */
int FindGridSocketedItem(const Item *grid)
{
    for (int i = 0; i < GridSlots; i++) {
        if (!grid[i].isEmpty() && grid[i].socketedCount() > 0)
            return i;
    }
    return -1;
}

/** @brief The slots recipe @p index would consume from @p grid, empty when it cannot run. */
std::vector<int> GridMaterialsFor(const Item *grid, int index)
{
	switch (index) {
	case 0: { // three identical gems - same type AND quality, perfect excluded
		std::vector<int> gems = FindGridMaterials(grid, IsGem);
		gems.erase(std::remove_if(gems.begin(), gems.end(),
		                [&](int i) { return IsPerfectGem(static_cast<uint16_t>(grid[i].IDidx)); }),
		    gems.end());
		return LargestSameKindGridGroup(grid, gems, 3);
	}
	case 1: { // two identical runes, Zod excluded - it is the top of the ladder
		std::vector<int> runes = FindGridMaterials(grid, IsRune);
		runes.erase(std::remove_if(runes.begin(), runes.end(),
		                [&](int i) { return IsTopRune(static_cast<uint16_t>(grid[i].IDidx)); }),
		    runes.end());
		return LargestSameKindGridGroup(grid, runes, 2);
	}
	case 2: { // two charms of any kind
		std::vector<int> charms = FindGridMaterials(grid, IsCharm);
		if (charms.size() < 2)
			return {};
		charms.resize(2);
		return charms;
	}
	case 3: { // one socketed item with something in it
		const int socketed = FindGridSocketedItem(grid);
		return socketed < 0 ? std::vector<int> {} : std::vector<int> { socketed };
	}
	case 4: { // three identical jewels, Radiant excluded
		std::vector<int> jewels = FindGridMaterials(grid, IsJewel);
		jewels.erase(std::remove_if(jewels.begin(), jewels.end(),
		                 [&](int i) { return IsTopJewel(static_cast<uint16_t>(grid[i].IDidx)); }),
		    jewels.end());
		return LargestSameKindGridGroup(grid, jewels, 3);
	}
	// ---------------------------------------------------------------------------------------
	// The four adopted from Kanai's Cube (v1.9.17). Each takes ONE item plus a REAGENT, and the
	// reagent is what makes them tell each other apart: with four recipes all eating "one item",
	// an auto-picked transmute needs the inputs to be disjoint or the monument becomes a
	// lottery. The reagents are the salvage materials, which until now had no consumer anywhere
	// in the game - they dropped, stacked, sorted into their stash row, and were never spent.
	// ---------------------------------------------------------------------------------------
	case 5:
	case 6:
	case 7:
	case 8:
	case 9:
	case 10:
	case 11:
	case 12:
	case 13:
	case 14:
	case 15:
	case 16: {
		// One shape for all four: find the target this recipe operates on, then find its reagent
		// through ReagentFor - the same table the consume step reads, so a recipe cannot match on
		// one cost and charge another.
		int target = -1;
		switch (index) {
		case 5:
			target = FindGridReforgeTarget(grid);
			break;
		case 6:
			target = FindGridEnnobleTarget(grid);
			break;
		case 7:
			target = FindGridSetPieceTarget(grid);
			break;
		case 8:
			target = FindGridGem(grid);
			break;
		case 9:
			target = FindGridUntieredItem(grid);
			break;
		case 10:
			target = FindGridConsecrateTarget(grid);
			break;
		case 11:
		case 13:
			target = FindGridUniqueItem(grid);
			break;
		case 12:
			target = FindGridItemOfTier(grid, OracoolItemTier::Rare);
			break;
		case 14:
			target = FindGridItemOfTier(grid, OracoolItemTier::Primal);
			break;
		case 15:
			target = FindGridEtherealTarget(grid);
			break;
		case 16:
			target = FindGridDamagedEthereal(grid);
			break;
		default:
			return {};
		}
		if (target < 0)
			return {};

		const ReagentSpec spec = ReagentFor(index);
		std::vector<int> out = FindGridReagents(grid, spec.material, spec.count);
		if (out.empty())
			return {};
		// Target FIRST - TransmuteLevskiGrid reads materials[0] as the thing being transformed and
		// everything after it as reagent slots.
		out.insert(out.begin(), target);
		return out;
	}
	default:
		return {};
	}
}

/** @brief How many free slots @p grid has once @p consumed are emptied. */
int GridRoomAfter(const Item *grid, const std::vector<int> &consumed)
{
	int free = 0;
	for (int i = 0; i < GridSlots; i++) {
		if (grid[i].isEmpty() || std::find(consumed.begin(), consumed.end(), i) != consumed.end())
			free++;
	}
	return free;
}

} // namespace

int CraftingRecipeReagentCount(int index)
{
	return ReagentFor(index).count;
}

int CraftingRecipeReagentItem(int index)
{
	return ReagentFor(index).material;
}

bool CanCraftFromLevskiGrid(const Item *grid, int index)
{
	return !GridMaterialsFor(grid, index).empty();
}

int FirstReadyLevskiRecipe(const Item *grid)
{
	// MOST SLOTS WINS, ties to the lowest index.
	//
	// This was "the lowest-numbered ready recipe", which was fine while the five recipes had
	// disjoint inputs. It stopped being fine the moment four more arrived that all eat "one item
	// plus a reagent": a socketed rare with three Unique Encrustments beside it satisfies both
	// Free the Sockets (one slot) and Reforge (four), and lowest-index would silently pick the
	// former every time - so the reforge reagents would be unusable on anything socketed and the
	// player would have no way to tell why.
	//
	// Most-slots is the rule because it is the one a player can predict without reading this file:
	// the monument runs the recipe that uses the most of what you put in front of it. Putting in
	// only what a recipe needs is how you choose, which is how the Horadric Cube always worked.
	int best = -1;
	size_t bestSlots = 0;
	for (int i = 0; i < CraftingRecipeCount; i++) {
		const std::vector<int> materials = GridMaterialsFor(grid, i);
		if (materials.empty() || materials.size() <= bestSlots)
			continue;
		best = i;
		bestSlots = materials.size();
	}
	return best;
}

std::string TransmuteLevskiGrid(Item *grid)
{
	return TransmuteLevskiGridWith(grid, -1);
}

std::string TransmuteLevskiGridWith(Item *grid, int index)
{
	// A CHOSEN recipe that is not ready runs NOTHING, and returns empty like every other
	// nothing-happened path. Falling back to whatever else is ready would be the worst possible
	// answer: the player selected Reroll Rares, is short one fibre, and the monument ennobles the
	// item instead using a different pile of materials.
	//
	// Empty rather than a refusal MESSAGE, though the first draft returned one - the return value's
	// contract is "what was made", and a caller cannot tell a refusal from a success if both are
	// text. The monument asks CanCraftFromLevskiGrid itself to say why nothing happened.
	if (index >= 0 && index < CraftingRecipeCount && !CanCraftFromLevskiGrid(grid, index))
		return {};
	const int recipe = index >= 0 ? index : FirstReadyLevskiRecipe(grid);
	if (recipe < 0)
		return {};
	const std::vector<int> materials = GridMaterialsFor(grid, recipe);
	if (materials.empty())
		return {};

	// Freeing sockets is the one recipe that gives back MORE than it takes, so its room check is
	// its own: the host stays in place and each stone needs a cell of its own.
	//
	// CELLS, not array slots. GridRoomAfter counts free entries in a twelve-long array, which is
	// the right answer for recipes that shrink 1x1 inputs into a 1x1 output and the wrong one here:
	// a 2x3 breastplate holding six stones occupies one array slot and six cells, and "eleven slots
	// free" approved a transmute that needed thirteen cells in a twelve-cell grid. The repack then
	// silently dropped whatever did not fit. The full case - a six-socket 2x3 host - exactly fills
	// the grid with nothing to spare, so anything else sharing the grid is already one cell too
	// many; this is a boundary the player walks straight into, not a corner.
	//
	// So the check is a real packing of the real result, run by the grid's own placement code
	// (LevskiGridCanHold) rather than by arithmetic here that would have to be kept in step with it.
	if (recipe == 3) {
		Item &host = grid[materials[0]];
		const int stones = host.socketedCount();

		std::vector<Item> after;
		after.reserve(GridSlots + Item::MaxItemSockets);
		for (int i = 0; i < GridSlots; i++) {
			if (!grid[i].isEmpty())
				after.push_back(grid[i]);
		}
		for (const uint16_t socketed : host._iSocketed) {
			if (socketed == Item::EmptySocket)
				continue;
			Item stone;
			InitializeItem(stone, static_cast<_item_indexes>(socketed));
			after.push_back(stone);
		}
		if (!LevskiGridCanHold(after.data(), static_cast<int>(after.size())))
			return std::string(_("not enough room to free the stones"));

		std::string freed;
		int placed = 0;
		for (uint16_t &socketed : host._iSocketed) {
			if (socketed == Item::EmptySocket)
				continue;
			for (int slot = 0; slot < GridSlots && placed <= stones; slot++) {
				if (!grid[slot].isEmpty())
					continue;
				InitializeItem(grid[slot], static_cast<_item_indexes>(socketed));
				GenerateNewSeed(grid[slot]);
				grid[slot]._iIdentified = true;
				placed++;
				break;
			}
			socketed = Item::EmptySocket;
		}
		// The host keeps its sockets - they are empty again, ready to take something else. Zod's
		// stamp is undone by restoring durability from the maximum it deliberately left intact.
		if (host._iMaxDur > 0 && host._iDurability == DUR_INDESTRUCTIBLE)
			host._iDurability = host._iMaxDur;
		// A completed runeword's name came from the word; with the runes gone it is a base item
		// again, so the name has to go back too.
		host._iIName[0] = '\0';
		freed = fmt::format(fmt::runtime(_("{:d} stones freed")), placed);
		return freed;
	}

	// The four adopted from Kanai's Cube. All of them TRANSFORM the target in place and consume
	// their reagent, so the grid can only ever get emptier - no room check is needed or wanted, and
	// there is no "output item" for the generic path below to place.
	if (recipe >= 5) {
		Item &target = grid[materials[0]];
		const std::vector<int> reagents(materials.begin() + 1, materials.end());
		std::string what;

		switch (recipe) {
		case 5: // REFORGE - the same base, every roll taken again at its own item level
			if (!ReforgeOracoolItem(target))
				return {};
			what = std::string(target.getName());
			break;
		case 6: // ENNOBLE - a rare becomes a unique of its own kind
			if (!EnnobleOracoolRare(target))
				return {};
			what = std::string(target.getName());
			break;
		case 7: { // RECAST - a set piece becomes a DIFFERENT piece of the same set
			const SetItemDefinition *piece = FindSetItemByCursor(target._iCurs);
			if (piece == nullptr)
				return {};
			const ItemSetDefinition *set = FindItemSetOwning(piece->id);
			if (set == nullptr || set->itemCount < 2)
				return {};
			// Every OTHER piece of the set that this fork can actually build. Excluding the one in
			// hand is the whole recipe - "convert" that returned the same piece would be a way to
			// spend three engravings on nothing.
			std::vector<const SetItemDefinition *> others;
			for (int i = 0; i < set->itemCount; i++) {
				const SetItemDefinition &candidate = ItemSetItems[set->firstItem + i];
				if (candidate.cursor == target._iCurs)
					continue;
				if (BaseItemForSetSlot(candidate.slot) < 0)
					continue;
				others.push_back(&candidate);
			}
			if (others.empty())
				return {};
			const SetItemDefinition *chosen = others[GenerateRnd(static_cast<int32_t>(others.size()))];
			InitializeItem(target, static_cast<_item_indexes>(BaseItemForSetSlot(chosen->slot)));
			MakeSetItem(target, *chosen);
			GenerateNewSeed(target);
			target._iIdentified = true;
			what = std::string(target.getName());
			break;
		}
		case 8: { // RECOLOUR - a gem keeps its quality and changes its type
			GemType type;
			GemQuality quality;
			if (!GemTypeAndQuality(static_cast<uint16_t>(target.IDidx), type, quality))
				return {};
			if (GemTypeCount < 2)
				return {};
			// Rolled among the OTHER types, so the recipe always changes something.
			const int step = 1 + GenerateRnd(static_cast<int32_t>(GemTypeCount) - 1);
			const auto newType = static_cast<GemType>((static_cast<int>(type) + step) % static_cast<int>(GemTypeCount));
			InitializeItem(target, static_cast<_item_indexes>(GemIndexFor(newType, quality)));
			GenerateNewSeed(target);
			target._iIdentified = true;
			what = std::string(target.getName());
			break;
		}
		case 9: // ENRICH - a plain or magic item rolled again as a rare
			if (!RetierOracoolItem(target, OracoolItemTier::Rare))
				return {};
			what = std::string(target.getName());
			break;
		case 10: { // CONSECRATE - a rare becomes a set piece for the same SLOT
			const std::vector<const SetItemDefinition *> pieces = SetPiecesForLoc(target._iLoc);
			if (pieces.empty())
				return {};
			const SetItemDefinition *chosen = pieces[GenerateRnd(static_cast<int32_t>(pieces.size()))];
			InitializeItem(target, static_cast<_item_indexes>(BaseItemForSetSlot(chosen->slot)));
			MakeSetItem(target, *chosen);
			GenerateNewSeed(target);
			target._iIdentified = true;
			what = std::string(target.getName());
			break;
		}
		case 11: // AWAKEN - a unique rolled again as a primal
			if (!RetierOracoolItem(target, OracoolItemTier::Primal))
				return {};
			what = std::string(target.getName());
			break;
		case 12: // REROLL RARES - the rare rolls taken again at the same rung
			if (!RetierOracoolItem(target, OracoolItemTier::Rare))
				return {};
			what = std::string(target.getName());
			break;
		case 13: // REROLL UNIQUES
			// A VANILLA unique has fixed powers - rerolling its affixes would return the identical
			// item - so for one of those the reroll re-picks WHICH unique it is. A BuffedUnique
			// tier item genuinely is a roll, so that one is rerolled in place. Two behaviours under
			// one name because a player calls both of them "a unique".
			if (target._iMagical == ITEM_QUALITY_UNIQUE) {
				if (!EnnobleOracoolRare(target))
					return {};
			} else if (!RetierOracoolItem(target, OracoolItemTier::BuffedUnique)) {
				return {};
			}
			what = std::string(target.getName());
			break;
		case 14: // REROLL PRIMALS
			if (!RetierOracoolItem(target, OracoolItemTier::Primal))
				return {};
			what = std::string(target.getName());
			break;
		case 15: // MAKE ETHEREAL - the bargain, stamped into the item's own numbers
			if (!MakeItemEthereal(target))
				return {};
			what = std::string(target.getName());
			break;
		case 16:
			// MEND - full durability, and it STAYS ethereal. The +35% is kept; what is bought is
			// the removal of the only price ethereal charges, which is why this is the most
			// expensive recipe in the game.
			target._iDurability = target._iMaxDur;
			what = std::string(target.getName());
			break;
		default:
			return {};
		}

		ConsumeGridReagents(grid, reagents, ReagentFor(recipe).count);
		return what;
	}

	if (GridRoomAfter(grid, materials) < 1)
		return {};

	_item_indexes output = IDI_NONE;
	switch (recipe) {
	case 0:
		output = static_cast<_item_indexes>(NextGemQuality(static_cast<uint16_t>(grid[materials[0]].IDidx)));
		break;
	case 1:
		output = static_cast<_item_indexes>(NextRune(static_cast<uint16_t>(grid[materials[0]].IDidx)));
		break;
	case 2: {
		constexpr _item_indexes CharmPool[] = {
			IDI_ORACOOL_CHARM_VIGOR, IDI_ORACOOL_CHARM_EMBERS, IDI_ORACOOL_CHARM_STORMS,
			IDI_ORACOOL_CHARM_FORTUNE, IDI_ORACOOL_CHARM_LUCK, IDI_ORACOOL_CHARM_GREED
		};
		output = CharmPool[GenerateRnd(6)];
		break;
	}
	case 4:
		output = static_cast<_item_indexes>(NextJewelGrade(static_cast<uint16_t>(grid[materials[0]].IDidx)));
		break;
	default:
		return {};
	}
	if (output == IDI_NONE)
		return {};

	// The same cell check "Free the Sockets" does, for the same reason - and it is NOT redundant
	// here just because these recipes take more items than they give back. They can still grow in
	// CELLS: Charm of Greed wears ICURS_MAGIC_ROCK, which is two cells wide, so two 1x1 charms can
	// transmute into one 2-cell charm. GridRoomAfter's slot count says "room" to that every time.
	//
	// Checked BEFORE anything is consumed, so a refusal costs the player nothing. The transmute
	// button's rollback would catch it anyway, but a recipe that half-runs and is then undone is a
	// worse thing to rely on than one that declines up front.
	{
		std::vector<Item> after;
		after.reserve(GridSlots + 1);
		for (int i = 0; i < GridSlots; i++) {
			if (grid[i].isEmpty() || std::find(materials.begin(), materials.end(), i) != materials.end())
				continue;
			after.push_back(grid[i]);
		}
		Item produced;
		InitializeItem(produced, output);
		after.push_back(produced);
		if (!LevskiGridCanHold(after.data(), static_cast<int>(after.size())))
			return std::string(_("not enough room for the result"));
	}

	for (const int slot : materials)
		grid[slot].clear();
	for (int slot = 0; slot < GridSlots; slot++) {
		if (!grid[slot].isEmpty())
			continue;
		InitializeItem(grid[slot], output);
		GenerateNewSeed(grid[slot]);
		grid[slot]._iIdentified = true;
		return std::string(_(AllItemsList[output].iName));
	}
	return {};
}
} // namespace devilution::oracool
