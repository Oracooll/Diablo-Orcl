#include "oracool/monster_difficulty.h"

#include "oracool/lesser_uniques.h"
#include "utils/attributes.h"

namespace devilution::oracool {

uint16_t DemoteImmunitiesToResistances(uint16_t resistances)
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

uint16_t PromoteResistancesToImmunities(uint16_t resistances)
{
	// The mirror of DemoteImmunitiesToResistances, with one rule it cannot do without: a monster
	// must never come out immune to ALL THREE schools. Promoting blindly would hand Torment
	// monsters that no caster can hurt at all - not a harder fight, an impossible one for half the
	// classes - and the physical classes would not notice the difficulty existed.
	//
	// So the promotion is applied in school order and stops before the last one standing. Whatever
	// the monster was weakest to on Hell stays merely resisted on Torment, which is also the more
	// interesting rule: every monster keeps exactly one answer, and finding it is the game.
	constexpr uint16_t ResistBits = RESIST_MAGIC | RESIST_FIRE | RESIST_LIGHTNING;
	constexpr uint16_t ImmuneBits = IMMUNE_MAGIC | IMMUNE_FIRE | IMMUNE_LIGHTNING;

	uint16_t out = resistances;
	const struct {
		uint16_t resist;
		uint16_t immune;
	} schools[] = {
		{ RESIST_MAGIC, IMMUNE_MAGIC },
		{ RESIST_FIRE, IMMUNE_FIRE },
		{ RESIST_LIGHTNING, IMMUNE_LIGHTNING },
	};

	for (const auto &school : schools) {
		if ((out & school.resist) == 0)
			continue;
		// Would this promotion leave nothing but immunities? Count what stays answerable.
		const uint16_t promoted = static_cast<uint16_t>((out & ~school.resist) | school.immune);
		if ((promoted & ImmuneBits) == ImmuneBits && (promoted & ResistBits) == 0)
			break; // the last school standing keeps its resistance
		out = promoted;
	}
	return out;
}

uint16_t MonsterResistancesFor(const MonsterData &data, _difficulty difficulty)
{
	switch (difficulty) {
	case DIFF_NIGHTMARE:
		// Everything Normal already had, plus Hell's extra walls arriving as resistances first.
		// Unioned rather than replaced, so a monster can never LOSE a resistance by the difficulty
		// going up - `resistanceHell` is authored as a replacement set, not a superset.
		return static_cast<uint16_t>(data.resistance) | DemoteImmunitiesToResistances(data.resistanceHell);
	case DIFF_HELL:
		return data.resistanceHell;
	case DIFF_TORMENT:
		// Torment was byte-identical to Hell, so the fourth difficulty asked nothing the third had
		// not already asked (Pipeline: "make it difficulty-aware so Hell and Torment demand real
		// resistance gear"). Hell's resistances harden into immunities here - the same step
		// Nightmare-to-Hell makes, taken once more.
		return PromoteResistancesToImmunities(data.resistanceHell);
	default:
		return data.resistance;
	}
}

bool ChampionAffixAllowedOn(LesserUniqueAffix affix, _difficulty difficulty)
{
	// The difficulty at which each modifier first appears. A rung ADDS - nothing is withdrawn -
	// so this is a "from here on" test rather than a per-difficulty list, which is also what makes
	// it impossible to write a pool that accidentally drops something Normal already had.
	switch (affix) {
	case LesserUniqueAffix::Relentless:
	case LesserUniqueAffix::Fortified:
	case LesserUniqueAffix::Colossal:
		// The three a new character can read and answer: it does not get knocked back, it is
		// armoured, it is large. None of them require an item the player may not own yet.
		return true;
	case LesserUniqueAffix::Warded:
	case LesserUniqueAffix::Thunderous:
		// The two that ask for gear - resistances, and a death that hurts. Nightmare is where a
		// character has a second damage type and something to resist with.
		return difficulty >= DIFF_NIGHTMARE;
	case LesserUniqueAffix::Vampiric:
		// The one that punishes low damage hardest: a champion out-healing a character is a wall
		// rather than a fight. Hell is where the damage exists to break it.
		return difficulty >= DIFF_HELL;
	case LesserUniqueAffix::Dread:
		// The endgame-boss marker, never rolled for a champion on any difficulty. It is not a
		// modifier a champion can have - see oracool/endgame_boss.h.
		return false;
	case LesserUniqueAffix::None:
		break;
	}
	return false;
}

uint16_t ChampionResistancesFor(uint16_t uniqueResistances, const MonsterData &baseData, _difficulty difficulty)
{
	// The union is the whole point: the champion keeps every bit it was hand-authored with, and
	// gains whatever an ordinary monster of its own type would have on this difficulty. It can
	// therefore never be softer than the rank and file it leads.
	return uniqueResistances | MonsterResistancesFor(baseData, difficulty);
}

} // namespace devilution::oracool
