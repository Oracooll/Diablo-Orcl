#include "oracool/save_indicator.h"

#include <SDL.h>

#include "DiabloUI/ui_flags.hpp"
#include "engine/render/text_render.hpp"

namespace devilution::oracool {

namespace {

bool IndicatorActive = false;
uint32_t IndicatorStartTime = 0;

// Oracool: user request - a static gold "Game Saved" sign for a full second, replacing the
// original quick blink-twice-and-vanish flash.
constexpr uint32_t TotalDurationMs = 1000;

} // namespace

void TriggerSaveIndicator()
{
	IndicatorActive = true;
	IndicatorStartTime = SDL_GetTicks();
}

void DrawSaveIndicator(const Surface &out)
{
	if (!IndicatorActive)
		return;

	const uint32_t elapsed = SDL_GetTicks() - IndicatorStartTime;
	if (elapsed >= TotalDurationMs) {
		IndicatorActive = false;
		return;
	}

	// Under the clock, not on it (audit, 2026-09-27): the clock is at {8, 8} too, and the two words overlapped for a
	// second after every kill's autosave.
	// Beside the speed readout, not under it (round 27 audit): the readout's band is {8, 28, 72x16}, and "x20" and "Game
	// Saved" printed on top of each other; the companion header starts at 50, so the row is shared.
	DrawString(out, "Game Saved", Rectangle { { 88, 28 }, { 100, 16 } }, { UiFlags::VerticalCenter | UiFlags::ColorGold | UiFlags::FontSize12 });
}

} // namespace devilution::oracool
