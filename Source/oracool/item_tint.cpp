/**
 * @file oracool/item_tint.cpp
 *
 * See item_tint.h. The tables are built once from the ramp layout of the shared palette half rather
 * than from any level's .pal, because they only ever move indices between ramps.
 */
#include "oracool/item_tint.h"

#include <algorithm>
#include <array>

namespace devilution::oracool {

namespace {

/** @brief One ramp of the sprite moved onto another. */
struct Tint {
	item_misc_id id;
	uint8_t fromBase;   // first (lightest) index of the ramp the sprite paints on
	uint8_t fromLength; // 8 for the four mini ramps at 128..159, 16 for the rest
	uint8_t toBase;     // first index of the target ramp
	uint8_t toLength;
	int8_t lift;        // steps towards the light end - the flask is mostly its ramp's dark half
};

/**
 * The shared half of the palette is: 128 blue, 136 red, 144 yellow, 152 orange (eight entries each,
 * light to dark); then 160 dusty rose, 176 steel blue, 192 gold, 208 orange, 224 crimson, 240 grey
 * (sixteen each; 255 is pure white and not part of the grey ramp).
 *
 * Oils: the flask's glass is the grey ramp, 243..254. The "greater" oil of a pair takes the more
 * saturated ramp of a related hue, so the pair reads as a pair; Permanence stays grey, lifted to
 * silver.
 *
 * Runes (probed 2026-09-05): Fire is an orange tablet (208..223), Greater Fire dusty rose
 * (160..175), Lightning steel blue (176..191) with gold highlights, Greater Lightning gold
 * (192..207), Stone grey (239..253). Each moves onto the vivid mini ramp nearest its element -
 * red for the greater fire, blue for lightning, yellow for the greater lightning, green for stone,
 * which has no other claimant - and Fire keeps its orange, already the loudest of the five.
 */
constexpr Tint Tints[] = {
	{ IMISC_OILACC, 240, 16, 176, 16, 3 },   // Accuracy: steel blue
	{ IMISC_OILMAST, 240, 16, 128, 8, 3 },   // Mastery: pure blue
	{ IMISC_OILSHARP, 240, 16, 136, 8, 3 },  // Sharpness: red
	{ IMISC_OILDEATH, 240, 16, 224, 16, 3 }, // Death: crimson
	{ IMISC_OILSKILL, 240, 16, 152, 8, 3 },  // Skill: the vivid orange minis (was green until the ramp went back to the fire, 2026-09-10)
	{ IMISC_OILBSMTH, 240, 16, 160, 16, 3 }, // Blacksmith: dusty rose
	{ IMISC_OILFORT, 240, 16, 144, 8, 3 },   // Fortitude: yellow
	{ IMISC_OILPERM, 240, 16, 240, 16, 6 },  // Permanence: silver
	{ IMISC_OILHARD, 240, 16, 192, 16, 3 },  // Hardening: gold
	{ IMISC_OILIMP, 240, 16, 208, 16, 3 },   // Imperviousness: orange
	{ IMISC_GR_RUNEF, 160, 16, 136, 8, 2 },  // Greater Rune of Fire: red
	{ IMISC_RUNEL, 176, 16, 128, 8, 2 },     // Rune of Lightning: blue
	{ IMISC_GR_RUNEL, 192, 16, 144, 8, 2 },  // Greater Rune of Lightning: yellow
	{ IMISC_RUNES, 240, 16, 160, 8, 2 },     // Rune of Stone: dusty rose, light half (was green until 2026-09-10)
};
constexpr size_t TintCount = sizeof(Tints) / sizeof(Tints[0]);

std::array<uint8_t, 256> BuildTable(const Tint &tint)
{
	std::array<uint8_t, 256> table {};
	for (int i = 0; i < 256; i++)
		table[static_cast<size_t>(i)] = static_cast<uint8_t>(i);
	const int fromEnd = std::min(255, tint.fromBase + tint.fromLength); // 255 stays white
	for (int i = tint.fromBase; i < fromEnd; i++) {
		int pos = (i - tint.fromBase) - tint.lift;
		if (tint.toLength < tint.fromLength)
			pos /= 2;
		pos = std::clamp(pos, 0, tint.toLength - 1);
		table[static_cast<size_t>(i)] = static_cast<uint8_t>(tint.toBase + pos);
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

const uint8_t *ItemTRN(item_misc_id miscId)
{
	for (size_t i = 0; i < TintCount; i++) {
		if (Tints[i].id == miscId)
			return Tables()[i].data();
	}
	return nullptr;
}

const uint8_t *ItemTRN(const Item &item)
{
	if (item.isEmpty())
		return nullptr;
	return ItemTRN(item._iMiscId);
}

} // namespace devilution::oracool
