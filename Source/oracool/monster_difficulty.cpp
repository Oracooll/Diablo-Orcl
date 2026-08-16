#include "oracool/monster_difficulty.h"

#include "utils/attributes.h"

namespace devilution::oracool {

namespace {

/**
 * @brief @p resistances with every immunity replaced by the matching plain resistance.
 *
 * The Nightmare rung. IMMUNE_ACID is dropped rather than demoted because the enum has no
 * RESIST_ACID to demote it to - an acid immunity simply waits for Hell.
 */
uint16_t DemoteImmunities(uint16_t resistances)
{
	uint16_t out = resistances & ~static_cast<uint16_t>(IMMUNE_MAGIC | IMMUNE_FIRE | IMMUNE_LIGHTNING | IMMUNE_ACID);
	if ((resistances & IMMUNE_MAGIC) != 0)
		out |= RESIST_MAGIC;
	if ((resistances & IMMUNE_FIRE) != 0)
		out |= RESIST_FIRE;
	if ((resistances & IMMUNE_LIGHTNING) != 0)
		out |= RESIST_LIGHTNING;
	return out;
}

} // namespace

uint16_t MonsterResistancesFor(const MonsterData &data, _difficulty difficulty)
{
	switch (difficulty) {
	case DIFF_NIGHTMARE:
		// Everything Normal already had, plus Hell's extra walls arriving as resistances first.
		// Unioned rather than replaced, so a monster can never LOSE a resistance by the difficulty
		// going up - `resistanceHell` is authored as a replacement set, not a superset.
		return static_cast<uint16_t>(data.resistance) | DemoteImmunities(data.resistanceHell);
	case DIFF_HELL:
	case DIFF_TORMENT:
		return data.resistanceHell;
	default:
		return data.resistance;
	}
}

uint16_t ChampionResistancesFor(uint16_t uniqueResistances, const MonsterData &baseData, _difficulty difficulty)
{
	// The union is the whole point: the champion keeps every bit it was hand-authored with, and
	// gains whatever an ordinary monster of its own type would have on this difficulty. It can
	// therefore never be softer than the rank and file it leads.
	return uniqueResistances | MonsterResistancesFor(baseData, difficulty);
}

} // namespace devilution::oracool
