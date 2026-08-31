/**
 * @file oracool/skill_picker.h
 *
 * Oracool: choosing what sits on a mouse button.
 *
 * User direction, 2026-08-20: "Selecting lmb/rmb skills/spell happens from these pop-ups when i
 * click lmb/rmb... Basically we only navigate using the left mouse button. Easier to remember and
 * to muscle memorize."
 *
 * ## Why this is a separate window from the Abilities window
 *
 * The Abilities window used to do both jobs, and they want opposite things. MANAGING points wants
 * space, tier structure, descriptions and room to read. PICKING wants density and speed. Every
 * sizing problem the layout ran into - ninety-three icons against fifty cells - came from trying to
 * satisfy both in one grid.
 *
 * Splitting them does not solve that problem so much as dissolve it: a picker only needs what the
 * character can cast RIGHT NOW - invested tree skills and known spells - not the 163-row table with
 * its unlearned tiers. That is a few dozen entries, not ninety-three, and it fits comfortably.
 *
 * ## The gesture
 *
 * LEFT click on the LMB well opens the LMB picker; LEFT click on the RMB well opens the RMB
 * picker. One popup at a time, each binding only its own button, the same motion mirrored. Right
 * click is never an opener - it still CASTS, which is the one hard constraint here: the picker is
 * reached from the well on the HUD, never from right-clicking the world.
 *
 * ## What it supersedes
 *
 * The two-icon attack quick list (oracool/attack_skills.h). That was this window with only the
 * basic attacks in it; the attacks are now simply its first two entries.
 */
#pragma once

#include <cstddef>
#include <cstdint>

#include "engine/point.hpp"
#include "engine/rectangle.hpp"
#include "engine/surface.hpp"
#include "oracool/class_tree.h" // ClassTreeSkill - an aura is named by its row, not by a SpellID
#include "spelldat.h"

namespace devilution {
struct Player;
} // namespace devilution

namespace devilution::oracool {

/** @brief Whether either picker is open. */
bool IsSkillPickerOpen();

/** @brief Opens the picker for one button. @p forLeftButton picks which button it will bind. */
void OpenSkillPicker(bool forLeftButton);

/** @brief Closes whichever picker is open. Safe when none is. */
void CloseSkillPicker();

/** @brief The open picker's screen rect, or an empty rect when none is open. */
Rectangle GetSkillPickerRect();

/** @brief Draws the open picker. No-op when none is. */
void DrawSkillPicker(const Surface &out);

/**
 * @brief Routes a click. Returns true when the picker consumed it - which it does for ANY click
 * inside its rect, so a miss inside the window cannot fall through to the world behind it.
 *
 * A click outside the rect closes the picker and returns false, letting the click land where it was
 * aimed: a picker you have to dismiss before you can act is a picker that costs two clicks to
 * cancel.
 */
bool CheckSkillPickerClick(Point mousePosition);

/** @brief Scrolls the list by @p notches, for the mouse wheel. Only matters when it overflows. */
void ScrollSkillPicker(int notches);

/** @brief Which button the open picker binds. Only meaningful while IsSkillPickerOpen(). */
bool IsSkillPickerForLeftButton();

/**
 * @brief The spell under the cursor as of the last draw, or SpellID::Invalid over none.
 *
 * For the F-key binding path (user, 2026-08-30): hovering an entry in a quick list and pressing
 * F1-F8 binds that key to that skill, on the button the open picker belongs to. Attacks and auras
 * carry no SpellID and so answer Invalid, which is what makes them unbindable rather than a
 * special case at the call site.
 */
SpellID GetSkillPickerHoveredSpell();

/**
 * @brief The AURA under the cursor, or ClassTreeSkill::None.
 *
 * Separate from the spell above because an aura row carries SpellID::Invalid - it is a toggle, not
 * a cast, and has no spell slot to be named by. Binding one to an F-key therefore cannot go through
 * the SpellID hotkey arrays at all; it uses Player::_pAuraHotKey (user, 2026-08-31: "i cant set
 * them on auras").
 */
ClassTreeSkill GetSkillPickerHoveredAura();

} // namespace devilution::oracool
