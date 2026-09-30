#pragma once
/**
 * @file oracool/sprite_colours.h
 *
 * Oracool: a sprite sheet's own colours - the 32-bit renderer reaching the heroes (2026-09-17).
 *
 * The screen has been 32-bit since v1.11, and UI art, text and the frost tint all draw colour
 * VALUES. Hero bodies did not: a player sprite is palette indices, DrawPlayer sent them through the
 * level palette and the index light tables, and so anything done to a hero - the Barbarian's dye, a
 * PNG sheet supplied for a class - could only ever land on one of the palette's 256 entries (128,
 * for an import). This is the missing piece: what each index of a sheet MEANS, as a colour.
 *
 * An index is one of two things. Left alone, it is the level palette's entry, lit by the level's own
 * light table - which is pixel for pixel what ClxDrawLight draws, so a sheet whose every index is
 * left alone looks exactly as it did. Given a colour of its own, it is that colour, and lighting
 * darkens it arithmetically by as much as the light table darkens its FALLBACK index - the palette
 * entry nearest to it - so a hand-picked blue falls into shadow at the pace the palette's blue does.
 *
 * The fallback is also what an INDEXED target gets (the golden tests, offscreen sprite work): there
 * is no colour to write there, only an index, and the fallback is the honest nearest one.
 */

#include <array>
#include <cstdint>

#include "engine/clx_sprite.hpp"
#include "engine/point.hpp"
#include "engine/surface.hpp"
#include "lighting.h"

namespace devilution::oracool {

struct SpriteColours {
	/** @brief Every index left alone: the level palette, the level's light. */
	SpriteColours();
	/**
	 * @brief The colours, not the lit tables: the sprite mixer's worker copies a dye while the main thread may be building
	 * one of its tables lazily, and a torn table could be copied marked as built (round 14 audit, v1.12.239). The copy
	 * builds its own tables on first use.
	 */
	SpriteColours(const SpriteColours &other)
	    : rgb_(other.rgb_)
	    , own_(other.own_)
	    , fallback_(other.fallback_)
	{
	}
	SpriteColours &operator=(const SpriteColours &other)
	{
		rgb_ = other.rgb_;
		own_ = other.own_;
		fallback_ = other.fallback_;
		builtFor_.fill(0);
		return *this;
	}

	/** @brief Gives @p index the colour @p rgb (0xRRGGBB), shaded like - and standing in as - @p fallbackIndex. */
	void Set(uint8_t index, uint32_t rgb, uint8_t fallbackIndex);

	[[nodiscard]] bool HasOwn(uint8_t index) const { return own_[index] != 0; }
	[[nodiscard]] uint32_t Own(uint8_t index) const { return rgb_[index]; }
	[[nodiscard]] uint8_t Fallback(uint8_t index) const { return fallback_[index]; }
	/** @brief The index translation an 8-bit target draws through, before any lighting. */
	[[nodiscard]] const uint8_t *FallbackTrn() const { return fallback_.data(); }

	/** @brief 256 colour values for light level @p level (0 = full light). Cached per palette generation. */
	[[nodiscard]] const uint32_t *Table(int level) const;
	/** @brief The same through the infravision translation. */
	[[nodiscard]] const uint32_t *InfravisionTable() const;

private:
	std::array<uint32_t, 256> rgb_ {};
	std::array<uint8_t, 256> own_ {};
	std::array<uint8_t, 256> fallback_;

	// NumLightingLevels lit tables and one more for infravision.
	mutable std::array<std::array<uint32_t, 256>, NumLightingLevels + 1> tables_;
	mutable std::array<uint32_t, NumLightingLevels + 1> builtFor_ {};

	void Build(size_t slot, const uint8_t *indexTable) const;
};

/** @brief Draws with infravision rather than a light level. */
constexpr int InfravisionLight = -1;

/**
 * @brief Draws @p sprite through @p colours at @p lightLevel (or InfravisionLight), position being
 * the sprite's bottom-left as for ClxDraw. A 32-bit target gets colour values; an indexed one gets
 * the fallback indices through the same light table.
 */
void DrawSpriteWithColours(const Surface &out, Point position, ClxSprite sprite, const SpriteColours &colours, int lightLevel);

} // namespace devilution::oracool
