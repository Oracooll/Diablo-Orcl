/**
 * @file text_render.cpp
 *
 * Text rendering.
 */
#include "text_render.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <utility>
#include <variant>

#include <fmt/core.h>

#include "DiabloUI/ui_flags.hpp"
#include "engine/clx_sprite.hpp"
#include "engine/load_cel.hpp"
#include "engine/load_clx.hpp"
#include "engine/load_file.hpp"
#include "engine/load_pcx.hpp"
#include "engine/point.hpp"
#include "engine/rectangle.hpp"
#include "engine/palette.h"
#include "engine/render/clx_render.hpp"
#include "engine/render/primitive_render.hpp"
#include "engine/surface.hpp"
#include "utils/algorithm/container.hpp"
#include "utils/language.h"
#include "utils/log.hpp"
#include "utils/stdcompat/optional.hpp"
#include "utils/stdcompat/string_view.hpp"
#include "utils/str_cat.hpp"
#include "utils/utf8.hpp"

namespace devilution {

OptionalOwnedClxSpriteList pSPentSpn2Cels;

namespace {

constexpr char32_t ZWSP = U'\u200B'; // Zero-width space

struct OwnedFontStack {
	OptionalOwnedClxSpriteList baseFont;
	OptionalOwnedClxSpriteList overrideFont;
};

struct FontStack {
	OptionalClxSpriteList baseFont;
	OptionalClxSpriteList overrideFont;

	FontStack() = default;

	explicit FontStack(const OwnedFontStack &owned)
	{
		if (owned.baseFont.has_value()) baseFont.emplace(*owned.baseFont);
		if (owned.overrideFont.has_value()) overrideFont.emplace(*owned.overrideFont);
	}

	[[nodiscard]] bool has_value() const // NOLINT(readability-identifier-naming)
	{
		return baseFont.has_value() || overrideFont.has_value();
	}

