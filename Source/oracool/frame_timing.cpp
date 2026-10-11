#include "oracool/frame_timing.h"

#include <SDL.h>

#include <algorithm>
#include <cstdint>
#include <cstring>

#include <fmt/format.h>

#include <array>
#include <cstdio>
#include <ctime>
#include <string>

#include "DiabloUI/ui_flags.hpp"
#include "engine/render/scrollrt.h"
#include "engine/render/text_render.hpp"
#include "init.h"
#include "levels/gendung.h"
#include "options.h"
#include "utils/display.h"
#include "utils/file_util.h"
#include "utils/paths.h"
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

// ----- Stage 0: the passes ---------------------------------------------------------------------------------------

constexpr size_t PassCount = static_cast<size_t>(FramePass::Count);
std::array<uint64_t, PassCount> PassStart {}; // performance counter at BeginFramePass
std::array<uint64_t, PassCount> PassFrame {}; // this frame's ticks per pass
std::array<uint64_t, PassCount> ShownSum {};  // the shown second's ticks
uint32_t ShownFrames = 0;
uint64_t ShownSince = 0; // micros
std::array<uint64_t, PassCount> LogSum {}; // the logged ten seconds' ticks
uint32_t LogFrames = 0;
uint64_t LogSince = 0;
std::array<std::string, 3> PassLines;

constexpr uint64_t LogMicros = 10000000;

float TicksToMs(uint64_t ticks)
{
	static const double Frequency = static_cast<double>(SDL_GetPerformanceFrequency());
	return static_cast<float>(static_cast<double>(ticks) * 1000.0 / Frequency);
}

/** The effect options' states, one digit each, in the order the CSV header's comment names them. */
std::string OptionDigits()
{
	const OracoolOptions &o = sgOptions.Oracool;
	const bool states[] = { *o.smoothLighting, *o.colouredLighting, *o.torchFlicker, *o.glowingSpells, *o.ambientOcclusion, *o.areaMood,
		*o.vignette, *o.bloom, *o.particles, *o.heatHaze, *o.rayTracedShadows, *o.zoomFilter, *o.dayNightCycle, *o.floorMarks,
		*o.lootBeams, *o.surfaceRelief, *o.zoomSharpen, *o.animationBlending };
	std::string digits;
	for (const bool on : states)
		digits.push_back(on ? '1' : '0');
	return digits;
}

void WriteLogRow(const FrameTimingStats &frames)
{
	if (LogFrames == 0)
		return;
	const std::string path = paths::PrefPath() + "frame_passes.csv";
	bool fresh = true;
	if (FILE *head = OpenFile(path.c_str(), "rb"); head != nullptr) {
		fresh = false;
		std::fclose(head);
	}
	FILE *file = OpenFile(path.c_str(), "ab");
	if (file == nullptr)
		return;
	if (fresh) {
		std::string header = "time,leveltype,level,setlevel,zoom,width,height,world_frames,avg_frame_ms,worst_frame_ms,over25,world_ms";
		for (size_t i = 0; i < PassCount; i++)
			header += std::string(",") + FramePassName(static_cast<FramePass>(i)) + "_ms";
		// options: smooth colour flicker glow ao mood vignette bloom particles haze shadows zoomfilter daynight marks beams relief sharpen blend
		header += ",options\n";
		std::fputs(header.c_str(), file);
	}
	const std::time_t t = std::time(nullptr);
	char when[32] {};
	std::strftime(when, sizeof(when), "%Y-%m-%d %H:%M:%S", std::localtime(&t));
	uint64_t worldTicks = 0;
	for (const uint64_t v : LogSum)
		worldTicks += v;
	std::string row = fmt::format("{},{},{},{},{:.1f},{},{},{},{:.2f},{:.1f},{},{:.2f}", when, static_cast<int>(leveltype), static_cast<int>(currlevel),
	    setlevel ? static_cast<int>(setlvlnum) : -1, *sgOptions.Oracool.dungeonZoomLevel, gnScreenWidth, gnScreenHeight, LogFrames,
	    frames.averageMicros / 1000.0, frames.worstMicros / 1000.0, frames.overThreshold, TicksToMs(worldTicks) / LogFrames);
	for (size_t i = 0; i < PassCount; i++)
		row += fmt::format(",{:.2f}", TicksToMs(LogSum[i]) / LogFrames);
	row += "," + OptionDigits() + "\n";
	std::fputs(row.c_str(), file);
	std::fclose(file);
}

} // namespace

