#include "oracool/event_log.h"

#include <algorithm>
#include <ctime>
#include <deque>
#include <string>
#include <utility>

#include <fmt/format.h>

#include "DiabloUI/ui_flags.hpp"
#include "automap.h"
#include "control.h"
#include "engine/palette.h"
#include "engine/rectangle.hpp"
#include "engine/render/primitive_render.hpp"
#include "engine/render/text_render.hpp"
#include "options.h"
#include "utils/ui_fwd.h"

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
constexpr size_t MinVisibleLines = 3;

constexpr int ButtonWidth = 32;
constexpr int ButtonHeight = 20;
constexpr int LineHeight = 14;
constexpr int WindowPadding = 8;
constexpr int ScreenMargin = 4;
constexpr uint8_t EventLogBorderColor = PAL16_YELLOW + 2;

std::deque<LogEntry> Entries;
bool WindowOpen = false;
std::string PendingDeathSource;
// Oracool: how many entries are skipped from the front (most recent) before drawing - 0 means
// "showing the newest entries", matching qol/chatlog.cpp's SkipLines convention. Reset to 0 every
// time the window is opened, same as chatlog's own reset-on-open behavior.
size_t ScrollOffset = 0;

// Oracool: user request - the window should be no wider than the mini-map. Queried live rather
// than duplicated as a constant, since it's itself derived from AutoMapScale.
int WindowWidth()
{
	return GetMiniMapWidth();
}

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

// Oracool: the window opens upward from the button, capped at 5px below the mini-map's own
// bottom border per user request - it never crowds the mini-map, and never needs to (the mini-map
// sits well above the button, so there's still plenty of room between the two for most entry
// counts). This also happens to keep it safely on-screen at any resolution: at low resolutions
// (640x480 is this engine's default) there isn't enough room below the button to fit a
// fixed-height window without running off the bottom of the screen -
// UnsafeDrawBorder2px/DrawHalfTransparentRectTo don't bounds-check, so that used to write past the
// framebuffer and crash the game. Visible line count is derived from actual available space
// instead of a fixed constant.
size_t VisibleLineCount()
{
	const int windowBottom = ButtonPosition().y - 4;
	const int windowTopBound = GetMiniMapBottom() + 5;
	const int availableHeight = windowBottom - windowTopBound;
	const int contentHeight = availableHeight - WindowPadding * 2 - LineHeight;
	const int lines = contentHeight / LineHeight;
	return static_cast<size_t>(std::clamp(lines, static_cast<int>(MinVisibleLines), static_cast<int>(MaxVisibleLines)));
}

int WindowHeight()
{
	return WindowPadding * 2 + LineHeight + static_cast<int>(VisibleLineCount()) * LineHeight;
}

Point WindowPosition()
{
	const Point button = ButtonPosition();
	const int windowHeight = WindowHeight();
	const int windowWidth = WindowWidth();
	int x = button.x + ButtonWidth - windowWidth;
	x = std::clamp(x, ScreenMargin, static_cast<int>(gnScreenWidth) - windowWidth - ScreenMargin);
	// The mini-map-bottom-plus-5 boundary is enforced by VisibleLineCount() above; ScreenMargin here
	// is only a last-resort safety net against ever drawing off the top of the screen.
	int y = std::max(button.y - 4 - windowHeight, ScreenMargin);
	return { x, y };
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
	if (WindowOpen)
		ScrollOffset = 0;
}

bool IsEventLogOpen()
{
	return *sgOptions.Oracool.eventLog && WindowOpen;
}

void ScrollEventLogUp()
{
	if (ScrollOffset > 0)
		ScrollOffset--;
}

void ScrollEventLogDown()
{
	const size_t maxShown = VisibleLineCount() - 1;
	const size_t maxScroll = Entries.size() > maxShown ? Entries.size() - maxShown : 0;
	if (ScrollOffset < maxScroll)
		ScrollOffset++;
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
	const int windowWidth = WindowWidth();
	DrawHalfTransparentRectTo(out, windowPosition.x, windowPosition.y, windowWidth, windowHeight);
	UnsafeDrawBorder2px(out, Rectangle { windowPosition, { windowWidth, windowHeight } }, EventLogBorderColor);

	Point linePosition = windowPosition + Displacement { WindowPadding, WindowPadding };
	const Size lineSize { windowWidth - WindowPadding * 2, LineHeight };

	DrawString(out, "Event Log", Rectangle { linePosition, lineSize }, { UiFlags::ColorGold | UiFlags::FontSize12 });
	linePosition.y += LineHeight;

	if (Entries.empty()) {
		DrawString(out, "No events yet.", Rectangle { linePosition, lineSize }, { UiFlags::ColorGold | UiFlags::FontSize12 });
		return;
	}

	const size_t maxShown = VisibleLineCount() - 1;
	const size_t maxScroll = Entries.size() > maxShown ? Entries.size() - maxShown : 0;
	if (ScrollOffset > maxScroll)
		ScrollOffset = maxScroll;

	size_t index = 0;
	size_t shown = 0;
	for (const LogEntry &entry : Entries) {
		if (index++ < ScrollOffset)
			continue;
		if (shown >= maxShown)
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
