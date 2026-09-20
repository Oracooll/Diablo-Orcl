/**
 * @file oracool/attack_skills.h
 *
 * Oracool: user request (2026-08-13) - "Regular Attack" and "Fist Attack", the two basic attacks,
 * as visible abilities: listed on the Abilities window's Skills sheet and shown in the HUD's LMB and
 * RMB skill wells.
 *
 * They are deliberately NOT new SpellIDs. Both are the engine's existing "no spell readied" state
 * (SpellID::Invalid), which is exactly what makes a click swing the weapon rather than cast; adding
 * enum entries would mean new rows in SpellITbl, SpellsData, the save format's spell bitmasks and
 * every table keyed on MAX_SPELLS, all to describe a state the player already has. So this module is
 * a *presentation* of that state, and the only real decision it makes is which of the two icons it
 * wears:
 *
 *   armed   -> Regular Attack
 *   unarmed -> Fist Attack
 *
 * That also explains why one of them is selectable and the other is not, which otherwise looks
 * arbitrary. Selecting Regular Attack means "ready the basic attack" - it clears the readied spell.
 * There is nothing separate to select for Fist Attack: it is the same underlying state, and which
 * icon you get is decided by what is in your hand, not by a click.
 */
#pragma once

#include "engine/rectangle.hpp"
#include "engine/surface.hpp"
#include "player.h"
#include "utils/stdcompat/string_view.hpp"

namespace devilution::oracool {

/**
 * @brief A cell of ui\attack_icons.png. The strip is cut in this order - see tools/CutAttackIcons.ps1.
 */
enum class AttackIcon : int {
	/** Crossed swords. */
	Regular = 0,
	/** A bare fist. */
	Fist = 1,

	LAST = Fist,
};

/** @brief Number of cells in the strip, and therefore rows the Skills sheet lists for it. */
inline constexpr size_t AttackIconCount = static_cast<size_t>(AttackIcon::LAST) + 1;

/**
 * @brief The order the Skills sheet LISTS the two attacks in - Fist first, then Regular.
 *
 * Oracool: user request (2026-08-15) - "Move Fist attack on top of skills list, followed by regular
 * attack. Follow this for all classes."
 *
 * Deliberately separate from the enum, which is the ICON STRIP's order and must not move: the strip
 * is cut in enum order by tools/CutAttackIcons.ps1, and DrawAttackIcon indexes it by the enum value.
 * Reordering the enum to change a list would have silently swapped the two pictures instead. Same
 * split the auras already use, and for the same reason - see GetAuraAtDisplayIndex.
 */
inline constexpr AttackIcon AttackIconDisplayOrder[AttackIconCount] = {
	AttackIcon::Fist,
	AttackIcon::Regular,
};

/**
 * @brief Whether @p player would swing fists rather than a weapon right now.
 *
 * Asks the animation's weapon class rather than reading InvBody directly, because that is the value
 * the engine itself already resolved from the equipped items (CalcPlrItemVals recomputes it on every
 * equip change) and it is what decides the attack animation actually played. Reading the two hand
 * slots here would be a second, parallel answer to the same question, free to disagree.
 */
bool IsFightingUnarmed(const Player &player);

/** @brief Which icon @p player's basic attack currently wears. */
AttackIcon BasicAttackIcon(const Player &player);

/** @brief Display name for @p icon, untranslated - callers wrap it in _(). */
const char *AttackIconName(AttackIcon icon);

/**
 * @brief The row's second line: whether this is the attack currently in the player's hands.
 *
 * Exactly one of the two is "active" at any moment, so the pair reads as one status rather than as
 * two independent abilities. Untranslated - callers wrap it in _().
 */
const char *AttackIconDetail(AttackIcon icon, bool active);

/**
 * @brief Fills the HUD plate's LMB well with the basic attack's icon.
 *
 * Unconditionally: left-clicking a target attacks with whatever is in hand, always, and that has
 * never been assignable. The well was drawn empty until now for want of an icon to put in it, which
 * made the plate's only permanently-empty socket the one control that never changes.
 */
void DrawLmbSkillWell(const Surface &out);

/**
 * @brief THE HUD BUTTONS' FEEL (user, 2026-09-20: "apply titlemov.wav on every hover over lmb/rmb/belt
 * icons. lmb/rmb to adopt the 2,2 click relocation when clicked") - the waypoint Act buttons' recipe
 * on the HUD: the UI move sound on the frame the cursor arrives over the LMB well, the RMB well or
 * any of the seven belt cells; a clicked well's icon sinks 2px down and 2px left until the mouse is
 * released, the well's painted socket staying put.
 *
 * TrackHudButtonHover runs once per frame from DrawLmbSkillWell (the HUD's own draw); PressHudWell
 * is the click (diablo.cpp's LMB-well branch, control.cpp's DoPanBtn); ReleaseHudWells is called
 * from both mouse buttons' release paths.
 */
void TrackHudButtonHover();
void PressHudWell(bool leftWell);
void ReleaseHudWells();

// The basic-attack quick list - Open/Close/IsOpen/Draw/CheckClick, and the strip geometry behind
// them - lived here until 2026-08-30 and is gone. It was the two-icon popup from 2026-08-18, and
// the skill picker (oracool/skill_picker.h) superseded it on 2026-08-20: the two attacks are simply
// that window's first two entries now, and clicking one still means the same thing it meant here,
// SpellID::Invalid with SpellType::Invalid.
//
// Removed by audit rather than left parked. Nothing had called any of it since the picker shipped,
// but it kept its own open/close state and its own click router, so a single stray call would have
// put a second, unreachable popup on screen over the live one.

/**
 * @brief The two badges a well's occupant wears: its rank bottom-centre, its F-key top-right.
 *
 * User request (2026-09-02): "i want you to show skill badges also on well icons." The quick lists,
 * the Abilities window and the speedbook all label an icon with what it is worth and what fires it;
 * the wells - the two icons the player actually looks at while fighting - carried at most the F-key,
 * and the LMB well carried nothing at all. Same corners, same plate, same font as everywhere else:
 * see oracool/badge.h. The picker's cells are 38px like these wells, so the two match pixel for
 * pixel and an icon does not change size or labelling as it moves from the list into the well.
 *
 * The RANK is the tree investment when the spell came from a tree row and the spell's own level
 * otherwise - the same either/or the picker's EntryLevel makes, for the same reason: those are two
 * separate stores and only one of them holds a number for any given icon.
 *
 * @param leftButton which button's bindings to search. The two have separate hotkey arrays, and
 * asking the wrong one labels a well with a key that fires the other one.
 * @param hotkeyFallback drawn top-right when the occupant holds none of F1-F8. A well can also be
 * bound to the vanilla QuickSpell9-12 keymapper rows, which are named by the ini rather than by
 * their slot number; the caller that already knows those names passes one here rather than this
 * file learning to read the keymapper.
 */
void DrawWellBadges(const Surface &out, Rectangle net, SpellID spell, bool leftButton,
    string_view hotkeyFallback = {});

/**
 * @brief The RMB well's counterpart, drawn when no spell is readied - which IS the basic attack.
 *
 * Called from DrawSpell rather than from the plate's draw block, because unlike the LMB well this
 * one is only the attack some of the time.
 */
void DrawRmbSkillWell(const Surface &out);

} // namespace devilution::oracool
