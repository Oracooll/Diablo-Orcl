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
#include "oracool/class_tree.h"
#include "oracool/skill_points.h"
#include "oracool/spell_descriptions.h"
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
	//
	// Substance first, though, ahead of both. Three of the entries above - Doom Serpents, Blood
	// Ritual and Invisibility - are the original developers' name-only stubs, and their table rows
	// are zeroed throughout: no mana, no book price, MissileID::Null in both slots, and minInt 0.
	// That last zero is a placeholder, not a requirement, so sorting on it alone put all three
	// AHEAD of every real spell - the Spells sheet opened with the only three entries in it that do
	// nothing at all (user, 2026-08-16). They sink to the bottom until they are given substance.
	//
	// "Has a missile" is the right test only because this list is the BOOK spells and nothing else:
	// a book spell is its missile. It would be quite wrong applied generally - every Paladin skill
	// carries MissileID::Null in both slots too, and each of those is implemented in code.
	const auto isImplemented = [](SpellID sn) {
		const SpellData &data = GetSpellData(sn);
		return data.sMissiles[0] != MissileID::Null || data.sMissiles[1] != MissileID::Null;
	};
	std::sort(rows, rows + count, [&isImplemented](SpellID a, SpellID b) {
		const bool liveA = isImplemented(a);
		const bool liveB = isImplemented(b);
		if (liveA != liveB)
			return liveA;
		const int reqA = GetSpellData(a).minInt;
		const int reqB = GetSpellData(b).minInt;
		if (reqA != reqB)
			return reqA < reqB;
		return oracool::GetSpellDisplayName(a) < oracool::GetSpellDisplayName(b);
	});
	return count;
}

// Oracool V1: the shared theme and geometry - the same 340x720 window as the waypoint list, quest
// log, character sheet, stash and inventory.
//
//   0..8       top margin
//   8..46      title band, the sheet's name - oracool::PanelTitleTop / PanelTitleHeight, so this
//              window's title sits on the same line as the other five
//   46..101    the painted background's arch shoulders - ornament, nothing may be drawn here
//   101..127   the nav row: the two sheet arrows at its ends, the unspent-points count between them
//   127..624   content area - the scrolling list, ending at oracool::SidePanelContentBottom
//   624..720   the background's bottom ornament
//
// Rebuilt 2026-08-16. The title band used to be 24..74 with a 74..77 separator rule under it, and
// the title, the points count AND the right-hand arrow were all drawn into that one 292px-wide rect
// with three different alignments - so on any sheet whose name is long ("DEFENSIVE AURAS",
// "OFFENSIVE AURAS", "COMBAT SKILLS") the three simply overlapped. The points count has its own row
// now, which is also where the arrows moved to; the title has the band to itself and cannot collide
// with anything.
constexpr Size AbilitiesPanelSize { 340, 720 };
constexpr int AbilitiesMargin = 24;

/**
 * @brief First y at which a full-width row clears the painted background's arch.
 *
 * Measured off ui/stash_background.png rather than derived from margins: the header strip and the
 * arch's shoulders carry ornament down to about y=101, and the interior is clear from there to about
 * y=655. The stash's own page row starts on this same line, which is why the two windows' first rows
 * agree when both are open.
 */
constexpr int AbilitiesArchTop = 101;
/**
 * @brief The content begins where the arch ends. The 26px nav row that used to sit between them is
 * gone (2026-08-17): its arrows moved up into the title band and its points readout moved out to
 * the HUD, into the frame above the RMB well - so the row held nothing, and the list gets its
 * height.
 */
constexpr int AbilitiesContentTop = AbilitiesArchTop;
/**
 * @brief The list's height.
 *
 * Bounded by oracool::SidePanelContentBottom, not by the panel's own height less a margin. The old
 * bound ran the list to y=696, straight across the background art's bottom ornament - the same
 * mistake the character sheet and quest log made against the health orb, and the reason that
 * constant exists.
 */
constexpr Size AbilitiesContentSize { AbilitiesPanelSize.width,
	oracool::SidePanelContentBottom - AbilitiesContentTop };
static_assert(AbilitiesContentTop >= oracool::PanelTitleTop + oracool::PanelTitleHeight,
    "the list starts inside the title band");
static_assert(AbilitiesContentSize.height > 0, "the nav row has eaten the whole list");

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

