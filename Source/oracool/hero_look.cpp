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
 * Measured on the exported Sorcerer sheets, all three armour tiers. THE SHARED PALETTE'S RAMPS COME IN PAIRS: 224-239
 * is ONE red sixteen entries long, not a bright trim beside a dark robe - the robe's lit folds are 229-231 and its
 * body 232-237. The first dye (v1.12.030) treated the halves as two materials and sent the highlights to
 * near-black, which flattened the cloth (user screenshot, the same day). So:
 *
 *  - the robe, 224-239, and the heavy tier's pure reds 136-143: a dark green, each entry at 62% of the brightness
 *    of the red it replaces, so every fold keeps its place;
 *  - the skin, 160-175 (again one ramp of sixteen) and the dark end of the tan, 204-207, where his face and hands
 *    actually sit: ash, BRIGHTER than the brown it replaces - he is pale;
 *  - the boots, sash and staff bindings, 208-223: dried blood instead of orange leather, his one accent.
 *
 * The greys, the blues and the rest of the tan are left alone. One table serves all three tiers because the same
 * ramps mean the same materials on each. Own colours, not an index dye: the palette has no green at all. Each
 * entry falls back to itself, so an indexed target draws the plain Sorcerer.
 */
std::shared_ptr<const SpriteColours> NecromancerColours()
{
	constexpr std::array<uint32_t, 16> Robe = { 0x68877A, 0x587368, 0x4E655C, 0x475C53, 0x40534B, 0x384942, 0x31403A, 0x2A3731,
		0x25302B, 0x202A26, 0x1B2320, 0x161D1A, 0x121715, 0x0D110F, 0x080B0A, 0x040504 };
	constexpr std::array<uint32_t, 8> PureRed = { 0x5D796D, 0x445950, 0x32413B, 0x242E2A, 0x1C2521, 0x161C19, 0x0D1110, 0x050706 };
	constexpr std::array<uint32_t, 16> Ash = { 0xF2F0DA, 0xEFEDD7, 0xEDEBD5, 0xEBE9D3, 0xEAE8D1, 0xE9E7D0, 0xCBC9B5, 0xB0AF9E,
		0x9D9C8C, 0x89887B, 0x767569, 0x626158, 0x4E4E46, 0x3B3B35, 0x20201D, 0x0F0F0E };
	constexpr std::array<uint32_t, 4> DarkTan = { 0x65645B, 0x54534B, 0x2A2A26, 0x1A1917 };
	constexpr std::array<uint32_t, 16> Blood = { 0xD8353A, 0xD8353A, 0xD8353A, 0xD8353A, 0xCD3238, 0xBA2E32, 0xA3282C, 0x8D2326,
		0x7E1F22, 0x6F1B1E, 0x5E171A, 0x4E1315, 0x401011, 0x2C0B0C, 0x1A0607, 0x0A0203 };

	auto colours = std::make_shared<SpriteColours>();
	const auto set = [&](int first, const uint32_t *values, int count) {
		for (int i = 0; i < count; i++)
			colours->Set(static_cast<uint8_t>(first + i), values[i], static_cast<uint8_t>(first + i));
	};
	set(224, Robe.data(), 16);
	set(136, PureRed.data(), 8);
	set(160, Ash.data(), 16);
	set(204, DarkTan.data(), 4);
	set(208, Blood.data(), 16);
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
