/**
 * @file cursor.h
 *
 * Interface of cursor tracking functionality.
 */
#pragma once

#include <cstdint>
#include <utility>

#include "engine.h"
#include "engine/clx_sprite.hpp"
#include "utils/attributes.h"
#include "utils/stdcompat/optional.hpp"

namespace devilution {

enum cursor_id : uint8_t {
	CURSOR_NONE,
	CURSOR_HAND,
	CURSOR_IDENTIFY,
	CURSOR_REPAIR,
	CURSOR_RECHARGE,
	CURSOR_DISARM,
	CURSOR_OIL,
	CURSOR_TELEKINESIS,
	CURSOR_RESURRECT,
	CURSOR_TELEPORT,
	CURSOR_HEALOTHER,
	CURSOR_HOURGLASS,
	CURSOR_FIRSTITEM,
};

extern int pcursmonst;
extern DVL_API_FOR_TEST int8_t pcursinvitem;
/**
 * @brief Oracool Tabbed Inventory: set by CheckInvHLight, alongside pcursinvitem, whenever the
 * mouse is hovering an item stored in one of the Tabbed Inventory extra tabs (2-10). pcursinvitem
 * itself stays -1 for these (legacy drag/drop code assumes tab-1 indices and must keep doing so),
 * so the single-shot cursor-target actions that don't involve drag state at all - Identify,
 * Repair, Recharge, applying an Oil - read these two instead, mirroring how pcursstashitem already
 * gives those same four actions a second, independent target besides pcursinvitem. -1/-1 when no
 * extra-tab item is hovered.
 */
// DVL_API_FOR_TEST since 1.11.081: IsActiveInvItemHovered answers from these on every tab but the
// first, and the test that pins it has to be able to set them.
extern DVL_API_FOR_TEST int8_t pcursinvtabidx;
extern DVL_API_FOR_TEST int8_t pcursinvtabitem;
extern DVL_API_FOR_TEST uint16_t pcursstashitem;
extern DVL_API_FOR_TEST int8_t pcursitem;

struct Object; // Defined in objects.h
extern Object *ObjectUnderCursor;

extern int8_t pcursplr;
extern Point cursPosition;
extern DVL_API_FOR_TEST int pcurs;

void InitCursor();
void FreeCursor();
void ResetCursor();

struct Item;
/**
 * @brief Use the item sprite as the cursor (or show the default hand cursor if the item isEmpty)
 */
void NewCursor(const Item &item);

void NewCursor(int cursId);

void InitLevelCursor();
void CheckRportal();
void CheckTown();
void CheckCursMove();

void DrawSoftwareCursor(const Surface &out, Point position, int cursId);

void DrawItem(const Item &item, const Surface &out, Point position, ClxSprite clx);

/**
 * @brief Oracool: stamps a red X over a broken (0 durability) item's icon, inset within its own
 * bounds. Shared between DrawItem here (inventory/belt/equipped/tabs) and the ground-item renderer
 * (engine/render/scrollrt.cpp) - those two are separate rendering paths (UI panels vs. the live
 * dungeon view) with no other code in common, so this exists specifically to avoid duplicating the
 * X-drawing logic between them.
 * @param topLeft Top-left corner of the icon's own bounding box.
 */
void DrawBrokenItemMarker(const Surface &out, Point topLeft, int width, int height);

/** Returns the sprite for the given inventory index. */
ClxSprite GetInvItemSprite(int cursId);

/** @brief Oracool: how many sprites the original item-cursor sheets hold - 1 = objcurs.cel, 2 = objcurs2.cel. For the art export. */
size_t GetNumInvItemsInSheet(int sheet);
/** @brief Every cursor frame across the three sheets, the hand's included: an item's _iCurs + CURSOR_FIRSTITEM is at most this. */
size_t GetNumInvItems();

ClxSprite GetHalfSizeItemSprite(int cursId);
ClxSprite GetHalfSizeItemSpriteRed(int cursId);
void CreateHalfSizeItemSprites();
void FreeHalfSizeItemSprites();

/** Returns the width and height for an inventory index. */
Size GetInvItemSize(int cursId);

} // namespace devilution
