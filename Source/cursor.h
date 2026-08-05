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
extern int8_t pcursinvitem;
extern uint16_t pcursstashitem;
extern int8_t pcursitem;

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

ClxSprite GetHalfSizeItemSprite(int cursId);
ClxSprite GetHalfSizeItemSpriteRed(int cursId);
void CreateHalfSizeItemSprites();
void FreeHalfSizeItemSprites();

/** Returns the width and height for an inventory index. */
Size GetInvItemSize(int cursId);

} // namespace devilution
