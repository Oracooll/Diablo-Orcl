/**
 * @file oracool/sprite_scale.h
 *
 * Oracool: Megaplan Phase 0.6 - the load-time CLX sprite scaler.
 *
 * Palette indices cannot be interpolated (entry 143 is not "between" 142 and 144), so scaling in
 * an 8-bit engine is nearest-neighbour resampling - cheap to do ONCE when a sheet loads, wasteful
 * to redo per frame on the CPU renderer. This module produces real scaled CLX data, so the
 * renderer, hit-testing and clipping all see honest width/height values and NOTHING downstream
 * changes: a scaled monster is just a monster whose sprites are bigger.
 *
 * Transparency is preserved exactly by decoding the CL2 runs themselves rather than rendering
 * through a colour key - index 0 is a real, OPAQUE colour in this art (the baked-in shadows;
 * see ClxDrawOutlineSkipColorZero's doc), so any key-based decode would punch holes in every
 * shadow. Re-encoding picks, per list, a palette index the art never uses opaquely.
 *
 * Phase 3 wires this into monster variants (Colossal lesser-uniques, swarm runts, boss
 * silhouettes); until then it is a tool with tests.
 */
#pragma once

#include "engine/clx_sprite.hpp"

namespace devilution::oracool {

/**
 * @brief Nearest-neighbour rescale of every sprite in @p src to @p percent of its size.
 *
 * Sane inputs only: percent in [25, 400]. Each sprite's dimensions are scaled independently and
 * floored at 1x1. Frames keep their order; the result is a standalone owned list.
 */
OwnedClxSpriteList ScaleClxList(ClxSpriteList src, unsigned percent);

/** @brief ScaleClxList over every direction list of a sheet. */
OwnedClxSpriteSheet ScaleClxSheet(ClxSpriteSheet src, unsigned percent);

/**
 * @brief @p src at @p percent of its size, played forward and back and turning as it plays: @p frames sprites, where
 * sprite k is source frame k along the ping-pong (0, 1 .. last .. 1, 0 ..) turned by @p turns whole turns times k /
 * @p frames. Every sprite is one square canvas, the turned frame centred in it. Nearest-neighbour, like ScaleClxList.
 * Built for Earthquake's spinning flare (the Barbarian Skill Cards page, 2026-09-29).
 */
OwnedClxSpriteList SpinPingPongClxList(ClxSpriteList src, unsigned percent, unsigned frames, unsigned turns);

} // namespace devilution::oracool
