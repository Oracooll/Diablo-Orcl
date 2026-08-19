/**
 * @file oracool/monster_difficulty.h
 *
 * Oracool: Megaplan Phase 3.3 - resistances and immunities that actually answer to the difficulty.
 *
 * Two things were wrong, and both are fixed from data that already exists rather than from new
 * columns nobody has authored.
 *
 * ## Champions were softer than their own rank and file
 *
 * `MonsterData` carries TWO resistance sets, `resistance` and `resistanceHell`, and ordinary
 * monsters switch to the hard one on Hell. `UniqueMonsterData` carries only ONE, `mMagicRes`, and
 * `PrepareUniqueMonst` assigned it flat on every difficulty. So on Hell a rank-and-file Skeleton
 * could be fire-immune while the champion leading it - a hand-authored unique, or one of this
 * fork's lesser uniques borrowing that unique's identity - was merely fire-resistant, or nothing at
 * all. Champions got RELATIVELY softer as the difficulty rose, which is backwards.
 *
 * A champion's set is now the union of its own hand-authored one with what an ordinary monster of
 * its type would carry. Every champion keeps the identity it was written with, and gains the
 * guarantee that it is never weaker than the monsters it leads. That is the invariant a test pins.
 *
 * ## Nightmare was Normal with fatter monsters
 *
 * Vanilla steps hit points, damage and armour on Nightmare but leaves resistances at the Normal
 * set; the whole second column only arrives on Hell. Nightmare now gets a real middle rung: Hell's
 * set with its IMMUNITIES DEMOTED to plain resistances. Nothing becomes unkillable a difficulty
 * early, but the elements Hell will eventually wall off start pushing back, which is what a middle
 * difficulty is for.
 *
 * IMMUNE_ACID has no RESIST_ACID counterpart to demote to, so it simply does not appear early.
 */
#pragma once

#include <cstdint>

#include "monstdat.h"
#include "utils/enum_traits.h"

namespace devilution {
enum _difficulty : uint8_t;
} // namespace devilution

namespace devilution::oracool {

/**
 * @brief @p resistances with every immunity replaced by the matching plain resistance.
 *
 * Shared rather than private because two systems need the same step down: Nightmare's middle rung
 * here, and Conviction breaking an immunity in oracool/aura_field.cpp. IMMUNE_ACID is dropped
 * rather than demoted - the enum has no RESIST_ACID to demote it to.
 */
uint16_t DemoteImmunitiesToResistances(uint16_t resistances);

/**
 * @brief The mirror: every resisted school becomes immune, EXCEPT the last one standing.
 *
 * Torment's step. A monster must never come out immune to all three schools at once - that is not
 * a harder fight but an impossible one for the caster classes, while the physical classes would
 * never notice. Whatever the monster was weakest to on Hell stays merely resisted, so every monster
 * keeps exactly one answer.
 */
uint16_t PromoteResistancesToImmunities(uint16_t resistances);

/** @brief The resistance and immunity bits an ordinary monster of @p data carries on @p difficulty. */
uint16_t MonsterResistancesFor(const MonsterData &data, _difficulty difficulty);

/**
 * @brief The bits a unique - or a lesser unique borrowing its identity - carries on @p difficulty.
 *
 * @param uniqueResistances the champion's own hand-authored set (UniqueMonsterData::mMagicRes).
 * @param baseData the monster type it wears, whose difficulty ladder it is held to.
 */
uint16_t ChampionResistancesFor(uint16_t uniqueResistances, const MonsterData &baseData, _difficulty difficulty);

} // namespace devilution::oracool
