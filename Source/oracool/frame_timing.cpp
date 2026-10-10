#include "oracool/frame_timing.h"

#include <SDL.h>

#include <algorithm>
#include <cstdint>
#include <cstring>

#include <fmt/format.h>

#include "DiabloUI/ui_flags.hpp"
#include "engine/render/scrollrt.h"
#include "engine/render/text_render.hpp"
#include "init.h"
#include "utils/stdcompat/string_view.hpp"

namespace devilution::oracool {

namespace {

/** The window the line speaks for: the last second, as DrawFPS's average does. */
constexpr uint64_t WindowMicros = 1000000;
/** A frame past this is a stutter the eye catches (user, 2026-10-10): 25 ms is a 40 FPS frame. */
constexpr uint32_t StutterMicros = 25000;
/** How often the shown line is rebuilt. Every frame would flicker past reading. */
constexpr uint64_t RefreshMicros = 250000;

FrameTimingRing Ring;
uint64_t LastPresentCounter = 0;
uint64_t LastRefreshMicros = 0;
char Shown[64] {};
size_t ShownLength = 0;

uint64_t NowMicros()
{
	static const uint64_t Frequency = SDL_GetPerformanceFrequency();
	const uint64_t counter = SDL_GetPerformanceCounter();
	// Split so a long uptime cannot overflow counter * 1e6.
	return (counter / Frequency) * 1000000 + (counter % Frequency) * 1000000 / Frequency;
}

} // namespace

void FormatFrameTiming(const FrameTimingStats &stats, char (&out)[64])
{
	const uint32_t worstMs = (stats.worstMicros + 500) / 1000;
	const uint32_t avgTenths = (stats.averageMicros + 50) / 100;
	// "|", not a middle dot: the game font has no page for it in every language.
	const auto result = fmt::format_to_n(&out[0], sizeof(out) - 1, "worst {} ms | avg {}.{} ms | >{} ms: {}", worstMs,
	    avgTenths / 10, avgTenths % 10, StutterMicros / 1000, stats.overThreshold);
	*result.out = '\0';
}

void NoteFramePresented()
{
	const uint64_t now = SDL_GetPerformanceCounter();
	if (LastPresentCounter != 0 && now > LastPresentCounter) {
		static const uint64_t Frequency = SDL_GetPerformanceFrequency();
		const uint64_t micros = (now - LastPresentCounter) * 1000000 / Frequency;
		Ring.Push(NowMicros(), static_cast<uint32_t>(std::min<uint64_t>(micros, UINT32_MAX)));
	}
	LastPresentCounter = now;
}

void DrawFrameTiming(const Surface &out, Point position)
{
	// The same gate as DrawFPS: the counter's toggle, and a window that is drawn at all.
	if (!frameflag || !gbActive)
		return;
	const uint64_t now = NowMicros();
	if (ShownLength == 0 || now - LastRefreshMicros >= RefreshMicros) {
		LastRefreshMicros = now;
		FormatFrameTiming(Ring.Summarise(now, WindowMicros, StutterMicros), Shown);
		ShownLength = std::strlen(Shown);
	}
	DrawString(out, string_view { Shown, ShownLength }, position, { UiFlags::ColorRed });
}

} // namespace devilution::oracool
