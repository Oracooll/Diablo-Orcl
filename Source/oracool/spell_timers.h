/**
 * @file oracool/spell_timers.h
 *
 * Oracool: the countdown column beside the mini-map (user, 2026-09-13: "when a spell with countdown
 * timer is active (infravision, etc...) put an 28x28px icon with blue backing of it next to the
 * minimap, 6px away from it's left border, and next to the spell icon run a countdown seconds timer.
 * if more than 1 such spells are active simultaniously make a columnd of them 6px vertically apart.").
 *
 * One row per timed effect the local hero carries: the spell's icon on a blue square whose right edge
 * sits 6px left of the mini-map's frame, and the seconds left to the left of the icon. Rows stack
 * downward from the mini-map's top edge, 6px apart. Nothing is stored here - every frame asks the
 * effects themselves (the missiles, the cries, the RfA-12 buffs) what is running and for how long.
 */
#pragma once

#include "engine/surface.hpp"

namespace devilution::oracool {

/** @brief Draws the countdown column for the local hero. Call once per frame, beside the mini-map. */
void DrawSpellTimers(const Surface &out);

} // namespace devilution::oracool
