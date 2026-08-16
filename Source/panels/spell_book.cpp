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
#include "oracool/class_skills.h"
#include "oracool/paladin_melee.h"
#include "oracool/paladin_skills.h"
#include "oracool/skill_points.h"
#include "oracool/spell_descriptions.h"
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

// Oracool: 6 -> 7 on 2026-08-15, when fifteen spells that had no book were given one. The table is
// no longer a LAYOUT - the list sorts itself by requirement (see BuildSpellRows) - so a "page" is
// now only a block of the set, and the split that still matters is which blocks a plain Diablo game
// reads. Pages 0-4 are Diablo's, 5-6 are Hellfire's; BuildSpellRows stops at 5 or 7 accordingly.
const size_t SpellBookPages = 7;
const size_t SpellBookPageEntries = 7;
/** @brief Pages 0..DiabloSpellPages-1 hold spells a non-Hellfire game must also be able to find. */
const size_t DiabloSpellPages = 5;

/** Maps from spellbook page number and position to SpellID. */
const SpellID SpellPages[SpellBookPages][SpellBookPageEntries] = {
	// --- Diablo's, pages 0-4 -------------------------------------------------------------------
	// Slot 0 was SpellID::Null, the placeholder the class skill used to occupy; the class skills
	// moved to their own sheet on 2026-08-15, so it takes a real spell now.
	{ SpellID::Infravision, SpellID::Firebolt, SpellID::ChargedBolt, SpellID::HolyBolt, SpellID::Healing, SpellID::HealOther, SpellID::Inferno },
	{ SpellID::Resurrect, SpellID::FireWall, SpellID::Telekinesis, SpellID::Lightning, SpellID::TownPortal, SpellID::Flash, SpellID::StoneCurse },
	{ SpellID::Phasing, SpellID::ManaShield, SpellID::Elemental, SpellID::Fireball, SpellID::FlameWave, SpellID::ChainLightning, SpellID::Guardian },
	{ SpellID::Nova, SpellID::Golem, SpellID::Teleport, SpellID::Apocalypse, SpellID::BoneSpirit, SpellID::BloodStar, SpellID::Etherealize },
	// The three the original developers left as name-only stubs - they carry MissileID::Null in both
	// slots, so casting one does nothing at all. Listed and given books on the user's explicit call
	// (2026-08-15), to be given substance later. Search used to sit on the old page 4 and is a Class
	// Skill now.
	{ SpellID::DoomSerpents, SpellID::BloodRitual, SpellID::Invisibility, SpellID::Invalid, SpellID::Invalid, SpellID::Invalid, SpellID::Invalid },
	// --- Hellfire's, pages 5-6 -----------------------------------------------------------------
	{ SpellID::LightningWall, SpellID::Immolation, SpellID::Warp, SpellID::Reflect, SpellID::Berserk, SpellID::RingOfFire, SpellID::Mana },
	{ SpellID::Magi, SpellID::Jester, SpellID::RuneOfFire, SpellID::RuneOfLight, SpellID::RuneOfNova, SpellID::RuneOfImmolation, SpellID::RuneOfStone }
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
	const size_t pages = gbIsHellfire ? SpellBookPages : DiabloSpellPages;
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
 * @brief Fills @p rows with the Class Skills sheet's entries. Returns how many.
 *
 * All six, to every class (user request 2026-08-15) - see oracool/class_skills.h. The one omission
 * is Item Repair while it is rendering as the Paladin's Charge: that has its own described row on
 * the Skills sheet, with a level gate and a mana price this compact row could not show, and listing
 * it twice under one name would be the same duplicate Search used to be.
 */
size_t BuildClassSkillRows(SpellID *rows)
{
	// All six, unconditionally. Oracool: user request (2026-08-15) - "Class skills sheet is missing
	// Repair Skill. Bring it back. New paladin skills we introduce or have already introduced will be
	// independent, not replacing Repair Skill."
	//
	// This used to skip Item Repair while it was rendering as Charge, because Charge is implemented
	// as a behaviour substitution ON that slot - so listing both would have been one ability under
	// two names. Repair is now always listed, which makes the sheet honest about what the class
	// actually has; the remaining half of the user's instruction - Charge getting a slot of its own
	// so it stops displacing Repair at all - is a deeper change and is NOT done here. See the note in
	// oracool/furious_charge.cpp.
	size_t count = 0;
	for (const SpellID skill : oracool::ClassSkills)
		rows[count++] = skill;
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
size_t BuildSkillRows(SpellID * /*rows*/)
{
	// Empty since 2026-08-15: the class skill that used to be this sheet's one spell-backed row now
	// lives on the Class Skills sheet with the other five. Kept as a function rather than deleted
	// because the Skills sheet is where a future spell-backed skill would go, and its caller already
	// threads the result through BuildSkillsSheetRows.
	return 0;
}

/**
 * @brief Rows the Skills sheet puts ahead of the spell-backed ones: Fist Attack and Regular Attack.
 *
 * First, for every class, because they are what the character does when nothing else is chosen - the
 * floor the rest of the sheet sits on. Whatever skills a class has of its own follow them.
 *
 * Fist above Regular per the user (2026-08-15); the listing order lives in
 * oracool::AttackIconDisplayOrder, apart from the enum, which is the icon strip's order.
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
	ClassSkills,
	Auras,
	Barbarian,
	LAST = Barbarian,
};
constexpr size_t AbilitySheetCount = 5;

AbilitySheet CurrentSheet = AbilitySheet::Spells;

/**
 * @brief The hover panel's text and anchor, handed forward to the end of the frame.
 *
 * See the note at the bottom of DrawHoverFeedback: the panel has to be drawn after the HUD, and the
 * Abilities window is drawn long before it.
 */
std::string PendingHoverTitle;
std::string PendingHoverText;
Rectangle PendingHoverAnchor;
bool HasPendingHover = false;
/** @brief Scroll offset per sheet, so switching sheets does not lose your place in the others. */
int ScrollOffset[AbilitySheetCount] = {};
int MaxScrollOffset = 0;
int ListHeight = 0;
/** @brief Set while an arrow is held, purely so it can be drawn pressed. */
int PressedArrow = 0;

/**
 * @brief Oracool: user request (2026-08-15) - "hide SKILLS abilities pages in Paladin and Barb for
 * now. We will reintroduce them when they are more developed."
 *
 * The Auras sheet (Paladin) and the Barbarian sheet are finished as LISTS - names, descriptions,
 * tiers, icons, unlock-by-level - and completely inert in play. Selecting one has no combat effect,
 * and there is not even anywhere to record a choice: oracool/auras.h's doc comment points at
 * "ActiveAura on Player" and no such member exists anywhere in the codebase. A page of things that
 * cannot be used reads as a bug rather than as a promise, so both wait for the Skills system - the
 * same gate already holding Furious Charge and the Warrior splash (both fully implemented and
 * returning false, see oracool/furious_charge.cpp and paladin_melee.cpp).
 *
 * Gated HERE and not in ClassHasAuras/ClassHasBarbSkills deliberately. Those answer "does this class
 * have auras at all", which is still true, and is the question the unlock-by-level checks inside
 * oracool/auras.cpp and barb_skills.cpp ask of themselves. This flag answers the different question
 * of whether the window should offer the page yet. Reintroducing them is deleting this constant and
 * the two terms below - nothing else has to be remembered.
 */
constexpr bool ClassAbilitySheetsHidden = true;

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
		return !ClassAbilitySheetsHidden && oracool::ClassHasAuras(*InspectPlayer);
	case AbilitySheet::Barbarian:
		return !ClassAbilitySheetsHidden && oracool::ClassHasBarbSkills(*InspectPlayer);
	case AbilitySheet::Skills:
		// Always: every class has the two basic attacks, so this sheet is never empty and is the
		// safe landing place when nothing else is available.
		break;
	case AbilitySheet::ClassSkills:
		// Always, and that is the point of it: as of 2026-08-15 every class has all six.
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
	case AbilitySheet::ClassSkills:
		return _("CLASS SKILLS");
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

/**
 * @brief One row of the Skills sheet, which is the only sheet with more than one KIND of row.
 *
 * Oracool: introduced 2026-08-15 with Charge and Zeal. Everywhere else a sheet's rows are uniform,
 * so drawing, scrolling and hit-testing could each do `index * rowHeight` independently. The Skills
 * sheet now mixes compact spell rows with tall described ones, and three separate copies of that
 * arithmetic would be three chances to disagree about which row the cursor is on. So the sheet is
 * enumerated ONCE, here, and all three walk the same list.
 *
 * It also retires the `rowIndex -= AttackRowCount` shift the draw and click paths each used to do
 * by hand.
 */
enum class SkillRowKind : uint8_t {
	Attack,   ///< Regular/Fist Attack, from the attack strip. Not a spell.
	Spell,    ///< The class's innate skill, drawn as a compact spell row.
	Paladin,  ///< Charge or Zeal - a described row with an icon, description and price.
};

struct SkillRow {
	SkillRowKind kind;
	oracool::AttackIcon attack;
	SpellID spell;
	oracool::PaladinSkill paladin;
};

constexpr size_t MaxSkillSheetRows = MaxSpellRows + AttackRowCount + oracool::PaladinSkillCount;

/**
 * @brief Fills @p out with the Skills sheet's rows, in display order. Returns how many.
 *
 * Charge is NOT a row of its own: it IS the Paladin's class skill (SpellID::ItemRepair), so the
 * spell row that would have carried it is replaced by the described one rather than sitting beside
 * it. Zeal has no spell backing at all and is simply appended.
 */
size_t BuildSkillsSheetRows(SkillRow *out)
{
	size_t count = 0;
	// Display order, not enum order - Fist above Regular (user request, 2026-08-15). The enum is the
	// icon strip's order and stays put; see oracool::AttackIconDisplayOrder.
	for (size_t i = 0; i < AttackRowCount; i++)
		out[count++] = { SkillRowKind::Attack, oracool::AttackIconDisplayOrder[i], SpellID::Invalid, {} };

	const bool paladin = InspectPlayer != nullptr && oracool::ClassHasPaladinSkills(*InspectPlayer);

	SpellID spells[MaxSpellRows];
	const size_t spellCount = BuildSkillRows(spells);
	for (size_t i = 0; i < spellCount; i++) {
		// Once Charge is earned it REPLACES the slot's Item Repair, so the compact spell row would be
		// a second name for a thing already listed below. Before then the slot really is Item Repair
		// - "the skill he is gifted at birth" - and keeps its row.
		if (oracool::IsFuriousChargeSpell(spells[i]))
			continue;
		out[count++] = { SkillRowKind::Spell, {}, spells[i], {} };
	}

	// All listed for the class that has them whether or not the level gate has opened: a locked row
	// says "Requires level 12", which is the useful thing to know at level 4.
	//
	// Every skill carries its own spell slot as of 2026-08-15, which is what makes every row
	// assignable to a mouse button; the slot comes from the skill table rather than being decided
	// here, so this loop has no opinion about which skills exist.
	if (paladin) {
		// Ordered by level requirement, lowest first (user request, 2026-08-15) - so the sheet reads
		// as the order they will actually be earned in, under the two attacks that need no level at
		// all. SORTED from the table rather than kept as a second hand-written order: change a
		// minLevel in paladin_skills.cpp and the row moves with it, and the two cannot disagree.
		// Stable, so skills that ever share a level keep their enum (and icon strip) order.
		size_t order[oracool::PaladinSkillCount];
		for (size_t i = 0; i < oracool::PaladinSkillCount; i++)
			order[i] = i;
		std::stable_sort(std::begin(order), std::end(order), [](size_t a, size_t b) {
			return oracool::GetPaladinSkillData(static_cast<oracool::PaladinSkill>(a)).minLevel
			    < oracool::GetPaladinSkillData(static_cast<oracool::PaladinSkill>(b)).minLevel;
		});
		for (const size_t index : order) {
			const auto skill = static_cast<oracool::PaladinSkill>(index);
			out[count++] = { SkillRowKind::Paladin, {}, oracool::GetPaladinSkillData(skill).spellId, skill };
		}
	}
	return count;
}

/** @brief How many rows @p sheet has right now. */
size_t GetRowCount(AbilitySheet sheet)
{
	SpellID rows[MaxSpellRows];
	switch (sheet) {
	case AbilitySheet::Spells:
		return BuildSpellRows(rows);
	case AbilitySheet::Skills: {
		SkillRow skillRows[MaxSkillSheetRows];
		return BuildSkillsSheetRows(skillRows);
	}
	case AbilitySheet::ClassSkills:
		return BuildClassSkillRows(rows);
	case AbilitySheet::Auras:
		return oracool::AuraCount;
	case AbilitySheet::Barbarian:
		return oracool::BarbSkillCount;
	}
	return 0;
}

/** @brief Height of row @p index on @p sheet - uniform everywhere except the Skills sheet. */
int RowHeightAt(AbilitySheet sheet, size_t index)
{
	if (sheet != AbilitySheet::Skills)
		return RowHeightFor(sheet);
	SkillRow rows[MaxSkillSheetRows];
	const size_t count = BuildSkillsSheetRows(rows);
	if (index >= count)
		return SpellRowHeight;
	return SpellRowHeight; // uniform again since the sheet lost its text (2026-08-15)
}

/** @brief Total height of every row on @p sheet. */
int TotalListHeight(AbilitySheet sheet)
{
	const size_t count = GetRowCount(sheet);
	if (sheet != AbilitySheet::Skills)
		return static_cast<int>(count) * RowHeightFor(sheet);
	int total = 0;
	for (size_t i = 0; i < count; i++)
		total += RowHeightAt(sheet, i);
	return total;
}

/** @brief Recomputes the scroll extent for the current sheet and re-clamps its offset. */
void UpdateScrollBounds()
{
	ListHeight = TotalListHeight(CurrentSheet);
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

	// Initialised even though GetDamageAmt now always writes both - this is the call site that
	// printed 0xCCCCCCCC when it did not.
	int min = -1;
	int max = -1;
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

/**
 * @brief The colour-coded ring saying which mouse button an ability is readied on.
 *
 * Oracool: user request (2026-08-15) - "We need RED outline on a skill/spell when it is used in LMB,
 * just like we need the existing YELLOW outline to inform us a skill/spell is assigned to RMB. These
 * color code outlines to work across all ability sheets."
 *
 * Both rings are the SAME rect and the same weight - the icon's own edge (user request,
 * 2026-08-15: "make RED outline of skills same size as Yellow outline"). The first version nested
 * them, red inside yellow, which made the left button's marker visibly the lesser of the two when
 * the buttons are equals.
 *
 * That leaves the case where one ability is readied on both buttons, and one rect cannot be two
 * colours. It is split by EDGE instead: "If both outlines end up on the same skill/spell - RED take
 * left and top edges, YELLOW take right and bottom edges." One square, read as two halves.
 *
 * @p iconRect is the icon's own top-left rect, which is why this takes a rect rather than a row: the
 * three row kinds anchor their icons differently and only they know where theirs ended up.
 */
void DrawAssignmentRings(const Surface &content, Rectangle iconRect, SpellID sn, SpellType st)
{
	if (IsInspectingPlayer())
		return;
	const Player &player = *InspectPlayer;

	// The gold the readied-spell border has used since the spellbook had one, and its counterpart on
	// the red ramp - PAL8_RED sits beside PAL8_YELLOW in the palette, so the two read as a pair.
	constexpr uint8_t RightButtonColor = PAL8_YELLOW + 2;
	constexpr uint8_t LeftButtonColor = PAL8_RED + 2;
	// 2px: thick enough to read against a busy icon, and the weight the single yellow border had
	// before there were two of them.
	constexpr int RingWeight = 2;

	const bool right = sn == player._pRSpell && st == player._pRSplType;
	const bool left = sn == player._pLRSpell && st == player._pLRSplType;
	if (!right && !left)
		return;

	// When only one button holds it, both halves get that button's colour and the result is a plain
	// square - so a single assignment looks the same whichever button it is on, which is the whole
	// point of the two markers being the same size now.
	oracool::DrawSplitOutline(content, iconRect,
	    left ? LeftButtonColor : RightButtonColor,
	    right ? RightButtonColor : LeftButtonColor,
	    RingWeight);
}

/** @brief One row of the Skills sheet's two basic attacks. @p index is oracool::AttackIcon order. */
void DrawAttackRow(const Surface &content, size_t index, int top)
{
	const auto icon = static_cast<oracool::AttackIcon>(index);
	// Exactly one of the two is what the hand is currently doing, and the other is blended - the
	// same treatment a locked aura gets, used here as an indicator rather than as an availability
	// state. It is what makes the pair read as one status line instead of two abilities.
	const bool active = oracool::BasicAttackIcon(*InspectPlayer) == icon;

	// The plate's height when no attack strip is shipped - it is what the row actually draws then.
	Size iconSize = oracool::GetAttackIconSize();
	if (iconSize.height == 0)
		iconSize = oracool::GetSkillIconPlateSize();
	const Point iconPos { AbilitiesIconX, top + (SpellRowHeight - iconSize.height) / 2 };
	// Pink even for the inactive one, deliberately: grey means "not earned yet" everywhere else on
	// these sheets, and both attacks are always earned. Which one is in your hand is said by the
	// blended icon, not by the plate under it.
	oracool::DrawAttackIcon(content, iconPos, static_cast<int>(index), active, oracool::SkillPlateTint::Green);

	// The ring goes on whichever row is what the button ACTUALLY does - which is the active one, not
	// Regular unconditionally.
	//
	// Oracool: user bug report (2026-08-15) - "if only shield is equipped LMB shows Fist Attack, but
	// skill sheet shows RegAtak selected". The well has always drawn BasicAttackIcon's answer, and a
	// shield is not a weapon, so shield-only draws the fist; the sheet meanwhile ringed Regular
	// whatever was equipped. Two readings of one state. `active` is that same answer, so the two
	// cannot disagree again.
	if (active)
		DrawAssignmentRings(content, { iconPos, iconSize }, SpellID::Invalid, SpellType::Invalid);

	// Oracool: user request (2026-08-15) - "Remove all text from Skills Ability sheet. I will later
	// introduce skill runes which will take its place. Text will only be reachable through the pop-up
	// window." So the row is the icon and nothing else; the name and description live in the hover
	// panel, and the space the text used to fill is being kept for the runes.
}

// Phase 2.1: the invest control. A small gold "+" at the row's right edge, shown only while the
// local player has an unspent point this row can take - so the sheet is quiet until level-up hands
// out a point, and quiet again once it is sunk. The invested count sits beside it permanently.
constexpr Size InvestButtonSize { 22, 22 };
constexpr int InvestButtonRightPad = 4;

Rectangle InvestButtonRect(int top, int rowHeight)
{
	return { { AbilitiesContentRightLimit - InvestButtonSize.width - InvestButtonRightPad,
		         top + (rowHeight - InvestButtonSize.height) / 2 },
		InvestButtonSize };
}

bool InvestZoneClicked(int localX)
{
	return localX >= AbilitiesContentRightLimit - InvestButtonSize.width - InvestButtonRightPad
	    && localX < AbilitiesContentRightLimit - InvestButtonRightPad;
}

void DrawInvestControls(const Surface &content, int top, int rowHeight, SpellID sn)
{
	if (IsInspectingPlayer())
		return;
	const Player &player = *MyPlayer;
	if (sn == SpellID::Invalid)
		return;
	const int invested = player._pSkillInvestment[static_cast<size_t>(sn)];
	if (invested > 0) {
		// The sunk points, said plainly and always - the "+" comes and goes with the unspent pool,
		// but what a skill has already been fed is permanent information.
		DrawString(content, fmt::format("+{:d}", invested),
		    { { AbilitiesContentRightLimit - InvestButtonSize.width - InvestButtonRightPad - 34,
			      top },
		        { 30, rowHeight } },
		    { UiFlags::ColorWhitegold | UiFlags::AlignRight | UiFlags::VerticalCenter });
	}
	if (!oracool::CanInvestSkillPoint(player, sn))
		return;
	const Rectangle button = InvestButtonRect(top, rowHeight);
	oracool::DrawHoverOutline(content, button);
	DrawString(content, "+", button,
	    { UiFlags::ColorWhitegold | UiFlags::FontSize24 | UiFlags::AlignCenter | UiFlags::VerticalCenter });
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
	if (known) {
		// iconPos is a BOTTOM-left anchor; the rings want the top-left rect.
		DrawAssignmentRings(content, { { iconPos.x, iconPos.y - iconSize.height + 1 }, iconSize },
		    sn, GetSBookTrans(sn, true));
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

	DrawInvestControls(content, top, SpellRowHeight, sn);
}

/** @brief Which 38x38 strip a described row takes its icon from. */
enum class DescribedIcons : uint8_t {
	Aura,
	Barbarian,
	Paladin,
};

/**
 * @brief Draws an icon-plus-prose row - the shape the Auras, Barbarian and Paladin rows all use.
 *
 * @p tag is an optional short right-aligned label on the name line (the Barbarian's Combat/Warcry/
 * Passive/Utility, the Paladin skills' mana price); empty for the auras, which have no such
 * distinction.
 * @p requirement replaces the description when the entry is locked.
 */
void DrawDescribedRow(const Surface &content, int top, int iconIndex, DescribedIcons icons, bool unlocked,
    string_view name, string_view tag, string_view description, int requiredLevel)
{
	Size iconSize {};
	switch (icons) {
	case DescribedIcons::Aura:
		iconSize = oracool::GetAuraIconSize();
		break;
	case DescribedIcons::Barbarian:
		iconSize = oracool::GetBarbSkillIconSize();
		break;
	case DescribedIcons::Paladin:
		iconSize = oracool::GetPaladinSkillIconSize();
		break;
	}
	// With no custom strip shipped the row still shows the plate, so it is the plate that decides the
	// layout - asking hud_art for it rather than having the Get*IconSize functions pretend the art
	// exists, which is the mistake that fired an assert in the HUD's skill wells on 2026-08-15.
	if (iconSize.width == 0)
		iconSize = oracool::GetSkillIconPlateSize();
	const int iconWidth = iconSize.width > 0 ? iconSize.width : 38;
	const int iconHeight = iconSize.height > 0 ? iconSize.height : 38;
	const int textX = AbilitiesIconX + iconWidth + AbilitiesTextGap;
	const int textWidth = AbilitiesContentRightLimit - textX;

	// Top-left origin here, unlike the spell icons' bottom-left - these are blitted rather than
	// drawn as CLX sprites.
	const Point iconPos { AbilitiesIconX, top + (DescribedRowHeight - iconHeight) / 2 };
	// Pink on every plate-drawn sheet (user request, 2026-08-15): the Skills sheet got it first, and
	// Auras and Barbarian followed once it was clear the plate colour separates ABILITY KINDS from
	// spells and class skills, not one sheet from the other three. Grey when the row is not earned
	// yet - the same grey an unlearned spell gets on the Spells sheet.
	const oracool::SkillPlateTint tint = unlocked ? oracool::SkillPlateTint::Green : oracool::SkillPlateTint::Grey;
	switch (icons) {
	case DescribedIcons::Aura:
		oracool::DrawAuraIcon(content, iconPos, iconIndex, unlocked, tint);
		break;
	case DescribedIcons::Barbarian:
		oracool::DrawBarbSkillIcon(content, iconPos, iconIndex, unlocked, tint);
		break;
	case DescribedIcons::Paladin:
		oracool::DrawPaladinSkillIcon(content, iconPos, iconIndex, unlocked, tint);
		break;
	}

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
	DrawDescribedRow(content, top, oracool::GetAuraIconIndex(aura), DescribedIcons::Aura,
	    oracool::IsAuraUnlocked(*InspectPlayer, aura), _(data.name),
	    oracool::GetAuraTierName(data.tier), _(data.description),
	    oracool::GetAuraTierMinLevel(data.tier));
}

/**
 * @brief Charge and Zeal: the two Paladin skills that actually do something.
 *
 * The tag slot carries the mana price rather than a category. With two entries a category would say
 * nothing, whereas "10 mana" is the one number a player needs before deciding to lean on it - and it
 * is the field the Barbarian sheet already established for "the thing to know at a glance".
 */
/**
 * @brief Charge and Zeal - icon only, like the rest of the Skills sheet.
 *
 * Oracool: user request (2026-08-15) stripped this sheet's text. These two used to be tall described
 * rows carrying a name, a mana price, a wrapped description and a "Requires level N" line; all four
 * moved to the hover panel, which is now the only place they appear. That also returned the sheet to
 * a UNIFORM row height - the mixed-height walk BuildSkillsSheetRows exists for is currently
 * academic, and is kept because the rune slots the user is planning will reintroduce the variety.
 */
void DrawPaladinSkillRow(const Surface &content, oracool::PaladinSkill skill, int top)
{
	Size iconSize = oracool::GetPaladinSkillIconSize();
	if (iconSize.height == 0)
		iconSize = oracool::GetSkillIconPlateSize();
	const Point iconPos { AbilitiesIconX, top + (SpellRowHeight - iconSize.height) / 2 };
	const bool unlocked = oracool::IsPaladinSkillUnlocked(*InspectPlayer, skill);
	oracool::DrawPaladinSkillIcon(content, iconPos, oracool::GetPaladinSkillIconIndex(skill), unlocked,
	    unlocked ? oracool::SkillPlateTint::Green : oracool::SkillPlateTint::Grey);

	// Every skill carries a slot now, so every unlocked row can show which button holds it.
	if (unlocked) {
		const SpellID sn = oracool::GetPaladinSkillData(skill).spellId;
		DrawAssignmentRings(content, { iconPos, iconSize }, sn, GetSBookTrans(sn, true));
		// Investment keys on the skill's SpellID even where the row is not readiable (Zeal): the
		// slot exists for every skill, only BuildSkillsSheetRows withholds it from the ready path.
		DrawInvestControls(content, top, SpellRowHeight, sn);
	}
}

void DrawBarbSkillRow(const Surface &content, size_t index, int top)
{
	const oracool::BarbSkill skill = oracool::GetBarbSkillAtDisplayIndex(index);
	const oracool::BarbSkillData &data = oracool::GetBarbSkillData(skill);
	DrawDescribedRow(content, top, oracool::GetBarbSkillIconIndex(skill), DescribedIcons::Barbarian,
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

/**
 * @brief The stat block the hover panel puts under a spell's description.
 *
 * Oracool: user request (2026-08-15) - "stats of spells like dmg, which level it currently is, dmg
 * at next level and whatever else is required for a full informative pop-up".
 *
 * What is worth showing turns out to be decided by the spell rather than uniform, so this is a set
 * of conditional lines rather than a fixed template:
 *
 *   - The magic requirement is shown ONLY when the character does not meet it, where it is the
 *     reason the spell is unusable. Once met it is noise.
 *   - Damage is skipped entirely for the spells that have none - GetDamageAmt reports -1 for them,
 *     which is every utility spell in the game. Bone Spirit is its own case: its damage is a
 *     fraction of the target's health, not a range.
 *   - Healing spells say "Heals" rather than "Damage", because they do.
 *   - The next-level line is omitted when the spell is unusable (level 0 tells you nothing about
 *     level 1's numbers being reachable) and when the numbers do not actually change, which is true
 *     of several spells whose damage scales on character level alone.
 */
std::string BuildSpellStatBlock(SpellID sn)
{
	const Player &player = *InspectPlayer;
	std::string out;
	const auto line = [&out](const std::string &text) {
		if (!out.empty())
			out += '\n';
		out += text;
	};

	const int required = GetSpellData(sn).minInt;
	const int level = player.GetSpellLevel(sn);

	if (GetSBookTrans(sn, false) == SpellType::Spell) {
		line(level == 0
		        ? std::string(_("Spell Level 0 - Unusable"))
		        : fmt::format(fmt::runtime(_("Spell Level {:d}")), level));
	}
	if (player._pMagic < required)
		line(fmt::format(fmt::runtime(_("Requires {:d} Magic")), required));

	line(fmt::format(fmt::runtime(_("Mana: {:d}")), GetManaAmount(player, sn) >> 6));

	if (sn == SpellID::BoneSpirit) {
		line(std::string(_("Damage: 1/3 of target's health")));
		return out;
	}

	int min = -1;
	int max = -1;
	GetDamageAmt(sn, &min, &max);
	if (min == -1)
		return out; // a utility spell - it has no damage to report, so it says nothing

	const bool heals = sn == SpellID::Healing || sn == SpellID::HealOther;
	line(fmt::format(fmt::runtime(heals ? _("Heals: {:d} - {:d}") : _("Damage: {:d} - {:d}")), min, max));

	if (level > 0) {
		int nextMin = -1;
		int nextMax = -1;
		GetDamageAmtAtLevel(sn, level + 1, &nextMin, &nextMax);
		if (nextMin != -1 && (nextMin != min || nextMax != max)) {
			line(fmt::format(fmt::runtime(heals ? _("Next level: {:d} - {:d}") : _("Next level: {:d} - {:d}")),
			    nextMin, nextMax));
		}
	}
	return out;
}

/**
 * @brief Marks the row under the cursor and describes it.
 *
 * Oracool: user request (2026-08-15) - a subtle gold outline on the hovered row, and a pop-up
 * describing it. Both together here because they answer the same question ("which row is the cursor
 * on?") and answering it twice would be two chances to disagree.
 *
 * Walks the rows exactly as the draw loop below does, accumulating heights, so the Skills sheet's
 * mixed row heights are handled without a second copy of that arithmetic. Runs BEFORE the rows are
 * drawn so the outline sits under the icons and text rather than across them; the pop-up is drawn
 * into `out` at the end for the opposite reason - it must cover whatever it overlaps.
 */
void DrawHoverFeedback(const Surface &out, const Surface &content, Rectangle contentRect, int scroll)
{
	// Same span a click uses: short of the scrollbar, so hovering the bar does not light a row.
	const Rectangle hoverArea { contentRect.position, { AbilitiesContentRightLimit, contentRect.size.height } };
	if (!hoverArea.contains(MousePosition))
		return;

	const int y = MousePosition.y - contentRect.position.y + scroll;
	std::string title;
	std::string description;
	int rowTop = 0;
	int rowHeight = 0;
	bool found = false;

	// A spell's panel is its prose and then its numbers, separated by a blank line. The other sheets
	// pass through unchanged: an aura or Barbarian skill has no numbers yet, and a Paladin skill
	// already carries its price and gate on the row itself.
	const auto spellInfo = [](SpellID spell) {
		// Explicit construction: _() hands back a string_view, and that conversion is explicit.
		std::string text { _(oracool::GetSpellDescription(spell)) };
		const std::string stats = BuildSpellStatBlock(spell);
		if (!stats.empty()) {
			if (!text.empty())
				text += "\n\n";
			text += stats;
		}
		return text;
	};

	if (CurrentSheet == AbilitySheet::Skills) {
		SkillRow rows[MaxSkillSheetRows];
		const size_t rowCount = BuildSkillsSheetRows(rows);
		int top = 0;
		for (size_t i = 0; i < rowCount && !found; i++) {
			const int height = SpellRowHeight;
			if (y >= top && y < top + height) {
				rowTop = top;
				rowHeight = height;
				found = true;
				switch (rows[i].kind) {
				case SkillRowKind::Attack:
					title = _(oracool::AttackIconName(rows[i].attack));
					description = rows[i].attack == oracool::AttackIcon::Regular
					    ? _("Swing whatever is in hand. What a click does when no spell is readied.")
					    : _("Strike bare-handed. What Regular Attack becomes with no weapon held.");
					break;
				case SkillRowKind::Spell:
					title = oracool::GetSpellDisplayName(rows[i].spell); // already translated
					description = spellInfo(rows[i].spell);
					break;
				case SkillRowKind::Paladin: {
					const oracool::PaladinSkillData &pd = oracool::GetPaladinSkillData(rows[i].paladin);
					title = _(pd.name);
					// The mana price and level gate lived on the row until the sheet lost its text.
					// Range joined them once it started deciding whether a click casts or walks - a
					// rule the player is subject to has to be a rule the player can read.
					description = std::string(_(pd.description)) + "\n\n";
					// Zeal's strike count grows with character level, so the popup shows what the
					// player has NOW rather than a sentence that goes stale two levels later. Its
					// mana line reads per strike for the same reason - the burst's real price is the
					// count beside it.
					if (rows[i].paladin == oracool::PaladinSkill::Zeal) {
						description += fmt::format(fmt::runtime(_("Strikes: {:d}")),
						                   oracool::ZealStrikeCount(*InspectPlayer))
						    + "\n"
						    + fmt::format(fmt::runtime(_("Mana: {:d} per strike")), pd.manaCost) + "\n";
					} else {
						description += fmt::format(fmt::runtime(_("Mana: {:d}")), pd.manaCost) + "\n";
					}
					description +=
					    (pd.rangeTiles <= oracool::MeleeSkillRangeTiles
					            ? std::string(_("Range: melee"))
					            : fmt::format(fmt::runtime(_("Range: {:d} tiles")), pd.rangeTiles))
					    + "\n"
					    + fmt::format(fmt::runtime(_("Requires level {:d}")), pd.minLevel);
					break;
				}
				}
			}
			top += height;
		}
	} else {
		const int height = RowHeightFor(CurrentSheet);
		const size_t index = static_cast<size_t>(y / height);
		if (index < GetRowCount(CurrentSheet)) {
			rowTop = static_cast<int>(index) * height;
			rowHeight = height;
			found = true;
			switch (CurrentSheet) {
			case AbilitySheet::Auras: {
				const oracool::Aura aura = oracool::GetAuraAtDisplayIndex(index);
				title = _(oracool::GetAuraData(aura).name);
				description = _(oracool::GetAuraData(aura).description);
				break;
			}
			case AbilitySheet::Barbarian: {
				const oracool::BarbSkill skill = oracool::GetBarbSkillAtDisplayIndex(index);
				title = _(oracool::GetBarbSkillData(skill).name);
				description = _(oracool::GetBarbSkillData(skill).description);
				break;
			}
			case AbilitySheet::Spells:
			case AbilitySheet::ClassSkills: {
				SpellID rows[MaxSpellRows];
				const size_t rowCount = CurrentSheet == AbilitySheet::ClassSkills
				    ? BuildClassSkillRows(rows)
				    : BuildSpellRows(rows);
				if (index < rowCount) {
					title = oracool::GetSpellDisplayName(rows[index]); // already translated
					description = spellInfo(rows[index]);
				}
				break;
			}
			case AbilitySheet::Skills:
				break; // handled above
			}
		}
	}

	if (!found)
		return;

	// Local to the content subregion, which is what clips it to the scrolling area.
	oracool::DrawHoverOutline(content, { { 0, rowTop - scroll }, { AbilitiesContentRightLimit, rowHeight } });

	// DEFERRED, not drawn here. Oracool: user request (2026-08-15) - "pop-up windows to be rendered
	// on top of all including bottom hud, to be readable." The Abilities window is drawn early in the
	// frame (scrollrt.cpp ~1436), well before the HUD plate and the orbs, so a panel drawn at this
	// point is painted over by them - which is exactly what a screenshot showed, the panel's lower
	// half swallowed by the belt.
	//
	// So the row hands its text to DrawAbilityHoverPanel, which the frame calls beside the cursor
	// tooltip - the slot already reserved for "above everything". The OUTLINE stays here on purpose:
	// it belongs under the row's own icon and text, and inside the clipped content subregion.
	// Either half is enough to be worth showing: the Skills sheet's rows now carry NO text at all, so
	// on that sheet the name in the panel is the only thing naming the icon under the cursor.
	if (!description.empty() || !title.empty()) {
		PendingHoverTitle = title;
		PendingHoverText = description;
		PendingHoverAnchor = { { contentRect.position.x, contentRect.position.y + rowTop - scroll },
			{ AbilitiesContentRightLimit, rowHeight } };
		HasPendingHover = true;
	}
}

void DrawAbilityHoverPanel(const Surface &out)
{
	if (!HasPendingHover)
		return;
	// Cleared unconditionally, before the draw rather than after: this is the only consumer, and if
	// it is ever called on a frame where the window closed after setting it, the panel must not
	// survive into the next one.
	HasPendingHover = false;
	oracool::DrawHoverPanel(out, PendingHoverTitle, PendingHoverText, PendingHoverAnchor);
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
	// Phase 2.1: the unspent pool, right-aligned in the title band - present only while there is
	// something to spend, so the band stays clean the rest of the time.
	if (!IsInspectingPlayer() && MyPlayer->_pUnspentSkillPoints > 0) {
		DrawString(out, fmt::format(fmt::runtime(_("Points: {:d}")), int(MyPlayer->_pUnspentSkillPoints)),
		    labelArea, { UiFlags::ColorWhitegold | UiFlags::AlignRight | UiFlags::VerticalCenter });
	}
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

	DrawHoverFeedback(out, content, contentRect, scroll);

	// The Skills sheet walks its own enumeration because its rows differ in kind AND in height; every
	// other sheet is uniform and keeps the simple stride.
	if (CurrentSheet == AbilitySheet::Skills) {
		SkillRow rows[MaxSkillSheetRows];
		const size_t rowCount = BuildSkillsSheetRows(rows);
		int top = -scroll;
		for (size_t i = 0; i < rowCount; i++) {
			const int height = SpellRowHeight;
			if (top + height > 0 && top < AbilitiesContentSize.height) {
				switch (rows[i].kind) {
				case SkillRowKind::Attack:
					DrawAttackRow(content, static_cast<size_t>(rows[i].attack), top);
					break;
				case SkillRowKind::Spell:
					DrawSpellRow(content, i, rows[i].spell, top);
					break;
				case SkillRowKind::Paladin:
					DrawPaladinSkillRow(content, rows[i].paladin, top);
					break;
				}
			}
			top += height;
		}
		return;
	}

	SpellID rows[MaxSpellRows];
	size_t rowCount = 0;
	if (CurrentSheet == AbilitySheet::Spells)
		rowCount = BuildSpellRows(rows);
	else if (CurrentSheet == AbilitySheet::ClassSkills)
		rowCount = BuildClassSkillRows(rows);
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
		case AbilitySheet::ClassSkills:
			DrawSpellRow(content, i, rows[i], top);
			break;
		case AbilitySheet::Skills:
			break; // handled above
		}
	}
}

void CheckSBook(bool assignToRightButton)
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
	const int y = MousePosition.y - content.position.y + CurrentScroll();
	// Phase 2.1: whether this click landed on the row's invest button rather than the row itself.
	const bool investClick = InvestZoneClicked(MousePosition.x - content.position.x);

	// Auras and Barbarian skills are listed and described but not yet selectable - the gameplay
	// passes that give them effects have not been built. Clicking one deliberately does nothing
	// rather than setting a state nothing reads. See the vault's aura implementation plan.
	if (CurrentSheet == AbilitySheet::Auras || CurrentSheet == AbilitySheet::Barbarian)
		return;

	SpellID sn = SpellID::Invalid;
	if (CurrentSheet == AbilitySheet::Skills) {
		// Walks the same enumeration the draw loop does, accumulating heights, because this sheet's
		// rows are not all the same height - `y / rowHeight` would land on the wrong row the moment a
		// tall Paladin row sat above the cursor.
		SkillRow rows[MaxSkillSheetRows];
		const size_t rowCount = BuildSkillsSheetRows(rows);
		int top = 0;
		for (size_t i = 0; i < rowCount; i++) {
			const int height = SpellRowHeight;
			if (y >= top && y < top + height) {
				switch (rows[i].kind) {
				case SkillRowKind::Attack:
					// Either attack row readies the basic attack, which in this engine means clearing
					// the readied spell - that IS the state in which a click swings.
					//
					// BOTH rows, where Fist Attack used to be inert. They are one state shown as two
					// pictures, and which picture you get is decided by what is in your hand, not by
					// a click. Leaving Fist inert meant a player with only a shield equipped - whose
					// well correctly shows the fist - clicked the row matching their own HUD and had
					// nothing happen. See the same bug's other half in DrawAttackRow.
					//
					// Clears whichever button you clicked it with, matching how assignment works
					// below, so either row is the way back to swinging on either button.
					if (assignToRightButton) {
						ClearReadiedSpell(*MyPlayer);
					} else {
						MyPlayer->_pLRSpell = SpellID::Invalid;
						MyPlayer->_pLRSplType = SpellType::Invalid;
						RedrawEverything();
					}
					return;
				case SkillRowKind::Spell:
					if (investClick && oracool::InvestSkillPoint(*MyPlayer, rows[i].spell)) {
						RedrawEverything();
						return;
					}
					sn = rows[i].spell;
					break;
				case SkillRowKind::Paladin:
					// Investment works through the skill's own SpellID even on rows the ready path
					// withholds a slot from (Zeal) - so the invest check comes before that gate.
					if (investClick
					    && oracool::InvestSkillPoint(*MyPlayer,
					        oracool::GetPaladinSkillData(rows[i].paladin).spellId)) {
						RedrawEverything();
						return;
					}
					// A row is readiable when it carries a spell slot, which BuildSkillsSheetRows is
					// the only place that decides. Charge has one; Zeal does not, because it applies
					// itself to every melee swing rather than being cast, and neither do the five
					// skills that are still art and a description. Testing the slot rather than
					// naming Charge means a skill that later gains one needs no edit here.
					if (!IsValidSpell(rows[i].spell))
						return;
					// A locked row is inert too - it names something the player has not earned yet.
					if (!oracool::IsPaladinSkillUnlocked(*InspectPlayer, rows[i].paladin))
						return;
					sn = rows[i].spell;
					break;
				}
				break;
			}
			top += height;
		}
		if (sn == SpellID::Invalid)
			return;
	} else {
		SpellID rows[MaxSpellRows];
		const size_t rowCount = CurrentSheet == AbilitySheet::ClassSkills
		    ? BuildClassSkillRows(rows)
		    : BuildSpellRows(rows);
		const size_t rowIndex = static_cast<size_t>(y / RowHeightFor(CurrentSheet));
		if (rowIndex >= rowCount)
			return;
		sn = rows[rowIndex];
		if (investClick && oracool::InvestSkillPoint(*MyPlayer, sn)) {
			RedrawEverything();
			return;
		}
	}
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
	// Oracool: user request (2026-08-15) - "whichever mouse button i click with on a skill/spell
	// that's where the skill/spell lands." The row is assigned to the button that clicked it, full
	// stop: no modifier to discover, and the gesture states the outcome.
	//
	// This replaced a shift-click, which was chosen because right-clicking inside a panel is usually
	// how this game closes one. It turns out not to be here - RightMouseDown's only response to a
	// click inside the Abilities window was to return - so the obvious gesture was free all along.
	if (assignToRightButton) {
		player._pRSpell = sn;
		player._pRSplType = st;
	} else {
		player._pLRSpell = sn;
		player._pLRSplType = st;
	}
	RedrawEverything();
}

void ReleaseSpellBookButtons()
{
	PressedArrow = 0;
}

} // namespace devilution
