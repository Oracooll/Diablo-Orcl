/**
 * @file oracool/frame_blend.cpp
 *
 * Smooth animation - see frame_blend.h.
 */
#include "oracool/frame_blend.h"

#include <SDL.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <vector>

#include "diablo.h"
#include "engine/animationinfo.h"
#include "engine/surface.hpp"
#include "monster.h"
#include "multi.h"
#include "options.h"
#include "oracool/chill.h"
#include "oracool/companion.h"
#include "player.h"

// The mix walks every pixel under each hero and monster, from the Debug build: optimised even there (see colour_light.cpp).
#ifdef _MSC_VER
#pragma runtime_checks("", off)
#pragma optimize("gt", on)
#endif

namespace devilution::oracool {

namespace {

/** Below this the next frame would not show: no second draw. */
constexpr uint16_t MinAlpha = 12;

/** The last state each animation was seen in, and when it last changed - a stalled animation is never blended. */
struct SeenState {
	const uint8_t *sheet = nullptr;
	int8_t frame = -1;
	int8_t counter = -1;
	int8_t frames = -1;
	uint32_t changedAt = 0;
};
std::array<SeenState, MaxMonsters> SeenMonsters {};
std::array<SeenState, MAX_PLRS> SeenPlayers {};

bool StillAdvancing(SeenState &seen, const AnimationInfo &anim)
{
	const uint8_t *sheet = anim.sprites ? anim.sprites->data() : nullptr;
	const uint32_t now = SDL_GetTicks();
	if (seen.sheet != sheet || seen.frame != anim.currentFrame || seen.counter != anim.tickCounterOfCurrentFrame || seen.frames != anim.numberOfFrames) {
		// A new sheet is a new animation, not an advance: it has to show it moves before it blends.
		const bool advance = seen.sheet == sheet && seen.frames == anim.numberOfFrames;
		seen.sheet = sheet;
		seen.frame = anim.currentFrame;
		seen.counter = anim.tickCounterOfCurrentFrame;
		seen.frames = anim.numberOfFrames;
		seen.changedAt = advance ? (now == 0 ? 1 : now) : 0;
	}
	if (seen.changedAt == 0)
		return false;
	const uint32_t window = std::max<uint32_t>(gnTickDelay, 1) * 3 / 2;
	return now - seen.changedAt <= window;
}

struct Kept {
	int x = 0, y = 0, w = 0, h = 0;
	std::vector<uint32_t> pixels;
	bool active = false;
};
Kept Keep;

} // namespace

uint16_t FrameBlendAlphaForFraction(uint8_t fraction)
{
	const int alpha = static_cast<int>(fraction) * 256 / AnimationInfo::baseValueFraction;
	return static_cast<uint16_t>(alpha < MinAlpha || alpha >= 256 ? 0 : alpha);
}

std::optional<FrameBlendPlan> PlanAnimationFrameBlend(const AnimationInfo &anim)
{
	if (!anim.sprites || anim.isPetrified || anim.numberOfFrames <= 1 || anim.ticksPerFrame <= 0)
		return std::nullopt;
	const int frame = anim.getFrameToUseForRendering();
	// The last frame is never faded into the first: a looping walk wraps there, but an attack, a hit or a death hands over
	// to whatever animation the game picks next.
	if (frame < 0 || frame + 1 >= anim.numberOfFrames || static_cast<uint32_t>(frame + 1) >= anim.sprites->numSprites())
		return std::nullopt;
	const uint16_t alpha = FrameBlendAlphaForFraction(anim.getFrameBlendFraction());
	if (alpha == 0)
		return std::nullopt;
	return FrameBlendPlan { (*anim.sprites)[static_cast<size_t>(frame + 1)], alpha };
}

std::optional<FrameBlendPlan> PlanMonsterFrameBlend(const Surface &out, const Monster &monster)
{
	if (!*sgOptions.Oracool.animationBlending || out.isIndexed())
		return std::nullopt;
	if (monster.mode == MonsterMode::Petrified || (monster.flags & (MFLAG_LOCK_ANIMATION | MFLAG_ALLOW_SPECIAL)) != 0)
		return std::nullopt;
	if (IsMonsterFrozen(monster) || CompanionSpinSprite(monster))
		return std::nullopt;
	const size_t id = monster.getId();
	if (id >= SeenMonsters.size() || !StillAdvancing(SeenMonsters[id], monster.animInfo))
		return std::nullopt;
	return PlanAnimationFrameBlend(monster.animInfo);
}

std::optional<FrameBlendPlan> PlanPlayerFrameBlend(const Surface &out, const Player &player)
{
	if (!*sgOptions.Oracool.animationBlending || out.isIndexed() || player.previewCelSprite)
		return std::nullopt;
	const size_t id = player.getId();
	if (id >= SeenPlayers.size() || !StillAdvancing(SeenPlayers[id], player.AnimInfo))
		return std::nullopt;
	return PlanAnimationFrameBlend(player.AnimInfo);
}

void BeginFrameBlend(const Surface &out, Point bottomLeft, ClxSprite next)
{
	Keep.active = false;
	if (out.isIndexed())
		return;
	const int x0 = std::max(bottomLeft.x, 0);
	const int x1 = std::min(bottomLeft.x + static_cast<int>(next.width()), out.w());
	const int y0 = std::max(bottomLeft.y - static_cast<int>(next.height()) + 1, 0);
	const int y1 = std::min(bottomLeft.y + 1, out.h());
	if (x0 >= x1 || y0 >= y1)
		return;
	Keep.x = x0;
	Keep.y = y0;
	Keep.w = x1 - x0;
	Keep.h = y1 - y0;
	Keep.pixels.resize(static_cast<size_t>(Keep.w) * Keep.h);
	for (int y = 0; y < Keep.h; y++)
		std::copy_n(out.at<uint32_t>(Keep.x, Keep.y + y), Keep.w, Keep.pixels.data() + static_cast<size_t>(y) * Keep.w);
	Keep.active = true;
}

void EndFrameBlend(const Surface &out, uint16_t alpha)
{
	if (!Keep.active)
		return;
	Keep.active = false;
	const uint32_t a = alpha > 256 ? 256 : alpha;
	const uint32_t b = 256 - a;
	for (int y = 0; y < Keep.h; y++) {
		uint32_t *row = out.at<uint32_t>(Keep.x, Keep.y + y);
		const uint32_t *kept = Keep.pixels.data() + static_cast<size_t>(y) * Keep.w;
		for (int x = 0; x < Keep.w; x++) {
			const uint32_t now = row[x];
			const uint32_t was = kept[x];
			if (now == was)
				continue;
			const uint32_t r = (((now >> 16) & 0xFF) * a + ((was >> 16) & 0xFF) * b) >> 8;
			const uint32_t g = (((now >> 8) & 0xFF) * a + ((was >> 8) & 0xFF) * b) >> 8;
			const uint32_t bl = ((now & 0xFF) * a + (was & 0xFF) * b) >> 8;
			row[x] = (r << 16) | (g << 8) | bl;
		}
	}
}

bool InFrameBlendPass()
{
	return Keep.active;
}

} // namespace devilution::oracool

#ifdef _MSC_VER
#pragma optimize("", on)
#pragma runtime_checks("", restore)
#endif
