/**
 * @file oracool/auras.h
 *
 * Oracool: the Paladin's 24 auras - names, descriptions, tiers and icons.
 *
 * This is DATA ONLY. Nothing here applies an effect to the player: the auras are listed, described
 * and unlocked by level, but selecting one does not yet do anything in combat. The gameplay design
 * is written up separately in the vault (see "Paladin Auras - Gameplay Implementation Plan") and is
 * deliberately a later pass, because it touches damage, resistances, attack speed and monster AI,
 * none of which should ride along with a UI change.
 *
 * The one rule the design settles on and which this file already encodes: only ONE aura is active
 * at a time, with no duration. See ActiveAura on Player.
 */
#pragma once

#include <cstddef>
#include <cstdint>

#include "utils/stdcompat/string_view.hpp"

namespace devilution {

struct Player;

namespace oracool {

/**
 * @brief The 24 auras, in the order of the icon sheet (ui\aura_icons.png).
 *
 * Enum order IS icon order - GetAuraIconIndex is the identity - so the sheet and this list cannot
 * drift. Adding an aura means adding an icon at the matching position in the strip.
 */
enum class Aura : uint8_t {
	Might,
	FIRST = Might,
	Defense,
	Vigor,
	Fanaticism,
	Regeneration,
	Resistance,

	Defiance,
	HolyFire,
	HolyShock,
	HolyFreeze,
	Conviction,
	Sanctuary,

	Retribution,
	Purge,
	LifeAura,
	ShieldAura,
	Focus,
	AuraMastery,

	Cleanse,
	Blessing,
	Swiftness,
	Endurance,
	Vigilance,
	Righteousness,
	LAST = Righteousness,

	None = 0xFF,
};

constexpr size_t AuraCount = 24;

/**
 * @brief Auras unlock in four tiers by character level, rather than by spending points.
 *
 * Chosen to match Diablo's own philosophy - the game has no skill tree, and bolting one on would
 * sit oddly next to spell levels. Six auras per tier, which is also exactly one row of the icon
 * sheet's 6x4 grid.
 */
enum class AuraTier : uint8_t {
	Initiate,  // level 1+
	Templar,   // level 8+
	Crusader,  // level 16+
	Champion,  // level 24+
};

struct AuraData {
	/** Untranslated; run through _() at the point of display. */
	const char *name;
	/** One sentence, kept short enough to wrap to two lines in the Abilities window. */
	const char *description;
	AuraTier tier;
};

const AuraData &GetAuraData(Aura aura);

/** @brief Character level at which @p tier's auras become available. */
int GetAuraTierMinLevel(AuraTier tier);

/** @brief Display name of a tier, for the Abilities window's grouping. */
string_view GetAuraTierName(AuraTier tier);

/** @brief Whether @p player is high enough level to use @p aura. */
bool IsAuraUnlocked(const Player &player, Aura aura);

/** @brief Whether this class has auras at all. Paladin only, by design. */
bool ClassHasAuras(const Player &player);

/** @brief Index of @p aura's icon in ui\aura_icons.png - the identity, see the enum's note. */
inline int GetAuraIconIndex(Aura aura)
{
	return static_cast<int>(aura);
}

// ---- Phase 2 Stage 1: activation and effects (see the vault's aura implementation plan) ----

struct ItemBonusTotals;

/** @brief The active aura, decoded from the Player's raw byte. None when nothing is on. */
Aura GetActiveAura(const Player &player);

/**
 * @brief The click rule: activates @p aura, or clears it if it is already the active one.
 * Refuses (returns false, changes nothing) for the wrong class or a locked tier. The caller owns
 * the recalculation (CalcPlrInv) - the same split every other state-setter here uses.
 */
bool ToggleAura(Player &player, Aura aura);

/**
 * @brief Contributes @p aura's standing effects onto @p totals, scaled by character level (aura
 * levels are Stage 2). The fourteen accumulator-shaped auras act; the rest are silent until their
 * own stages (pulses, monster-facing, movement). Aura::None contributes nothing.
 */
void ApplyAuraToTotals(Aura aura, int characterLevel, ItemBonusTotals &totals);

/**
 * @brief The aura at display position @p index, ordered by unlock level then by name.
 *
 * The DISPLAY order and the ICON order are deliberately different things. The enum follows the art
 * sheet, because that is what makes GetAuraIconIndex the identity and keeps the two impossible to
 * desync. The Abilities window shows them by tier instead (user request), which is a property of
 * the list rather than of the asset - so it is a lookup here, not a reordering of the enum.
 */
Aura GetAuraAtDisplayIndex(size_t index);

} // namespace oracool
} // namespace devilution
