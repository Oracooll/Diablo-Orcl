#include "panels/spell_list.hpp"

#include <cstdint>

#include <fmt/format.h>

#include "control.h"
#include "oracool/cursor_tooltip.h" // ShowPanelStringsAsHintCard - the spell list's card
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
	// ONE draw for both wells (round 27 audit): this path drew a readied spell itself and drifted from DrawWellIcon -
	// a book spell with a tree row kept full colour at 0 mana, a staff spell got no orange plate or charge count, and
	// the well did not sink while held. DrawRmbSkillWell draws the lit aura, the basic attack and every readied spell.
	//
	// GetHotkeyName goes in as the badge fallback: it also reads the vanilla QuickSpell9-12 slots, which have keymapper
	// names rather than fixed F-numbers and which DrawWellBadges cannot name.
	const SpellID spl = myPlayer._pRSpell;
	const std::optional<string_view> hotkeyName = IsValidSpell(spl) ? GetHotkeyName(spl, myPlayer._pRSplType, true) : std::nullopt;
	oracool::DrawRmbSkillWell(out, hotkeyName ? *hotkeyName : string_view {});
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
			// Oracool: the fallback for Charge when its strip art is missing - the bare plate, never
			// the vanilla Item Repair icon, for the Warrior's slot.
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
		oracool::ShowPanelStringsAsHintCard(); // the vendors' gold card (dev note, 2026-09-28)
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
	// A binding whose kind the hero no longer has, for a spell he can still cast another way, takes that way (round 90 audit:
	// F3 bound to a scroll went dead once the book was read and the scrolls used up).
	// Not while the scroll stack is in hand (round 91 audit: sorting it away re-typed the binding to the spell for good).
	if (!HeroHasBinding(myPlayer, spellId, myPlayer._pSplTHotKey[slot])
	    && !(myPlayer._pSplTHotKey[slot] == SpellType::Scroll && (myPlayer.HoldItem.isScrollOf(spellId) || myPlayer.HoldItem.isRuneOf(spellId)))) {
		const SpellType derived = BindingTypeFor(myPlayer, spellId);
		if (HeroHasBinding(myPlayer, spellId, derived))
			myPlayer._pSplTHotKey[slot] = derived;
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
