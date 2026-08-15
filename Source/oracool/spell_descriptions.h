/**
 * @file oracool/spell_descriptions.h
 *
 * Oracool: one sentence per spell, for the Abilities window's hover panel.
 *
 * THESE ARE AUTHORED. The engine has no spell description text of any kind - no field, no table,
 * nothing - because the original game never showed one; it gave you a name, a mana cost and a spell
 * level and left the rest to the manual. So none of this is recovered from Diablo. It is written to
 * match what each spell actually does in THIS codebase, checked against the missile each one fires
 * rather than against memory of the game, and it is meant to be edited freely.
 *
 * Three entries say nothing rather than promise something. Doom Serpents, Blood Ritual and
 * Invisibility carry MissileID::Null in both missile slots - casting them does not do anything at
 * all. They are in the book on the user's explicit call (2026-08-15) with substance to follow, and
 * until it follows a description claiming an effect would just be a lie the player can test.
 */
#pragma once

#include "spelldat.h"
#include "utils/stdcompat/string_view.hpp"

namespace devilution {
namespace oracool {

/**
 * @brief One sentence on what @p spell does, untranslated. Empty string for SpellID::Null.
 *
 * `const char *` rather than string_view specifically so it can be handed straight to _() at the
 * point of display, the way every other description table here is used.
 */
const char *GetSpellDescription(SpellID spell);

} // namespace oracool
} // namespace devilution
