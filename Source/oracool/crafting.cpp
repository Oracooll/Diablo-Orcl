#include "oracool/crafting.h"

#include "oracool/gems.h"
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
// Stat charms only. A Charm of Salvaging is a charm structurally - it obeys the same active cap -
// but recipe 2 turns two charms into one random STAT charm, and letting a bought 40,000 gold Primal
// charm be consumed for a Charm of Vigor is a trap, not a recipe.
bool IsCharm(int idx) { return IsOracoolCharmIdx(idx) && !IsOracoolSalvageCharmIdx(idx); }

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
	default:
		return "";
	}
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

bool CanCraftFromLevskiGrid(const Item *grid, int index)
{
	return !GridMaterialsFor(grid, index).empty();
}

int FirstReadyLevskiRecipe(const Item *grid)
{
	for (int i = 0; i < CraftingRecipeCount; i++) {
		if (CanCraftFromLevskiGrid(grid, i))
			return i;
	}
	return -1;
}

std::string TransmuteLevskiGrid(Item *grid)
{
	const int recipe = FirstReadyLevskiRecipe(grid);
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
	default:
		return {};
	}
	if (output == IDI_NONE)
		return {};

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
