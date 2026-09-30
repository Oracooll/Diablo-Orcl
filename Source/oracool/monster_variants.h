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

#include "levels/gendung.h"

namespace devilution {
struct Monster;
enum _difficulty : uint8_t;
enum class MonsterGraphic : uint8_t;
enum class DamageType : uint8_t;
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
	// The eleven of 2026-09-19 (the user's verdicts on the Monster Variants page: all approved, and
	// "meet-able in all zones except cathedral"). APPENDED, never inserted: a seed maps to a roster
	// position, so the order of a roster is what a monster's variant is.
	/** Resists magic - the third resistance, next to Ashen and Stormtouched. */
	Veiled,
	/** Armour class up by half: harder to hit, the opposite lesson to Hollow. */
	Ironhide,
	/** Its special attack hits for half again as much. Only monsters with a special roll it. */
	Brutal,
	/** Attacks faster: one tick fewer per frame of the attack animation. */
	Frenzied,
	/** Walks faster: one tick fewer per frame of the walk animation. */
	Fleet,
	/** A third of its blow is fire, against the player's fire resistance. */
	Searing,
	/** A third of its blow is lightning. */
	Voltaic,
	/** Its blows poison: a slow bleed after the hit, resisted as magic (the fork's poison rule). */
	Venomous,
	/** Cannot be knocked back. */
	Unyielding,
	/** Drops better: its death drop rolls two rungs deeper with the good-item bias on. */
	Gilded,
	/** Carries a light, and so gives itself away. */
	Luminous,
	/**
	 * A third of its blow is cold, against the hero's cold resistance, and it chills; its missiles are cold too
	 * (2026-09-26, the ice variant the user picked when heroes got a real cold resistance). APPENDED.
	 */
	Glacial,
	LAST = Glacial,
};

/** @brief A Luminous monster's light radius - public since RelightLoadedMonsters re-lights one after a revisit's load. */
constexpr int LuminousRadius = 5;

/** @brief Which variant @p monster is, derived from its seed and the floor it stands on. */
MonsterVariant VariantOf(const Monster &monster);

/**
 * @brief The variant a seed draws from @p dungeon's roster. The whole rule, with nothing implicit.
 *
 * Split out of VariantOf so the roster can be tested without building a Monster and a level: this
 * takes the two inputs that decide the answer and reads no globals. VariantOf is the same rule with
 * `leveltype` and the monster's exclusions supplied.
 *
 * The ROSTER is what makes a variant mean something. With one global list a Cathedral skeleton and
 * a Hell knight drew from the same four, so "Ashen" said nothing about where you were - it was
 * texture, not information. Each dungeon type now offers a subset, so learning a floor's roster is
 * learning the floor.
 */
MonsterVariant VariantForSeed(uint32_t seed, dungeon_type dungeon);

/**
 * @brief What percentage of ordinary monsters are variants on @p difficulty.
 *
 * Climbs with the ladder (v1.9.16). A re-run walks the same twenty-four floors, so without this the
 * fourth pass through the Cathedral met exactly as many variants as the first - the monsters were
 * bigger and nothing else about the encounter had changed. Denser special encounters is something a
 * player notices; a bigger health bar on the same fight is not.
 */
int VariantPercentFor(_difficulty difficulty);

/** @brief How many variants @p dungeon's roster offers. Zero means no variants spawn there. */
int VariantRosterSize(dungeon_type dungeon);

/** @brief The @p index'th variant of @p dungeon's roster, or None if out of range. */
MonsterVariant VariantInRoster(dungeon_type dungeon, int index);

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
/**
 * @brief A loaded ordinary monster's palette, rebuilt from its variant (audit, 2026-09-27). The tint is not saved, and a
 * revisit builds - and throws away - a whole monster set before LoadLevel: the slot kept that discarded monster's
 * TRN, so an "Ashen" monster came back in another kind's colour, or a unique's. Called by SyncMonsterAnim.
 */
void RestoreVariantTint(Monster &monster);

// The hooks the 2026-09-19 kinds ask at seams that already exist. Each is one question, answered
// from the derived variant, so nothing is stored and a monster that is not that kind costs one
// comparison.

/** @brief Frenzied / Fleet: frames to skip from @p graphic's animation (0 for everyone else); NewMonsterAnim bounds it. */
int VariantSkippedFrames(const Monster &monster, MonsterGraphic graphic);

/** @brief Searing / Voltaic: the element a third of this monster's blow is dealt as, or Physical. */
DamageType VariantHitElement(const Monster &monster);

/** @brief Venomous: whether a landed blow poisons the player. */
bool VariantPoisonsOnHit(const Monster &monster);

/** @brief Unyielding: whether the player's knockback is refused. */
bool VariantIsKnockbackImmune(const Monster &monster);

/** @brief Gilded: whether the death drop rolls deeper and good. */
bool VariantDropsGilded(const Monster &monster);

} // namespace devilution::oracool
