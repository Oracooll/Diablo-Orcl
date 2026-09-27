/**
 * @file qol/stash.h
 *
 * Interface of player stash.
 */
#pragma once

#include <cstdint>
#include <array>
#include <map>
#include <vector>

#include "engine/point.hpp"
#include "engine/rectangle.hpp"
#include "items.h"

namespace devilution {

/**
 * @brief Stash page dimensions, in cells.
 *
 * Oracool V1: the stash window grew to the shared 340x720 theme, and the page grew with it - the
 * old 10x10 filled barely half the taller window.
 *
 * 16 rows, not the 17 that stood here. 17 was sized against y=660, "below which the central HUD
 * begins" - but the health orb is not part of the central HUD. It is pinned to the screen's
 * bottom-LEFT corner, which is the corner the stash occupies, and it reaches higher than the middle
 * row does. Measured: the grid starts at y=161 on a 29px pitch, so the 17th row ran 625..654 and the
 * orb's disc starts around 628. The 16th ends at 625 and clears it.
 *
 * Deleted rather than hidden, on the user's call - a row you cannot see but the auto-place code can
 * still fill is worse than no row.
 *
 * These live here rather than in stash.cpp because StashGrid's type is built from them AND
 * loadsave.cpp sizes the save file from them; three places deriving from one pair of numbers is
 * what keeps the array, the layout and the save format from disagreeing.
 */
constexpr int StashGridColumns = 10;
constexpr int StashGridRows = 16;

class StashStruct {
public:
	using StashCell = uint16_t;
	// Indexed [x][y] - see GetItemIdAtPosition - so the OUTER array is columns.
	using StashGrid = std::array<std::array<StashCell, StashGridRows>, StashGridColumns>;
	static constexpr StashCell EmptyCell = -1;

	void RemoveStashItem(StashCell iv);
	std::map<unsigned, StashGrid> stashGrids;
	std::vector<Item> stashList;
	int gold;
	bool dirty = false;

	unsigned GetPage() const
	{
		return page;
	}

	StashGrid &GetCurrentGrid()
	{
		return stashGrids[GetPage()];
	}

	/**
	 * @brief Returns the 0-based index of the item at the specified position, or EmptyCell if no item occupies that slot
	 * @param gridPosition x,y coordinate of the current stash page
	 * @return a value which can be used to index into stashList or StashStruct::EmptyCell
	 */
	StashCell GetItemIdAtPosition(Point gridPosition)
	{
		// Because StashCell is an unsigned type we can let this underflow
		return GetCurrentGrid()[gridPosition.x][gridPosition.y] - 1;
	}

	bool IsItemAtPosition(Point gridPosition)
	{
		return GetItemIdAtPosition(gridPosition) != EmptyCell;
	}

	void SetPage(unsigned newPage);
	void NextPage(unsigned offset = 1);
	void PreviousPage(unsigned offset = 1);

	/** @brief Updates _iStatFlag for all stash items. */
	void RefreshItemStatFlags();

private:
	/** Current Page */
	unsigned page;
};

constexpr Point InvalidStashPoint { -1, -1 };

extern DVL_API_FOR_TEST bool IsStashOpen;
extern DVL_API_FOR_TEST StashStruct Stash;

extern bool IsWithdrawGoldOpen;

Point GetStashSlotCoord(Point slot);
void InitStash();

/**
 * @brief Oracool: user request - opens the Stash panel exactly as Gillian's "Access Storage"
 * dialog option does. Shared so the physical Stash Chest object in town (objects.cpp) can trigger
 * the same panel-opening sequence without duplicating it.
 */
void OpenStash();
/**
 * @brief Screen rect of the stash: 340x720, flush to the top-left corner.
 *
 * Its own rect rather than GetLeftPanel's 320x352, like every other window that outgrew that slot.
 * Anything routing or absorbing a click over the stash must use this.
 */
Rectangle GetStashPanelRect();

void FreeStashGFX();
void TransferItemToInventory(Player &player, uint16_t itemId);
/**
 * @brief Render the inventory panel to the given buffer.
 */
void DrawStash(const Surface &out);
void CheckStashItem(Point mousePosition, bool isShiftHeld = false, bool isCtrlHeld = false);
bool UseStashItem(uint16_t cii);
uint16_t CheckStashHLight(Point mousePosition);
void CheckStashButtonRelease(Point mousePosition);
void CheckStashButtonPress(Point mousePosition);

void StartGoldWithdraw();
/** Transfers as much of amount as fits and returns the amount actually withdrawn. */
int WithdrawGold(Player &player, int amount);
void WithdrawGoldKeyPress(SDL_Keycode vkey);
void DrawGoldWithdraw(const Surface &out);
void CloseGoldWithdraw();
/**
 * @brief A press while the withdraw box is open: its red X, or the gold pile that toggles it. True when the
 * press was one of those - the box is modal, so every other press is swallowed after this.
 */
bool CheckGoldWithdrawPromptPress(Point mousePosition);
bool HandleGoldWithdrawTextInputEvent(const SDL_Event &event);

/**
 * @brief Checks whether the given item can be placed on the specified player's stash.
 * If 'persistItem' is 'True', the item is also placed in the inventory.
 * @param player The player to check.
 * @param item The item to be checked.
 * @param persistItem Pass 'True' to actually place the item in the inventory. The default is 'False'.
 * @return 'True' in case the item can be placed on the player's inventory and 'False' otherwise.
 */
bool AutoPlaceItemInStash(Player &player, const Item &item, bool persistItem);

/** @brief Takes out of the stash every item whose size no longer fits the cells it was saved in, into @p displaced. */
void TakeOutgrownStashItems(std::vector<Item> &displaced);

/**
 * @brief Oracool: user request - re-sorts the entire Stash by item category (Weapons, Armor,
 * Helms, Shields, Jewelry, then everything else), descending price within each category, packing
 * each page as tightly as the existing first-fit placement algorithm (AutoPlaceItemInStash)
 * already does. Triggered from Gillian's dialog ("Sort Stash").
 * @return false when the sorted layout could not hold everything; the stash is then left exactly as it was.
 */
bool SortStash(Player &player);

} // namespace devilution
