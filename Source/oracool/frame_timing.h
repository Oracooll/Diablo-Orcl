#pragma once
/**
 * @file oracool/frame_timing.h
 *
 * Oracool: the frame-time line under the FPS counter (user, 2026-10-10). An average FPS hides a stutter - sixty frames
 * a second with one of them 80 ms long still reads "59.0 FPS" - so this keeps the length of every presented frame of
 * the last second and says the worst, the average, and how many ran past 25 ms:
 *
 *     worst 34 ms · avg 16.7 ms · >25 ms: 3
 *
 * Measured present to present (NoteFramePresented, from RenderPresent), so it is what the eye gets: the frame limiter's
 * sleep and a hitch in the game logic both count. Drawn only while the FPS counter is on (frameflag).
 */

#include <array>
#include <cstddef>
#include <cstdint>

#include "engine/point.hpp"
#include "engine/surface.hpp"
#include "utils/attributes.h"

namespace devilution::oracool {

/** @brief What one window of frames came to. All times in microseconds. */
struct FrameTimingStats {
	uint32_t frames = 0;
	uint32_t worstMicros = 0;
	uint32_t averageMicros = 0;
	uint32_t overThreshold = 0;
};

/**
 * @brief The last frames' lengths, each with the moment it ended. Pure - no clock of its own - so the maths is testable.
 * 1024 entries is a second at 1000 FPS; anything older than the window is ignored rather than removed.
 */
class FrameTimingRing {
public:
	static constexpr size_t Capacity = 1024;

	/** @brief One frame of @p durationMicros that ended at @p endMicros (any monotonic microsecond clock). */
	void Push(uint64_t endMicros, uint32_t durationMicros)
	{
		ends_[next_] = endMicros;
		durations_[next_] = durationMicros;
		next_ = (next_ + 1) % Capacity;
		if (count_ < Capacity)
			count_++;
	}

	/** @brief The frames that ended within @p windowMicros before @p nowMicros, and how many took longer than @p thresholdMicros. */
	[[nodiscard]] FrameTimingStats Summarise(uint64_t nowMicros, uint64_t windowMicros, uint32_t thresholdMicros) const
	{
		FrameTimingStats stats;
		uint64_t total = 0;
		for (size_t k = 0; k < count_; k++) {
			const size_t i = (next_ + Capacity - 1 - k) % Capacity; // newest first
			if (ends_[i] > nowMicros || nowMicros - ends_[i] > windowMicros)
				break; // older than the window, and so is everything behind it
			stats.frames++;
			total += durations_[i];
			if (durations_[i] > stats.worstMicros)
				stats.worstMicros = durations_[i];
			if (durations_[i] > thresholdMicros)
				stats.overThreshold++;
		}
		if (stats.frames > 0)
			stats.averageMicros = static_cast<uint32_t>(total / stats.frames);
		return stats;
	}

	void Clear()
	{
		next_ = 0;
		count_ = 0;
	}

	[[nodiscard]] size_t size() const { return count_; }

private:
	std::array<uint64_t, Capacity> ends_ {};
	std::array<uint32_t, Capacity> durations_ {};
	size_t next_ = 0;
	size_t count_ = 0;
};

/** @brief The line DrawFrameTiming shows for @p stats, e.g. "worst 34 ms · avg 16.7 ms · >25 ms: 3". At most 63 bytes. */
DVL_API_FOR_TEST void FormatFrameTiming(const FrameTimingStats &stats, char (&out)[64]);

/** @brief Once a presented frame, from RenderPresent: the time since the last one goes into the ring. */
void NoteFramePresented();

/**
 * @brief The frame-time line at @p position, in DrawFPS's font and colour, while the FPS counter is on. Refreshed four
 * times a second, so it can be read.
 */
void DrawFrameTiming(const Surface &out, Point position);

/**
 * GPU renderer plan, Stage 0 (user, 2026-10-11): what each pass of the world draw costs. DrawGame wraps every pass in a
 * ScopedFramePass; the averages per drawn world frame over the last second are shown under the frame-time line while the
 * FPS counter is on, and every ten seconds of play with it on a row goes to frame_passes.csv in the pref folder (the
 * level, zoom, screen size, the effect options and every pass), so a play session can be read back afterwards.
 */
enum class FramePass : uint8_t {
	Floor,       // DrawFloor
	FloorLight,  // smooth floors, wall shadows, ray-traced shadows
	Marks,       // floor marks and loot glows
	FloorThings, // floor-pass objects, aura and skill rings
	Sprites,     // DrawTileContent: walls, objects, items, monsters, heroes, missiles (with spell glow, smooth animation)
	Relief,      // lit surface relief (keep + pass)
	Colour,      // coloured light and torch flicker
	DayNight,    // Tristram's day and night
	Beams,       // loot beams
	Particles,
	Haze,        // lava haze and water light
	Bloom,
	Mood,        // area mood and vignette
	Outlines,    // the deferred monster outlines
	Zoom,        // ZoomScale: the zoom filter and sharpening
	Gpu,         // Stage 1: the world up to the graphics card, its passes, and the picture back
	Count,
};

/** @brief Starts timing @p pass (cumulative within the frame). */
void BeginFramePass(FramePass pass);
/** @brief Stops timing @p pass. */
void EndFramePass(FramePass pass);
/** @brief Once a world frame, at the end of DrawGame: the frame's pass times go into the averages. */
void NoteWorldFrameDrawn();

struct ScopedFramePass {
	explicit ScopedFramePass(FramePass pass)
	    : pass_(pass)
	{
		BeginFramePass(pass_);
	}
	~ScopedFramePass() { EndFramePass(pass_); }
	ScopedFramePass(const ScopedFramePass &) = delete;
	ScopedFramePass &operator=(const ScopedFramePass &) = delete;

private:
	FramePass pass_;
};

/** @brief The short name a pass is shown and logged under ("floor", "sprites"...). */
DVL_API_FOR_TEST const char *FramePassName(FramePass pass);

} // namespace devilution::oracool
