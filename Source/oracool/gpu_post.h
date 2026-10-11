/**
 * @file oracool/gpu_post.h
 *
 * The GPU renderer plan, Stage 1 (user, 2026-10-11: "start stage 1"): the world's tail passes on the graphics card.
 *
 * The CPU draws the world as before - floor, lighting, sprites, relief, colour, day and night, beams, particles, haze -
 * and the monster outlines. Then, instead of the CPU's bloom, area mood and vignette, zoom filter and zoom sharpening,
 * the world is uploaded to a Direct3D 11 device, those passes run as pixel shaders, and the finished (zoomed) picture is
 * read back into the back buffer, where the HUD is drawn over it as ever. So the HUD, its see-through panels, the
 * screenshots and the window keep working exactly as they do; only the cost moves.
 *
 * The shaders repeat the CPU passes' own integer sums with the same tables (post_fx.h's shared ones), so the picture is
 * the same pixel for pixel - the comparison test runs both on the same frame. The one difference is the order of the
 * monster outlines: on the CPU they go on after the bloom and mood, here before (they are drawn into the world the
 * card receives) - a shade at most, on an outline.
 *
 * Behind the option gpuRenderer (Graphics Card Renderer, on by default); if the device cannot be made, or any call
 * fails, the game says so in its log once and the CPU passes run for the rest of the session. Windows only: elsewhere
 * nothing is ever available and the CPU passes run.
 */
#pragma once

#include <cstdint>
#include <vector>

#include "engine/point.hpp"
#include "engine/rectangle.hpp"
#include "levels/gendung.h"
#include "oracool/splats.h"

namespace devilution {
struct Surface;
}

namespace devilution::oracool {

struct TintPlan;
struct ReliefPlan;
struct HazePlan;
struct GpuStage3;

/** @brief The world zoom as the card does it: none (1.0x), nearest, or the filter (Scale2x at 2x, sharp bilinear else). */
enum class GpuZoom : uint8_t {
	None,
	Nearest,
	Filtered,
	Smooth, // Pixel Art Smoothing (user, 2026-10-11): xBR's edge rule at any scale - card only, the CPU falls back to Filtered
};

struct GpuWorldPostSettings {
	// Stage 3 (user, 2026-10-11): the effects only the card runs - each its own option (scrollrt.cpp sets them).
	bool perPixelLights = false;
	bool litSprites = false; // in place of the relief, when the relief applies
	bool wideBloom = false;
	bool lightShafts = false;
	bool heatDistortion = false;
	bool filmicGrade = false;
	bool bloom = false;
	bool mood = false;
	dungeon_type moodType = DTYPE_TOWN;
	bool vignette = false;
	GpuZoom zoom = GpuZoom::None;
	bool sharpen = false; // after a zoom only
	/** Read the picture the card finished a frame ago (user, 2026-10-11): the CPU never waits; one frame later on screen. */
	bool pipelined = false;
};

/**
 * @brief Stage 2 (user, 2026-10-11: "proceed with the other stages"): the passes between the sprites and the bloom, in
 * the CPU's order - the relief (against the world GpuKeepWorld sent before the sprites), the coloured light or Tristram's
 * day and night, the beams' and particles' splats, the water light and lava haze - and the deferred monster outlines,
 * laid on after the mood as the CPU lays them. Any null part is skipped.
 */
struct GpuStage2 {
	const ReliefPlan *relief = nullptr;
	const TintPlan *tint = nullptr;
	const std::vector<Splat> *splats = nullptr;
	const HazePlan *haze = nullptr;
	/** The outlines, drawn on their own: a surface the world region's size, X byte 0xFF where nothing is. */
	const Surface *outlines = nullptr;
	/** The part of @p outlines that may hold any, in the world region's coordinates (empty: none). */
	Rectangle outlineBox {};
	/** Stage 3's inputs, when any of its effects is on. */
	const GpuStage3 *stage3 = nullptr;
};

/** @brief A light for Stage 3's passes, in tile coordinates. Kind: 0 a hero's own (neutral), 1 a flame, 2 a spell or portal. */
struct GpuLight {
	float x, y, reach, intensity;
	float r, g, b;
	int kind;
};
/** @brief A source of heat for the distortion, in tile coordinates. */
struct GpuHeat {
	float x, y, radius, strength;
};
/** @brief Stage 3's inputs: where the world is on the floor grid, the lights, the heat, what stops light, the clock. */
struct GpuStage3 {
	Point firstTile {};
	Point firstTileBuffer {};
	const GpuLight *lights = nullptr;
	size_t lightCount = 0;
	const GpuHeat *heat = nullptr;
	size_t heatCount = 0;
	const uint8_t *blocks = nullptr; // FieldW x FieldH, x major: 1 where a tile stops light
	const uint8_t *liquid = nullptr; // the haze's liquid map, or null
	float seconds = 0;
};

/** @brief Sends the world as it stands - before the sprites - for this frame's relief. */
void GpuKeepWorld(const Surface &src);

/** @brief True when the card can take the world's tail passes this frame: the option on and a device made (lazily). */
bool GpuWorldPostAvailable();

/**
 * @brief Bloom, mood and vignette over @p src (the pre-zoom visible world), then the zoom and sharpening into @p dst
 * (the viewport; the same surface as @p src at 1.0x). False when the card could not do it, with nothing written - the
 * caller runs the CPU passes then.
 */
bool GpuWorldPost(const Surface &src, const Surface &dst, const GpuWorldPostSettings &settings, const GpuStage2 *stage2 = nullptr);

/** @brief A world frame begins (DrawGame's first line): what a pipelined read-back counts by. */
void GpuBeginWorldFrame();

/** @brief Releases the device. From dx_cleanup. */
void ShutdownGpuWorldPost();

/** @brief Test hook: the next device is Direct3D's software rasteriser (WARP), so the comparison test needs no GPU. */
void GpuWorldPostUseSoftwareDeviceForTests();

/** @brief What the last attempt to make the device said: "" while none was tried, else the adapter or the error. */
const char *GpuWorldPostStatus();

} // namespace devilution::oracool
