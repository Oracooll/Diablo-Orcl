#include "oracool/divine_trn.h"

#include <array>
#include <cstring>

#include "engine/palette.h"

namespace devilution::oracool {

namespace {

/**
 * @brief How far each colour is pushed toward white, in percent.
 *
 * Enough to read as lit from inside rather than merely pale. Below about 40 the shield still looks
 * like loot; much above 70 and the whole sprite flattens into one bright blob and stops being a
 * shield at all.
 */
constexpr int ShinePercent = 55;

/**
 * @brief A cool highlight rather than a neutral one, so the shine reads as holy and not as sunburn.
 *
 * Blue and green are lifted slightly more than red, which is what separates "divine" from "on fire"
 * - the game already owns the warm end of that vocabulary for its fire spells.
 */
constexpr int RedBias = 100;
constexpr int GreenBias = 112;
constexpr int BlueBias = 125;

std::array<uint8_t, 256> DivineTrn;
std::array<SDL_Color, 256> BuiltAgainst;
bool Built = false;

uint8_t Brighten(uint8_t channel, int bias)
{
	const int lifted = channel + (255 - channel) * ShinePercent / 100;
	return static_cast<uint8_t>(std::min(lifted * bias / 100, 255));
}

/** @brief Nearest palette entry to an RGB, searched over the GLOBAL half only. */
uint8_t NearestGlobalIndex(int red, int green, int blue)
{
	// 128..255: the half that is identical across every level type by design (see engine/palette.h),
	// so a table built on one level is still right on the next. The low half is level-specific
	// scenery colour and mapping into it would make the shield change hue on a staircase.
	uint8_t best = 128;
	int bestDistance = INT32_MAX;
	for (int i = 128; i < 256; i++) {
		const SDL_Color &candidate = orig_palette[i];
		const int dr = candidate.r - red;
		const int dg = candidate.g - green;
		const int db = candidate.b - blue;
		const int distance = dr * dr + dg * dg + db * db;
		if (distance < bestDistance) {
			bestDistance = distance;
			best = static_cast<uint8_t>(i);
		}
	}
	return best;
}

bool PaletteChanged()
{
	return !Built || std::memcmp(BuiltAgainst.data(), orig_palette.data(), sizeof(BuiltAgainst)) != 0;
}

} // namespace

const uint8_t *GetDivineTrn()
{
	if (PaletteChanged()) {
		// Index 0 is transparent in a CLX blit and must map to itself, or the sprite gains a solid
		// background the moment it is drawn through a TRN.
		DivineTrn[0] = 0;
		for (int i = 1; i < 256; i++) {
			const SDL_Color &source = orig_palette[i];
			DivineTrn[i] = NearestGlobalIndex(Brighten(source.r, RedBias),
			    Brighten(source.g, GreenBias), Brighten(source.b, BlueBias));
		}
		BuiltAgainst = orig_palette;
		Built = true;
	}
	return DivineTrn.data();
}

} // namespace devilution::oracool
