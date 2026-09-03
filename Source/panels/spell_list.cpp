#include "panels/spell_list.hpp"

#include <cstdint>

#include <fmt/format.h>

#include "control.h"
#include "oracool/class_tree.h" // the lit aura owns the RMB well
#include "controls/plrctrls.h"
#include "engine.h"
#include "engine/backbuffer_state.hpp"
#include "engine/palette.h"
#include "engine/render/primitive_render.hpp"
#include "engine/render/text_render.hpp"
#include "inv_iterators.hpp"
#include "options.h"
#include "oracool/badge.h"
#include "oracool/attack_skills.h"
#include "oracool/furious_charge.h"
#include "oracool/hud_art.h"
#include "oracool/hud_layout.h"
#include "oracool/oracool.h"
#include "oracool/ornate_border.h"
#include "oracool/paladin_skills.h"
#include "panels/spell_book.hpp" // ToggleAbilitiesWindow
#include "panels/spell_icons.hpp"
#include "oracool/auto_save.h"
#include "player.h"
#include "spells.h"
#include "utils/language.h"
#include "utils/str_cat.hpp"
#include "utils/utf8.hpp"

#define SPLROWICONLS 10

namespace devilution {

namespace {

void PrintSBookSpellType(const Surface &out, Point position, string_view text, uint8_t rectColorIndex)
{
	DrawLargeSpellIconBorder(out, position, rectColorIndex);

	// Align the spell type text with bottom of spell icon
	position += Displacement { SPLICONLENGTH / 2 - GetLineWidth(text) / 2, (IsSmallFontTall() ? -19 : -15) };

	// Then draw the text over the top
	DrawString(out, text, position, { UiFlags::ColorWhite | UiFlags::Outlined });
}

void PrintSBookHotkey(const Surface &out, Point position, const string_view text)
{
	// The badge template (user, 2026-08-31), not the one-pixel outline this used to wear. An outline
	// keeps white text legible against a dark sprite and loses to a bright one; the dark plate wins
	// against both. See oracool/badge.h.
	//
	// `position` is the icon's BOTTOM-left, which is where the speedbook's own draw calls are
	// anchored, so the icon rect is built back from it rather than passed in.
	const Rectangle icon { { position.x, position.y - SPLICONLENGTH }, { SPLICONLENGTH, SPLICONLENGTH } };
	oracool::DrawBadge(out, icon, oracool::BadgeCorner::TopRight, text);
}

bool GetSpellListSelection(SpellID &pSpell, SpellType &pSplType)
{
	pSpell = SpellID::Invalid;
	pSplType = SpellType::Invalid;
	Player &myPlayer = *MyPlayer;

	for (auto &spellListItem : GetSpellListItems()) {
		if (spellListItem.isSelected) {
			pSpell = spellListItem.id;
			pSplType = spellListItem.type;
			if (myPlayer._pClass == HeroClass::Monk && spellListItem.id == SpellID::Search)
				pSplType = SpellType::Skill;
			return true;
		}
	}

	return false;
}

std::optional<string_view> GetHotkeyName(SpellID spellId, SpellType spellType, bool useShortName = false)
{
	Player &myPlayer = *MyPlayer;
	for (size_t t = 0; t < NumHotkeys; t++) {
		if (myPlayer._pSplHotKey[t] != spellId || myPlayer._pSplTHotKey[t] != spellType)
			continue;

		// The first eight slots ARE F1-F8, and this says so directly instead of asking the keymapper
		// (user, 2026-08-31: "i have set F1-F4 skills in rmb but when actualy loading the skills
		// with the hotkeys the icons in rmb show F5-F8 as a badge").
		//
		// Those keys are intercepted in PressKey BEFORE sgOptions.Keymapper ever sees them, and the
		// QuickSpell actions were made default-unbound in 2026-08-17 for exactly that reason. But a
		// SETTLED diablo.ini still carries the old QuickSpell1..4 = F5..F8 rows, and this function
		// was reading them - so the badge reported a binding that does nothing while hiding the key
		// that actually fires. Off by four, which is precisely the gap between the two.
		//
		// Asking the keymapper here could never have been right once the keys became reserved: the
		// mapping is a constant of the code now, not a setting.
		if (t < AbilityFKeyCount) {
			static char reservedName[4];
			*BufCopy(reservedName, "F", t + 1) = '\0';
			return string_view(reservedName);
		}

		// Slots past the reserved eight are still ordinary keymapper actions, and there the ini is
		// the truth.
		auto quickSpellActionKey = StrCat("QuickSpell", t + 1);
		if (ControlMode == ControlTypes::Gamepad)
			return sgOptions.Padmapper.InputNameForAction(quickSpellActionKey, useShortName);
		return sgOptions.Keymapper.KeyNameForAction(quickSpellActionKey);
	}
	return {};
}


} // namespace
void DrawSpell(const Surface &out)
{
	Player &myPlayer = *MyPlayer;

	// Hidden while the chat box is open (user, 2026-08-18: "some HUD UI assets remain visible when
	// the text input dialog is ON - town portal and RMB skill"). The plate itself already steps
	// aside for the chat; these two were drawn separately and stayed behind.
	if (talkflag)
		return;

	// The points frame behind whatever this well ends up holding (user, 2026-08-30). HERE rather
	// than in either branch below, because this well has two of them - DrawRmbSkillWell for an
	// empty slot or a lit aura, and the direct drawing further down for a readied spell - and a
	// backing added to one of them only would appear and vanish as the player readies and clears.
	oracool::DrawSkillWellBacking(out, oracool::GetRmbSkillButtonRect());

	// A LIT AURA never reaches this function's own drawing, and does not need to: lighting one clears
	// _pRSpell, so the no-spell-readied branch below runs and DrawRmbSkillWell paints the aura at
	// full size. See ClearClassAuraForRightButton for why the two cannot both be here.
	//
	// The history is worth keeping: for one version the aura drew HERE and returned, which hid every
	// skill readied afterwards ("combat skills don't assign to RMB"); for the next it wore a corner
	// badge over the skill, which the user read as the aura failing to land at all. One slot, one
	// occupant, is the version that answers both.
	SpellID spl = myPlayer._pRSpell;
	SpellType st = myPlayer._pRSplType;

	if (!IsValidSpell(spl)) {
		// Oracool: no spell readied IS the basic attack - that is exactly the state in which a
		// right click swings rather than casts - so the well shows the attack's own icon instead of
		// the engine's blank SpellID::Null tile. See oracool/attack_skills.h. Nothing follows: an
		// unreadied slot has no hotkey to label and no cooldown to fill.
		oracool::DrawRmbSkillWell(out);
		return;
	}

	if (st == SpellType::Spell) {
		int tlvl = myPlayer.GetSpellLevel(spl);
		if (CheckSpell(*MyPlayer, spl, st, true) != SpellCheckResult::Success)
			st = SpellType::Invalid;
		if (tlvl <= 0)
			st = SpellType::Invalid;
	}

	if (leveltype == DTYPE_TOWN && st != SpellType::Invalid && !GetSpellData(spl).isAllowedInTown())
		st = SpellType::Invalid;

	// The NET rect, exactly like every other well draw (user, 2026-08-19: "spells icons and
	// background also need to respect the 46x46px size and also align where other abilities will
	// align"). This path used to compute its own geometry from the button rect plus a hand nudge, so
	// a readied SPELL sat a few pixels off from a readied SKILL in the same hole. The nudge moved to
	// GetRmbSkillWellNetRect itself, where one correction serves all of them.
	const Rectangle net = oracool::GetRmbSkillWellNetRect();

	// Oracool: while Furious Charge is active, this slot renders with a borrowed icon (see
	// FuriousChargeIcon - there's no dedicated art for a mod-only skill) instead of the normal
	// single-color Item Repair icon. User request - the cooldown fill grows in red (rather than
	// the ready color) bottom-up as it cools, then flips to the normal ready tint the instant it's fully
	// cooled, instead of gradually blending from gray to color.
	// Oracool: user report (2026-08-15) - "the skill icons dont show on rmb". The fix that gave the
	// LMB well the Paladin skills' own art went into oracool::DrawWellIcon, and THIS well only
	// delegates there when nothing is readied - the moment a spell is on the button, the drawing
	// happens right here instead, still asking the engine's icon sheet, where these skills have no
	// frame and SpellITbl points at the empty plate. So the ask has to be repeated on this path.
	//
	// The strip icons anchor TOP-left where DrawSmallSpellIcon anchors bottom-left; the net rect is
	// top-left, so the cooldown overlay below works from it directly.

	// Oracool: user request (2026-08-16) - "skills unable to perform due to whatever reason to have
	// their background turned into pink until able to perform again." For a Paladin skill the answer
	// comes from its own gate (mana, shield, level); for an engine spell it is the st that the checks
	// above already downgraded to Invalid. Pink rather than Invalid's grey, because grey already means
	// "not learned" on the Abilities window.
	oracool::SkillPlateTint wellTint = oracool::SkillPlateTint::Green;
	if (const std::optional<oracool::PaladinSkill> paladinSkill = oracool::PaladinSkillForSpell(spl);
	    paladinSkill.has_value() && !oracool::CanUsePaladinSkill(myPlayer, *paladinSkill))
		wellTint = oracool::SkillPlateTint::Pink;

	if (oracool::IsFuriousChargeSpell(spl)) {
		const float progress = oracool::GetFuriousChargeCooldownProgress();
		SetSpellTrans(st);
		if (!oracool::TryDrawSkillSpellIcon(out, net, spl, wellTint))
			DrawSmallSpellIconFittedTo(out, net, oracool::FuriousChargeIcon);
		if (progress < 1.0f) {
			// The cooldown still reads as a fill rising from the bottom, but as a DARKENED band over
			// the part not yet cooled rather than as two differently-tinted copies of the sprite. The
			// two-copy trick needed a single-ramp CLX to recolour; Charge's icon is a blitted image
			// now, with no ramp to remap, and an overlay works on any art the user ships next.
			const int cooled = static_cast<int>(net.size.height * progress);
			if (cooled < net.size.height)
				DrawHalfTransparentRectTo(out, net.position.x, net.position.y, net.size.width,
				    net.size.height - cooled);
		}
	} else if (!oracool::TryDrawSkillSpellIcon(out, net, spl, wellTint)) {
		// The engine-spell equivalent of the pink plate: st has already been downgraded to Invalid by
		// the checks above when the spell cannot be cast, and the Scroll table is the beige/pink ramp.
		SetSpellTrans(st == SpellType::Invalid ? SpellType::Scroll : st);
		DrawSmallSpellIconFittedTo(out, net, spl);
	}

	// The HUD well's badges: the rank bottom-centre and the F-key top-right, the same pair the quick
	// lists and the Abilities window put on the same icon (user, 2026-09-02). This path draws the
	// right button's well ITSELF whenever a spell is readied - DrawRmbSkillWell only gets the empty
	// and aura cases - so the shared helper has to be called here too, or the badges would appear on
	// a lit aura and vanish the moment a skill was readied over it.
	//
	// GetHotkeyName goes in as the fallback: it also reads the vanilla QuickSpell9-12 slots, which
	// have keymapper names rather than fixed F-numbers and which DrawWellBadges cannot name.
	const std::optional<string_view> hotkeyName = GetHotkeyName(spl, myPlayer._pRSplType, true);
	oracool::DrawWellBadges(out, net, spl, /*leftButton=*/false,
	    hotkeyName ? *hotkeyName : string_view {});
}

// DrawRmbAuraBadge is gone (user, 2026-08-19: "whenever an aura lands on RMB a letter appears on top
// corner of RMB slot. why?"). It was the compromise for an aura and a readied skill sharing one well;
// they no longer share it - see ClearClassAuraForRightButton - so the well simply shows its occupant,
// aura or skill, at full size and with nothing stuck to the corner.

void DrawSpellList(const Surface &out)
{
	ClearPanelStrings(); // text and its per-line colours go together - see control.h

	Player &myPlayer = *MyPlayer;

	for (auto &spellListItem : GetSpellListItems()) {
		const SpellID spellId = spellListItem.id;
		SpellType transType = spellListItem.type;
		int spellLevel = 0;
		const SpellData &spellDataItem = GetSpellData(spellListItem.id);
		if (leveltype == DTYPE_TOWN && !spellDataItem.isAllowedInTown()) {
			transType = SpellType::Invalid;
		}
		if (spellListItem.type == SpellType::Spell) {
			spellLevel = myPlayer.GetSpellLevel(spellListItem.id);
			if (spellLevel == 0)
				transType = SpellType::Invalid;
		}

		// Oracool: user bug report (2026-08-15) - the Paladin skills have no frame in the engine's
		// LARGE icon sheet, so the speedbook drew seven blank plates for them. Their strip art is
		// centred on the large plate instead, in the Skills-sheet pink; everything else keeps the
		// vanilla sheet and the type's own ramp.
		if (!oracool::TryDrawSkillSpellIconLarge(out, spellListItem.location, spellId)) {
			SetSpellTrans(transType);
			// Oracool: user request - the SpeedBook list must show the same borrowed icon Furious
			// Charge uses everywhere else, not the vanilla Item Repair icon, for the Warrior's slot.
			DrawLargeSpellIcon(out, spellListItem.location, oracool::IsFuriousChargeSpell(spellId) ? oracool::FuriousChargeIcon : spellId);
		}

		// The staff's charges, bottom-left of the large plate (user, 2026-09-03).
		if (spellListItem.type == SpellType::Charges) {
			oracool::DrawStaffChargeBadge(out,
			    Rectangle { Point { spellListItem.location.x, spellListItem.location.y - SPLICONLENGTH + 1 },
			        Size { SPLICONLENGTH, SPLICONLENGTH } },
			    myPlayer, spellId);
		}

		std::optional<string_view> shortHotkeyName = GetHotkeyName(spellId, spellListItem.type, true);

		if (shortHotkeyName)
			PrintSBookHotkey(out, spellListItem.location, *shortHotkeyName);

		if (!spellListItem.isSelected)
			continue;

		uint8_t spellColor = PAL16_GRAY + 5;

		switch (spellListItem.type) {
		case SpellType::Skill:
			spellColor = PAL16_YELLOW - 46;
			PrintSBookSpellType(out, spellListItem.location, _("Skill"), spellColor);
			InfoString = fmt::format(fmt::runtime(_("{:s} Skill")), oracool::GetSpellDisplayName(spellId));
			break;
		case SpellType::Spell:
			if (!myPlayer.isOnLevel(0)) {
				spellColor = PAL16_BLUE + 5;
			}
			PrintSBookSpellType(out, spellListItem.location, _("Spell"), spellColor);
			InfoString = fmt::format(fmt::runtime(_("{:s} Spell")), oracool::GetSpellDisplayName(spellId));
			if (spellId == SpellID::HolyBolt) {
				AddPanelString(_("Damages undead only"));
			}
			if (spellLevel == 0)
				AddPanelString(_("Spell Level 0 - Unusable"));
			else
				AddPanelString(fmt::format(fmt::runtime(_("Spell Level {:d}")), spellLevel));
			break;
		case SpellType::Scroll: {
			if (!myPlayer.isOnLevel(0)) {
				spellColor = PAL16_RED - 59;
			}
			PrintSBookSpellType(out, spellListItem.location, _("Scroll"), spellColor);
			InfoString = fmt::format(fmt::runtime(_("Scroll of {:s}")), oracool::GetSpellDisplayName(spellId));
			int scrollCount = 0;
			for (const Item &item : InventoryAndBeltPlayerItemsRange { myPlayer }) {
				if (item.isScrollOf(spellId))
					scrollCount += item.stackCount();
			}
			AddPanelString(fmt::format(fmt::runtime(ngettext("{:d} Scroll", "{:d} Scrolls", scrollCount)), scrollCount));
		} break;
		case SpellType::Charges: {
			if (!myPlayer.isOnLevel(0)) {
				spellColor = PAL16_ORANGE + 5;
			}
			PrintSBookSpellType(out, spellListItem.location, _("Staff"), spellColor);
			InfoString = fmt::format(fmt::runtime(_("Staff of {:s}")), oracool::GetSpellDisplayName(spellId));
			int charges = myPlayer.InvBody[INVLOC_HAND_LEFT]._iCharges;
			AddPanelString(fmt::format(fmt::runtime(ngettext("{:d} Charge", "{:d} Charges", charges)), charges));
		} break;
		case SpellType::Invalid:
			break;
		}
		std::optional<string_view> fullHotkeyName = GetHotkeyName(spellId, spellListItem.type);
		if (fullHotkeyName) {
			AddPanelString(fmt::format(fmt::runtime(_("Spell Hotkey {:s}")), *fullHotkeyName));
		}
	}
}

std::vector<SpellListItem> GetSpellListItems()
{
	std::vector<SpellListItem> spellListItems;

	SpellMask mask;
	const Point mainPanelPosition = GetMainPanel().position;

	int x = mainPanelPosition.x + 12 + SPLICONLENGTH * SPLROWICONLS;
	int y = mainPanelPosition.y - 17;

	for (auto i : enum_values<SpellType>()) {
		Player &myPlayer = *MyPlayer;
		switch (static_cast<SpellType>(i)) {
		case SpellType::Skill:
			mask = myPlayer._pAblSpells;
			break;
		case SpellType::Spell:
			mask = myPlayer._pMemSpells;
			break;
		case SpellType::Scroll:
			mask = myPlayer._pScrlSpells;
			break;
		case SpellType::Charges:
			mask = myPlayer._pISpells;
			break;
		default:
			continue;
		}
		// Oracool audit (2026-09-03): this walked a uint64 bit, `spl <<= 1`, which is zero from the
		// 65th spell on - so no spell with an id past 64 (the cold page, the melee and bow pages, the
		// cries) could ever appear in this list or be given an F-key from it. The masks widened to
		// 128 bits in Round 2; the walk now asks them the way everything else does.
		for (int j = static_cast<int>(SpellID::Firebolt); j < MAX_SPELLS; j++) {
			if ((mask & GetSpellBitmask(static_cast<SpellID>(j))) == 0)
				continue;
			// Oracool: Town Portal is a built-in ability cast from the HUD's Portal button, not a
			// SpeedBook entry - see oracool::IsBuiltInPortalAbility. Filtering it here also keeps
			// it out of the F5-F8 hotkey assignment, which selects from this same list.
			if (oracool::IsBuiltInPortalAbility(static_cast<SpellID>(j)))
				continue;
			int lx = x;
			int ly = y - SPLICONLENGTH;
			bool isSelected = (MousePosition.x >= lx && MousePosition.x < lx + SPLICONLENGTH && MousePosition.y >= ly && MousePosition.y < ly + SPLICONLENGTH);
			spellListItems.emplace_back(SpellListItem { { x, y }, static_cast<SpellType>(i), static_cast<SpellID>(j), isSelected });
			x -= SPLICONLENGTH;
			if (x == mainPanelPosition.x + 12 - SPLICONLENGTH) {
				x = mainPanelPosition.x + 12 + SPLICONLENGTH * SPLROWICONLS;
				y -= SPLICONLENGTH;
			}
		}
		if (mask != 0 && x != mainPanelPosition.x + 12 + SPLICONLENGTH * SPLROWICONLS)
			x -= SPLICONLENGTH;
		if (x == mainPanelPosition.x + 12 - SPLICONLENGTH) {
			x = mainPanelPosition.x + 12 + SPLICONLENGTH * SPLROWICONLS;
			y -= SPLICONLENGTH;
		}
	}

	return spellListItems;
}

void SetSpell()
{
	SpellID pSpell;
	SpellType pSplType;

	spselflag = false;
	if (!GetSpellListSelection(pSpell, pSplType)) {
		return;
	}

	Player &myPlayer = *MyPlayer;
	// The aura and the right button are one slot - see ClearClassAuraForRightButton.
	oracool::ClearClassAuraForRightButton(myPlayer);
	myPlayer._pRSpell = pSpell;
	myPlayer._pRSplType = pSplType;
	// The option is called "Auto Save on Skill Change" and this IS a skill change (audit,
	// 2026-08-26). It only fired from the class tree before, so readying a spell - the commonest
	// skill change there is - was covered only by accident, when it happened to replace an aura.
	oracool::ScheduleAutoSaveForSkillChange();

	RedrawEverything();
}

void SetSpeedSpell(size_t slot)
{
	SpellID pSpell;
	SpellType pSplType;

	if (!GetSpellListSelection(pSpell, pSplType)) {
		return;
	}
	Player &myPlayer = *MyPlayer;

	if (myPlayer._pSplHotKey[slot] == pSpell && myPlayer._pSplTHotKey[slot] == pSplType) {
		// Unset spell hotkey
		myPlayer._pSplHotKey[slot] = SpellID::Invalid;
		return;
	}

	for (size_t i = 0; i < NumHotkeys; ++i) {
		if (myPlayer._pSplHotKey[i] == pSpell && myPlayer._pSplTHotKey[i] == pSplType)
			myPlayer._pSplHotKey[i] = SpellID::Invalid;
	}
	myPlayer._pSplHotKey[slot] = pSpell;
	myPlayer._pSplTHotKey[slot] = pSplType;
}

void ToggleSpell(size_t slot)
{
	SpellMask spells;

	Player &myPlayer = *MyPlayer;

	const SpellID spellId = myPlayer._pSplHotKey[slot];
	if (!IsValidSpell(spellId)) {
		return;
	}

	switch (myPlayer._pSplTHotKey[slot]) {
	case SpellType::Skill:
		spells = myPlayer._pAblSpells;
		break;
	case SpellType::Spell:
		spells = myPlayer._pMemSpells;
		break;
	case SpellType::Scroll:
		spells = myPlayer._pScrlSpells;
		break;
	case SpellType::Charges:
		spells = myPlayer._pISpells;
		break;
	case SpellType::Invalid:
		return;
	}

	if ((spells & GetSpellBitmask(spellId)) != 0) {
		oracool::ClearClassAuraForRightButton(myPlayer);
		myPlayer._pRSpell = spellId;
		myPlayer._pRSplType = myPlayer._pSplTHotKey[slot];
		oracool::ScheduleAutoSaveForSkillChange();
		RedrawEverything();
	}
}

void DoSpeedBook()
{
	// Oracool: user request - "S button to open Abilities window. Forget about this belt of
	// spells/skills above the bottom HUD." The speedbook ring is retired: choosing a spell is the
	// Abilities window's job now, and having both would be two competing answers to the same
	// question on screen at once.
	//
	// Replacing this body rather than deleting the function is deliberate - all three callers (the
	// S key, the gamepad's quick-spell action, and the RMB well via DoPanBtn) want "open the
	// ability chooser", so they inherit the new behaviour without three separate edits.
	//
	// This is also the ONLY place that ever set spselflag true. With it gone the flag is
	// permanently false, so DrawSpellList and CheckSpellList below are unreachable and the ring
	// never renders. They are left in place for now rather than deleted, because the game is
	// running and this change cannot be compiled to check - the removal is a follow-up.
	ToggleAbilitiesWindow();
}

} // namespace devilution
