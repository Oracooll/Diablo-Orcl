/**
 * @file floatingnumbers.h
 *
 * Adds floating numbers QoL feature
 */
#pragma once

#include "DiabloUI/ui_flags.hpp"
#include "engine/point.hpp"
#include "misdat.h"
#include "monster.h"
#include "player.h"
#include "utils/attributes.h"

namespace devilution {

/**
 * @brief The font colour a damage number wears, by element.
 *
 * Exported so the mapping is a fact the suite can assert rather than a switch inside a file-local
 * draw helper. It earned that after fire spent ten versions drawing GREY: the colour was chosen
 * by a name (ColorUiSilver) whose meaning changed underneath it when renderer stage 4 turned the
 * .trn files into RGB values, and nothing anywhere compared the two.
 */
DVL_API_FOR_TEST UiFlags DamageTextColor(DamageType type);

void AddFloatingNumber(DamageType damageType, const Monster &monster, int damage);
void AddFloatingNumber(DamageType damageType, const Player &player, int damage);
void DrawFloatingNumbers(const Surface &out, Point viewPosition, Displacement offset);
void ClearFloatingNumbers();

} // namespace devilution