	[[nodiscard]] ClxSprite glyph(size_t i) const
	{
		if (overrideFont.has_value()) {
			ClxSprite overrideGlyph = (*overrideFont)[i];
			if (overrideGlyph.width() != 0) return overrideGlyph;
		}
		return (*baseFont)[i];
	}
};

std::unordered_map<uint32_t, OwnedFontStack> Fonts;

// Indexed by GameFontTables. The last four are derived from Font 12 - see docs/THIRD_PARTY.md.
std::array<int, 10> FontSizes = { 12, 24, 30, 42, 46, 22, 11, 10, 9, 8 };
constexpr std::array<int, 10> LineHeights = { 12, 26, 38, 42, 50, 22, 11, 10, 9, 8 };
constexpr int SmallFontTallLineHeight = 16;
std::array<int, 10> BaseLineOffset = { -3, -2, -3, -6, -7, 3, -3, -3, -2, -2 };

std::array<const char *, 36> ColorTranslations = {
	"fonts\\goldui.trn",
	"fonts\\grayui.trn",
	"fonts\\golduis.trn",
	"fonts\\grayuis.trn",

	nullptr, // ColorDialogWhite
	nullptr, // ColorDialogRed
	"fonts\\yellow.trn",

	nullptr,
	"fonts\\black.trn",

	"fonts\\white.trn",
	"fonts\\whitegold.trn",
	"fonts\\red.trn",
	"fonts\\blue.trn",
	// OR-1 as a file of its own (2026-09-07, the rule: every colour has its own .trn, only the raw
	// gold has none). This is vanilla orange.trn with its band moved onto PAL16_ORANGE, which LoadFont
	// used to do in memory after the fork's green ramp overwrote the orange minis the file pointed at.
	nullptr, // oracool_orange1: a value in RgbDefinedColors since stage 4 (v1.11.010)

	"fonts\\buttonface.trn",
	"fonts\\buttonpushed.trn",
	// The in-game dialog three are Orcl files since 2026-09-07 (user rule: "colors to match their trn
	// file names"): vanilla's gamedialog*.trn are identity on the glyph band, so all three drew the raw
	// gold. These say white, yellow and red - the in-play white, yellow and red bands.
	nullptr, // oracool_dialogwhite: a value in RgbDefinedColors since stage 4 (v1.11.010)
	nullptr, // oracool_dialogyellow: a value in RgbDefinedColors since stage 4 (v1.11.010)
	nullptr, // oracool_dialogred: a value in RgbDefinedColors since stage 4 (v1.11.010)

	// Oracool: generated, not authored - see tools/MakeYellowFontTrn.ps1.
	// The focus-glow pair, renamed 2026-09-07 to say what they are: MENU-palette files (the ramp at
	// 128-135 is yellow only there). Their in-play twins are at the end of the table.
	nullptr, // oracool_menuyellow: a value in RgbDefinedColors since stage 4 (v1.11.010)
	nullptr, // oracool_menuyellowdark: a value in RgbDefinedColors since stage 4 (v1.11.010)

	// Oracool: GREEN text (user, 2026-08-15 - "Set Green items as in belzebub"). Deliberately the
	// same file as ColorYellow: yellow.trn has always pointed its glyphs at indices 144-151, and
	// LoadPalette now injects the green ramp exactly there - so the file that used to mean yellow
	// IS the green, unhealed. ColorYellow itself gets healed onto the PAL16_YELLOW ramp at load
	// (see the remap below), which is what keeps rare items yellow.
	// GN-1 as a file of its own (2026-09-07, the same rule): yellow.trn with its band moved onto the
	// injected green minis, which LoadFont used to do in memory.
	nullptr, // oracool_green1: a value in RgbDefinedColors since stage 4 (v1.11.010)
	// Oracool: GR-5 (2026-09-07). Built from the legend's recipe by the script that added it.
	nullptr, // oracool_gray5: a value in RgbDefinedColors since stage 4 (v1.11.010)
	nullptr, // oracool_beige2: a value in RgbDefinedColors since stage 4 (v1.11.010)
	nullptr, // oracool_yellow3: a value in RgbDefinedColors since stage 4 (v1.11.010)
	nullptr, // oracool_brightred3: a value in RgbDefinedColors since stage 4 (v1.11.010)
	nullptr, // oracool_brightblue3: a value in RgbDefinedColors since stage 4 (v1.11.010)
	nullptr, // oracool_gold6: a value in RgbDefinedColors since stage 4 (v1.11.010)
	nullptr, // oracool_orange7: a value in RgbDefinedColors since stage 4 (v1.11.010)
	nullptr, // oracool_gray7: a value in RgbDefinedColors since stage 4 (v1.11.010)
	// The four front-end colours as they read in a LEVEL palette - see text_color's note.
	nullptr, // oracool_uigold: a value in RgbDefinedColors since stage 4 (v1.11.010)
	nullptr, // oracool_uigolddark: a value in RgbDefinedColors since stage 4 (v1.11.010)
	nullptr, // oracool_uisilver: a value in RgbDefinedColors since stage 4 (v1.11.010)
	nullptr, // oracool_uisilverdark: a value in RgbDefinedColors since stage 4 (v1.11.010)
	nullptr, // oracool_uiyellow: a value in RgbDefinedColors since stage 4 (v1.11.010)
	nullptr, // oracool_uiyellowdark: a value in RgbDefinedColors since stage 4 (v1.11.010)
};

std::array<std::optional<std::array<uint8_t, 256>>, 36> ColorTranslationsData;

constexpr int GlyphBandFirst = 192;
constexpr int GlyphBandSize = 16;

/**
 * Renderer stage 3 (v1.11): text colours as VALUES.
 *
 * On the 32-bit screen a glyph is drawn through a 256-entry table of colour values instead of a
 * .trn and the palette. For a colour that has a file, the table is what that file gave through the
 * loaded palette - baked once per palette and identical to the old draw, pixel for pixel. For a
 * colour defined by DefineTextColorRgb, the table carries the value itself, shaded across the
 * glyph band (192-207, the gold ramp the fonts were painted on) the way that ramp shades, so a
 * new colour needs no file and no palette entry. The bake keys on PaletteRgbGeneration, so a
 * palette load or a gamma change rebuilds it.
 */
struct RgbBake {
	std::array<uint32_t, 256> table {};
	uint32_t generation = 0;
};
std::array<RgbBake, ColorTranslations.size()> ColorRgbBakes;
/** 0 = the colour is its file; else bit 31 set and the value in the low 24 bits. */
std::array<uint32_t, ColorTranslations.size()> ColorRgbValues {};

/** One palette index outside the band that a colour also recolours (index 0 = unused slot). */
struct IndexColor {
	uint8_t index;
	uint32_t rgb;
};

/**
 * The colours defined by VALUE: the 16 colours of the glyph band (192-207, brightest first), as
 * values. Stage 4 (v1.11.010): every .trn this fork had made became one of these lines, each
 * entry the exact colour that file produced through its palette (the level palette; the menu
 * palette for the two menu colours), so nothing in play moved and the files are gone. A NEW
 * colour is one line here with no file: sixteen values; or DefineTextColorRgb(colour, 0xRRGGBB)
 * at runtime, which shades one value across the band the way the font's own ramp does.
 */
struct RgbDefinedColor {
	text_color color;
	std::array<uint32_t, GlyphBandSize> band;
	/** The few out-of-band remaps some files carried (the yellow minis for the green, one red for a gray); most have none. */
	std::array<IndexColor, 16> extra {};
};
constexpr RgbDefinedColor RgbDefinedColors[] = {
	{ ColorInGameUiGold,
	    { 0xFFE3A4, 0xEED18C, 0xDDC47E, 0xCCB775, 0xBCA86C, 0xAB9A63, 0x988B5D, 0x877E54, 0x786F49, 0x69603F, 0x5B5134, 0x484027, 0x39311D, 0x312816, 0x1A1408, 0x140B00 } }, // was fonts\\oracool_uigold.trn
	{ ColorInGameUiGoldDark,
	    { 0xDDC47E, 0xCCB775, 0xDDC47E, 0xBCA86C, 0xAB9A63, 0x988B5D, 0x877E54, 0x786F49, 0x5B5134, 0x484027, 0x39311D, 0x312816, 0x1A1408, 0x1A1408, 0x1A1408, 0x1A1408 } }, // was fonts\\oracool_uigolddark.trn
	{ ColorInGameUiSilver,
	    { 0xF3F3F3, 0xDEDEDE, 0xCCCCCC, 0xB8B8B8, 0xA3A3A3, 0x949494, 0x858585, 0x737373, 0x666666, 0x595959, 0x4C4C4C, 0x3D3D3D, 0x2E2E2E, 0x1E1E1E, 0x111111, 0x111111 } }, // was fonts\\oracool_uisilver.trn
	{ ColorInGameUiSilverDark,
	    { 0xCCCCCC, 0xB8B8B8, 0xA3A3A3, 0x949494, 0x858585, 0x737373, 0x666666, 0x4C4C4C, 0x3D3D3D, 0x2E2E2E, 0x1E1E1E, 0x111111, 0x111111, 0x111111, 0x111111, 0x111111 } }, // was fonts\\oracool_uisilverdark.trn
	{ ColorInGameUiYellow,
	    { 0xFFFD9F, 0xFFFD9F, 0xFFFC57, 0xFFFC57, 0xFEFB24, 0xFEFB24, 0xF0EC00, 0xF0EC00, 0xC3C300, 0xC3C300, 0x868600, 0x868600, 0x575500, 0x575500, 0x191900, 0x191900 } }, // was fonts\\oracool_uiyellow.trn
	{ ColorInGameUiYellowDark,
	    { 0xFEFB24, 0xFEFB24, 0xF0EC00, 0xF0EC00, 0xC3C300, 0xC3C300, 0x868600, 0x868600, 0x575500, 0x575500, 0x191900, 0x191900, 0x191900, 0x191900, 0x191900, 0x191900 } }, // was fonts\\oracool_uiyellowdark.trn
	{ ColorUiYellow,
	    { 0xFFFD9F, 0xFFFD9F, 0xFFFC57, 0xFFFC57, 0xFEFB24, 0xFEFB24, 0xF0EC00, 0xF0EC00, 0xC3C300, 0xC3C300, 0x868600, 0x868600, 0x575500, 0x575500, 0x191900, 0x191900 } }, // was fonts\\oracool_menuyellow.trn (menu palette)
	{ ColorUiYellowDark,
	    { 0xFEFB24, 0xFEFB24, 0xF0EC00, 0xF0EC00, 0xC3C300, 0xC3C300, 0x868600, 0x868600, 0x575500, 0x575500, 0x191900, 0x191900, 0x191900, 0x191900, 0x191900, 0x191900 } }, // was fonts\\oracool_menuyellowdark.trn (menu palette)
	{ ColorInGameDialogWhite,
	    { 0xCCCCCC, 0xCCCCCC, 0xCCCCCC, 0xCCCCCC, 0xCCCCCC, 0xCCCCCC, 0xA3A3A3, 0xA3A3A3, 0xB8B8B8, 0xA3A3A3, 0x858585, 0x595959, 0x3D3D3D, 0x2E2E2E, 0x2E2E2E, 0x111111 } }, // was fonts\\oracool_dialogwhite.trn
	{ ColorInGameDialogYellow,
	    { 0xFFFD9F, 0xFFFD9F, 0xFFFC57, 0xFFFC57, 0xFEFB24, 0xFEFB24, 0xF0EC00, 0xF0EC00, 0xC3C300, 0xC3C300, 0x868600, 0x868600, 0x575500, 0x575500, 0x191900, 0x191900 },
	    { { { 224, 0x3D3D3D } } } }, // was fonts\\oracool_dialogyellow.trn
	{ ColorInGameDialogRed,
	    { 0xE06C6C, 0xE06C6C, 0xE06C6C, 0xE06C6C, 0xE06C6C, 0xE06C6C, 0xD85B5B, 0xCF4949, 0xC73838, 0xBF2727, 0xA92222, 0x7C1919, 0x661515, 0x4F1111, 0x390D0D, 0x230909 } }, // was fonts\\oracool_dialogred.trn
	{ ColorOracoolGreen,
	    { 0x8CBE8C, 0x8CBE8C, 0x64A064, 0x64A064, 0x3E823E, 0x3E823E, 0x226E22, 0x226E22, 0x185A18, 0x185A18, 0x104610, 0x104610, 0x0A320A, 0x0A320A, 0x041C04, 0x041C04 },
	    { { { 144, 0x8CBE8C }, { 145, 0x64A064 }, { 146, 0x3E823E }, { 147, 0x226E22 }, { 148, 0x185A18 }, { 149, 0x104610 }, { 150, 0x0A320A }, { 151, 0x041C04 }, { 224, 0x3D3D3D } } } }, // was fonts\\oracool_green1.trn
	{ ColorOrange,
	    { 0xFFE2B3, 0xFFE2B3, 0xF4C996, 0xF4C996, 0xE7B37E, 0xE7B37E, 0xDC9F70, 0xDC9F70, 0xD08C62, 0xD08C62, 0xC77B52, 0xC77B52, 0xCC6133, 0xCC6133, 0xC74B1F, 0xC74B1F },
	    { { { 152, 0xFFE2B3 }, { 153, 0xF4C996 }, { 154, 0xE7B37E }, { 155, 0xDC9F70 }, { 156, 0xD08C62 }, { 157, 0xC77B52 }, { 158, 0xCC6133 }, { 159, 0xC74B1F } } } }, // was fonts\\oracool_orange1.trn
	{ ColorGray5,
	    { 0xCCCCCC, 0xB8B8B8, 0xA3A3A3, 0x949494, 0x858585, 0x737373, 0x666666, 0x595959, 0x4C4C4C, 0x3D3D3D, 0x2E2E2E, 0x1E1E1E, 0x111111, 0x111111, 0x111111, 0x111111 } }, // was fonts\\oracool_gray5.trn
	{ ColorBeige2,
	    { 0xE8CACA, 0xE8CACA, 0xD7B2B2, 0xCA9E9E, 0xBD8F8F, 0xB38080, 0xA87171, 0xA55A5A, 0x9C4949, 0x8B4141, 0x793939, 0x683131, 0x562929, 0x442121, 0x331919, 0x1B0E0E } }, // was fonts\\oracool_beige2.trn
	{ ColorYellow3,
	    { 0xFEFB24, 0xFEFB24, 0xF0EC00, 0xF0EC00, 0xC3C300, 0xC3C300, 0x868600, 0x868600, 0x575500, 0x575500, 0x191900, 0x191900, 0x191900, 0x191900, 0x191900, 0x191900 } }, // was fonts\\oracool_yellow3.trn
	{ ColorBrightRed3,
	    { 0xFE2424, 0xFE2424, 0xF00000, 0xF00000, 0xBD0000, 0xBD0000, 0x910000, 0x910000, 0x5A0000, 0x5A0000, 0x230000, 0x230000, 0x230000, 0x230000, 0x230000, 0x230000 } }, // was fonts\\oracool_brightred3.trn
	{ ColorBrightBlue3,
	    { 0x2424FE, 0x2424FE, 0x0101EF, 0x0101EF, 0x0000BD, 0x0000BD, 0x00008A, 0x00008A, 0x000057, 0x000057, 0x000019, 0x000019, 0x000019, 0x000019, 0x000019, 0x000019 } }, // was fonts\\oracool_brightblue3.trn
	{ ColorGold6,
	    { 0xCCB775, 0xBCA86C, 0xAB9A63, 0x988B5D, 0x877E54, 0x786F49, 0x69603F, 0x5B5134, 0x484027, 0x39311D, 0x312816, 0x1A1408, 0x140B00, 0x140B00, 0x140B00, 0x140B00 } }, // was fonts\\oracool_gold6.trn
	{ ColorOrange7,
	    { 0xD08C62, 0xC77B52, 0xCC6133, 0xC74B1F, 0xB1431B, 0x9B3B18, 0x853213, 0x6F2910, 0x5A220C, 0x3F1708, 0x250E03, 0x0F0500, 0x0F0500, 0x0F0500, 0x0F0500, 0x0F0500 } }, // was fonts\\oracool_orange7.trn
	{ ColorGray7,
	    { 0xA3A3A3, 0x949494, 0x858585, 0x737373, 0x666666, 0x595959, 0x4C4C4C, 0x3D3D3D, 0x2E2E2E, 0x1E1E1E, 0x111111, 0x111111, 0x111111, 0x111111, 0x111111, 0x111111 } }, // was fonts\\oracool_gray7.trn
};
bool RgbDefaultsApplied = false;
/** The definition applied from RgbDefinedColors; a hex from DefineTextColorRgb wins over it while set. */
std::array<std::optional<RgbDefinedColor>, ColorTranslations.size()> ColorRgbBands;

uint32_t PackRgb(const SDL_Color &c)
{
	return (static_cast<uint32_t>(c.r) << 16) | (static_cast<uint32_t>(c.g) << 8) | c.b;
}

int Luminance(const SDL_Color &c)
{
	return (299 * c.r + 587 * c.g + 114 * c.b) / 1000;
}

void BakeRgbTable(text_color color, RgbBake &bake)
{
	const std::array<SDL_Color, 256> &pal = logical_palette; // gamma applied, fades not: a fade is a present-time transform
	for (int i = 0; i < 256; i++)
		bake.table[i] = PackRgb(pal[i]);
	if (ColorRgbValues[color] == 0 && ColorRgbBands[color]) {
		for (int j = 0; j < GlyphBandSize; j++)
			bake.table[GlyphBandFirst + j] = ColorRgbBands[color]->band[j];
		for (const IndexColor &e : ColorRgbBands[color]->extra) {
			if (e.index != 0)
				bake.table[e.index] = e.rgb;
		}
		return;
	}
	if (ColorRgbValues[color] != 0) {
		const uint32_t base = ColorRgbValues[color];
		const int r = (base >> 16) & 0xFF, g = (base >> 8) & 0xFF, b = base & 0xFF;
		const int top = std::max(1, Luminance(pal[GlyphBandFirst]));
		for (int j = 0; j < GlyphBandSize; j++) {
			const int lum = std::min(top, Luminance(pal[GlyphBandFirst + j]));
			bake.table[GlyphBandFirst + j] = (static_cast<uint32_t>(r * lum / top) << 16) | (static_cast<uint32_t>(g * lum / top) << 8) | static_cast<uint32_t>(b * lum / top);
		}
		return;
	}
	if (ColorTranslationsData[color]) {
		const std::array<uint8_t, 256> &trn = *ColorTranslationsData[color];
		for (int i = 0; i < 256; i++)
			bake.table[i] = PackRgb(pal[trn[i]]);
	}
}

} // namespace

void DefineTextColorRgb(text_color color, uint32_t rgb)
{
	if (color >= ColorRgbValues.size())
		return;
	ColorRgbValues[color] = 0x80000000u | (rgb & 0x00FFFFFFu);
	ColorRgbBakes[color].generation = 0;
}

void ClearTextColorRgb(text_color color)
{
	if (color >= ColorRgbValues.size())
		return;
	ColorRgbValues[color] = 0;
	ColorRgbBakes[color].generation = 0;
}

const uint32_t *TextColorRgbTable(text_color color)
{
	if (color >= ColorRgbValues.size())
		return nullptr;
	if (!RgbDefaultsApplied) {
		RgbDefaultsApplied = true;
		for (const RgbDefinedColor &d : RgbDefinedColors) {
			if (d.color < ColorRgbBands.size())
				ColorRgbBands[d.color] = d;
		}
	}
	if (ColorRgbValues[color] == 0 && !ColorRgbBands[color] && !ColorTranslationsData[color])
		return nullptr;
	RgbBake &bake = ColorRgbBakes[color];
	if (bake.generation != PaletteRgbGeneration) {
		BakeRgbTable(color, bake);
		bake.generation = PaletteRgbGeneration;
	}
	return bake.table.data();
}

namespace {

text_color GetColorFromFlags(UiFlags flags)
{
	// One field read since 2026-09-07 (it was a 28-way chain of bit tests with a precedence order).
	// The index is the one behind the UiFlags::Color name - see UiFlagsColorShift - and 0, "no
	// colour", is Whitegold, the fallback every unrecognised flag has always drawn in.
	switch (UiFlagsColorIndex(flags)) {
	case UiFlagsColorIndex(UiFlags::ColorWhite):
		return ColorWhite;
	case UiFlagsColorIndex(UiFlags::ColorBlue):
		return ColorBlue;
	case UiFlagsColorIndex(UiFlags::ColorOrange):
		return ColorOrange;
	case UiFlagsColorIndex(UiFlags::ColorRed):
		return ColorRed;
	case UiFlagsColorIndex(UiFlags::ColorBlack):
		return ColorBlack;
	case UiFlagsColorIndex(UiFlags::ColorGold):
		return ColorGold;
	// The menu files on the menu palette, their in-play counterparts on a level's (2026-09-07).
	case UiFlagsColorIndex(UiFlags::ColorUiGold):
		return gbRunGame ? ColorInGameUiGold : ColorUiGold;
	case UiFlagsColorIndex(UiFlags::ColorUiSilver):
		return gbRunGame ? ColorInGameUiSilver : ColorUiSilver;
	case UiFlagsColorIndex(UiFlags::ColorUiGoldDark):
		return gbRunGame ? ColorInGameUiGoldDark : ColorUiGoldDark;
	case UiFlagsColorIndex(UiFlags::ColorUiSilverDark):
		return gbRunGame ? ColorInGameUiSilverDark : ColorUiSilverDark;
	case UiFlagsColorIndex(UiFlags::ColorDialogWhite):
		return gbRunGame ? ColorInGameDialogWhite : ColorDialogWhite;
	case UiFlagsColorIndex(UiFlags::ColorDialogYellow):
		return ColorInGameDialogYellow;
	case UiFlagsColorIndex(UiFlags::ColorDialogRed):
		return ColorInGameDialogRed;
	case UiFlagsColorIndex(UiFlags::ColorYellow):
		return ColorYellow;
	case UiFlagsColorIndex(UiFlags::ColorButtonface):
		return ColorButtonface;
	case UiFlagsColorIndex(UiFlags::ColorButtonpushed):
		return ColorButtonpushed;
	case UiFlagsColorIndex(UiFlags::ColorUiYellow):
		return gbRunGame ? ColorInGameUiYellow : ColorUiYellow;
	case UiFlagsColorIndex(UiFlags::ColorUiYellowDark):
		return gbRunGame ? ColorInGameUiYellowDark : ColorUiYellowDark;
	case UiFlagsColorIndex(UiFlags::ColorOracoolGreen):
		return ColorOracoolGreen;
	case UiFlagsColorIndex(UiFlags::ColorGray5):
		return ColorGray5;
	case UiFlagsColorIndex(UiFlags::ColorBeige2):
		return ColorBeige2;
	case UiFlagsColorIndex(UiFlags::ColorYellow3):
		return ColorYellow3;
	case UiFlagsColorIndex(UiFlags::ColorBrightRed3):
		return ColorBrightRed3;
	case UiFlagsColorIndex(UiFlags::ColorBrightBlue3):
		return ColorBrightBlue3;
	case UiFlagsColorIndex(UiFlags::ColorGold6):
		return ColorGold6;
	case UiFlagsColorIndex(UiFlags::ColorOrange7):
		return ColorOrange7;
	case UiFlagsColorIndex(UiFlags::ColorGray7):
		return ColorGray7;
	case UiFlagsColorIndex(UiFlags::ColorWhitegold):
	default:
		return ColorWhitegold;
	}
}

uint16_t GetUnicodeRow(char32_t codePoint)
{
	return static_cast<uint32_t>(codePoint) >> 8;
}

bool IsCJK(uint16_t row)
{
	return row >= 0x30 && row <= 0x9f;
}

bool IsHangul(uint16_t row)
{
	return row >= 0xac && row <= 0xd7;
}

bool IsSmallFontTallRow(uint16_t row)
{
	return IsCJK(row) || IsHangul(row);
}

void GetFontPath(GameFontTables size, uint16_t row, string_view ext, char *out)
{
	*BufCopy(out, "fonts\\", FontSizes[size], "-", AsHexPad2(row), ext) = '\0';
}

void GetFontPath(string_view language_code, GameFontTables size, uint16_t row, string_view ext, char *out)
{
	*BufCopy(out, "fonts\\", language_code, "\\", FontSizes[size], "-", AsHexPad2(row), ext) = '\0';
}

uint32_t GetFontId(GameFontTables size, uint16_t row)
{
	return (size << 16) | row;
}

FontStack LoadFont(GameFontTables size, text_color color, uint16_t row)
{
	if (ColorTranslations[color] != nullptr && !ColorTranslationsData[color]) {
		ColorTranslationsData[color].emplace();
		LoadFileInMem(ColorTranslations[color], *ColorTranslationsData[color]);
		// No edits after loading, since 2026-09-07. Two colours used to be made here in memory -
		// the set green was yellow.trn shifted onto the injected green minis (2026-08-15), and orange
		// was vanilla orange.trn re-pointed from those same minis onto PAL16_ORANGE after the green
		// ramp took them (2026-08-16, "Primals are now Green"). Both are files of their own now,
		// oracool_green1.trn and oracool_orange1.trn, by the rule that every colour has its own
		// .trn and only the raw gold (ColorGold, an empty slot) has none. What a file says is what
		// is drawn.
	}

	const uint32_t fontId = GetFontId(size, row);
	auto hotFont = Fonts.find(fontId);
	if (hotFont != Fonts.end()) {
		return FontStack(hotFont->second);
	}

	OwnedFontStack &font = Fonts[fontId];
	char path[32];

	// Load language-specific glyphs:
	const string_view languageCode = GetLanguageCode();
	const string_view lang = languageCode.substr(0, 2);
	if (lang == "zh" || lang == "ja" || lang == "ko"
	    || (lang == "tr" && row == 0)) {
		GetFontPath(languageCode, size, row, ".clx", &path[0]);
		font.overrideFont = LoadOptionalClx(path);
	}

	// Load the base glyphs:
	GetFontPath(size, row, ".clx", &path[0]);
	font.baseFont = LoadOptionalClx(path);

#ifndef UNPACKED_MPQS
	if (!font.baseFont.has_value()) {
		// Could be an old devilutionx.mpq or fonts.mpq with PCX instead of CLX.
		//
		// We'll show an error elsewhere (in `CheckArchivesUpToDate`) and we need to load
		// the font files to display it.
		char pcxPath[32];
		GetFontPath(size, row, "", &pcxPath[0]);
		font.baseFont = LoadPcxSpriteList(pcxPath, /*numFramesOrFrameHeight=*/256, /*transparentColor=*/1);
	}
#endif

	if (!font.baseFont.has_value()) {
		LogError("Error loading font: {}", path);
	}

	return FontStack(font);
}

class CurrentFont {
public:
	FontStack fontStack;

