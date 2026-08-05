#include "oracool/save_indicator.h"

#include <SDL.h>

#include "DiabloUI/ui_flags.hpp"
#include "engine/render/text_render.hpp"

namespace devilution::oracool {

namespace {

bool IndicatorActive = false;
uint32_t IndicatorStartTime = 0;

// Blink twice, then stop drawing entirely - a quick flash rather than a static banner, since the
// whole point is to be less intrusive than the vanilla "Game Saved" message it replaces here.
constexpr uint32_t TotalDurationMs = 900;
constexpr uint32_t BlinkPeriodMs = 150;

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

	if ((elapsed / BlinkPeriodMs) % 2 != 0)
		return;

	DrawString(out, "Saved", Rectangle { { 8, 8 }, { 80, 16 } }, { UiFlags::ColorWhite | UiFlags::FontSize12 });
}

} // namespace devilution::oracool
