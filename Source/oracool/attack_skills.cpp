#include "oracool/attack_skills.h"

#include "panels/spell_icons.hpp"
#include "spells.h"

#include <cassert>

#include "engine/size.hpp"
#include "levels/gendung.h"
#include "engine/backbuffer_state.hpp" // RedrawEverything
#include "oracool/class_tree.h" // the lit aura takes the RMB well when nothing is readied
#include "oracool/hud_art.h"
#include "oracool/ornate_border.h" // DrawHoverOutline
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
void DrawWellIcon(const Surface &out, Rectangle net, SpellID spell, SpellType type)
{
	// Takes ITS OWN well's net rect, and this is not a detail.
	//
	// Bug (fixed 2026-08-18, user: "now nothing lands on the LMB [...] i think left click lands
	// everything on RMB"): the Paladin-skill branch below asked for GetRmbSkillWellNetRect()
	// unconditionally, so a skill readied on the LEFT button was painted into the RIGHT button's
	// well. The assignment was right, the picture was in the wrong hole, and the two wells looked
	// exactly like one well that ignored the left mouse button.
	if (!IsValidSpell(spell)) {
		DrawAttackIconScaledTo(out, net, static_cast<int>(BasicAttackIcon(*MyPlayer)), /*active=*/true);
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
		if (TryDrawSkillSpellIcon(out, net, spell, tint))
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
	// Any skill carrying its own tree art, now that the answer to "can I cast it" is in hand - the
	// tree rows are the wells' usual content, and this is what makes them fill the net rect rather
	// than sitting at their natural 56px over the bezel.
	if (TryDrawSkillSpellIcon(out, net, spell,
	        usable ? SkillPlateTint::Green : SkillPlateTint::Pink))
		return;
	// Pink, not SpellType::Invalid's grey (user request, 2026-08-16): grey already means "not learned"
	// on the Abilities window, and one colour meaning two things is how the confusion started. The
	// Scroll table is the engine's own beige/pink mapping.
	SetSpellTrans(usable ? type : SpellType::Scroll);
	// Scaled into the net rect like everything else, so a readied SPELL sits exactly where a readied
	// skill would (user, 2026-08-19).
	DrawSmallSpellIconFittedTo(out, net, spell);
}

void DrawLmbSkillWell(const Surface &out)
{
	if (WellIconSize().width == 0)
		return;
	// Always "active": a well shows what its button does right now, so there is no inactive state for
	// it to render. The dimmed variant belongs to the Abilities window's row pair, where it says
	// which of the two attacks is the one in your hands.
	DrawWellIcon(out, GetLmbSkillWellNetRect(), MyPlayer->_pLRSpell, MyPlayer->_pLRSplType);
}

namespace {

/** @brief Quick-list state. Not persisted: it is a popup, not a setting. */
bool QuickListOpen = false;
bool QuickListForLeft = false;

/** @brief Air between the strip's cells, and between the strip and the well it sits above. */
constexpr int QuickListGap = 4;

/** @brief Screen rect of the whole strip, and of entry @p index within it. */
Rectangle QuickListRect()
{
	const Size icon = GetAttackIconSize();
	const Rectangle well = QuickListForLeft ? GetLmbSkillButtonRect() : GetRmbSkillButtonRect();
	const int width = static_cast<int>(AttackIconCount) * icon.width
	    + (static_cast<int>(AttackIconCount) - 1) * QuickListGap;
	// Centred over its own well and sitting directly above it, so the strip reads as belonging to
	// the button that opened it rather than as a panel of its own.
	return { { well.position.x + (well.size.width - width) / 2,
		         well.position.y - icon.height - QuickListGap },
		{ width, icon.height } };
}

Rectangle QuickListEntryRect(size_t index)
{
	const Size icon = GetAttackIconSize();
	const Rectangle strip = QuickListRect();
	return { { strip.position.x + static_cast<int>(index) * (icon.width + QuickListGap), strip.position.y },
		icon };
}

} // namespace

void OpenAttackQuickList(bool forLeftButton)
{
	QuickListOpen = true;
	QuickListForLeft = forLeftButton;
}

bool IsAttackQuickListOpen()
{
	return QuickListOpen;
}

void CloseAttackQuickList()
{
	QuickListOpen = false;
}

void DrawAttackQuickList(const Surface &out)
{
	if (!QuickListOpen)
		return;
	if (GetAttackIconSize().width == 0)
		return; // no strip shipped - drawing an empty frame would be worse than drawing nothing

	const AttackIcon inHand = BasicAttackIcon(*MyPlayer);
	for (size_t i = 0; i < AttackIconCount; i++) {
		const AttackIcon icon = AttackIconDisplayOrder[i];
		const Rectangle cell = QuickListEntryRect(i);
		// The one in hand is drawn active; the other is the same state wearing the other face, so it
		// is dimmed rather than hidden - the pair is the point.
		DrawAttackIcon(out, cell.position, static_cast<int>(icon), icon == inHand,
		    SkillPlateTint::Green);
		if (cell.contains(MousePosition))
			DrawHoverOutline(out, cell);
	}
}

bool CheckAttackQuickListClick()
{
	if (!QuickListOpen)
		return false;

	for (size_t i = 0; i < AttackIconCount; i++) {
		if (!QuickListEntryRect(i).contains(MousePosition))
			continue;
		// Both entries mean the same thing - see the header. Readying the basic attack IS clearing
		// the readied spell.
		Player &player = *MyPlayer;
		if (QuickListForLeft) {
			player._pLRSpell = SpellID::Invalid;
			player._pLRSplType = SpellType::Invalid;
		} else {
			player._pRSpell = SpellID::Invalid;
			player._pRSplType = SpellType::Invalid;
		}
		QuickListOpen = false;
		RedrawEverything();
		return true;
	}

	// A click anywhere else dismisses it, matching every other popup here.
	QuickListOpen = false;
	RedrawEverything();
	return true;
}

void DrawRmbSkillWell(const Surface &out)
{
	if (WellIconSize().width == 0)
		return;
	// A lit AURA is this well's occupant, at full size (user, 2026-08-18: "auras don't land
	// anywhere"). An aura carries no SpellID - it is a toggle, not a cast - so it can never arrive
	// here as _pRSpell, and lighting one used to change nothing on the HUD at all.
	//
	// No coexistence to arbitrate: lighting an aura clears the readied skill and readying a skill
	// puts the aura out, so at most one of them is ever here. See ClearClassAuraForRightButton.
	if (const ClassTreeSkill aura = GetActiveClassAura(*MyPlayer); aura != ClassTreeSkill::None) {
		DrawClassTreeSkillInWell(out, GetRmbSkillWellNetRect(), MyPlayer->_pClass,
		    ClassTreeIconIndex(aura));
		return;
	}
	DrawWellIcon(out, GetRmbSkillWellNetRect(), MyPlayer->_pRSpell, MyPlayer->_pRSplType);
}

} // namespace devilution::oracool
