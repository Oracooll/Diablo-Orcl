/**
 * @file oracool/endgame_boss.h
 *
 * Oracool: Phase 5 - endgame bosses. The thing that guards the best treasure class.
 *
 * The treasure classes (v1.9.13) gave every zone something worth farming for. This gives the deep
 * floors something standing in front of it. A boss is not a new monster: it is the champion path
 * with a heavier profile, so it needs no sprite, no animation and no type slot - the row in the
 * backlog said "built entirely from machinery that already exists" and that is held to here,
 * because the moment a boss needs its own art it becomes blocked on somebody drawing it.
 *
 * ## How a boss is marked, for one byte
 *
 * `LesserUniqueAffix::Dread` - a seventh value on an enum that already lives in a saved `uint8_t`
 * (loadsave.cpp, in a byte that was Unused before champions took it). No new field on Monster, no
 * format bump, nothing for an existing character to migrate.
 *
 * A boss wants to be TWO things wrong with a monster rather than one, and one byte holds one affix.
 * So the second trait is DERIVED from `lesserNameSeed` - a value the champion path already rolls
 * and already saves - through SecondaryTraitOf. Same discipline as the monster variants: derived,
 * never stored, and identical every time you walk back onto the floor because the seed is.
 *
 * ## Guaranteed, not rolled, once the endgame starts
 *
 * From area level BossGuaranteedAreaLevel up, every generated floor has exactly one. That is a
 * deliberate choice against making them rarer: a boss you can plan a run around is a destination,
 * and a destination is the entire point of having just built per-zone drop tables. A boss you MIGHT
 * get is a lottery, and the treasure class it guards is unfarmable through it.
 *
 * Below that, a chance on floors past BossIntroAreaLevel, so a player meets the idea before the
 * endgame rather than meeting it for the first time at the point it matters.
 */
#pragma once

#include <cstdint>

namespace devilution {
struct Monster;
enum class LesserUniqueAffix : uint8_t;
} // namespace devilution

namespace devilution::oracool {

/**
 * @brief A boss's SECOND trait, on top of the Dread affix itself.
 *
 * Four, so the same boss type met twice is a different fight. Each is a channel an ordinary monster
 * already has - the constraint that kept the champion affixes honest, and it kills the obvious idea
 * here for the same reason: there can be no speed trait, because monster movement is paced by the
 * animation and animation timing lives on the shared CMonster rather than on the individual.
 */
enum class BossTrait : uint8_t {
	/** Resists everything a little. Never immune, for the champion list's own reason. */
	Warding,
	/** Takes the player's knockback away. */
	Implacable,
	/** Heals from what it deals. */
	Devouring,
	/** More life again, on top of the boss profile. */
	Adamant,
	LAST = Adamant,
};

/** @brief Whether @p monster is an endgame boss. */
bool IsEndgameBoss(const Monster &monster);

/**
 * @brief The second trait @p seed produces. Pure, so it can be tested without a monster.
 *
 * Takes the name seed rather than the monster because that is the whole input - and because a test
 * that can sweep every seed is the only way to know all four traits are reachable.
 */
BossTrait SecondaryTraitOf(uint16_t seed);

/** @brief @p monster's second trait. Meaningless unless IsEndgameBoss. */
BossTrait SecondaryTraitFor(const Monster &monster);

/** @brief The word that goes in front of a boss's name, e.g. "Devouring". Untranslated. */
const char *BossTraitName(BossTrait trait);

/**
 * @brief How many bosses the floor the game is currently on should host: 1 or 0.
 *
 * A count rather than a bool so the caller reads the same way PlaceLesserUniques does, and so a
 * future floor that wants two is a table change rather than a control-flow change.
 */
int BossCountForLevel();

/** @brief Applies @p monster's second trait. Called once, after the Dread affix is set. */
void ApplyBossTrait(Monster &monster);

/**
 * @brief Whether @p monster cannot be knocked back by the player.
 *
 * Audit finding, 2026-08-26, and it was an INVERSION rather than an omission. Both Relentless (the
 * champion affix) and Implacable (the boss trait) set MFLAG_KNOCKBACK, with comments saying the
 * point was to take away the player's ability to buy space. MFLAG_KNOCKBACK does the opposite: it
 * lives in the monster-hits-PLAYER path and means "this monster's blows knock YOU back". So both
 * granted an offensive power nobody designed and neither granted the immunity both described,
 * because the player's knockback goes through M_GetKnockback and never consults that flag at all.
 *
 * A predicate rather than a new flag bit: the boss trait is derived from the monster's seed
 * (SecondaryTraitFor) rather than stored, so there is nothing to set at spawn time, and asking the
 * question where it is asked keeps one source of truth instead of a flag that can drift from it.
 */
bool IsKnockbackImmune(const Monster &monster);

/** @brief A boss's on-hit hook, for the traits that fire during combat rather than being fields. */
void OnBossDealtDamage(Monster &monster, int damage);

/** @brief Health, damage and armour percentages a boss is built at. See the .cpp for the numbers. */
int BossHealthPercent();
int BossDamagePercent();
int BossArmorBonus();
int BossPackSize();

} // namespace devilution::oracool
