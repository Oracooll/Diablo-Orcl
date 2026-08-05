#include "oracool/xp_gain_indicator.h"

#include <SDL.h>

#include <fmt/format.h>

#include "DiabloUI/ui_flags.hpp"
#include "automap.h"
#include "engine/rectangle.hpp"
#include "engine/render/text_render.hpp"

namespace devilution::oracool {

namespace {

bool IndicatorActive = false;
uint32_t IndicatorStartTime = 0;
uint64_t IndicatorAmount = 0;

// Oracool: user request - visible for half a second, then dismissed.
constexpr uint32_t TotalDurationMs = 500;
// Matches xp_counter.cpp's own CounterHeight, so this sits exactly 1px below that row.
constexpr int XpCounterHeight = 20;

} // namespace

void TriggerXpGainIndicator(uint64_t amount)
{
	IndicatorActive = true;
	IndicatorStartTime = SDL_GetTicks();
	IndicatorAmount = amount;
}

void DrawXpGainIndicator(const Surface &out)
{
	if (!IndicatorActive)
		return;

	const uint32_t elapsed = SDL_GetTicks() - IndicatorStartTime;
	if (elapsed >= TotalDurationMs) {
		IndicatorActive = false;
		return;
	}

	const std::string text = fmt::format("+{:d}", IndicatorAmount);
	const int textWidth = GetLineWidth(text, GameFont12, 1);
	const Rectangle miniMap = GetMiniMapScreenRect();
	const Point position {
		miniMap.position.x + miniMap.size.width / 2 - textWidth / 2,
		miniMap.position.y + miniMap.size.height + 1 + XpCounterHeight + 1
	};
	const Rectangle rect { position, { textWidth, XpCounterHeight } };
	DrawString(out, text, rect, { UiFlags::AlignCenter | UiFlags::VerticalCenter | UiFlags::FontSize12 | UiFlags::ColorGold });
}

} // namespace devilution::oracool
