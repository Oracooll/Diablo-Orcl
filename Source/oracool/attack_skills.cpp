#include "oracool/attack_skills.h"

#include <cassert>

#include "engine/size.hpp"
#include "oracool/hud_art.h"
#include "oracool/hud_layout.h"
#include "utils/language.h"

namespace devilution::oracool {

bool IsFightingUnarmed(const Player &player)
{
	const auto weapon = static_cast<PlayerWeaponGraphic>(player._pgfxnum & 0xF);
	return weapon == PlayerWeaponGraphic::Unarmed || weapon == PlayerWeaponGraphic::UnarmedShield;
}

AttackIcon BasicAttackIcon(const Player &player)
{
	return IsFightingUnarmed(player) ? AttackIcon::Fist : AttackIcon::Regular;
}

const char *AttackIconName(AttackIcon icon)
{
	switch (icon) {
	case AttackIcon::Fist:
		return N_("Fist Attack");
	case AttackIcon::Regular:
		break;
	}
	return N_("Regular Attack");
}

const char *AttackIconDetail(AttackIcon icon, bool active)
{
	if (active)
		return N_(/* TRANSLATORS: UI constraints, keep short please.*/ "Your basic attack");
	switch (icon) {
	case AttackIcon::Fist:
		return N_(/* TRANSLATORS: UI constraints, keep short please.*/ "Only with no weapon");
	case AttackIcon::Regular:
		break;
	}
	return N_(/* TRANSLATORS: UI constraints, keep short please.*/ "Needs a weapon");
}

namespace {

/**
 * @brief The wells' icon size, or {0,0} if the art is missing.
 *
 * DrawAttackIcon takes a TOP-left origin (it is a strip blit), unlike the engine's
 * DrawSmallSpellIcon, which anchors at the BOTTOM-left. Worth stating because the RMB well draws
 * both, depending on whether a spell is readied, and the two would sit a whole icon apart if the
 * anchors were confused.
 */
Size WellIconSize()
{
	const Size loaded = GetAttackIconSize();
	// The static_asserts in hud_layout.cpp check SkillWellIconSize against the wells; this checks the
	// shipped art against SkillWellIconSize, which is the half they cannot see. Together they mean an
	// icon recut at a size that no longer fits cannot reach the screen unnoticed.
	assert(loaded.width == 0
	    || (loaded.width == SkillWellIconSize.width && loaded.height == SkillWellIconSize.height));
	return loaded;
}

} // namespace

void DrawLmbSkillWell(const Surface &out)
{
	const Size iconSize = WellIconSize();
	if (iconSize.width == 0)
		return;
	// Always "active": a well shows what its button does right now, so there is no inactive state for
	// it to render. The dimmed variant belongs to the Abilities window's row pair, where it says
	// which of the two attacks is the one in your hands.
	DrawAttackIcon(out, GetLmbSkillIconOrigin(iconSize), static_cast<int>(BasicAttackIcon(*MyPlayer)), /*active=*/true);
}

void DrawRmbSkillWell(const Surface &out)
{
	const Size iconSize = WellIconSize();
	if (iconSize.width == 0)
		return;
	DrawAttackIcon(out, GetRmbSkillIconOrigin(iconSize), static_cast<int>(BasicAttackIcon(*MyPlayer)), /*active=*/true);
}

} // namespace devilution::oracool
