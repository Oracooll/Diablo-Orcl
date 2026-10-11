/**
 * @file oracool/frame_blend.h
 *
 * Smooth animation (user, 2026-10-11: one of the second graphics batch). Diablo animates at the game's 20 ticks a
 * second; between two ticks a hero or monster is drawn as its frame cross-faded into the next one, by how far the
 * animation is toward it (AnimationInfo::getFrameBlendFraction), so walking and fighting move at the screen's rate.
 *
 * Generic over every way a sprite is drawn (lit, recoloured, glazed, frozen...): what lies under both frames is kept, the
 * draw runs as usual, its picture is kept and the ground put back, the same draw runs with the next frame, and the two
 * pictures are mixed - so the frame left behind fades out as the next fades in (audit round 1). An
 * animation that has stopped advancing (paused, frozen, petrified, a golem holding its still) is never blended: the state
 * it was last seen in must have changed within a tick and a half, and an animation that skips game ticks (a chill, a slow
 * rift boss) is left as it was. 32-bit only; off with the option.
 */
#pragma once

#include <cstdint>
#include <optional>

#include "engine/clx_sprite.hpp"
#include "engine/point.hpp"

namespace devilution {
struct Surface;
struct Monster;
struct Player;
class AnimationInfo;
} // namespace devilution

namespace devilution::oracool {

/** @brief The next frame to fade in and how much of it, 0..256. */
struct FrameBlendPlan {
	ClxSprite next;
	uint16_t alpha;
};

/** @brief The blend for @p monster this frame, or none (option off, 8-bit, stalled, last frame, spinning...). */
std::optional<FrameBlendPlan> PlanMonsterFrameBlend(const Surface &out, const Monster &monster);

/** @brief The blend for @p player this frame, or none. */
std::optional<FrameBlendPlan> PlanPlayerFrameBlend(const Surface &out, const Player &player);

/**
 * @brief A true cross-fade, in three steps round the two draws: BeginFrameBlend keeps what lies under both frames (the
 * first drawn at @p firstBottomLeft, the next at @p nextBottomLeft); the first frame is drawn; SwapFrameBlend keeps that
 * picture and puts back what lay under it; the next frame is drawn; EndFrameBlend mixes the two pictures.
 */
void BeginFrameBlend(const Surface &out, Point firstBottomLeft, ClxSprite firstSprite, Point nextBottomLeft, ClxSprite next);
void SwapFrameBlend(const Surface &out);
/** @brief Mixes the first frame's picture with the next's: @p alpha of 256 is the next alone. */
void EndFrameBlend(const Surface &out, uint16_t alpha);

/** @brief True between BeginFrameBlend and EndFrameBlend. */
bool InFrameBlendPass();

/** @brief Test hook: the blend's alpha for an animation's fraction (0..AnimationInfo::baseValueFraction), 0 = none. */
uint16_t FrameBlendAlphaForFraction(uint8_t fraction);

/** @brief Test hook: the plan for a bare animation (no stall check): the next frame and its alpha, or none. */
std::optional<FrameBlendPlan> PlanAnimationFrameBlend(const AnimationInfo &anim);

} // namespace devilution::oracool
