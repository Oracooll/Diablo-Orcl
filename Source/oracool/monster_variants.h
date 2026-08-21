/**
 * @file oracool/monster_variants.h
 *
 * Oracool: Phase 3 - recoloured monster variants, the cheapest bestiary multiplier there is.
 *
 * A floor loads at most a handful of monster TYPES, so the same four sprites walk past all evening.
 * A variant is one of those sprites wearing a different palette and a different name, with one trait
 * that makes it a different fight. No new art, no new animation, no new type slot.
 *
 * ## Derived, never stored
 *
 * The variant is a pure function of the monster's own `rndItemSeed` - a value InitMonster already
 * rolls and the game already persists for the drop. So this needs no field on Monster, no delta
 * entry and no save format change, and a monster is the same variant every time you walk back onto
 * the floor because the level's RNG stream reproduces the same seed.
 *
 * That is the whole reason this was cheap. A stored variant would have meant touching the monster
 * delta, which is the part of the save this project has been most careful with.
 *
 * ## Why the recolour is procedural
 *
 * TintLesserUnique already proved the technique: shift each colour a step or two along its OWN
 * 16-shade ramp rather than changing hue, and the creature stays recognisably itself while reading
 * clearly different from its twin. Entries below 128 are the level-specific half of the palette and
 * are left alone - shifting inside them changes colour unpredictably from floor to floor.
 *
 * The difference here is that an ordinary monster has no TRN to shift, so one is built: an identity
 * map first, then the same ramp walk.
 */
#pragma once

#include <cstdint>

namespace devilution {
struct Monster;
} // namespace devilution

namespace devilution::oracool {

/**
 * @brief The variants. Each is a recolour, a name, and exactly one trait.
 *
 * One trait apiece on purpose: a variant has to be readable from across a room, and "this one
 * shrugs off fire" is a thing a player can learn. Two traits would make it a champion, and
 * champions already exist with their own affixes and their own naming.
 */
enum class MonsterVariant : uint8_t {
	None,
	/** Fire-hardened, and paler for it. */
	Ashen,
	/** Lightning-hardened. */
	Stormtouched,
	/** Hardier, but hits softer - a wall rather than a threat. */
	Hollow,
	/** Hits harder, but dies faster - the mirror of Hollow. */
	Feral,
	LAST = Feral,
};

/** @brief Which variant @p monster is, derived from its seed. None for most monsters. */
MonsterVariant VariantOf(const Monster &monster);

/** @brief The word put in front of the monster's name, or nullptr for None. Untranslated. */
const char *VariantNamePrefix(MonsterVariant variant);

/**
 * @brief Rolls @p monster's variant and applies it: its trait, and its recolour.
 *
 * Called from InitMonster, after the difficulty ladder has set the base stats - the trait adjusts
 * what is already there rather than competing with it. Does nothing to uniques or champions, which
 * have identities of their own and would only be muddied by a second one.
 */
void ApplyMonsterVariant(Monster &monster);

} // namespace devilution::oracool
