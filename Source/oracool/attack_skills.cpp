#include "oracool/attack_skills.h"

#include "panels/spell_icons.hpp"
#include "spells.h"

#include <cassert>

#include "engine/size.hpp"
#include "levels/gendung.h"
#include "oracool/hud_art.h"
#include "oracool/hud_layout.h"
#include "oracool/paladin_skills.h"
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
	if (loaded.width != 0)
		return loaded;
	// No attack strip shipped. The wells used to give up here and draw nothing - which is exactly
	// what a screenshot showed once the icon sheets were removed - but they now also draw a readied
	// SPELL's own icon, and that art is the engine's and always present. So fall back to its size
	// rather than returning 0 and blanking a well that has something to show.
	return GetSmallSpellIconSize();
}

} // namespace

/**
 * @brief Draws @p spell's own icon in a well, or the basic-attack icon when nothing is assigned.
 *
 * Oracool: user request (2026-08-15) - "LMB and RMB - To show actual Skills Icons." Both wells used
 * to draw the basic-attack icon unconditionally, so a readied spell was invisible on the button that
 * would cast it. The spell icons anchor BOTTOM-left where the attack strip anchors top-left, which is
 * the whole reason this is one function rather than two lines at each call site.
 */
void DrawWellIcon(const Surface &out, Point origin, Size iconSize, SpellID spell, SpellType type)
{
	if (!IsValidSpell(spell)) {
		DrawAttackIcon(out, origin, static_cast<int>(BasicAttackIcon(*MyPlayer)), /*active=*/true);
		return;
	}
	// The Paladin's skills are real SpellIDs but have no frame in the engine's icon sheet, so asking
	// for one draws the empty plate - which is exactly what the wells showed for a readied Charge.
	// Their art comes from ui\paladin_skill_icons.png instead; anything else falls through.
	//
	// The plate says whether the skill can be thrown RIGHT NOW: pink for "not at the moment" (out of
	// mana, missing shield - user request, 2026-08-16), green otherwise. The same question every
	// engine spell answers below by going pink.
	if (const std::optional<PaladinSkill> skill = PaladinSkillForSpell(spell); skill.has_value()) {
		const SkillPlateTint tint = CanUsePaladinSkill(*MyPlayer, *skill)
		    ? SkillPlateTint::Green
		    : SkillPlateTint::Pink;
		if (TryDrawSkillSpellIcon(out, origin, spell, tint))
			return;
	}
	// The same can-I-actually-cast-this dance DrawSpell does for the RMB well (self-audit,
	// 2026-08-15): without it the LMB well kept a spell's full colour while the RMB well correctly
	// greyed it on empty mana - two wells, two answers to one question.
	bool usable = true;
	if (type == SpellType::Spell) {
		if (MyPlayer->GetSpellLevel(spell) <= 0
		    || CheckSpell(*MyPlayer, spell, type, /*manaonly=*/true) != SpellCheckResult::Success)
			usable = false;
	}
	if (leveltype == DTYPE_TOWN && !GetSpellData(spell).isAllowedInTown())
		usable = false;
	// Pink, not SpellType::Invalid's grey (user request, 2026-08-16): grey already means "not learned"
	// on the Abilities window, and one colour meaning two things is how the confusion started. The
	// Scroll table is the engine's own beige/pink mapping.
	SetSpellTrans(usable ? type : SpellType::Scroll);
	DrawSmallSpellIcon(out, { origin.x, origin.y + iconSize.height - 1 }, spell);
}

void DrawLmbSkillWell(const Surface &out)
{
	const Size iconSize = WellIconSize();
	if (iconSize.width == 0)
		return;
	// Always "active": a well shows what its button does right now, so there is no inactive state for
	// it to render. The dimmed variant belongs to the Abilities window's row pair, where it says
	// which of the two attacks is the one in your hands.
	DrawWellIcon(out, GetLmbSkillIconOrigin(iconSize), iconSize, MyPlayer->_pLRSpell, MyPlayer->_pLRSplType);
}

void DrawRmbSkillWell(const Surface &out)
{
	const Size iconSize = WellIconSize();
	if (iconSize.width == 0)
		return;
	DrawWellIcon(out, GetRmbSkillIconOrigin(iconSize), iconSize, MyPlayer->_pRSpell, MyPlayer->_pRSplType);
}

} // namespace devilution::oracool
