/**
 * @file oracool/missile_tint.h
 *
 * Recolouring by colour VALUES, for the 32-bit renderer (v1.12.211, the user's animation review, 2026-09-27).
 *
 * A PNG effect sheet now carries its own colours (MissileFileData::colours), so its sprites index a table of its own and a
 * palette translation (TRN) would scramble them. Every tint here therefore works on a table of 256 colour values - the
 * sheet's own, or the level palette's for a CL2 - and returns another, for ClxDrawRgbMap. On an 8-bit target there are no
 * colour values to change and callers draw untinted.
 */
#pragma once

#include <cstdint>

namespace devilution::oracool {

enum class Tint : uint8_t {
	None,
	/** Every colour taken to the given hue, its brightness kept: gold flame, dark-green dead, a violet flash. */
	Hue,
	/** Hue, with two bands of light running through the brightness levels as the effect plays (the war cry ring). */
	HueCycle,
	/** Brown to dark orange and back, bands running through it: the Earthquake's molten floor. */
	Earthquake,
	/** A cold blue glaze over a body (the ice armours, in place of the shell). */
	Ice,
	/** A pale violet shimmer over a body (Astral Projection, in place of its overlay). */
	Astral,
	/** A lavender glow fading off a healed minion (Dark Mending, in place of its sheet). `strength` is 0..1. */
	Mend,
	/**
	 * Bands of light running through the brightness levels on the game clock alone, so a looping or held sheet cycles
	 * without a seam (dev notes, 2026-09-30: Frozen Orb, Brittle Ground, the ice armours). `rgb` 0 keeps the sheet's
	 * own colours; any other glazes them toward that hue, as Ice does.
	 */
	Glint,
	/**
	 * Electric bluish-white glazed over a body, bands of light racing through it on the clock (user, 2026-10-01: Conduit
	 * on the Sorcerer for as long as it lasts).
	 */
	Electric,
	/**
	 * Every colour pure white, lightning-blue bands running through it by brightness on the clock; black (a shadow) kept
	 * (user, 2026-10-01: Lightning Clone).
	 */
	Clone,
};

/** @brief 0xRRGGBB. */
constexpr uint32_t Rgb(uint8_t r, uint8_t g, uint8_t b)
{
	return (static_cast<uint32_t>(r) << 16) | (static_cast<uint32_t>(g) << 8) | b;
}

/** @brief The named hues the Skill Cards pages offer (2026-09-28), by the same names, for Tint::Hue. */
namespace hue {
constexpr uint32_t PaladinGold = Rgb(244, 204, 96);
constexpr uint32_t VengeanceAmber = Rgb(255, 196, 96);
constexpr uint32_t Infrared = Rgb(255, 56, 32);
constexpr uint32_t FireOrange = Rgb(255, 128, 40);
constexpr uint32_t HolyBlue = Rgb(96, 150, 255);
constexpr uint32_t IceBlue = Rgb(150, 210, 255);
constexpr uint32_t SpectralLavender = Rgb(206, 182, 255);
constexpr uint32_t PaleWarm = Rgb(236, 220, 186);
/** The three ice armours' glints (dev note, 2026-09-30: "white, yellow, darker blue"). */
constexpr uint32_t FrostWhite = Rgb(236, 244, 255);
constexpr uint32_t ShiverYellow = Rgb(255, 220, 110);
constexpr uint32_t DeepIceBlue = Rgb(64, 104, 224);
} // namespace hue

/**
 * @brief @p base (256 colour values, already lit) recoloured by @p tint. @p rgb is the hue for Hue and HueCycle;
 * @p progress is how far the effect has played, 0..1 (HueCycle), or the strength (Mend). Tables are cached (16 of them,
 * keyed by the base's values, the tint, the hue, the progress and an 8 ms step of the clock), so a frame of like missiles
 * builds one; the result may be reused by a later call - draw with it at once.
 */
const uint32_t *TintedTable(const uint32_t *base, Tint tint, uint32_t rgb, double progress);

/** @brief The level palette through light level @p lightLevel (0 = full light), as colour values: the base for a CL2. */
const uint32_t *LitPaletteTable(int lightLevel);

/** @brief The war cry ring's hue for the class-tree skill that raised it (a ClassTreeSkill value; 0xFFFF for none). */
uint32_t RingHueForSkill(uint16_t classTreeSkill);

} // namespace devilution::oracool
