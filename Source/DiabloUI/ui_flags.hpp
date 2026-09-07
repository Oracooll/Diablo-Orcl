#pragma once

#include <cstdint>

#include "utils/enum_traits.h"

namespace devilution {

// Widened from uint32_t (v0.2.0+): the 32-bit version had all 32 bits already assigned - one
// bit per color, no room for a new hue without dropping something else. Never serialized to
// any save/network struct, so this is a purely additive change. Bits 32+ must use the ULL
// suffix (1ULL << N), not the plain 1 used below 32 - a bare `1 << 32` is undefined behavior
// on a 32-bit int regardless of what it gets assigned into.
/** @brief Where the 12-bit colour field sits in UiFlags - see ColorMask. */
constexpr unsigned UiFlagsColorShift = 48;

enum class UiFlags : uint64_t {
	// clang-format off
	None               = 0,

	FontSize12         = 1 << 0,
	FontSize24         = 1 << 1,
	FontSize30         = 1 << 2,
	FontSize42         = 1 << 3,
	FontSize46         = 1 << 4,
	FontSizeDialog     = 1 << 5,

	/**
	 * THE COLOUR FIELD (2026-09-07). A colour used to be one BIT each, which is why the word ran to
	 * 64 bits and still had only 16 to spare after the legend's first eight. It is a 12-bit NUMBER
	 * now, bits 48-59: the value is the colour's index, 0 meaning "none" (drawn as Whitegold, the
	 * fallback every unrecognised flag always got). 4096 colours fit; the palette offers 66.
	 *
	 * The names below are unchanged and still compose with the layout bits - ColorWhite | AlignCenter
	 * is what it always was. Two things are different: two colours must never be ORed together (the
	 * indices would add up to a third colour; the bit scheme used to pick one by precedence), and a
	 * colour is TESTED with HasColor or read with UiFlagsColorIndex, never with HasAnyOf. Cleared as
	 * a whole with WithoutColor (ColorMask), so the outline pass never again needs a list of bits.
	 */
	ColorMask          = 0xFFFULL << UiFlagsColorShift,
	ColorUiGold        = 1ULL << UiFlagsColorShift,
	ColorUiSilver      = 2ULL << UiFlagsColorShift,
	ColorUiGoldDark    = 3ULL << UiFlagsColorShift,
	ColorUiSilverDark  = 4ULL << UiFlagsColorShift,
	ColorDialogWhite   = 5ULL << UiFlagsColorShift,
	ColorDialogYellow  = 6ULL << UiFlagsColorShift,
	ColorDialogRed     = 7ULL << UiFlagsColorShift,
	ColorYellow        = 8ULL << UiFlagsColorShift,
	ColorGold          = 9ULL << UiFlagsColorShift,
	ColorBlack         = 10ULL << UiFlagsColorShift,
	ColorWhite         = 11ULL << UiFlagsColorShift,
	ColorWhitegold     = 12ULL << UiFlagsColorShift,
	ColorRed           = 13ULL << UiFlagsColorShift,
	ColorBlue          = 14ULL << UiFlagsColorShift,
	ColorOrange        = 15ULL << UiFlagsColorShift,
	ColorButtonface    = 16ULL << UiFlagsColorShift,
	ColorButtonpushed  = 17ULL << UiFlagsColorShift,

	AlignCenter        = 1 << 23,
	AlignRight         = 1 << 24,
	VerticalCenter     = 1 << 25,

	KerningFitSpacing  = 1 << 26,

	ElementDisabled    = 1 << 27,
	ElementHidden      = 1 << 28,

	PentaCursor        = 1 << 29,
	Outlined           = 1 << 30,

	/** @brief Ensures that the if current element is active that the next element is also visible. */
	NeedsNextElement   = 1U << 31U,

	/**
	 * @brief Oracool: the focus glow's yellow, and its dim companion.
	 *
	 * The first users of the bits the widening below made room for, and the note beneath is what
	 * made them possible: the palette's named bright blocks DO include yellow, at 128-135. The
	 * shipped `yellow.trn` was no use - it targets 144-151, which is yellow in the level palette
	 * and pink in ui_art\diablo.pal. These two point at 128-135 instead; see
	 * tools/MakeYellowFontTrn.ps1 for the ramp and why each entry is doubled.
	 */
	ColorOracoolYellow     = 18ULL << UiFlagsColorShift,
	ColorOracoolYellowDark = 19ULL << UiFlagsColorShift,
	/** @brief Oracool: green text on the injected PAL8_GREEN ramp - set items, Belzebub-style (2026-08-15). */
	ColorOracoolGreen      = 20ULL << UiFlagsColorShift,