/**
 * @brief The window's sheets: the book of spells, then the class's three tree pages.
 *
 * Two more sat here until 2026-08-16, and the user removed both: "get rid of skills sheet + get rid
 * of class skills. V1 will not need those, they are useless."
 *
 * SKILLS held the two basic attacks and the seven Paladin skills, five of which the Combat Skills
 * tree page already carried - the same spell slot, the same investment, drawn twice. The remaining
 * two (Hammer of Faith, Blessed Shield) were given tree rows of their own rather than deleted, so
 * the tree is now the single place a Paladin skill lives.
 *
 * CLASS SKILLS listed Item Repair, Trap Disarm, Staff Recharge, Search, Identify and Rage. The
 * ABILITIES are untouched - every class still has all six in _pAblSpells, and the speedbook still
 * offers them; only the sheet is gone.
 */
enum class AbilitySheet : uint8_t {
	Spells,
	/**
	 * Diablo II's class tree, one sheet per page. These replaced the single "Auras" sheet, which
	 * listed an invented 24-aura set that never matched D2 and was hidden from the window anyway.
	 * See oracool/class_tree.h.
	 */
	ClassTree0,
	ClassTree1,
	ClassTree2,
	LAST = ClassTree2,
};
/**
 * @brief How many sheets exist. DERIVED, never restated.
 *
 * Bug (fixed 2026-08-16, user report - a screenshot of a sheet with no title and no content): this
 * was the literal 7 while the enum held six. Nothing indexed out of bounds, because every consumer
 * either switches on the enum or indexes ScrollOffset, which was sized from this same number - so
 * the phantom simply fell through every switch: GetSheetTitle returned an empty string, GetRowCount
 * returned 0, and IsSheetAvailable's default said yes. The arrows cycled you onto a blank page.
 */
constexpr size_t AbilitySheetCount = static_cast<size_t>(AbilitySheet::LAST) + 1;

/**
 * @brief The tree page index a sheet shows, or nullopt if it is not one of the three.
 *
 * The sheets are numbered rather than named because WHICH tree they show depends on the class -
 * page 1 is the Paladin's Offensive Auras, the Barbarian's Combat Masteries, the Sorceress's
 * Lightning Spells and the Rogue's Passive & Magic. The names come from the tree itself.
 */
std::optional<int> TreePageOf(AbilitySheet sheet)
{
	switch (sheet) {
	case AbilitySheet::ClassTree0:
		return 0;
	case AbilitySheet::ClassTree1:
		return 1;
	case AbilitySheet::ClassTree2:
		return 2;
	default:
		return std::nullopt;
	}
}

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

// The ClassAbilitySheetsHidden flag that used to live here is gone with the two sheets it hid.
// It was raised on 2026-08-15 because the Paladin's Auras sheet and the Barbarian's skill sheet
// were finished LISTS with no gameplay behind them, and a page of things that cannot be used reads
// as a bug rather than a promise. Both lists were invented before the real Diablo II sheets
// arrived, and both have since been superseded by oracool/class_tree - which is spendable, acts,
// and is honest about the rows that do not - so there is nothing left to hide.

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
	case AbilitySheet::ClassTree0:
	case AbilitySheet::ClassTree1:
	case AbilitySheet::ClassTree2:
		// Not behind ClassAbilitySheetsHidden: unlike the list it replaced, the tree is spendable
		// and its skills act, so there is nothing inert to hide.
		return oracool::ClassHasTree(InspectPlayer->_pClass);
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
 * Always Spells today, since Spells is universal - so this looks like a loop that could be a
 * constant. It was a hardcoded Spells once, and that was fine right up until Spells was reserved to
 * the Sorcerer, at which point a Paladin was sent to a sheet it did not have by the one guard whose
 * whole job was to prevent that. Spells is universal again now, which is exactly why this stays a
 * search: availability has already changed twice.
 */
AbilitySheet FirstAvailableSheet()
{
	for (size_t i = 0; i < AbilitySheetCount; i++) {
		const auto sheet = static_cast<AbilitySheet>(i);
		if (IsSheetAvailable(sheet))
			return sheet;
	}
	return AbilitySheet::Spells;
}

string_view GetSheetTitle(AbilitySheet sheet)
{
	switch (sheet) {
	case AbilitySheet::Spells:
		return _("SPELLS");
	case AbilitySheet::ClassTree0:
	case AbilitySheet::ClassTree1:
	case AbilitySheet::ClassTree2:
		// Named by the tree, not here: each class calls its three pages something different.
		return oracool::GetClassTreePageName(InspectPlayer->_pClass, *TreePageOf(sheet));
	}
	return {};
}

int RowHeightFor(AbilitySheet sheet)
{
	return SpellRowHeight;
}

