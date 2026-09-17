#pragma once
/**
 * @file oracool/sprite_mix.h
 *
 * Oracool: a hero's sheets ASSEMBLED from the game's own - a shield or a sword from another armour tier
 * on this tier's body (user, 2026-09-17: "introduce Shield from another tier and Weapon from another
 * tier and ... assign Shield and Sword to particular item types").
 *
 * Every player sheet is one flat render, so nothing can be taken off it by asking. What makes this
 * possible is that inside one armour tier the sheets of the one-handed family - sword, mace, axe, staff,
 * and the two with a shield - are THE SAME BODY RENDER with different things in its hands (measured
 * 2026-09-17: 65-78% of pixels identical, the rest weapon, arm and dither). So:
 *
 *  - a SHIELD is what differs between "sword and shield" and "sword": subtract, keep the piece by the arm;
 *  - a BODY WITH EMPTY HANDS is a vote, pixel by pixel, across sword, mace, axe and staff - whatever at
 *    least two agree on is body, because no two of them hold the same thing;
 *  - a SWORD is what the sword sheet shows that the vote does not.
 *
 * Do that in the heavy tier and in the body's own tier, and the heavy pieces go where the own pieces
 * were. It is done at LOAD time, from the archive's CL2s, in palette-index space - no art is shipped and
 * drawing costs nothing - and only for the Warrior's sheets, the only ones measured.
 *
 * Known limits, all seen in the proofs and none hidden: each tier holds its sword at its own angle, so a
 * heavy sword sits near a light hand rather than in it; heavy armour is bulkier, so a shield lifted from
 * it leaves a pixel or two of gap against a slimmer body; the BLOCK animation has no shieldless twin to
 * subtract from and keeps its own tier's shield; unarmed and bow are other poses and are left alone.
 */

#include <cstdint>
#include <memory>
#include <optional>

#include "oracool/sprite_import.h"
#include "player.h"

namespace devilution::oracool {

/** @brief Which tier a piece of the look comes from. Own = whatever the body armour is. */
enum class LookTier : uint8_t {
	Own,
	Heavy,
};

struct GearLook {
	LookTier shield = LookTier::Own;
	LookTier sword = LookTier::Own;
};

/**
 * @brief What @p player's hands say about the look: the big shields (Kite, Tower, Gothic) are the heavy
 * tier's heater shield, the big swords (Long, Broad, Bastard, Two-Handed, Great) its longsword, decided by
 * the BASE item so a unique's own cursor changes nothing. Everything else keeps its tier's look.
 */
GearLook GearLookFor(const Player &player);

/** @brief The look as one byte, for "did it change" - CalcPlrItemVals reloads the sheets when it does. */
uint8_t GearLookCode(const Player &player);

/**
 * @brief The mixed sheet for one animation, or nullopt when there is nothing to mix (the look is the
 * tier's own, the body is already heavy, the pose family has no pieces, a sheet is missing) - the caller
 * then loads the CL2 as always.
 *
 * @param dye the class's colours, or null. Pieces brought in from another tier are moved to indices the
 * dye does not touch and given their true colour, so a white lion stays white on a dyed Barbarian - which
 * is also why the result carries colours of its own.
 */
std::optional<ColouredSpriteSheet> MixPlayerSheet(const Player &player, HeroClass spriteClass, PlayerWeaponGraphic weapon,
    const char *szCel, uint16_t frameWidth, const std::shared_ptr<const SpriteColours> &dye);

} // namespace devilution::oracool
