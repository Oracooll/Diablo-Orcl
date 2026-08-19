#include "oracool/crafting.h"

#include "oracool/gems.h"

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
bool IsCharm(int idx) { return IsOracoolCharmIdx(idx); }

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

} // namespace devilution::oracool
