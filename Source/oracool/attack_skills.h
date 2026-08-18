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
 * @brief The basic-attack quick list - the horizontal icon strip that pops above a skill well.
 *
 * Oracool: user request (2026-08-18) - "bring back the vanilla quick skill/quick spell lists which
 * appear as horizontal icon list when i click on LMB/RMB. That was D2 legacy behaviour. BUT we put
 * in this list only Fist/Regular attacks."
 *
 * It carries the two attacks and nothing else. Everything with a rank, a level gate or an icon of
 * its own is chosen in the Abilities window; this strip exists for the one ability that is never
 * listed there because it is never earned - the swing you always have. It is also the ONLY way back
 * to a plain attack now that an unbuilt tree cell no longer hands one out (1.7.89).
 *
 * Both entries select the same state. SpellID::Invalid with SpellType::Invalid IS the basic attack -
 * that is precisely the state in which a click swings rather than casts - and whether the well then
 * shows a fist or crossed swords is decided by what is in the player's hand, not by which entry was
 * picked. They are shown as a pair because that is what the player sees on the two wells, not
 * because they are two different choices.
 */
void OpenAttackQuickList(bool forLeftButton);

/** @brief Whether the quick list is showing. While it is, the wells do not answer clicks. */
bool IsAttackQuickListOpen();

/** @brief Shuts it, without selecting anything. */
void CloseAttackQuickList();

/** @brief Draws the strip. No-op when closed. */
void DrawAttackQuickList(const Surface &out);

/**
 * @brief Handles a click while the strip is open. True if the click was the strip's.
 *
 * A click on an entry readies the basic attack on whichever button opened the list; a click
 * anywhere else just closes it, which is how every other popup in this fork behaves.
 */
bool CheckAttackQuickListClick();

/**
 * @brief The RMB well's counterpart, drawn when no spell is readied - which IS the basic attack.
 *
 * Called from DrawSpell rather than from the plate's draw block, because unlike the LMB well this
 * one is only the attack some of the time.
 */
void DrawRmbSkillWell(const Surface &out);

} // namespace devilution::oracool
