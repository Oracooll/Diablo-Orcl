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
		// A resistance bit beside its own school's immunity is no answer (a Hell row resisting all three and immune to
		// lightning came out immune to everything), so only the immunities count.
		if ((promoted & ImmuneBits) == ImmuneBits)
			break; // the last school standing keeps its resistance
		out = promoted;
	}
	return out;
}

uint16_t MonsterResistancesFor(const MonsterData &data, _difficulty difficulty)
{
	// COLD (Round 2, 2026-09-03). The monster tables predate the element, so the resistance is
	// derived rather than authored: the undead resist it, every difficulty, which is the game this
	// line comes from and gives Cold Mastery something to master from the first cathedral level. The
	// base tables stay untouched - ORed in on the way out, on every difficulty branch below.
	const uint16_t cold = data.monsterClass == MonsterClass::Undead ? RESIST_COLD : 0;
	switch (difficulty) {
	case DIFF_NIGHTMARE:
		// Everything Normal already had, plus Hell's extra walls arriving as resistances first.
		// Unioned rather than replaced, so a monster can never LOSE a resistance by the difficulty
		// going up - `resistanceHell` is authored as a replacement set, not a superset.
		return cold | static_cast<uint16_t>(data.resistance) | DemoteImmunitiesToResistances(data.resistanceHell);
	case DIFF_HELL:
		return cold | data.resistanceHell;
	case DIFF_TORMENT:
		// Torment was byte-identical to Hell, so the fourth difficulty asked nothing the third had
		// not already asked (Pipeline: "make it difficulty-aware so Hell and Torment demand real
		// resistance gear"). Hell's resistances harden into immunities here - the same step
		// Nightmare-to-Hell makes, taken once more.
	{
		uint16_t torment = PromoteResistancesToImmunities(data.resistanceHell);
		// And no monster immune to all three schools on Torment, not even one authored so on Hell (round 86 audit, the user's
		// call): the Obsidian Lord, Soul Burner, Advocate, Arch Lich and Reaper were walls no caster could touch, the case the
		// promotion above refuses to create and the Sealed Map arena and the rift guardians already demote. Lightning is given
		// back as a resistance - the one school, in the order ChampionResistancesFor gives one back - so every monster keeps
		// one answer, and every other Hell immunity stands.
		constexpr uint16_t AllImmune = IMMUNE_MAGIC | IMMUNE_FIRE | IMMUNE_LIGHTNING;
		if ((torment & AllImmune) == AllImmune)
			torment = static_cast<uint16_t>((torment & ~static_cast<uint16_t>(IMMUNE_LIGHTNING)) | RESIST_LIGHTNING);
		return cold | torment;
	}
	default:
		return cold | data.resistance;
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
	const uint16_t ordinary = MonsterResistancesFor(baseData, difficulty);
	uint16_t merged = uniqueResistances | ordinary;
	// But never immune to all three schools where the champion alone was not (audit, 2026-09-27) - the rule
	// PromoteResistancesToImmunities keeps for every monster. Two sets that each left one school open could close the
	// last one together: Webwidow's clones came out untouchable by any spell on Normal. The immunity that came only from
	// the base type is the one given back, as a resistance - the last such school, in school order.
	constexpr uint16_t AllImmune = IMMUNE_MAGIC | IMMUNE_FIRE | IMMUNE_LIGHTNING;
	if ((merged & AllImmune) == AllImmune && (uniqueResistances & AllImmune) != AllImmune && (ordinary & AllImmune) != AllImmune) {
		constexpr struct {
			uint16_t resist;
			uint16_t immune;
		} Schools[] = { { RESIST_LIGHTNING, IMMUNE_LIGHTNING }, { RESIST_FIRE, IMMUNE_FIRE }, { RESIST_MAGIC, IMMUNE_MAGIC } };
		for (const auto &school : Schools) {
			if ((uniqueResistances & school.immune) == 0) {
				merged = static_cast<uint16_t>((merged & ~school.immune) | school.resist);
				break;
			}
		}
	}
	return merged;
}

} // namespace devilution::oracool
