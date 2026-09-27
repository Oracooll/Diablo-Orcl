#include "oracool/hero_look.h"

#include <algorithm>
#include <array>
#include <memory>

#include "oracool/sprite_colours.h"
#include "player.h"

namespace devilution::oracool {

namespace {

/**
 * @brief The ramp dyes (2026-09-27, tools/GenHeroRampDye.js): each material's palette ramp takes a new hue and saturation
 * and keeps its own shading - Infravision's trick, one material at a time. The Barbarian is north steel and moss (cool
 * steel plate, moss-green cloth, grey fur, worn leather), the Necromancer bone and violet (violet robe, bone skin,
 * violet-grey plate). Chosen by the user from rendered comparisons (OracoolPreview.DISABLED_HeroRampDyes).
 *
 * History: the band swap (v1.12.021-184: the light-armour-only Barbarian, one Necromancer table), then RfA-28's hand
 * recolour (v1.12.185-206, tools/GenHeroRecolour.js: one colour per index per tier from ChatGPT's painted frames). The data
 * file keeps the hand recolour's format - one table per hero and tier - so the tiers could differ again one day.
 */
struct HeroRecolourEntry {
	uint8_t own;
	uint32_t rgb;
	uint8_t fallback;
};

#include "oracool/hero_recolour_data.inc"

/** @brief Armour tier from _pgfxnum: 0 light, 1 medium, 2 heavy. */
int TierOf(uint8_t gfxnum)
{
	return std::min(gfxnum >> 4, 2);
}

std::shared_ptr<const SpriteColours> Build(const HeroRecolourEntry (&table)[256])
{
	auto colours = std::make_shared<SpriteColours>();
	for (int i = 0; i < 256; i++) {
		if (table[i].own != 0)
			colours->Set(static_cast<uint8_t>(i), table[i].rgb, table[i].fallback);
	}
	return colours;
}

const HeroRecolourEntry (*TableFor(HeroClass heroClass, uint8_t gfxnum))[256]
{
	static const HeroRecolourEntry (*const barbarian[3])[256] = { &BarbarianLightRecolour, &BarbarianMediumRecolour, &BarbarianHeavyRecolour };
	static const HeroRecolourEntry (*const necromancer[3])[256] = { &NecromancerLightRecolour, &NecromancerMediumRecolour, &NecromancerHeavyRecolour };
	if (heroClass == HeroClass::Barbarian)
		return barbarian[TierOf(gfxnum)];
	if (heroClass == HeroClass::Necromancer)
		return necromancer[TierOf(gfxnum)];
	return nullptr;
}

} // namespace

int SpriteScalePercent(HeroClass heroClass)
{
	// The Barbarian was drawn 20% larger from 2026-09-16 until the user dropped it on 2026-09-26: pixel art scaled by
	// an uneven factor doubles some rows and columns and not others, and it read as grain. Every class at its own size
	// now; the scaler stays for a class that one day wants it.
	(void)heroClass;
	return 100;
}

const uint8_t *HeroDyeTrn(const Player &player)
{
	const std::shared_ptr<const SpriteColours> colours = HeroColours(player);
	return colours != nullptr ? colours->FallbackTrn() : nullptr;
}

std::shared_ptr<const SpriteColours> HeroColours(const Player &player)
{
	return HeroColoursFor(player._pClass, player._pgfxnum);
}

std::shared_ptr<const SpriteColours> HeroColoursFor(HeroClass heroClass, uint8_t gfxnum)
{
	const HeroRecolourEntry(*table)[256] = TableFor(heroClass, gfxnum);
	if (table == nullptr)
		return nullptr;
	// One SpriteColours per table, kept: its lit tables are cached inside it.
	static std::array<std::shared_ptr<const SpriteColours>, 6> built;
	const size_t slot = (heroClass == HeroClass::Necromancer ? 3 : 0) + static_cast<size_t>(TierOf(gfxnum));
	if (built[slot] == nullptr)
		built[slot] = Build(*table);
	return built[slot];
}

uint8_t HeroDyeId(HeroClass heroClass, uint8_t gfxnum)
{
	// A distinct id per hero and tier: 1-3 the Barbarian, 4-6 the Necromancer; 0 no dye (the Warrior, the Sorcerer).
	if (heroClass == HeroClass::Barbarian)
		return static_cast<uint8_t>(1 + TierOf(gfxnum));
	if (heroClass == HeroClass::Necromancer)
		return static_cast<uint8_t>(4 + TierOf(gfxnum));
	return 0;
}

} // namespace devilution::oracool
