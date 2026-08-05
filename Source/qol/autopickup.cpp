/**
 * @file autopickup.cpp
 *
 * QoL feature for automatically picking up gold
 */

#include "options.h"
#include "oracool/oracool.h"
#include "player.h"
#include "qol/stash.h"
#include <algorithm>
#include <limits>

namespace devilution {
namespace {

bool HasRoomForGold()
{
	// Oracool: picked-up gold goes to the shared Stash pool, which practically always has room
	// (it caps at INT_MAX) - checking inventory space here would incorrectly stop gold auto-pickup
	// once the backpack fills up with unrelated items.
	if (oracool::IsSinglePlayer())
		return Stash.gold < std::numeric_limits<int>::max();

	for (int idx : MyPlayer->InvGrid) {
		// Secondary item cell. No need to check those as we'll go through the main item cells anyway.
		if (idx < 0)
			continue;

		// Empty cell. 1x1 space available.
		if (idx == 0)
			return true;

		// Main item cell. Potentially a gold pile so check it.
		auto item = MyPlayer->InvList[idx - 1];
		if (item._itype == ItemType::Gold && item._ivalue < MaxGold)
			return true;
	}

	return false;
}

bool DoPickup(Item item)
{
	if (item._itype == ItemType::Gold && *sgOptions.Gameplay.autoGoldPickup && HasRoomForGold())
		return true;

	if (item._itype == ItemType::Misc && item.isScroll())
		return oracool::IsSinglePlayer() && *sgOptions.Oracool.autoScrollPickup
		    && (AutoPlaceItemInInventory(*MyPlayer, item, false) || AutoPlaceItemInBelt(*MyPlayer, item, false));

	if (item._itype == ItemType::Misc
	    && (AutoPlaceItemInInventory(*MyPlayer, item, false) || AutoPlaceItemInBelt(*MyPlayer, item, false))) {
		switch (item._iMiscId) {
		case IMISC_HEAL:
			return *sgOptions.Gameplay.numHealPotionPickup;
		case IMISC_FULLHEAL:
			return *sgOptions.Gameplay.numFullHealPotionPickup;
		case IMISC_MANA:
			return *sgOptions.Gameplay.numManaPotionPickup;
		case IMISC_FULLMANA:
			return *sgOptions.Gameplay.numFullManaPotionPickup;
		case IMISC_REJUV:
			return *sgOptions.Gameplay.numRejuPotionPickup;
		case IMISC_FULLREJUV:
			return *sgOptions.Gameplay.numFullRejuPotionPickup;
		case IMISC_ELIXSTR:
		case IMISC_ELIXMAG:
		case IMISC_ELIXDEX:
		case IMISC_ELIXVIT:
			return *sgOptions.Gameplay.autoElixirPickup;
		case IMISC_OILFIRST:
		case IMISC_OILOF:
		case IMISC_OILACC:
		case IMISC_OILMAST:
		case IMISC_OILSHARP:
		case IMISC_OILDEATH:
		case IMISC_OILSKILL:
		case IMISC_OILBSMTH:
		case IMISC_OILFORT:
		case IMISC_OILPERM:
		case IMISC_OILHARD:
		case IMISC_OILIMP:
		case IMISC_OILLAST:
			return *sgOptions.Gameplay.autoOilPickup;
		default:
			return false;
		}
	}

	return false;
}

} // namespace

void AutoPickup(const Player &player)
{
	if (&player != MyPlayer)
		return;
	if (leveltype == DTYPE_TOWN && !*sgOptions.Gameplay.autoPickupInTown)
		return;

	const int pickupRange = oracool::IsSinglePlayer()
	    ? std::clamp(*sgOptions.Oracool.autoPickupRange, 1, 10)
	    : 1;
	for (int distance = 1; distance <= pickupRange; ++distance) {
		for (int deltaY = -distance; deltaY <= distance; ++deltaY) {
			for (int deltaX = -distance; deltaX <= distance; ++deltaX) {
				if (std::max(std::abs(deltaX), std::abs(deltaY)) != distance)
					continue;
				const Point tile = player.position.tile + Displacement { deltaX, deltaY };
				if (!InDungeonBounds(tile) || dItem[tile.x][tile.y] == 0)
					continue;
				const int itemIndex = dItem[tile.x][tile.y] - 1;
				auto &item = Items[itemIndex];
				if (item._iRequest || !DoPickup(item))
					continue;
				NetSendCmdGItem(true, CMD_REQUESTAGITEM, player.getId(), itemIndex);
				item._iRequest = true;
			}
		}
	}
}
} // namespace devilution
