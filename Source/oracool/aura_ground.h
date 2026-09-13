/**
 * @file oracool/aura_ground.h
 *
 * Oracool: the ring of light a lit aura casts on the floor.
 *
 * In Diablo II a Paladin with an aura burning stands inside a glowing ellipse. It is how the player
 * knows the aura is on, and which one. This fork had twenty auras and no such effect at all - an
 * active aura was visible only as a lit icon in the interface.
 *
 * The art is thirty 512x256 RGBA images (`ui\aura_*.png`), one per aura, commissioned to the brief
 * in `01-Project-Overview/Asset Brief - Paladin Aura Ground Effects.md` - the Paladin's twenty, then
 * the Bard's nine songs and the Monk's Healing Mantra (batch 8, 2026-09-11). They arrive already
 * quantised to the shared palette - measured error zero against every entry in 128-255 - which is
 * what lets them go into the world at all.
 *
 * ## Why this is not just a blit
 *
 * Two problems the ordinary HUD art path cannot solve.
 *
 * **Alpha.** The render target is 8-bit indexed. `oracool/hud_art.cpp` treats alpha as a yes/no at
 * 128, which is right for an interface plate and wrong for a glow: it would turn a soft gradient
 * into a hard-edged blob wherever it crossed the threshold, and delete everything softer. What the
 * engine actually has is `paletteTransparencyLookup`, a fixed FIFTY per cent blend of two indices.
 * Applying it once gives 50%, twice 75%, and nothing in between - so the in-between is dithered,
 * with a 4x4 ordered matrix choosing per pixel which of the two neighbouring levels to use. At the
 * distances these are seen from that reads as a smooth ramp.
 *
 * **Depth.** A ground decal spanning sixteen tiles cannot be drawn by the per-tile renderer, which
 * interleaves entities with terrain for depth sorting. It is drawn between the two passes instead -
 * after every floor tile, before any wall, monster or player - so everything in the world stands on
 * top of it, which is what "on the floor" means.
 *
 * ## Why it walks the tiles instead of computing a position
 *
 * DrawAuraGround takes the same four arguments as DrawFloor and repeats its walk exactly, drawing
 * when it reaches the player's tile. Deriving the screen position independently would mean a second
 * copy of the isometric mapping, and this project has already been bitten twice by two copies of
 * one bound drifting apart. Same walk, same answer, by construction.
 */
#pragma once

#include "engine/point.hpp"
#include "engine/surface.hpp"
#include "oracool/class_tree.h"

namespace devilution::oracool {

/**
 * @brief The file id of @p aura's ground ring (`ui\aura_<id>.png`), or nullptr if the table has none.
 *
 * Exposed for the audit that every Kind::Aura row has a ring - RfA-12 added 27 auras and songs that
 * burned with no ring, and nothing noticed, because a missing ring is only a verbose log line.
 */
const char *AuraRingFileId(ClassTreeSkill aura);

/**
 * @brief Draws the lit aura's ground ring, if there is one.
 *
 * Call between DrawFloor and DrawTileContent with the same arguments both receive. Does nothing
 * when no aura is lit, when the art is missing, or when the player is not on the drawn level.
 */
void DrawAuraGround(const Surface &out, Point tilePosition, Point targetBufferPosition,
    int rows, int columns);

} // namespace devilution::oracool
