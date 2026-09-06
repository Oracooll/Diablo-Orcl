#pragma once

#include "engine/clx_sprite.hpp"
#include "engine/rectangle.hpp"
#include "engine/surface.hpp"
#include "spelldat.h" // SpellID, for BuildSpellStatBlock

#include <string>

namespace devilution {

struct Player; // BindAbilityHotkey takes one; player.h is heavy and this header is widely included

// Forward-declared rather than including oracool/class_tree.h: that header pulls in a great deal,
// and oracool/skill_picker.h already includes BOTH it and this one - a full include here would make
// that a cycle. The underlying type has to match class_tree.h's declaration exactly.
namespace oracool {
enum class ClassTreeSkill : uint16_t;
} // namespace oracool


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

/**
 * @brief F1-F8 are the ability hotkeys - the first eight slots of the vanilla _pSplHotKey array,
 * claimed outright (user, 2026-08-17: F1-F6 "not be used in any other way in the game"; widened to
 * F8 on 2026-08-18, with F9-F12 taken for game speed, the log and screenshots).
 *
 * Each key carries TWO bindings: _pSplHotKey[i] is what the bare key readies on the RIGHT button,
 * _pSplLHotKey[i] what LShift+key readies on the LEFT. NumHotkeys is 12, so both arrays already had
 * the room for the two extra keys.
 */
constexpr size_t AbilityFKeyCount = 8;

/**
 * @brief One F-key press: with the Abilities window open, binds/unbinds (@p shift unbinds) the
 * hovered ability to slot @p slot; in play, readies slot @p slot's ability through the vanilla
 * quick-spell path. Returns false only for a slot out of range.
 */
bool HandleAbilityFKey(size_t slot, bool shift);

/**
 * @brief Binds F-key @p slot to @p spell on @p leftButton's side, or unbinds it if it is already
 * there. THE hard rule (user, 2026-09-07): "a hot key can only be assigned to a single skill on a
 * single picker" - so the key is first emptied on BOTH buttons and the aura array, and the spell is
 * swept off every other key. Exported so the rule can be tested without a window open.
 */
void BindAbilityHotkey(Player &player, size_t slot, SpellID spell, bool leftButton);

/**
 * @brief Which F-key @p spell sits on for @p leftButton's side, 1-8, or 0 for none.
 *
 * Exported for the quick lists, which bind these keys too and so have to show what is already bound
 * - a picker that let you press F3 without saying F3 was taken would be a picker that silently
 * moves bindings.
 */
int GetAbilityFKeyNumber(SpellID spell, bool leftButton);

/**
 * @brief Which F-key @p aura sits on, 1-8, or 0 for none.
 *
 * No button parameter: an aura occupies the right button whichever list it was bound from, so it
 * has one binding rather than one per side.
 */
int GetAuraFKeyNumber(oracool::ClassTreeSkill aura);


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
/**
 * @brief Handles a click inside the Abilities window.
 *
 * @param assignToRightButton which mouse button did the clicking - a row is readied on the button
 * that clicked it (user request, 2026-08-15). Defaults to the left, which is also where a touch tap
 * belongs, since a touchscreen has no second button to offer.
 */
void CheckSBook(bool assignToRightButton = false);
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

/**
 * @brief The spell's numbers as newline-separated lines: level, mana, damage, and next level's.
 *
 * Exported so the skill picker can show the same block the Abilities window does (user, 2026-08-28:
 * "i want more information in the hover opoups of skills/spells/auras"). One builder rather than
 * two, because the two would disagree about the next-level line the first time a formula changed.
 */
std::string BuildSpellStatBlock(SpellID sn, bool withNext = true);

} // namespace devilution
