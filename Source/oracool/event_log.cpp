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
#include "oracool/ornate_border.h"
#include "oracool/telemetry.h"
#include "oracool/runeword_book.h"
#include "oracool/window_close.h"
#include "utils/language.h"
#ifdef _DEBUG
#include "debug.h"
#endif

namespace devilution::oracool {

namespace {

struct LogEntry {
	std::string timestamp;
	std::string message;
	UiFlags color;
};

// Oracool: bounded ring buffer - oldest entries silently drop off once the cap is hit. Session-only
// (never saved to disk, never cleared mid-session otherwise), so a generous cap costs only a little
// memory (a few hundred short strings at most) in exchange for never needing a "clear log" UI.
constexpr size_t MaxEntries = 200;
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
/** @brief The newest entry's message as logged, and how many times in a row it was (2026-09-27). */
std::string FrontMessage;
int FrontRepeats = 0;

// Oracool: user request (2026-08-11) - the standalone "LOG" button is gone; the log is opened from
// the belt's Menu popup instead (see oracool/hud_menu.cpp). This is now just the window's own top
// edge: the row directly below the mini-map, where the button used to sit.
Point WindowTopLeftBelowMiniMap()
{
	const Rectangle miniMap = GetMiniMapScreenRect();
	return { miniMap.position.x, miniMap.position.y + miniMap.size.height + 1 };
}

// Oracool: user request - the window's left and right edges should always line up with the
// mini-map's own, at any screen resolution, not just match its width.
int WindowWidth()
{
	return GetMiniMapScreenRect().size.width;
}

// Oracool: user request (2026-08-13) - the window unfolds until its bottom edge is as far from the
// bottom of the screen as the mini-map's top edge is from the top, so the column reads as
// symmetrically inset. It used to stop 1px above the legacy main-panel rect, which was an
// invisible anchor and left the window ending well short of the screen's bottom.
//
// The gap is taken from GetMiniMapScreenRect().position.y rather than automap.cpp's MiniMapMargin
// constant, which is private to that file - and this is the same rect the window already uses for
// its top edge and its width, so all three stay tied to one source.
//
// This is the authoritative height - unlike the previous line-count-driven design, it isn't
// quantized to a whole number of entry lines, so there may be a little unused padding at the bottom
// if the exact span doesn't divide evenly by LineHeight. The MinVisibleLines-based floor only
// guards against a pathological custom resolution where the mini-map would otherwise nearly reach
// the bottom of the screen.
int WindowHeight()
{
	const int windowTop = WindowTopLeftBelowMiniMap().y;
	const int windowBottom = gnScreenHeight - GetMiniMapScreenRect().position.y;
	const int minHeight = WindowPadding * 2 + LineHeight + static_cast<int>(MinVisibleLines) * LineHeight;
	return std::max(windowBottom - windowTop, minHeight);
}

size_t VisibleLineCount()
{
	const int contentHeight = WindowHeight() - WindowPadding * 2 - LineHeight;
	const int lines = contentHeight / LineHeight;
	// No ceiling (user, 2026-09-24 dev note: "make sure the event log is capable of displaying text all the
	// way down to the bottom of the screen"). A cap of 18 lines stopped the text well short of the window
	// the height rule above unfolds, which at 720 tall holds some thirty; the window's own height is the limit.
	return static_cast<size_t>(std::max(lines, static_cast<int>(MinVisibleLines)));
}

Point WindowPosition()
{
	// Always exactly the mini-map's own left edge - together with WindowWidth() matching its width,
	// this keeps both windows' left AND right borders aligned in a straight line, at any resolution.
	return WindowTopLeftBelowMiniMap();
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

void LogEvent(std::string message, UiFlags color)
{
	// The same line again folds into the newest entry with a count and a fresh time (audit, 2026-09-27): an autosave on
	// every kill logged "Game saved (auto)" each time, and two hundred of them pushed every boss, drop and death out.
	if (!Entries.empty() && FrontRepeats > 0 && message == FrontMessage && Entries.front().color == color) {
		FrontRepeats++;
		Entries.front().timestamp = CurrentTimestamp();
		Entries.front().message = FrontMessage + " (x" + std::to_string(FrontRepeats) + ")";
		return;
	}
	FrontMessage = message;
	FrontRepeats = 1;
	Entries.push_front({ CurrentTimestamp(), std::move(message), color });
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

void ClearEventLogForNewGame()
{
	// Audit, 2026-08-30. Entries is a file-local deque, so it outlives a GAME rather than the
	// process, and nothing cleared it. The next character started in the same session opened the
	// log onto the previous one's history - their kills, their crafts, the death that ended them.
	//
	// PendingDeathSource goes too: it is a half-finished sentence about someone else's death, and
	// left set it would be attached to the first death of the new character.
	Entries.clear();
	FrontMessage.clear();
	FrontRepeats = 0;
	PendingDeathSource.clear();
	ScrollOffset = 0;
	WindowOpen = false;
}

size_t EventLogEntryCount()
{
	return Entries.size();
}

bool IsCornerHudShown()
{
	return !AutomapActive && !IsRightPanelOpen() && !talkflag && !IsRunewordBookOpen()
#ifdef _DEBUG
	    && !DebugClearUi
#endif
	    ;
}

Rectangle GetEventLogWindowRect()
{
	if (!IsEventLogOpen() || !IsCornerHudShown())
		return Rectangle { { 0, 0 }, { 0, 0 } };
	// The same three helpers the draw uses, so the rect that rejects a click and the rect that gets
	// painted cannot drift apart - the failure GetLeftPanelContentRect was written to end.
	return Rectangle { WindowPosition(), { WindowWidth(), WindowHeight() } };
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

void DrawEventLogWindow(const Surface &out)
{
	if (!*sgOptions.Oracool.eventLog || !WindowOpen)
		return;

	const Point windowPosition = WindowPosition();
	const int windowHeight = WindowHeight();
	const int windowWidth = WindowWidth();
	DrawHalfTransparentRectTo(out, windowPosition.x, windowPosition.y, windowWidth, windowHeight);
	// Oracool: user request - the same textbox_frame00 bevel the mini-map now wears, so the two
	// stacked windows in the top-right corner read as one set (see oracool/ornate_border.h).
	const Rectangle window { windowPosition, { windowWidth, windowHeight } };
	DrawOrnateBorder(out, window);
	// The close button (audit, 2026-08-31). This window had none - it is a floating window the
	// player opens, and the fork's standing rule is that every one of those carries the shared red X
	// in its own top-right corner. It was reachable only by pressing its key again, which is exactly
	// the "a rule enforced by remembering is not a rule" case oracool/window_close.h was written for.
	DrawWindowCloseButton(out, window);

	Point linePosition = windowPosition + Displacement { WindowPadding, WindowPadding };
	// The title's own line stops short of the button, so a long title cannot run under it. The rows
	// below keep the full width - they start under the button, not beside it.
	const int contentWidth = windowWidth - WindowPadding * 2;
	const Size lineSize { contentWidth, LineHeight };
	const Size titleSize { contentWidth - WindowCloseButtonSize, LineHeight };

	DrawString(out, _("Event Log"), Rectangle { linePosition, titleSize }, { UiFlags::ColorGold | UiFlags::FontSize12 });
	linePosition.y += LineHeight;

	if (Entries.empty()) {
		DrawString(out, _("No events yet."), Rectangle { linePosition, lineSize }, { UiFlags::ColorGold | UiFlags::FontSize12 });
		return;
	}

	if (ScrollOffset > Entries.size() - 1)
		ScrollOffset = Entries.size() - 1;

	// Oracool: user request - wrap entry text at the window's right edge instead of letting it run
	// past the border. Word-wrapped up front (rather than relying on DrawString's own per-character
	// wrap-on-overflow) so long words don't get split mid-word. Line budget is now consumed in
	// wrapped lines, not one-line-per-entry, since a single long entry can take several.
	// Every content line, down to the bottom padding - the "- 1" here left one more row empty at the foot.
	const size_t lineBudget = VisibleLineCount();
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
		// At the loop's own line height (audit, 2026-09-27): the font's 12 inside an entry against 14 between entries made
		// uneven spacing, and a taller font clipped each entry's last line.
		DrawString(out, wrapped, entryRect, { entry.color | UiFlags::FontSize12, /*spacing=*/1, /*lineHeight=*/LineHeight });
		linePosition.y += static_cast<int>(linesToDraw) * LineHeight;
		linesUsed += linesToDraw;
	}
}

void NotePendingDeathSource(std::string source)
{
	PendingDeathSource = std::move(source);
}

void ClearPendingDeathSource()
{
	PendingDeathSource.clear();
}

void LogPlayerDeath(const std::string &fallbackReason)
{
	const std::string &source = PendingDeathSource.empty() ? fallbackReason : PendingDeathSource;
	LogEvent(fmt::format("Slain by {:s}", source));
	// Phase 0.9: deaths are the loudest tuning signal there is.
	TelemetryRecordPlayerDeath(source);
	PendingDeathSource.clear();
}

} // namespace devilution::oracool
