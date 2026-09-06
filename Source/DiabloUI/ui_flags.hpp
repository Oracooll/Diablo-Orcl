#pragma once

#include <cstdint>

#include "utils/enum_traits.h"

namespace devilution {

// Widened from uint32_t (v0.2.0+): the 32-bit version had all 32 bits already assigned - one
// bit per color, no room for a new hue without dropping something else. Never serialized to
// any save/network struct, so this is a purely additive change. Bits 32+ must use the ULL
// suffix (1ULL << N), not the plain 1 used below 32 - a bare `1 << 32` is undefined behavior
// on a 32-bit int regardless of what it gets assigned into.
enum class UiFlags : uint64_t {
	// clang-format off
	None               = 0,

	FontSize12         = 1 << 0,
	FontSize24         = 1 << 1,
	FontSize30         = 1 << 2,
	FontSize42         = 1 << 3,
	FontSize46         = 1 << 4,
	FontSizeDialog     = 1 << 5,

	ColorUiGold        = 1 << 6,
	ColorUiSilver      = 1 << 7,
	ColorUiGoldDark    = 1 << 8,
	ColorUiSilverDark  = 1 << 9,
	ColorDialogWhite   = 1 << 10,
	ColorDialogYellow  = 1 << 11,
	ColorDialogRed     = 1 << 12,
	ColorYellow        = 1 << 13,
	ColorGold          = 1 << 14,
	ColorBlack         = 1 << 15,
	ColorWhite         = 1 << 16,
	ColorWhitegold     = 1 << 17,
	ColorRed           = 1 << 18,
	ColorBlue          = 1 << 19,
	ColorOrange        = 1 << 20,
	ColorButtonface    = 1 << 21,
	ColorButtonpushed  = 1 << 22,

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
	ColorOracoolYellow     = 1ULL << 32,
	ColorOracoolYellowDark = 1ULL << 33,
	/** @brief Oracool: green text on the injected PAL8_GREEN ramp - set items, Belzebub-style (2026-08-15). */
	ColorOracoolGreen      = 1ULL << 34,

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
	ColorGray5             = 1ULL << 40,
	/** @brief Oracool: BE-2 of the legend, the beige ramp two in - Primal items, game-wide (user, 2026-09-07). */
	ColorBeige2            = 1ULL << 41,
	/** @brief Oracool: YL-3 of the legend, the bright yellow minis three in - Rare items (user, 2026-09-07). */
	ColorYellow3           = 1ULL << 42,

	// Bits 40+ are free for a future color or flag - see the widening note above. The entries above
	// were the first assigned: every existing font-color .trn (Packaging/resources/assets/fonts/*.trn)
	// works by remapping a 16-shade ramp inside vanilla Diablo's own palette, and that
	// palette's only named bright-color blocks (Source/engine/palette.h) are blue, red,
	// yellow, orange, beige, and gray - there is no green/cyan/purple block to remap into.
	// A genuinely new hue needs a palette edit (out of scope here, and riskier than this
	// pass warrants since the palette is shared by every other rendering path), not just an
	// available bit; this widening only removes the bit-count ceiling for whenever that
	// happens.
	// clang-format on
};
use_enum_as_flags(UiFlags);

} // namespace devilution
