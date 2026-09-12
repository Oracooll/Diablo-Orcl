#pragma once

#include <string>

#include "DiabloUI/ui_flags.hpp" // UiFlags - the readied-slot rows are coloured by damage type
#include "engine/clx_sprite.hpp"
#include "engine/surface.hpp"
#include "misdat.h"              // DamageType
#include "utils/attributes.h"    // DVL_API_FOR_TEST on DamageTypeColor

namespace devilution {

/**
 * @brief Screen rect of the character sheet: 340x720, flush to the top-left corner.
 *
 * Its own rect rather than GetLeftPanel()'s, which is 320x352 and shared with the quest log and
 * stash - the same reason the inventory and waypoint list took theirs. Matches the waypoint list
 * and quest log so the four windows are one set.
 */
Rectangle GetCharacterPanelRect();

/**
 * @brief Screen origin of the sheet's CONTENT - the two-column row list and its stat buttons.
 *
 * The panel's left edge, at the top of the area below the title band's separator. Everything
 * positioned against the sheet goes through GetPanelPosition(UiPanels::Character, ...), which
 * returns this, so drawing, the stat buttons and their hit-testing all move together and cannot
 * disagree.
 */
Point GetCharacterContentOrigin();

/**
 * @brief Content-relative position of the Reset Stats button, paired with ResetStatsButtonSize.
 *
 * A function rather than a constant because the button is right-aligned on the "Points to
 * distribute" row, whose y depends on the measured row layout. Both charpanel.cpp's drawing and
 * control.cpp's hit-testing read it, so the two cannot disagree.
 */
Point GetResetStatsButtonPosition();

/**
 * @brief Screen rect of the scrolling row area - below the title separator, above the bottom margin.
 *
 * Hit-testing for anything inside the sheet MUST be gated on this. The + and RESET buttons move
 * with the scroll (see ScrollCharacterSheet), so a button scrolled off the top would otherwise
 * still accept clicks at its new position - under the title band, or off the panel entirely.
 */
Rectangle GetCharacterContentRect();

/**
 * @brief Scrolls the sheet by @p notches wheel steps, positive down. Clamped to the list's extent.
 *
 * The hidden stats below Mana make the list roughly twice the window's height. Scrolling also
 * rewrites the + and RESET buttons' hit rects, since those are positioned per-row.
 */
void ScrollCharacterSheet(int notches);

/** @brief Returns the sheet to the top. Called when the panel is opened. */
void ResetCharacterSheetScroll();

/**
 * @brief The damage text the sheet shows for one mouse button's slot, for @p leftButton.
 *
 * Exported for the test that pins WHICH source each answer comes from - the weapon, a spell's own
 * formula, or a dash. That choice is four branches of live logic and the only other way to observe
 * it is to read the panel off a screenshot.
 */
std::string GetReadiedSlotDamageText(bool leftButton);

/**
 * @brief The name row's text for one mouse button's slot - "Left: Firebolt".
 *
 * Exported alongside the damage text so the test can check the two rows describe the same thing.
 */
std::string GetReadiedSlotNameText(bool leftButton);

/**
 * @brief The colour BOTH of a button's rows are drawn in, keyed to the damage type.
 *
 * Exported for the test that pins the palette. The colour is the feature - it is what tells a
 * player at a glance that a number is fire rather than physical - so it needs pinning like any
 * other output, and a screenshot is the only other way to see it.
 */
UiFlags GetReadiedSlotColor(bool leftButton);

/**
 * @brief The colour an element is written in, wherever it is written.
 *
 * The user's own table (2026-08-31, after Diablo II), and since 1.11.080 the ONE definition:
 * DamageTextColor in qol/floatingnumbers.cpp defers to it for fire, lightning, magic and cold so
 * the sheet and the numbers over a monster's head cannot say different things about the same
 * element. They did for months - the sheet called fire red while the floating number drew grey.
 *
 * Physical is the one deliberate difference: white here (it is a sheet row among coloured rows),
 * gold there (vanilla's damage number, and the most common number on screen).
 */
DVL_API_FOR_TEST UiFlags DamageTypeColor(DamageType type);

extern OptionalOwnedClxSpriteList pChrButtons;

void DrawChr(const Surface &);
void LoadCharPanel();
void FreeCharPanel();

} // namespace devilution
