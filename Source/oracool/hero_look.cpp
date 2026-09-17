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

/**
 * @brief The Necromancer: the Sorcerer's body in grave clothes (plan decision D2, 2026-09-17).
 *
 * Measured on the exported Sorcerer sheets, all three armour tiers: the robe is the red ramp 232-239 (34% of the
 * light figure, 14% of the heavy, 7% of the medium), its bright trim 224-231 and, on the heavy tier, the pure reds
 * 136-143; the skin - and the staff he is born holding - is 168-175, a steady tenth of the figure in every tier.
 * The greys, the tans and the boots are left alone. One table serves all three tiers because the same ramps mean
 * the same materials on each.
 *
 * Own colours, not an index dye: the palette has no green at all and its greys are dead neutral, and a dark
 * green-grey robe with ashen skin is neither. Each entry falls back to itself, so an indexed target simply draws
 * the Sorcerer.
 */
std::shared_ptr<const SpriteColours> NecromancerColours()
{
	constexpr std::array<uint32_t, 8> Robe = { 0x46524B, 0x3B4640, 0x313A35, 0x272F2B, 0x1E2421, 0x151A18, 0x0D100F, 0x060807 };
	constexpr std::array<uint32_t, 8> Trim = { 0x6B8072, 0x5B6E61, 0x4C5C51, 0x3E4B42, 0x303A33, 0x232B26, 0x171C19, 0x0C0F0D };
	constexpr std::array<uint32_t, 8> Ash = { 0x9A988C, 0x888679, 0x767468, 0x646258, 0x524F47, 0x3F3D37, 0x272622, 0x131311 };

	auto colours = std::make_shared<SpriteColours>();
	for (int i = 0; i < 8; i++) {
		const auto at = static_cast<size_t>(i);
		colours->Set(static_cast<uint8_t>(232 + i), Robe[at], static_cast<uint8_t>(232 + i));
		colours->Set(static_cast<uint8_t>(224 + i), Trim[at], static_cast<uint8_t>(224 + i));
		colours->Set(static_cast<uint8_t>(136 + i), Trim[at], static_cast<uint8_t>(136 + i));
		colours->Set(static_cast<uint8_t>(168 + i), Ash[at], static_cast<uint8_t>(168 + i));
	}
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
	return HeroColoursFor(player._pClass, player._pgfxnum);
}

std::shared_ptr<const SpriteColours> HeroColoursFor(HeroClass heroClass, uint8_t gfxnum)
{
	if (heroClass == HeroClass::Necromancer) {
		static const std::shared_ptr<const SpriteColours> colours = NecromancerColours();
		return colours;
	}
	if (heroClass != HeroClass::Barbarian || (gfxnum >> 4) != 0)
		return nullptr;
	static const std::shared_ptr<const SpriteColours> colours = LightBarbarianColours();
	return colours;
}

uint8_t HeroDyeId(HeroClass heroClass, uint8_t gfxnum)
{
	if (heroClass == HeroClass::Necromancer)
		return 2;
	return HeroColoursFor(heroClass, gfxnum) != nullptr ? 1 : 0;
}

} // namespace devilution::oracool
