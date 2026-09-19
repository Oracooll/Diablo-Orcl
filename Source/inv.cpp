/**
 * @file inv.cpp
 *
 * Implementation of player inventory.
 */
#include <cstdint>
#include <limits>
#include <utility>
#include <vector>

#include <fmt/format.h>

#include "DiabloUI/ui_flags.hpp"
#include "controls/plrctrls.h"
#include "cursor.h"
#include "engine/backbuffer_state.hpp"
#include <array>

#include "engine/clx_sprite.hpp"
#include "engine/load_cel.hpp"
#include "engine/render/clx_render.hpp"
#include "engine/render/text_render.hpp"
#include "engine/size.hpp"
#include "hwcursor.hpp"
#include "inv_iterators.hpp"
#include "items.h"
#include "levels/town.h"
#include "minitext.h"
#include "options.h"
#include "oracool/passives.h"
#include "oracool/auto_save.h"
#include "oracool/hud_art.h"
#include "oracool/hud_layout.h"
#include "oracool/spell_ranks.h"
#include "oracool/hud_menu.h"
#include "engine/palette.h" // PaletteRGB, the grey socket border as a value
#include "engine/render/primitive_render.hpp" // TintRectRgb, the item slot backings
#include "oracool/event_log.h"
#include "oracool/gems.h"
#include "oracool/imbuement.h"
#include "oracool/named_encounters.h"
#include "oracool/signets.h"
#include "oracool/inventory_layout.h"
#include "oracool/levski_roar.h" // ctrl+click routes into the transmute grid
#include "oracool/runewords.h"
#include "oracool/salvage.h"
#include "oracool/socket_overlay.h"
#include "oracool/skill_sounds.h"
#include "oracool/telemetry.h"
#include "oracool/ornate_border.h"
#include "oracool/ui_sound.h"
#include "oracool/oracool.h"
#include "panels/ui_panels.hpp"
#include "plrmsg.h"
#include "qol/stash.h"
#include "stores.h"
#include "towners.h"
#include "utils/format_int.hpp"
#include "utils/language.h"
#include "utils/sdl_geometry.h"
#include "utils/stdcompat/algorithm.hpp"
#include "utils/stdcompat/optional.hpp"
#include "utils/str_cat.hpp"
#include "utils/utf8.hpp"