// ---------------------------------------------------------------------------------------------
// The Paladin tree's grid. Three columns by six tiers, laid out from each skill's own (tier,
// column) rather than from its position in a list - the pages are sparse (a tier may hold one
// skill or three) and a running index would close those gaps and destroy the shape.
// ---------------------------------------------------------------------------------------------

constexpr int TreeIconSize = 56;
constexpr int TreeColumns = 3;
constexpr int TreeColPitch = 100;
/** Centres the three columns in the content width. */
constexpr int TreeColX0 = (AbilitiesContentRightLimit - (TreeColumns - 1) * TreeColPitch - TreeIconSize) / 2;
/** The point counter under each icon, which doubles as the invest button. */
constexpr int TreeBarHeight = 16;
constexpr int TreeBarGap = 4;
/**
 * @brief Air between one tier's counter and the next tier's icon.
 *
 * 6, down from 12 (2026-08-16). The nav row cost the list 26px, and at the old pitch a six-tier page
 * - which is every page the Paladin, Barbarian, Sorcerer and Rogue have - no longer fitted, so all
 * of them grew a scrollbar for the sake of their last row's counter. The static_assert below is the
 * real statement: a six-tier page must fit without scrolling.
 */
constexpr int TreeRowGap = 6;
constexpr int TreeRowPitch = TreeIconSize + TreeBarGap + TreeBarHeight + TreeRowGap;
static_assert(6 * TreeRowPitch <= AbilitiesContentSize.height,
    "a six-tier tree page no longer fits the list unscrolled - tighten TreeRowGap or the nav row");

Rectangle TreeIconRect(int column, int tier)
{
	return { { TreeColX0 + column * TreeColPitch, tier * TreeRowPitch }, { TreeIconSize, TreeIconSize } };
}

Rectangle TreeBarRect(int column, int tier)
{
	return { { TreeColX0 + column * TreeColPitch, tier * TreeRowPitch + TreeIconSize + TreeBarGap },
		{ TreeIconSize, TreeBarHeight } };
}

/** @brief How many rows @p sheet has right now. */
size_t GetRowCount(AbilitySheet sheet)
{
	SpellID rows[MaxSpellRows];
	switch (sheet) {
	case AbilitySheet::Spells:
		return BuildSpellRows(rows);
	case AbilitySheet::ClassTree0:
	case AbilitySheet::ClassTree1:
	case AbilitySheet::ClassTree2: {
		// A grid, not a list - the count is only used for the "is there anything here" question
		// and for scrolling, which TotalListHeight answers separately for these sheets.
		oracool::ClassTreeSkill skills[oracool::ClassTreeSkillCount];
		return oracool::BuildClassTreePage(InspectPlayer->_pClass, *TreePageOf(sheet), skills);
	}
	}
	return 0;
}

