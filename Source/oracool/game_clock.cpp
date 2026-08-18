#include "oracool/game_clock.h"

#include <SDL.h>

#include <ctime>

#include <fmt/format.h>

#include "DiabloUI/ui_flags.hpp"
#include "automap.h"
#include "engine/rectangle.hpp"
#include "engine/render/text_render.hpp"
#include "options.h"
#include "oracool/game_speed.h"

namespace devilution::oracool {

namespace {

// Wide enough for the longer 12-hour "12:45 PM" form, not just the default 24-hour "14:45".
constexpr int ClockWidth = 72;
constexpr int ClockHeight = 20;
// Oracool: user request (2026-08-11) - moved out from under the mini-map to the screen's top-left
// corner, where nothing else competes for space.
constexpr int ClockMargin = 8;
/** @brief The speed readout's band, directly under the clock. Reserved even when nothing is drawn. */
constexpr int SpeedHeight = 16;

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


/**
 * @brief The game-speed readout, in its own band directly beneath the clock.
 *
 * The band is reserved whether or not the readout is drawn (user request, 2026-08-18: the setting
 * has a Blink mode, and a line that appears and disappears would shove anything below it around
 * twice a second). Nothing currently sits under it - GetLevelUpIconRect moved to the LMB skill
 * button - but reserving the space is what keeps that true.
 */
void DrawGameSpeedReadout(const Surface &out)
{
	const GameSpeedReadout mode = *sgOptions.Oracool.gameSpeedReadout;
	if (mode == GameSpeedReadout::Off)
		return;
	if (mode == GameSpeedReadout::Blink) {
		if (!GameSpeedChangedRecently())
			return;
		// ~4Hz: two full on/off cycles inside the one second the readout is up, which reads as a
		// blink rather than as a flicker or a single flash.
		if (((SDL_GetTicks() / 125) % 2) == 0)
			return;
	}

	const Rectangle rect { Point { ClockMargin, ClockMargin + ClockHeight }, Size { ClockWidth, SpeedHeight } };
	DrawString(out, fmt::format("x{:d}", CurrentGameSpeed()), rect,
	    { UiFlags::VerticalCenter | UiFlags::FontSize12 | UiFlags::ColorWhitegold });
}

void DrawGameClock(const Surface &out)
{
	if (!*sgOptions.Oracool.gameClock)
		return;

	const Rectangle rect { Point { ClockMargin, ClockMargin }, Size { ClockWidth, ClockHeight } };
	DrawString(out, CurrentClockText(), rect, { UiFlags::VerticalCenter | UiFlags::FontSize12 | UiFlags::ColorGold });
}

} // namespace devilution::oracool
