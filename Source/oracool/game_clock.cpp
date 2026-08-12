#include "oracool/game_clock.h"

#include <ctime>

#include <fmt/format.h>

#include "DiabloUI/ui_flags.hpp"
#include "automap.h"
#include "engine/rectangle.hpp"
#include "engine/render/text_render.hpp"
#include "options.h"

namespace devilution::oracool {

namespace {

// Wide enough for the longer 12-hour "12:45 PM" form, not just the default 24-hour "14:45".
constexpr int ClockWidth = 72;
constexpr int ClockHeight = 20;
// Oracool: user request (2026-08-11) - moved out from under the mini-map to the screen's top-left
// corner, where nothing else competes for space.
constexpr int ClockMargin = 8;

std::string CurrentClockText()
{
	const std::time_t timeResult = std::time(nullptr);
	const std::tm *localTime = std::localtime(&timeResult);
	if (localTime == nullptr)
		return "--:--";

	if (!*sgOptions.Oracool.gameClock12HourFormat)
		return fmt::format("{:02d}:{:02d}", localTime->tm_hour, localTime->tm_min);

	int hour12 = localTime->tm_hour % 12;
	if (hour12 == 0)
		hour12 = 12;
	const char *suffix = localTime->tm_hour < 12 ? "AM" : "PM";
	return fmt::format("{:d}:{:02d} {:s}", hour12, localTime->tm_min, suffix);
}

} // namespace

void DrawGameClock(const Surface &out)
{
	if (!*sgOptions.Oracool.gameClock)
		return;

	const Rectangle rect { Point { ClockMargin, ClockMargin }, Size { ClockWidth, ClockHeight } };
	DrawString(out, CurrentClockText(), rect, { UiFlags::VerticalCenter | UiFlags::FontSize12 | UiFlags::ColorGold });
}

} // namespace devilution::oracool