/** @brief Total height of every row on @p sheet. */
int TotalListHeight(AbilitySheet sheet)
{
	// A tree page is as tall as its deepest tier, not as tall as the tier system allows.
	//
	// This used to return TreeTiers * TreeRowPitch unconditionally, on the reasoning that a fixed
	// grid keeps rows lined up across pages - which it does, but the ROWS are placed from each
	// skill's own tier and were never affected by this number. All it decided was the scroll extent,
	// so every six-tier page (the Paladin's three, and every class but the Monk) advertised a
	// seventh, empty tier and grew a scrollbar to reach it.
	if (const std::optional<int> page = TreePageOf(sheet); page.has_value()) {
		oracool::ClassTreeSkill skills[oracool::ClassTreeSkillCount];
		const size_t count = oracool::BuildClassTreePage(InspectPlayer->_pClass, *page, skills);
		int deepest = -1;
		for (size_t i = 0; i < count; i++)
			deepest = std::max(deepest, oracool::GetClassTreeSkillData(skills[i]).tier);
		return (deepest + 1) * TreeRowPitch;
	}
	// Spells is the only list sheet left, and it is a uniform stride.
	return static_cast<int>(GetRowCount(sheet)) * RowHeightFor(sheet);
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

/**
 * @brief Screen rect of the sheet-cycling arrow. @p direction is -1 for left, +1 for right.
 *
 * In the nav row, not the title band (2026-08-16). The right-hand arrow used to share the band with
 * the right-aligned points count, which drew straight over it - "Points: 3" rendered as "Points>3"
 * with the colon swallowed. Down here each arrow has an end of the row to itself.
 */
Rectangle GetArrowRect(int direction)
{
	// In the TITLE band (user, 2026-08-17: "Arrows for prev/next ability window to be at the title
	// row"), which is what let the nav row die and the content start 26px higher. Pushed to the
	// band's extreme ends - inside the ornate border, outside the margin the title text uses - so
	// the widest sheet names ("OFFENSIVE AURAS", drawn centred at FontSize30) cannot reach them.
	constexpr int ArrowEdgeInset = 10;
	const Rectangle panel = GetSpellBookPanelRect();
	const int cx = direction < 0
	    ? panel.position.x + ArrowEdgeInset + ArrowHitSize.width / 2
	    : panel.position.x + AbilitiesPanelSize.width - ArrowEdgeInset - ArrowHitSize.width / 2;
	const int cy = panel.position.y + oracool::PanelTitleTop + oracool::PanelTitleHeight / 2;
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
 * A property of the LIST rather than of whichever icon a row happens to hold, so rows starting from
 * different icon sources still line their text up. Only one source is left - Spells is the only list
 * sheet - but the seam stays, because it cost nothing and the sheet has had two sources before.
 */
int RowIconColumnWidth()
{
	return GetSmallSpellIconSize().width;
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

// Phase 2.1: the invest control. A small gold "+" at the row's right edge, shown only while the
// local player has an unspent point this row can take - so the sheet is quiet until level-up hands
// out a point, and quiet again once it is sunk. The invested count sits beside it permanently.
constexpr Size InvestButtonSize { 22, 22 };
constexpr int InvestButtonRightPad = 4;
/** @brief The sunk-points readout that sits left of the button. */
constexpr int InvestCountWidth = 30;
constexpr int InvestCountGap = 4;
/** @brief [count][gap][+] - moved as one, so the two can never drift apart. */
constexpr int InvestGroupWidth = InvestCountWidth + InvestCountGap + InvestButtonSize.width;

/**
 * @brief Left edge of the [count][+] group: the row's right edge, tucked against the scrollbar.
 *
 * This briefly special-cased the Skills sheet, whose icon-only rows left 200px of nothing between
 * the icon and a control parked at that edge. That sheet is gone, and Spells - the only list left -
 * has text all the way across, so the plain rule is correct again for every row that reaches here.
 */
int InvestGroupLeft()
{
	return AbilitiesContentRightLimit - InvestGroupWidth - InvestButtonRightPad;
}

Rectangle InvestButtonRect(int top, int rowHeight)
{
	return { { InvestGroupLeft() + InvestCountWidth + InvestCountGap,
		         top + (rowHeight - InvestButtonSize.height) / 2 },
		InvestButtonSize };
}

bool InvestZoneClicked(int localX)
{
	const int buttonLeft = InvestGroupLeft() + InvestCountWidth + InvestCountGap;
	return localX >= buttonLeft && localX < buttonLeft + InvestButtonSize.width;
}

void DrawInvestControls(const Surface &content, int top, int rowHeight, SpellID sn)
{
	if (IsInspectingPlayer())
		return;
	const Player &player = *MyPlayer;
	if (sn == SpellID::Invalid)
		return;
	const int left = InvestGroupLeft();
	const int invested = player._pSkillInvestment[static_cast<size_t>(sn)];
	if (invested > 0) {
		// The sunk points, said plainly and always - the "+" comes and goes with the unspent pool,
		// but what a skill has already been fed is permanent information.
		DrawString(content, fmt::format("+{:d}", invested),
		    { { left, top }, { InvestCountWidth, rowHeight } },
		    { UiFlags::ColorWhitegold | UiFlags::AlignRight | UiFlags::VerticalCenter });
	}
	if (!oracool::CanInvestSkillPoint(player, sn))
		return;
	const Rectangle button = InvestButtonRect(top, rowHeight);
	oracool::DrawHoverOutline(content, button);
	DrawString(content, "+", button,
	    { UiFlags::ColorWhitegold | UiFlags::FontSize24 | UiFlags::AlignCenter | UiFlags::VerticalCenter });
}

void DrawFKeyBadge(const Surface &out, Rectangle iconRect, SpellID sn); // defined with the tree helpers below

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
		const Rectangle iconRect { { iconPos.x, iconPos.y - iconSize.height + 1 }, iconSize };
		DrawAssignmentRings(content, iconRect, sn, GetSBookTrans(sn, true));
		DrawFKeyBadge(content, iconRect, sn);
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


/**
 * @brief The invisible hit box for a spend glyph - 13x13 (user, 2026-08-17).
 *
 * "Invisible frame" is exact: the box is the CLICK target and the glyph is drawn inside it with a
 * margin, so the bars are 4-5px thick as asked without the target shrinking to the bars themselves.
 */
constexpr int SpendBoxSize = 13;
/** @brief Thickness of the plus/minus bars. */
constexpr int SpendBarThickness = 4;

/** @brief Bottom-RIGHT of the icon: spend a point here. */
Rectangle SpendPlusRect(Rectangle icon)
{
	return { { icon.position.x + TreeIconSize - SpendBoxSize,
		         icon.position.y + TreeIconSize - SpendBoxSize },
		{ SpendBoxSize, SpendBoxSize } };
}

/** @brief Bottom-LEFT of the icon: take one back. */
Rectangle SpendMinusRect(Rectangle icon)
{
	return { { icon.position.x, icon.position.y + TreeIconSize - SpendBoxSize },
		{ SpendBoxSize, SpendBoxSize } };
}

/**
 * @brief Whether the cursor is inside @p box, which is in CONTENT-local coordinates.
 *
 * The spend boxes are drawn from the content surface, whose origin is the panel's, so the mouse has
 * to come the same way down rather than being compared in screen space.
 */
bool SpendBoxHovered(Rectangle box)
{
	const Rectangle content = GetSpellBookContentRect();
	return box.contains(MousePosition.x - content.position.x, MousePosition.y - content.position.y);
}

/** @brief A thick plus (green) or minus (red), centred in its 13x13 box. */
void DrawSpendGlyph(const Surface &out, Rectangle box, bool plus)
{
	// 1px black frame on hover (user, 2026-08-17; thinned from 2px the same day - "make the outline
	// 1px thick"). Drawn FIRST so the glyph's own bars sit on top of it rather than being clipped by
	// it - the box is only 13px and the bars are 4.
	if (SpendBoxHovered(box)) {
		constexpr uint8_t Black = 0;
		DrawHorizontalLine(out, box.position, box.size.width, Black);
		DrawHorizontalLine(out, { box.position.x, box.position.y + box.size.height - 1 }, box.size.width, Black);
		DrawVerticalLine(out, { box.position.x, box.position.y + 1 }, box.size.height - 2, Black);
		DrawVerticalLine(out, { box.position.x + box.size.width - 1, box.position.y + 1 }, box.size.height - 2, Black);
	}

	// PAL8_YELLOW is the injected green ramp (see oracool::SkillPlateTint), so the plus reads in the
	// same green the sheets already wear; the minus takes the game's own red.
	const uint8_t color = plus ? static_cast<uint8_t>(PAL8_YELLOW + 1) : static_cast<uint8_t>(PAL16_RED + 4);
	const int len = box.size.width - 2;
	const int mid = (box.size.width - SpendBarThickness) / 2;
	// The crossbar of both glyphs.
	FillRect(out, box.position.x + 1, box.position.y + mid, len, SpendBarThickness, color);
	if (plus)
		FillRect(out, box.position.x + mid, box.position.y + 1, SpendBarThickness, len, color);
}

/**
 * @brief The castable ability under the cursor in this window, refreshed by DrawHoverFeedback
 * every frame the window draws. What an F-key press binds to.
 */
SpellID HoveredAbilitySpell = SpellID::Invalid;

/** @brief Which of the six F-keys @p sn is bound to (1-6), or 0. The lowest slot wins a double. */
int AssignedFKeyNumber(SpellID sn)
{
	for (size_t i = 0; i < AbilityFKeyCount; i++) {
		if (MyPlayer->_pSplHotKey[i] == sn)
			return static_cast<int>(i) + 1;
	}
	return 0;
}

/**
 * @brief The "F1".."F6" badge in a 13x13 invisible frame at the icon's top-RIGHT corner (user,
 * 2026-08-17: "a Font 12 'FX' 13x13 invisible frame box be put in top right corner").
 */
void DrawFKeyBadge(const Surface &out, Rectangle iconRect, SpellID sn)
{
	const int fkey = AssignedFKeyNumber(sn);
	if (fkey == 0)
		return;
	const Rectangle box { { iconRect.position.x + iconRect.size.width - SpendBoxSize, iconRect.position.y },
		{ SpendBoxSize, SpendBoxSize } };
	DrawString(out, fmt::format("F{:d}", fkey), box,
	    { UiFlags::ColorWhite | UiFlags::FontSize12 | UiFlags::AlignCenter | UiFlags::VerticalCenter });
}

/**
 * @brief One cell of a tree page: the icon, and under it the point counter that doubles as the
 * invest button.
 *
 * The counter is always drawn, so a page reads as a spend sheet even before the first point; it
 * turns gold and grows a "+" only while the character actually holds a point this cell can take.
 * The burning aura keeps the gold ring on its icon.
 */
void DrawTreeCell(const Surface &content, oracool::ClassTreeSkill skill, int scroll)
{
	const oracool::ClassTreeSkillData &data = oracool::GetClassTreeSkillData(skill);
	const Player &player = *InspectPlayer;
	const bool unlocked = oracool::IsClassTreeSkillUnlocked(player, skill);
	const int invested = oracool::ClassTreeInvestment(player, skill);

	Rectangle icon = TreeIconRect(data.column, data.tier);
	icon.position.y -= scroll;
	Rectangle bar = TreeBarRect(data.column, data.tier);
	bar.position.y -= scroll;

	// An unbuilt skill is drawn like a locked one even at level: "listed but inert" and "not yet
	// earned" are both "you cannot use this", and a bright icon that does nothing is the lie.
	//
	// Three states, not two (user, 2026-08-17: "Unlocked skills with 0 points in them are unavailable
	// and inactive, ergo need to have red background, not green"). Green now means the skill has
	// something in it; red means it is yours to fill and empty; grey means it is not yours yet.
	const bool usable = unlocked && data.implemented;
	const oracool::SkillPlateTint tint = !usable
	    ? oracool::SkillPlateTint::Grey
	    : (invested > 0 ? oracool::SkillPlateTint::Green : oracool::SkillPlateTint::Red);
	oracool::DrawClassTreeIcon(content, icon, player._pClass, oracool::ClassTreeIconIndex(skill),
	    usable, tint);

	if (data.kind == oracool::ClassTreeKind::Aura) {
		if (oracool::GetActiveClassAura(player) == skill)
			oracool::DrawHoverOutline(content, icon);
	} else if (const SpellID slot = oracool::ClassTreeSpellId(skill); IsValidSpell(slot)) {
		DrawAssignmentRings(content, icon, slot, GetSBookTrans(slot, true));
		DrawFKeyBadge(content, icon, slot);
	}

	// The gold "+ 0" bar under each icon is gone (user, 2026-08-17: "Remove the gold +0 buttons below
	// the skills"). Spending now happens on the icon itself: a green plus at the bottom-right when
	// there is a point to put in, a red minus at the bottom-left when there is a rank to take back.
	// The rank sits between them, on the icon's bottom edge, so the whole cell is one object instead
	// of an icon plus a widget - which also gives the sheets back a row of height.
	if (IsInspectingPlayer())
		return;
	const Player &me = *MyPlayer;
	if (invested > 0) {
		DrawString(content, fmt::format("{:d}", invested),
		    { { icon.position.x + SpendBoxSize, icon.position.y + TreeIconSize - SpendBoxSize },
		        { TreeIconSize - 2 * SpendBoxSize, SpendBoxSize } },
		    { UiFlags::ColorWhitegold | UiFlags::AlignCenter | UiFlags::VerticalCenter });
	}
	if (oracool::CanInvestClassTreePoint(me, skill))
		DrawSpendGlyph(content, SpendPlusRect(icon), true);
	if (oracool::CanRefundClassTreePoint(me, skill))
		DrawSpendGlyph(content, SpendMinusRect(icon), false);
}

/** @brief Draws a whole tree page. */
void DrawTreePage(const Surface &content, int page, int scroll)
{
	oracool::ClassTreeSkill skills[oracool::ClassTreeSkillCount];
	const size_t count = oracool::BuildClassTreePage(InspectPlayer->_pClass, page, skills);
	for (size_t i = 0; i < count; i++)
		DrawTreeCell(content, skills[i], scroll);
}

/**
 * @brief The tree cell under @p localPoint (content-local, scroll already added), or nullopt.
 * @param onBar set when the point landed on the cell's counter rather than its icon.
 */
std::optional<oracool::ClassTreeSkill> TreeCellAt(int page, Point localPoint, bool &onBar)
{
	oracool::ClassTreeSkill skills[oracool::ClassTreeSkillCount];
	const size_t count = oracool::BuildClassTreePage(InspectPlayer->_pClass, page, skills);
	for (size_t i = 0; i < count; i++) {
		const oracool::ClassTreeSkillData &data = oracool::GetClassTreeSkillData(skills[i]);
		if (TreeIconRect(data.column, data.tier).contains(localPoint)) {
			onBar = false;
			return skills[i];
		}
		if (TreeBarRect(data.column, data.tier).contains(localPoint)) {
			onBar = true;
			return skills[i];
		}
	}
	return std::nullopt;
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

bool HandleAbilityFKey(size_t slot, bool shift)
{
	if (slot >= AbilityFKeyCount)
		return false;
	Player &me = *MyPlayer;

	// With the window open, an F-key EDITS bindings rather than using them (user, 2026-08-17:
	// "Assigning hotkeys is by clicking F1-F6 while hovering over a skill/spell. SHIFT+Hotkey
	// unassignes"). The key is consumed even when nothing is hovered - a bind key that fell
	// through to casting mid-edit would be worse than one that does nothing.
	if (sbookflag && !IsInspectingPlayer()) {
		const SpellID spell = HoveredAbilitySpell;
		if (!IsValidSpell(spell))
			return true;
		if (shift || me._pSplHotKey[slot] == spell) {
			// SHIFT clears; pressing a key on the ability that already holds it also clears,
			// which is the vanilla speedbook's own toggle and costs nothing to keep.
			if (me._pSplHotKey[slot] == spell) {
				me._pSplHotKey[slot] = SpellID::Invalid;
				RedrawEverything();
			}
			return true;
		}
		me._pSplHotKey[slot] = spell;
		me._pSplTHotKey[slot] = GetSBookTrans(spell, false);
		RedrawEverything();
		return true;
	}

	// In play: the vanilla quick-spell path, which readies the bound ability on the right button
	// (or casts outright under quickCast). Slots 0-5 ARE F1-F6 - one array, one meaning.
	ToggleSpell(slot);
	return true;
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
	// Refreshed every frame BEFORE the early-outs, so leaving the rows also clears the F-key
	// binding target rather than leaving it on the last row touched.
	HoveredAbilitySpell = SpellID::Invalid;

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

	// The tree pages hit-test against their grid rather than against a row stride.
	if (const std::optional<int> page = TreePageOf(CurrentSheet); page.has_value()) {
		bool onBar = false;
		const Point local { MousePosition.x - contentRect.position.x, y };
		const std::optional<oracool::ClassTreeSkill> hovered = TreeCellAt(*page, local, onBar);
		if (!hovered.has_value())
			return;
		const oracool::ClassTreeSkillData &data = oracool::GetClassTreeSkillData(*hovered);
		const Rectangle cell = TreeIconRect(data.column, data.tier);
		// A tree cell is an F-key target when it is a real, buildable, KNOWN ability - the same
		// gate the click-to-ready path applies.
		if (data.implemented) {
			if (const SpellID slot = oracool::ClassTreeSpellId(*hovered);
			    IsValidSpell(slot) && IsSpellKnown(slot))
				HoveredAbilitySpell = slot;
		}
		PendingHoverTitle = _(data.name);
		PendingHoverText = std::string(_(data.description)) + "\n\n"
		    + oracool::ClassTreeEffectLine(*InspectPlayer, *hovered);
		PendingHoverAnchor = { { contentRect.position.x, contentRect.position.y + cell.position.y - scroll },
			{ AbilitiesContentRightLimit, cell.size.height } };
		HasPendingHover = true;
		oracool::DrawHoverOutline(content, { { cell.position.x, cell.position.y - scroll }, cell.size });
		return;
	}

	// Everything past the tree is the Spells list, a uniform stride.
	{
		const int height = RowHeightFor(CurrentSheet);
		const size_t index = static_cast<size_t>(y / height);
		SpellID rows[MaxSpellRows];
		const size_t rowCount = BuildSpellRows(rows);
		if (index < rowCount) {
			rowTop = static_cast<int>(index) * height;
			rowHeight = height;
			found = true;
			if (IsSpellKnown(rows[index]))
				HoveredAbilitySpell = rows[index];
			title = oracool::GetSpellDisplayName(rows[index]); // already translated
			description = spellInfo(rows[index]);
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
	// Oracool (2026-08-16): the shared painted side-panel background - see quests.cpp for the note.
	// This was the sixth and last themed window still on the half-transparent procedural fill, which
	// is why the town showed through it in every screenshot the user sent. The fill and bevel stay as
	// the fallback so the art remains droppable rather than required.
	//
	// The rule under the title went with it, as it did in the other five: the art brings its own
	// header framing, so the separator was a second line drawn across the first.
	if (oracool::HasSidePanelArt()) {
		oracool::DrawSidePanelArt(out, panel.position);
	} else {
		oracool::DrawThemedFill(out, panel);
		oracool::DrawOrnateBorder(out, panel);
	}

	// The title has the band to itself, at the same PanelTitleTop every other side panel uses.
	const Rectangle labelArea { { panel.position.x + AbilitiesMargin, panel.position.y + oracool::PanelTitleTop },
		{ panel.size.width - 2 * AbilitiesMargin, oracool::PanelTitleHeight } };
	oracool::DrawOutlinedString(out, GetSheetTitle(CurrentSheet), labelArea,
	    UiFlags::ColorWhitegold | UiFlags::FontSize30 | UiFlags::AlignCenter | UiFlags::VerticalCenter);

	// No nav row and no "Points:" readout here any more (2026-08-17): the arrows live at the ends of
	// the title band above, and the unspent pool is a HUD element now - the frame above the RMB well,
	// where it is visible while playing rather than only with this window open. See
	// DrawUnspentPointsFrame in control.cpp.
	// Only when there is somewhere to go - arrows on a window that cannot turn are a control that
	// lies. Every class has at least Spells and Skills today so this is always true, but it was not
	// while Spells was the Sorcerer's alone (a Rogue was down to Skills on its own), and the guard
	// costs nothing.
	if (AvailableSheetCount() > 1) {
		DrawArrow(out, -1);
		DrawArrow(out, 1);
	}

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

	if (const std::optional<int> page = TreePageOf(CurrentSheet); page.has_value()) {
		DrawTreePage(content, *page, scroll);
		return;
	}

	// Spells: the one list sheet left, a uniform stride.
	SpellID rows[MaxSpellRows];
	const size_t rowCount = BuildSpellRows(rows);
	for (size_t i = 0; i < rowCount; i++) {
		const int top = static_cast<int>(i) * rowHeight - scroll;
		if (top + rowHeight <= 0 || top >= AbilitiesContentSize.height)
			continue;
		DrawSpellRow(content, i, rows[i], top);
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

	// The tree: the counter under an icon spends a point into it; the icon itself readies an active
	// skill on the clicked button, or lights an aura. Two targets rather than one gesture with a
	// modifier, so neither action can be taken by accident while reaching for the other.
	if (const std::optional<int> page = TreePageOf(CurrentSheet); page.has_value()) {
		bool onBar = false;
		const Point local { MousePosition.x - content.position.x, y };
		const std::optional<oracool::ClassTreeSkill> hit = TreeCellAt(*page, local, onBar);
		if (!hit.has_value())
			return;
		// The spend corners are tested BEFORE the icon's own action, because they sit inside the
		// icon's rect: a click in the bottom-left is a refund, not a "ready this skill".
		{
			Rectangle cellIcon = TreeIconRect(oracool::GetClassTreeSkillData(*hit).column,
			    oracool::GetClassTreeSkillData(*hit).tier);
			const bool spent = SpendPlusRect(cellIcon).contains(local)
			        && oracool::CanInvestClassTreePoint(*MyPlayer, *hit)
			    ? oracool::InvestClassTreePoint(*MyPlayer, *hit)
			    : (SpendMinusRect(cellIcon).contains(local)
			              && oracool::CanRefundClassTreePoint(*MyPlayer, *hit)
			            ? oracool::RefundClassTreePoint(*MyPlayer, *hit)
			            : false);
			if (spent) {
				// The whole of "make it take effect": the aura provider and every ladder read the
				// investment on the next totals walk.
				CalcPlrInv(*MyPlayer, false);
				RedrawEverything();
				return;
			}
		}
		if (onBar)
			return; // the counter row is inert now; the corners above are the spend controls
		const oracool::ClassTreeSkillData &data = oracool::GetClassTreeSkillData(*hit);
		if (data.kind == oracool::ClassTreeKind::Aura) {
			if (oracool::ToggleClassAura(*MyPlayer, *hit)) {
				CalcPlrInv(*MyPlayer, false);
				RedrawEverything();
			}
			return;
		}
		// An active skill readies exactly as it would on any other sheet: unbuilt and locked rows
		// are inert, and the button that clicked it is the button it lands on.
		if (!data.implemented || !IsValidSpell(oracool::ClassTreeSpellId(*hit))
		    || !oracool::IsClassTreeSkillUnlocked(*MyPlayer, *hit))
			return;
		Player &treePlayer = *MyPlayer;
		const SpellID treeSpell = oracool::ClassTreeSpellId(*hit);
		const SpellType treeType = (treePlayer._pAblSpells & GetSpellBitmask(treeSpell)) != 0
		    ? SpellType::Skill
		    : SpellType::Spell;
		if (assignToRightButton) {
			treePlayer._pRSpell = treeSpell;
			treePlayer._pRSplType = treeType;
		} else {
			treePlayer._pLRSpell = treeSpell;
			treePlayer._pLRSplType = treeType;
		}
		RedrawEverything();
		return;
	}
	// Spells: the one list sheet left.
	SpellID sn = SpellID::Invalid;
	{
		SpellID rows[MaxSpellRows];
		const size_t rowCount = BuildSpellRows(rows);
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
