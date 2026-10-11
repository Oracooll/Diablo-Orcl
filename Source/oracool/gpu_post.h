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

#include "levels/gendung.h"

namespace devilution {
struct Surface;
}

namespace devilution::oracool {

/** @brief The world zoom as the card does it: none (1.0x), nearest, or the filter (Scale2x at 2x, sharp bilinear else). */
enum class GpuZoom : uint8_t {
	None,
	Nearest,
	Filtered,
};

struct GpuWorldPostSettings {
	bool bloom = false;
	bool mood = false;
	dungeon_type moodType = DTYPE_TOWN;
	bool vignette = false;
	GpuZoom zoom = GpuZoom::None;
	bool sharpen = false; // after a zoom only
};

/** @brief True when the card can take the world's tail passes this frame: the option on and a device made (lazily). */
bool GpuWorldPostAvailable();

/**
 * @brief Bloom, mood and vignette over @p src (the pre-zoom visible world), then the zoom and sharpening into @p dst
 * (the viewport; the same surface as @p src at 1.0x). False when the card could not do it, with nothing written - the
 * caller runs the CPU passes then.
 */
bool GpuWorldPost(const Surface &src, const Surface &dst, const GpuWorldPostSettings &settings);

/** @brief Releases the device. From dx_cleanup. */
void ShutdownGpuWorldPost();

/** @brief Test hook: the next device is Direct3D's software rasteriser (WARP), so the comparison test needs no GPU. */
void GpuWorldPostUseSoftwareDeviceForTests();

/** @brief What the last attempt to make the device said: "" while none was tried, else the adapter or the error. */
const char *GpuWorldPostStatus();

} // namespace devilution::oracool
