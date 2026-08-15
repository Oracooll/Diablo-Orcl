#include "oracool/xp_gain_indicator.h"

#include <SDL.h>

#include <fmt/format.h>

#include "DiabloUI/ui_flags.hpp"
#include "engine/rectangle.hpp"
#include "engine/render/text_render.hpp"
#include "oracool/xp_counter.h"
#include "player.h"

namespace devilution::oracool {

namespace {

bool IndicatorActive = false;
uint32_t IndicatorStartTime = 0;
uint64_t IndicatorAmount = 0;
// Tenths of a percent of the current level's experience span, captured at trigger time - the level
// (and with it the span) can change before the half-second is up, and the question the number
// answers ("what fraction of the level I was on did that kill buy?") is asked at kill time.
uint64_t IndicatorPercentTenths = 0;

// Oracool: user request - visible for half a second, then dismissed.
constexpr uint32_t TotalDurationMs = 500;
constexpr int IndicatorHeight = 20;

} // namespace

void TriggerXpGainIndicator(uint64_t amount)
{
	IndicatorActive = true;
	IndicatorStartTime = SDL_GetTicks();
	IndicatorAmount = amount;
	IndicatorPercentTenths = amount * 1000 / GetLevelExperienceSpan(*MyPlayer);
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

	// Oracool: user request (2026-08-16) - "exp per monster blinker to move from under the minimap to
	// above the exp counter and to also receive percentage indicator, so i know what percentage of
	// level one mob generates." One decimal place because a single kill is routinely under 1% of a
	// level, and a blinker that always said "+312 (0%)" would answer the question with a shrug.
	const std::string text = IndicatorPercentTenths == 0
	    ? fmt::format("+{:d} (<0.1%)", IndicatorAmount)
	    : fmt::format("+{:d} ({:d}.{:d}%)", IndicatorAmount, IndicatorPercentTenths / 10, IndicatorPercentTenths % 10);

	// Directly above the XP counter's strip, sharing its centre, so the flash and the running total
	// read as one instrument.
	const Rectangle counter = GetXpCounterDrawRect();
	const Rectangle rect {
		{ counter.position.x, counter.position.y - IndicatorHeight - 1 },
		{ counter.size.width, IndicatorHeight }
	};
	DrawString(out, text, rect, { UiFlags::AlignCenter | UiFlags::VerticalCenter | UiFlags::FontSize12 | UiFlags::ColorGold });
}

} // namespace devilution::oracool
