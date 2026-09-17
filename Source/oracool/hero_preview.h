/**
 * @file oracool/hero_preview.h
 *
 * Oracool: user request (2026-08-13) - the character-select screen shows the character itself,
 * animated, instead of the class portrait and the stat block.
 *
 * The sprite is the player's own in-game artwork: `plrgfx\<class>\<caw>\<caw>st.cl2`, the TOWN stand,
 * which is the idle breathing loop rather than a static pose. It needs no Player object to load - the
 * three prefix letters (class, armour, weapon) and a frame width are the whole input, and the armour
 * and weapon letters come from `_pgfxnum`, which `pfile_ui_set_hero_infos` already computes for every
 * saved character before it fills `_uiheroinfo`. So the figure wears what the character actually
 * wears, not a generic class pose.
 *
 * The one real obstacle is the palette. CL2 pixels are palette INDICES, and a front-end screen has a
 * UI palette loaded, not a level one - drawn raw, the character comes out in the wrong colours
 * entirely. So this builds a 256-entry translation from the town palette to whatever palette the
 * screen currently has and draws through `ClxDrawTRN`, which is the same mechanism the engine already
 * uses for class tints. The table is rebuilt only if the screen's palette changes.
 */
#pragma once

#include <cstdint>

#include "engine/rectangle.hpp"
#include "engine/surface.hpp"
#include "player.h"

namespace devilution::oracool {

/**
 * @brief Loads the idle animation for @p heroClass wearing the gear @p gfxnum describes.
 *
 * Since v1.12.029 the figure is the one the GAME draws: @p gearLook (oracool::GearLookCode, saved in
 * _uiheroinfo) brings the shield or sword from another armour tier, and a class with a dye wears it. The mixed
 * sheet comes out of the same cache the game fills. A look never built before is built in the background from
 * here too - DrawHeroPreview feeds the mixer while it waits - and the plain sheet is shown until it lands.
 *
 * Cheap to call repeatedly - it is a no-op when the class and gear are already loaded, which is what
 * makes it safe to call from the list's focus handler on every arrow key.
 */
void SetHeroPreview(HeroClass heroClass, uint8_t gfxnum, uint8_t gearLook = 0);

/** @brief Drops the preview - the "no character under the cursor" state, e.g. an empty list. */
void ClearHeroPreview();

/**
 * @brief Draws the current frame, centred horizontally in @p area and standing on its bottom edge.
 *
 * Blown up by an integer factor with nearest-neighbour sampling, so a 96px sprite reads at menu scale
 * without the pixel art turning to soup. No-op when nothing is loaded or the art is missing.
 */
void DrawHeroPreview(const Surface &out, Rectangle area);

/** @brief Releases the sprite sheet. Call when the screen closes. */
void FreeHeroPreview();

} // namespace devilution::oracool
