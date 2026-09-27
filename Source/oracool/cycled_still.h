/**
 * @file oracool/cycled_still.h
 *
 * One still picture, animated by colour cycling (v1.12.211, the user's animation review, 2026-09-27: "we can draw one
 * beautiful looking static image and animate it through colorcycling"). The same two bands of light the aura rings run
 * (aura_ground.cpp's AuraCycleFactors: one lap every 2.4 s, a 35% swing), either round the picture - measured on the
 * ellipse, about the mean of its opaque pixels - or up and down its height. 32-bit targets only: an indexed screen has no
 * colours to cycle, and draws nothing.
 */
#pragma once

#include "engine/rectangle.hpp"
#include "engine/surface.hpp"

namespace devilution::oracool {

/** @brief Which part of the picture to draw: a ring round a body goes half behind it and half in front. */
enum class StillPart : uint8_t {
	Whole,
	/** The rows above the picture's centre - the far side of a ring, drawn before the body. */
	Back,
	/** The rows from its centre down - the near side, drawn over the body. */
	Front,
};

enum class CycleShape : uint8_t {
	/** The bands run round the picture: a mantra's ring, Serenity's. */
	Ring,
	/** The bands run up and down its height: a rod, a pylon. */
	Vertical,
};

/**
 * @brief Draws the PNG at archive path @p path into @p dst (scaled nearest-neighbour when the sizes differ), colour-cycled
 * by @p shape, composited with its own alpha up to @p opacityPercent - all of it, or the @p part behind or in front of
 * its centre. False when the picture is missing or the target is indexed. The picture is loaded once and kept.
 */
bool DrawCycledStill(const Surface &out, const char *path, Rectangle dst, CycleShape shape, int opacityPercent = 100,
    StillPart part = StillPart::Whole);

} // namespace devilution::oracool
