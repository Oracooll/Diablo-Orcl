#include "oracool/sprite_colours.h"

#include <algorithm>

#include "engine/palette.h"
#include "engine/render/clx_render.hpp"
#include "engine/trn.hpp"

namespace devilution::oracool {

namespace {

/** Below this brightness a ratio of two luminances is noise; the lit palette entry is used as it stands. */
constexpr int MinShadeLuminance = 8;

int Luminance(uint32_t rgb)
{
	const int r = (rgb >> 16) & 0xFF;
	const int g = (rgb >> 8) & 0xFF;
	const int b = rgb & 0xFF;
	return (299 * r + 587 * g + 114 * b) / 1000;
}

uint32_t Scale(uint32_t rgb, int numerator, int denominator)
{
	const auto channel = [&](int shift) {
		const int value = static_cast<int>((rgb >> shift) & 0xFF) * numerator / denominator;
		return static_cast<uint32_t>(std::clamp(value, 0, 255)) << shift;
	};
	return channel(16) | channel(8) | channel(0);
}

} // namespace

SpriteColours::SpriteColours()
{
	for (int i = 0; i < 256; i++)
		fallback_[static_cast<size_t>(i)] = static_cast<uint8_t>(i);
}

void SpriteColours::Set(uint8_t index, uint32_t rgb, uint8_t fallbackIndex)
{
	rgb_[index] = rgb & 0xFFFFFF;
	own_[index] = 1;
	fallback_[index] = fallbackIndex;
	builtFor_.fill(0);
}

void SpriteColours::Build(size_t slot, const uint8_t *indexTable) const
{
	std::array<uint32_t, 256> &table = tables_[slot];
	for (size_t i = 0; i < 256; i++) {
		const uint8_t fallback = fallback_[i];
		const uint8_t shaded = indexTable != nullptr ? indexTable[fallback] : fallback;
		if (own_[i] == 0) {
			// Exactly what the index path draws: the palette's entry, through the same table.
			table[i] = PaletteRGB[shaded];
			continue;
		}
		if (indexTable == nullptr) {
			table[i] = rgb_[i];
			continue;
		}
		const int base = Luminance(PaletteRGB[fallback]);
		if (base < MinShadeLuminance) {
			table[i] = PaletteRGB[shaded];
			continue;
		}
		table[i] = Scale(rgb_[i], Luminance(PaletteRGB[shaded]), base);
	}
	builtFor_[slot] = PaletteRgbGeneration;
}

const uint32_t *SpriteColours::Table(int level) const
{
	const auto slot = static_cast<size_t>(std::clamp(level, 0, static_cast<int>(NumLightingLevels) - 1));
	if (builtFor_[slot] != PaletteRgbGeneration || builtFor_[slot] == 0) {
		// Level 0 is "no table", as in ClxDrawLight: table 0's one non-identity entry sends white to
		// black, and the plain draw it stands in for never consults it.
		Build(slot, slot == 0 ? nullptr : LightTables[slot].data());
	}
	return tables_[slot].data();
}

const uint32_t *SpriteColours::InfravisionTable() const
{
	constexpr size_t Slot = NumLightingLevels;
	if (builtFor_[Slot] != PaletteRgbGeneration || builtFor_[Slot] == 0) {
		// Infravision is a recolour, not a darkening: every index goes to the red ramp by way of its
		// fallback, own colours included - a heat image has no blue shirt in it.
		const uint8_t *infravision = GetInfravisionTRN();
		std::array<uint32_t, 256> &table = tables_[Slot];
		for (size_t i = 0; i < 256; i++)
			table[i] = PaletteRGB[infravision[fallback_[i]]];
		builtFor_[Slot] = PaletteRgbGeneration;
	}
	return tables_[Slot].data();
}

void DrawSpriteWithColours(const Surface &out, Point position, ClxSprite sprite, const SpriteColours &colours, int lightLevel)
{
	if (!out.isIndexed()) {
		ClxDrawRgbMap(out, position, sprite,
		    lightLevel == InfravisionLight ? colours.InfravisionTable() : colours.Table(lightLevel));
		return;
	}

	// An indexed target has no colours to take: the fallback indices, through the same table.
	const uint8_t *after = nullptr;
	if (lightLevel == InfravisionLight)
		after = GetInfravisionTRN();
	else if (lightLevel > 0)
		after = LightTables[static_cast<size_t>(std::min(lightLevel, static_cast<int>(NumLightingLevels) - 1))].data();
	std::array<uint8_t, 256> trn;
	for (size_t i = 0; i < 256; i++)
		trn[i] = after != nullptr ? after[colours.Fallback(static_cast<uint8_t>(i))] : colours.Fallback(static_cast<uint8_t>(i));
	ClxDrawTRN(out, position, sprite, trn.data());
}

} // namespace devilution::oracool
