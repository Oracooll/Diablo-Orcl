#include "oracool/hero_look.h"

#include <array>

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

} // namespace devilution::oracool