namespace devilution {

bool invflag;

/**
 * @brief Oracool Tabbed Inventory: which backpack page is currently displayed in the 10x4 grid
 * area. 0 is the original backpack (Player::InvList/InvGrid, untouched by this feature); 1-9
 * index into Player::InvTabList/InvTabGrid. Transient UI state, not saved - reopening the
 * inventory always starts back on tab 0, matching invflag's own not-saved convention.
 */
int ActiveInventoryTab;
bool ActiveTabItemHovered;

/**
 * Maps from inventory slot to panel-relative position.
 *
 * Oracool V1: generated from oracool/inventory_layout.h rather than written out by hand. The
 * old table was 55 literal rectangles for a 320x352 panel with a 10x4 grid; at 10x7 that would
 * be 85, and every one of them would have to be re-typed by hand whenever a slot moved - which
 * happened three times during the layout pass alone. Equipment rects and grid cells now come
 * straight from the same constants the artwork is composed against, so the hit-testing and the
 * picture cannot drift apart.
 *
 * The belt entries at the end are NOT part of that: they are positions on the main HUD, not
 * this panel, and are left exactly as they were.
 *
 * @code{.unparsed}
 *                     01 00 00 03
 *                        00 00
 *              04 04    06 06    05 05
 *              04 04    06 06    05 05
 *                       06 06
 *
 *              07 08 09 10 ... 16      (10 wide)
 *                    ... 7 rows ...
 *              67 68 69 70 ... 76
 *
 * 77 78 79 80 81 82 83 84                (belt, on the main HUD)
 * @endcode
 */
namespace {

/**
 * @brief Which oracool paperdoll slot backs each body location.
 *
 * Now total rather than a switch with a `default:` catch-all. That default used to return Chest
 * for anything unlisted, which was fine while the six extra slots drew nothing - and would have
 * become six slots all hit-testing the chest the moment they went live. Every case is spelled out
 * so adding a body location without a rect fails to compile instead.
 */
constexpr oracool::EquipSlot EquipSlotForBodyLocation(int slotXy)
{
	switch (slotXy) {
	case SLOTXY_HEAD: return oracool::EquipSlot::Helm;
	case SLOTXY_RING_LEFT: return oracool::EquipSlot::RingLeft;
	case SLOTXY_RING_RIGHT: return oracool::EquipSlot::RingRight;
	case SLOTXY_AMULET: return oracool::EquipSlot::Amulet;
	case SLOTXY_HAND_LEFT: return oracool::EquipSlot::Weapon;
	case SLOTXY_HAND_RIGHT: return oracool::EquipSlot::Shield;
	case SLOTXY_CHEST: return oracool::EquipSlot::Chest;
	case SLOTXY_SHOULDERS: return oracool::EquipSlot::Shoulders;
	case SLOTXY_BRACERS: return oracool::EquipSlot::Bracers;
	case SLOTXY_GLOVES: return oracool::EquipSlot::Gloves;
	case SLOTXY_WAIST: return oracool::EquipSlot::Belt;
	case SLOTXY_LEGS: return oracool::EquipSlot::Legs;
	case SLOTXY_BOOTS: return oracool::EquipSlot::Boots;
	default: return oracool::EquipSlot::Chest;
	}
}

// The two enums have to stay in step: InvRect is built by walking inv_body_loc and asking
// inventory_layout.h for each slot's rect, so a mismatch mis-targets clicks rather than failing.
static_assert(static_cast<int>(SLOTXY_EQUIPPED_LAST) - static_cast<int>(SLOTXY_EQUIPPED_FIRST) + 1 == NUM_INVLOC,
    "inv_xy_slot's equipment range no longer matches inv_body_loc");
static_assert(NUM_INVLOC == oracool::EquipSlotCount,
    "inv_body_loc and oracool::EquipSlot have drifted apart");

/** @brief Belt cell rects on the main HUD. Unchanged from vanilla; see the note above. */
constexpr Rectangle BeltRect(int index)
{
	return { { 205 + index * 29, 5 }, { 29, 29 } };
}

constexpr std::array<Rectangle, NUM_XY_SLOTS> MakeInvRect()
{
	std::array<Rectangle, NUM_XY_SLOTS> rects {};
	for (int i = SLOTXY_EQUIPPED_FIRST; i <= SLOTXY_EQUIPPED_LAST; i++)
		rects[i] = oracool::GetEquipSlotRect(EquipSlotForBodyLocation(i));
	for (int i = 0; i < InventoryGridCells; i++) {
		const int col = i % InventorySizeInSlots.width;
		const int row = i / InventorySizeInSlots.width;
		rects[SLOTXY_INV_FIRST + i] = {
			{ oracool::GridOrigin.x + col * oracool::CellPx, oracool::GridOrigin.y + row * oracool::CellPx },
			{ oracool::CellPx, oracool::CellPx }
		};
	}
	for (int i = 0; i < MaxBeltItems; i++)
		rects[SLOTXY_BELT_FIRST + i] = BeltRect(i);
	return rects;
}

} // namespace

const std::array<Rectangle, NUM_XY_SLOTS> InvRect = MakeInvRect();


bool TabbedInventoryEnabled()
{
	return oracool::IsSinglePlayer();
}

/**
 * @brief Oracool Tabbed Inventory: extra tabs are inert storage only - gold stays tracked
 * through the normal InvList/_pGold path (it isn't a grid item players place by hand at all, so
 * there's nothing to move into a tab in the first place).
 *
 * Quest items used to be excluded here too, because the quest-progression code that looks for
 * them (RemoveInventoryItemById/HasInventoryItemWithId, built on InventoryPlayerItemsRange) only
 * ever scanned InvList - a quest item filed into an extra tab would have been invisible to every
 * turn-in check in the game. That scanning gap is fixed now (InventoryPlayerItemsRange flattens
 * InvList and every extra tab into one iteration), so the restriction was no longer protecting
 * anything and only produced the exact bug reported: "the tavern sign doesn't go in tabs 2-10."
 * Verified every one of the game's 8 real quest items (Magic Rock, Tavern Sign, Anvil of Fury,
 * Black Mushroom, Brain, Fungal Tome, Blood Stone, Cathedral Map) turns in via one of those two
 * now-tab-aware helpers before lifting this.
 */
bool CanItemEnterExtraTab(const Item &item)
{
	return item._itype != ItemType::Gold;
}

/**
 * @brief Oracool Tabbed Inventory: the InvGrid cell for the currently displayed backpack page
 * (tab 0 = Player::InvGrid itself, untouched; 1-9 = Player::InvTabGrid[tab-1]). Every existing
 * InvGrid consumer keeps reading/writing the real InvGrid directly and is unaffected - only the
 * new tab-aware call sites (DrawInv's grid loops, CheckInvPaste/CheckInvCut's early redirect,
 * and the new CheckExtraTab* functions) go through this indirection.
 */
int8_t &GetActiveInvGridCell(Player &player, int cellIndex)
{
	if (ActiveInventoryTab == 0)
		return player.InvGrid[cellIndex];
	return player.InvTabGrid[ActiveInventoryTab - 1][cellIndex];
}

/** @brief InvList equivalent of GetActiveInvGridCell; see that function for the shared rationale. */
Item &GetActiveInvListItem(Player &player, int listIndex)
{
	if (ActiveInventoryTab == 0)
		return player.InvList[listIndex];
	return player.InvTabList[ActiveInventoryTab - 1][listIndex];
}

/** @brief InvGrid array equivalent, for a whole-array scan (e.g. "does anything reference this list index"). */
int &GetActiveNumInv(Player &player)
{
	if (ActiveInventoryTab == 0)
		return player._pNumInv;
	return player._pNumInvTab[ActiveInventoryTab - 1];
}

/**
 * @brief Tab-aware equivalent of Player::RemoveInvItem: removes the item at list index iv from
 * the currently displayed backpack page, compacting the list exactly the same way (swap the last
 * item into the vacated slot, fix up grid references). Never network-syncs for an extra tab -
 * single-player only, no packet format exists for it.
 */
void RemoveActiveInvItem(Player &player, int iv)
{
	if (ActiveInventoryTab == 0) {
		player.RemoveInvItem(iv, false);
		return;
	}

	for (int k = 0; k < InventoryGridCells; k++) {
		int8_t &itemIndex = GetActiveInvGridCell(player, k);
		if (abs(itemIndex) - 1 == iv)
			itemIndex = 0;
	}

	GetActiveInvListItem(player, iv).clear();

	int &numInv = GetActiveNumInv(player);
	numInv--;

	if (numInv > 0 && numInv != iv) {
		GetActiveInvListItem(player, iv) = GetActiveInvListItem(player, numInv).pop();

		for (int k = 0; k < InventoryGridCells; k++) {
			int8_t &itemIndex = GetActiveInvGridCell(player, k);
			if (itemIndex == numInv + 1)
				itemIndex = iv + 1;
			if (itemIndex == -(numInv + 1))
				itemIndex = -(iv + 1);
		}
	}
}

/**
 * @brief Tab-index-parameterized equivalent of RemoveActiveInvItem, for background operations
 * (e.g. completing a store sale) that need to remove an item from a specific extra tab without
 * depending on - or disturbing - whichever tab ActiveInventoryTab currently has displayed.
 */
void RemoveExtraTabItem(Player &player, int tabIndex, int iv)
{
	auto &grid = player.InvTabGrid[tabIndex];
	auto &list = player.InvTabList[tabIndex];
	int &numInv = player._pNumInvTab[tabIndex];

	for (int8_t &itemIndex : grid) {
		if (abs(itemIndex) - 1 == iv)
			itemIndex = 0;
	}

	list[iv].clear();
	numInv--;

	if (numInv > 0 && numInv != iv) {
		list[iv] = list[numInv].pop();

		for (int8_t &itemIndex : grid) {
			if (itemIndex == numInv + 1)
				itemIndex = iv + 1;
			if (itemIndex == -(numInv + 1))
				itemIndex = -(iv + 1);
		}
	}
}

/**
 * @brief Removes the first item matching the given identity from InvList or, if Tabbed
 * Inventory is enabled, any extra tab - used to undo an AutoPlaceItemInInventory placement
 * whose actual destination isn't known to the caller (that function can silently fall back to
 * an extra tab when tab 1 has no room, so a caller that assumed tab 1 - e.g. reading back
 * InvList[_pNumInv - 1] - could otherwise grab and remove a completely unrelated item).
 * @return true if a match was found and removed.
 */
bool RemoveMatchingInventoryOrExtraTabItem(Player &player, const Item &item)
{
	for (int i = 0; i < player._pNumInv; i++) {
		if (player.InvList[i].keyAttributesMatch(item._iSeed, item.IDidx, item._iCreateInfo)) {
			player.RemoveInvItem(i, false);
			return true;
		}
	}
	if (TabbedInventoryEnabled()) {
		for (int t = 0; t < Player::NumExtraInventoryTabs; t++) {
			for (int i = 0; i < player._pNumInvTab[t]; i++) {
				if (player.InvTabList[t][i].keyAttributesMatch(item._iSeed, item.IDidx, item._iCreateInfo)) {
					RemoveExtraTabItem(player, t, i);
					return true;
				}
			}
		}
	}
	return false;
}

/** @brief Tab-aware equivalent of AddItemToInvGrid; never network-syncs for an extra tab (single-player only, no packet format for it). */
void AddItemToActiveInvGrid(Player &player, int invGridIndex, int invListIndex, Size itemSize)
{
	const int pitch = InventorySizeInSlots.width;
	for (int y = 0; y < itemSize.height; y++) {
		int rowGridIndex = invGridIndex + pitch * y;
		for (int x = 0; x < itemSize.width; x++) {
			int8_t &cell = GetActiveInvGridCell(player, rowGridIndex + x);
			cell = static_cast<int8_t>((x == 0 && y == itemSize.height - 1) ? invListIndex : -invListIndex);
		}
	}

	if (ActiveInventoryTab == 0 && &player == MyPlayer) {
		NetSendCmdChInvItem(false, invGridIndex);
	}
}

namespace {

OptionalOwnedClxSpriteList pInvCels;

/**
 * @brief Adds an item to a player's InvGrid array
 * @param player The player reference
 * @param invGridIndex Item's position in InvGrid (this should be the item's topleft grid tile)
 * @param invListIndex The item's InvList index (it's expected this already has +1 added to it since InvGrid can't store a 0 index)
 * @param itemSize Size of item
 */
void AddItemToInvGrid(Player &player, int invGridIndex, int invListIndex, Size itemSize)
{
	const int pitch = 10;
	for (int y = 0; y < itemSize.height; y++) {
		int rowGridIndex = invGridIndex + pitch * y;
		for (int x = 0; x < itemSize.width; x++) {
			if (x == 0 && y == itemSize.height - 1)
				player.InvGrid[rowGridIndex + x] = invListIndex;
			else
				player.InvGrid[rowGridIndex + x] = -invListIndex;
		}
	}

	if (&player == MyPlayer) {
		NetSendCmdChInvItem(false, invGridIndex);
	}
}

/**
 * @brief Checks whether the given item can fit in a belt slot (i.e. the item's size in inventory cells is 1x1).
 * @param item The item to be checked.
 * @return 'True' in case the item can fit a belt slot and 'False' otherwise.
 */
bool FitsInBeltSlot(const Item &item)
{
	return GetInventorySize(item) == Size { 1, 1 };
}

/**
 * @brief Checks whether the given item can be equipped. Since this overload doesn't take player information, it only considers
 * general aspects about the item, like if its requirements are met and if the item's target location is valid for the body.
 * @param item The item to check.
 * @return 'True' in case the item could be equipped in a player, and 'False' otherwise.
 */
bool CanEquip(const Item &item)
{
	return item.isEquipment()
	    && item._iStatFlag;
}

/**
 * @brief A specialized version of 'CanEquip(int, Item&, int)' that specifically checks whether the item can be equipped
 * in one/both of the player's hands.
 * @param player The player whose inventory will be checked for compatibility with the item.
 * @param item The item to check.
 * @return 'True' if the player can currently equip the item in either one of his hands (i.e. the required hands are empty and
 * allow the item), and 'False' otherwise.
 */
bool CanWield(Player &player, const Item &item)
{
	if (!CanEquip(item) || IsNoneOf(player.GetItemLocation(item), ILOC_ONEHAND, ILOC_TWOHAND))
		return false;

	Item &leftHandItem = player.InvBody[INVLOC_HAND_LEFT];
	Item &rightHandItem = player.InvBody[INVLOC_HAND_RIGHT];

	if (leftHandItem.isEmpty() && rightHandItem.isEmpty()) {
		return true;
	}

	if (!leftHandItem.isEmpty() && !rightHandItem.isEmpty()) {
		return false;
	}

	Item &occupiedHand = !leftHandItem.isEmpty() ? leftHandItem : rightHandItem;

	// Bard can dual wield swords and maces, so we allow equiping one-handed weapons in her free slot as long as her occupied
	// slot is another one-handed weapon.
	if (player._pClass == HeroClass::Bard) {
		bool occupiedHandIsOneHandedSwordOrMace = player.GetItemLocation(occupiedHand) == ILOC_ONEHAND
		    && IsAnyOf(occupiedHand._itype, ItemType::Sword, ItemType::Mace);

		bool weaponToEquipIsOneHandedSwordOrMace = player.GetItemLocation(item) == ILOC_ONEHAND
		    && IsAnyOf(item._itype, ItemType::Sword, ItemType::Mace);

		if (occupiedHandIsOneHandedSwordOrMace && weaponToEquipIsOneHandedSwordOrMace) {
			return true;
		}
	}

	return player.GetItemLocation(item) == ILOC_ONEHAND
	    && player.GetItemLocation(occupiedHand) == ILOC_ONEHAND
	    && item._iClass != occupiedHand._iClass;
}

/**
 * @brief Checks whether the specified item can be equipped in the desired body location on the player.
 * @param player The player whose inventory will be checked for compatibility with the item.
 * @param item The item to check.
 * @param bodyLocation The location in the inventory to be checked against.
 * @return 'True' if the player can currently equip the item in the specified body location (i.e. the body location is empty and
 * allows the item), and 'False' otherwise.
 */
bool CanEquip(Player &player, const Item &item, inv_body_loc bodyLocation)
{
	if (!CanEquip(item) || player._pmode > PM_WALK_SIDEWAYS || !player.InvBody[bodyLocation].isEmpty()) {
		return false;
	}

	switch (bodyLocation) {
	case INVLOC_AMULET:
		return item._iLoc == ILOC_AMULET;

	case INVLOC_CHEST:
		return item._iLoc == ILOC_ARMOR;

	case INVLOC_HAND_LEFT:
	case INVLOC_HAND_RIGHT:
		return CanWield(player, item);

	case INVLOC_HEAD:
		return item._iLoc == ILOC_HELM;

	case INVLOC_RING_LEFT:
	case INVLOC_RING_RIGHT:
		return item._iLoc == ILOC_RING;

	// Oracool: the six new worn slots. One item location each, so the mapping is a straight
	// equality like the helm and amulet cases above - no dual slots, no two-handed rule.
	case INVLOC_SHOULDERS:
		return item._iLoc == ILOC_SHOULDERS;

	case INVLOC_BRACERS:
		return item._iLoc == ILOC_BRACERS;

	case INVLOC_GLOVES:
		return item._iLoc == ILOC_GLOVES;

	case INVLOC_WAIST:
		return item._iLoc == ILOC_WAIST;

	case INVLOC_LEGS:
		return item._iLoc == ILOC_LEGS;

	case INVLOC_BOOTS:
		return item._iLoc == ILOC_BOOTS;

	default:
		return false;
	}
}

/**
 * @brief The body location an item goes in, for the slots where that is a straight 1:1 mapping.
 *
 * Oracool: the six new locations have no dual-slot or two-handed rules, so every place that needs
 * "where does this go" can share one table instead of repeating a switch. Returns NUM_INVLOC for
 * anything it does not own - rings, weapons and shields all have their own placement logic.
 */
inv_body_loc OracoolBodyLocationFor(item_equip_type loc)
{
	switch (loc) {
	case ILOC_SHOULDERS: return INVLOC_SHOULDERS;
	case ILOC_BRACERS: return INVLOC_BRACERS;
	case ILOC_GLOVES: return INVLOC_GLOVES;
	case ILOC_WAIST: return INVLOC_WAIST;
	case ILOC_LEGS: return INVLOC_LEGS;
	case ILOC_BOOTS: return INVLOC_BOOTS;
	default: return NUM_INVLOC;
	}
}

void ChangeEquipment(Player &player, inv_body_loc bodyLocation, const Item &item)
{
	player.InvBody[bodyLocation] = item;

	if (&player == MyPlayer) {
		NetSendCmdChItem(false, bodyLocation, true);
		// Oracool: user request - equipment changes persist instantly, matching Diablo 3's
		// always-saved progress rather than waiting for the next scheduled/periodic save.
		oracool::ScheduleAutoSaveForEquipmentChange();
	}
}

bool AutoEquip(Player &player, const Item &item, inv_body_loc bodyLocation, bool persistItem)
{
	if (!CanEquip(player, item, bodyLocation)) {
		return false;
	}

	if (persistItem) {
		ChangeEquipment(player, bodyLocation, item);

		if (*sgOptions.Audio.autoEquipSound && &player == MyPlayer) {
			PlaySFX(ItemInvSnds[GetItemDropAnimIndexFor(item)]);
		}

		CalcPlrInv(player, true);
	}

	return true;
}

int FindTargetSlotUnderItemCursor(Point cursorPosition, Size itemSize)
{
	Displacement panelOffset = Point { 0, 0 } - oracool::GetInventoryPanelRect().position;
	for (int r = SLOTXY_EQUIPPED_FIRST; r <= SLOTXY_EQUIPPED_LAST; r++) {
		if (InvRect[r].contains(cursorPosition + panelOffset))
			return r;
	}
	for (int r = SLOTXY_INV_FIRST; r <= SLOTXY_INV_LAST; r++) {
		if (InvRect[r].contains(cursorPosition + panelOffset)) {
			// When trying to paste into the inventory we need to determine the top left cell of the nearest area that could fit the item, not the slot under the center/hot pixel.
			if (itemSize.height <= 1 && itemSize.width <= 1) {
				// top left cell of a 1x1 item is the same cell as the hot pixel, no work to do
				return r;
			}
			// Otherwise work out how far the central cell is from the top-left cell
			Displacement hotPixelCellOffset = { (itemSize.width - 1) / 2, (itemSize.height - 1) / 2 };
			// For even dimension items we need to work out if the cursor is in the left/right (or top/bottom) half of the central cell and adjust the offset so the item lands in the area most covered by the cursor.
			if (itemSize.width % 2 == 0 && InvRect[r].contains(cursorPosition + panelOffset + Displacement { INV_SLOT_HALF_SIZE_PX, 0 })) {
				// hot pixel was in the left half of the cell, so we want to increase the offset to preference the column to the left
				hotPixelCellOffset.deltaX++;
			}
			if (itemSize.height % 2 == 0 && InvRect[r].contains(cursorPosition + panelOffset + Displacement { 0, INV_SLOT_HALF_SIZE_PX })) {
				// hot pixel was in the top half of the cell, so we want to increase the offset to preference the row above
				hotPixelCellOffset.deltaY++;
			}
			// Then work out the top left cell of the nearest area that could fit this item (as pasting on the edge of the inventory would otherwise put it out of bounds)
			int hotPixelCell = r - SLOTXY_INV_FIRST;
			int targetRow = clamp((hotPixelCell / InventorySizeInSlots.width) - hotPixelCellOffset.deltaY, 0, InventorySizeInSlots.height - itemSize.height);
			int targetColumn = clamp((hotPixelCell % InventorySizeInSlots.width) - hotPixelCellOffset.deltaX, 0, InventorySizeInSlots.width - itemSize.width);
			return SLOTXY_INV_FIRST + targetRow * InventorySizeInSlots.width + targetColumn;
		}
	}

	// Oracool: HUD art pass - belt cells live on the plate art now (hud_layout's GetBeltSlotRect),
	// not in InvRect. Slot 0 is the Menu button, 5 is Town Portal, 6/7 are hidden.
	for (int r = SLOTXY_BELT_FIRST; r <= SLOTXY_BELT_LAST; r++) {
		if (!oracool::IsRealBeltItemSlot(r - SLOTXY_BELT_FIRST))
			continue;
		if (oracool::GetBeltSlotRect(r - SLOTXY_BELT_FIRST).contains(cursorPosition))
			return r;
	}
	return NUM_XY_SLOTS;
}

/**
 * @brief Spends ONE unit of what the player is holding, leaving the rest in hand.
 *
 * Socketing a gem and pasting a Mystic Orb both used to `HoldItem.clear()`, which is right only when
 * the hand holds exactly one. Gems, runes, jewels and orbs all stack in this fork
 * (Item::isStackableConsumable), so socketing one rune out of a held stack of twenty destroyed
 * nineteen (external audit, 2026-08-25).
 *
 * The cursor is refreshed from the item rather than reset to CURSOR_HAND when units remain, so the
 * player keeps carrying the rest and can see that they do.
 */
void ConsumeOneHeldUnit(Player &player)
{
	if (player.HoldItem.stackCount() > 1) {
		player.HoldItem.setStackCount(player.HoldItem.stackCount() - 1);
		NewCursor(player.HoldItem);
		return;
	}
	player.HoldItem.clear();
	NewCursor(CURSOR_HAND);
}

void CheckInvPaste(Player &player, Point cursorPosition)
{
	Size itemSize = GetInventorySize(player.HoldItem);

	int slot = FindTargetSlotUnderItemCursor(cursorPosition, itemSize);
	if (slot == NUM_XY_SLOTS)
		return;

	if (slot >= SLOTXY_INV_FIRST && slot <= SLOTXY_INV_LAST && ActiveInventoryTab != 0
	    && !CanItemEnterExtraTab(player.HoldItem)) {
		return;
	}

	item_equip_type il = ILOC_UNEQUIPABLE;
	if (slot == SLOTXY_HEAD)
		il = ILOC_HELM;
	if (slot == SLOTXY_RING_LEFT || slot == SLOTXY_RING_RIGHT)
		il = ILOC_RING;
	if (slot == SLOTXY_AMULET)
		il = ILOC_AMULET;
	if (slot == SLOTXY_HAND_LEFT || slot == SLOTXY_HAND_RIGHT)
		il = ILOC_ONEHAND;
	if (slot == SLOTXY_CHEST)
		il = ILOC_ARMOR;
	// Oracool: the six new worn slots, each accepting exactly one item location.
	if (slot == SLOTXY_SHOULDERS)
		il = ILOC_SHOULDERS;
	if (slot == SLOTXY_BRACERS)
		il = ILOC_BRACERS;
	if (slot == SLOTXY_GLOVES)
		il = ILOC_GLOVES;
	if (slot == SLOTXY_WAIST)
		il = ILOC_WAIST;
	if (slot == SLOTXY_LEGS)
		il = ILOC_LEGS;
	if (slot == SLOTXY_BOOTS)
		il = ILOC_BOOTS;
	if (slot >= SLOTXY_BELT_FIRST && slot <= SLOTXY_BELT_LAST)
		il = ILOC_BELT;

	item_equip_type desiredIl = player.GetItemLocation(player.HoldItem);
	if (il == ILOC_ONEHAND && desiredIl == ILOC_TWOHAND)
		il = ILOC_TWOHAND;

	int8_t it = 0;
	if (il == ILOC_UNEQUIPABLE) {
		int ii = slot - SLOTXY_INV_FIRST;
		if (player.HoldItem._itype == ItemType::Gold) {
			if (GetActiveInvGridCell(player, ii) != 0) {
				int8_t iv = GetActiveInvGridCell(player, ii);
				if (iv > 0) {
					if (GetActiveInvListItem(player, iv - 1)._itype != ItemType::Gold) {
						it = iv;
					}
				} else {
					it = -iv;
				}
			}
		} else {
			// check that the item we're pasting only overlaps one other item (or is going into empty space)
			unsigned originCell = static_cast<unsigned>(slot - SLOTXY_INV_FIRST);
			for (unsigned rowOffset = 0; rowOffset < static_cast<unsigned>(itemSize.height * InventorySizeInSlots.width); rowOffset += InventorySizeInSlots.width) {
				for (unsigned columnOffset = 0; columnOffset < static_cast<unsigned>(itemSize.width); columnOffset++) {
					unsigned testCell = originCell + rowOffset + columnOffset;
					// FindTargetSlotUnderItemCursor returns the top left slot of the inventory region that fits the item, we can be confident this calculation is not going to read out of range.
					assert(testCell < InventoryGridCells);
					if (GetActiveInvGridCell(player, testCell) != 0) {
						int8_t iv = abs(GetActiveInvGridCell(player, testCell));
						if (it != 0) {
							if (it != iv) {
								// Found two different items that would be displaced by the held item, can't paste the item here.
								return;
							}
						} else {
							it = iv;
						}
					}
				}
			}
		}
	} else if (il == ILOC_BELT) {
		if (!CanBePlacedOnBelt(player.HoldItem))
			return;
	} else if (desiredIl != il) {
		return;
	}

	// Oracool Phase 1: a gem dropped onto a single socketed backpack item goes INTO the item
	// instead of swapping with it - the entire insertion UI, riding the paste path's own target
	// resolution. Backpack-hosted only, deliberately: a paste onto an EQUIPPED slot already
	// returned above on the location mismatch, so socketing worn gear means carrying it first -
	// a beat of friction that makes the decision feel like smithing rather than a hover-swap.
	if (il == ILOC_UNEQUIPABLE && it > 0 && player.HoldItem._itype != ItemType::Gold) {
		Item &socketTarget = GetActiveInvListItem(player, it - 1);
		// An Imbuement Shard dropped onto a backpack item is RECORDED on it (2026-09-19; the Mystic
		// Orbs were consumed into the item's stats here until then). Same seam as the gem paste below
		// and for the same reasons - it inherits the target resolution, the backpack-only rule (so
		// imbuing worn gear means carrying it first), and needs no window of its own.
		//
		// Tried BEFORE the socket insert, because the two can never both apply - a shard is not a
		// socketable and TrySocketGem declines it - and ordering the specific test first keeps that
		// independence obvious rather than resting on the other function's exclusion list.
		if (oracool::TryImbue(player, socketTarget, player.HoldItem)) {
			if (&player == MyPlayer) {
				if (!oracool::PlayUiEventSound(oracool::UiEventSound::ShardImbue))
					PlaySFX(IS_CAST7); // the old stand-in
				oracool::LogEvent(fmt::format("{:s} imbued into {:s} ({:s})",
				    std::string(player.HoldItem.getName()), std::string(socketTarget.getName()),
				    oracool::ImbueBreakdownLine(socketTarget)));
				// Phase 2: filling an item to its cap is a milestone. Checked through the passive
				// walk rather than tested here, so "an item at its cap" has one definition.
				oracool::CheckPassiveMilestones(player);
			}
			ConsumeOneHeldUnit(player);
			CalcPlrInv(player, true);
			return;
		}
		if (oracool::TrySocketGem(socketTarget, player.HoldItem)) {
			// The moment of transformation: the right rune sequence, completed just now, renames
			// the item and announces itself. Derived state - the check costs one table walk.
			// Phase 2: a completed runeword is a milestone, and the ONLY moment it can be noticed -
			// afterwards the item is just an item with a name, and nothing on the player says a
			// word was ever finished.
			const bool runewordComplete = oracool::TryCompleteRuneword(socketTarget);
			bool milestoneRang = false;
			if (runewordComplete && &player == MyPlayer) {
				oracool::LogEvent(fmt::format("Runeword complete: {:s}", socketTarget._iIName));
				// The FIRST runeword is also a milestone, and the milestone's sound then speaks for
				// both - never two sounds for one click (the 2026-09-03 rule).
				milestoneRang = oracool::ClaimMilestone(player, oracool::Milestone::CompleteRuneword);
			}
			// Otherwise one sound: the runeword's own when the rune completed one, the socket's when
			// it did not. IS_IGRAB stays the stand-in if the file is missing.
			if (&player == MyPlayer && !milestoneRang
			    && !oracool::PlayUiEventSound(runewordComplete ? oracool::UiEventSound::RunewordComplete : oracool::UiEventSound::Socket))
				PlaySFX(IS_IGRAB);
			ConsumeOneHeldUnit(player);
			CalcPlrInv(player, true);
			return;
		}
	}

	if (IsNoneOf(il, ILOC_UNEQUIPABLE, ILOC_BELT) && !player.CanUseItem(player.HoldItem)) {
		player.Say(HeroSpeech::ICantUseThisYet);
		return;
	}

	if (player._pmode > PM_WALK_SIDEWAYS && IsNoneOf(il, ILOC_UNEQUIPABLE, ILOC_BELT))
		return;

	if (&player == MyPlayer)
		PlaySFX(ItemInvSnds[GetItemDropAnimIndexFor(player.HoldItem)]);

	switch (il) {
	case ILOC_HELM:
	case ILOC_RING:
	case ILOC_AMULET:
	case ILOC_ARMOR:
	// Oracool: the six new worn slots take exactly this path - one slot, swap whatever is there
	// for what is held. They have no dual-slot rule like the rings and no two-handed rule like the
	// weapons, so there is nothing for them to do that the helm and armour cases do not already do.
	case ILOC_SHOULDERS:
	case ILOC_BRACERS:
	case ILOC_GLOVES:
	case ILOC_WAIST:
	case ILOC_LEGS:
	case ILOC_BOOTS: {
		auto iLocToInvLoc = [&slot](item_equip_type loc) {
			switch (loc) {
			case ILOC_HELM:
				return INVLOC_HEAD;
			case ILOC_RING:
				return (slot == SLOTXY_RING_LEFT ? INVLOC_RING_LEFT : INVLOC_RING_RIGHT);
			case ILOC_AMULET:
				return INVLOC_AMULET;
			case ILOC_ARMOR:
				return INVLOC_CHEST;
			default: {
				const inv_body_loc oracoolSlot = OracoolBodyLocationFor(loc);
				if (oracoolSlot == NUM_INVLOC)
					app_fatal("Unexpected equipment type");
				return oracoolSlot;
			}
			}
		};
		inv_body_loc slot = iLocToInvLoc(il);
		Item previouslyEquippedItem = player.InvBody[slot];
		ChangeEquipment(player, slot, player.HoldItem.pop());
		if (!previouslyEquippedItem.isEmpty()) {
			player.HoldItem = previouslyEquippedItem;
		}
		break;
	}
	case ILOC_ONEHAND: {
		inv_body_loc selectedHand = slot == SLOTXY_HAND_LEFT ? INVLOC_HAND_LEFT : INVLOC_HAND_RIGHT;
		inv_body_loc otherHand = slot == SLOTXY_HAND_LEFT ? INVLOC_HAND_RIGHT : INVLOC_HAND_LEFT;

		bool pasteIntoSelectedHand = (player.InvBody[otherHand].isEmpty() || player.InvBody[otherHand]._iClass != player.HoldItem._iClass)
		    || (player._pClass == HeroClass::Bard && player.InvBody[otherHand]._iClass == ICLASS_WEAPON && player.HoldItem._iClass == ICLASS_WEAPON);

		bool dequipTwoHandedWeapon = (!player.InvBody[otherHand].isEmpty() && player.GetItemLocation(player.InvBody[otherHand]) == ILOC_TWOHAND);

		inv_body_loc pasteHand = pasteIntoSelectedHand ? selectedHand : otherHand;
		Item previouslyEquippedItem = dequipTwoHandedWeapon ? player.InvBody[otherHand] : player.InvBody[pasteHand];
		if (dequipTwoHandedWeapon) {
			RemoveEquipment(player, otherHand, false);
		}
		ChangeEquipment(player, pasteHand, player.HoldItem.pop());
		if (!previouslyEquippedItem.isEmpty()) {
			player.HoldItem = previouslyEquippedItem;
		}
		break;
	}
	case ILOC_TWOHAND:
		if (!player.InvBody[INVLOC_HAND_LEFT].isEmpty() && !player.InvBody[INVLOC_HAND_RIGHT].isEmpty()) {
			inv_body_loc locationToUnequip = INVLOC_HAND_LEFT;
			if (player.InvBody[INVLOC_HAND_RIGHT]._itype == ItemType::Shield) {
				locationToUnequip = INVLOC_HAND_RIGHT;
			}
			bool done2h = AutoPlaceItemInInventory(player, player.InvBody[locationToUnequip], true);
			if (!done2h)
				return;

			if (locationToUnequip == INVLOC_HAND_RIGHT) {
				RemoveEquipment(player, INVLOC_HAND_RIGHT, false);
			} else {
				// CMD_CHANGEPLRITEMS will eventually be sent for the left hand
				player.InvBody[INVLOC_HAND_LEFT].clear();
			}
		}

		if (player.InvBody[INVLOC_HAND_RIGHT].isEmpty()) {
			Item previouslyEquippedItem = player.InvBody[INVLOC_HAND_LEFT];
			ChangeEquipment(player, INVLOC_HAND_LEFT, player.HoldItem.pop());
			if (!previouslyEquippedItem.isEmpty()) {
				player.HoldItem = previouslyEquippedItem;
			}
		} else {
			Item previouslyEquippedItem = player.InvBody[INVLOC_HAND_RIGHT];
			RemoveEquipment(player, INVLOC_HAND_RIGHT, false);
			ChangeEquipment(player, INVLOC_HAND_LEFT, player.HoldItem);
			player.HoldItem = previouslyEquippedItem;
		}
		break;
	case ILOC_UNEQUIPABLE:
		if (player.HoldItem._itype == ItemType::Gold && it == 0) {
			int ii = slot - SLOTXY_INV_FIRST;
			if (player.InvGrid[ii] > 0) {
				int invIndex = player.InvGrid[ii] - 1;
				int gt = player.InvList[invIndex]._ivalue;
				int ig = player.HoldItem._ivalue + gt;
				if (ig <= MaxGold) {
					player.InvList[invIndex]._ivalue = ig;
					SetPlrHandGoldCurs(player.InvList[invIndex]);
					player._pGold += player.HoldItem._ivalue;
					player.HoldItem.clear();
				} else {
					ig = MaxGold - gt;
					player._pGold += ig;
					player.HoldItem._ivalue -= ig;
					SetPlrHandGoldCurs(player.HoldItem);
					player.InvList[invIndex]._ivalue = MaxGold;
					player.InvList[invIndex]._iCurs = ICURS_GOLD_LARGE;
				}
			} else {
				int invIndex = player._pNumInv;
				player._pGold += player.HoldItem._ivalue;
				player.InvList[invIndex] = player.HoldItem.pop();
				player._pNumInv++;
				player.InvGrid[ii] = player._pNumInv;
			}
			if (&player == MyPlayer) {
				NetSendCmdChInvItem(false, ii);
			}
		} else if (it != 0 && oracool::IsSinglePlayer()
		    && player.HoldItem.canStackWith(GetActiveInvListItem(player, it - 1))) {
			Item &target = GetActiveInvListItem(player, it - 1);
			int room = Item::MaxStackCount - target.stackCount();
			int moved = std::min(room, player.HoldItem.stackCount());
			target.setStackCount(target.stackCount() + moved);
			int remainder = player.HoldItem.stackCount() - moved;
			if (remainder <= 0)
				player.HoldItem.clear();
			else
				player.HoldItem.setStackCount(remainder);
			if (&player == MyPlayer && ActiveInventoryTab == 0) {
				NetSyncInvItem(player, it - 1);
			}
		} else {
			if (it == 0) {
				int &numInv = GetActiveNumInv(player);
				// The list is exactly InventoryGridCells long, and a poisoned sidecar (a count with
				// no grid backing it) could present an apparently-free cell while the list is
				// already full - this append would then write one past the array (external audit,
				// 2026-08-17). A consistent save cannot get here; a torn one must not scribble.
				if (numInv >= InventoryGridCells)
					break;
				GetActiveInvListItem(player, numInv) = player.HoldItem.pop();
				numInv++;
				it = numInv;
			} else {
				int invIndex = it - 1;
				if (player.HoldItem._itype == ItemType::Gold)
					player._pGold += player.HoldItem._ivalue;
				std::swap(GetActiveInvListItem(player, invIndex), player.HoldItem);
				if (player.HoldItem._itype == ItemType::Gold)
					player._pGold = CalculateGold(player);
				for (int k = 0; k < InventoryGridCells; k++) {
					int8_t &itemIndex = GetActiveInvGridCell(player, k);
					if (itemIndex == it)
						itemIndex = 0;
					if (itemIndex == -it)
						itemIndex = 0;
				}
			}

			AddItemToActiveInvGrid(player, slot - SLOTXY_INV_FIRST, it, itemSize);
		}
		break;
	case ILOC_BELT: {
		int ii = slot - SLOTXY_BELT_FIRST;
		if (player.SpdList[ii].isEmpty()) {
			player.SpdList[ii] = player.HoldItem.pop();
		} else if (oracool::IsSinglePlayer()
		    && player.HoldItem.canStackWith(player.SpdList[ii])) {
			Item &target = player.SpdList[ii];
			int room = Item::MaxStackCount - target.stackCount();
			int moved = std::min(room, player.HoldItem.stackCount());
			target.setStackCount(target.stackCount() + moved);
			int remainder = player.HoldItem.stackCount() - moved;
			if (remainder <= 0)
				player.HoldItem.clear();
			else
				player.HoldItem.setStackCount(remainder);
			player.CalcScrolls();
		} else {
			std::swap(player.SpdList[ii], player.HoldItem);
			if (player.HoldItem._itype == ItemType::Gold)
				player._pGold = CalculateGold(player);
		}
		if (&player == MyPlayer) {
			NetSendCmdChBeltItem(false, ii);
		}
		RedrawComponent(PanelDrawComponent::Belt);
	} break;
	case ILOC_NONE:
	case ILOC_INVALID:
		break;
	}
	CalcPlrInv(player, true);
	if (&player == MyPlayer) {
		NewCursor(player.HoldItem);
	}
}

} // namespace

/**
 * @brief The TOP-LEFT grid cell of active-tab list item @p iv (1-based), or -1 if it is not found.
 *
 * AddItemToInvGrid writes the POSITIVE list index at the item's BOTTOM-left cell and the negative at
 * every other cell it covers, so the anchor the placement helpers take - the top-left - is that cell
 * less one row per cell of height. Getting this backwards would place a displaced item one or two
 * rows below the hole and usually fail, which is a silent wrong answer rather than a crash.
 */
bool IsActiveInvItemHovered(int listIndex)
{
	// Hover has to be asked of the RIGHT cursor variable, and which one that is depends on the tab.
	// CheckInvHLight deliberately leaves pcursinvitem == -1 for an item on an extra tab - the legacy
	// drag/drop code reads that value and assumes tab-1 indices - and records the hover in
	// pcursinvtabidx/pcursinvtabitem instead.
	//
	// The backpack draw loop used to ask only pcursinvitem, so from tab 2 onward NOTHING was ever
	// "hovered": no gold socket rings, no socketed stones, and no hover outline either (user,
	// 2026-09-12, on a 4-socket Canticle). It presented as a size problem - the later tabs fill up
	// with the big two-by-two items - but a 1x1's ring on a later tab was just as invisible.
	//
	// This decides DRAWING only. Drag/drop still goes through pcursinvitem, untouched.
	if (ActiveInventoryTab == 0)
		return pcursinvitem == listIndex + INVITEM_INV_FIRST;
	return pcursinvtabidx == ActiveInventoryTab - 1 && pcursinvtabitem == listIndex;
}

int ActiveInvAnchorSlotOf(Player &player, int iv)
{
	for (int cell = 0; cell < InventoryGridCells; cell++) {
		if (GetActiveInvGridCell(player, cell) != static_cast<int8_t>(iv))
			continue;
		const Size size = GetInventorySize(GetActiveInvListItem(player, iv - 1));
		const int anchor = cell - InventorySizeInSlots.width * (size.height - 1);
		return anchor >= 0 ? anchor : -1;
	}
	return -1;
}

// Defined further down this file, outside the anonymous namespace.
bool AutoPlaceItemInExtraTabSlot(Player &player, int tabIndex, int slotIndex, const Item &item, bool persistItem);

/**
 * @brief Places @p item with its top-left at @p slotIndex of backpack page @p tab (0 = the base grid).
 *
 * Fails, without writing anything, when the item does not fit there - which is what makes "unless it
 * is bigger, in which case look for suitable place" need no size test of its own.
 */
bool PlaceItemInTabSlot(Player &player, int tab, int slotIndex, const Item &item)
{
	if (slotIndex < 0)
		return false;
	if (tab == 0)
		return AutoPlaceItemInInventorySlot(player, slotIndex, item, true);
	return AutoPlaceItemInExtraTabSlot(player, tab - 1, slotIndex, item, true);
}

// Oracool: hoisted out of the anonymous namespace (same pattern as stores.cpp's test hooks) so
// RightMouseDown can reach it - a right-click inside the inventory window routes here with
// automaticMove=true, the exact machinery shift-click uses. The anonymous-namespace helpers it
// calls stay visible from here, since that visibility spans the whole translation unit.
void CheckInvCut(Player &player, Point cursorPosition, bool automaticMove, bool dropItem)
{
	if (player._pmode > PM_WALK_SIDEWAYS) {
		return;
	}

	if (DropGoldFlag) {
		CloseGoldDrop();
	}

	uint32_t r = 0;
	for (; r < NUM_XY_SLOTS; r++) {
		// Oracool: HUD art pass - belt cells hit-test against the plate art's cell rects
		// (hud_layout), not InvRect. Slot 0 is Menu, 5 is Town Portal, 6/7 are hidden.
		if (r >= SLOTXY_BELT_FIRST) {
			const int beltIndex = static_cast<int>(r) - SLOTXY_BELT_FIRST;
			if (!oracool::IsRealBeltItemSlot(beltIndex))
				continue;
			if (oracool::GetBeltSlotRect(beltIndex).contains(cursorPosition))
				break;
			continue;
		}

		// check which inventory rectangle the mouse is in, if any
		const Displacement panelOffset { oracool::GetInventoryPanelRect().position.x, oracool::GetInventoryPanelRect().position.y };
		if (InvRect[r].contains(cursorPosition - panelOffset)) {
			break;
		}
	}

	if (r == NUM_XY_SLOTS) {
		// not on an inventory slot rectangle
		return;
	}

	Item &holdItem = player.HoldItem;
	holdItem.clear();

	bool automaticallyMoved = false;
	bool automaticallyEquipped = false;
	bool automaticallyUnequip = false;

	Item &headItem = player.InvBody[INVLOC_HEAD];
	if (r == SLOTXY_HEAD && !headItem.isEmpty()) {
		holdItem = headItem;
		if (automaticMove) {
			automaticallyUnequip = true;
			automaticallyMoved = automaticallyEquipped = AutoPlaceItemInInventory(player, holdItem, true);
		}

		if (!automaticMove || automaticallyMoved) {
			RemoveEquipment(player, INVLOC_HEAD, false);
		}
	}

	Item &leftRingItem = player.InvBody[INVLOC_RING_LEFT];
	if (r == SLOTXY_RING_LEFT && !leftRingItem.isEmpty()) {
		holdItem = leftRingItem;
		if (automaticMove) {
			automaticallyUnequip = true;
			automaticallyMoved = automaticallyEquipped = AutoPlaceItemInInventory(player, holdItem, true);
		}

		if (!automaticMove || automaticallyMoved) {
			RemoveEquipment(player, INVLOC_RING_LEFT, false);
		}
	}

	Item &rightRingItem = player.InvBody[INVLOC_RING_RIGHT];
	if (r == SLOTXY_RING_RIGHT && !rightRingItem.isEmpty()) {
		holdItem = rightRingItem;
		if (automaticMove) {
			automaticallyUnequip = true;
			automaticallyMoved = automaticallyEquipped = AutoPlaceItemInInventory(player, holdItem, true);
		}

		if (!automaticMove || automaticallyMoved) {
			RemoveEquipment(player, INVLOC_RING_RIGHT, false);
		}
	}

	Item &amuletItem = player.InvBody[INVLOC_AMULET];
	if (r == SLOTXY_AMULET && !amuletItem.isEmpty()) {
		holdItem = amuletItem;
		if (automaticMove) {
			automaticallyUnequip = true;
			automaticallyMoved = automaticallyEquipped = AutoPlaceItemInInventory(player, holdItem, true);
		}

		if (!automaticMove || automaticallyMoved) {
			RemoveEquipment(player, INVLOC_AMULET, false);
		}
	}

	Item &leftHandItem = player.InvBody[INVLOC_HAND_LEFT];
	if (r == SLOTXY_HAND_LEFT && !leftHandItem.isEmpty()) {
		holdItem = leftHandItem;
		if (automaticMove) {
			automaticallyUnequip = true;
			automaticallyMoved = automaticallyEquipped = AutoPlaceItemInInventory(player, holdItem, true);
		}

		if (!automaticMove || automaticallyMoved) {
			RemoveEquipment(player, INVLOC_HAND_LEFT, false);
		}
	}

	Item &rightHandItem = player.InvBody[INVLOC_HAND_RIGHT];
	if (r == SLOTXY_HAND_RIGHT && !rightHandItem.isEmpty()) {
		holdItem = rightHandItem;
		if (automaticMove) {
			automaticallyUnequip = true;
			automaticallyMoved = automaticallyEquipped = AutoPlaceItemInInventory(player, holdItem, true);
		}

		if (!automaticMove || automaticallyMoved) {
			RemoveEquipment(player, INVLOC_HAND_RIGHT, false);
		}
	}

	Item &chestItem = player.InvBody[INVLOC_CHEST];
	if (r == SLOTXY_CHEST && !chestItem.isEmpty()) {
		holdItem = chestItem;
		if (automaticMove) {
			automaticallyUnequip = true;
			automaticallyMoved = automaticallyEquipped = AutoPlaceItemInInventory(player, holdItem, true);
		}

		if (!automaticMove || automaticallyMoved) {
			RemoveEquipment(player, INVLOC_CHEST, false);
		}
	}

	// Oracool: the six new worn slots, one range branch instead of six copies of the block above.
	// SLOTXY_* and inv_body_loc are parallel across the whole equipped range (static_assert near
	// EquipSlotForBodyLocation), so `r` IS the body location. Without this, an equipped item in
	// one of these slots simply could not be picked back out - the click fell through and did
	// nothing.
	if (r >= SLOTXY_SHOULDERS && r <= SLOTXY_BOOTS) {
		Item &wornItem = player.InvBody[r];
		if (!wornItem.isEmpty()) {
			holdItem = wornItem;
			if (automaticMove) {
				automaticallyUnequip = true;
				automaticallyMoved = automaticallyEquipped = AutoPlaceItemInInventory(player, holdItem, true);
			}

			if (!automaticMove || automaticallyMoved) {
				RemoveEquipment(player, static_cast<inv_body_loc>(r), false);
			}
		}
	}

	if (r >= SLOTXY_INV_FIRST && r <= SLOTXY_INV_LAST) {
		int ig = r - SLOTXY_INV_FIRST;
		int8_t ii = GetActiveInvGridCell(player, ig);
		if (ii != 0) {
			int iv = (ii < 0) ? -ii : ii;

			// Set when the equip swap below has already pulled the item out of the bag to free its
			// cells for the displaced one, so the removal at the bottom does not run twice. A second
			// RemoveActiveInvItem on a compacted list would delete whatever item was swapped into
			// this index.
			bool newItemTakenFromBag = false;

			holdItem = GetActiveInvListItem(player, iv - 1);
			if (automaticMove) {
				if (CanBePlacedOnBelt(holdItem)) {
					automaticallyMoved = AutoPlaceItemInBelt(player, holdItem, true);
				} else if (CanEquip(holdItem)) {
					/*
					 * Move the respective InvBodyItem to inventory before moving the item from inventory
					 * to InvBody with AutoEquip. AutoEquip requires the InvBody slot to be empty.
					 * First identify the correct InvBody slot and store it in invloc.
					 */
					automaticallyUnequip = true; // Switch to say "I have no room when inventory is too full"
					int invloc = NUM_INVLOC;
					switch (player.GetItemLocation(holdItem)) {
					case ILOC_ARMOR:
						invloc = INVLOC_CHEST;
						break;
					case ILOC_HELM:
						invloc = INVLOC_HEAD;
						break;
					case ILOC_AMULET:
						invloc = INVLOC_AMULET;
						break;
					// Oracool: shift-click on one of the new worn types targets its single slot,
					// exactly as armour and helms do above.
					case ILOC_SHOULDERS:
					case ILOC_BRACERS:
					case ILOC_GLOVES:
					case ILOC_WAIST:
					case ILOC_LEGS:
					case ILOC_BOOTS:
						invloc = OracoolBodyLocationFor(player.GetItemLocation(holdItem));
						break;
					case ILOC_ONEHAND:
						// User is attempting to move a weapon (left hand)
						if (GetActiveInvListItem(player, iv - 1)._iClass == player.InvBody[INVLOC_HAND_LEFT]._iClass
						    && player.GetItemLocation(GetActiveInvListItem(player, iv - 1)) == player.GetItemLocation(player.InvBody[INVLOC_HAND_LEFT])) {
							invloc = INVLOC_HAND_LEFT;
						}
						// User is attempting to move a shield (right hand)
						if (GetActiveInvListItem(player, iv - 1)._iClass == player.InvBody[INVLOC_HAND_RIGHT]._iClass
						    && player.GetItemLocation(GetActiveInvListItem(player, iv - 1)) == player.GetItemLocation(player.InvBody[INVLOC_HAND_RIGHT])) {
							invloc = INVLOC_HAND_RIGHT;
						}
						// A two-hand item can always be replaced with a one-hand item
						if (player.GetItemLocation(player.InvBody[INVLOC_HAND_LEFT]) == ILOC_TWOHAND) {
							invloc = INVLOC_HAND_LEFT;
						}
						break;
					case ILOC_TWOHAND:
						// Moving a two-hand item from inventory to InvBody requires emptying both hands
						if (!player.InvBody[INVLOC_HAND_RIGHT].isEmpty()) {
							holdItem = player.InvBody[INVLOC_HAND_RIGHT];
							if (!AutoPlaceItemInInventory(player, holdItem, true)) {
								// No space to  move right hand item to inventory, abort.
								break;
							}
							{
								Item placedRightHandItem = holdItem; // AutoPlaceItemInInventory already placed this copy; remember its identity in case the next check fails and it needs undoing.
								holdItem = player.InvBody[INVLOC_HAND_LEFT];
								if (!AutoPlaceItemInInventory(player, holdItem, false)) {
									// No space for left item. Move back right item to right hand and abort.
									// The earlier placement may have landed in an extra tab rather than
									// InvList (AutoPlaceItemInInventory falls back there when tab 1 is
									// full), so it must be located by identity rather than assumed to be
									// the last InvList slot.
									player.InvBody[INVLOC_HAND_RIGHT] = placedRightHandItem;
									RemoveMatchingInventoryOrExtraTabItem(player, placedRightHandItem);
									break;
								}
							}
							RemoveEquipment(player, INVLOC_HAND_RIGHT, false);
							invloc = INVLOC_HAND_LEFT;
						} else {
							invloc = INVLOC_HAND_LEFT;
						}
						break;
					default:
						automaticallyUnequip = false; // Switch to say "I can't do that"
						break;
					}
					// Empty the identified InvBody slot (invloc) and hand over to AutoEquip.
					//
					// THE DISPLACED ITEM TAKES THE HOLE THE NEW ONE LEAVES (user, 2026-09-12: "the
					// current item being replaced to take the same spot in the inventory the new item
					// just occupied, unless it is bigger, in which case look for suitable place or
					// throw on ground").
					//
					// That needed the order reversed. This used to place the displaced item while the
					// item being equipped was STILL in the grid, so the cells it is about to vacate
					// were never free and AutoPlaceItemInInventory's scan sent the displaced item
					// wherever it found room - usually the far end of the bag. The new item therefore
					// comes out first, and only then is the displaced one placed.
					if (invloc != NUM_INVLOC) {
						Item displaced = player.InvBody[invloc];
						if (!displaced.isEmpty()) {
							// Both captured BEFORE anything moves: RemoveActiveInvItem compacts the
							// list, so iv - 1 stops meaning this item the moment it is called.
							const Item equipping = GetActiveInvListItem(player, iv - 1);
							const int vacatedTab = ActiveInventoryTab;
							const int vacatedSlot = ActiveInvAnchorSlotOf(player, iv);

							RemoveActiveInvItem(player, iv - 1);
							newItemTakenFromBag = true;

							// The hole, then anywhere, then the floor. PlaceItemInTabSlot fails of
							// its own accord when the displaced item does not fit the hole, so
							// "unless it is bigger" needs no size test here.
							if (!PlaceItemInTabSlot(player, vacatedTab, vacatedSlot, displaced)
							    && !AutoPlaceItemInInventory(player, displaced, true)) {
								// Onto the floor at the player's feet. If there is nowhere to put it
								// either, TryDropItem says so in the hero's own voice and the item
								// stays on the cursor rather than being destroyed - the one outcome
								// that must never happen here, since the body slot is about to be
								// cleared.
								player.HoldItem = displaced;
								if (!TryDropItem())
									NewCursor(player.HoldItem);
							}
							player.InvBody[invloc].clear();
							holdItem = equipping;
						} else {
							holdItem = GetActiveInvListItem(player, iv - 1);
						}
					} else {
						holdItem = GetActiveInvListItem(player, iv - 1);
					}
					automaticallyMoved = automaticallyEquipped = AutoEquip(player, holdItem);
					// The equip cannot fail here - invloc was chosen from this item's own location and
					// the slot was just cleared - but if it ever did, the item is already out of the
					// bag, so put it back rather than lose it.
					if (newItemTakenFromBag && !automaticallyEquipped && !AutoPlaceItemInInventory(player, holdItem, true)) {
						player.HoldItem = holdItem;
						if (!TryDropItem())
							NewCursor(player.HoldItem);
					}
				}
			}

			// Already gone if the swap above took it out to free its cells.
			if (!newItemTakenFromBag && (!automaticMove || automaticallyMoved)) {
				RemoveActiveInvItem(player, iv - 1);
			}
		}
	}

	if (r >= SLOTXY_BELT_FIRST) {
		Item &beltItem = player.SpdList[r - SLOTXY_BELT_FIRST];
		if (!beltItem.isEmpty()) {
			holdItem = beltItem;
			if (automaticMove) {
				automaticallyMoved = AutoPlaceItemInInventory(player, holdItem, true);
			}

			if (!automaticMove || automaticallyMoved) {
				player.RemoveSpdBarItem(r - SLOTXY_BELT_FIRST);
			}
		}
	}

	if (!holdItem.isEmpty()) {
		if (holdItem._itype == ItemType::Gold) {
			player._pGold = CalculateGold(player);
		}

		CalcPlrInv(player, true);
		holdItem._iStatFlag = player.CanUseItem(holdItem);

		if (&player == MyPlayer) {
			if (automaticallyEquipped) {
				PlaySFX(ItemInvSnds[GetItemDropAnimIndexFor(holdItem)]);
			} else if (!automaticMove || automaticallyMoved) {
				PlaySFX(IS_IGRAB);
			}

			if (automaticMove) {
				if (!automaticallyMoved) {
					if (CanBePlacedOnBelt(holdItem) || automaticallyUnequip) {
						player.SaySpecific(HeroSpeech::IHaveNoRoom);
					} else {
						player.SaySpecific(HeroSpeech::ICantDoThat);
					}
				}

				holdItem.clear();
			} else {
				NewCursor(holdItem);
			}
		}
	}

	if (dropItem && !holdItem.isEmpty()) {
		TryDropItem();
	}
}

namespace {

void TryCombineNaKrulNotes(Player &player, Item &noteItem)
{
	int idx = noteItem.IDidx;
	_item_indexes notes[] = { IDI_NOTE1, IDI_NOTE2, IDI_NOTE3 };

	if (IsNoneOf(idx, IDI_NOTE1, IDI_NOTE2, IDI_NOTE3)) {
		return;
	}

	for (_item_indexes note : notes) {
		if (idx != note && !HasInventoryItemWithId(player, note)) {
			return; // the player doesn't have all notes
		}
	}

	MyPlayer->Say(HeroSpeech::JustWhatIWasLookingFor, 10);

	for (_item_indexes note : notes) {
		if (idx != note) {
			RemoveInventoryItemById(player, note);
		}
	}

	Point position = noteItem.position; // copy the position to restore it after re-initialising the item
	noteItem = {};
	GetItemAttrs(noteItem, IDI_FULLNOTE, 16);
	SetupItem(noteItem);
	noteItem.position = position; // this ensures CleanupItem removes the entry in the dropped items lookup table
}

void CheckQuestItem(Player &player, Item &questItem)
{
	Player &myPlayer = *MyPlayer;

	if (Quests[Q_BLIND]._qactive == QUEST_ACTIVE
	    && (questItem.IDidx == IDI_OPTAMULET
	        || (Quests[Q_BLIND].IsAvailable() && questItem.position == (SetPiece.position.megaToWorld() + Displacement { 5, 5 })))) {
		Quests[Q_BLIND]._qactive = QUEST_DONE;
		NetSendCmdQuest(true, Quests[Q_BLIND]);
	}

	if (questItem.IDidx == IDI_MUSHROOM && Quests[Q_MUSHROOM]._qactive == QUEST_ACTIVE && Quests[Q_MUSHROOM]._qvar1 == QS_MUSHSPAWNED) {
		player.Say(HeroSpeech::NowThatsOneBigMushroom, 10); // BUGFIX: Voice for this quest might be wrong in MP
		Quests[Q_MUSHROOM]._qvar1 = QS_MUSHPICKED;
		NetSendCmdQuest(true, Quests[Q_MUSHROOM]);
	}

	if (questItem.IDidx == IDI_ANVIL && Quests[Q_ANVIL]._qactive != QUEST_NOTAVAIL) {
		if (Quests[Q_ANVIL]._qactive == QUEST_INIT) {
			Quests[Q_ANVIL]._qactive = QUEST_ACTIVE;
			NetSendCmdQuest(true, Quests[Q_ANVIL]);
		}
		if (Quests[Q_ANVIL]._qlog) {
			myPlayer.Say(HeroSpeech::INeedToGetThisToGriswold, 10);
		}
	}

	if (questItem.IDidx == IDI_GLDNELIX && Quests[Q_VEIL]._qactive != QUEST_NOTAVAIL) {
		myPlayer.Say(HeroSpeech::INeedToGetThisToLachdanan, 30);
	}

	if (questItem.IDidx == IDI_ROCK && Quests[Q_ROCK]._qactive != QUEST_NOTAVAIL) {
		if (Quests[Q_ROCK]._qactive == QUEST_INIT) {
			Quests[Q_ROCK]._qactive = QUEST_ACTIVE;
			NetSendCmdQuest(true, Quests[Q_ROCK]);
		}
		if (Quests[Q_ROCK]._qlog) {
			myPlayer.Say(HeroSpeech::ThisMustBeWhatGriswoldWanted, 10);
		}
	}

	if (Quests[Q_BLOOD]._qactive == QUEST_ACTIVE
	    && (questItem.IDidx == IDI_ARMOFVAL
	        || (Quests[Q_BLOOD].IsAvailable() && questItem.position == (SetPiece.position.megaToWorld() + Displacement { 9, 3 })))) {
		Quests[Q_BLOOD]._qactive = QUEST_DONE;
		NetSendCmdQuest(true, Quests[Q_BLOOD]);
		myPlayer.Say(HeroSpeech::MayTheSpiritOfArkaineProtectMe, 20);
	}

	if (questItem.IDidx == IDI_MAPOFDOOM) {
		Quests[Q_GRAVE]._qactive = QUEST_ACTIVE;
		if (Quests[Q_GRAVE]._qvar1 != 1) {
			MyPlayer->Say(HeroSpeech::UhHuh, 10);
			Quests[Q_GRAVE]._qvar1 = 1;
		}
	}

	TryCombineNaKrulNotes(player, questItem);
}

void CleanupItems(int ii)
{
	auto &item = Items[ii];
	dItem[item.position.x][item.position.y] = 0;

	if (CornerStone.isAvailable() && item.position == CornerStone.position) {
		CornerStone.item.clear();
		CornerStone.item._iSelFlag = 0;
		CornerStone.item.position = { 0, 0 };
		CornerStone.item._iAnimFlag = false;
		CornerStone.item._iIdentified = false;
		CornerStone.item._iPostDraw = false;
	}

	int i = 0;
	while (i < ActiveItemCount) {
		if (ActiveItems[i] == ii) {
			DeleteItem(i);
			i = 0;
			continue;
		}

		i++;
	}
}

bool CanUseStaff(Item &staff, SpellID spell)
{
	return !staff.isEmpty()
	    && IsAnyOf(staff._iMiscId, IMISC_STAFF, IMISC_UNIQUE)
	    && staff._iSpell == spell
	    && staff._iCharges > 0;
}

void StartGoldDrop()
{
	CloseGoldWithdraw();

	const int8_t invIndex = pcursinvitem;

	const Player &myPlayer = *MyPlayer;

	const int max = (invIndex <= INVITEM_INV_LAST)
	    ? myPlayer.InvList[invIndex - INVITEM_INV_FIRST]._ivalue
	    : myPlayer.SpdList[invIndex - INVITEM_BELT_FIRST]._ivalue;

	if (talkflag)
		control_reset_talk();

	const Point start = GetPanelPosition(UiPanels::Inventory, { 67, 128 });
	SDL_Rect rect = MakeSdlRect(start.x, start.y, 180, 20);
	SDL_SetTextInputRect(&rect);

	OpenGoldDrop(invIndex, max);
}

int CreateGoldItemInInventorySlot(Player &player, int slotIndex, int value)
{
	if (player.InvGrid[slotIndex] != 0) {
		return value;
	}

	Item &goldItem = player.InvList[player._pNumInv];
	MakeGoldStack(goldItem, std::min(value, MaxGold));
	player._pNumInv++;
	player.InvGrid[slotIndex] = player._pNumInv;
	if (&player == MyPlayer) {
		NetSendCmdChInvItem(false, slotIndex);
	}

	value -= goldItem._ivalue;

	return value;
}

/**
 * @brief Oracool (2026-09-19): vanilla's slot STONE under an item, so the tint has something to tint.
 *
 * The user put the two screenshots side by side: vanilla's backing reads as a wash because its wells
 * are mid-grey textured stone and the shift keeps that texture; this fork's panel art paints every
 * well as a near-black hole, so a luminance-keeping tint of it is a flat colour at the floor - the
 * old plate by another route. The fix is not in the tint: it is to put stone back under the item.
 *
 * The stone is vanilla's own - the first backpack cell of data\inv\inv.cel, which InitInv still
 * loads for every class (1.5.5 InvRect: {17, 222} 29x29, the 28x28 well at 17, 223). Decoded ONCE
 * into a tile of palette indices and copied across the footprint, tiled every 28 pixels, before
 * TintRectRgb runs. Indices rather than colours so the same tile serves the 8-bit test surface and
 * the 32-bit screen (through PaletteRGB). Nothing is drawn when the panel is not loaded (tests),
 * and nothing for a plain item, which never reaches this - a bare slot is still the tier.
 */
constexpr int SlotStoneTileSize = 28;
std::array<uint8_t, SlotStoneTileSize * SlotStoneTileSize> SlotStoneTile;
bool SlotStoneTileReady = false;

bool EnsureSlotStoneTile()
{
	if (SlotStoneTileReady)
		return true;
	if (!pInvCels)
		return false;
	const ClxSprite panel = (*pInvCels)[0];
	constexpr Point StoneTileOrigin { 17, 223 };
	OwnedSurface tile(SlotStoneTileSize, SlotStoneTileSize);
	SDL_FillRect(tile.surface, nullptr, 0);
	// ClxDraw places the sprite's BOTTOM-left at the position, so the panel's top-left goes to
	// minus the origin and the wanted cell lands at (0, 0) of the tile; the rest is clipped away.
	ClxDraw(tile, { -StoneTileOrigin.x, -StoneTileOrigin.y + static_cast<int>(panel.height()) - 1 }, panel);
	for (int y = 0; y < SlotStoneTileSize; y++)
		for (int x = 0; x < SlotStoneTileSize; x++)
			SlotStoneTile[y * SlotStoneTileSize + x] = tile[Point { x, y }];
	SlotStoneTileReady = true;
	return true;
}

void DrawSlotStoneUnderlay(const Surface &out, Rectangle rect)
{
	if (!EnsureSlotStoneTile())
		return;
	const int x0 = std::max(rect.position.x, 0), y0 = std::max(rect.position.y, 0);
	const int x1 = std::min(rect.position.x + rect.size.width, out.w());
	const int y1 = std::min(rect.position.y + rect.size.height, out.h());
	for (int y = y0; y < y1; y++) {
		const int row = ((y - rect.position.y) % SlotStoneTileSize) * SlotStoneTileSize;
		if (out.isIndexed()) {
			uint8_t *dst = out.at(x0, y);
			for (int x = x0; x < x1; x++, dst++)
				*dst = SlotStoneTile[row + (x - rect.position.x) % SlotStoneTileSize];
		} else {
			uint32_t *dst = out.at<uint32_t>(x0, y);
			for (int x = x0; x < x1; x++, dst++)
				*dst = PaletteRGB[SlotStoneTile[row + (x - rect.position.x) % SlotStoneTileSize]];
		}
	}
}

} // namespace

void InvDrawSlotBack(const Surface &out, Point targetPosition, Size size, const Item &item)
{
	SDL_Rect srcRect = MakeSdlRect(0, 0, size.width, size.height);
	out.Clip(&srcRect, &targetPosition);
	if (size.width <= 0 || size.height <= 0)
		return;

	// Oracool, 2026-09-19: VANILLA'S METHOD, back. DevilutionX 1.5.5 never covered the slot: it read
	// the slot art back from the frame and moved its grey pixels one shade deeper into the item
	// class's colour ramp, so the stone stayed stone, tinted. That could not survive this fork's
	// true-colour panels (the test was on palette indices, of which the 32-bit screen has none), so
	// from 2026-08-16 the backing was an opaque plate with a two-pixel border - which said the tier
	// loudly and hid the art. TintRectRgb is the same idea on colour values: each pixel keeps its
	// luminance and takes the tier's hue. No border anywhere (user: "build it without borders").
	//
	// The ladder below is the one the plate had, unchanged in ORDER: sockets outrank quality (user,
	// 2026-08-20), ethereal comes next because it is the fact a player has to act on, then the
	// Oracool tiers (every one of which is ITEM_QUALITY_MAGIC underneath - see
	// GetTieredItemAffixes), then vanilla's two. A plain item gets nothing at all (user, 2026-08-16:
	// "basic item to have no backing"); vanilla's beige for it is deliberately NOT restored.
	//
	// Three numbers to tune from a screenshot, and only these: the depth vanilla's "one shade deeper"
	// comes out at, the darker depth the socket family wears (the user asked for a DARK grey / dark
	// green backing for them, and dark is a depth now, not an index), and the floor - the share of
	// the hue a black pixel still shows, so a tint on a dark well reads as colour rather than as a
	// darker well.
	constexpr int TintDepthPercent = 90;
	constexpr int DarkTintDepthPercent = 60;
	// 30 while the tint sat on the black well, where the floor was all there was to see; 10 now that
	// vanilla's stone is under it (DrawSlotStoneUnderlay) and the luminance carries the colour.
	constexpr int TintFloorPercent = 10;

	// Hues from the palette's own ramps where a ramp is the colour (the light end, +2, but only the
	// hue is used - TintRectRgb normalises it). Values where the palette has no such colour: the
	// fork's one green (0x64A064, ColorOracoolGreen's band) and the ethereal violet, both exactly the
	// values the old borders used, so nothing changes colour by moving from a border to a tint. The
	// index beside each is the ramp vanilla's shift uses on an indexed surface (tests); green has no
	// ramp there, so its fallback is the orange PAL8_GREEN names - as it was.
	uint32_t hue;
	uint8_t fallbackRamp;
	int depth = TintDepthPercent;
	if (IsInspectingPlayer()) {
		// Vanilla: every class is orange while looking at another player's gear.
		hue = PaletteRGB[PAL16_ORANGE + 2];
		fallbackRamp = PAL16_ORANGE;
	} else if (item._iSocketCount > 0) {
		// "Dark gray ... backing" for a socketed base, green for a formed runeword (user, 2026-08-20).
		// Dark is the depth; white as a hue is the stone's own grey, so this is the well pushed down.
		const bool isRuneword = oracool::GetActiveRuneword(item) != nullptr;
		hue = isRuneword ? 0x64A064u : 0xFFFFFFu;
		fallbackRamp = isRuneword ? PAL8_GREEN : PAL16_GRAY;
		depth = DarkTintDepthPercent;
	} else if (item._iOracoolEthereal) {
		// "a purple-ish backing ... for ethereal items" (user, 2026-09-13).
		hue = 0xA070E0u;
		fallbackRamp = PAL16_BLUE;
	} else if (item.hasOracoolTier() && item._iOracoolTier == OracoolItemTier::Rare) {
		// The saturated PAL8_YELLOW the rare NAME wears, not the pale unique gold (user, 2026-08-16:
		// "now is gold").
		hue = PaletteRGB[PAL8_YELLOW + 1];
		fallbackRamp = PAL8_YELLOW;
	} else if (item.hasOracoolTier() && item._iOracoolTier == OracoolItemTier::BuffedUnique) {
		hue = PaletteRGB[PAL16_YELLOW + 2];
		fallbackRamp = PAL16_YELLOW;
	} else if (item.hasOracoolTier() && item._iOracoolTier == OracoolItemTier::Primal) {
		// The beige ramp the Primal font colour (BE-2) lives on, so backing and name agree.
		hue = PaletteRGB[PAL16_BEIGE + 2];
		fallbackRamp = PAL16_BEIGE;
	} else if (item.hasOracoolTier() && item._iOracoolTier == OracoolItemTier::Set) {
		// "Green is for future Set Items" (user, 2026-08-16) and "light green, not orange"
		// (2026-09-12): the stone's own brightness, no shade taken off.
		hue = 0x64A064u;
		fallbackRamp = PAL8_GREEN;
		depth = 100;
	} else {
		switch (item._iMagical) {
		case ITEM_QUALITY_MAGIC:
			hue = PaletteRGB[PAL16_BLUE + 2];
			fallbackRamp = PAL16_BLUE;
			break;
		case ITEM_QUALITY_UNIQUE:
			hue = PaletteRGB[PAL16_YELLOW + 2];
			fallbackRamp = PAL16_YELLOW;
			break;
		default:
			// A bare slot IS the tier. Gold rides this branch too.
			return;
		}
	}

	// targetPosition is the footprint's bottom-left, as every caller passes it: one rectangle per
	// item, not vanilla's one per 28x28 cell (user, 2026-08-16: "outline the entire item, not each
	// 28x28 square" - with a tint the only difference left is whether the gutters between cells
	// take the colour, and one piece reads better).
	const Rectangle footprint { { targetPosition.x, targetPosition.y - size.height + 1 }, size };
	DrawSlotStoneUnderlay(out, footprint);
	TintRectRgb(out, footprint.position.x, footprint.position.y, footprint.size.width, footprint.size.height,
	    hue, depth, TintFloorPercent, fallbackRamp);
}

bool CanBePlacedOnBelt(const Item &item)
{
	return FitsInBeltSlot(item)
	    && item._itype != ItemType::Gold
	    && MyPlayer->CanUseItem(item)
	    && item.isUsable();
}

void FreeInvGFX()
{
	pInvCels = std::nullopt;
}

void InitInv()
{
	switch (MyPlayer->_pClass) {
	case HeroClass::Warrior:
	case HeroClass::Barbarian:
		pInvCels = LoadCel("data\\inv\\inv", static_cast<uint16_t>(SidePanelSize.width));
		break;
	case HeroClass::Rogue:
	case HeroClass::Bard:
		pInvCels = LoadCel("data\\inv\\inv_rog", static_cast<uint16_t>(SidePanelSize.width));
		break;
	case HeroClass::Sorcerer:
	case HeroClass::Necromancer:
		pInvCels = LoadCel("data\\inv\\inv_sor", static_cast<uint16_t>(SidePanelSize.width));
		break;
	case HeroClass::Monk:
		pInvCels = LoadCel(!gbIsSpawn ? "data\\inv\\inv_sor" : "data\\inv\\inv", static_cast<uint16_t>(SidePanelSize.width));
		break;
	}
}

/**
 * @brief Oracool Tabbed Inventory: draws the 10 small tab buttons in the gap between the ring
 * row and the backpack grid. Purely code-drawn text (no new art, matching the Reset Stats "R"
 * button precedent) since the gap is only ~16px tall and the ring/grid artwork - baked into the
 * game's original, non-editable inv.cel/inv_rog.cel/inv_sor.cel background images - can't move to
 * make room. Selected tab is gold and drawn a few pixels larger; inactive tabs are a muted gray.
 */
/**
 * @brief When the SORT button's click flash expires, as an SDL tick.
 *
 * SORT is a button, not a tab - there is no "selected" state for it to sit in - so the only
 * feedback that a click registered is a brief flash of the selected-tab look. Held as an expiry
 * rather than a bool because nothing polls a mouse-up here: the sort runs instantly on mouse-down,
 * and a flag cleared elsewhere would either linger or need a second owner.
 */
uint32_t InventorySortFlashUntil = 0;
/** Long enough to see, short enough not to read as a mode change. */
constexpr uint32_t InventorySortFlashMs = 170;

bool InventorySortFlashActive()
{
	return SDL_GetTicks() < InventorySortFlashUntil;
}

void DrawInventoryTabs(const Surface &out)
{
	// Oracool V1: real artwork now - roman numerals cut from the user's sheet, silver unselected
	// and gold selected. Was code-drawn text, which the old 320x352 panel forced because the
	// ring-to-grid gap was only ~16px tall and its background art could not be edited to make room.
	// The new window is composed from components, so the tab row is part of the design.
	//
	// Unselected tabs draw first so the selected one is never clipped by a neighbour. That costs
	// nothing today (ActiveTabGrow is 0, so they are all the same size) but keeps the ordering
	// correct if the selected tab is ever made to stand proud again.
	// Oracool V1: the tabs are drawn, not blitted. The v3 numeral sheet is gone - positions 0-8 are
	// the digits 1-9 and the last is "S", the sort button.
	//
	// Unselected tabs get the bevel and NOTHING behind it, so the panel shows through unchanged.
	// The selected tab is the only one with a fill, and it also grows to 30x30 - one pixel out each
	// side and two up, bottom edge pinned, so the row stays seated on one line and only the open
	// tab stands proud. Drawn in a second pass for that reason: its extra two pixels would
	// otherwise be painted over by whichever neighbour drew next.
	const Rectangle panel = oracool::GetInventoryPanelRect();

	// PLATES (user, 2026-09-06: "tab icons above inv grid - to utilize 28x28px vanilla backing - grey
	// for inactive, gold for active. gold to be again bigger to stand out from inactive"). Each tab
	// is the vanilla spell plate shrunk to its 28x28 cell: light grey (Unspent) when closed, GOLD
	// (Ready) and grown to GetActiveTabRect - two out each side, three up, bottom pinned - when
	// open. Hover turns the numeral gold (user, 2026-09-06: "shadows of inv grid tabs is too much.
	// dont use it here. hover efect to be turning the number from white to gold").
	//
	// The reliquary-chest atlas (oracool-stash-tab-button-pack, 2026-08-16) is no longer drawn and
	// stays filed; the code-drawn bevelled strip that preceded it is in the history at v1.9.289.
	// The tab's glyph is a ChatGPT commission (brief in .ProjectDocumentation/06-Reference); until it
	// lands the numeral stands in, white with the text shadow, like the belt's TP and M.
	const auto drawTab = [&](int tab, Rectangle panelRect, oracool::SkillPlateTint tint) {
		const Rectangle screenRect { panel.position + Displacement { panelRect.position.x, panelRect.position.y }, panelRect.size };
		oracool::DrawPlateIn(out, screenRect, tint);
		const bool open = tab == ActiveInventoryTab;
		const bool hovered = !open && screenRect.contains(MousePosition);
		// The chest glyph (oracool-tab-chest-glyphs-v1, in since v1.9.293): lid down on a closed tab,
		// raised on the open one, gold under the cursor. The numerals (v1.9.292) were not liked; the
		// font's numeral stands in when the strip is missing.
		if (!oracool::DrawTabGlyph(out, screenRect, open, hovered)) {
			DrawString(out, oracool::TabLabel(tab), screenRect,
			    { (hovered ? UiFlags::ColorGold : UiFlags::ColorWhite) | UiFlags::FontSize12 | UiFlags::AlignCenter | UiFlags::VerticalCenter | UiFlags::Shadowed });
		}
	};
	// Every other tab first, the open one last: it stands proud of its neighbours by two pixels
	// each side, so drawing in index order would let tab N+1 paint over its edge.
	for (int tab = 0; tab < oracool::TabCount; tab++) {
		if (tab != ActiveInventoryTab)
			drawTab(tab, oracool::GetTabRect(tab), oracool::SkillPlateTint::Unspent);
	}
	drawTab(ActiveInventoryTab, oracool::GetActiveTabRect(ActiveInventoryTab), oracool::SkillPlateTint::Ready);
}

/**
 * @brief Oracool V1: the SORT button and the player's gold, in the band under the backpack grid.
 *
 * Drawn from scrollrt rather than from DrawInv, and BEFORE the orbs - the reverse of what it did
 * until 1.5.23. At 340x720 the inventory reaches the bottom of the screen and the mana orb
 * (x 658..755, y 624..720 at 960x720) sits over this band, so the two have to be ordered
 * deliberately either way; drawing the text last was the first answer and the user's call is the
 * other one: "mana orb to draw in front of sort and gold texts". An orb with a word printed across
 * its glass looked worse than a word with its tail tucked behind one.
 *
 * The label was shortened in the same pass, which is what makes the ordering nearly moot: these rows
 * are centred at screen x 790 and the orb ends at 755, so a SHORT label never reaches it. It was
 * "SORT INVENTORY" - about 110px, spreading to x 735 and well into the glass. "SORT" stops around
 * 772. The z-order now only covers the gold row, whose longest values still creep left.
 */
void DrawInventoryFooter(const Surface &out)
{
	if (!invflag || !oracool::IsSinglePlayer())
		return;

	const Rectangle panel = oracool::GetInventoryPanelRect();
	const auto toScreen = [&panel](Rectangle r) {
		return Rectangle { panel.position + Displacement { r.position.x, r.position.y }, r.size };
	};

	// Both rows are Shadowed (user, 2026-09-02: "the shadows of SORT and GOLD in inventory were
	// supposed to be black font offset a bit and draw behind. like in the hero stats windows").
	//
	// The first attempt, hours earlier, put a half-transparent PLATE behind each word. That is the
	// badge treatment - right for a label stuck on an icon, wrong here, where the two words sit on
	// open stone and a plate reads as a box someone forgot to finish drawing. UiFlags::Shadowed is
	// what the character sheet uses (see CharTextShadow there) and what vanilla does throughout: the
	// same glyphs in black, one pixel down and right, under the real ones. It costs no rectangle and
	// works whatever the word's own colour is.

	// Gold, and white for the moment after a click - the same treatment and the same word the stash's
	// own Sort button uses, so the two read as one control in two windows.
	// Left-aligned on tab 1's border (user, 2026-08-19), the mirror of the gold counter's right edge
	// on tab 10's. The rect already started at TabColumnX(0); it was the TEXT that sat centred inside
	// two tabs' width, which put the word a few pixels shy of the border it was supposed to line up
	// with. There is no AlignLeft flag - left is what you get by not asking for centre or right - so
	// this is a deletion rather than a substitution.
	DrawString(out, _("SORT"), toScreen(oracool::GetSortButtonRect()),
	    { (InventorySortFlashActive() ? UiFlags::ColorWhite : UiFlags::ColorGold)
	        | UiFlags::VerticalCenter | UiFlags::Shadowed });

	// TotalPlayerGold(), the same call the store screen uses and the same sum the character sheet
	// shows. Gold reads 0 from the player alone in this project: picked-up and sold gold goes to
	// the shared Stash pool, so the money lives in Stash.gold, not _pGold. Two earlier attempts
	// here - CalculateGold over InvList, then _pGold - both read player-side fields and both
	// therefore showed 0 while the sheet showed the real figure.
	//
	// Digits only, right-aligned on tab 10's border (user, 2026-08-19: "remove the GOLD: text. leave
	// only the digits. Their a information enough."). The label was carrying no information the
	// figure does not - it sits in the gold colour, on the money row, in a window whose only number
	// is money - and dropping it also drops the one part of this readout that needed translating.
	DrawString(out, FormatInteger(TotalPlayerGold()), toScreen(oracool::GetGoldRowRect()),
	    { UiFlags::ColorWhitegold | UiFlags::AlignRight | UiFlags::VerticalCenter | UiFlags::Shadowed });
}

bool inventorySortButtonDown;

void DrawInv(const Surface &out)
{
	// Oracool V1: the window wears the shared theme - a half-transparent fill under the ornate
	// textbox_frame00 bevel - the same treatment as the waypoint list, quest log and event log. It
	// replaces the composed stone panel (ui\inventory_panel.png), which is no longer drawn.
	//
	// That art carried the class silhouette baked into it, so the silhouette is gone with it. The
	// slot frames and grid it also carried are now drawn here instead, procedurally, which is what
	// lets them take the bevel and their own fill.
	//
	// Oracool (2026-08-16): superseded again, by ui\inventory_background.png - a 340x720 painted
	// panel that carries its own arches and border, so it replaces BOTH the fill and the bevel.
	// The procedural pair is kept as the fallback, which is what makes the art droppable: delete
	// the PNG and the window goes back to the shared theme rather than rendering as a hole.
	//
	// Only the background moved. The title, equipment slots, grid, tabs and footer are still drawn
	// here, on top - the art deliberately carries none of them, so the layout stays the code's.
	const Rectangle invPanel = oracool::GetInventoryPanelRect();
	// Three deep now (2026-08-18, unit D): the shared limestone panel, then the older painted
	// inventory background, then the procedural fill. The inventory used to be the one window with a
	// background all its own; it joined the shared family when that family arrived, and the old art
	// stays as the middle rung rather than being deleted.
	if (oracool::HasSidePanelArt()) {
		oracool::DrawSidePanelArt(out, invPanel.position);
	} else if (oracool::HasInventoryPanelArt()) {
		oracool::DrawInventoryPanelArt(out);
	} else {
		oracool::DrawThemedFill(out, invPanel);
		oracool::DrawOrnateBorder(out, invPanel);
	}

	// The class figure, behind the equipment slots. Drawn between the panel fill and the slots so
	// the slots sit on top of it, exactly as they did when both were baked into the old panel art.
	//
	// Sits ABOVE the panel margin on purpose. At the margin its 370px reached y 398 and its feet
	// stopped two pixels short of the tab row, which read as the figure standing on the grid.
	// Raising it clears the tab row by 14. The panel's bevel only occupies the first 3px, so there
	// is nothing up here for it to collide with.
	constexpr int SilhouetteTop = 16;
	oracool::DrawClassSilhouette(out, invPanel.position, oracool::InventoryPanelSize.width, SilhouetteTop);

	// "INVENTORY", in the band between the panel's top edge and the helm slot. Drawn AFTER the
	// silhouette so it sits over the figure's head rather than under it - that band is the only
	// space above the equipment block, and the silhouette reaches into it. The outline is what
	// keeps it legible there.
	//
	// The band's bottom is taken from the helm slot itself, so the title follows if the equipment
	// block ever moves again.
	{
		// User request (2026-08-16): 12px from the top, matching the stash. The band is exactly one
		// FontSize30 line rather than stretching to the helm slot, so VerticalCenter has no slack
		// to drift in - the title's top edge IS PanelTitleTop... for every other window. The INVENTORY
		// title alone sits 617px lower (user, 2026-09-06: "move inventory title 617px lower. just the
		// inventory. leave other canvas windows alone"): a band at y 645..683 of the 720px canvas.
		constexpr int InventoryTitleDrop = 617;
		const Rectangle titleArea {
			invPanel.position + Displacement { oracool::PanelMargin, oracool::PanelTitleTop + InventoryTitleDrop },
			{ oracool::InventoryPanelSize.width - 2 * oracool::PanelMargin, oracool::PanelTitleHeight }
		};
		oracool::DrawOutlinedString(out, _("INVENTORY"), titleArea,
		    UiFlags::ColorWhitegold | UiFlags::FontSize30 | UiFlags::AlignCenter | UiFlags::VerticalCenter);
	}

	// Equipment slots and the backpack grid sit at two fill passes against the panel's one, so they
	// read as recesses cut into it rather than outlines drawn on it.
	constexpr int RecessPasses = 2;
	for (int s = 0; s < oracool::EquipSlotCount; s++) {
		const Rectangle r = oracool::GetEquipSlotRect(static_cast<oracool::EquipSlot>(s));
		const Rectangle screenRect { invPanel.position + Displacement { r.position.x, r.position.y }, r.size };
		oracool::DrawThemedFill(out, screenRect, RecessPasses);
		// The carved stone bezel replaces the 3px bevel around each slot. All four slot shapes this
		// fork uses (1x1, 2x1, 2x2, 2x3) have one; the bevel stays as the fallback so a missing or
		// unreadable PNG costs the slot its stone, not its outline.
		//
		// Note the two frames sit differently: the bezel is OUTSIDE the slot, so the slot keeps all
		// 28 pixels of each cell, where DrawOrnateBorder drew inside and took three from every edge.
		if (oracool::HasGridBezel(screenRect.size)) {
			// The Abilities window's slot shadow, here too (user, 2026-09-05: "apply the same shadow
			// to inventory and stash slots"): the bezel's footprint, three left and three down.
			oracool::DrawDropShadow(out, screenRect, oracool::GridBezelInset);
			oracool::DrawGridBezel(out, screenRect);
		} else {
			oracool::DrawOrnateBorder(out, screenRect);
		}
	}

	const Rectangle gridRect {
		invPanel.position + Displacement { oracool::GridOrigin.x, oracool::GridOrigin.y },
		{ oracool::GridSizeInCells.width * oracool::CellPx, oracool::GridSizeInCells.height * oracool::CellPx }
	};
	oracool::DrawThemedFill(out, gridRect, RecessPasses);
	const bool gridHasBezel = oracool::HasGridBezel(gridRect.size);
	// One bevel around the whole grid, with 1px grey rules between the cells - the same treatment
	// the stash grid uses, on user request, and shared through oracool::ThemeGridLineColor so the
	// two cannot drift.
	//
	// These were 3px gold separators, centred on each boundary. Bevelling all seventy cells
	// individually was never on the table (70 three-pixel frames in a 280x196 box is noise), but
	// even as plain rules the gold dominated: it drew the grid rather than the items in it. One
	// dark pixel per boundary separates the cells and gets out of the way.
	for (int c = 1; c < oracool::GridSizeInCells.width; c++) {
		const int x = gridRect.position.x + c * oracool::CellPx - 1;
		DrawVerticalLine(out, { x, gridRect.position.y }, gridRect.size.height, oracool::ThemeGridLineColor);
	}
	for (int r = 1; r < oracool::GridSizeInCells.height; r++) {
		const int y = gridRect.position.y + r * oracool::CellPx - 1;
		DrawHorizontalLine(out, { gridRect.position.x, y }, gridRect.size.width, oracool::ThemeGridLineColor);
	}
	// Framed from OUTSIDE the cells (user request, 2026-08-16), so the outer row and column keep
	// all 28 of their pixels instead of surrendering 3 to the bevel. The grid was raised by the
	// frame's width to pay for it - see GridOrigin, which subtracts it from the mana orb's
	// clearance line rather than sitting flush against it.
	if (gridHasBezel) {
		oracool::DrawDropShadow(out, gridRect, oracool::GridBezelInset); // the slot shadow (2026-09-05)
		oracool::DrawGridBezel(out, gridRect);
	} else {
		oracool::DrawOrnateBorderOutside(out, gridRect);
	}

	// Oracool bug fix: user report - the game crashed as soon as one of the six new slots held an
	// item. This was a hand-written 7-entry table indexed by `slot`, which now runs to 12: reading
	// slotSize[7..12] walked off the end of a stack array, and a smashed stack takes the next
	// unrelated thing down with it (the user also saw a crash equipping a plain magic amulet).
	//
	// Derived from the layout table for the same reason slotPos below is - one source of truth, and
	// total by construction, so a fourteenth slot cannot reintroduce this.
	Size slotSize[NUM_INVLOC];
	for (int slot = INVLOC_HEAD; slot < NUM_INVLOC; slot++) {
		const Size rectSize = oracool::GetEquipSlotRect(EquipSlotForBodyLocation(slot)).size;
		slotSize[slot] = { rectSize.width / oracool::CellPx, rectSize.height / oracool::CellPx };
	}

	// Item sprites draw from their bottom-left corner, so each equipment slot's draw origin is the
	// bottom-left of its rect. Taken from InvRect rather than a second hardcoded table: its first
	// seven entries are the equipment rects, in this exact order, generated from the same layout
	// the artwork was composed against.
	Point slotPos[NUM_INVLOC];
	for (int slot = INVLOC_HEAD; slot < NUM_INVLOC; slot++) {
		slotPos[slot] = { InvRect[slot].position.x, InvRect[slot].position.y + InvRect[slot].size.height };
	}

	Player &myPlayer = *InspectPlayer;

	for (int slot = INVLOC_HEAD; slot < NUM_INVLOC; slot++) {
		if (!myPlayer.InvBody[slot].isEmpty()) {
			int screenX = slotPos[slot].x;
			int screenY = slotPos[slot].y;
			InvDrawSlotBack(out, GetPanelPosition(UiPanels::Inventory, { screenX, screenY }), { slotSize[slot].width * InventorySlotSizeInPixels.width, slotSize[slot].height * InventorySlotSizeInPixels.height }, myPlayer.InvBody[slot]);

			const int cursId = myPlayer.InvBody[slot]._iCurs + CURSOR_FIRSTITEM;

			auto frameSize = GetInvItemSize(cursId);

			// calc item offsets for weapons/armor smaller than 2x3 slots
			if (IsAnyOf(slot, INVLOC_HAND_LEFT, INVLOC_HAND_RIGHT, INVLOC_CHEST)) {
				screenX += frameSize.width == InventorySlotSizeInPixels.width ? INV_SLOT_HALF_SIZE_PX : 0;
				screenY += frameSize.height == (3 * InventorySlotSizeInPixels.height) ? 0 : -INV_SLOT_HALF_SIZE_PX;
			}

			const ClxSprite sprite = GetInvItemSprite(cursId);
			const Point position = GetPanelPosition(UiPanels::Inventory, { screenX, screenY });

			if (pcursinvitem == slot) {
				ClxDrawOutline(out, GetOutlineColor(myPlayer.InvBody[slot], true), position, sprite);
			}

			DrawItem(myPlayer.InvBody[slot], out, position, sprite);
			// The WORN item's sockets (user, 2026-09-07: "i dont see the gold rings over the asset of a
			// six socket staff. bug?" - it was equipped). The backpack, the stash and Levski's grid all
			// drew the overlay on hover; the body slots never did, so a socketed weapon lost its rings
			// the moment it was wielded. Same call, same hover rule. `position` is the sprite's
			// bottom-left after the centring above, which is where the footprint's cells start.
			if (pcursinvitem == slot)
				oracool::DrawSocketOverlay(out, myPlayer.InvBody[slot], position, GetInventorySize(myPlayer.InvBody[slot]));

			if (slot == INVLOC_HAND_LEFT) {
				if (myPlayer.GetItemLocation(myPlayer.InvBody[slot]) == ILOC_TWOHAND) {
					InvDrawSlotBack(out, GetPanelPosition(UiPanels::Inventory, slotPos[INVLOC_HAND_RIGHT]), { slotSize[INVLOC_HAND_RIGHT].width * InventorySlotSizeInPixels.width, slotSize[INVLOC_HAND_RIGHT].height * InventorySlotSizeInPixels.height }, myPlayer.InvBody[slot]);
					const int dstX = oracool::GetInventoryPanelRect().position.x + slotPos[INVLOC_HAND_RIGHT].x + (frameSize.width == InventorySlotSizeInPixels.width ? INV_SLOT_HALF_SIZE_PX : 0) - 1;
					const int dstY = oracool::GetInventoryPanelRect().position.y + slotPos[INVLOC_HAND_RIGHT].y;
					ClxDrawBlended(out, { dstX, dstY }, sprite);
				}
			}
		}
	}

	for (int i = 0; i < InventoryGridCells; i++) {
		int8_t cell = GetActiveInvGridCell(myPlayer, i);
		// User request (2026-08-16): "outline the entire item, not each 28x28 square". This used to
		// run on EVERY occupied cell (`cell != 0`) and paint a 28x28 backing in each, so a 2x3
		// weapon wore six separate borders in a grid pattern.
		//
		// `cell > 0` is the item's first slot - the same test the sprite loop below uses - and the
		// item's own footprint is the size to paint, so one backing wraps the whole thing. The
		// equipment slots never had this problem: they always passed the slot's full size.
		if (cell > 0) {
			Item &gridItem = GetActiveInvListItem(myPlayer, cell - 1);
			const Size itemCells = GetInventorySize(gridItem);
			InvDrawSlotBack(
			    out,
			    GetPanelPosition(UiPanels::Inventory, InvRect[i + SLOTXY_INV_FIRST].position) + Displacement { 0, InventorySlotSizeInPixels.height },
			    { itemCells.width * InventorySlotSizeInPixels.width, itemCells.height * InventorySlotSizeInPixels.height },
			    gridItem);
		}
	}

	for (int j = 0; j < InventoryGridCells; j++) {
		int8_t cell = GetActiveInvGridCell(myPlayer, j);
		if (cell > 0) { // first slot of an item
			int ii = cell - 1;
			Item &invItem = GetActiveInvListItem(myPlayer, ii);
			int cursId = invItem._iCurs + CURSOR_FIRSTITEM;

			const ClxSprite sprite = GetInvItemSprite(cursId);
			const Point position = GetPanelPosition(UiPanels::Inventory, InvRect[j + SLOTXY_INV_FIRST].position) + Displacement { 0, InventorySlotSizeInPixels.height };
			const bool hovered = IsActiveInvItemHovered(ii);
			if (hovered) {
				ClxDrawOutline(out, GetOutlineColor(invItem, true), position, sprite);
			}

			DrawItem(invItem, out, position, sprite);
			// AFTER the sprite, on purpose: the user asked for the stones and rings "rendered on
			// top of the sprite", and hover-only so a full stash does not turn into a wall of runes.
			if (hovered)
				oracool::DrawSocketOverlay(out, invItem, position, GetInventorySize(invItem));
		}
	}

	if (TabbedInventoryEnabled())
		DrawInventoryTabs(out);

	// The SORT button and the gold readout are NOT drawn here - see DrawInventoryFooter's own comment.
	// They have to come after the orbs, which scrollrt draws long after this function.
}

void DrawInvBelt(const Surface &out)
{
	if (talkflag) {
		return;
	}

	Player &myPlayer = *InspectPlayer;

	// Oracool: HUD art pass - the plate art (drawn by oracool::DrawMiddleHudArt before this)
	// carries the cell frames and the Menu/1-4/Portal labels, so this only draws the item sprites
	// themselves, centered in the art's cells. Slots 0 (Menu) and 5 (Town Portal) are pure
	// buttons - nothing to draw for them here. The old hotkey-number overlay is gone too: the
	// art's baked-in 1-4 labels replace it.
	for (int i = 1; i <= 4; i++) {
		// Oracool: the gold plate under every item slot, filled or not (user, 2026-09-06).
		oracool::DrawBeltSlotPlate(out, oracool::GetBeltSlotRect(i));
		if (myPlayer.SpdList[i].isEmpty()) {
			continue;
		}

		// Oracool: user tuning (2026-08-11) - item sprites carry their own internal padding, which
		// is not symmetric, so geometric centring alone leaves them looking left of centre in the
		// cell. This nudge is measured by eye against the art, not derived.
		//
		// Which is exactly why it does not apply to the PLATELESS row: it was measured against the
		// plate's cell, and that cell is gone. The plateless cell is sized so the backing's recess
		// comes out at 28x28 - the sprite's own size - so the sprite fills it and there is nothing
		// left to nudge. Keeping the +3 here would push every potion three pixels off the well it
		// was sized to fit, which is the one thing the sizing was for (user, 2026-08-30).
		//
		// And it does not apply to the FIFTH HUD either (2026-09-05): its belt cells are the
		// painting's own holes, 28 wide like the sprite, so as on the plateless row there is nothing
		// to nudge into and +3 would hang the potion over the rim. The +3 was the third plate's and
		// is kept in the history with it.
		const Displacement BeltItemNudge { 0, 0 };
		const Rectangle cell = oracool::GetBeltSlotRect(i);
		const Point position = cell.position + BeltItemNudge
		    + Displacement { (cell.size.width - InventorySlotSizeInPixels.width) / 2,
			      (cell.size.height - InventorySlotSizeInPixels.height) / 2 + InventorySlotSizeInPixels.height };
		const int cursId = myPlayer.SpdList[i]._iCurs + CURSOR_FIRSTITEM;

		const ClxSprite sprite = GetInvItemSprite(cursId);

		// Oracool: the item's own shadow on the plate, before the outline and the item (2026-09-06).
		oracool::DrawBeltItemShadow(out, position, sprite);

		if (pcursinvitem == i + INVITEM_BELT_FIRST) {
			if (ControlMode == ControlTypes::KeyboardAndMouse || invflag) {
				ClxDrawOutline(out, GetOutlineColor(myPlayer.SpdList[i], true), position, sprite);
			}
		}

		DrawItem(myPlayer.SpdList[i], out, position, sprite);
	}
}

void RemoveEquipment(Player &player, inv_body_loc bodyLocation, bool hiPri)
{
	if (&player == MyPlayer) {
		NetSendCmdDelItem(hiPri, bodyLocation);
		// Oracool: user request - equipment changes persist instantly, matching Diablo 3's
		// always-saved progress rather than waiting for the next scheduled/periodic save.
		oracool::ScheduleAutoSaveForEquipmentChange();
	}

	player.InvBody[bodyLocation].clear();
}

void BreakOrRemoveEquipment(Player &player, inv_body_loc bodyLocation, bool hiPri)
{
	if (oracool::IsSinglePlayer()) {
		Item &item = player.InvBody[bodyLocation];
		item._iDurability = 0;
		item._iOracoolBroken = true;
		oracool::ScheduleAutoSaveForItemBreak();
		return;
	}

	RemoveEquipment(player, bodyLocation, hiPri);
}

/**
 * @brief Merges as much of @p item as will fit into matching belt stacks.
 *
 * @return how many UNITS were taken. Zero means nothing merged.
 *
 * This returned `bool` and moved exactly ONE unit however many the incoming stack held, while every
 * caller read the `true` as "the source is dealt with" and discarded all of it. Withdrawing forty
 * gems onto a stack of fifty produced fifty-one and destroyed thirty-nine (external audit,
 * 2026-08-25). The count is the fix: a caller cannot mistake "5 of 40" for "all of it".
 *
 * Honours each destination's headroom and keeps going across slots, so a large stack can fill
 * several partial ones. With @p persistItem false nothing is written and the return is what WOULD
 * be taken, which is what makes the probe-then-commit callers correct.
 */
int MergeStackableItemIntoBelt(Player &player, const Item &item, bool persistItem)
{
	int remaining = item.stackCount();
	for (int i = 0; i < MaxBeltItems && remaining > 0; i++) {
		if (!oracool::IsRealBeltItemSlot(i))
			continue; // Oracool: HUD overhaul - slot 0 is Menu, 5 is Town Portal, 6/7 are hidden
		Item &beltItem = player.SpdList[i];
		if (!beltItem.canStackWith(item) || beltItem.stackCount() >= Item::MaxStackCount)
			continue;

		const int take = std::min(Item::MaxStackCount - beltItem.stackCount(), remaining);
		if (take <= 0)
			continue;
		remaining -= take;

		if (persistItem) {
			beltItem.setStackCount(beltItem.stackCount() + take);
			player.CalcScrolls();
			RedrawComponent(PanelDrawComponent::Belt);
			if (&player == MyPlayer) {
				NetSendCmdChBeltItem(false, i);
			}
		}
	}

	return item.stackCount() - remaining;
}

bool AutoPlaceItemInBelt(Player &player, const Item &item, bool persistItem)
{
	if (!CanBePlacedOnBelt(item)) {
		return false;
	}

	// All-or-nothing (external audit, 2026-09-06: INV-01). The merge below WROTE into partial
	// stacks before it was known whether the remainder had a slot, so a false return had already
	// changed the belt while the caller, told "not placed", kept the whole source - the merged
	// units existed twice. A commit is now preceded by a full probe of the same decisions, and
	// nothing is written unless all of it fits. The probe and the commit walk the same slots in
	// the same order, and merging does not change which slots are empty, so they agree.
	if (persistItem && !AutoPlaceItemInBelt(player, item, /*persistItem=*/false))
		return false;

	// Merge what fits, then put the REMAINDER in an empty slot - see AutoPlaceItemInInventory for
	// why returning true on a partial merge destroyed the rest.
	Item remainder = item;
	if (oracool::IsSinglePlayer() && item.isStackableConsumable()) {
		const int merged = MergeStackableItemIntoBelt(player, remainder, persistItem);
		if (merged >= remainder.stackCount())
			return true;
		if (merged > 0)
			remainder.setStackCount(remainder.stackCount() - merged);
	}

	for (int i = 0; i < MaxBeltItems; i++) {
		if (!oracool::IsRealBeltItemSlot(i))
			continue; // Oracool: HUD overhaul - slot 0 is Menu, 5 is Town Portal, 6/7 are hidden
		Item &beltItem = player.SpdList[i];
		if (beltItem.isEmpty()) {
			if (persistItem) {
				beltItem = remainder;
				player.CalcScrolls();
				RedrawComponent(PanelDrawComponent::Belt);
				if (&player == MyPlayer) {
					NetSendCmdChBeltItem(false, i);
				}
			}

			return true;
		}
	}

	return false;
}

bool AutoEquip(Player &player, const Item &item, bool persistItem)
{
	if (!CanEquip(item)) {
		return false;
	}

	for (int bodyLocation = INVLOC_HEAD; bodyLocation < NUM_INVLOC; bodyLocation++) {
		if (AutoEquip(player, item, (inv_body_loc)bodyLocation, persistItem)) {
			return true;
		}
	}

	return false;
}

bool AutoEquipEnabled(const Player &player, const Item &item)
{
	if (item.isWeapon()) {
		// Monk can use unarmed attack as an encouraged option, thus we do not automatically equip weapons on him so as to not
		// annoy players who prefer that playstyle.
		return player._pClass != HeroClass::Monk && *sgOptions.Gameplay.autoEquipWeapons;
	}

	// Oracool: the new worn types ride the existing armour toggle rather than getting six options
	// of their own - they are armour by any reasonable reading, and a player who wants armour
	// auto-equipped wants their boots auto-equipped.
	if (item.isArmor() || item.isOracoolWorn()) {
		return *sgOptions.Gameplay.autoEquipArmor;
	}

	if (item.isHelm()) {
		return *sgOptions.Gameplay.autoEquipHelms;
	}

	if (item.isShield()) {
		return *sgOptions.Gameplay.autoEquipShields;
	}

	if (item.isJewelry()) {
		return *sgOptions.Gameplay.autoEquipJewelry;
	}

	return true;
}

/** @brief The backpack's half of the same fix - see MergeStackableItemIntoBelt for the whole story. */
int MergeStackableItemIntoInventory(Player &player, const Item &item, bool persistItem)
{
	int remaining = item.stackCount();
	for (int i = 0; i < player._pNumInv && remaining > 0; i++) {
		Item &existing = player.InvList[i];
		if (!existing.canStackWith(item) || existing.stackCount() >= Item::MaxStackCount)
			continue;

		const int take = std::min(Item::MaxStackCount - existing.stackCount(), remaining);
		if (take <= 0)
			continue;
		remaining -= take;

		if (persistItem) {
			existing.setStackCount(existing.stackCount() + take);
			NetSyncInvItem(player, i);
		}
	}

	// A matching stack sitting in one of the 9 Tabbed Inventory extra tabs is just as valid a
	// merge target as one in the real backpack - RefillBeltSlotFromInventory already treats
	// extra tabs this way for belt refills, so a fresh pickup/purchase should too rather than
	// creating a redundant new stack while an existing one in an extra tab goes untouched.
	if (TabbedInventoryEnabled()) {
		for (int t = 0; t < Player::NumExtraInventoryTabs && remaining > 0; t++) {
			for (int i = 0; i < player._pNumInvTab[t] && remaining > 0; i++) {
				Item &existing = player.InvTabList[t][i];
				if (!existing.canStackWith(item) || existing.stackCount() >= Item::MaxStackCount)
					continue;

				const int take = std::min(Item::MaxStackCount - existing.stackCount(), remaining);
				if (take <= 0)
					continue;
				remaining -= take;

				if (persistItem)
					existing.setStackCount(existing.stackCount() + take);
			}
		}
	}

	return item.stackCount() - remaining;
}

bool AutoPlaceItemInInventory(Player &player, const Item &item, bool persistItem)
{
	// Merge what fits into existing stacks, then place whatever is LEFT as a new one. The old shape
	// returned true the moment anything merged, which is how a partly-merged stack lost its
	// remainder: the caller heard "placed" and threw the source away.
	//
	// `remainder` is a local copy, so decrementing it is safe on the probe path too - nothing the
	// caller owns is touched until persistItem says so.
	//
	// And nothing is touched at all unless the WHOLE item fits (external audit, 2026-09-06:
	// INV-01): the merge used to be committed before the remainder's cell was looked for, so a
	// false return had already topped up stacks the caller then kept the source of. The probe
	// below makes every decision the commit will make, without writing.
	if (persistItem && !AutoPlaceItemInInventory(player, item, /*persistItem=*/false))
		return false;
	Item remainder = item;
	if (oracool::IsSinglePlayer() && item.isStackableConsumable()) {
		const int merged = MergeStackableItemIntoInventory(player, remainder, persistItem);
		if (merged >= remainder.stackCount())
			return true;
		if (merged > 0)
			remainder.setStackCount(remainder.stackCount() - merged);
	}

	Size itemSize = GetInventorySize(remainder);
	bool placed = false;

	if (itemSize.height == 1) {
		for (int i = 30; i <= 39 && !placed; i++)
			placed = AutoPlaceItemInInventorySlot(player, i, remainder, persistItem);
		for (int x = 9; x >= 0 && !placed; x--) {
			for (int y = 2; y >= 0 && !placed; y--)
				placed = AutoPlaceItemInInventorySlot(player, 10 * y + x, remainder, persistItem);
		}
	} else if (itemSize.height == 2) {
		for (int x = 10 - itemSize.width; x >= 0 && !placed; x -= itemSize.width) {
			for (int y = 0; y < 3 && !placed; y++)
				placed = AutoPlaceItemInInventorySlot(player, 10 * y + x, remainder, persistItem);
		}
		if (!placed && itemSize.width == 2) {
			for (int x = 7; x >= 0 && !placed; x -= 2) {
				for (int y = 0; y < 3 && !placed; y++)
					placed = AutoPlaceItemInInventorySlot(player, 10 * y + x, remainder, persistItem);
			}
		}
	} else if (itemSize == Size { 1, 3 }) {
		for (int i = 0; i < 20 && !placed; i++)
			placed = AutoPlaceItemInInventorySlot(player, i, remainder, persistItem);
	} else if (itemSize == Size { 2, 3 }) {
		for (int i = 0; i < 9 && !placed; i++)
			placed = AutoPlaceItemInInventorySlot(player, i, remainder, persistItem);
		if (!placed) {
			for (int i = 10; i < 19 && !placed; i++)
				placed = AutoPlaceItemInInventorySlot(player, i, remainder, persistItem);
		}
	} else {
		app_fatal(StrCat("Unknown item size: ", itemSize.width, "x", itemSize.height));
	}

	// Oracool Tabbed Inventory: every caller of this function - store purchases, manual and
	// auto ground pickup, Stash withdrawal, auto-equip's displacement shuffling - should see the
	// extra tabs as real usable space once tab 1 has no room, not just the store purchase path
	// this was first added for.
	//
	// The REMAINDER, not the item: the fallback was handed the original stack after part of it
	// had already merged, so an extra tab received units the backpack had also kept (external
	// audit, 2026-09-06: INV-01).
	if (!placed && TabbedInventoryEnabled())
		placed = AutoPlaceItemInExtraTabs(player, remainder, persistItem);

	return placed;
}

bool AutoPlaceItemInInventorySlot(Player &player, int slotIndex, const Item &item, bool persistItem)
{
	int yy = (slotIndex > 0) ? (10 * (slotIndex / 10)) : 0;

	Size itemSize = GetInventorySize(item);
	for (int j = 0; j < itemSize.height; j++) {
		if (yy >= InventoryGridCells) {
			return false;
		}
		int xx = (slotIndex > 0) ? (slotIndex % 10) : 0;
		for (int i = 0; i < itemSize.width; i++) {
			if (xx >= 10 || player.InvGrid[xx + yy] != 0) {
				return false;
			}
			xx++;
		}
		yy += 10;
	}

	if (persistItem) {
		player.InvList[player._pNumInv] = item;
		player._pNumInv++;

		AddItemToInvGrid(player, slotIndex, player._pNumInv, itemSize);
		player.CalcScrolls();
	}

	return true;
}

/**
 * @brief Oracool Tabbed Inventory: tab-index-parameterized equivalent of
 * AutoPlaceItemInInventorySlot, operating directly on InvTabGrid[tabIndex]/InvTabList[tabIndex]/
 * _pNumInvTab[tabIndex] rather than the ActiveInventoryTab-driven accessors - this runs during
 * a store purchase, a background operation unrelated to whichever page is currently displayed,
 * so it must never depend on (or disturb) ActiveInventoryTab.
 */
bool AutoPlaceItemInExtraTabSlot(Player &player, int tabIndex, int slotIndex, const Item &item, bool persistItem)
{
	auto &grid = player.InvTabGrid[tabIndex];

	int yy = (slotIndex > 0) ? (10 * (slotIndex / 10)) : 0;

	Size itemSize = GetInventorySize(item);
	for (int j = 0; j < itemSize.height; j++) {
		if (yy >= InventoryGridCells) {
			return false;
		}
		int xx = (slotIndex > 0) ? (slotIndex % 10) : 0;
		for (int i = 0; i < itemSize.width; i++) {
			if (xx >= 10 || grid[xx + yy] != 0) {
				return false;
			}
			xx++;
		}
		yy += 10;
	}

	if (persistItem) {
		int &numInv = player._pNumInvTab[tabIndex];
		// Same capacity guard as the paste path: the grid said there was room, but the LIST is the
		// thing being written, and a poisoned sidecar can disagree with its own grid.
		if (numInv >= InventoryGridCells)
			return false;
		player.InvTabList[tabIndex][numInv] = item;
		numInv++;

		const int pitch = InventorySizeInSlots.width;
		for (int y = 0; y < itemSize.height; y++) {
			int rowGridIndex = slotIndex + pitch * y;
			for (int x = 0; x < itemSize.width; x++) {
				grid[rowGridIndex + x] = static_cast<int8_t>((x == 0 && y == itemSize.height - 1) ? numInv : -numInv);
			}
		}
	}

	return true;
}

/**
 * @brief Oracool Tabbed Inventory: tries every extra tab (2-10) in order for an empty area big
 * enough for item, used as a fallback once AutoPlaceItemInInventory (tab 1 only) has already
 * failed - covers store purchases, ground/auto pickup, and Stash withdrawal alike, since they
 * all ultimately funnel through AutoPlaceItemInInventory. Deliberately does not touch
 * ActiveInventoryTab; whichever page the player is looking at stays displayed regardless of
 * which tab the item actually lands in. Gold and quest items are never auto-placed into an
 * extra tab (same restriction as manually pasting one there) - a quest item landing here
 * automatically, just because tab 1 happened to be full at the time, would put it somewhere
 * the existing quest-scanning code (which only ever looks at InvList) could never find it.
 */
bool AutoPlaceItemInExtraTabs(Player &player, const Item &item, bool persistItem)
{
	if (!CanItemEnterExtraTab(item))
		return false;

	Size itemSize = GetInventorySize(item);
	for (int tabIndex = 0; tabIndex < Player::NumExtraInventoryTabs; tabIndex++) {
		for (int y = 0; y <= InventorySizeInSlots.height - itemSize.height; y++) {
			for (int x = 0; x <= InventorySizeInSlots.width - itemSize.width; x++) {
				if (AutoPlaceItemInExtraTabSlot(player, tabIndex, y * InventorySizeInSlots.width + x, item, persistItem))
					return true;
			}
		}
	}
	return false;
}

int64_t RoomForGold()
{
	// int64_t, and not for a distant edge case: MaxGold is 100,000,000 and the grid has 70 cells,
	// so an EMPTY backpack is 7,000,000,000 - more than three times what a signed 32-bit int can
	// hold. The accumulator overflowed on the commonest state there is, which is undefined
	// behaviour rather than a large number (external audit, 2026-08-25).
	int64_t amount = 0;
	for (int8_t &itemIndex : MyPlayer->InvGrid) {
		if (itemIndex < 0) {
			continue;
		}
		if (itemIndex == 0) {
			amount += MaxGold;
			continue;
		}

		Item &goldItem = MyPlayer->InvList[itemIndex - 1];
		if (goldItem._itype != ItemType::Gold || goldItem._ivalue == MaxGold) {
			continue;
		}

		amount += MaxGold - goldItem._ivalue;
	}

	return amount;
}

int AddGoldToInventory(Player &player, int value)
{
	// Top off existing piles
	for (int i = 0; i < player._pNumInv && value > 0; i++) {
		Item &goldItem = player.InvList[i];
		if (goldItem._itype != ItemType::Gold || goldItem._ivalue >= MaxGold) {
			continue;
		}

		if (goldItem._ivalue + value > MaxGold) {
			value -= MaxGold - goldItem._ivalue;
			goldItem._ivalue = MaxGold;
		} else {
			goldItem._ivalue += value;
			value = 0;
		}

		NetSyncInvItem(player, i);
		SetPlrHandGoldCurs(goldItem);
	}

	// Oracool bug fix (2026-08-19): these two loops were written for the vanilla 10x4 backpack -
	// `i = 39..30` for its last row, then `y = 2..0` for the three above. The grid has been 10x7
	// since the inventory rebuild, so cells 40-69 were unreachable: gold could never be
	// auto-placed below the fourth row, no matter how full the rest was.
	//
	// Exactly the stash's "last 6 rows" bug of 2026-08-16, in a different file - a literal that did
	// not grow when its grid did. Derived from InventorySizeInSlots now, so the next resize follows.
	//
	// Vanilla's fill ORDER is preserved deliberately, because it is the sensible one and players
	// read it as intentional: the bottom row first, right to left, then the rest in columns from
	// the bottom up and the right across. Gold collects in the corner furthest from where items
	// land, instead of fragmenting the space they need.
	constexpr int Columns = InventorySizeInSlots.width;
	constexpr int Rows = InventoryGridCells / Columns;
	constexpr int LastRowFirst = (Rows - 1) * Columns;

	// Last row right to left
	for (int i = InventoryGridCells - 1; i >= LastRowFirst && value > 0; i--) {
		value = CreateGoldItemInInventorySlot(player, i, value);
	}

	// Remaining inventory in columns, bottom to top, right to left
	for (int x = Columns - 1; x >= 0 && value > 0; x--) {
		for (int y = Rows - 2; y >= 0 && value > 0; y--) {
			value = CreateGoldItemInInventorySlot(player, Columns * y + x, value);
		}
	}

	return value;
}

bool GoldAutoPlace(Player &player, Item &goldStack)
{
	// Oracool: picked-up gold goes straight to the shared Stash pool instead of the inventory,
	// which otherwise remains as it was for anything this doesn't cover (e.g. legacy gold already
	// sitting in inventory from before this change, or the extremely unlikely case of Stash.gold
	// itself sitting within goldStack._ivalue of the int32 ceiling).
	if (oracool::IsSinglePlayer()) {
		const int depositable = std::min(goldStack._ivalue, std::numeric_limits<int>::max() - Stash.gold);
		if (depositable > 0) {
			Stash.gold += depositable;
			Stash.dirty = true;
			goldStack._ivalue -= depositable;
		}
	}

	if (goldStack._ivalue > 0)
		goldStack._ivalue = AddGoldToInventory(player, goldStack._ivalue);
	SetPlrHandGoldCurs(goldStack);

	player._pGold = CalculateGold(player);

	return goldStack._ivalue == 0;
}

void CheckInvSwap(Player &player, inv_body_loc bLoc)
{
	Item &item = player.InvBody[bLoc];

	if (bLoc == INVLOC_HAND_LEFT && player.GetItemLocation(item) == ILOC_TWOHAND) {
		player.InvBody[INVLOC_HAND_RIGHT].clear();
	} else if (bLoc == INVLOC_HAND_RIGHT && player.GetItemLocation(item) == ILOC_TWOHAND) {
		player.InvBody[INVLOC_HAND_LEFT].clear();
	}

	CalcPlrInv(player, true);
}

void inv_update_rem_item(Player &player, inv_body_loc iv)
{
	player.InvBody[iv].clear();

	CalcPlrInv(player, player._pmode != PM_DEATH);
}

void CheckInvSwap(Player &player, const Item &item, int invGridIndex)
{
	auto itemSize = GetInventorySize(item);

	const int pitch = 10;
	int invListIndex = [&]() -> int {
		for (int y = 0; y < itemSize.height; y++) {
			int rowGridIndex = invGridIndex + pitch * y;
			for (int x = 0; x < itemSize.width; x++) {
				int gridIndex = rowGridIndex + x;
				if (player.InvGrid[gridIndex] != 0)
					return abs(player.InvGrid[gridIndex]);
			}
		}
		player._pNumInv++;
		return player._pNumInv;
	}();

	if (invListIndex < player._pNumInv) {
		for (auto &itemIndex : player.InvGrid) {
			if (itemIndex == invListIndex)
				itemIndex = 0;
			if (itemIndex == -invListIndex)
				itemIndex = 0;
		}
	}

	player.InvList[invListIndex - 1] = item;

	for (int y = 0; y < itemSize.height; y++) {
		int rowGridIndex = invGridIndex + pitch * y;
		for (int x = 0; x < itemSize.width; x++) {
			if (x == 0 && y == itemSize.height - 1)
				player.InvGrid[rowGridIndex + x] = invListIndex;
			else
				player.InvGrid[rowGridIndex + x] = -invListIndex;
		}
	}

	CalcPlrInv(player, true);
}

void CheckInvRemove(Player &player, int invGridIndex)
{
	int invListIndex = abs(player.InvGrid[invGridIndex]) - 1;

	if (invListIndex >= 0) {
		player.RemoveInvItem(invListIndex);
	}
}

void TransferItemToStash(Player &player, int location)
{
	if (location == -1) {
		return;
	}

	Item &item = GetInventoryItem(player, location);
	if (!AutoPlaceItemInStash(player, item, true)) {
		player.SaySpecific(HeroSpeech::WhereWouldIPutThis);
		return;
	}

	PlaySFX(ItemInvSnds[GetItemDropAnimIndexFor(item)]);

	if (location < INVITEM_INV_FIRST) {
		RemoveEquipment(player, static_cast<inv_body_loc>(location), false);
		CalcPlrInv(player, true);
	} else if (location <= INVITEM_INV_LAST)
		player.RemoveInvItem(location - INVITEM_INV_FIRST);
	else
		player.RemoveSpdBarItem(location - INVITEM_BELT_FIRST);

	if (&player == MyPlayer)
		oracool::ScheduleAutoSaveForStashChange();
}

/**
 * @brief Ctrl+click with Levski's Roar open: moves the hovered backpack item into its grid.
 *
 * The mirror of TryTransferHoveredActiveTabItemToStash below, and it resolves the hovered cell the
 * same way - through the ACTIVE tab, so the gesture works on all ten pages rather than the first.
 * Unlike the stash version it does not exclude tab 0, because the grid has no legacy index encoding
 * to protect.
 *
 * Lives here rather than in levski_roar.cpp because the resolution needs InvRect and the active-tab
 * helpers, and that file cannot include the inventory layout header - its own GridWidth and
 * CellSize collide with it. Only the placement crosses the boundary.
 */
bool TryMoveHoveredItemToLevskiGrid(Player &player)
{
	const Displacement panelOffset = Point { 0, 0 } - oracool::GetInventoryPanelRect().position;
	int8_t r = SLOTXY_INV_FIRST;
	for (; r <= SLOTXY_INV_LAST; r++) {
		if (InvRect[r].contains(MousePosition + panelOffset))
			break;
	}
	if (r > SLOTXY_INV_LAST)
		return false;

	const int itemId = abs(GetActiveInvGridCell(player, r - SLOTXY_INV_FIRST));
	if (itemId == 0)
		return false;
	const int iv = itemId - 1;
	Item &item = GetActiveInvListItem(player, iv);
	if (item.isEmpty())
		return false;

	// Placed first, removed only if it landed - a refusal must leave the item in the backpack.
	if (!oracool::PlaceItemInLevskiGrid(item)) {
		player.SaySpecific(HeroSpeech::WhereWouldIPutThis);
		return true; // consumed: the gesture was understood and refused, not ignored
	}
	PlaySFX(ItemInvSnds[GetItemDropAnimIndexFor(item)]);
	RemoveActiveInvItem(player, iv);
	return true;
}

bool TryTransferHoveredActiveTabItemToStash(Player &player)
{
	if (ActiveInventoryTab == 0)
		return false;

	const Displacement panelOffset = Point { 0, 0 } - oracool::GetInventoryPanelRect().position;
	int8_t r = SLOTXY_INV_FIRST;
	for (; r <= SLOTXY_INV_LAST; r++) {
		if (InvRect[r].contains(MousePosition + panelOffset))
			break;
	}
	if (r > SLOTXY_INV_LAST)
		return false;

	const int itemId = abs(GetActiveInvGridCell(player, r - SLOTXY_INV_FIRST));
	if (itemId == 0)
		return false;

	const int iv = itemId - 1;
	Item &item = GetActiveInvListItem(player, iv);
	if (item.isEmpty())
		return false;

	if (!AutoPlaceItemInStash(player, item, true)) {
		player.SaySpecific(HeroSpeech::WhereWouldIPutThis);
		return true;
	}

	PlaySFX(ItemInvSnds[GetItemDropAnimIndexFor(item)]);
	RemoveActiveInvItem(player, iv);
	if (&player == MyPlayer)
		oracool::ScheduleAutoSaveForStashChange();
	return true;
}

/**
 * @brief Extra tabs the sort may place INTO - now every one it drains FROM.
 *
 * It used to stop one short, because the tenth tab position was the SORT button and so the last extra
 * tab could not be opened: refilling it would have put items straight back somewhere unreachable.
 * SORT is a footer button now and all ten pages open, so the asymmetry has nothing left to protect
 * against - and keeping it would waste a page's worth of space on every sort.
 */
constexpr int PlaceableExtraTabs = Player::NumExtraInventoryTabs;

void SortInventoryBySellValue(Player &player)
{
	if (!oracool::IsSinglePlayer())
		return;

	struct SortEntry {
		Item item;
		int value;
	};
	std::vector<SortEntry> entries;

	// Gold and quest items are pinned in place (see CanItemEnterExtraTab) - repeatedly remove the
	// first *relocatable* item found rather than walking indices in order, since removal
	// compacts the list by swapping the last item into the vacated slot, which would otherwise
	// let a pinned item silently jump to an index this loop has already passed.
	for (;;) {
		int foundIndex = -1;
		for (int i = 0; i < player._pNumInv; i++) {
			if (CanItemEnterExtraTab(player.InvList[i])) {
				foundIndex = i;
				break;
			}
		}
		if (foundIndex < 0)
			break;
		entries.push_back({ player.InvList[foundIndex], GetItemSellValue(player.InvList[foundIndex]) });
		player.RemoveInvItem(foundIndex);
	}

	for (int tab = 0; tab < Player::NumExtraInventoryTabs; tab++) {
		for (;;) {
			int foundIndex = -1;
			for (int i = 0; i < player._pNumInvTab[tab]; i++) {
				if (CanItemEnterExtraTab(player.InvTabList[tab][i])) {
					foundIndex = i;
					break;
				}
			}
			if (foundIndex < 0)
				break;
			entries.push_back({ player.InvTabList[tab][foundIndex], GetItemSellValue(player.InvTabList[tab][foundIndex]) });
			RemoveExtraTabItem(player, tab, foundIndex);
		}
	}

	std::stable_sort(entries.begin(), entries.end(), [](const SortEntry &a, const SortEntry &b) {
		return a.value > b.value;
	});

	constexpr int InventoryColumnsPerRow = 10;
	constexpr int InventoryRowCount = InventoryGridCells / InventoryColumnsPerRow;

	// Oracool: user request - 1x1 items sort to the bottom row(s) of whichever tab they land in,
	// instead of wherever the plain top-down first-fit scan happens to put them. Placed in their
	// own pass, before the main placement loop below, specifically so they claim the bottom rows
	// first - if the main loop ran first, a larger item's ordinary top-down scan could reach the
	// bottom row before a 1x1 item got a chance to claim it. Still highest-sell-value-first within
	// this group (entries is already sorted), and still tries the backpack before extra tabs, same
	// as the main loop - only the row scan direction (bottom-up instead of top-down) differs.
	std::vector<SortEntry> oneByOneEntries;
	std::vector<SortEntry> otherEntries;
	for (const SortEntry &entry : entries) {
		if (FitsInBeltSlot(entry.item))
			oneByOneEntries.push_back(entry);
		else
			otherEntries.push_back(entry);
	}

	for (const SortEntry &entry : oneByOneEntries) {
		bool placed = false;
		for (int row = InventoryRowCount - 1; row >= 0 && !placed; row--) {
			for (int col = 0; col < InventoryColumnsPerRow && !placed; col++) {
				if (AutoPlaceItemInInventorySlot(player, row * InventoryColumnsPerRow + col, entry.item, true))
					placed = true;
			}
		}
		for (int tab = 0; tab < PlaceableExtraTabs && !placed; tab++) {
			for (int row = InventoryRowCount - 1; row >= 0 && !placed; row--) {
				for (int col = 0; col < InventoryColumnsPerRow && !placed; col++) {
					if (AutoPlaceItemInExtraTabSlot(player, tab, row * InventoryColumnsPerRow + col, entry.item, true))
						placed = true;
				}
			}
		}
		// Every entry came from this same 10-tab space and nothing pinned was removed, so it must
		// fit somewhere - this should never actually trigger.
	}

	// Oracool: second-priority packing heuristic (after sell-value order) - when placing a 2x2
	// item, first try stacking it directly below the most recently placed 2x2 item in the same
	// tab (same column, two rows down), before falling back to the normal first-fit scan. Two 2x2
	// items land in a single dense 2x4 column this way instead of two unrelated scattered gaps.
	// Only tracks one pending "partner" at a time - once a pairing is attempted (successfully or
	// not), tracking resets, so this pairs 2x2 items up rather than chaining an unbounded column.
	int pendingTwoByTwoSlot = -1;
	int pendingTwoByTwoTab = -1; // -1 means the backpack (InvList); >= 0 is an extra tab index.

	for (const SortEntry &entry : otherEntries) {
		const bool isTwoByTwo = GetInventorySize(entry.item) == Size { 2, 2 };
		bool placed = false;

		if (isTwoByTwo && pendingTwoByTwoSlot >= 0) {
			const int stackedSlot = pendingTwoByTwoSlot + 2 * 10;
			placed = (pendingTwoByTwoTab < 0)
			    ? AutoPlaceItemInInventorySlot(player, stackedSlot, entry.item, true)
			    : AutoPlaceItemInExtraTabSlot(player, pendingTwoByTwoTab, stackedSlot, entry.item, true);
			pendingTwoByTwoSlot = -1;
		}

		int placedSlot = -1;
		int placedTab = -1;
		for (int slot = 0; slot < InventoryGridCells && !placed; slot++) {
			if (AutoPlaceItemInInventorySlot(player, slot, entry.item, true)) {
				placed = true;
				placedSlot = slot;
			}
		}
		for (int tab = 0; tab < PlaceableExtraTabs && !placed; tab++) {
			for (int slot = 0; slot < InventoryGridCells && !placed; slot++) {
				if (AutoPlaceItemInExtraTabSlot(player, tab, slot, entry.item, true)) {
					placed = true;
					placedSlot = slot;
					placedTab = tab;
				}
			}
		}
		// Every entry came from this same 10-tab space and nothing pinned was removed, so it must
		// fit somewhere - this should never actually trigger.

		if (isTwoByTwo && placedSlot >= 0) {
			pendingTwoByTwoSlot = placedSlot;
			pendingTwoByTwoTab = placedTab;
		}
	}

	player.CalcScrolls();
	CalcPlrInv(player, true);
}

bool CheckInventorySortButtonClick(Point cursorPosition)
{
	if (!oracool::IsSinglePlayer())
		return false;

	// Oracool V1: a text button in the footer under the grid (user request). It was the last tab
	// position labelled "S" for a while, which cost the tenth storage page its only way to be opened.
	const Rectangle sortRect = oracool::GetSortButtonRect();
	const Rectangle button { oracool::GetInventoryPanelRect().position
		    + Displacement { sortRect.position.x, sortRect.position.y },
		sortRect.size };
	if (!button.contains(cursorPosition))
		return false;

	// Oracool: the sort itself still runs immediately on mouse-down (unchanged) - this flag is
	// purely so the button visibly changes color for the moment the mouse stays pressed, cleared
	// in diablo.cpp's LeftMouseUp regardless of where the mouse is by then.
	inventorySortButtonDown = true;
	InventorySortFlashUntil = SDL_GetTicks() + InventorySortFlashMs;
	SortInventoryBySellValue(*MyPlayer);
	// Oracool: user request - same sound as the Stash's Sort button (shield-into-slot sound).
	PlaySFX(IS_ISHIEL);
	return true;
}

/**
 * @brief Oracool Tabbed Inventory: hit-tests the 10 tab buttons drawn by DrawInventoryTabs.
 *
 * Uses each tab's base (non-enlarged) rectangle regardless of selection state, so the clickable
 * region stays fixed and predictable even if the selected tab is ever drawn larger than the rest.
 * Switching tabs works the same whether or not an item is currently held on the cursor - that's
 * how you carry an item from one tab's view into another's.
 */
bool CheckInventoryTabClick(Point cursorPosition)
{
	if (!TabbedInventoryEnabled())
		return false;

	const Displacement panelOffset = Point { 0, 0 } - oracool::GetInventoryPanelRect().position;

	// All ten, now that SORT has left the row for the footer. It used to stop one short, because the
	// last position was the SORT button and claiming it here set ActiveInventoryTab to a tab that was
	// never drawn as selected AND swallowed the click before CheckInventorySortButtonClick could see
	// it. Both problems are gone with the button; the tenth page is simply a page.
	for (int tab = 0; tab < oracool::TabCount; tab++) {
		if (oracool::GetTabRect(tab).contains(cursorPosition + panelOffset)) {
			if (ActiveInventoryTab != tab)
				oracool::PlayUiMoveSound(); // the tab you are on absorbs the click silently, as the shop's does
			ActiveInventoryTab = tab;
			return true;
		}
	}
	return false;
}

void CheckInvItem(bool isShiftHeld, bool isCtrlHeld)
{
	if (IsInspectingPlayer())
		return;
	if (CheckInventoryTabClick(MousePosition))
		return;
	if (MyPlayer->HoldItem.isEmpty() && CheckInventorySortButtonClick(MousePosition))
		return;
	if (!MyPlayer->HoldItem.isEmpty()) {
		CheckInvPaste(*MyPlayer, MousePosition);
	} else if (IsStashOpen && isCtrlHeld) {
		if (!TryTransferHoveredActiveTabItemToStash(*MyPlayer))
			TransferItemToStash(*MyPlayer, pcursinvitem);
	} else if (oracool::IsLevskiRoarOpen() && isCtrlHeld
	    && TryMoveHoveredItemToLevskiGrid(*MyPlayer)) {
		// Ctrl means "send it to the other container" (user, 2026-08-28: "ctrl+click to send items
		// to levskis grid and back to my inv grid, not drop them on the ground"). With the stash
		// open it already did; with Levski's Roar open it fell through to CheckInvCut, whose ctrl
		// arm is vanilla's DROP IT ON THE FLOOR - the one thing the gesture must not do beside an
		// open crafting window.
		//
		// Tested after the stash so an open stash still wins, which is the order the player set by
		// opening it.
	} else {
		CheckInvCut(*MyPlayer, MousePosition, isShiftHeld, isCtrlHeld);
	}
}

void CheckInvScrn(bool isShiftHeld, bool isCtrlHeld)
{
	// Oracool: HUD art pass - the belt's item region is the plate art's middle HUD rect now.
	if (oracool::GetMiddleHudRect().contains(MousePosition)) {
		CheckInvItem(isShiftHeld, isCtrlHeld);
	}
}

void InvGetItem(Player &player, int ii)
{
	auto &item = Items[ii];
	// Oracool: user request - gold pickup persists instantly too, matching Diablo 3's
	// always-saved progress; previously excluded to avoid spamming saves while raking in gold,
	// but the delay setting (now defaulting to 0) already debounces rapid pickups if desired.
	const bool scheduleAutoSave = &player == MyPlayer;
	if (DropGoldFlag) {
		CloseGoldDrop();
	}

	if (dItem[item.position.x][item.position.y] == 0)
		return;

	item._iCreateInfo &= ~CF_PREGEN;
	CheckQuestItem(player, item);
	item.updateRequiredStatsCacheForPlayer(player);
	// Oracool: self-heal a Rare/Buffed Unique/Primal item generated before the v0.3.42 affix-value
	// fix the moment it's picked up, matching the on-load repair in loadsave.cpp's
	// LoadAndValidateItemData - ground items that were never saved (dropped and picked back up in
	// the same session) never go through that path, so pickup needs its own call too.
	RepairOracoolAffixesIfCorrupted(item);
	// Phase 0.9: what actually enters the player's hands, by tier - the economy's tuning signal.
	if (&player == MyPlayer)
		oracool::TelemetryRecordPickup(item);

	// Oracool: Charms of Salvaging. An armed charm converts the drop to material before it ever
	// reaches the pack. Placed AFTER telemetry on purpose - the drop still happened and the economy
	// signal is about what the floor produced, not what the player chose to keep. Both pickup paths
	// need this: the QoL auto-pickup radius goes through AutoGetItem, but an item on the exact tile
	// the player stands on always comes through here instead.
	if (&player == MyPlayer && oracool::TrySalvageOnPickup(player, item)) {
		// The item became material, so it sounds like salvage, not like a pickup (2026-09-11); the
		// pickup sound is the fallback when salvage.wav is missing, and keeps its own option.
		if (!oracool::PlayUiEventSound(oracool::UiEventSound::Salvage) && *sgOptions.Audio.itemPickupSound)
			PlaySFX(IS_IGRAB);
		CleanupItems(ii);
		pcursitem = -1;
		if (scheduleAutoSave)
			oracool::ScheduleAutoSaveForItemPickup();
		return;
	}

	// Oracool bug fix: user report - elixirs (and, in principle, any stackable consumable) didn't
	// stack when picked up. Root cause: the QoL "Auto Pickup Range" loop (qol/autopickup.cpp) only
	// covers tiles 1+ away, so an item on the exact tile the player is standing on always comes
	// through this vanilla function instead - which never attempted a merge at all, unlike
	// AutoGetItem (used for the surrounding radius), which already merges via
	// AutoPlaceItemInBelt/AutoPlaceItemInInventory. Potions are dropped and picked up often enough
	// that most of them get grabbed from a step or two away and never hit this gap; a rarer item a
	// player deliberately walks onto (like an elixir) hits it far more often, making the gap look
	// elixir-specific even though it equally affects every stackable consumable.
	// PROBE, then commit only if every unit finds a home - belt first, backpack for the rest.
	//
	// All-or-nothing on purpose, and this is the trap: when `merged` comes out false the else branch
	// below puts the WHOLE original item into the player's hand. A partial commit would therefore
	// have moved some units into the belt AND handed the player the full stack, duplicating them.
	// The old expression was `MergeBelt(...) || MergeInv(...)`, which stopped at the first function
	// to move anything - so walking onto forty gems with a part-full belt slot moved what fitted,
	// reported success, and destroyed the rest.
	auto mergeAll = [&player](const Item &source, bool persist) -> int {
		Item rest = source;
		int taken = MergeStackableItemIntoBelt(player, rest, persist);
		if (taken > 0 && taken < rest.stackCount()) {
			rest.setStackCount(rest.stackCount() - taken);
			taken += MergeStackableItemIntoInventory(player, rest, persist);
		} else if (taken == 0) {
			taken = MergeStackableItemIntoInventory(player, rest, persist);
		}
		return taken;
	};

	bool merged = false;
	if (item._itype != ItemType::Gold && oracool::IsSinglePlayer() && item.isStackableConsumable()
	    && mergeAll(item, /*persist=*/false) >= item.stackCount()) {
		mergeAll(item, /*persist=*/true);
		merged = true;
	}

	if (merged) {
		if (MyPlayer == &player && *sgOptions.Audio.itemPickupSound) {
			PlaySFX(IS_IGRAB);
		}
	} else if (item._itype == ItemType::Gold && GoldAutoPlace(player, item)) {
		if (MyPlayer == &player) {
			// Non-gold items (or gold when you have a full inventory) go to the hand then provide audible feedback on
			//  paste. To give the same feedback for auto-placed gold we play the sound effect now.
			PlaySFX(IS_GOLD);
		}
	} else {
		// The item needs to go into the players hand
		if (MyPlayer == &player && !player.HoldItem.isEmpty()) {
			// drop whatever the player is currently holding
			NetSendCmdPItem(true, CMD_SYNCPUTITEM, player.position.tile, player.HoldItem);
		}

		// need to copy here instead of move so CleanupItems still has access to the position
		player.HoldItem = item;
		NewCursor(player.HoldItem);
	}

	// This potentially moves items in memory so must be done after we've made a copy
	CleanupItems(ii);
	pcursitem = -1;
	if (scheduleAutoSave)
		oracool::ScheduleAutoSaveForItemPickup();
}

std::optional<Point> FindAdjacentPositionForItem(Point origin, Direction facing)
{
	if (ActiveItemCount >= MAXITEMS)
		return {};

	if (CanPut(origin + facing))
		return origin + facing;

	if (CanPut(origin + Left(facing)))
		return origin + Left(facing);

	if (CanPut(origin + Right(facing)))
		return origin + Right(facing);

	if (CanPut(origin + Left(Left(facing))))
		return origin + Left(Left(facing));

	if (CanPut(origin + Right(Right(facing))))
		return origin + Right(Right(facing));

	if (CanPut(origin + Left(Left(Left(facing)))))
		return origin + Left(Left(Left(facing)));

	if (CanPut(origin + Right(Right(Right(facing)))))
		return origin + Right(Right(Right(facing)));

	if (CanPut(origin + Opposite(facing)))
		return origin + Opposite(facing);

	if (CanPut(origin))
		return origin;

	return {};
}

void AutoGetItem(Player &player, Item *itemPointer, int ii)
{
	Item &item = *itemPointer;

	if (DropGoldFlag) {
		CloseGoldDrop();
	}

	if (dItem[item.position.x][item.position.y] == 0)
		return;

	item._iCreateInfo &= ~CF_PREGEN;
	CheckQuestItem(player, item);
	item.updateRequiredStatsCacheForPlayer(player);
	// Oracool: see the matching comment in InvGetItem - self-heals a pre-v0.3.42 tiered item on
	// the auto-pickup path too.
	RepairOracoolAffixesIfCorrupted(item);
	// Phase 0.9: the auto-pickup half of the same signal InvGetItem records.
	if (&player == MyPlayer)
		oracool::TelemetryRecordPickup(item);

	// Oracool: the auto-pickup half of the Charm of Salvaging hook - see InvGetItem's copy. This is
	// the path that matters most in practice, because it is the one the QoL pickup radius uses.
	if (&player == MyPlayer && oracool::TrySalvageOnPickup(player, item)) {
		// The item became material, so it sounds like salvage, not like a pickup (2026-09-11); the
		// pickup sound is the fallback when salvage.wav is missing, and keeps its own option.
		if (!oracool::PlayUiEventSound(oracool::UiEventSound::Salvage) && *sgOptions.Audio.itemPickupSound)
			PlaySFX(IS_IGRAB);
		CleanupItems(ii);
		oracool::ScheduleAutoSaveForItemPickup();
		return;
	}

	bool done;
	bool autoEquipped = false;

	if (item._itype == ItemType::Gold) {
		done = GoldAutoPlace(player, item);
		if (!done) {
			SetPlrHandGoldCurs(item);
		}
	} else {
		done = AutoEquipEnabled(player, item) && AutoEquip(player, item);
		if (done) {
			autoEquipped = true;
		}

		// Only potions go to the belt on their own; everything else goes to the inventory, and never to
		// the belt (user, 2026-09-14: "dont auto put stuff in belt unless they are potions").
		if (!done && item.isPotion()) {
			done = AutoPlaceItemInBelt(player, item, true);
		}
		if (!done) {
			done = AutoPlaceItemInInventory(player, item, true);
		}
	}

	if (done) {
		if (!autoEquipped && *sgOptions.Audio.itemPickupSound && &player == MyPlayer) {
			PlaySFX(IS_IGRAB);
		}

		// Oracool: user request - gold pickup persists instantly too, matching Diablo 3's
		// always-saved progress; previously excluded to avoid spamming saves while raking in gold,
		// but the delay setting (now defaulting to 0) already debounces rapid pickups if desired.
		const bool scheduleAutoSave = &player == MyPlayer;
		CleanupItems(ii);
		if (scheduleAutoSave)
			oracool::ScheduleAutoSaveForItemPickup();
		return;
	}

	if (&player == MyPlayer) {
		player.Say(HeroSpeech::ICantCarryAnymore);
	}
	RespawnItem(item, true);
	NetSendCmdPItem(true, CMD_SPAWNITEM, item.position, item);
}

int FindGetItem(uint32_t iseed, _item_indexes idx, uint16_t createInfo)
{
	for (uint8_t i = 0; i < ActiveItemCount; i++) {
		auto &item = Items[ActiveItems[i]];
		if (item.keyAttributesMatch(iseed, idx, createInfo)) {
			return i;
		}
	}

	return -1;
}

void SyncGetItem(Point position, uint32_t iseed, _item_indexes idx, uint16_t ci)
{
	// Check what the local client has at the target position
	int ii = dItem[position.x][position.y] - 1;

	if (ii >= 0 && ii < MAXITEMS) {
		// If there was an item there, check that it's the same item as the remote player has
		if (!Items[ii].keyAttributesMatch(iseed, idx, ci)) {
			// Key attributes don't match so we must've desynced, ignore this index and try find a matching item via lookup
			ii = -1;
		}
	}

	if (ii == -1) {
		// Either there's no item at the expected position or it doesn't match what is being picked up, so look for an item that matches the key attributes
		ii = FindGetItem(iseed, idx, ci);

		if (ii != -1) {
			// Translate to Items index for CleanupItems, FindGetItem returns an ActiveItems index
			ii = ActiveItems[ii];
		}
	}

	if (ii == -1) {
		// Still can't find the expected item, assume it was collected earlier and this caused the desync
		return;
	}

	CleanupItems(ii);
}

bool CanPut(Point position)
{
	if (!InDungeonBounds(position)) {
		return false;
	}

	if (IsTileSolid(position)) {
		return false;
	}

	if (dItem[position.x][position.y] != 0) {
		return false;
	}

	if (leveltype == DTYPE_TOWN) {
		if (dMonster[position.x][position.y] != 0) {
			return false;
		}
		if (dMonster[position.x + 1][position.y + 1] != 0) {
			return false;
		}
	}

	if (IsItemBlockingObjectAtPosition(position)) {
		return false;
	}

	return true;
}

int ClampDurability(const Item &item, int durability)
{
	if (item._iMaxDur == 0)
		return 0;

	return clamp<int>(durability, 1, item._iMaxDur);
}

int16_t ClampToHit(const Item &item, int16_t toHit)
{
	if (toHit < item._iPLToHit || toHit > 51)
		return item._iPLToHit;

	return toHit;
}

uint8_t ClampMaxDam(const Item &item, uint8_t maxDam)
{
	if (maxDam < item._iMaxDam || maxDam - item._iMinDam > 30)
		return item._iMaxDam;

	return maxDam;
}

int SyncDropItem(Point position, _item_indexes idx, uint16_t icreateinfo, int iseed, int id, int dur, int mdur, int ch, int mch, int ivalue, uint32_t ibuff, int toHit, int maxDam)
{
	if (ActiveItemCount >= MAXITEMS)
		return -1;

	Item item;

	RecreateItem(*MyPlayer, item, idx, icreateinfo, iseed, ivalue, (ibuff & CF_HELLFIRE) != 0);
	if (id != 0)
		item._iIdentified = true;
	item._iMaxDur = mdur;
	item._iDurability = ClampDurability(item, dur);
	item._iMaxCharges = clamp<int>(mch, 0, item._iMaxCharges);
	item._iCharges = clamp<int>(ch, 0, item._iMaxCharges);
	if (gbIsHellfire) {
		item._iPLToHit = ClampToHit(item, toHit);
		item._iMaxDam = ClampMaxDam(item, maxDam);
	}
	item.dwBuff = ibuff;

	return PlaceItemInWorld(std::move(item), position);
}

int SyncDropEar(Point position, uint16_t icreateinfo, uint32_t iseed, uint8_t cursval, string_view heroname)
{
	if (ActiveItemCount >= MAXITEMS)
		return -1;

	Item item;
	RecreateEar(item, icreateinfo, iseed, cursval, heroname);

	return PlaceItemInWorld(std::move(item), position);
}

int8_t CheckInvHLight()
{
	ActiveTabItemHovered = false;
	pcursinvtabidx = -1;
	pcursinvtabitem = -1;
	// The structural half of the fix above: this IS the top of the inventory hover pass, the way
	// CheckPanelInfo is the top of the main panel's - and CheckPanelInfo clears here for exactly
	// this reason. CheckCursMove routes an inventory hover straight here and returns, so without
	// this, nothing on that path ever drops last frame's per-line colours.
	ClearPanelStrings();

	int8_t r = 0;
	for (; r < NUM_XY_SLOTS; r++) {
		// Oracool: HUD art pass - belt cells hit-test against the plate art's cell rects
		// (hud_layout), not InvRect. Slot 0 is Menu, 5 is Town Portal, 6/7 are hidden.
		if (r >= SLOTXY_BELT_FIRST) {
			const int beltIndex = r - SLOTXY_BELT_FIRST;
			if (!oracool::IsRealBeltItemSlot(beltIndex))
				continue;
			if (oracool::GetBeltSlotRect(beltIndex).contains(MousePosition))
				break;
			continue;
		}

		if (InvRect[r].contains(MousePosition - Displacement(oracool::GetInventoryPanelRect().position.x, oracool::GetInventoryPanelRect().position.y))) {
			break;
		}
	}

	if (r >= NUM_XY_SLOTS)
		return -1;

	int8_t rv = -1;
	InfoColor = UiFlags::ColorWhite;
	Item *pi = nullptr;
	Player &myPlayer = *InspectPlayer;

	if (r == SLOTXY_HEAD) {
		rv = INVLOC_HEAD;
		pi = &myPlayer.InvBody[rv];
	} else if (r == SLOTXY_RING_LEFT) {
		rv = INVLOC_RING_LEFT;
		pi = &myPlayer.InvBody[rv];
	} else if (r == SLOTXY_RING_RIGHT) {
		rv = INVLOC_RING_RIGHT;
		pi = &myPlayer.InvBody[rv];
	} else if (r == SLOTXY_AMULET) {
		rv = INVLOC_AMULET;
		pi = &myPlayer.InvBody[rv];
	} else if (r == SLOTXY_HAND_LEFT) {
		rv = INVLOC_HAND_LEFT;
		pi = &myPlayer.InvBody[rv];
	} else if (r == SLOTXY_HAND_RIGHT) {
		pi = &myPlayer.InvBody[INVLOC_HAND_LEFT];
		if (pi->isEmpty() || myPlayer.GetItemLocation(*pi) != ILOC_TWOHAND) {
			rv = INVLOC_HAND_RIGHT;
			pi = &myPlayer.InvBody[rv];
		} else {
			rv = INVLOC_HAND_LEFT;
		}
	} else if (r == SLOTXY_CHEST) {
		rv = INVLOC_CHEST;
		pi = &myPlayer.InvBody[rv];
	} else if (r >= SLOTXY_SHOULDERS && r <= SLOTXY_BOOTS) {
		// Oracool bug fix: user report - "I tried putting boots in the boots slot - crash!" This
		// chain had no branch for the six new slots, so pi stayed nullptr and the isEmpty() check
		// below dereferenced it. The paste itself succeeded; the crash fired on the first
		// bare-handed hover over any new slot - which the frame right after dropping an item into
		// one always is, since the paste empties the cursor while it still sits on the slot.
		// SLOTXY_* and inv_body_loc are parallel across the equipped range, so r IS the location.
		rv = r;
		pi = &myPlayer.InvBody[r];
	} else if (r >= SLOTXY_INV_FIRST && r <= SLOTXY_INV_LAST) {
		int8_t itemId = abs(GetActiveInvGridCell(myPlayer, r - SLOTXY_INV_FIRST));
		if (itemId == 0)
			return -1;
		int ii = itemId - 1;
		pi = &GetActiveInvListItem(myPlayer, ii);
		// Oracool Tabbed Inventory: only tab 1 (the real InvList) has a pcursinvitem encoding
		// that legacy drag/drop code understands (it assumes tab-1 indices). An extra tab's item
		// still gets its full hover tooltip (InfoString/PrintItemDetails below, via pi), and the
		// single-shot cursor-target actions (Identify/Repair/Recharge/Oil, see TryIconCurs) can
		// still reach it via pcursinvtabidx/pcursinvtabitem - it just isn't a drag/drop target.
		// ActiveTabItemHovered tells DrawInfoBox not to wipe that tooltip the way it normally
		// would for pcursinvitem == -1 (which otherwise also means "hovering nothing at all").
		if (ActiveInventoryTab == 0) {
			rv = ii + INVITEM_INV_FIRST;
		} else {
			rv = -1;
			ActiveTabItemHovered = true;
			pcursinvtabidx = static_cast<int8_t>(ActiveInventoryTab - 1);
			pcursinvtabitem = static_cast<int8_t>(ii);
		}
	} else if (r >= SLOTXY_BELT_FIRST) {
		r -= SLOTXY_BELT_FIRST;
		RedrawComponent(PanelDrawComponent::Belt);
		pi = &myPlayer.SpdList[r];
		if (pi->isEmpty())
			return -1;
		rv = r + INVITEM_BELT_FIRST;
	}

	// The nullptr arm is load-bearing, not paranoia: pi starts null and the chain above must
	// explicitly claim every slot. A slot nobody claims used to dereference null right here.
	if (pi == nullptr || pi->isEmpty())
		return -1;

	if (pi->_itype == ItemType::Gold) {
		int nGold = pi->_ivalue;
		// Through SetPanelString, like the item arm below it. A bare assignment leaves
		// InfoStringLineColors holding the PREVIOUS hover's per-line colours while the text is
		// replaced by one line, and DrawCursorTooltip asserts on exactly that mismatch (user
		// screenshot, 2026-08-19: hovering gold after any coloured item popped the assert dialog,
		// which is also why gold could not be Ctrl+Clicked to the stash - the click never got past
		// the dialog). The inventory hover pass has no ClearPanelStrings of its own; CheckPanelInfo
		// owns that, and it only runs for the main panel.
		SetPanelString(fmt::format(fmt::runtime(ngettext("{:s} gold piece", "{:s} gold pieces", nGold)), FormatInteger(nGold)), UiFlags::ColorWhite);
	} else {
		// Through SetPanelString, so the name's tier colour is recorded as line 0's colour. A bare
		// assignment leaves the per-line colour list one entry short of the block PrintItemDetails
		// then builds, and the tooltip's size check correctly refuses the whole thing and falls
		// back to one colour - which is exactly how this went unnoticed the first time.
		SetPanelString(pi->getName(), pi->getTextColor());
		if (pi->_iIdentified) {
			PrintItemDetails(*pi);
		} else {
			PrintItemDur(*pi);
		}
		// What the vendor would pay for it, while a vendor is open to ask (user, 2026-08-27: "When
		// i am in a vendor shop hover pop-ups to show sell price of items i hover over").
		//
		// Appended to the hover the player is already reading rather than given a popup of its own:
		// the question "what is this worth" is only ever asked about an item they are already
		// looking at.
		//
		// Silent when the vendor will not take it. Adria does not buy armour and Griswold does not
		// buy potions, and a price beside an item they will refuse is worse than no price - it is a
		// promise the shop then breaks.
		if (const int offer = ShopSellOfferFor(*pi); offer > 0) {
			AddPanelString(fmt::format(fmt::runtime(_("Sells for {:s} gold")), FormatInteger(offer)),
			    UiFlags::ColorWhitegold);
		}
	}

	return rv;
}

void DecrementOrRemoveInvItem(Player &player, int invIndex, int tabIdx)
{
	Item &item = tabIdx >= 0 ? player.InvTabList[tabIdx][invIndex] : player.InvList[invIndex];
	if (item.isStackableConsumable() && item.stackCount() > 1) {
		item.setStackCount(item.stackCount() - 1);
		if (tabIdx < 0)
			NetSyncInvItem(player, invIndex); // never network-synced for an extra tab - single-player only
		return;
	}

	if (tabIdx >= 0) {
		RemoveExtraTabItem(player, tabIdx, invIndex);
	} else {
		player.RemoveInvItem(invIndex);
	}
}

void DecrementOrRemoveSpdBarItem(Player &player, int spdIndex)
{
	Item &item = player.SpdList[spdIndex];
	if (item.isStackableConsumable() && item.stackCount() > 1) {
		item.setStackCount(item.stackCount() - 1);
		player.CalcScrolls();
		RedrawComponent(PanelDrawComponent::Belt);
		if (&player == MyPlayer) {
			NetSendCmdChBeltItem(false, spdIndex);
		}
		return;
	}

	const bool tryRefill = oracool::IsSinglePlayer() && item.isStackableConsumable();
	const _item_indexes idx = item.IDidx;
	const bool identified = item._iIdentified;

	player.RemoveSpdBarItem(spdIndex);

	if (tryRefill)
		RefillBeltSlotFromInventory(player, spdIndex, idx, identified);
}

void RefillBeltSlotFromInventory(Player &player, int spdIndex, _item_indexes idx, bool identified)
{
	Item &beltItem = player.SpdList[spdIndex];
	bool filledSlot = false;

	// Shared scan/move logic against one source array (the real backpack, or one Tabbed
	// Inventory extra tab). removeAt/syncPartial are the only things that differ between
	// sources: removeAt swap-compacts whichever array actually backs this source, and
	// syncPartial only means anything for the real backpack (InvGrid-based network sync
	// has no equivalent for the extra tabs, which are single-player-only).
	auto scanSource = [&](Item *list, int &numInv, auto removeAt, auto syncPartial) {
		for (int i = 0; i < numInv;) {
			Item &sourceItem = list[i];
			if (!sourceItem.isStackableConsumable() || sourceItem.IDidx != idx || sourceItem._iIdentified != identified) {
				i++;
				continue;
			}

			int capacity = filledSlot ? Item::MaxStackCount - beltItem.stackCount() : Item::MaxStackCount;
			if (capacity <= 0)
				return;

			int moved = std::min(capacity, sourceItem.stackCount());
			if (!filledSlot) {
				beltItem = sourceItem;
				beltItem.setStackCount(moved);
				filledSlot = true;
			} else {
				beltItem.setStackCount(beltItem.stackCount() + moved);
			}

			int remaining = sourceItem.stackCount() - moved;
			if (remaining > 0) {
				sourceItem.setStackCount(remaining);
				syncPartial(i);
				i++;
			} else {
				// removeAt swap-compacts the array, so slot i now holds a
				// different item (or none, if this was the last one) - recheck it.
				removeAt(i);
			}

			if (beltItem.stackCount() >= Item::MaxStackCount)
				return;
		}
	};

	scanSource(
	    player.InvList, player._pNumInv,
	    [&](int i) { player.RemoveInvItem(i); },
	    [&](int i) { NetSyncInvItem(player, i); });

	if ((!filledSlot || beltItem.stackCount() < Item::MaxStackCount) && TabbedInventoryEnabled()) {
		for (int tabIndex = 0; tabIndex < Player::NumExtraInventoryTabs; tabIndex++) {
			scanSource(
			    player.InvTabList[tabIndex].data(), player._pNumInvTab[tabIndex],
			    [&](int i) { RemoveExtraTabItem(player, tabIndex, i); },
			    [](int) {});
			if (filledSlot && beltItem.stackCount() >= Item::MaxStackCount)
				break;
		}
	}

	if (!filledSlot)
		return;

	player.CalcScrolls();
	RedrawComponent(PanelDrawComponent::Belt);
	if (&player == MyPlayer) {
		NetSendCmdChBeltItem(false, spdIndex);
	}
}

void ConsumeScroll(Player &player)
{
	const SpellID spellId = player.executedSpell.spellId;

	// Custom Engineering (Rogue, 2026-09-14): half the time the rune is set without being used up.
	if (IsAnyOf(spellId, SpellID::RuneOfFire, SpellID::RuneOfLight, SpellID::RuneOfNova, SpellID::RuneOfImmolation, SpellID::RuneOfStone)
	    && oracool::PassiveSparesRune(player))
		return;

	const auto isCurrentSpell = [spellId](const Item &item) {
		return item.isScrollOf(spellId) || item.isRuneOf(spellId);
	};

	// Try to remove the scroll from selected inventory slot
	const int8_t itemSlot = player.executedSpell.spellFrom;
	if (itemSlot >= INVITEM_INV_FIRST && itemSlot <= INVITEM_INV_LAST) {
		const int itemIndex = itemSlot - INVITEM_INV_FIRST;
		const Item *item = &player.InvList[itemIndex];
		if (!item->isEmpty() && isCurrentSpell(*item)) {
			DecrementOrRemoveInvItem(player, itemIndex);
			return;
		}
	} else if (itemSlot >= INVITEM_BELT_FIRST && itemSlot <= INVITEM_BELT_LAST) {
		const int itemIndex = itemSlot - INVITEM_BELT_FIRST;
		const Item *item = &player.SpdList[itemIndex];
		if (!item->isEmpty() && isCurrentSpell(*item)) {
			DecrementOrRemoveSpdBarItem(player, itemIndex);
			return;
		}
	} else if (itemSlot != 0) {
		app_fatal(StrCat("ConsumeScroll: Invalid item index ", itemSlot));
	}

	// Didn't find it at the selected slot, take the first one we find
	// This path is always used when the scroll is consumed via spell selection
	DecrementOrRemoveInventoryOrBeltItem(player, isCurrentSpell);
}

bool CanUseScroll(Player &player, SpellID spell)
{
	if (leveltype == DTYPE_TOWN && !GetSpellData(spell).isAllowedInTown())
		return false;

	return HasInventoryOrBeltItem(player, [spell](const Item &item) {
		return item.isScrollOf(spell) || item.isRuneOf(spell);
	});
}

void ConsumeStaffCharge(Player &player)
{
	auto &staff = player.InvBody[INVLOC_HAND_LEFT];

	if (!CanUseStaff(staff, player.executedSpell.spellId))
		return;

	staff._iCharges--;
	CalcPlrStaff(player);
}

bool CanUseStaff(Player &player, SpellID spellId)
{
	return CanUseStaff(player.InvBody[INVLOC_HAND_LEFT], spellId);
}

Item &GetInventoryItem(Player &player, int location)
{
	if (location < INVITEM_INV_FIRST)
		return player.InvBody[location];

	if (location <= INVITEM_INV_LAST)
		return player.InvList[location - INVITEM_INV_FIRST];

	return player.SpdList[location - INVITEM_BELT_FIRST];
}

bool TryStartStackSplit(int cii)
{
	if (cii < INVITEM_INV_FIRST)
		return false;
	if (!oracool::IsSinglePlayer())
		return false;

	Player &player = *MyPlayer;
	const Item &item = (cii <= INVITEM_INV_LAST)
	    ? player.InvList[cii - INVITEM_INV_FIRST]
	    : player.SpdList[cii - INVITEM_BELT_FIRST];

	if (!item.isStackableConsumable() || item.stackCount() <= 1)
		return false;

	CloseGoldWithdraw();

	if (talkflag)
		control_reset_talk();

	const Point start = GetPanelPosition(UiPanels::Inventory, { 67, 128 });
	SDL_Rect rect = MakeSdlRect(start.x, start.y, 180, 20);
	SDL_SetTextInputRect(&rect);

	OpenGoldDrop(static_cast<int8_t>(cii), item.stackCount());
	return true;
}

bool UseInvItem(int cii)
{
	if (IsInspectingPlayer())
		return false;

	Player &player = *MyPlayer;

	if (player._pInvincible && player._pHitPoints == 0 && &player == MyPlayer)
		return true;
	if (pcurs != CURSOR_HAND)
		return true;
	if (stextflag != TalkID::None)
		return true;
	if (cii < INVITEM_INV_FIRST)
		return false;

	bool speedlist = false;
	int c;
	Item *item;
	if (cii <= INVITEM_INV_LAST) {
		c = cii - INVITEM_INV_FIRST;
		// Oracool: pcursinvitem is set from GetActiveInvListItem (see CheckInvHLight), which is
		// tab-aware - c is an index into whichever tab is currently active, not always InvList. A
		// raw player.InvList[c] read here silently used-the-wrong-item (or nothing) for anything
		// hovered in an extra tab, e.g. right-clicking a book in tabs 2-10 appeared to do nothing.
		item = &GetActiveInvListItem(player, c);
	} else {
		if (talkflag)
			return true;
		c = cii - INVITEM_BELT_FIRST;

		// Oracool: HUD overhaul - defense-in-depth against any caller still reaching slot 0/5/6/7
		// (the Menu/Town Portal/hidden slots); the normal mouse/keymap paths already can't produce
		// these indices (see FindTargetSlotUnderItemCursor, CheckInvHLight, InitKeymapActions).
		if (!oracool::IsRealBeltItemSlot(c))
			return false;

		item = &player.SpdList[c];
		speedlist = true;

		// Belt Mod gives each belt slot its own physical stock that refills itself from
		// inventory once emptied (see DecrementOrRemoveSpdBarItem/RefillBeltSlotFromInventory),
		// which makes vanilla's autoRefillBelt redirect below counterproductive: it would
		// silently consume straight from inventory instead of the belt's own stock, so a
		// Belt Mod slot would never actually deplete (or refill) the way the player sees it.
		const bool beltModActive = oracool::IsSinglePlayer();

		// If selected speedlist item exists in InvList, use the InvList item.
		for (int i = 0; i < player._pNumInv && *sgOptions.Gameplay.autoRefillBelt && !beltModActive; i++) {
			if (player.InvList[i]._iMiscId == item->_iMiscId && player.InvList[i]._iSpell == item->_iSpell) {
				c = i;
				item = &player.InvList[c];
				cii = c + INVITEM_INV_FIRST;
				speedlist = false;
				break;
			}
		}

		// If speedlist item is not inventory, use same item at the end of the speedlist if exists.
		if (speedlist && *sgOptions.Gameplay.autoRefillBelt && !beltModActive) {
			for (int i = INVITEM_BELT_LAST - INVITEM_BELT_FIRST; i > c; i--) {
				Item &candidate = player.SpdList[i];

				if (!candidate.isEmpty() && candidate._iMiscId == item->_iMiscId && candidate._iSpell == item->_iSpell) {
					c = i;
					cii = c + INVITEM_BELT_FIRST;
					item = &candidate;
					break;
				}
			}
		}
	}

	constexpr int SpeechDelay = 10;
	if (item->IDidx == IDI_MUSHROOM) {
		player.Say(HeroSpeech::NowThatsOneBigMushroom, SpeechDelay);
		return true;
	}
	if (item->IDidx == IDI_FUNGALTM) {

		PlaySFX(IS_IBOOK);
		player.Say(HeroSpeech::ThatDidntDoAnything, SpeechDelay);
		return true;
	}

	if (player.isOnLevel(0)) {
		if (UseItemOpensHive(*item, player.position.tile)) {
			OpenHive();
			player.RemoveInvItem(c);
			return true;
		}
		if (UseItemOpensGrave(*item, player.position.tile)) {
			OpenGrave();
			player.RemoveInvItem(c);
			return true;
		}
	}

	if (!item->isUsable())
		return false;

	if (!player.CanUseItem(*item)) {
		player.Say(HeroSpeech::ICantUseThisYet);
		return true;
	}

	// The spell bands and the Rule of Rangs (user, 2026-08-19: "apply lvl req rule to books as
	// well"). Checked HERE rather than inside UseItem, because the book is consumed by this function
	// AFTER UseItem returns - refusing any later would eat the book and teach nothing.
	if (item->_iMiscId == IMISC_BOOK) {
		const SpellID bookSpell = item->_iSpell;
		const int nextLevel = player._pSplLvl[static_cast<size_t>(bookSpell)] + 1;
		// AND the ceiling, which used to be checked only inside UseItem - and only to skip the
		// level change, not to refuse the read. So a book of a spell already at MaxSpellLevel was
		// eaten for nothing (user, 2026-09-03: "make sure these lvl req ban me from reading and
		// thus destroying a book"). Refused here, where refusing still saves the book.
		if (nextLevel > MaxSpellLevel) {
			player.Say(HeroSpeech::ICantUseThisYet);
			if (&player == MyPlayer)
				EventPlrMsg(fmt::format(fmt::runtime(_("{:s} is already at its highest level.")),
				                pgettext("spell", GetSpellData(bookSpell).sNameText)),
				    UiFlags::ColorRed);
			return true;
		}
		if (!oracool::CanReadSpellBookTo(player, bookSpell, nextLevel)) {
			player.Say(HeroSpeech::ICantUseThisYet);
			// SAYS THE NUMBER. The voice line alone left the player unable to tell a refusal from a
			// read that did nothing - "i am not sure i learned the spells" (user, 2026-09-03). The
			// book is intact either way; this is what makes that visible.
			if (&player == MyPlayer)
				EventPlrMsg(fmt::format(fmt::runtime(_("{:s} needs level {:d} to reach level {:d}. The book is unread.")),
				                pgettext("spell", GetSpellData(bookSpell).sNameText),
				                oracool::SpellRankRequiredLevel(bookSpell, nextLevel), nextLevel),
				    UiFlags::ColorRed);
			return true;
		}
	}

	// The signet's lifetime cap, checked HERE for exactly the reason the book gate above is here:
	// this function consumes the item AFTER UseItem returns, so refusing any later would eat a
	// capped, permanent, unrecoverable resource and teach nothing. One rule, asked in two places -
	// the description panel prints the same count.
	// A Sealed Map opens its encounter from HERE rather than from UseItem, which is handed only the
	// misc id and so cannot tell three maps apart. Refused outside town without being consumed -
	// same reason as the signet gate below: this function eats the item after UseItem returns.
	if (item->_iMiscId == IMISC_ORACOOL_MAP) {
		oracool::NamedEncounter encounter;
		if (!oracool::EncounterForMapItem(item->IDidx, encounter))
			return true;
		if (!oracool::EnterNamedEncounter(player, encounter)) {
			player.Say(HeroSpeech::ICantUseThisYet);
			if (&player == MyPlayer)
				oracool::LogEvent("A sealed map only opens in town.");
			return true;
		}
		// Consumed only once the encounter has actually opened.
		if (&player == MyPlayer)
			oracool::PlayUiEventSound(oracool::UiEventSound::MapUnseal);
		player.RemoveInvItem(cii - INVITEM_INV_FIRST);
		return true;
	}

	if (item->_iMiscId == IMISC_ORACOOL_SIGNET && !oracool::CanConsumeSignet(player)) {
		player.Say(HeroSpeech::ICantUseThisYet);
		if (&player == MyPlayer)
			oracool::LogEvent("Signet of Learning: this life has no room for another.");
		return true;
	}

	if (item->_iMiscId == IMISC_NONE && item->_itype == ItemType::Gold) {
		StartGoldDrop();
		return true;
	}

	if (DropGoldFlag) {
		CloseGoldDrop();
	}

	if (item->isScroll() && leveltype == DTYPE_TOWN && !GetSpellData(item->_iSpell).isAllowedInTown()) {
		return true;
	}

	if (item->_iMiscId > IMISC_RUNEFIRST && item->_iMiscId < IMISC_RUNELAST && leveltype == DTYPE_TOWN) {
		return true;
	}

	if (item->_iMiscId == IMISC_ARENAPOT && !player.isOnArenaLevel()) {
		player.Say(HeroSpeech::ThatWontWorkHere);
		return true;
	}

	int idata = GetItemDropAnimIndexFor(*item);
	if (item->_iMiscId == IMISC_BOOK)
		PlaySFX(IS_RBOOK);
	// A Signet of Learning speaks with its own sound instead of the generic use sound (RfA-04) - the
	// cap gate above has already refused a signet that would not be consumed.
	else if (&player == MyPlayer
	    && !(item->_iMiscId == IMISC_ORACOOL_SIGNET && oracool::PlayUiEventSound(oracool::UiEventSound::SignetUse)))
		PlaySFX(ItemInvSnds[idata]);

	UseItem(player.getId(), item->_iMiscId, item->_iSpell, cii);

	if (speedlist) {
		if (player.SpdList[c]._iMiscId == IMISC_NOTE) {
			InitQTextMsg(TEXT_BOOK9);
			CloseInventory();
			return true;
		}
		if (!item->isScroll() && !item->isRune())
			DecrementOrRemoveSpdBarItem(player, c);
		return true;
	}
	if (item->_iMiscId == IMISC_MAPOFDOOM)
		return true;
	if (item->_iMiscId == IMISC_NOTE) {
		InitQTextMsg(TEXT_BOOK9);
		CloseInventory();
		return true;
	}
	if (!item->isScroll() && !item->isRune())
		DecrementOrRemoveInvItem(player, c, ActiveInventoryTab == 0 ? -1 : ActiveInventoryTab - 1);

	return true;
}

void CloseInventory()
{
	CloseGoldWithdraw();
	CloseStash();
	invflag = false;
	ActiveInventoryTab = 0;
}

void CloseStash()
{
	if (!IsStashOpen)
		return;

	Player &myPlayer = *MyPlayer;
	if (!myPlayer.HoldItem.isEmpty()) {
		std::optional<Point> itemTile = FindAdjacentPositionForItem(myPlayer.position.future, myPlayer._pdir);
		if (itemTile) {
			NetSendCmdPItem(true, CMD_PUTITEM, *itemTile, myPlayer.HoldItem);
		} else {
			// Potions to the belt first; anything else to the inventory, the belt only as the last room
			// before the fatal below.
			if (!(myPlayer.HoldItem.isPotion() && AutoPlaceItemInBelt(myPlayer, myPlayer.HoldItem, true))
			    && !AutoPlaceItemInInventory(myPlayer, myPlayer.HoldItem, true)
			    && !AutoPlaceItemInStash(myPlayer, myPlayer.HoldItem, true)
			    && !AutoPlaceItemInBelt(myPlayer, myPlayer.HoldItem, true)) {
				// This can fail for max gold, arena potions and a stash that has been arranged
				// to not have room for the item all 3 cases are extremely unlikely
				app_fatal(_("No room for item"));
			}
			PlaySFX(ItemInvSnds[GetItemDropAnimIndexFor(myPlayer.HoldItem)]);
		}
		myPlayer.HoldItem.clear();
		NewCursor(CURSOR_HAND);
	}

	IsStashOpen = false;
	oracool::CloseStashChestObject();
}

void DoTelekinesis()
{
	if (ObjectUnderCursor != nullptr && !ObjectUnderCursor->IsDisabled())
		NetSendCmdLoc(MyPlayerId, true, CMD_OPOBJT, cursPosition);
	if (pcursitem != -1)
		NetSendCmdGItem(true, CMD_REQUESTAGITEM, MyPlayerId, pcursitem);
	if (pcursmonst != -1) {
		auto &monter = Monsters[pcursmonst];
		if (!M_Talker(monter) && monter.talkMsg == TEXT_NONE)
			NetSendCmdParam1(true, CMD_KNOCKBACK, pcursmonst);
	}
	NewCursor(CURSOR_HAND);
}

int CalculateGold(Player &player)
{
	// Accumulated in int64_t and SATURATED on the way out. Twenty-two full stacks is 2.2 billion,
	// past what an int holds, and the backpack has seventy cells - so the old int accumulator was
	// reachable overflow, not a theoretical one (external audit, 2026-08-25).
	//
	// Saturating rather than widening the return, because the destination is `Player::_pGold`, an
	// int. Widening this alone would move the overflow one line down into the caller. A player who
	// somehow holds more than INT_MAX in the backpack sees INT_MAX, which is wrong by a knowable
	// amount in a knowable direction - unlike a wrap, which is wrong by an unknowable one.
	int64_t gold = 0;

	for (int i = 0; i < player._pNumInv; i++) {
		if (player.InvList[i]._itype == ItemType::Gold)
			gold += player.InvList[i]._ivalue;
	}

	return static_cast<int>(std::min<int64_t>(gold, std::numeric_limits<int>::max()));
}

Size GetInventorySize(const Item &item)
{
	int itemSizeIndex = item._iCurs + CURSOR_FIRSTITEM;
	auto size = GetInvItemSize(itemSizeIndex);

	return { size.width / InventorySlotSizeInPixels.width, size.height / InventorySlotSizeInPixels.height };
}

} // namespace devilution
