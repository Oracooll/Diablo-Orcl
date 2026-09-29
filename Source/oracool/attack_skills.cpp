#include "oracool/attack_skills.h"

#include "panels/spell_icons.hpp"
#include "spells.h"

#include <cassert>

#include "engine/size.hpp"
#include "levels/gendung.h"
#include "engine/backbuffer_state.hpp" // RedrawEverything
#include "oracool/class_tree.h" // the lit aura takes the RMB well when nothing is readied
#include "oracool/badge.h"
#include "oracool/hud_art.h"
#include "oracool/ornate_border.h" // DrawHoverOutline
#include "diablo.h"           // MousePosition - the HUD buttons' hover sense
#include "oracool/hud_layout.h"
#include "oracool/ui_sound.h"  // PlayUiMoveSound - the hover and click sound
#include "oracool/xp_counter.h" // IsPointOverXpBar - the XP bar is one of the HUD's buttons
#include "oracool/paladin_skills.h"
#include "oracool/whirlwind.h" // RightButtonOnly
#include "panels/spell_book.hpp" // GetAbilityFKeyNumber, GetAuraFKeyNumber
#include "utils/language.h"
#include "utils/str_cat.hpp"

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

/**
 * @brief What @p spell is worth to @p player, for the rank badge. Zero when it has no rank to show.
 *
 * Two stores, one question. A tree row's rank is the investment recorded against the row - which is
 * why this asks ClassTreeSkillForSpell FIRST: a tree skill also has a Player::_pSplLvl entry, and it
 * is not the number the tree pages, the quick lists or the tooltips report. Reading the wrong one
 * would put a different rank on the well than on every other surface showing the same icon.
 */
int WellRank(const Player &player, SpellID spell)
{
	if (!IsValidSpell(spell))
		return 0; // the basic attack, which has no rank - see the header on why it is Invalid
	if (const ClassTreeSkill row = ClassTreeSkillForSpell(player._pClass, spell);
	    row != ClassTreeSkill::None) {
		return ClassTreeInvestment(player, row);
	}
	return player.GetSpellLevel(spell);
}

} // namespace

void DrawWellBadges(const Surface &out, Rectangle net, SpellID spell, bool leftButton,
    string_view hotkeyFallback)
{
	if (const int rank = WellRank(*MyPlayer, spell); rank > 0)
		DrawBadge(out, net, BadgeCorner::BottomRight, StrCat(rank));

	// F1-F8 first, from the arrays those keys actually read, and only then the caller's name for the
	// keymapper rows past them. Not the other way round: the reserved keys are a constant of the code
	// now (they are intercepted before the keymapper ever sees them), so a settled ini carrying a
	// stale QuickSpell row must never be able to overwrite the key that really fires.
	if (const int fkey = GetAbilityFKeyNumber(spell, leftButton); fkey != 0) {
		DrawBadge(out, net, BadgeCorner::TopRight, StrCat("F", fkey));
	} else if (!hotkeyFallback.empty()) {
		DrawBadge(out, net, BadgeCorner::TopRight, hotkeyFallback);
	}
}

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
		    ? SkillPlateTint::Ready
		    : SkillPlateTint::Blocked;
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
	if (LacksShieldFor(*MyPlayer, spell))
		usable = false; // Aegis Slam without a shield: red (2026-09-29); Smite and Blessed Shield answered above
	if (RightButtonOnly(spell))
		usable = false; // Whirlwind and Earthquake belong on the right button: red here (the Barbarian Skill Cards page, 2026-09-29)

	// A STAFF cast keeps the engine's orange charge plate, and skips the tree-art path entirely
	// (user, 2026-09-03: "staff spells to use legacy orange backing"). Without this a staff spell
	// that also has a tree row - which, since the Sorceress's book rows came back, is most of her
	// arsenal - was drawn on the green skill plate, so the well disagreed with the picker cell that
	// bound it about where the cast is coming from.
	if (type == SpellType::Charges) {
		SetSpellTrans(usable ? SpellType::Charges : SpellType::Scroll);
		DrawSpellIconFittedTo(out, SkillWellPlateRect(net), spell); // the 56px frame at the 56px opening (2026-09-05)
		DrawStaffChargeBadge(out, net, *MyPlayer, spell);
		return;
	}
	// Any skill carrying its own tree art, now that the answer to "can I cast it" is in hand - the
	// tree rows are the wells' usual content, and this is what makes them fill the net rect rather
	// than sitting at their natural 56px over the bezel.
	if (TryDrawSkillSpellIcon(out, net, spell,
	        usable ? SkillPlateTint::Ready : SkillPlateTint::Blocked))
		return;
	// RED when it cannot be cast right now (2026-09-05 coding): the beige that said so before is a
	// scroll's own colour now. A castable spell wears its type's colour - blue, a scroll's beige,
	// a staff's orange.
	if (usable)
		SetSpellTrans(type);
	else
		SetSpellTransRed();
	// Scaled into the net rect like everything else, so a readied SPELL sits exactly where a readied
	// skill would (user, 2026-08-19).
	DrawSpellIconFittedTo(out, SkillWellPlateRect(net), spell); // the 56px frame at the 56px opening (2026-09-05)
}

