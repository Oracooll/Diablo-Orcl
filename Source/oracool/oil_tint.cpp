/**
 * @file oracool/oil_tint.cpp
 *
 * See oil_tint.h. The tables are built once from the ramp layout of the shared palette half rather
 * than from any level's .pal, because they only ever move indices between ramps.
 */
#include "oracool/oil_tint.h"

#include <algorithm>
#include <array>

namespace devilution::oracool {

namespace {

/** @brief Where the flask's grey ramp goes. */
struct Tint {
	item_misc_id oil;
	uint8_t rampBase;   // first (lightest) index of the target ramp
	uint8_t rampLength; // 8 for the four mini ramps at 128..159, 16 for the rest
	int8_t lift;        // steps towards the light end, because the flask is mostly the ramp's dark half
};

/**
 * The shared half of the palette is: 128 blue, 136 red, 144 yellow, 152 green (eight entries each,
 * light to dark); then 160 dusty rose, 176 steel blue, 192 gold, 208 orange, 224 crimson, 240 grey
 * (sixteen each). The "greater" oil of a pair takes the more saturated ramp of a related hue, so the
 * pair reads as a pair; Permanence stays on the grey ramp but lifted to silver.
 */
constexpr Tint Tints[] = {
	{ IMISC_OILACC, 176, 16, 3 },   // Accuracy: steel blue
	{ IMISC_OILMAST, 128, 8, 3 },   // Mastery: pure blue
	{ IMISC_OILSHARP, 136, 8, 3 },  // Sharpness: red
	{ IMISC_OILDEATH, 224, 16, 3 }, // Death: crimson
	{ IMISC_OILSKILL, 152, 8, 3 },  // Skill: green
	{ IMISC_OILBSMTH, 160, 16, 3 }, // Blacksmith: dusty rose
	{ IMISC_OILFORT, 144, 8, 3 },   // Fortitude: yellow
	{ IMISC_OILPERM, 240, 16, 6 },  // Permanence: silver
	{ IMISC_OILHARD, 192, 16, 3 },  // Hardening: gold
	{ IMISC_OILIMP, 208, 16, 3 },   // Imperviousness: orange
};
constexpr size_t TintCount = sizeof(Tints) / sizeof(Tints[0]);

constexpr uint8_t GreyRampBase = 240;

std::array<uint8_t, 256> BuildTable(const Tint &tint)
{
	std::array<uint8_t, 256> table {};
	for (int i = 0; i < 256; i++)
		table[static_cast<size_t>(i)] = static_cast<uint8_t>(i);
	// 255 is pure white and not part of the grey ramp proper; it is left where it is.
	for (int i = GreyRampBase; i < 255; i++) {
		int pos = (i - GreyRampBase) - tint.lift;
		if (tint.rampLength == 8)
			pos /= 2;
		pos = std::clamp(pos, 0, tint.rampLength - 1);
		table[static_cast<size_t>(i)] = static_cast<uint8_t>(tint.rampBase + pos);
	}
	return table;
}

const std::array<std::array<uint8_t, 256>, TintCount> &Tables()
{
	static const std::array<std::array<uint8_t, 256>, TintCount> tables = [] {
		std::array<std::array<uint8_t, 256>, TintCount> t {};
		for (size_t i = 0; i < TintCount; i++)
			t[i] = BuildTable(Tints[i]);
		return t;
	}();
	return tables;
}

} // namespace

const uint8_t *OilTRN(item_misc_id miscId)
{
	for (size_t i = 0; i < TintCount; i++) {
		if (Tints[i].oil == miscId)
			return Tables()[i].data();
	}
	return nullptr;
}

const uint8_t *OilTRN(const Item &item)
{
	if (item.isEmpty())
		return nullptr;
	return OilTRN(item._iMiscId);
}

} // namespace devilution::oracool
