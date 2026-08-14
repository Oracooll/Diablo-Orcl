#include "panels/spell_book.hpp"

#include <cstdint>

#include <algorithm>

#include <fmt/format.h>

#include "control.h"
#include "engine/backbuffer_state.hpp"
#include "engine/clx_sprite.hpp"
#include "engine/rectangle.hpp"
#include "engine/render/clx_render.hpp"
#include "engine/render/primitive_render.hpp"
#include "engine/render/text_render.hpp"
#include "init.h"
#include "inv.h" // CloseInventory
#include "missiles.h"
#include "oracool/attack_skills.h"
#include "oracool/auras.h"
#include "oracool/barb_skills.h"
#include "oracool/furious_charge.h"
#include "oracool/hud_art.h"
#include "oracool/oracool.h"
#include "oracool/ornate_border.h"
#include "panels/spell_icons.hpp"
#include "panels/ui_panels.hpp"
#include "player.h"
#include "spelldat.h"
#include "spells.h" // ClearReadiedSpell
#include "utils/display.h"
#include "utils/language.h"
#include "utils/stdcompat/optional.hpp"

namespace devilution {

namespace {

const size_t SpellBookPages = 6;
const size_t SpellBookPageEntries = 7;

/** Maps from spellbook page number and position to SpellID. */
const SpellID SpellPages[SpellBookPages][SpellBookPageEntries] = {
	{ SpellID::Null, SpellID::Firebolt, SpellID::ChargedBolt, SpellID::HolyBolt, SpellID::Healing, SpellID::HealOther, SpellID::Inferno },
	{ SpellID::Resurrect, SpellID::FireWall, SpellID::Telekinesis, SpellID::Lightning, SpellID::TownPortal, SpellID::Flash, SpellID::StoneCurse },
	{ SpellID::Phasing, SpellID::ManaShield, SpellID::Elemental, SpellID::Fireball, SpellID::FlameWave, SpellID::ChainLightning, SpellID::Guardian },
	{ SpellID::Nova, SpellID::Golem, SpellID::Teleport, SpellID::Apocalypse, SpellID::BoneSpirit, SpellID::BloodStar, SpellID::Etherealize },
	{ SpellID::LightningWall, SpellID::Immolation, SpellID::Warp, SpellID::Reflect, SpellID::Berserk, SpellID::RingOfFire, SpellID::Search },
	{ SpellID::Invalid, SpellID::Invalid, SpellID::Invalid, SpellID::Invalid, SpellID::Invalid, SpellID::Invalid, SpellID::Invalid }
};

/**
 * @brief The class's innate skill - Item Repair, Trap Disarm, Staff Recharge and so on.
 *
 * It used to sit in page 0 slot 0 of the table above, which made it look like a spell. It has its
 * own sheet now (user request), so it is resolved here rather than being patched into the spell
 * grid on the way past.
 */
SpellID GetClassSkill()
{
	switch (InspectPlayer->_pClass) {
	case HeroClass::Warrior:
		return SpellID::ItemRepair;
	case HeroClass::Rogue:
		return SpellID::TrapDisarm;
	case HeroClass::Sorcerer:
		return SpellID::StaffRecharge;
	case HeroClass::Monk:
		return SpellID::Search;
	case HeroClass::Bard:
		return SpellID::Identify;
	case HeroClass::Barbarian:
		return SpellID::Rage;
	}
	return SpellID::Invalid;
}

// Oracool V1: the book is one scrolling list per sheet, not six paged grids. The pages above survive
// as the SOURCE of the Spells list rather than as a layout - they are the game's own curated set of
// book spells, so flattening them lists exactly what the tabs used to reach, in the order they used
// to appear. Enumerating SpellID directly would instead sweep up the unimplemented entries
// (DoomSerpents, BloodRitual, Invisibility, Mana, Magi, Jester) and the Hellfire rune items, none of
// which the book has ever shown.

/** @brief Upper bound on spell rows: every page entry, before any is filtered out. */
constexpr size_t MaxSpellRows = SpellBookPages * SpellBookPageEntries;

/**
 * @brief Fills @p rows with every spell the Spells sheet lists, in page order. Returns how many.
 *
 * Page 4 is Hellfire's, and page 5 is entirely Invalid padding, so the loop stops where the tab row
 * used to: four pages in Diablo, five in Hellfire. Slot 0 of page 0 is skipped - that was the class
 * skill, which lives on the Skills sheet now.
 */
size_t BuildSpellRows(SpellID *rows)
{
	const size_t pages = gbIsHellfire ? 5 : 4;
	size_t count = 0;
	for (size_t page = 0; page < pages; page++) {
		for (size_t entry = 0; entry < SpellBookPageEntries; entry++) {
			const SpellID sn = SpellPages[page][entry];
			if (!IsValidSpell(sn))
				continue;
			// Town Portal is a built-in ability here, never a book spell - see
			// oracool::IsBuiltInPortalAbility. This is now the ONE place the window drops it:
			// the Skills sheet listed it too until the user asked for it gone, so nothing else
			// in this file needs to know it exists.
			if (oracool::IsBuiltInPortalAbility(sn))
				continue;
			rows[count++] = sn;
		}
	}

	// Oracool: user request - "arrange Spells by LVL req + alphabetically". The page order the
	// table above encodes is the original book's layout, which is neither; sorting it here keeps
	// the table as the authoritative SET of book spells while the list decides its own order.
	//
	// Requirement first, name second, so the list reads as a progression and ties stay predictable
	// rather than falling back on the table's incidental order. Names are compared translated,
	// because "alphabetical" means alphabetical in the language on screen.
	//
	// `minInt` is the Magic a spell demands, and it is the only real requirement Diablo puts on a
	// spell - there is no character-level gate. (sBookLvl exists but is about which dungeon level
	// drops the BOOK, which is loot availability rather than a requirement.)
	std::sort(rows, rows + count, [](SpellID a, SpellID b) {
		const int reqA = GetSpellData(a).minInt;
		const int reqB = GetSpellData(b).minInt;
		if (reqA != reqB)
			return reqA < reqB;
		return oracool::GetSpellDisplayName(a) < oracool::GetSpellDisplayName(b);
	});
	return count;
}

/**
 * @brief Fills @p rows with the SPELL-backed skills the Skills sheet lists. Returns how many.
 *
 * The two basic attacks are listed ahead of these and are not in here, because they are not spells -
 * see AttackRowCount below and oracool/attack_skills.h.
 *
 * The built-in Town Portal is deliberately NOT here either (user request). It was listed for a while
 * on the reasoning that it is a skill the character has - but it is a fixed HUD button, always
 * available and never chosen, so a row for it can say nothing the button does not already say and
 * cannot be clicked to any effect. Everything the window does with a row - ready it, price it in
 * mana, grey it when unlearned - is meaningless for it.
 */
size_t BuildSkillRows(SpellID *rows)
{
	size_t count = 0;
	const SpellID skill = GetClassSkill();
	if (IsValidSpell(skill))
		rows[count++] = skill;
	return count;
}

/**
 * @brief Rows the Skills sheet puts ahead of the spell-backed ones: Regular Attack and Fist Attack.
 *
 * First, because they are what the character does when nothing else is chosen - the floor the rest
 * of the sheet sits on.
 */
constexpr size_t AttackRowCount = oracool::AttackIconCount;

// Oracool V1: the shared theme and geometry - the same 340x720 window, title band and separator as
// the waypoint list, quest log, character sheet and inventory.
//   0..24     top margin
//   24..74    label band, the sheet's name, with an arrow at each end
//   74..77    separator rule
//   77..101   gap below the rule
//   101..696  content area - the scrolling list
//   696..720  bottom margin
constexpr Size AbilitiesPanelSize { 340, 720 };
constexpr int AbilitiesMargin = 24;
constexpr int AbilitiesLabelHeight = 50;
constexpr int AbilitiesContentTop = AbilitiesMargin + AbilitiesLabelHeight + oracool::OrnateBorderWidth + AbilitiesMargin;
constexpr Size AbilitiesContentSize { AbilitiesPanelSize.width,
	AbilitiesPanelSize.height - AbilitiesContentTop - AbilitiesMargin };

/** @brief A spell or skill row: the small icon with a little air above and below it. */
constexpr int SpellRowHeight = 44;
/**
 * @brief How many wrapped lines a described row gives its description.
 *
 * THREE, not two. The descriptions were written to about 75 characters on the assumption that the
 * text column took ~38 per line; in game it takes closer to 25, so every longer one wrapped to three
 * lines and lost the third - "Reduces the duration and damage of poison and other" simply stopped
 * there. Measured from the rendering rather than re-estimated, and the row grew to match instead of
 * the descriptions being cut down, because the third line is where several of them say what the
 * ability actually does.
 */
constexpr int DescribedRowDescLines = 3;
/** @brief Height of one text line inside a row. */
constexpr int AbilitiesLineHeight = 18;
/** @brief Air above the name line, and the same below the last description line. */
constexpr int DescribedRowPadding = 6;
/** @brief A described row (aura, Barbarian skill): a name line plus the wrapped description. */
constexpr int DescribedRowHeight = 2 * DescribedRowPadding
    + AbilitiesLineHeight * (1 + DescribedRowDescLines);
constexpr int AbilitiesIconX = AbilitiesMargin;
/** @brief Between the icon's right edge and the text column. */
constexpr int AbilitiesTextGap = 10;
constexpr int AbilitiesScrollbarWidth = oracool::OrnateBorderWidth;
constexpr int AbilitiesScrollbarMinThumb = 24;
constexpr int AbilitiesScrollbarGap = 6;
constexpr int AbilitiesRightPad = 8;
/** @brief Right edge available to a row - short of the scrollbar, not of the panel. */
constexpr int AbilitiesContentRightLimit = AbilitiesPanelSize.width - AbilitiesRightPad - AbilitiesScrollbarWidth - AbilitiesScrollbarGap;

/**
 * @brief The sheet-cycling arrows, at each end of the title band.
 *
 * Solid triangles drawn from primitives rather than art: they are two shapes on a flat background,
 * and an asset for them would be another file to cut, ship, pack and keep in step with the theme's
 * gold for no gain.
 */
constexpr int ArrowWidth = 11;
constexpr int ArrowHeight = 16;
/** @brief The arrow's clickable box - generous around a small glyph, as a target should be. */
constexpr Size ArrowHitSize { 28, 28 };

enum class AbilitySheet : uint8_t {
	Spells,
	Skills,
	Auras,
	Barbarian,
	LAST = Barbarian,
};
constexpr size_t AbilitySheetCount = 4;

AbilitySheet CurrentSheet = AbilitySheet::Spells;
/** @brief Scroll offset per sheet, so switching sheets does not lose your place in the others. */
int ScrollOffset[AbilitySheetCount] = {};
int MaxScrollOffset = 0;
int ListHeight = 0;
/** @brief Set while an arrow is held, purely so it can be drawn pressed. */
int PressedArrow = 0;

/** @brief Whether @p sheet is available to this character. */
bool IsSheetAvailable(AbilitySheet sheet)
{
	switch (sheet) {
	case AbilitySheet::Spells:
		// Oracool: available to every class again (user request 2026-08-13), reversing the earlier
		// "reserve spells ability sheet for sorcerer only". Recorded rather than deleted because the
		// engine agrees with the reversal: book spells are not class-gated at all - a spell is
		// memorised by reading its book, and any class that finds one can. Reserving the sheet hid a
		// list of things a Paladin can genuinely learn.
		break;
	case AbilitySheet::Auras:
		return oracool::ClassHasAuras(*InspectPlayer);
	case AbilitySheet::Barbarian:
		return oracool::ClassHasBarbSkills(*InspectPlayer);
	case AbilitySheet::Skills:
		// Always: every class has an innate skill, so this sheet is never empty and is the safe
		// landing place when nothing else is available.
		break;
	}
	return true;
}

size_t AvailableSheetCount()
{
	size_t count = 0;
	for (size_t i = 0; i < AbilitySheetCount; i++) {
		if (IsSheetAvailable(static_cast<AbilitySheet>(i)))
			count++;
	}
	return count;
}

/**
 * @brief The sheet to fall back to when the current one is not available to this class.
 *
 * Always Spells today, since Spells and Skills are both universal - so this looks like a loop that
 * could be a constant. It was a hardcoded Spells once, and that was fine right up until Spells was
 * reserved to the Sorcerer, at which point a Paladin was sent to a sheet it did not have by the one
 * guard whose whole job was to prevent that. Spells is universal again now, which is exactly why
 * this stays a search: availability has already changed twice.
 */
AbilitySheet FirstAvailableSheet()
{
	for (size_t i = 0; i < AbilitySheetCount; i++) {
		const auto sheet = static_cast<AbilitySheet>(i);
		if (IsSheetAvailable(sheet))
			return sheet;
	}
	return AbilitySheet::Skills;
}

string_view GetSheetTitle(AbilitySheet sheet)
{
	switch (sheet) {
	case AbilitySheet::Spells:
		return _("SPELLS");
	case AbilitySheet::Skills:
		return _("SKILLS");
	case AbilitySheet::Auras:
		return _("AURAS");
	case AbilitySheet::Barbarian:
		return _("BARBARIAN");
	}
	return {};
}

int RowHeightFor(AbilitySheet sheet)
{
	return (sheet == AbilitySheet::Auras || sheet == AbilitySheet::Barbarian) ? DescribedRowHeight : SpellRowHeight;
}

/** @brief How many rows @p sheet has right now. */
size_t GetRowCount(AbilitySheet sheet)
{
	SpellID rows[MaxSpellRows];
	switch (sheet) {
	case AbilitySheet::Spells:
		return BuildSpellRows(rows);
	case AbilitySheet::Skills:
		return AttackRowCount + BuildSkillRows(rows);
	case AbilitySheet::Auras:
		return oracool::AuraCount;
	case AbilitySheet::Barbarian:
		return oracool::BarbSkillCount;
	}
	return 0;
}

/** @brief Recomputes the scroll extent for the current sheet and re-clamps its offset. */
void UpdateScrollBounds()
{
	ListHeight = static_cast<int>(GetRowCount(CurrentSheet)) * RowHeightFor(CurrentSheet);
	MaxScrollOffset = std::max(0, ListHeight - AbilitiesContentSize.height);
	int &offset = ScrollOffset[static_cast<size_t>(CurrentSheet)];
	offset = std::clamp(offset, 0, MaxScrollOffset);
}

int CurrentScroll()
{
	return ScrollOffset[static_cast<size_t>(CurrentSheet)];
}

SpellType GetSBookTrans(SpellID ii, bool townok)
{
	Player &player = *InspectPlayer;
	if ((player._pClass == HeroClass::Monk) && (ii == SpellID::Search))
		return SpellType::Skill;
	SpellType st = SpellType::Spell;
	if ((player._pISpells & GetSpellBitmask(ii)) != 0) {
		st = SpellType::Charges;
	}
	if ((player._pAblSpells & GetSpellBitmask(ii)) != 0) {
		st = SpellType::Skill;
	}
	if (st == SpellType::Spell) {
		if (CheckSpell(*InspectPlayer, ii, st, true) != SpellCheckResult::Success) {
			st = SpellType::Invalid;
		}
		if (player.GetSpellLevel(ii) == 0) {
			st = SpellType::Invalid;
		}
	}
	if (townok && leveltype == DTYPE_TOWN && st != SpellType::Invalid && !GetSpellData(ii).isAllowedInTown()) {
		st = SpellType::Invalid;
	}

	return st;
}

/** @brief Whether the player has this spell at all - by memory, item or innate ability. */
bool IsSpellKnown(SpellID sn)
{
	const Player &player = *InspectPlayer;
	const uint64_t known = player._pMemSpells | player._pISpells | player._pAblSpells;
	return (known & GetSpellBitmask(sn)) != 0;
}

/** @brief The second line of a spell/skill row: what it costs, does, or that it is not yet known. */
std::string GetSpellDetail(SpellID sn, bool known)
{
	if (!known)
		return std::string(_(/* TRANSLATORS: UI constraints, keep short please.*/ "Not learned"));

	Player &player = *InspectPlayer;
	switch (GetSBookTrans(sn, false)) {
	case SpellType::Skill:
		return std::string(_("Skill"));
	case SpellType::Charges: {
		const int charges = player.InvBody[INVLOC_HAND_LEFT]._iCharges;
		return fmt::format(fmt::runtime(ngettext("Staff ({:d} charge)", "Staff ({:d} charges)", charges)), charges);
	}
	default:
		break;
	}

	const int lvl = player.GetSpellLevel(sn);
	if (lvl == 0)
		return std::string(_("Unusable"));

	const int mana = GetManaAmount(player, sn) >> 6;
	if (sn == SpellID::BoneSpirit)
		return fmt::format(fmt::runtime(pgettext("spellbook", "Mana: {:d}")), mana) + "   "
		    + std::string(_(/* TRANSLATORS: UI constraints, keep short please.*/ "Dmg: 1/3 target hp"));

	int min;
	int max;
	GetDamageAmt(sn, &min, &max);
	std::string cost = fmt::format(fmt::runtime(pgettext("spellbook", "Mana: {:d}")), mana);
	if (min == -1)
		return cost;
	if (sn == SpellID::Healing || sn == SpellID::HealOther)
		return cost + "   " + fmt::format(fmt::runtime(_("Heals: {:d} - {:d}")), min, max);
	return cost + "   " + fmt::format(fmt::runtime(_("Damage: {:d} - {:d}")), min, max);
}

/** @brief The theme's scrollbar: a recessed groove in the right margin with a bevelled thumb. */
void DrawScrollbar(const Surface &out, const Rectangle &panel)
{
	if (MaxScrollOffset <= 0)
		return;

	const int x = panel.position.x + AbilitiesPanelSize.width - AbilitiesRightPad - AbilitiesScrollbarWidth;
	const int top = panel.position.y + AbilitiesContentTop;
	oracool::DrawThemedFill(out, { { x, top }, { AbilitiesScrollbarWidth, AbilitiesContentSize.height } }, 2);

	const int thumbHeight = std::max(AbilitiesScrollbarMinThumb,
	    AbilitiesContentSize.height * AbilitiesContentSize.height / ListHeight);
	const int travel = AbilitiesContentSize.height - thumbHeight;
	const int thumbY = top + travel * CurrentScroll() / MaxScrollOffset;
	oracool::DrawOrnateSeparatorVertical(out, { x, thumbY }, thumbHeight);
}

/** @brief Screen rect of the sheet-cycling arrow. @p direction is -1 for left, +1 for right. */
Rectangle GetArrowRect(int direction)
{
	const Rectangle panel = GetSpellBookPanelRect();
	const int cx = direction < 0
	    ? panel.position.x + AbilitiesMargin + ArrowHitSize.width / 2
	    : panel.position.x + AbilitiesPanelSize.width - AbilitiesMargin - ArrowHitSize.width / 2;
	const int cy = panel.position.y + AbilitiesMargin + AbilitiesLabelHeight / 2;
	return { { cx - ArrowHitSize.width / 2, cy - ArrowHitSize.height / 2 }, ArrowHitSize };
}

/** @brief Draws one solid triangle, pointing left (@p direction -1) or right (+1). */
void DrawArrow(const Surface &out, int direction)
{
	const Rectangle hit = GetArrowRect(direction);
	const Point centre { hit.position.x + hit.size.width / 2, hit.position.y + hit.size.height / 2 };
	const uint8_t color = PressedArrow == direction ? PAL16_YELLOW + 2 : oracool::ThemeEdgeColor;

	// Filled by drawing one horizontal run per row, the run growing toward the base. Row r counts
	// out from the tip, so the widths are symmetric about the vertical centre.
	for (int r = 0; r < ArrowWidth; r++) {
		const int halfHeight = (ArrowHeight / 2) * (r + 1) / ArrowWidth;
		const int x = direction < 0 ? centre.x - ArrowWidth / 2 + r : centre.x + ArrowWidth / 2 - r;
		const int y0 = centre.y - halfHeight;
		DrawVerticalLine(out, { x, y0 }, halfHeight * 2 + 1, color);
	}
}

/**
 * @brief Width of the icon column every row of a list shares.
 *
 * The Skills sheet mixes two icon sources - the engine's 37px small spell icon and the 38px attack
 * strip - and a row taking its text offset from its own icon would start its text one pixel off the
 * row above it. Both read this instead, so the column is a property of the LIST rather than of
 * whichever icon a row happens to hold.
 */
int RowIconColumnWidth()
{
	return std::max(GetSmallSpellIconSize().width, oracool::GetAttackIconSize().width);
}

int RowTextX()
{
	return AbilitiesIconX + RowIconColumnWidth() + AbilitiesTextGap;
}

/** @brief One row of the Skills sheet's two basic attacks. @p index is oracool::AttackIcon order. */
void DrawAttackRow(const Surface &content, size_t index, int top)
{
	const auto icon = static_cast<oracool::AttackIcon>(index);
	// Exactly one of the two is what the hand is currently doing, and the other is blended - the
	// same treatment a locked aura gets, used here as an indicator rather than as an availability
	// state. It is what makes the pair read as one status line instead of two abilities.
	const bool active = oracool::BasicAttackIcon(*InspectPlayer) == icon;

	const Size iconSize = oracool::GetAttackIconSize();
	const Point iconPos { AbilitiesIconX, top + (SpellRowHeight - iconSize.height) / 2 };
	oracool::DrawAttackIcon(content, iconPos, static_cast<int>(index), active);

	const int textX = RowTextX();
	const int textWidth = AbilitiesContentRightLimit - textX;
	const UiFlags nameColor = active ? UiFlags::ColorWhitegold : UiFlags::ColorUiSilverDark;
	const UiFlags detailColor = active ? UiFlags::ColorWhite : UiFlags::ColorUiSilverDark;
	const int textTop = top + (SpellRowHeight - 2 * AbilitiesLineHeight) / 2;
	DrawString(content, _(oracool::AttackIconName(icon)),
	    { { textX, textTop }, { textWidth, AbilitiesLineHeight } },
	    { nameColor | UiFlags::VerticalCenter });
	DrawString(content, _(oracool::AttackIconDetail(icon, active)),
	    { { textX, textTop + AbilitiesLineHeight }, { textWidth, AbilitiesLineHeight } },
	    { detailColor | UiFlags::VerticalCenter });
}

void DrawSpellRow(const Surface &content, size_t index, SpellID sn, int top)
{
	Player &player = *InspectPlayer;
	const Size iconSize = GetSmallSpellIconSize();
	const int textX = RowTextX();
	const int textWidth = AbilitiesContentRightLimit - textX;
	const bool known = IsSpellKnown(sn);

	// The whole "unlearned" treatment is this one line: SpellType::Invalid's translation table maps
	// the icon onto PAL16_GRAY, which is exactly the desaturated, inactive reading wanted - no
	// second set of art, and it is the same grey the game already uses for a spell you cannot cast.
	SetSpellTrans(known ? GetSBookTrans(sn, true) : SpellType::Invalid);

	// DrawSmallSpellIcon takes a BOTTOM-left origin, so this centres the icon in the row.
	const Point iconPos { AbilitiesIconX, top + (SpellRowHeight + iconSize.height) / 2 };
	// Oracool: user request - the book must show the same borrowed icon Furious Charge uses
	// everywhere else, not the vanilla Item Repair icon.
	DrawSmallSpellIcon(content, iconPos, oracool::IsFuriousChargeSpell(sn) ? oracool::FuriousChargeIcon : sn);
	if (known && sn == player._pRSpell && GetSBookTrans(sn, true) == player._pRSplType && !IsInspectingPlayer()) {
		SetSpellTrans(SpellType::Skill);
		DrawSmallSpellIconBorder(content, iconPos);
	}

	const UiFlags nameColor = known ? UiFlags::ColorWhitegold : UiFlags::ColorUiSilverDark;
	const UiFlags detailColor = known ? UiFlags::ColorWhite : UiFlags::ColorUiSilverDark;
	const int textTop = top + (SpellRowHeight - 2 * AbilitiesLineHeight) / 2;
	DrawString(content, oracool::GetSpellDisplayName(sn),
	    { { textX, textTop }, { textWidth, AbilitiesLineHeight } },
	    { nameColor | UiFlags::VerticalCenter });
	DrawString(content, GetSpellDetail(sn, known),
	    { { textX, textTop + AbilitiesLineHeight }, { textWidth, AbilitiesLineHeight } },
	    { detailColor | UiFlags::VerticalCenter });
}

/**
 * @brief Draws an icon-plus-prose row - the shape both the Auras and Barbarian sheets use.
 *
 * @p tag is an optional short right-aligned label on the name line (the Barbarian's Combat/Warcry/
 * Passive/Utility); empty for the auras, which have no such distinction.
 * @p requirement replaces the description when the entry is locked.
 */
void DrawDescribedRow(const Surface &content, int top, int iconIndex, bool isAura, bool unlocked,
    string_view name, string_view tag, string_view description, int requiredLevel)
{
	const Size iconSize = isAura ? oracool::GetAuraIconSize() : oracool::GetBarbSkillIconSize();
	const int iconWidth = iconSize.width > 0 ? iconSize.width : 38;
	const int textX = AbilitiesIconX + iconWidth + AbilitiesTextGap;
	const int textWidth = AbilitiesContentRightLimit - textX;

	// Top-left origin here, unlike the spell icons' bottom-left - these are blitted rather than
	// drawn as CLX sprites.
	const Point iconPos { AbilitiesIconX, top + (DescribedRowHeight - iconSize.height) / 2 };
	if (isAura)
		oracool::DrawAuraIcon(content, iconPos, iconIndex, unlocked);
	else
		oracool::DrawBarbSkillIcon(content, iconPos, iconIndex, unlocked);

	const UiFlags nameColor = unlocked ? UiFlags::ColorWhitegold : UiFlags::ColorUiSilverDark;
	const UiFlags detailColor = unlocked ? UiFlags::ColorWhite : UiFlags::ColorUiSilverDark;

	const int textTop = top + DescribedRowPadding;
	const Rectangle nameArea { { textX, textTop }, { textWidth, AbilitiesLineHeight } };
	DrawString(content, name, nameArea, { nameColor | UiFlags::VerticalCenter });
	if (!tag.empty()) {
		// Right-aligned in the same rect as the name. "Is this something I press, or is it always
		// on?" is not answerable from the description alone, and it is the first thing to know.
		DrawString(content, tag, nameArea,
		    { UiFlags::ColorUiSilverDark | UiFlags::AlignRight | UiFlags::VerticalCenter });
	}

	// A locked entry says what would unlock it rather than what it does - the requirement is the
	// useful information at that point, and it keeps the row height identical either way.
	if (!unlocked) {
		DrawString(content, fmt::format(fmt::runtime(_("Requires level {:d}")), requiredLevel),
		    { { textX, textTop + AbilitiesLineHeight }, { textWidth, AbilitiesLineHeight } },
		    { detailColor | UiFlags::VerticalCenter });
		return;
	}

	// Wrapped to the text column rather than trusted to fit: the descriptions are written short
	// enough for two lines in English, and a longer translation wraps instead of running under the
	// scrollbar.
	DrawString(content, WordWrapString(description, textWidth, GameFont12, 1),
	    { { textX, textTop + AbilitiesLineHeight }, { textWidth, DescribedRowDescLines * AbilitiesLineHeight } },
	    { detailColor, 1, AbilitiesLineHeight });
}

void DrawAuraRow(const Surface &content, size_t index, int top)
{
	// Through the display-order lookup, not a cast: the enum follows the icon sheet, the list is
	// ordered by unlock level. See GetAuraAtDisplayIndex.
	const oracool::Aura aura = oracool::GetAuraAtDisplayIndex(index);
	const oracool::AuraData &data = oracool::GetAuraData(aura);
	DrawDescribedRow(content, top, oracool::GetAuraIconIndex(aura), /*isAura=*/true,
	    oracool::IsAuraUnlocked(*InspectPlayer, aura), _(data.name),
	    oracool::GetAuraTierName(data.tier), _(data.description),
	    oracool::GetAuraTierMinLevel(data.tier));
}

void DrawBarbSkillRow(const Surface &content, size_t index, int top)
{
	const oracool::BarbSkill skill = oracool::GetBarbSkillAtDisplayIndex(index);
	const oracool::BarbSkillData &data = oracool::GetBarbSkillData(skill);
	DrawDescribedRow(content, top, oracool::GetBarbSkillIconIndex(skill), /*isAura=*/false,
	    oracool::IsBarbSkillUnlocked(*InspectPlayer, skill), _(data.name),
	    oracool::GetBarbSkillKindName(data.kind), _(data.description), data.minLevel);
}

} // namespace

Rectangle GetSpellBookPanelRect()
{
	// Flush to the top-right corner, mirroring the inventory - the two share the right-hand slot
	// and are never open at once, so they should occupy exactly the same space.
	return { { gnScreenWidth - AbilitiesPanelSize.width, 0 }, AbilitiesPanelSize };
}

Rectangle GetSpellBookContentRect()
{
	const Rectangle panel = GetSpellBookPanelRect();
	return { { panel.position.x, panel.position.y + AbilitiesContentTop }, AbilitiesContentSize };
}

void CycleAbilitySheet(int direction)
{
	// Skips any sheet this class does not have, so a non-Paladin never lands on an empty Auras
	// page. Bounded by the sheet count so a class with only one sheet cannot spin forever.
	for (size_t attempts = 0; attempts < AbilitySheetCount; attempts++) {
		int next = static_cast<int>(CurrentSheet) + direction;
		if (next < 0)
			next = static_cast<int>(AbilitySheetCount) - 1;
		if (next >= static_cast<int>(AbilitySheetCount))
			next = 0;
		CurrentSheet = static_cast<AbilitySheet>(next);
		if (IsSheetAvailable(CurrentSheet))
			break;
	}
	UpdateScrollBounds();
}

void ScrollSpellBook(int notches)
{
	UpdateScrollBounds();
	int &offset = ScrollOffset[static_cast<size_t>(CurrentSheet)];
	offset = std::clamp(offset + notches * RowHeightFor(CurrentSheet) * 3, 0, MaxScrollOffset);
}

void ResetSpellBookScroll()
{
	for (int &offset : ScrollOffset)
		offset = 0;
	// Opening on a sheet the character cannot use would show an empty window.
	if (!IsSheetAvailable(CurrentSheet))
		CurrentSheet = FirstAvailableSheet();
}

void ToggleAbilitiesWindow()
{
	// The single funnel for opening the window, shared by the burger menu and the HUD's two skill
	// buttons. The other two toggle sites (the keybind, and the gamepad action) each carry extra
	// control-scheme bookkeeping of their own and stay where they are - but they call
	// ResetSpellBookScroll for the same reason this does.
	CloseInventory();
	if (DropGoldFlag)
		CloseGoldDrop();
	// The speedbook overlay and this window both answer "which spell is readied"; showing them at
	// once would be two competing answers on screen.
	spselflag = false;
	sbookflag = !sbookflag;
	ResetSpellBookScroll();
}

void InitSpellBook()
{
	// Oracool V1: data\spellbk (the parchment page) and data\spellbkb (the tab buttons) are no
	// longer loaded. The window is the shared procedural theme now, and the six tabs are gone -
	// each sheet is one scrolling list, cycled with the arrows in the title band.
	LoadSmallSpellIcons();
}

void FreeSpellBook()
{
	FreeSmallSpellIcons();
}

void DrawSpellBook(const Surface &out)
{
	if (!IsSheetAvailable(CurrentSheet))
		CurrentSheet = FirstAvailableSheet();

	const Rectangle panel = GetSpellBookPanelRect();
	oracool::DrawThemedFill(out, panel);
	oracool::DrawOrnateBorder(out, panel);

	const Rectangle labelArea { { panel.position.x + AbilitiesMargin, panel.position.y + AbilitiesMargin },
		{ panel.size.width - 2 * AbilitiesMargin, AbilitiesLabelHeight } };
	oracool::DrawOutlinedString(out, GetSheetTitle(CurrentSheet), labelArea,
	    UiFlags::ColorWhitegold | UiFlags::FontSize30 | UiFlags::AlignCenter | UiFlags::VerticalCenter);
	// Only when there is somewhere to go - arrows on a window that cannot turn are a control that
	// lies. Every class has at least Spells and Skills today so this is always true, but it was not
	// while Spells was the Sorcerer's alone (a Rogue was down to Skills on its own), and the guard
	// costs nothing.
	if (AvailableSheetCount() > 1) {
		DrawArrow(out, -1);
		DrawArrow(out, 1);
	}
	oracool::DrawOrnateSeparator(out,
	    { panel.position.x + AbilitiesMargin, panel.position.y + AbilitiesMargin + AbilitiesLabelHeight },
	    panel.size.width - 2 * AbilitiesMargin);

	UpdateScrollBounds();
	DrawScrollbar(out, panel);

	// Rows draw through a subregion covering only the scrolling area, so a row straddling its top
	// or bottom edge is clipped there rather than spilling onto the title band or the bottom bevel.
	const Rectangle contentRect = GetSpellBookContentRect();
	const Surface content = out.subregion(contentRect.position.x, contentRect.position.y,
	    contentRect.size.width, contentRect.size.height);

	const int rowHeight = RowHeightFor(CurrentSheet);
	const int scroll = CurrentScroll();

	SpellID rows[MaxSpellRows];
	size_t rowCount = 0;
	// Rows the Skills sheet draws from the attack strip instead of from `rows`, because they are not
	// spells. Zero on every other sheet, which makes the index shift below a no-op there.
	const size_t attackRows = CurrentSheet == AbilitySheet::Skills ? AttackRowCount : 0;
	if (CurrentSheet == AbilitySheet::Spells)
		rowCount = BuildSpellRows(rows);
	else if (CurrentSheet == AbilitySheet::Skills)
		rowCount = attackRows + BuildSkillRows(rows);
	else
		rowCount = GetRowCount(CurrentSheet);

	for (size_t i = 0; i < rowCount; i++) {
		const int top = static_cast<int>(i) * rowHeight - scroll;
		if (top + rowHeight <= 0 || top >= AbilitiesContentSize.height)
			continue;
		switch (CurrentSheet) {
		case AbilitySheet::Auras:
			DrawAuraRow(content, i, top);
			break;
		case AbilitySheet::Barbarian:
			DrawBarbSkillRow(content, i, top);
			break;
		case AbilitySheet::Spells:
		case AbilitySheet::Skills:
			if (i < attackRows)
				DrawAttackRow(content, i, top);
			else
				DrawSpellRow(content, i, rows[i - attackRows], top);
			break;
		}
	}
}

void CheckSBook()
{
	// The arrows first, and outside the inspect guard - cycling sheets is reading, not acting, so
	// it stays available while inspecting another player's abilities. Hit-tested only when they are
	// actually drawn, so a class with one sheet has no invisible control to hit.
	if (AvailableSheetCount() > 1) {
		for (const int direction : { -1, 1 }) {
			if (GetArrowRect(direction).contains(MousePosition)) {
				CycleAbilitySheet(direction);
				PressedArrow = direction;
				RedrawEverything();
				return;
			}
		}
	}

	if (IsInspectingPlayer())
		return;

	const Rectangle content = GetSpellBookContentRect();
	// Oracool: user request - a row's clickable span stops at AbilitiesContentRightLimit, short of
	// the scrollbar, rather than running the full panel width. Dragging or clicking the scrollbar
	// would otherwise also select whatever row happened to be under it.
	const Rectangle rowClickArea { content.position, { AbilitiesContentRightLimit, content.size.height } };
	if (!rowClickArea.contains(MousePosition))
		return;

	UpdateScrollBounds();
	const int rowHeight = RowHeightFor(CurrentSheet);
	const int y = MousePosition.y - content.position.y + CurrentScroll();
	const size_t index = static_cast<size_t>(y / rowHeight);

	// Auras and Barbarian skills are listed and described but not yet selectable - the gameplay
	// passes that give them effects have not been built. Clicking one deliberately does nothing
	// rather than setting a state nothing reads. See the vault's aura implementation plan.
	if (CurrentSheet == AbilitySheet::Auras || CurrentSheet == AbilitySheet::Barbarian)
		return;

	if (CurrentSheet == AbilitySheet::Skills && index < AttackRowCount) {
		// Regular Attack readies the basic attack, which in this engine means clearing the readied
		// spell - that IS the state in which a click swings the weapon. It is worth having as a row
		// because there was previously no way back to it once a spell was readied, short of the
		// undiscoverable shift-click on the RMB well.
		//
		// Fist Attack is inert on purpose (user request): it is the same underlying state, and which
		// of the two icons you get is decided by what is in your hand, not by a click. Its row exists
		// to say so.
		if (static_cast<oracool::AttackIcon>(index) == oracool::AttackIcon::Regular)
			ClearReadiedSpell(*MyPlayer);
		return;
	}

	SpellID rows[MaxSpellRows];
	size_t rowCount = 0;
	size_t rowIndex = index;
	if (CurrentSheet == AbilitySheet::Spells) {
		rowCount = BuildSpellRows(rows);
	} else {
		rowCount = BuildSkillRows(rows);
		rowIndex -= AttackRowCount;
	}
	if (rowIndex >= rowCount)
		return;

	const SpellID sn = rows[rowIndex];
	// An unlearned row is inert. It is listed so the book shows the whole set, not so it can be
	// readied - and its greyed icon already says so.
	if (!IsSpellKnown(sn))
		return;

	Player &player = *InspectPlayer;
	SpellType st = SpellType::Spell;
	if ((player._pISpells & GetSpellBitmask(sn)) != 0) {
		st = SpellType::Charges;
	}
	if ((player._pAblSpells & GetSpellBitmask(sn)) != 0) {
		st = SpellType::Skill;
	}
	player._pRSpell = sn;
	player._pRSplType = st;
	RedrawEverything();
}

void ReleaseSpellBookButtons()
{
	PressedArrow = 0;
}

} // namespace devilution
