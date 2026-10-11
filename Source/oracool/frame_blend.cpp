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
#include "oracool/cold.h"
#include "oracool/companion.h"
#include "oracool/rift_bosses.h"
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

/** The box both frames cover, what lay under them before either was drawn, and the first frame's picture. */
struct Kept {
	int x = 0, y = 0, w = 0, h = 0;
	std::vector<uint32_t> under;
	std::vector<uint32_t> first;
	bool active = false;
};
Kept Keep;

void CopyOut(const Surface &out, std::vector<uint32_t> &to)
{
	to.resize(static_cast<size_t>(Keep.w) * Keep.h);
	for (int y = 0; y < Keep.h; y++)
		std::copy_n(out.at<uint32_t>(Keep.x, Keep.y + y), Keep.w, to.data() + static_cast<size_t>(y) * Keep.w);
}

void CopyIn(const Surface &out, const std::vector<uint32_t> &from)
{
	for (int y = 0; y < Keep.h; y++)
		std::copy_n(from.data() + static_cast<size_t>(y) * Keep.w, Keep.w, out.at<uint32_t>(Keep.x, Keep.y + y));
}

} // namespace

uint16_t FrameBlendAlphaForFraction(uint8_t fraction)
{
	const int alpha = static_cast<int>(fraction) * 256 / AnimationInfo::baseValueFraction;
	// A due tick (the fraction at its whole) shows the next frame whole, not the one it leaves (audit round 1).
	return static_cast<uint16_t>(alpha < MinAlpha ? 0 : (alpha > 256 ? 256 : alpha));
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
	// A chill and a slow rift boss skip game ticks (monster.cpp ProcessMonsters): an animation that moves only on some
	// ticks would fade toward its next frame and snap back on each skipped one (audit round 1).
	if (IsMonsterFrozen(monster) || IsMonsterChilled(monster) || ActiveRiftBossFor(monster) != nullptr || CompanionSpinSprite(monster))
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
	if (IsPlayerChilled(player)) // a chilled hero's attacks and casts skip every other tick (player.cpp ProcessPlayers)
		return std::nullopt;
	const size_t id = player.getId();
	if (id >= SeenPlayers.size() || !StillAdvancing(SeenPlayers[id], player.AnimInfo))
		return std::nullopt;
	return PlanAnimationFrameBlend(player.AnimInfo);
}

void BeginFrameBlend(const Surface &out, Point firstBottomLeft, ClxSprite firstSprite, Point nextBottomLeft, ClxSprite next)
{
	Keep.active = false;
	if (out.isIndexed())
		return;
	// The union of the two frames' boxes, clipped to the surface.
	int x0 = std::min(firstBottomLeft.x, nextBottomLeft.x);
	int x1 = std::max(firstBottomLeft.x + static_cast<int>(firstSprite.width()), nextBottomLeft.x + static_cast<int>(next.width()));
	int y0 = std::min(firstBottomLeft.y - static_cast<int>(firstSprite.height()) + 1, nextBottomLeft.y - static_cast<int>(next.height()) + 1);
	int y1 = std::max(firstBottomLeft.y, nextBottomLeft.y) + 1;
	x0 = std::max(x0, 0);
	y0 = std::max(y0, 0);
	x1 = std::min(x1, out.w());
	y1 = std::min(y1, out.h());
	if (x0 >= x1 || y0 >= y1)
		return;
	Keep.x = x0;
	Keep.y = y0;
	Keep.w = x1 - x0;
	Keep.h = y1 - y0;
	CopyOut(out, Keep.under);
	Keep.active = true;
}

void SwapFrameBlend(const Surface &out)
{
	if (!Keep.active)
		return;
	CopyOut(out, Keep.first);
	CopyIn(out, Keep.under);
}

void EndFrameBlend(const Surface &out, uint16_t alpha)
{
	if (!Keep.active)
		return;
	Keep.active = false;
	if (Keep.first.size() != static_cast<size_t>(Keep.w) * Keep.h)
		return; // no swap: the second draw is all there is
	const uint32_t a = alpha > 256 ? 256 : alpha;
	const uint32_t b = 256 - a;
	for (int y = 0; y < Keep.h; y++) {
		uint32_t *row = out.at<uint32_t>(Keep.x, Keep.y + y);
		const uint32_t *first = Keep.first.data() + static_cast<size_t>(y) * Keep.w;
		for (int x = 0; x < Keep.w; x++) {
			const uint32_t now = row[x];
			const uint32_t was = first[x];
			if (now == was)
				continue;
			const uint32_t r = (((now >> 16) & 0xFF) * a + ((was >> 16) & 0xFF) * b) >> 8;
			const uint32_t g = (((now >> 8) & 0xFF) * a + ((was >> 8) & 0xFF) * b) >> 8;
			const uint32_t bl = ((now & 0xFF) * a + (was & 0xFF) * b) >> 8;
			row[x] = (r << 16) | (g << 8) | bl;
		}
	}
	Keep.first.clear();
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