	[[nodiscard]] ClxSprite glyph(size_t i) const
	{
		return fontStack.glyph(i);
	}

	bool load(GameFontTables size, text_color color, char32_t next)
	{
		const uint32_t unicodeRow = GetUnicodeRow(next);
		if (unicodeRow == currentUnicodeRow_ && hasAttemptedLoad_) {
			return true;
		}

		fontStack = LoadFont(size, color, unicodeRow);
		hasAttemptedLoad_ = true;
		currentUnicodeRow_ = unicodeRow;

		return fontStack.has_value();
	}

	void clear()
	{
		hasAttemptedLoad_ = false;
	}

private:
	bool hasAttemptedLoad_ = false;
	uint32_t currentUnicodeRow_ = 0;
};

/**
 * @brief Maps every palette index to 0, so a glyph rendered through it comes out solid black.
 *
 * A CLX sprite's transparency lives in its run-length structure, not in a key colour, so this
 * produces a silhouette of the glyph's exact shape rather than a black box. See UiFlags::Shadowed.
 */
const std::array<uint8_t, 256> &BlackTrn()
{
	static const std::array<uint8_t, 256> trn {}; // value-initialised: every entry 0
	return trn;
}

/**
 * @brief Where the drop shadow falls, relative to the glyph: two left, two down.
 *
 * NOT a guess and not a taste call - it is vanilla's own offset. DevilutionX 1.5.5, which this fork
 * is built from, drew every character-sheet label twice:
 *
 *     DrawString(out, text, { labelPosition + Displacement { -2, 2 }, ... }, style | ColorBlack);
 *     DrawString(out, text, { labelPosition,                          ... }, style | ColorWhite);
 *
 * Down-LEFT, by two. My first pass used +1,+1 - down-right by one - which is the conventional
 * direction for a drop shadow and the wrong one for this game; the light in Diablo's panel art
 * comes from the lower right, so the shadow has to fall the other way or the text stops agreeing
 * with the stone it sits on.
 */
constexpr Displacement TextShadowOffset { -2, 2 };

/**
 * @brief Which half of a shadowed string a walk is drawing.
 *
 * User report with screenshot (2026-09-03): "it seem as if the shadows of certain letters are
 * overlaping the left adjacent white letters. white text should always be on top of shadow text."
 *
 * Exactly right, and the cause is the ORDER, not the offset. A shadowed string used to be drawn
 * glyph by glyph, each character laying its own shadow and then its own face. The shadow falls two
 * pixels LEFT, so it lands in the character BEFORE it - which by then has already been drawn. Every
 * letter therefore had its right side smeared by its neighbour's shadow, and the wider the letter to
 * its right, the worse it looked.
 *
 * Per-character was fine while the shadow fell down-RIGHT, into ground no glyph had covered yet. It
 * stopped being fine when the offset was corrected to vanilla's down-left on 2026-08-29, and nobody
 * noticed for five days because it only shows on close inspection of dense rows.
 *
 * So the string is walked twice: every shadow first, then every face over the top. Two passes rather
 * than a right-to-left walk, which would also work and would be far harder to prove correct with
 * wrapping, alignment and kerning in the mix.
 */
enum class TextPass : uint8_t {
	/** The black silhouettes only. */
	Shadow,
	/** Outline and face only - the shadow pass has already run. */
	Glyph,
	/** One walk that draws both, for text with no shadow to sequence. */
	Both,
};

void DrawFont(const Surface &out, Point position, ClxSprite glyph, text_color color, bool outline, bool shadow,
    TextPass pass = TextPass::Both)
{
	// BEFORE the outline and the glyph, so both cover it where they overlap - a shadow that drew
	// last would sit on top of the letter it belongs under. Within one character that ordering was
	// always right; see TextPass for why it was not enough across a whole string.
	if (shadow && pass != TextPass::Glyph) {
		RenderClxSpriteWithTRN(out, glyph, position + TextShadowOffset, BlackTrn().data());
	}
	if (pass == TextPass::Shadow)
		return;
	if (outline) {
		ClxDrawOutlineSkipColorZero(out, 0, { position.x, position.y + glyph.height() - 1 }, glyph);
	}
	// Stage 3 (v1.11): on the screen, through the colour's table of values; the .trn only serves
	// an indexed surface now.
	if (!out.isIndexed()) {
		if (const uint32_t *rgb = TextColorRgbTable(color); rgb != nullptr) {
			RenderClxSpriteWithRgbMap(out, glyph, position, rgb);
			return;
		}
	}
	if (ColorTranslationsData[color]) {
		RenderClxSpriteWithTRN(out, glyph, position, ColorTranslationsData[color]->data());
	} else {
		RenderClxSprite(out, glyph, position);
	}
}

bool IsFullWidthPunct(char32_t c)
{
	return IsAnyOf(c, U'，', U'、', U'。', U'？', U'！');
}

bool IsBreakAllowed(char32_t codepoint, char32_t nextCodepoint)
{
	return IsFullWidthPunct(codepoint) && !IsFullWidthPunct(nextCodepoint);
}

std::size_t CountNewlines(string_view fmt, const DrawStringFormatArg *args, std::size_t argsLen)
{
	std::size_t result = c_count(fmt, '\n');
	for (std::size_t i = 0; i < argsLen; ++i) {
		if (std::holds_alternative<string_view>(args[i].value()))
			result += c_count(args[i].GetFormatted(), '\n');
	}
	return result;
}

class FmtArgParser {
public:
	FmtArgParser(string_view fmt,
	    DrawStringFormatArg *args,
	    size_t len,
	    size_t offset = 0)
	    : fmt_(fmt)
	    , args_(args)
	    , len_(len)
	    , next_(offset)
	{
	}

