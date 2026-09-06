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

	// Oracool: user request (2026-08-20) - runes and gems, on by default.
	//
	// Their own branch above the Misc switch below, because that switch keys on _iMiscId and both
	// families are IMISC_NONE: they are identified by IDidx, the same test the description and the
	// socket code use. Routed through IsOracoolRuneIdx/IsOracoolGemIdx rather than an id range, so
	// a rune added later is picked up without this line being touched.
	//
	// Backpack only, no belt: a rune in a belt slot would be a hotkey that does nothing.
	// A rune waits for a STEP (user, 2026-09-07: "i never see El-Zod runes drop. They appear straight
	// into my backpack. Make them drop and only auto-pickup after i move a tile"). The pickup runs at
	// the end of every step within a ten-tile radius, so a rune that landed while the player was
	// fighting in place - or mid-step - was gone at the first footfall. RespawnItem stamps the tile
	// the player was on or heading to; the rune stays on the floor until the player stands somewhere
	// else. Runes only: gold, potions and gems keep their instant pickup.
	if (IsOracoolRuneIdx(item.IDidx)) {
		if (item._iOracoolLandedNear == Point { MyPlayer->position.tile.x, MyPlayer->position.tile.y })
			return false;
		return *sgOptions.Oracool.autoRunePickup && AutoPlaceItemInInventory(*MyPlayer, item, false);
	}
	if (IsOracoolGemIdx(item.IDidx))
		return *sgOptions.Oracool.autoGemPickup && AutoPlaceItemInInventory(*MyPlayer, item, false);
	// Jewels ride the gem toggle rather than getting a fourth option of their own. They are the
	// third socket family, and a player who wants gems picked up off the floor wants jewels picked
	// up too; a separate switch would be one more thing to find before the feature appears to work.
	if (IsOracoolJewelIdx(item.IDidx))
		return *sgOptions.Oracool.autoGemPickup && AutoPlaceItemInInventory(*MyPlayer, item, false);

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
