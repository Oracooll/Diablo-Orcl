/**
 * @file oracool_frame_smoothing_test.cpp
 *
 * The frame-time line under the FPS counter (user, 2026-10-10): the ring that keeps the last second of frame lengths,
 * and the line it is shown as. Pure maths - no window, no archive.
 */

#include <gtest/gtest.h>

#include <cstring>
#include <string>

#include "oracool/frame_timing.h"

using namespace devilution;

namespace {

constexpr uint64_t Second = 1000000;

TEST(OracoolFrameTiming, SummarisesTheWindowWorstAverageAndStutters)
{
	oracool::FrameTimingRing ring;
	uint64_t now = 10 * Second;
	// Nine 16 ms frames and one 34 ms hitch.
	for (int i = 0; i < 9; i++) {
		now += 16000;
		ring.Push(now, 16000);
	}
	now += 34000;
	ring.Push(now, 34000);

	const oracool::FrameTimingStats stats = ring.Summarise(now, Second, 25000);
	EXPECT_EQ(stats.frames, 10u);
	EXPECT_EQ(stats.worstMicros, 34000u);
	EXPECT_EQ(stats.averageMicros, (9u * 16000u + 34000u) / 10u);
	EXPECT_EQ(stats.overThreshold, 1u) << "only the hitch ran past 25 ms";
}

TEST(OracoolFrameTiming, FramesOlderThanTheWindowDoNotCount)
{
	oracool::FrameTimingRing ring;
	ring.Push(1 * Second, 80000); // a load-screen hitch, long ago
	ring.Push(3 * Second, 16000);
	ring.Push(3 * Second + 16000, 16000);

	const oracool::FrameTimingStats stats = ring.Summarise(3 * Second + 16000, Second, 25000);
	EXPECT_EQ(stats.frames, 2u);
	EXPECT_EQ(stats.worstMicros, 16000u) << "the 80 ms frame two seconds back was counted";
	EXPECT_EQ(stats.overThreshold, 0u);
}

TEST(OracoolFrameTiming, AnEmptyRingSaysNothingWrong)
{
	const oracool::FrameTimingRing ring;
	const oracool::FrameTimingStats stats = ring.Summarise(Second, Second, 25000);
	EXPECT_EQ(stats.frames, 0u);
	EXPECT_EQ(stats.worstMicros, 0u);
	EXPECT_EQ(stats.averageMicros, 0u);
}

TEST(OracoolFrameTiming, TheRingWrapsAndKeepsTheNewest)
{
	oracool::FrameTimingRing ring;
	uint64_t now = 0;
	// Twice the capacity: the first half (all 50 ms) must be gone, the second half (all 1 ms) kept.
	for (size_t i = 0; i < oracool::FrameTimingRing::Capacity; i++) {
		now += 1;
		ring.Push(now, 50000);
	}
	for (size_t i = 0; i < oracool::FrameTimingRing::Capacity; i++) {
		now += 1;
		ring.Push(now, 1000);
	}
	EXPECT_EQ(ring.size(), oracool::FrameTimingRing::Capacity);
	const oracool::FrameTimingStats stats = ring.Summarise(now, Second, 25000);
	EXPECT_EQ(stats.frames, oracool::FrameTimingRing::Capacity);
	EXPECT_EQ(stats.worstMicros, 1000u) << "an overwritten frame was still counted";
}

TEST(OracoolFrameTiming, TheLineReadsAsSpecified)
{
	oracool::FrameTimingStats stats;
	stats.frames = 60;
	stats.worstMicros = 34200;
	stats.averageMicros = 16680;
	stats.overThreshold = 3;
	char line[64];
	oracool::FormatFrameTiming(stats, line);
	EXPECT_EQ(std::string(line), "worst 34 ms | avg 16.7 ms | >25 ms: 3");
	EXPECT_LT(std::strlen(line), sizeof(line));
}

} // namespace
