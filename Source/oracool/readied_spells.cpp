#include "oracool/readied_spells.h"

#include "player.h"
#include "spells.h"

#include "options.h"
#include "oracool/oracool.h"

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
	const SpellMask bit = GetSpellBitmask(spell);
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
	// +1 so that SpellID::Null (0) and "nothing readied" stay distinguishable. Read and written as a
	// plain int since SpellID widened to int16_t (2026-09-13): through int8_t, id 128 would have packed
	// as byte 129 and unpacked as -127. spelldat.h asserts the last id still fits the byte.
	return static_cast<uint8_t>(static_cast<int>(spell) + 1);
}

void UnpackReadiedSpell(const Player &player, uint8_t packed, SpellID &spell, SpellType &type)
{
	if (packed == 0)
		return;
	const auto stored = static_cast<SpellID>(static_cast<int>(packed) - 1);
	if (!IsValidSpell(stored))
		return;
	const SpellType derived = ReadiedSpellType(player, stored);
	if (derived == SpellType::Invalid)
		return;
	spell = stored;
	type = derived;
}

void RememberReadiedSpells(const Player &player)
{
	// Only the local player's choice is worth remembering, and only in single player - the options
	// are one machine's, not one character's, and a remembered slot from someone else's hero would
	// be a setting arriving from outside the game.
	if (&player != MyPlayer || !IsSinglePlayer())
		return;

	const uint8_t left = PackReadiedSpell(player._pLRSpell);
	const uint8_t right = PackReadiedSpell(player._pRSpell);
	if (left == *sgOptions.Oracool.lastReadiedSpellLeft
	    && right == *sgOptions.Oracool.lastReadiedSpellRight)
		return;

	sgOptions.Oracool.lastReadiedSpellLeft.SetValue(left);
	sgOptions.Oracool.lastReadiedSpellRight.SetValue(right);

	// Written to disk HERE, not left for whenever SaveOptions next happens to run (external audit
	// PO-01, 2026-08-30). SaveOptions is called at startup, from the settings screens, and on a
	// fullscreen toggle - never on a skill change and never on exit. So the remembered slot only
	// ever reached the file if the player happened to open Settings before quitting, and the whole
	// point of the feature is the next launch.
	//
	// Cheap because it is guarded above: this writes only when the pair actually changes, which is
	// a handful of times in a session, not once per call.
	SaveOptions();
}

void ApplyRememberedReadiedSpells(Player &player)
{
	if (!IsSinglePlayer())
		return;
	// AFTER the class defaults, deliberately: the Sorcerer's starting Firebolt is set by
	// CreatePlayer and should be what a first-ever character gets. A remembered byte overwrites it
	// only when it decodes to something this character can actually use, which is exactly the
	// condition UnpackReadiedSpell already enforces - it leaves the slot alone otherwise.
	UnpackReadiedSpell(player, static_cast<uint8_t>(*sgOptions.Oracool.lastReadiedSpellLeft),
	    player._pLRSpell, player._pLRSplType);
	UnpackReadiedSpell(player, static_cast<uint8_t>(*sgOptions.Oracool.lastReadiedSpellRight),
	    player._pRSpell, player._pRSplType);
}

} // namespace devilution::oracool
