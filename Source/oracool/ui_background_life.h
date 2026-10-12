/**
 * @file oracool/ui_background_life.h
 *
 * The front end's paintings alive (user, 2026-10-13: "can you use the 32bit renderer to add subtle animation on the front end
 * backgrounds - like animating flames, subtly moving flags and similar stuff?"). Each painting keeps a short list of spots
 * measured off it - flames, banners, smoke, a portal, the sky, water, glowing windows - and every frame those spots of the
 * painting already on the screen are bent, flickered and lit: a flame wavers and sheds embers, a banner sways from its top,
 * the clouds churn, a puddle shimmers. Nothing is repainted; the painting moves. 32-bit only.
 */
#pragma once

#include "engine/rectangle.hpp"
#include "oracool/ui_backgrounds.h"

namespace devilution {
struct Surface;
} // namespace devilution

namespace devilution::oracool {

/**
 * @brief Brings @p which to life on @p out, where the painting's @p crop (its own pixels) was drawn over @p screen.
 * @p seconds: a steady clock. Does nothing on an indexed surface or for a painting with no spots.
 */
void AnimateUiBackground(const Surface &out, UiBackground which, const Rectangle &crop, const Rectangle &screen, float seconds);

} // namespace devilution::oracool
