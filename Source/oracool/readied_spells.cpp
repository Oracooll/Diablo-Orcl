#include "oracool/readied_spells.h"

#include "player.h"
#include "spells.h"

namespace devilution::oracool {

namespace {

/**
 * @brief The type a readied spell should have now, or Invalid if the player cannot cast it at all.
 *
 * Re-derived rather than stored. Storing it would cost a second scarce save byte per binding and be
 * the less correct of the two: a stored Scroll or Charges type names an ITEM, and the item is
 * routinely gone by the next session, so a faithfully restored stale type would be a binding that
 * silently does nothing. Deriving means each kind is honored exactly as far as it is still real:
 * Skill and Spell belong to the character and always survive; Charges is honored when the staff is
 * STILL EQUIPPED at load time (the masks this reads are freshly rebuilt, so this is a live fact,
 * not a stale record - added in the external-audit round alongside the bind-time Charges
 * derivation in HandleAbilityFKey). A spell the character has since lost entirely (an unequipped
 * staff, a skill not yet unlocked at this level) resolves to Invalid and is dropped instead of
 * restored as an uncastable binding.
 */
SpellType ReadiedSpellType(const Player &player, SpellID spell)
{
	const uint64_t bit = GetSpellBitmask(spell);
	if ((player._pAblSpells & bit) != 0)
		return SpellType::Skill;
	if ((player._pMemSpells & bit) != 0)
		return SpellType::Spell;
	if ((player._pISpells & bit) != 0)
		return SpellType::Charges;
	return SpellType::Invalid;
}

} // namespace

uint8_t PackReadiedSpell(SpellID spell)
{
	if (!IsValidSpell(spell))
		return 0;
	// +1 so that SpellID::Null (0) and "nothing readied" stay distinguishable. MAX_SPELLS is 59 (it
	// was 53 when this was written; Charge and the six Paladin skills grew it), so the largest value
	// written is 59 and the byte never overflows.
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
