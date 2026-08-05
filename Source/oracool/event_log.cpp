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
constexpr uint8_t EventLogBorderColor = PAL16_YELLOW + 2;

std::deque<LogEntry> Entries;
bool WindowOpen = false;
std::string PendingDeathSource;
// Oracool: how many entries are skipped from the front (most recent) before drawing - 0 means
// "showing the newest entries", matching qol/chatlog.cpp's SkipLines convention. Reset to 0 every
// time the window is opened, same as chatlog's own reset-on-open behavior.
size_t ScrollOffset = 0;

// Oracool: user request - right-aligned to the mini-map's own right border, 1px below its bottom
// border. Queried live (never cached) since the mini-map's rect is itself resolution- and
// zoom-dependent, and always on-screen by construction, so no additional bounds-checking is
// needed here.
Point ButtonPosition()
{
	const Rectangle miniMap = GetMiniMapScreenRect();
	const int x = miniMap.position.x + miniMap.size.width - ButtonWidth;
	const int y = miniMap.position.y + miniMap.size.height + 1;
	return { x, y };
}

// Oracool: user request - the window's left and right edges should always line up with the
// mini-map's own, at any screen resolution, not just match its width.
int WindowWidth()
{
	return GetMiniMapScreenRect().size.width;
}

// Oracool: user request - the window spans from 1px below the button down to 1px above the main
// GUI panel at the bottom of the screen, whatever that leaves. This is the authoritative height -
// unlike the previous line-count-driven design, it isn't quantized to a whole number of entry
// lines, so there may be a little unused padding at the bottom if the exact span doesn't divide
// evenly by LineHeight. The main panel is always on-screen by construction, so this is always a
// safe, positive size in practice; the MinVisibleLines-based floor only guards against a
// pathological custom resolution where the mini-map and main panel would otherwise nearly touch.
int WindowHeight()
{
	const int windowTop = ButtonPosition().y + ButtonHeight + 1;
	const int windowBottom = GetMainPanel().position.y - 1;
	const int minHeight = WindowPadding * 2 + LineHeight + static_cast<int>(MinVisibleLines) * LineHeight;
	return std::max(windowBottom - windowTop, minHeight);
}

size_t VisibleLineCount()
{
	const int contentHeight = WindowHeight() - WindowPadding * 2 - LineHeight;
	const int lines = contentHeight / LineHeight;
	return static_cast<size_t>(std::clamp(lines, static_cast<int>(MinVisibleLines), static_cast<int>(MaxVisibleLines)));
}

Point WindowPosition()
{
	// Always exactly the mini-map's own left edge - together with WindowWidth() matching its width,
	// this keeps both windows' left AND right borders aligned in a straight line, at any resolution.
	const int x = GetMiniMapScreenRect().position.x;
	const int y = ButtonPosition().y + ButtonHeight + 1;
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
	if (!Entries.empty() && ScrollOffset < Entries.size() - 1)
		ScrollOffset++;
}

void DrawEventLogButton(const Surface &out)
{
	if (!*sgOptions.Oracool.eventLog)
		return;

	// Oracool: user request - no visible box, just the clickable "LOG" text itself. The hit-test
	// rect in CheckEventLogButtonClick still uses the same area, it's just no longer drawn.
	const Rectangle rect { ButtonPosition(), { ButtonWidth, ButtonHeight } };
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
	// Oracool: user request - match the mini-map's own 1px dashed border instead of a solid one.
	DrawDashedBorder1px(out, windowPosition.x, windowPosition.y, windowWidth, windowHeight, EventLogBorderColor);

	Point linePosition = windowPosition + Displacement { WindowPadding, WindowPadding };
	const int contentWidth = windowWidth - WindowPadding * 2;
	const Size lineSize { contentWidth, LineHeight };

	DrawString(out, "Event Log", Rectangle { linePosition, lineSize }, { UiFlags::ColorGold | UiFlags::FontSize12 });
	linePosition.y += LineHeight;

	if (Entries.empty()) {
		DrawString(out, "No events yet.", Rectangle { linePosition, lineSize }, { UiFlags::ColorGold | UiFlags::FontSize12 });
		return;
	}

	if (ScrollOffset > Entries.size() - 1)
		ScrollOffset = Entries.size() - 1;

	// Oracool: user request - wrap entry text at the window's right edge instead of letting it run
	// past the border. Word-wrapped up front (rather than relying on DrawString's own per-character
	// wrap-on-overflow) so long words don't get split mid-word. Line budget is now consumed in
	// wrapped lines, not one-line-per-entry, since a single long entry can take several.
	const size_t lineBudget = VisibleLineCount() - 1;
	size_t linesUsed = 0;
	size_t index = 0;
	for (const LogEntry &entry : Entries) {
		if (index++ < ScrollOffset)
			continue;
		if (linesUsed >= lineBudget)
			break;
		const std::string raw = fmt::format("{:s}  {:s}", entry.timestamp, entry.message);
		const std::string wrapped = WordWrapString(raw, contentWidth);
		const size_t entryLines = static_cast<size_t>(std::count(wrapped.begin(), wrapped.end(), '\n')) + 1;
		const size_t linesToDraw = std::min(entryLines, lineBudget - linesUsed);
		const Rectangle entryRect { linePosition, { contentWidth, static_cast<int>(linesToDraw) * LineHeight } };
		DrawString(out, wrapped, entryRect, { UiFlags::ColorGold | UiFlags::FontSize12 });
		linePosition.y += static_cast<int>(linesToDraw) * LineHeight;
		linesUsed += linesToDraw;
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
