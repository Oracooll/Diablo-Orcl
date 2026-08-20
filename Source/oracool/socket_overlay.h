/**
 * @file oracool/socket_overlay.h
 *
 * Oracool: what a socketed item shows when you hover it.
 *
 * User request, 2026-08-20, on assembling the fork's first runeword:
 *
 *   "When i hover over items with runes soketed in them i want to see the runes rendered on top of
 *    the sprite."
 *   "when i hover over them i want to see gold circles representing the number of sockets. each
 *    circle to fit in 28x28px grid box. Make these circles diameter 24px centered in each 28x28px
 *    box."
 *   "When an item has it's sockets partially filled, when i hover i want to see the socketed item
 *    in it and the remaining empty circles."
 *
 * Those are one feature, not three: every socket gets a cell, and the cell shows either the stone
 * in it or an empty gold ring. A completed runeword is simply the case where no ring is left.
 *
 * The overlay is HOVER-ONLY by design. A socketed item is still an item first; painting four rune
 * icons over every one of them permanently would make a full stash unreadable, and the socket count
 * already reads from the backing colour and the ground label.
 */
#pragma once

#include <cstdint>

#include "engine/point.hpp"
#include "engine/size.hpp"
#include "engine/surface.hpp"

namespace devilution {
struct Item;
} // namespace devilution

namespace devilution::oracool {

/** @brief Whether @p item has anything for the overlay to show - i.e. any sockets at all. */
bool HasSocketOverlay(const Item &item);

/**
 * @brief Draws one cell per socket over an item's inventory sprite.
 *
 * @param spriteBottomLeft the same Point the item's own ClxDraw was given. Item sprites are drawn
 *        from their BOTTOM-left in this engine, and passing the same anchor is what keeps the
 *        overlay locked to the sprite rather than to a rect computed twice, two different ways.
 * @param cells the item's footprint, from GetInventorySize.
 *
 * Cells are laid out across the footprint in reading order. The socket ceiling IS the footprint
 * (Sockets v2), so a socket can never run out of cells to sit in.
 */
void DrawSocketOverlay(const Surface &out, const Item &item, Point spriteBottomLeft, Size cells);

} // namespace devilution::oracool
