/**
 * @file oracool/runeword_book.h
 *
 * Oracool: user request (2026-08-20) - a browsable reference for all 370 runewords.
 *
 * 944x616, centred in the space above the HUD, semitransparent with the same ornate gold border the
 * mini-map and event log wear. Four columns, scrollable, grouped by host slot, with two independent
 * multi-select filter rows: the ten equipment slots along the top, and all 33 runes beneath them.
 *
 * ## Why a window and not a wiki page
 *
 * The wiki already lists every word. What it cannot do is answer "which of these can I build with
 * the runes in my stash right now", which is the question the rune filter exists for: select the
 * runes you have and the list narrows to words that use them.
 *
 * ## The two filters are ANDed, the members of each are ORed
 *
 * Selecting Helm and Body shows helm words AND body words - within one row, more selections mean
 * MORE results. Selecting Tir as well narrows that to helm-or-body words that use Tir - across the
 * rows, more selections mean fewer. That is the only combination that makes both rows useful: slot
 * filters answer "where can I put it", rune filters answer "what do I own", and those are different
 * questions about the same list.
 *
 * Nothing selected in a row means that row is not filtering at all, which is why clicking a lit key
 * again clears it rather than requiring a separate reset.
 */
#pragma once

#include "engine/surface.hpp"
#include "engine/rectangle.hpp"
#include "utils/attributes.h"

namespace devilution::oracool {

/** @brief Whether the runeword book is showing. */
DVL_API_FOR_TEST bool IsRunewordBookOpen();

/** @brief Opens the book, closing whatever else owns the screen. */
void OpenRunewordBook();

/** @brief Closes the book. Safe when it is already closed. */
void CloseRunewordBook();

/** @brief Opens the book if closed, closes it if open. The burger-menu entry's action. */
void ToggleRunewordBook();

/** @brief The window's screen rect - 944x616, centred above the HUD. */
Rectangle GetRunewordBookRect();

/** @brief Draws the book. Does nothing when closed. */
void DrawRunewordBook(const Surface &out);

/**
 * @brief Routes a left click at @p position.
 *
 * @return true when the book consumed it - which it does for EVERY point inside its rect, not only
 * the ones over a control. A click that lands on the window's background must not reach the world
 * behind it (the standing no-click-through rule for every window in this fork).
 */
bool HandleRunewordBookClick(Point position);

/** @brief Routes a mouse-wheel notch. @p delta is positive scrolling up. */
bool HandleRunewordBookScroll(int delta);

} // namespace devilution::oracool
