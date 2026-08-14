#include "panels/spell_list.hpp"

#include <cstdint>

#include <fmt/format.h>

#include "control.h"
#include "controls/plrctrls.h"
#include "engine.h"
#include "engine/backbuffer_state.hpp"
#include "engine/palette.h"
#include "engine/render/primitive_render.hpp"
#include "engine/render/text_render.hpp"
#include "inv_iterators.hpp"
#include "options.h"
#include "oracool/attack_skills.h"
#include "oracool/furious_charge.h"
#include "oracool/hud_layout.h"
#include "oracool/oracool.h"
#include "panels/spell_book.hpp" // ToggleAbilitiesWindow
#include "panels/spell_icons.hpp"
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
	// Align the hot key text with the top-right corner of the spell icon
	position += Displacement { SPLICONLENGTH - (GetLineWidth(text.data()) + 5), 5 - SPLICONLENGTH };

	// Then draw the text over the top
	DrawString(out, text, position, { UiFlags::ColorWhite | UiFlags::Outlined });
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

	// Oracool: HUD art pass - the plate art (assets/ui/middle_hud.png) frames the RMB well, so the
	// icon draws bare, centred inside it. The plate's wells fit the engine's SMALL spell icon, not
	// the 56px large one (small icons are always loaded in-game via InitSpellBook's
	// LoadSmallSpellIcons). The sprite's real dimensions are queried rather than assumed - only its
	// width is fixed by the CEL load call, and hardcoding a guessed height left the icon
	// visibly off-centre in the well.
	const Rectangle rmbWell = oracool::GetRmbSkillButtonRect();
	const Size iconSize = GetSmallSpellIconSize();
	const int SmallIconHeight = iconSize.height;
	// Oracool: user tuning (2026-08-11) - like the belt item sprites, the spell icon's artwork is
	// not centred within its own sprite bounds, so geometric centring still reads slightly high and
	// left. Nudged by eye against the art.
	constexpr Displacement RmbIconNudge { 1, 3 };
	// Draw(Small)SpellIcon anchors at the sprite's bottom-left.
	const Point position = rmbWell.position + RmbIconNudge
	    + Displacement { (rmbWell.size.width - iconSize.width) / 2,
		      (rmbWell.size.height - iconSize.height) / 2 + iconSize.height - 1 };

	// Oracool: while Furious Charge is active, this slot renders with a borrowed icon (see
	// FuriousChargeIcon - there's no dedicated art for a mod-only skill) instead of the normal
	// single-color Item Repair icon. User request - the cooldown fill grows in red (rather than
	// the ready color) bottom-up as it cools, then flips to the normal ready tint the instant it's fully
	// cooled, instead of gradually blending from gray to color.
	if (oracool::IsFuriousChargeSpell(spl)) {
		const float progress = oracool::GetFuriousChargeCooldownProgress();
		if (progress >= 1.0f) {
			SetSpellTrans(st);
			DrawSmallSpellIcon(out, position, oracool::FuriousChargeIcon);
		} else {
			const int partition = static_cast<int>(SmallIconHeight * progress);
			if (partition > 0) {
				const Surface filledBand = out.subregionY(position.y - partition, partition);
				SetSpellTransRed();
				DrawSmallSpellIcon(filledBand, { position.x, partition }, oracool::FuriousChargeIcon);
			}
			if (partition < SmallIconHeight) {
				const Surface unfilledBand = out.subregionY(position.y - SmallIconHeight, SmallIconHeight - partition);
				SetSpellTrans(SpellType::Invalid);
				DrawSmallSpellIcon(unfilledBand, { position.x, SmallIconHeight }, oracool::FuriousChargeIcon);
			}
		}
	} else {
		SetSpellTrans(st);
		DrawSmallSpellIcon(out, position, spl);
	}

	std::optional<string_view> hotkeyName = GetHotkeyName(spl, myPlayer._pRSplType, true);
	if (hotkeyName) {
		// PrintSBookHotkey aligns against the 56px large icon; this slot draws the small one, so
		// align to its top-right corner directly.
		const Point hotkeyPosition = position + Displacement { iconSize.width - (GetLineWidth(hotkeyName->data()) + 4), 5 - SmallIconHeight };
		DrawString(out, *hotkeyName, hotkeyPosition, { UiFlags::ColorWhite | UiFlags::Outlined });
	}
}

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

		SetSpellTrans(transType);
		// Oracool: user request - the SpeedBook list must show the same borrowed icon Furious
		// Charge uses everywhere else, not the vanilla Item Repair icon, for the Warrior's slot.
		DrawLargeSpellIcon(out, spellListItem.location, oracool::IsFuriousChargeSpell(spellId) ? oracool::FuriousChargeIcon : spellId);

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

	uint64_t mask;
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
		int8_t j = static_cast<int8_t>(SpellID::Firebolt);
		for (uint64_t spl = 1; j < MAX_SPELLS; spl <<= 1, j++) {
			if ((mask & spl) == 0)
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
	myPlayer._pRSpell = pSpell;
	myPlayer._pRSplType = pSplType;

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
	uint64_t spells;

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
		myPlayer._pRSpell = spellId;
		myPlayer._pRSplType = myPlayer._pSplTHotKey[slot];
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
