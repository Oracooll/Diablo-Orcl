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
#include <memory>

namespace devilution {

struct Player;
enum class HeroClass : uint8_t;

namespace oracool {

struct SpriteColours;

/**
 * @brief How large this class's body sheets are drawn, in percent of the original. 100 = untouched.
 *
 * User, 2026-09-16: "Can we make barb sprite 20% larger than warrior?"
 */
int SpriteScalePercent(HeroClass heroClass);

/**
 * @brief The dye as an INDEX translation, or nullptr for none: each dyed index to the nearest palette
 * entry. Since 2026-09-17 this is only the fallback - what an 8-bit target draws, and what each dyed
 * colour is shaded like - and no longer baked into the sheet; see HeroColours. Reads the class and the
 * armour tier (`_pgfxnum >> 4`), which is why it takes the player and not just the class.
 */
const uint8_t *HeroDyeTrn(const Player &player);

/**
 * @brief The dye as COLOURS, or null for none - what LoadPlrGFX hangs on each sheet it loads for @p player
 * and DrawPlayer draws through. Indices stay the Warrior's own; only what they mean changes, so the
 * mail's sixteen greys become sixteen blues rather than eight doubled, and grey hair is a silver the
 * palette does not have. See oracool/sprite_colours.h.
 */
std::shared_ptr<const SpriteColours> HeroColours(const Player &player);

/** @brief The same from the two things it depends on - for the hero-select screen, which has no Player to ask. */
std::shared_ptr<const SpriteColours> HeroColoursFor(HeroClass heroClass, uint8_t gfxnum);

} // namespace oracool
} // namespace devilution