	/**
	 * @brief The four small fonts, derived from Font 12 (see the notice in docs/THIRD_PARTY.md).
	 *
	 * Up here with the other widened bits rather than beside FontSize12-46 at bits 0-5, because
	 * those six are contiguous and several places mask them as a block; appending keeps that block
	 * intact. GetSizeFromFlags checks them after the original six, so a caller that sets none
	 * still lands on FontSize12 exactly as before.
	 */
	FontSize11             = 1ULL << 35,
	FontSize10             = 1ULL << 36,
	FontSize9              = 1ULL << 37,
	FontSize8              = 1ULL << 38,

	/**
	 * @brief Oracool: a DROP SHADOW under the glyphs - a black copy, two pixels down and LEFT.
	 *
	 * Not the same thing as Outlined, which rings a glyph on all four sides with
	 * ClxDrawOutlineSkipColorZero and reads as a hard sticker. This is vanilla Diablo's character
	 * sheet look (user, 2026-08-29): the text sits ON the stone and casts a shadow onto it.
	 *
	 * Vanilla got it by drawing the whole label string TWICE - black at an offset, then white on
	 * top (see TextShadowOffset in text_render.cpp for the original two lines). This does the same
	 * thing per GLYPH instead, which is one layout pass rather than two and identical on screen for
	 * a horizontal string.
	 *
	 * The black copy is the glyph rendered through an all-zero TRN, which maps every palette index
	 * to 0 - the palette's black. A CLX sprite carries its transparency in the run-length structure
	 * rather than in a key colour, so remapping every index to black yields a solid silhouette of
	 * exactly the glyph's shape, which is what a shadow is.
	 */
	Shadowed               = 1ULL << 39,
	/**
	 * @brief Oracool: GR-5 of the font colour legend (06-Reference/Font-Colour-Legend.html) - the
	 * gray ramp five entries in. Socketed drops on the ground and the "Sockets: x/y" row (user,
	 * 2026-09-07). The .trn is oracool_assets/fonts/oracool_gray5.trn; identity except 192-207.
	 */
	ColorGray5             = 21ULL << UiFlagsColorShift,
	/** @brief Oracool: BE-2 of the legend, the beige ramp two in - Primal items, game-wide (user, 2026-09-07). */
	ColorBeige2            = 22ULL << UiFlagsColorShift,
	/** @brief Oracool: YL-3 of the legend, the bright yellow minis three in - Rare items (user, 2026-09-07). */
	ColorYellow3           = 23ULL << UiFlagsColorShift,
	// Five more from the legend (user, 2026-09-07): the item KINDS that read as one colour each.
	/** @brief BR-3: health potions. */
	ColorBrightRed3        = 24ULL << UiFlagsColorShift,
	/** @brief BB-3: mana potions. */
	ColorBrightBlue3       = 25ULL << UiFlagsColorShift,
	/** @brief GD-6: books. */
	ColorGold6             = 26ULL << UiFlagsColorShift,
	/** @brief OR-7: runes. */
	ColorOrange7           = 27ULL << UiFlagsColorShift,
	/** @brief GR-7: ethereal items, and the Ethereal row. Beats the socketed gray on the floor. */
	ColorGray7             = 28ULL << UiFlagsColorShift,

	// Bits 40-47 fell free on 2026-09-07 when the colours moved into the field at 48-59; 60-63 are
	// free too. A NEW COLOUR is a new index in the field, not a bit - see ColorMask.
	// clang-format on
};
use_enum_as_flags(UiFlags);

/** @brief The colour index carried by @p flags: 0 for none, else the index behind a UiFlags::Color name. */
constexpr unsigned UiFlagsColorIndex(UiFlags flags)
{
	return static_cast<unsigned>((static_cast<uint64_t>(flags) >> UiFlagsColorShift) & 0xFFFULL);
}

/** @brief Whether @p flags carries exactly @p color. The replacement for HasAnyOf(flags, UiFlags::ColorX). */
constexpr bool HasColor(UiFlags flags, UiFlags color)
{
	return UiFlagsColorIndex(flags) == UiFlagsColorIndex(color);
}

/** @brief @p flags with its colour cleared - the layout and size bits alone. */
constexpr UiFlags WithoutColor(UiFlags flags)
{
	return static_cast<UiFlags>(static_cast<uint64_t>(flags) & ~(0xFFFULL << UiFlagsColorShift));
}

/** @brief @p flags with its colour replaced by @p color - the safe way to change a colour a style already has. */
constexpr UiFlags WithColor(UiFlags flags, UiFlags color)
{
	return static_cast<UiFlags>(static_cast<uint64_t>(WithoutColor(flags)) | (static_cast<uint64_t>(color) & (0xFFFULL << UiFlagsColorShift)));
}

} // namespace devilution
