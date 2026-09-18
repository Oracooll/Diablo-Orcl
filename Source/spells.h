/**
 * @file spells.h
 *
 * Interface of functionality for casting player spells.
 */
#pragma once

#include <cstdint>

#include "player.h"

namespace devilution {

enum class SpellCheckResult : uint8_t {
	Success,
	Fail_NoMana,
	Fail_Level0,
	Fail_Busy,
};

bool IsValidSpell(SpellID spl);
bool IsValidSpellFrom(int spellFrom);
bool IsWallSpell(SpellID spl);
bool TargetsMonster(SpellID id);
int GetManaAmount(const Player &player, SpellID sn);

/**
 * @brief What @p sn would cost @p player at @p spellLevel, rather than at the level they have.
 *
 * Oracool: GetManaAmount is this with the player's own level, so the two cannot disagree. It exists
 * because mana FALLS as a spell levels - the adjustment is subtracted - and the Abilities panel
 * quotes the next level's cost beside the current one, the way Diablo II does.
 */
int GetManaAmountAtLevel(const Player &player, SpellID sn, int spellLevel);
void ConsumeSpell(Player &player, SpellID sn);
SpellCheckResult CheckSpell(const Player &player, SpellID sn, SpellType st, bool manaonly);

/**
 * @brief Clears the player's readied spell selection - the state in which a click attacks.
 * @note Will force a UI redraw in case the values actually change, so that the new spell reflects on the bottom panel.
 * @param player The player whose readied spell is to be cleared.
 */
void ClearReadiedSpell(Player &player);

/**
 * @brief Ensures the player's current readied spell is a valid selection for the character. If the current selection is
 * incompatible with the player's items and spell (for example, if the player does not currently have access to the spell),
 * the selection is cleared.
 * @note Will force a UI redraw in case the values actually change, so that the new spell reflects on the bottom panel.
 * @param player The player whose readied spell is to be checked.
 */
void EnsureValidReadiedSpell(Player &player);
void CastSpell(int id, SpellID spl, int sx, int sy, int dx, int dy, int spllvl);

/**
 * @param pnum player index
 * @param rid target player index
 */
void DoResurrect(size_t pnum, Player &target);
void DoHealOther(const Player &caster, Player &target);
int GetSpellBookLevel(SpellID s);
int GetSpellStaffLevel(SpellID s);

/**
 * @brief Gets a value that represents the specified spellID in 64bit bitmask format.
 * For example:
 *  - spell ID  1: 0000.0000.0000.0000.0000.0000.0000.0000.0000.0000.0000.0000.0000.0000.0000.0001
 *  - spell ID 43: 0000.0000.0000.0000.0000.0100.0000.0000.0000.0000.0000.0000.0000.0000.0000.0000
 * @param spellId The id of the spell to get a bitmask for.
 * @return A 64bit bitmask representation for the specified spell.
 */
constexpr SpellMask GetSpellBitmask(SpellID spellId)
{
	// Oracool, Round 2 (2026-09-03): two words now - see SpellMask. Id 1 is bit 0 of the low word,
	// exactly as it always was, so every saved mask keeps its meaning; id 65 is bit 0 of the high.
	const int index = static_cast<int>(spellId) - 1;
	// Total over the enum: Null and Invalid gave a negative shift, which is undefined behaviour,
	// and a malformed readied scroll in a save could reach it (external audit, 2026-09-06: SAV-02).
	// An empty mask is the right answer for "no spell".
	if (index < 0 || index >= 320)
		return SpellMask {};
	const uint64_t bit = 1ULL << (index % 64);
	switch (index / 64) {
	case 0:
		return SpellMask { bit, 0, 0, 0 };
	case 1:
		return SpellMask { 0, bit, 0, 0 };
	case 2:
		return SpellMask { 0, 0, bit, 0 };
	case 3:
		return SpellMask { 0, 0, 0, bit };
	default:
		return SpellMask { 0, 0, 0, 0, bit }; // ids 257-320, the fifth word (2026-09-18)
	}
}

} // namespace devilution