	std::optional<std::size_t> operator()(string_view &rest)
	{
		std::optional<std::size_t> result;
		if (rest[0] != '{')
			return result;

		const std::size_t closingBracePos = rest.find('}', 1);
		if (closingBracePos == string_view::npos) {
			LogError("Unclosed format argument: {}", fmt_);
			return result;
		}

		std::size_t fmtLen;
		bool positional;
		if (closingBracePos == 2 && rest[1] >= '0' && rest[1] <= '9') {
			result = rest[1] - '0';
			fmtLen = 3;
			positional = true;
		} else {
			result = next_++;
			fmtLen = closingBracePos + 1;
			positional = false;
		}
		if (!result) {
			LogError("Unsupported format argument: {}", rest);
		} else if (*result >= len_) {
			LogError("Not enough format arguments, {} given for: {}", len_, fmt_);
			result = std::nullopt;
		} else {
			if (!args_[*result].HasFormatted()) {
				const auto fmtStr = positional ? "{}" : string_view(rest.data(), fmtLen);
				args_[*result].SetFormatted(fmt::format(fmt::runtime(fmtStr), std::get<int>(args_[*result].value())));
			}
			rest.remove_prefix(fmtLen);
		}
		return result;
	}

	size_t offset() const
	{
		return next_;
	}

private:
	string_view fmt_;
	DrawStringFormatArg *args_;
	std::size_t len_;
	std::size_t next_;
};

bool ContainsSmallFontTallCodepoints(string_view text)
{
	while (!text.empty()) {
		const char32_t next = ConsumeFirstUtf8CodePoint(&text);
		if (next == Utf8DecodeError)
			break;
		if (next == ZWSP)
			continue;
		if (IsSmallFontTallRow(GetUnicodeRow(next)))
			return true;
	}
	return false;
}

int GetLineHeight(string_view fmt, DrawStringFormatArg *args, std::size_t argsLen, GameFontTables fontIndex)
{
	constexpr std::array<int, 6> LineHeights = { 12, 26, 38, 42, 50, 22 };
	if (fontIndex == GameFont12 && IsSmallFontTall()) {
		FmtArgParser fmtArgParser { fmt, args, argsLen };
		string_view rest = fmt;
		while (!rest.empty()) {
			const std::optional<std::size_t> fmtArgPos = fmtArgParser(rest);
			if (fmtArgPos) {
				if (ContainsSmallFontTallCodepoints(args[*fmtArgPos].GetFormatted())) {
					return SmallFontTallLineHeight;
				}
				continue;
			}
			const char32_t cp = ConsumeFirstUtf8CodePoint(&rest);
			if (cp == Utf8DecodeError) break;
			if (cp == ZWSP) continue;
			if (IsSmallFontTallRow(GetUnicodeRow(cp))) return SmallFontTallLineHeight;
		}
	}
	return LineHeights[fontIndex];
}

Surface ClipSurface(const Surface &out, Rectangle rect)
{
	if (rect.size.height == 0) {
		return out.subregion(0, 0, std::min(rect.position.x + rect.size.width, out.w()), out.h());
	}
	return out.subregion(0, 0,
	    std::min(rect.position.x + rect.size.width, out.w()),
	    std::min(rect.position.y + rect.size.height, out.h()));
}

int AdjustSpacingToFitHorizontally(int &lineWidth, int maxSpacing, int charactersInLine, int availableWidth)
{
	if (lineWidth <= availableWidth || charactersInLine < 2)
		return maxSpacing;

	const int overhang = lineWidth - availableWidth;
	const int spacingRedux = (overhang + charactersInLine - 2) / (charactersInLine - 1);
	lineWidth -= spacingRedux * (charactersInLine - 1);
	return maxSpacing - spacingRedux;
}

void MaybeWrap(Point &characterPosition, int characterWidth, int rightMargin, int initialX, int lineHeight)
{
	if (characterPosition.x + characterWidth > rightMargin) {
		characterPosition.x = initialX;
		characterPosition.y += lineHeight;
	}
}

int GetLineStartX(UiFlags flags, const Rectangle &rect, int lineWidth)
{
	if (HasAnyOf(flags, UiFlags::AlignCenter)) {
		return std::max(rect.position.x, rect.position.x + (rect.size.width - lineWidth) / 2);
	}
	if (HasAnyOf(flags, UiFlags::AlignRight))
		return rect.position.x + rect.size.width - lineWidth;
	return rect.position.x;
}

uint32_t DoDrawString(const Surface &out, string_view text, Rectangle rect, Point &characterPosition,
    int lineWidth, int charactersInLine, int rightMargin, int bottomMargin, GameFontTables size, text_color color, bool outline, bool shadow,
    TextRenderOptions &opts, TextPass pass = TextPass::Both)
{
	CurrentFont currentFont;
	int curSpacing = opts.spacing;
	if (HasAnyOf(opts.flags, UiFlags::KerningFitSpacing)) {
		curSpacing = AdjustSpacingToFitHorizontally(lineWidth, opts.spacing, charactersInLine, rect.size.width);
		if (curSpacing != opts.spacing && HasAnyOf(opts.flags, UiFlags::AlignCenter | UiFlags::AlignRight)) {
			const int adjustedLineWidth = GetLineWidth(text, size, curSpacing, &charactersInLine);
			characterPosition.x = GetLineStartX(opts.flags, rect, adjustedLineWidth);
		}
	}

	char32_t next;
	string_view remaining = text;
	size_t cpLen;

	const auto maybeDrawCursor = [&]() {
		if (opts.cursorPosition == static_cast<int>(text.size() - remaining.size())) {
			Point position = characterPosition;
			MaybeWrap(position, 2, rightMargin, position.x, opts.lineHeight);
			if (GetAnimationFrame(2, 500) != 0) {
				FontStack baseFont = LoadFont(size, color, 0);
				if (baseFont.has_value()) {
					DrawFont(out, position, baseFont.glyph('|'), color, outline, shadow, pass);
				}
			}
			if (opts.renderedCursorPositionOut != nullptr) {
				*opts.renderedCursorPositionOut = position;
			}
		}
	};

	for (; !remaining.empty() && remaining[0] != '\0'
	     && (next = DecodeFirstUtf8CodePoint(remaining, &cpLen)) != Utf8DecodeError;
	     remaining.remove_prefix(cpLen)) {
		if (next == ZWSP)
			continue;

		if (!currentFont.load(size, color, next)) {
			next = U'?';
			if (!currentFont.load(size, color, next)) {
				app_fatal("Missing fonts");
			}
		}

		const uint8_t frame = next & 0xFF;
		const uint16_t width = currentFont.glyph(frame).width();
		if (next == U'\n' || characterPosition.x + width > rightMargin) {
			if (next == '\n')
				maybeDrawCursor();
			const int nextLineY = characterPosition.y + opts.lineHeight;
			if (nextLineY >= bottomMargin)
				break;
			characterPosition.y = nextLineY;

			if (HasAnyOf(opts.flags, UiFlags::KerningFitSpacing)) {
				int nextLineWidth = GetLineWidth(remaining.substr(cpLen), size, opts.spacing, &charactersInLine);
				curSpacing = AdjustSpacingToFitHorizontally(nextLineWidth, opts.spacing, charactersInLine, rect.size.width);
			}

			if (HasAnyOf(opts.flags, UiFlags::AlignCenter | UiFlags::AlignRight)) {
				lineWidth = width;
				if (remaining.size() > cpLen)
					lineWidth += curSpacing + GetLineWidth(remaining.substr(cpLen), size, curSpacing);
			}
			characterPosition.x = GetLineStartX(opts.flags, rect, lineWidth);

			if (next == U'\n')
				continue;
		}

		const ClxSprite glyph = currentFont.glyph(frame);
		const auto byteIndex = static_cast<int>(text.size() - remaining.size());

		// Draw highlight. Skipped on the shadow pass, and it MUST be: the highlight is an opaque
		// fill, so a second one drawn between the shadows and the faces would bury the shadows it is
		// meant to sit behind.
		if (pass != TextPass::Shadow
		    && byteIndex >= opts.highlightRange.begin && byteIndex < opts.highlightRange.end) {
			const bool lastInRange = static_cast<int>(byteIndex + cpLen) == opts.highlightRange.end;
			FillRect(out, characterPosition.x, characterPosition.y,
			    glyph.width() + (lastInRange ? 0 : curSpacing), glyph.height(),
			    opts.highlightColor);
		}

		DrawFont(out, characterPosition, glyph, color, outline, shadow, pass);
		maybeDrawCursor();
		characterPosition.x += width + curSpacing;
	}
	maybeDrawCursor();
	return static_cast<uint32_t>(remaining.data() - text.data());
}

} // namespace

void LoadSmallSelectionSpinner()
{
	pSPentSpn2Cels = LoadCel("data\\pentspn2", 12);
}

void UnloadFonts()
{
	Fonts.clear();
}

int GetLineWidth(string_view text, GameFontTables size, int spacing, int *charactersInLine)
{
	int lineWidth = 0;
	CurrentFont currentFont;
	uint32_t codepoints = 0;
	char32_t next;
	while (!text.empty()) {
		next = ConsumeFirstUtf8CodePoint(&text);
		if (next == Utf8DecodeError)
			break;
		if (next == ZWSP)
			continue;

		if (next == U'\n')
			break;

		if (!currentFont.load(size, text_color::ColorDialogWhite, next)) {
			next = U'?';
			if (!currentFont.load(size, text_color::ColorDialogWhite, next)) {
				app_fatal("Missing fonts");
			}
		}

		const uint8_t frame = next & 0xFF;
		lineWidth += currentFont.glyph(frame).width() + spacing;
		++codepoints;
	}
	if (charactersInLine != nullptr)
		*charactersInLine = codepoints;

	return lineWidth != 0 ? (lineWidth - spacing) : 0;
}

bool IsConsumed(string_view s) { return s.empty() || s[0] == '\0'; };

int GetLineWidth(string_view fmt, DrawStringFormatArg *args, std::size_t argsLen, size_t argsOffset, GameFontTables size, int spacing, int *charactersInLine,
    std::optional<size_t> firstArgOffset)
{
	int lineWidth = 0;
	CurrentFont currentFont;

	uint32_t codepoints = 0;
	char32_t prev = U'\0';
	char32_t next;
	string_view remaining = fmt;
	FmtArgParser fmtArgParser { fmt, args, argsLen, argsOffset };
	size_t cpLen;

	// The current formatted argument value being processed.
	string_view curFormatted;

	// The string that we're currently processing: either `remaining` or `curFormatted`.
	string_view *str;

	if (firstArgOffset.has_value()) {
		curFormatted = args[argsOffset - 1].GetFormatted().substr(*firstArgOffset);
	}

	for (; !(IsConsumed(curFormatted) && IsConsumed(remaining));
	     str->remove_prefix(cpLen), prev = next) {
		const bool isProcessingFormatArgValue = !IsConsumed(curFormatted);
		str = isProcessingFormatArgValue ? &curFormatted : &remaining;
		next = DecodeFirstUtf8CodePoint(*str, &cpLen);
		if (next == Utf8DecodeError) break;

		// {{ and }} escapes in fmt.
		if (!isProcessingFormatArgValue && (prev == U'{' || prev == U'}') && prev == next) continue;
		// ZWSP are line-breaking opportunities that can otherwise be skipped for rendering as they have 0-width.
		if (next == ZWSP) continue;
		if (next == U'\n') break;

		if (!isProcessingFormatArgValue) {
			const std::optional<std::size_t> fmtArgPos = fmtArgParser(*str);
			if (fmtArgPos.has_value()) {
				// `fmtArgParser` has already consumed `*str`. Ensure the loop doesn't consume any more.
				cpLen = 0;
				// The loop assigns `prev = next`.
				// We reset it to U'\0' to ensure that {{ and }} escapes are not processed accross
				// the boundary of the format string and a formatted value.
				next = U'\0';
				currentFont.clear();
				const DrawStringFormatArg &arg = args[*fmtArgPos];
				curFormatted = arg.GetFormatted();
				continue;
			}
		}

		if (!currentFont.load(size, text_color::ColorDialogWhite, next)) {
			next = U'?';
			if (!currentFont.load(size, text_color::ColorDialogWhite, next)) {
				app_fatal("Missing fonts");
			}
		}

		const uint8_t frame = next & 0xFF;
		lineWidth += currentFont.glyph(frame).width() + spacing;
		++codepoints;
	}
	if (charactersInLine != nullptr)
		*charactersInLine = codepoints;

	return lineWidth != 0 ? (lineWidth - spacing) : 0;
}

int GetLineHeight(string_view text, GameFontTables fontIndex)
{
	if (fontIndex == GameFont12 && IsSmallFontTall() && ContainsSmallFontTallCodepoints(text)) {
		return SmallFontTallLineHeight;
	}
	return LineHeights[fontIndex];
}

std::string WordWrapString(string_view text, unsigned width, GameFontTables size, int spacing)
{
	std::string output;
	if (text.empty() || text[0] == '\0')
		return output;

	output.reserve(text.size());
	const char *begin = text.data();
	const char *processedEnd = text.data();
	string_view::size_type lastBreakablePos = string_view::npos;
	std::size_t lastBreakableLen = 0;
	unsigned lineWidth = 0;
	CurrentFont currentFont;

	char32_t codepoint = U'\0'; // the current codepoint
	char32_t nextCodepoint;     // the next codepoint
	std::size_t nextCodepointLen;
	string_view remaining = text;
	nextCodepoint = DecodeFirstUtf8CodePoint(remaining, &nextCodepointLen);
	do {
		codepoint = nextCodepoint;
		const std::size_t codepointLen = nextCodepointLen;
		if (codepoint == Utf8DecodeError)
			break;
		remaining.remove_prefix(codepointLen);
		nextCodepoint = !remaining.empty() ? DecodeFirstUtf8CodePoint(remaining, &nextCodepointLen) : U'\0';

		if (codepoint == U'\n') { // Existing line break, scan next line
			lastBreakablePos = string_view::npos;
			lineWidth = 0;
			output.append(processedEnd, remaining.data());
			processedEnd = remaining.data();
			continue;
		}

		if (codepoint != ZWSP) {
			const uint8_t frame = codepoint & 0xFF;
			if (!currentFont.load(size, text_color::ColorDialogWhite, codepoint)) {
				codepoint = U'?';
				if (!currentFont.load(size, text_color::ColorDialogWhite, codepoint)) {
					app_fatal("Missing fonts");
				}
			}

			lineWidth += currentFont.glyph(frame).width() + spacing;
		}

		if (IsBreakableWhitespace(codepoint)) {
			lastBreakablePos = remaining.data() - begin - codepointLen;
			lastBreakableLen = codepointLen;
			continue;
		}

		if (lineWidth - spacing <= width) {
			if (IsBreakAllowed(codepoint, nextCodepoint)) {
				lastBreakablePos = remaining.data() - begin;
				lastBreakableLen = 0;
			}

			continue; // String is still within the limit, continue to the next symbol
		}

		if (lastBreakablePos == string_view::npos) { // Single word longer than width
			lastBreakablePos = remaining.data() - begin - codepointLen;
			lastBreakableLen = 0;
		}

		// Break line and continue to next line
		const char *end = &text[lastBreakablePos];
		output.append(processedEnd, end);
		output += '\n';

		// Restart from the beginning of the new line.
		remaining = text.substr(lastBreakablePos + lastBreakableLen);
		processedEnd = remaining.data();
		lastBreakablePos = string_view::npos;
		lineWidth = 0;
		nextCodepoint = !remaining.empty() ? DecodeFirstUtf8CodePoint(remaining, &nextCodepointLen) : U'\0';
	} while (!remaining.empty() && remaining[0] != '\0');
	output.append(processedEnd, remaining.data());
	return output;
}

/**
 * @todo replace Rectangle with cropped Surface
 */
uint32_t DrawString(const Surface &out, string_view text, const Rectangle &rect, TextRenderOptions opts)
{
	const GameFontTables size = GetFontSizeFromUiFlags(opts.flags);
	const text_color color = GetColorFromFlags(opts.flags);

	int charactersInLine = 0;
	int lineWidth = 0;
	if (HasAnyOf(opts.flags, (UiFlags::AlignCenter | UiFlags::AlignRight | UiFlags::KerningFitSpacing)))
		lineWidth = GetLineWidth(text, size, opts.spacing, &charactersInLine);

	Point characterPosition { GetLineStartX(opts.flags, rect, lineWidth), rect.position.y };
	const int initialX = characterPosition.x;

	const int rightMargin = rect.position.x + rect.size.width;
	const int bottomMargin = rect.size.height != 0 ? std::min(rect.position.y + rect.size.height + BaseLineOffset[size], out.h()) : out.h();

	if (opts.lineHeight == -1)
		opts.lineHeight = GetLineHeight(text, size);

	if (HasAnyOf(opts.flags, UiFlags::VerticalCenter)) {
		const int textHeight = static_cast<int>((c_count(text, '\n') + 1) * opts.lineHeight);
		characterPosition.y += std::max(0, (rect.size.height - textHeight) / 2);
	}

	characterPosition.y += BaseLineOffset[size];

	const bool outlined = HasAnyOf(opts.flags, UiFlags::Outlined);
	const bool shadowed = HasAnyOf(opts.flags, UiFlags::Shadowed);

	const Surface clippedOut = ClipSurface(out, rect);

	// Only draw the PentaCursor if the cursor is not at the end.
	if (HasAnyOf(opts.flags, UiFlags::PentaCursor) && static_cast<size_t>(opts.cursorPosition) == text.size()) {
		opts.cursorPosition = -1;
	}

	// SHADOWS FIRST, as a whole string, then the faces over them - see TextPass. The two walks are
	// the same walk: DoDrawString derives every position from its arguments and the font metrics, so
	// given the same inputs it lays the second pass exactly over the first. Only `characterPosition`
	// is carried across calls by reference, so the shadow pass gets a copy and the real pass starts
	// where it would have started anyway.
	//
	// Unshadowed text still takes ONE walk, which is nearly all text in the game.
	if (shadowed) {
		Point shadowPosition = characterPosition;
		DoDrawString(clippedOut, text, rect, shadowPosition,
		    lineWidth, charactersInLine, rightMargin, bottomMargin, size, color, outlined, shadowed, opts,
		    TextPass::Shadow);
	}
	const uint32_t bytesDrawn = DoDrawString(clippedOut, text, rect, characterPosition,
	    lineWidth, charactersInLine, rightMargin, bottomMargin, size, color, outlined, shadowed, opts,
	    shadowed ? TextPass::Glyph : TextPass::Both);

	if (HasAnyOf(opts.flags, UiFlags::PentaCursor)) {
		const ClxSprite sprite = (*pSPentSpn2Cels)[PentSpn2Spin()];
		MaybeWrap(characterPosition, sprite.width(), rightMargin, initialX, opts.lineHeight);
		ClxDraw(clippedOut, characterPosition + Displacement { 0, opts.lineHeight - BaseLineOffset[size] }, sprite);
	}

	return bytesDrawn;
}

void DrawStringWithColors(const Surface &out, string_view fmt, DrawStringFormatArg *args, std::size_t argsLen, const Rectangle &rect, TextRenderOptions opts)
{
	const GameFontTables size = GetFontSizeFromUiFlags(opts.flags);
	const text_color color = GetColorFromFlags(opts.flags);

	int charactersInLine = 0;
	int lineWidth = 0;
	if (HasAnyOf(opts.flags, (UiFlags::AlignCenter | UiFlags::AlignRight | UiFlags::KerningFitSpacing)))
		lineWidth = GetLineWidth(fmt, args, argsLen, 0, size, opts.spacing, &charactersInLine);

	Point characterPosition { GetLineStartX(opts.flags, rect, lineWidth), rect.position.y };
	const int initialX = characterPosition.x;

	const int rightMargin = rect.position.x + rect.size.width;
	const int bottomMargin = rect.size.height != 0 ? std::min(rect.position.y + rect.size.height + BaseLineOffset[size], out.h()) : out.h();

	if (opts.lineHeight == -1)
		opts.lineHeight = GetLineHeight(fmt, args, argsLen, size);

	if (HasAnyOf(opts.flags, UiFlags::VerticalCenter)) {
		const int textHeight = static_cast<int>((CountNewlines(fmt, args, argsLen) + 1) * opts.lineHeight);
		characterPosition.y += std::max(0, (rect.size.height - textHeight) / 2);
	}

	characterPosition.y += BaseLineOffset[size];

	const bool outlined = HasAnyOf(opts.flags, UiFlags::Outlined);
	// STILL PER-CHARACTER here, unlike DrawString - see TextPass. This walk interleaves format
	// arguments and switches colour mid-string, so it cannot simply be run twice without splitting
	// that state out first, and no caller passes Shadowed to it today (only diabloui's list and the
	// chat log use this entry point). A shadowed string drawn through here would show the smearing
	// the 2026-09-03 report describes; whoever needs one should lift the walk into DoDrawString's
	// two-pass shape rather than add a second special case.
	const bool shadowed = HasAnyOf(opts.flags, UiFlags::Shadowed);

	const Surface clippedOut = ClipSurface(out, rect);

	CurrentFont currentFont;
	const int originalSpacing = opts.spacing;
	if (HasAnyOf(opts.flags, UiFlags::KerningFitSpacing)) {
		opts.spacing = AdjustSpacingToFitHorizontally(lineWidth, originalSpacing, charactersInLine, rect.size.width);
		if (opts.spacing != originalSpacing && HasAnyOf(opts.flags, UiFlags::AlignCenter | UiFlags::AlignRight)) {
			const int adjustedLineWidth = GetLineWidth(fmt, args, argsLen, 0, size, opts.spacing, &charactersInLine);
			characterPosition.x = GetLineStartX(opts.flags, rect, adjustedLineWidth);
		}
	}

	char32_t prev = U'\0';
	char32_t next;
	string_view remaining = fmt;
	FmtArgParser fmtArgParser { fmt, args, argsLen };
	size_t cpLen;

	// The current formatted argument value being processed.
	string_view curFormatted;
	text_color curFormattedColor;

	// The string that we're currently processing: either `remaining` or `curFormatted`.
	string_view *str;

	for (; !(IsConsumed(curFormatted) && IsConsumed(remaining));
	     str->remove_prefix(cpLen), prev = next) {
		const bool isProcessingFormatArgValue = !IsConsumed(curFormatted);
		str = isProcessingFormatArgValue ? &curFormatted : &remaining;
		next = DecodeFirstUtf8CodePoint(*str, &cpLen);
		if (next == Utf8DecodeError) break;

		// {{ and }} escapes in fmt.
		if (!isProcessingFormatArgValue && (prev == U'{' || prev == U'}') && prev == next) continue;
		// ZWSP are line-breaking opportunities that can otherwise be skipped for rendering as they have 0-width.
		if (next == ZWSP) continue;

		if (!isProcessingFormatArgValue) {
			const std::optional<std::size_t> fmtArgPos = fmtArgParser(*str);
			if (fmtArgPos.has_value()) {
				// `fmtArgParser` has already consumed `*str`. Ensure the loop doesn't consume any more.
				cpLen = 0;
				// The loop assigns `prev = next`.
				// We reset it to U'\0' to ensure that {{ and }} escapes are not processed accross
				// the boundary of the format string and a formatted value.
				next = U'\0';
				currentFont.clear();
				const DrawStringFormatArg &arg = args[*fmtArgPos];
				curFormatted = arg.GetFormatted();
				curFormattedColor = GetColorFromFlags(arg.GetFlags());
				continue;
			}
		}

		const text_color curColor = isProcessingFormatArgValue ? curFormattedColor : color;
		if (!currentFont.load(size, curColor, next)) {
			next = U'?';
			if (!currentFont.load(size, curColor, next)) {
				app_fatal("Missing fonts");
			}
		}

		const uint8_t frame = next & 0xFF;
		const uint16_t width = currentFont.glyph(frame).width();
		if (next == U'\n' || characterPosition.x + width > rightMargin) {
			const int nextLineY = characterPosition.y + opts.lineHeight;
			if (nextLineY >= bottomMargin)
				break;
			characterPosition.y = nextLineY;

			if (HasAnyOf(opts.flags, UiFlags::KerningFitSpacing)) {
				int nextLineWidth = isProcessingFormatArgValue
				    ? GetLineWidth(remaining, args, argsLen, fmtArgParser.offset(), size, originalSpacing, &charactersInLine,
				          /*firstArgOffset=*/args[fmtArgParser.offset() - 1].GetFormatted().size() - (curFormatted.size() - cpLen))
				    : GetLineWidth(remaining.substr(cpLen), args, argsLen, fmtArgParser.offset(), size, originalSpacing, &charactersInLine);
				opts.spacing = AdjustSpacingToFitHorizontally(nextLineWidth, originalSpacing, charactersInLine, rect.size.width);
			}

			if (HasAnyOf(opts.flags, UiFlags::AlignCenter | UiFlags::AlignRight)) {
				lineWidth = width;
				if (str->size() > cpLen) {
					lineWidth += opts.spacing
					    + (isProcessingFormatArgValue
					            ? GetLineWidth(remaining, args, argsLen, fmtArgParser.offset(), size, opts.spacing, &charactersInLine,
					                  /*firstArgOffset=*/args[fmtArgParser.offset() - 1].GetFormatted().size() - (curFormatted.size() - cpLen))
					            : GetLineWidth(remaining.substr(cpLen), args, argsLen, fmtArgParser.offset(), size, opts.spacing, &charactersInLine));
				}
			}
			characterPosition.x = GetLineStartX(opts.flags, rect, lineWidth);

			if (next == U'\n')
				continue;
		}

		DrawFont(clippedOut, characterPosition, currentFont.glyph(frame), curColor, outlined, shadowed);
		characterPosition.x += width + opts.spacing;
	}

	if (HasAnyOf(opts.flags, UiFlags::PentaCursor)) {
		const ClxSprite sprite = (*pSPentSpn2Cels)[PentSpn2Spin()];
		MaybeWrap(characterPosition, sprite.width(), rightMargin, initialX, opts.lineHeight);
		ClxDraw(clippedOut, characterPosition + Displacement { 0, opts.lineHeight - BaseLineOffset[size] }, sprite);
	}
}

uint8_t PentSpn2Spin()
{
	return GetAnimationFrame(8, 50);
}

bool IsBreakableWhitespace(char32_t c)
{
	return IsAnyOf(c, U' ', U'　', ZWSP);
}

} // namespace devilution
