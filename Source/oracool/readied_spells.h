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

} // namespace oracool
} // namespace devilution