namespace {

/** @brief The well held down: 0 the LMB well, 1 the RMB well, -1 none. See TrackHudButtonHover in the header. */
int PressedWell = -1;
/** @brief The HUD button under the cursor last frame: 0 LMB well, 1 RMB well, 2 + slot a belt cell, -1 none. */
int LastHudHover = -1;
constexpr Displacement WellPressSink { -2, 2 };

Displacement WellSink(bool leftWell)
{
	return PressedWell == (leftWell ? 0 : 1) ? WellPressSink : Displacement { 0, 0 };
}

int HudButtonUnder(Point mouse)
{
	if (GetLmbSkillButtonRect().contains(mouse))
		return 0;
	if (GetRmbSkillButtonRect().contains(mouse))
		return 1;
	for (int slot = 0; slot < BeltVisibleSlotCount; slot++) {
		if (GetBeltSlotRect(slot).contains(mouse))
			return 2 + slot;
	}
	if (IsPointOverXpBar(mouse))
		return 2 + BeltVisibleSlotCount; // the XP bar above the belt (2026-09-27)
	return -1;
}

} // namespace

void TrackHudButtonHover()
{
	const int hovered = HudButtonUnder(MousePosition);
	if (hovered >= 0 && hovered != LastHudHover)
		PlayUiMoveSound();
	LastHudHover = hovered;
}

void PressHudWell(bool leftWell)
{
	PressedWell = leftWell ? 0 : 1;
}

void ReleaseHudWells()
{
	PressedWell = -1;
}

void DrawLmbSkillWell(const Surface &out)
{
	// The HUD's per-frame hover sense lives here because this draw runs whenever the HUD does.
	TrackHudButtonHover();
	if (WellIconSize().width == 0)
		return;
	// The points frame behind the icon (user, 2026-08-30). BEFORE the icon, obviously, but also
	// before the net rect is used: the backing is centred on the well's OPENING while the icon is
	// placed by GetLmbSkillWellNetRect, which carries its own optical nudge - so the two are
	// deliberately positioned by different rules and must not be made to share one.
	DrawSkillWellBacking(out, GetLmbSkillButtonRect());
	// Always "active": a well shows what its button does right now, so there is no inactive state for
	// it to render. The dimmed variant belongs to the Abilities window's row pair, where it says
	// which of the two attacks is the one in your hands.
	// The icon (and its badges) sink while the well is held down - the click effect (2026-09-20).
	const Rectangle net { GetLmbSkillWellNetRect().position + WellSink(/*leftWell=*/true), GetLmbSkillWellNetRect().size };
	DrawWellIcon(out, net, MyPlayer->_pLRSpell, MyPlayer->_pLRSplType);
	// The LEFT button's bindings, which live in their own array (user, 2026-09-02). No fallback name:
	// only the RMB well inherited the vanilla QuickSpell9-12 rows, and those write the right button's
	// hotkey array, so there is nothing here for a fallback to find.
	DrawWellBadges(out, net, MyPlayer->_pLRSpell, /*leftButton=*/true);
}

// The quick-list strip and its state were removed here on 2026-08-30 - see the note in the header.
// The skill picker owns this gesture now.

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
	const Rectangle net { GetRmbSkillWellNetRect().position + WellSink(/*leftWell=*/false), GetRmbSkillWellNetRect().size }; // sinks while held
	if (const ClassTreeSkill aura = GetActiveClassAura(*MyPlayer); aura != ClassTreeSkill::None) {
		DrawClassTreeSkillInWell(out, net, MyPlayer->_pClass, ClassTreeIconIndex(aura));
		// Badged by hand rather than through DrawWellBadges: an aura carries no SpellID - it is a
		// toggle, not a cast - so neither the rank lookup nor the F-key lookup there can find it.
		// It has one binding whichever list lit it, hence GetAuraFKeyNumber's missing side argument.
		if (const int rank = ClassTreeInvestment(*MyPlayer, aura); rank > 0)
			DrawBadge(out, net, BadgeCorner::BottomRight, StrCat(rank));
		if (const int fkey = GetAuraFKeyNumber(aura); fkey != 0)
			DrawBadge(out, net, BadgeCorner::TopRight, StrCat("F", fkey));
		return;
	}
	// Nothing readied is the basic attack, and the badges know it: DrawWellBadges finds no rank and
	// no binding for SpellID::Invalid, so this draws the icon alone, exactly as it did before.
	DrawWellIcon(out, net, MyPlayer->_pRSpell, MyPlayer->_pRSplType);
	DrawWellBadges(out, net, MyPlayer->_pRSpell, /*leftButton=*/false);
}

} // namespace devilution::oracool
