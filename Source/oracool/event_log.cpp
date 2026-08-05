#include "oracool/event_log.h"

#include <ctime>
#include <deque>
#include <string>
#include <utility>

#include <fmt/format.h>

#include "DiabloUI/ui_flags.hpp"
#include "control.h"
#include "engine/palette.h"
#include "engine/rectangle.hpp"
#include "engine/render/primitive_render.hpp"
#include "engine/render/text_render.hpp"
#include "options.h"

namespace devilution::oracool {

namespace {

struct LogEntry {
	std::string timestamp;
	std::string message;
};

// Oracool: bounded ring buffer - oldest entries silently drop off once the cap is hit. Session-only
// (never saved to disk, never cleared mid-session otherwise), so a generous cap costs only a little
// memory (a few hundred short strings at most) in exchange for never needing a "clear log" UI.
constexpr size_t MaxEntries = 200;
constexpr size_t MaxVisibleLines = 18;

constexpr int ButtonWidth = 32;
constexpr int ButtonHeight = 20;
constexpr int WindowWidth = 380;
constexpr int LineHeight = 14;
constexpr int WindowPadding = 8;
constexpr uint8_t EventLogBorderColor = PAL16_YELLOW + 2;

std::deque<LogEntry> Entries;
bool WindowOpen = false;
std::string PendingDeathSource;

// Oracool: sits just above the durability-warning icons (control.cpp's DrawDurIcon, anchored to
// the same top-right x and drawn upward from MainPanel.position.y - 17), so the button has a
// stable home whether or not any equipped item is currently damaged.
Point ButtonPosition()
{
	const Rectangle &mainPanel = GetMainPanel();
	int x = mainPanel.position.x + mainPanel.size.width - 32 - 16;
	int y = mainPanel.position.y - 17 - 32 - 8 - ButtonHeight;
	return { x, y };
}

int WindowHeight()
{
	return WindowPadding * 2 + LineHeight + static_cast<int>(MaxVisibleLines) * LineHeight;
}

Point WindowPosition()
{
	Point button = ButtonPosition();
	return { button.x + ButtonWidth - WindowWidth, button.y + ButtonHeight + 4 };
}

std::string CurrentTimestamp()
{
	std::time_t timeResult = std::time(nullptr);
	const std::tm *localTime = std::localtime(&timeResult);
	if (localTime == nullptr)
		return "--:--:--";
	return fmt::format("{:02d}:{:02d}:{:02d}", localTime->tm_hour, localTime->tm_min, localTime->tm_sec);
}

} // namespace

void LogEvent(std::string message)
{
	Entries.push_front({ CurrentTimestamp(), std::move(message) });
	while (Entries.size() > MaxEntries)
		Entries.pop_back();
}

void ToggleEventLog()
{
	WindowOpen = !WindowOpen;
}

void DrawEventLogButton(const Surface &out)
{
	if (!*sgOptions.Oracool.eventLog)
		return;

	const Rectangle rect { ButtonPosition(), { ButtonWidth, ButtonHeight } };
	DrawHalfTransparentRectTo(out, rect.position.x, rect.position.y, rect.size.width, rect.size.height);
	UnsafeDrawBorder2px(out, rect, EventLogBorderColor);
	DrawString(out, "LOG", rect, { UiFlags::AlignCenter | UiFlags::VerticalCenter | UiFlags::FontSize12 | (WindowOpen ? UiFlags::ColorWhite : UiFlags::ColorGold) });
}

void DrawEventLogWindow(const Surface &out)
{
	if (!*sgOptions.Oracool.eventLog || !WindowOpen)
		return;

	const Point windowPosition = WindowPosition();
	const int windowHeight = WindowHeight();
	DrawHalfTransparentRectTo(out, windowPosition.x, windowPosition.y, WindowWidth, windowHeight);
	UnsafeDrawBorder2px(out, Rectangle { windowPosition, { WindowWidth, windowHeight } }, EventLogBorderColor);

	Point linePosition = windowPosition + Displacement { WindowPadding, WindowPadding };
	const Size lineSize { WindowWidth - WindowPadding * 2, LineHeight };

	DrawString(out, "Event Log", Rectangle { linePosition, lineSize }, { UiFlags::ColorGold | UiFlags::FontSize12 });
	linePosition.y += LineHeight;

	if (Entries.empty()) {
		DrawString(out, "No events yet.", Rectangle { linePosition, lineSize }, { UiFlags::ColorGold | UiFlags::FontSize12 });
		return;
	}

	size_t shown = 0;
	for (const LogEntry &entry : Entries) {
		if (shown >= MaxVisibleLines - 1)
			break;
		const std::string line = fmt::format("{:s}  {:s}", entry.timestamp, entry.message);
		DrawString(out, line, Rectangle { linePosition, lineSize }, { UiFlags::ColorGold | UiFlags::FontSize12 });
		linePosition.y += LineHeight;
		shown++;
	}
}

void NotePendingDeathSource(std::string source)
{
	PendingDeathSource = std::move(source);
}

void LogPlayerDeath(const std::string &fallbackReason)
{
	LogEvent(fmt::format("Slain by {:s}", PendingDeathSource.empty() ? fallbackReason : PendingDeathSource));
	PendingDeathSource.clear();
}

bool CheckEventLogButtonClick(Point mousePosition)
{
	if (!*sgOptions.Oracool.eventLog)
		return false;

	const Rectangle rect { ButtonPosition(), { ButtonWidth, ButtonHeight } };
	if (!rect.contains(mousePosition))
		return false;

	ToggleEventLog();
	return true;
}

} // namespace devilution::oracool
