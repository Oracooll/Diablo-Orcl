/**
 * @file oracool/spell_glow.h
 *
 * Glowing Spells (user, 2026-10-10): light-emitting missiles drawn ADDITIVELY - their colours added onto what lies
 * behind them (ClxDrawRgbMapAdditive), so a fireball over a lit floor brightens it instead of covering it.
 *
 * Which missiles are light: fire, lightning, holy and magic bolts and their bursts, the cold pack's glints, the portals,
 * and the Orcl effect sheets whose file names say fire / flame / ember / lightning / storm / spark / holy / frost / nova /
 * flare / explosion... Never an arrow, a thrown weapon, an item tumble, acid, bone, blood, a curse or anything smoky
 * (smoke drawn as light would vanish), and never a sprite the missile borrowed (a charging monster's body, a creature
 * bolt's walk, a recoloured item). The missile's own colour table is kept, so every tint still applies.
 *
 * Visual only: nothing here is read by the game's rules. Off with the option, on an 8-bit surface (the golden tests),
 * and in town (daylight: a portal added onto bright grass only washes out).
 */
#pragma once

#include <cstdint>

namespace devilution {
struct Missile;
}

namespace devilution::oracool {

/** @brief The additive strength a glowing missile is drawn at (of 255). */
constexpr uint8_t SpellGlowStrength = 220;

/** @brief True when @p missile is drawn as light now: the option is on, this is not town, and its art glows. */
bool MissileGlows(const Missile &missile);

/** @brief The art rule alone (no option, no town check): whether @p missile's sprite is a light-emitting effect. */
bool MissileArtGlows(const Missile &missile);

/** @brief SpellGlowStrength scaled by the missile's own fade (Missile::oracoolAlpha of 256). */
uint8_t MissileGlowStrength(const Missile &missile);

/**
 * @brief The colour values the missile's plain (CL2, untinted) draw would have used: its caster's unique recolour, else
 * the level palette at its light level (@p lightTableIndex when the missile is lit, full light when not). The table may
 * be reused by the next call - draw with it at once.
 */
const uint32_t *MissileGlowTable(const Missile &missile, int lightTableIndex);

} // namespace devilution::oracool
