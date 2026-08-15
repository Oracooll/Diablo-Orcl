/**
 * @file oracool/divine_trn.h
 *
 * Oracool: the recolour that makes a borrowed object look blessed rather than dropped.
 *
 * The Paladin's thrown shield and falling mace reuse the game's ITEM DROP animations - items\shield
 * and items\mace, the tumbles an item plays when it lands on the floor. They are the only animations
 * of an object in flight the game has, but they are painted as loot: a plain steel shield reads as
 * something you could pick up, not as something a Paladin blessed and threw.
 *
 * This is the difference. "recolor them. make them a bit shiny. lightning shiny. divine shyni."
 */
#pragma once

#include <cstdint>

namespace devilution::oracool {

/**
 * @brief A 256-entry translation table that brightens every colour toward white.
 *
 * Stable for the lifetime of the process and safe to hand to a missile, which outlives any single
 * frame. Rebuilt automatically when the level palette changes - the tables are per-palette, and a
 * table built against the wrong one is exactly the colour noise that bit the HUD art in August.
 *
 * Returns nullptr before a palette exists, which every caller treats as "draw it as it is".
 */
const uint8_t *GetDivineTrn();

} // namespace devilution::oracool
