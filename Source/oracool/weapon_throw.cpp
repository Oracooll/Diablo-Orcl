#include "oracool/weapon_throw.h"

#include "engine/direction.hpp"
#include "misdat.h"
#include "missiles.h"
#include "oracool/rage.h"
#include "player.h"

namespace devilution::oracool {

namespace {

/** The latch: where the armed swing throws. Only ever armed for the local player. */
std::optional<Point> ArmedTarget;

} // namespace

bool IsWeaponThrow(SpellID spell)
{
	return spell == SpellID::WeaponThrow;
}

bool CanThrowWeapon(const Player &player)
{
	if (player.UsesRangedWeapon())
		return false;
	for (const Item &item : { player.InvBody[INVLOC_HAND_LEFT], player.InvBody[INVLOC_HAND_RIGHT] }) {
		if (!item.isEmpty() && item._iStatFlag && (item._itype == ItemType::Sword || item._itype == ItemType::Axe))
			return true;
	}
	return false;
}

MissileGraphicID ThrownWeaponGraphic(const Player &player)
{
	for (const Item &item : { player.InvBody[INVLOC_HAND_LEFT], player.InvBody[INVLOC_HAND_RIGHT] }) {
		if (!item.isEmpty() && item._iStatFlag && item._itype == ItemType::Axe)
			return MissileGraphicID::ThrownAxe;
	}
	return MissileGraphicID::ThrownSword;
}

void ArmWeaponThrow(std::optional<Point> target)
{
	ArmedTarget = target;
}

bool IsWeaponThrowArmed()
{
	return ArmedTarget.has_value();
}

bool ThrowArmedWeapon(Player &player)
{
	if (&player != MyPlayer || !ArmedTarget.has_value())
		return false;
	const Point target = *ArmedTarget;
	ArmedTarget = std::nullopt;
	if (!CanThrowWeapon(player) || !CanPaySkill(player, SpellID::WeaponThrow))
		return false;

	const Point from = player.position.tile;
	const Point dst = target == from ? from + player._pdir : target;
	// The engine's arrow flies with the wielder's own damage range (ProcessArrow), which is exactly a thrown weapon.
	Missile *thrown = AddMissile(from, dst, GetDirection(from, dst), MissileID::Arrow, TARGET_MONSTERS, static_cast<int>(player.getId()), 4, 0);
	if (thrown == nullptr)
		return false;
	// And it looks like the weapon, spinning (RfA-16): the axe's sheet for an axe, the sword's otherwise.
	const MissileGraphicID spin = ThrownWeaponGraphic(player);
	if (MissileArtLoaded(spin))
		UseMissileGraphic(*thrown, spin);
	SettleSkill(player, SpellID::WeaponThrow, /*landedBlows=*/0);
	return true;
}

} // namespace devilution::oracool
