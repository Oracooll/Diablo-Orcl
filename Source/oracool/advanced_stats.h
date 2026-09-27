/**
 * @file oracool/advanced_stats.h
 *
 * Oracool: the ADVANCED STATS window (user, 2026-09-26, approved from a mock-up with the grouped
 * hero sheet). The grouped character sheet keeps only the numbers a player reads every few minutes -
 * attributes, the two mouse buttons, armour, to hit, the pools, the four resistances. Everything the
 * old two-column list carried below Mana (steal, frames, trap damage, thorns, the armour breakdown,
 * the signets...) moves here, together with bonuses no sheet ever showed: the Hellfire weapon
 * specials, Cold Mastery, the per-kill gems and runes, movement speed, magic and gold find.
 *
 * WHERE IT LIVES: the RIGHT side-panel slot, 340x720 on the same stone canvas as the inventory and
 * the Abilities window (user: "right panel slot, overlap inventory/abilities screen if open").
 *
 * THE ONE RULE for sharing that slot, chosen for being the simplest that is also consistent:
 *
 *   - Opening this window COVERS whatever held the slot. The inventory or the Abilities window is put
 *     away (its flag cleared, nothing else touched - no held item returned, no tab reset) and
 *     remembered.
 *   - Closing it - the red X, Escape, the ADVANCED STATS button again, or the character sheet closing
 *     - puts the covered window BACK, so to the eye this window simply lay over it.
 *   - Opening the inventory or the Abilities window while this one is up closes this one for good
 *     (nothing is restored: the player has just chosen the window they want in the slot). That is
 *     detected in IsAdvancedStatsOpen() itself, so every opener - the I and S keys, the HUD buttons,
 *     a shop opening the backpack - is covered without each one having to know this window exists.
 *   - The space bar and ClosePanels() close it WITHOUT restoring (they are closing everything).
 *
 * Only one window ever occupies the slot, so the click and hover routing needs no precedence
 * between this window and the one it covers: while it is open, invflag and sbookflag are false.
 *
 * It reads InspectPlayer, like the sheet it extends.
 */
#pragma once

#include <cstdint>

#include "DiabloUI/ui_flags.hpp"
#include "engine/point.hpp"
#include "engine/rectangle.hpp"
#include "engine/surface.hpp"
#include "utils/stdcompat/string_view.hpp"

namespace devilution::oracool {

/** @brief Opens the window, covering (and remembering) the inventory or Abilities window if one is up. */
void OpenAdvancedStats();

/**
 * @brief Closes the window.
 *
 * @param restoreCovered true puts back the inventory or Abilities window it covered - the ordinary
 * close. false forgets it: ClosePanels() and the space bar pass false, because they are closing
 * everything and a window springing back open behind them would undo their work.
 */
void CloseAdvancedStats(bool restoreCovered = true);

/** @brief The ADVANCED STATS button on the grouped character sheet: open when shut, close when open. */
void ToggleAdvancedStats();

/**
 * @brief Whether the window is up.
 *
 * NOT a plain flag read: if the inventory or the Abilities window has been opened since this window
 * opened, it closes itself here, forgetting what it covered (see the file comment). Every caller
 * therefore sees one window in the right-hand slot, never two.
 */
bool IsAdvancedStatsOpen();

/** @brief The window's screen rect - the right-hand slot, bottom-docked like the inventory. */
Rectangle GetAdvancedStatsRect();

/** @brief Draws the window. Call from the panel draw chain; draws nothing when closed. */
void DrawAdvancedStats(const Surface &out);

/**
 * @brief A left click at @p mousePosition. True when the window took it - which is every click inside
 * its rect, so nothing under it (the ground) ever sees one. The red X is NOT tested here: diablo.cpp
 * tests every window's X ahead of its body, and this window's X is in that list.
 */
bool HandleAdvancedStatsClick(Point mousePosition);

/**
 * @brief Mouse wheel: @p notches positive scrolls DOWN the list. True when consumed - only while the
 * window is open and the cursor is over it, so the wheel still zooms the dungeon everywhere else.
 */
bool HandleAdvancedStatsScroll(int notches);

// -------------------------------------------------------------------------------------------------
// The box look both windows share - the grouped character sheet (panels/charpanel.cpp) and this one.
// One definition, so the two windows sitting side by side cannot draw their boxes two ways.
// -------------------------------------------------------------------------------------------------

/** @brief The kinds of box the two sheets draw. */
enum class SheetBoxTone : uint8_t {
	/** A dark translucent box with a 1px bronze edge - every ordinary stat box, and the grey facts. */
	Plain,
	/** Navy with a blue edge - a BONUS row (the hero has something extra), per the mock-up. */
	Bonus,
	/** A dark red wash - a curse the hero is carrying (No mana, Life drained, All resistances zero). */
	Curse,
	/** The small bronze strip a section heading (OFFENSE, DEFENSE...) sits in. */
	Heading,
	/** Darker and edge-less-looking: the recessed "base N" strip under an attribute, and bar grooves. */
	Recess,
};

/**
 * @brief Draws one box of @p tone at @p rect, with its drop shadow unless @p castShadow is false - for a box that
 * sinks when pressed, whose shadow the caller draws at its resting place. Clip-safe: a box half scrolled out of a
 * subregion is cut.
 */
void DrawSheetBox(const Surface &out, Rectangle rect, SheetBoxTone tone = SheetBoxTone::Plain, bool castShadow = true);

/**
 * @brief The fields' drop shadow on its own: @p rect's size, 3px down and 3px left, translucent black. For things that
 * sit among the fields but are not one - the sheet's RESET and ADVANCED STATS buttons (user, 2026-09-26). Draw it
 * before the thing it belongs to. False on an indexed surface, where nothing is drawn.
 */
bool DrawSheetShadow(const Surface &out, Rectangle rect);

/**
 * @brief A thin meter: a dark groove inside a 1px grey frame (corners cut) filling @p rect, with @p value /
 * @p maximum of its inner width in @p rgb (0xRRGGBB) from the left - clamped, and empty for a zero maximum.
 * The fill colour-cycles (bands of lighter and darker @p rgb flowing left to right) and grey marks sit at
 * every 10% - the HUD XP bar's frame and marks (2026-09-26). @p fromRight fills from the right edge leftwards, the
 * light flowing that way too - a negative resistance's bar (2026-09-27). @p fallbackIndex is the palette colour used
 * on an indexed surface (the tests' surfaces).
 */
void DrawSheetBar(const Surface &out, Rectangle rect, uint64_t value, uint64_t maximum, uint32_t rgb, uint8_t fallbackIndex,
    bool fromRight = false);

/** @brief Test hook: at 0 or above, the bars' colour cycle reads this many milliseconds instead of the clock. */
extern DVL_API_FOR_TEST int32_t SheetBarClockOverrideMs;

/**
 * @brief Draws @p text into @p rect in the largest of the 12/11/10/9 fonts whose single line fits the
 * rect's width, with the sheets' drop shadow.
 *
 * Because DrawString WRAPS a line that outgrows its rect, and a wrapped second line inside a 20px box is
 * drawn over the box below - which reads as a rendering fault, not as a long name. The longest skill name
 * in the game ("Master of the Long Staff") beside a three-digit range is the case this exists for.
 * @p flags carries colour and alignment and must NOT carry a font size - this function picks it.
 */
void DrawSheetTextFitted(const Surface &out, string_view text, Rectangle rect, UiFlags flags);

} // namespace devilution::oracool
