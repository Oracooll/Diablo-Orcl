#pragma once
/**
 * @file oracool/weapon_throw.h
 *
 * The Barbarian's Weapon Throw (user note, 2026-09-14: "We remove Double Throw skills from the game as we dont
 * have such sprites of the warior, but we need to keep single weapon throw. Barb with sield and sword of just
 * sword or just axe will be able to throw weapons just using normal attack animation.").
 *
 * The click becomes the ordinary attack, in place, toward the cursor (CMD_SATTACKXY), with a latch naming the
 * throw and its target - the same latch shape the melee and bow skills use. At the swing's hit frame DoAttack asks
 * ThrowArmedWeapon, which lets the weapon go instead of striking: a missile flying with the weapon's own damage
 * (the engine's arrow carries the wielder's damage range). The weapon is not dropped - the next blow swings it.
 *
 * Needs a sword or an axe in hand, one- or two-handed, with or without a shield; a bow is a bow. Its art - a
 * spinning sword and a spinning axe - is RfA-16 (delivered 2026-09-14); without those sheets it flies as an arrow.
 */

#include <optional>

#include "engine/point.hpp"
#include "misdat.h"
#include "spelldat.h"

namespace devilution {
struct Player;
} // namespace devilution

namespace devilution::oracool {

bool IsWeaponThrow(SpellID spell);

/** @brief Whether @p player holds something to throw: a sword or an axe, and no bow. */
bool CanThrowWeapon(const Player &player);

/** @brief The spinning sheet @p player's throw wears: the axe's if an axe is in hand, the sword's otherwise. */
MissileGraphicID ThrownWeaponGraphic(const Player &player);

/** @brief Arms the next swing to throw at @p target, or disarms it. */
void ArmWeaponThrow(std::optional<Point> target);

/**
 * @brief At the hit frame of @p player's swing: throws the armed weapon and settles its price. True if it threw,
 * in which case the swing strikes nothing itself. The latch is spent either way.
 */
bool ThrowArmedWeapon(Player &player);

} // namespace devilution::oracool
