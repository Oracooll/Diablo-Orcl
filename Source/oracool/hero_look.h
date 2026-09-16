#pragma once
/**
 * @file oracool/hero_look.h
 *
 * Oracool: a class's own look on a borrowed body.
 *
 * The Barbarian wears the Warrior's CL2s (GetPlayerSpriteClass), and until he has sheets of his own
 * the cheap way to make him read as someone else is to change the body at LOAD time: a dye baked
 * into the palette indices, the way LoadPlrGFX already bakes the class TRN, and the sheet blown up.
 * Both cost nothing per frame and touch no art.
 *
 * The dye works because the light-armour Warrior is drawn from clean 8-entry palette ramps, one per
 * body part (measured on the exported sheets, 2026-09-16, tools/oracool_sprite_export.cpp
 * --dump-palette): trousers 184-191 (blue), mail 240-255 (grey), boots and belt 216-223 (brown),
 * gloves 168-175 (dark red), hair 206-207 (the dark end of the skin ramp 200-207, so the face is
 * separable). A ramp-to-ramp table therefore recolours one garment and nothing else - except where
 * two things share a ramp, which is noted at the table.
 */

#include <cstdint>

namespace devilution {

struct Player;
enum class HeroClass : uint8_t;

namespace oracool {

/**
 * @brief How large this class's body sheets are drawn, in percent of the original. 100 = untouched.
 *
 * User, 2026-09-16: "Can we make barb sprite 20% larger than warrior?"
 */
int SpriteScalePercent(HeroClass heroClass);

/**
 * @brief The index translation to bake into the sheet LoadPlrGFX is loading for @p player, or nullptr
 * for none. Reads the class and the armour tier the sheet is for (`_pgfxnum >> 4`), which is why it
 * takes the player and not just the class.
 */
const uint8_t *HeroDyeTrn(const Player &player);

} // namespace oracool
} // namespace devilution
