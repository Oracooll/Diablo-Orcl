/**
 * @file oracool/hero_title.h
 *
 * Oracool: the hero's TITLE, earned by defeating Diablo - the Sanctified Order (user, 2026-09-13:
 * "we need to distinguish heroes who have defeated diablo in different difficulties. D2 does it nice
 * with a certain title after each kill").
 *
 * One rung per difficulty conquered, read from Player::pDiabloKillLevel - already saved with the hero
 * and already carried to the hero select screen as _uiheroinfo::herorank, so the title costs no save
 * format: 0 no kill, 1 Normal, 2 Nightmare, 3 Hell, 4 Torment.
 *
 *   Adventurer   no kill yet
 *   Slayer       Diablo slain on Normal
 *   Champion     on Nightmare
 *   Conqueror    on Hell
 *   Sanctified   on Torment
 *
 * Each rung wears the colour of the item-quality ladder a player already reads: white, blue, rare
 * yellow, unique gold, primal.
 */
#pragma once

#include <cstdint>

#include "DiabloUI/ui_flags.hpp"

namespace devilution::oracool {

/** @brief The untranslated title for a hero whose hardest Diablo kill is @p diabloKillLevel. Values past Torment read as Sanctified. */
const char *HeroTitleFor(uint8_t diabloKillLevel);

/** @brief The colour that title is written in. */
UiFlags HeroTitleColorFor(uint8_t diabloKillLevel);

} // namespace devilution::oracool
