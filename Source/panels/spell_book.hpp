#pragma once

#include "engine/clx_sprite.hpp"
#include "engine/rectangle.hpp"
#include "engine/surface.hpp"

namespace devilution {

/**
 * @brief Screen rect of the spell book: 340x720, flush to the top-right corner.
 *
 * Its own rect rather than GetRightPanel()'s, which is 320x352 - the same move the inventory,
 * character sheet, quest log and waypoint list each made when they outgrew the vanilla slot.
 *
 * Anything routing or absorbing a click over the book MUST use this, not GetRightPanel(): a window
 * hit-tested on a rect smaller than it draws lets clicks through to the ground beneath it, which is
 * exactly the bug the left-hand panels had.
 */
Rectangle GetSpellBookPanelRect();

/**
 * @brief Screen rect of the scrolling list - below the title separator, above the bottom margin.
 *
 * Hit-testing a row must be gated on this, since rows move with the scroll.
 */
Rectangle GetSpellBookContentRect();

/** @brief Scrolls the current sheet by @p notches wheel steps, positive down. Clamped to the list. */
void ScrollSpellBook(int notches);

/**
 * @brief Moves to the next (@p direction +1) or previous (-1) sheet.
 *
 * Skips sheets this class does not have, so a non-Paladin never lands on an empty Auras page.
 */
void CycleAbilitySheet(int direction);

/** @brief Returns every sheet to the top. Called when the window is opened. */
void ResetSpellBookScroll();

/** @brief Clears the pressed state of the sheet arrows, on mouse release. */
void ReleaseSpellBookButtons();

/**
 * @brief Opens or closes the Abilities window, closing whatever it would overlap.
 *
 * Shared by the burger menu and the HUD's two skill buttons, so those cannot drift apart in what
 * they close on the way.
 */
void ToggleAbilitiesWindow();

void InitSpellBook();
void FreeSpellBook();
void CheckSBook();
void DrawSpellBook(const Surface &out);

/**
 * @brief Draws the hovered row's description panel, if any, and clears it.
 *
 * Separate from DrawSpellBook so the frame can put it ABOVE the HUD. The window itself is drawn
 * early - before the plate, the belt and the orbs - so a panel drawn with it is painted over by
 * them. Call this beside the cursor tooltip, which occupies the same "above everything" slot.
 *
 * Safe to call on a frame where the window is closed: it simply has nothing pending.
 */
void DrawAbilityHoverPanel(const Surface &out);

} // namespace devilution
