/**
 * @file oracool/book_frame.h
 *
 * The painted book frames (user, 2026-09-05): two bezels with a transparent core, one wide for the
 * Runeword and Crafting books, one tall for Levski's recipe book. The game fills the core with the
 * dark translucent backing the item tooltip uses, lays the bezel over it, and draws its title and
 * contents inside; the red X goes at the frame's top-right like every other window.
 *
 * The frames ship 1:1 (ui\book_frame_wide.png 944x616, ui\book_frame_tall.png 420x620); the core
 * rects below were measured off the paintings and are what the backing fills.
 */
#pragma once

#include "engine/rectangle.hpp"
#include "engine/size.hpp"
#include "engine/surface.hpp"

namespace devilution::oracool {

enum class BookFrame : uint8_t {
	/** 944x616 - the Runeword book and the Crafting book. */
	Wide,
	/** 420x620 - Levski's recipe book. */
	Tall,
};

/** @brief The frame's painted size, which is the window's size. */
Size BookFrameSize(BookFrame frame);

/** @brief The transparent core of @p frame, in screen space for a window at @p window. */
Rectangle BookFrameCore(BookFrame frame, const Rectangle &window);

/** @brief The backing in the core, then the bezel over it. Titles, contents and the X are the caller's. */
void DrawBookFrame(const Surface &out, BookFrame frame, const Rectangle &window);

} // namespace devilution::oracool
