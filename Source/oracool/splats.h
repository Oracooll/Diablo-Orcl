/**
 * @file oracool/splats.h
 *
 * The GPU renderer plan, Stage 2: the particles and the loot beams, which the CPU draws pixel by pixel, can hand those
 * pixels to the graphics card instead (oracool/gpu_post.h). Each is a splat: a pixel, a premultiplied colour to add and
 * how much of what lies under it stays - the card blends them in order, one blend for both kinds:
 *     result = colour + under * (1 - cover)
 * An additive pixel (embers, sparks, motes, a beam) has cover 0; a covering one (ash) has its alpha. The values are the
 * CPU's own integer sums, so additive splats come out the same to the pixel; a covering one may round a step apart.
 */
#pragma once

#include <vector>

namespace devilution::oracool {

struct Splat {
	float x, y;    // the pixel (its top-left corner), in the world surface
	float r, g, b; // premultiplied, 0..1
	float cover;   // 0..1
};

/** @brief From here until EndSplatCollection the particles and the beams go into @p into instead of onto a surface. */
void BeginSplatCollection(std::vector<Splat> *into);
void EndSplatCollection();
/** @brief Where splats go now, or null when they are drawn. */
std::vector<Splat> *SplatCollection();

} // namespace devilution::oracool