const char *FramePassName(FramePass pass)
{
	switch (pass) {
	case FramePass::Floor: return "floor";
	case FramePass::FloorLight: return "light";
	case FramePass::Marks: return "marks";
	case FramePass::FloorThings: return "rings";
	case FramePass::Sprites: return "sprites";
	case FramePass::Relief: return "relief";
	case FramePass::Colour: return "colour";
	case FramePass::DayNight: return "night";
	case FramePass::Beams: return "beams";
	case FramePass::Particles: return "particles";
	case FramePass::Haze: return "haze";
	case FramePass::Bloom: return "bloom";
	case FramePass::Mood: return "mood";
	case FramePass::Outlines: return "outlines";
	case FramePass::Zoom: return "zoom";
	case FramePass::Count: break;
	}
	return "?";
}

void BeginFramePass(FramePass pass)
{
	PassStart[static_cast<size_t>(pass)] = SDL_GetPerformanceCounter();
}

void EndFramePass(FramePass pass)
{
	const size_t i = static_cast<size_t>(pass);
	const uint64_t now = SDL_GetPerformanceCounter();
	if (PassStart[i] != 0 && now > PassStart[i])
		PassFrame[i] += now - PassStart[i];
	PassStart[i] = 0;
}

void NoteWorldFrameDrawn()
{
	for (size_t i = 0; i < PassCount; i++) {
		ShownSum[i] += PassFrame[i];
		LogSum[i] += PassFrame[i];
	}
	PassFrame.fill(0);
	ShownFrames++;
	LogFrames++;
	const uint64_t now = NowMicros();
	if (ShownSince == 0)
		ShownSince = now;
	if (LogSince == 0)
		LogSince = now;
	if (now - ShownSince >= WindowMicros) {
		std::array<float, PassCount> avg {};
		float world = 0;
		for (size_t i = 0; i < PassCount; i++) {
			avg[i] = TicksToMs(ShownSum[i]) / static_cast<float>(ShownFrames);
			world += avg[i];
		}
		const auto v = [&avg](FramePass p) { return avg[static_cast<size_t>(p)]; };
		PassLines[0] = fmt::format("world {:.1f} ms | floor {:.1f} light {:.1f} marks {:.1f} rings {:.1f} sprites {:.1f}", world,
		    v(FramePass::Floor), v(FramePass::FloorLight), v(FramePass::Marks), v(FramePass::FloorThings), v(FramePass::Sprites));
		PassLines[1] = fmt::format("relief {:.1f} colour {:.1f} night {:.1f} beams {:.1f} particles {:.1f}", v(FramePass::Relief),
		    v(FramePass::Colour), v(FramePass::DayNight), v(FramePass::Beams), v(FramePass::Particles));
		PassLines[2] = fmt::format("haze {:.1f} bloom {:.1f} mood {:.1f} outlines {:.1f} zoom {:.1f}", v(FramePass::Haze), v(FramePass::Bloom),
		    v(FramePass::Mood), v(FramePass::Outlines), v(FramePass::Zoom));
		ShownSum.fill(0);
		ShownFrames = 0;
		ShownSince = now;
	}
	// The log: only while the FPS counter is on, so a session is recorded only when asked for.
	if (!frameflag) {
		LogSum.fill(0);
		LogFrames = 0;
		LogSince = now;
		return;
	}
	if (now - LogSince >= LogMicros) {
		WriteLogRow(Ring.Summarise(now, LogMicros, StutterMicros));
		LogSum.fill(0);
		LogFrames = 0;
		LogSince = now;
	}
}

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
		// Over a second is the window coming back from the background (RenderPresent skips while inactive) or a level
		// load, not a frame of play: not counted, or the line would show a false stutter (round 1 audit).
		if (micros <= 1000000)
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
	// Stage 0 of the GPU renderer plan: the world draw's passes, a line a group.
	for (size_t i = 0; i < PassLines.size(); i++) {
		if (!PassLines[i].empty())
			DrawString(out, PassLines[i], position + Displacement { 0, 16 * static_cast<int>(i + 1) }, { UiFlags::ColorRed });
	}
}

} // namespace devilution::oracool
