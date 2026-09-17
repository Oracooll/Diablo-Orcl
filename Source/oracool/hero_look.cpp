#include "oracool/hero_look.h"

#include <algorithm>
#include <array>
#include <memory>

#include "oracool/sprite_colours.h"
#include "player.h"

namespace devilution::oracool {

namespace {

/** @brief The light Warrior's trousers: 184 (lightest) to 191. Everything below is dyed to match it. */
constexpr int TrouserRamp = 184;
constexpr int TrouserRampSize = 8;

/**
 * @brief The light-armour Barbarian (user, 2026-09-16): "die his body shirt and boots matching blue
 * of pants", "die his hair grey", "die his gloves blue as pants". Light armour only - "these changes
 * are all regarding light sprites" - medium and heavy have not been measured and get nothing.
 *
 * Every mapping keeps the shading: a ramp runs light to dark with its index, and each source entry
 * lands on the trouser entry at the same relative depth.
 */
std::array<uint8_t, 256> LightBarbarianDye()
{
	std::array<uint8_t, 256> trn;
	for (int i = 0; i < 256; i++)
		trn[static_cast<size_t>(i)] = static_cast<uint8_t>(i);

	// The mail shirt: the 16-entry greyscale 240-255, two greys to a blue. Metal weapon blades share
	// this ramp, though they sit mostly at 243-244 where the mail never goes (it starts at 246), so a
	// blade keeps its body and takes a blue edge. Left whole rather than cut at 246, because the mail's
	// own highlights are 246-248 and a blue shirt with grey glints looked wrong on paper.
	for (int i = 240; i < 256; i++)
		trn[static_cast<size_t>(i)] = static_cast<uint8_t>(TrouserRamp + (i - 240) / 2);

	// The boots, 216-223, one to one. The belt, its pouch and the shoulder strap are the same leather
	// and go with them; weapon hafts barely touch it (a staff adds about a dozen pixels).
	for (int i = 0; i < TrouserRampSize; i++)
		trn[static_cast<size_t>(216 + i)] = static_cast<uint8_t>(TrouserRamp + i);

	// The gloves, 168-175, one to one.
	for (int i = 0; i < TrouserRampSize; i++)
		trn[static_cast<size_t>(168 + i)] = static_cast<uint8_t>(TrouserRamp + i);

	// The hair: only two tones, 206 and 207, at the dark end of the skin ramp. Grey means visibly
	// grey, so they go to mid greys rather than the near-blacks their brightness would pick - 247
	// (115) and 250 (76), a lit side and a shaded side.
	trn[206] = 247;
	trn[207] = 250;
	return trn;
}

/**
 * @brief The trouser ramp as colour values, 184 (lightest) to 191. The shared half of the palette is the same on
 * every level, and brightness is a present-time transform since v1.11.022, so these are constants rather than
 * something to read back from PaletteRGB at a moment it may not be loaded.
 */
constexpr std::array<uint32_t, 8> TrouserBlues = { 0x4E587D, 0x434C6F, 0x39415F, 0x2F3650, 0x252B41, 0x191E2D, 0x0D111B, 0x05070C };

/** @brief The blue @p step of @p steps down the trouser ramp, blended from the two entries it falls between. */
uint32_t TrouserBlueAt(int step, int steps)
{
	const int scaled = step * 7 * 256 / (steps - 1); // 0 .. 7*256
	const int lower = std::min(scaled >> 8, 6);
	const int t = scaled - (lower << 8);
	const uint32_t a = TrouserBlues[static_cast<size_t>(lower)];
	const uint32_t b = TrouserBlues[static_cast<size_t>(lower) + 1];
	const auto channel = [&](int shift) {
		const int from = static_cast<int>((a >> shift) & 0xFF);
		const int to = static_cast<int>((b >> shift) & 0xFF);
		return static_cast<uint32_t>(from + (to - from) * t / 256) << shift;
	};
	return channel(16) | channel(8) | channel(0);
}

std::shared_ptr<const SpriteColours> LightBarbarianColours()
{
	auto colours = std::make_shared<SpriteColours>();
	const std::array<uint8_t, 256> fallback = LightBarbarianDye();

	// The mail: sixteen greys to sixteen blues, a full ramp's worth of shading where the index dye had to land
	// two greys on each of eight entries.
	for (int i = 240; i < 256; i++)
		colours->Set(static_cast<uint8_t>(i), TrouserBlueAt(i - 240, 16), fallback[static_cast<size_t>(i)]);
	// Boots, belt and gloves are eight entries onto eight: the palette's own blues, exactly the trousers.
	for (int i = 0; i < TrouserRampSize; i++) {
		colours->Set(static_cast<uint8_t>(216 + i), TrouserBlues[static_cast<size_t>(i)], fallback[static_cast<size_t>(216 + i)]);
		colours->Set(static_cast<uint8_t>(168 + i), TrouserBlues[static_cast<size_t>(i)], fallback[static_cast<size_t>(168 + i)]);
	}
	// The hair: a cool silver, lit side and shaded side. The palette's greys are dead neutral; hair is not.
	colours->Set(206, 0x8E949C, fallback[206]);
	colours->Set(207, 0x5C626B, fallback[207]);
	return colours;
}

} // namespace

int SpriteScalePercent(HeroClass heroClass)
{
	return heroClass == HeroClass::Barbarian ? 120 : 100;
}

const uint8_t *HeroDyeTrn(const Player &player)
{
	if (player._pClass != HeroClass::Barbarian)
		return nullptr;
	if ((player._pgfxnum >> 4) != 0) // light armour is index 0 of ArmourChar
		return nullptr;
	static const std::array<uint8_t, 256> table = LightBarbarianDye();
	return table.data();
}

std::shared_ptr<const SpriteColours> HeroColours(const Player &player)
{
	if (HeroDyeTrn(player) == nullptr)
		return nullptr;
	static const std::shared_ptr<const SpriteColours> colours = LightBarbarianColours();
	return colours;
}

} // namespace devilution::oracool
