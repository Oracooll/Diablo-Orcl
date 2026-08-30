/**
 * @file oracool/readied_spells.h
 *
 * Oracool: user request (2026-08-15) - "make the readied spells persist across saves".
 *
 * Both save formats store a readied spell as a single byte, so the encoding lives here rather than
 * being written twice. See pack.h's pReadiedSpellRight for which bytes were used and why.
 */
#pragma once

#include <cstdint>

#include "spelldat.h"

namespace devilution {

struct Player;

namespace oracool {

/**
 * @brief Encodes a readied spell into one save byte.
 *
 * 0 means "nothing readied", so the byte's pre-existing zero value already reads correctly - which
 * is the whole reason no save had to be invalidated for this.
 */
uint8_t PackReadiedSpell(SpellID spell);

/**
 * @brief Decodes one save byte back into a readied spell.
 *
 * Leaves @p spell and @p type completely untouched when the byte holds nothing usable, so a save
 * written before this existed keeps whatever the load path had already established (notably
 * CalcPlrInv auto-readying an equipped staff's charged spell).
 *
 * Requires @p player's _pAblSpells and _pMemSpells to already be loaded - the spell type is
 * re-derived from them, not stored.
 */
void UnpackReadiedSpell(const Player &player, uint8_t packed, SpellID &spell, SpellType &type);

/**
 * @brief Records @p player's two readied slots as the ones a NEW character should start with.
 *
 * V1 starts a new game rather than continuing one (see the project notes), so the per-character
 * save is the wrong place for this: it is remembered and then never read, because the character it
 * belongs to is gone. These live in the options instead - one byte each, the same encoding the save
 * uses - so the choice outlives the hero who made it (user, 2026-08-30).
 */
void RememberReadiedSpells(const Player &player);

/**
 * @brief Applies the remembered slots to a freshly created @p player, where they still make sense.
 *
 * UnpackReadiedSpell re-derives the type from what the character actually knows and leaves the slot
 * untouched when the byte holds nothing usable, so a Sorceress's Firebolt simply does not arrive on
 * a Barbarian. No class check of its own is needed, and none is written.
 */
void ApplyRememberedReadiedSpells(Player &player);

} // namespace oracool
} // namespace devilution
