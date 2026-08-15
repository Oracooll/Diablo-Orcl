#include "oracool/readied_spells.h"

#include "player.h"
#include "spells.h"

namespace devilution::oracool {

namespace {

/**
 * @brief The type a readied spell should have now, or Invalid if the player cannot cast it at all.
 *
 * Re-derived rather than stored. Storing it would cost a second scarce save byte per binding and be
 * the less correct of the two: a Scroll or Charges binding names an ITEM, and the item is routinely
 * gone by the next session, so a faithfully restored Scroll type would be a binding that silently
 * does nothing. Skill and Spell are the two kinds that belong to the character rather than to their
 * bags, so they are the two that survive a save - and a spell the character has since lost (an
 * unequipped staff, a skill not yet unlocked at this level) resolves to Invalid and is dropped
 * instead of restored as an uncastable binding.
 */
SpellType ReadiedSpellType(const Player &player, SpellID spell)
{
	const uint64_t bit = GetSpellBitmask(spell);
	if ((player._pAblSpells & bit) != 0)
		return SpellType::Skill;
	if ((player._pMemSpells & bit) != 0)
		return SpellType::Spell;
	return SpellType::Invalid;
}

} // namespace

uint8_t PackReadiedSpell(SpellID spell)
{
	if (!IsValidSpell(spell))
		return 0;
	// +1 so that SpellID::Null (0) and "nothing readied" stay distinguishable. MAX_SPELLS is 53, so
	// the largest value written is 53 and the byte never overflows.
	return static_cast<uint8_t>(static_cast<int8_t>(spell) + 1);
}

void UnpackReadiedSpell(const Player &player, uint8_t packed, SpellID &spell, SpellType &type)
{
	if (packed == 0)
		return;
	const auto stored = static_cast<SpellID>(static_cast<int8_t>(packed - 1));
	if (!IsValidSpell(stored))
		return;
	const SpellType derived = ReadiedSpellType(player, stored);
	if (derived == SpellType::Invalid)
		return;
	spell = stored;
	type = derived;
}

} // namespace devilution::oracool
