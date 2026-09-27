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
#include "oracool/skill_facts.h"
#include "oracool/badge.h"
#include "oracool/auto_save.h"
#include "oracool/class_tree.h"
#include "oracool/rage.h"
#include "oracool/skill_points.h"
#include "oracool/spell_ranks.h"
#include "oracool/spell_descriptions.h"
#include "oracool/furious_charge.h"
#include "oracool/grid_bezel.h"
#include "oracool/hud_art.h"
#include "oracool/oracool.h"
#include "oracool/ornate_border.h"
#include "oracool/skill_picker.h" // the quick lists bind F-keys too
#include "oracool/ui_sound.h"
#include "panels/spell_icons.hpp"
#include "panels/ui_panels.hpp"
#include "player.h"
#include "spelldat.h"
#include "plrmsg.h" // EventPlrMsg - a locked row says why
#include "spells.h" // ClearReadiedSpell
#include "utils/display.h"
#include "utils/language.h"
#include "utils/stdcompat/optional.hpp"
#include "utils/str_cat.hpp"

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
	// The sort key is now the LEVEL BAND (user, 2026-08-19: "divide them into 6,12,18,24,30 groups as
	// skills and sort them alphabetically in each group and sort groups in ascending order"), not the
	// Magic requirement it used to be. Same shape as the class trees' tiers, which is the point: a
	// player reading either window learns the same progression.
	std::sort(rows, rows + count, [&isImplemented](SpellID a, SpellID b) {
		const bool liveA = isImplemented(a);
		const bool liveB = isImplemented(b);
		if (liveA != liveB)
			return liveA;
		const int bandA = oracool::SpellRequiredLevel(a);
		const int bandB = oracool::SpellRequiredLevel(b);
		if (bandA != bandB)
			return bandA < bandB;
		return oracool::GetSpellDisplayName(a) < oracool::GetSpellDisplayName(b);
	});
	return count;
}

// Oracool V1: the shared theme and geometry - the same 340x720 window as the waypoint list, quest
// log, character sheet, stash and inventory.
//
//   0..23      top margin and the canvas's top bezel
//   23..53     the tab row: five plates painted into the canvas, one per sheet (2026-09-12)
//   57..95     title band, the sheet's name - below the buttons, not on the shared PanelTitleTop
//   95..101    the painted background's arch shoulders - ornament, nothing may be drawn here
//   101..624   content area - the scrolling list, ending at oracool::SidePanelContentBottom
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
 * @brief The five tab plates painted into ui/abilities_panel.png (user, 2026-09-12: "This canvas
 * has prebuilt 5 buttons").
 *
 * MEASURED off the artwork rather than spaced by arithmetic, because the plates are painted and an
 * even pitch would drift off them: the gold bevels run x=22..80, 82..140, 142..201, 203..261 and
 * 263..317, so the widths are 59, 59, 60, 59 and 55 - the last one narrower where the right bezel
 * crowds it. Every plate spans y=23..53, with its interior inside the bevel at y=26..51.
 *
 * The x/width pair per plate lives in TabPlates, below, where the sheet enum exists to pair it with.
 */
constexpr int TabRowTop = 23;
constexpr int TabRowHeight = 31;
/** @brief Inset from a plate's edge to its interior, so an overlay leaves the gold bevel alone. */
constexpr int TabPlateBevel = 3;

/**
 * @brief The sheet title, UNDER the tab row (user, 2026-09-12: "move the Titles under the button
 * row").
 *
 * This window cannot use the shared oracool::PanelTitleTop any more. That constant is 28, which put
 * every window's title on one line - but on this canvas y=28 is inside the painted tab plates, so
 * the title would have been drawn straight through the five buttons. The other five windows keep
 * the shared line; only this one has a header row to clear.
 */
constexpr int AbilitiesTitleTop = TabRowTop + TabRowHeight + 3;
constexpr int AbilitiesTitleHeight = oracool::PanelTitleHeight;

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
static_assert(AbilitiesContentTop >= AbilitiesTitleTop + AbilitiesTitleHeight,
    "the list starts inside the title band");
static_assert(AbilitiesTitleTop >= TabRowTop + TabRowHeight,
    "the title is drawn over the canvas's tab plates");
static_assert(AbilitiesContentSize.height > 0, "the nav row has eaten the whole list");

/**
 * @brief Air at the top of every sheet, so the first row's carved frame is inside the viewport.
 *
 * User, 2026-09-03: "top row in abilities windows wont show top border of 2x2 slot icon. fix it.
 * increase the rendering area or move all row down a bit."
 *
 * The frames arrived the day before and they are drawn OUTSIDE the icon - GridBezelInset past it on
 * every side, which is what makes a 56px icon wear a 68px plate. The list's first row sits at y=0,
 * so its plate wanted to be drawn at y=-6, and the content is a subregion: everything above zero is
 * simply not in the surface. Every other row had the previous row's air to sit in; the top one had
 * the panel.
 *
 * Moving the rows down was chosen over growing the viewport upward because the six pixels above this
 * list are not free - the nav row with the page arrows is there, and a frame drawn into it would
 * exchange a clipped border for an overlapped one.
 *
 * It is exactly GridBezelInset rather than a hand-picked number, so it is the same six the frame
 * itself uses. If the bezel art is ever recut with a wider surround, both move together.
 */
constexpr int AbilitiesListTop = oracool::GridBezelInset;

/**
 * @brief The icon square every sheet in this window draws, list rows and tree cells alike.
 *
 * ONE number (user, 2026-08-19: "make spell list use 56x56(57)px icons and backgrounds and put on
 * top the minus, the plus and the lvl number at the bottom of the icons"). The tree has been at 56
 * since it was built; the Spells list was at the engine's 37x38, which is why the two sheets felt
 * like different windows and why a spell had no room for the spend corners a tree cell wears.
 */
constexpr int SheetIconSize = 56;

/**
 * @brief The invisible hit box for a spend glyph - 13x13 (user, 2026-08-17).
 *
 * "Invisible frame" is exact: the box is the CLICK target and the glyph is drawn inside it with a
 * margin, so the bars are 4-5px thick as asked without the target shrinking to the bars themselves.
 *
 * Up here with the icon size because both sheets place their corners from it.
 */
constexpr int SpendBoxSize = 13;

/**
 * @brief The outline on a cell that will accept a point right now.
 *
 * Near the LIGHT end of the gold ramp - PAL16 runs light to dark as the offset grows, and the
 * window's own frame sits at +9 - so an eligible cell reads as lit rather than merely outlined, and
 * cannot be mistaken for the chrome around it. This is what replaced the green plus glyph.
 */
constexpr uint8_t EligibleForPointColor = PAL16_YELLOW + 2;

/**
 * @brief A spell or skill row: the icon square with a little air above and below it.
 *
 * Also has to hold THREE text lines since v1.9.116 - name, mana, damage (user, 2026-08-30) - so it
 * is the taller of the two demands rather than the icon's alone. At 56 + 8 the icon already wins
 * (64 against 54), which is why no row got taller and nothing else in this file had to move; the
 * max is here so that a smaller icon later cannot silently clip the third line.
 */
// + 18, not + 8 (user, 2026-09-05: "introduce 6px gaps between spells in spells windows"): the bezel
// is six outside the icon on every side, so a row is the icon, twelve of bezel and six of air.
constexpr int SpellRowHeight = SheetIconSize + 2 * oracool::GridBezelInset + 6;
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
// A spell row carries THREE lines since v1.9.116 - name, mana, damage (user, 2026-08-30). The icon
// is the taller demand at 64 against 54, so no row grew and nothing else in this file moved; this
// asserts that stays true, because a smaller icon later would clip the damage line silently.
static_assert(SpellRowHeight >= 3 * AbilitiesLineHeight,
    "A spell row can no longer fit its three text lines - raise SpellRowHeight");
/** @brief Air above the name line, and the same below the last description line. */
constexpr int DescribedRowPadding = 6;
/** @brief A described row (aura, Barbarian skill): a name line plus the wrapped description. */
constexpr int DescribedRowHeight = 2 * DescribedRowPadding
    + AbilitiesLineHeight * (1 + DescribedRowDescLines);
/**
 * @brief The painted background's INTERIOR, measured from the art (user, 2026-08-17: "The left
 * column of skill is still partially sitting on top of the bezel... I want to avoid having assets
 * over the bezel").
 *
 * Measured off ui/stash_background.png by column brightness/variance over the content band: the
 * left bezel's ornament ridges run out to x=44 and the flat interior fill starts at 46; the right
 * interior ends at 292 with the ornament lip from 296. Every derived margin before this (24, 30,
 * 31) was an eyeballed number that landed ON the bezel's inner lip - which is exactly what the
 * user kept seeing. Rows, tree columns and hover outlines all stay inside these two.
 */
constexpr int AbilitiesInteriorLeft = 46;
constexpr int AbilitiesInteriorRight = 292;

constexpr int AbilitiesIconX = AbilitiesInteriorLeft;
/** @brief Between the icon's right edge and the text column. */
constexpr int AbilitiesTextGap = 10;
constexpr int AbilitiesScrollbarWidth = oracool::OrnateBorderWidth;
constexpr int AbilitiesScrollbarMinThumb = 24;
constexpr int AbilitiesScrollbarGap = 6;
constexpr int AbilitiesRightPad = 8;
/** @brief Right edge available to a row - short of the scrollbar, not of the panel. */
// The measured interior edge, no longer derived from the scrollbar: the old formula
// (width - pad - scrollbar - gap = 318) reached 26px onto the right bezel, the same class of
// mistake as the left margin. The scrollbar itself stays where it was, right of this limit, over
// the frame - where the stash has always drawn its own.
constexpr int AbilitiesContentRightLimit = AbilitiesInteriorRight;

// The sheet-cycling arrows are GONE (user, 2026-09-12: "remove the current nav arrows"). They were
// two solid triangles drawn from primitives at the ends of the title band, and cycling was the only
// way to reach a sheet - so finding the Passives page meant stepping through the tree pages to get
// there. The canvas's five tab plates replaced them with one click per sheet, which is also why the
// title could move down into the space the arrows used to share.

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
	/** The Diablo III-shaped fourth page: always-on passives, one rank each. */
	ClassTree3,
	LAST = ClassTree3,
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
 * @brief Which sheet each painted plate opens, left to right.
 *
 * The canvas labels them 1, 2, 3, P, S: the three class-tree pages, the Passives page, then the
 * book of Spells. That is the ARTWORK's order, not the enum's, which is the whole reason this table
 * exists - Spells is enum 0 and sits last on the canvas.
 */
struct TabPlate {
	int x;
	int width;
	AbilitySheet sheet;
	/**
	 * The glyph the canvas paints on this plate. Repeated here ONLY for the no-art fallback, which
	 * draws its own boxes and has to letter them - with the canvas present these are never drawn,
	 * because the artwork already carries them.
	 */
	const char *label;
};
constexpr TabPlate TabPlates[] = {
	{ 22, 59, AbilitySheet::ClassTree0, "1" },
	{ 82, 59, AbilitySheet::ClassTree1, "2" },
	{ 142, 60, AbilitySheet::ClassTree2, "3" },
	{ 203, 59, AbilitySheet::ClassTree3, "P" },
	{ 263, 55, AbilitySheet::Spells, "S" },
};
static_assert(sizeof(TabPlates) / sizeof(TabPlates[0]) == AbilitySheetCount,
    "one plate per sheet - a sheet with no plate could not be reached now the arrows are gone");

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
	case AbilitySheet::ClassTree3:
		return 3;
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
/** @brief Index of the tab plate being held down, or -1. Purely so it can be drawn pressed. */
int PressedTab = -1;

/**
 * @brief The ICON BUTTONS' click and hover effects - the ones the waypoint list's Act buttons got
 * first (user, 2026-09-20: "i like these move and sound effects we just applied on the Act buttons.
 * Apply them on all buttons in the abilities windows, regardless if clicking on a button actually
 * produces any meaningful effect", then narrowed to "the skills/spells/auras buttons. dont apply on
 * the navigation buttons yet"): a tree cell (skill or aura), a passive slot or a spell row's icon
 * sinks 2px down and 2px left from the press until the mouse is released, its shadow staying put;
 * titlemov.wav plays on every hover ENTRY and on every click, whatever the click then does. The
 * tab plates keep their own press wash and are not part of this.
 *
 * The click sounds that used to sit on the individual outcomes in CheckSBook (move on an emptying,
 * select on a slotting or a readying) are folded into the one press sound, or a click would ring
 * twice; a skill's own learn cue inside InvestClassTreePoint is not a click sound and stays.
 */
enum class IconButtonKind : uint8_t {
	None,
	TreeCell,    // id = the ClassTreeSkill
	PassiveSlot, // id = the slot
	SpellRow,    // id = the row index on the Spells sheet
};
struct IconButton {
	IconButtonKind kind = IconButtonKind::None;
	int id = -1;
	bool operator==(const IconButton &other) const { return kind == other.kind && id == other.id; }
	bool operator!=(const IconButton &other) const { return !(*this == other); }
};
IconButton PressedIcon;
/** @brief What the cursor is over THIS frame (set by DrawHoverFeedback) and was over last frame. */
IconButton FrameHoverIcon;
IconButton LastHoverIcon;
constexpr Displacement IconPressSink { -2, 2 };
/** @brief Points a Shift-click on a tree cell moves at once; Ctrl moves them all (2026-09-27). */
constexpr int ShiftClickPoints = 5;

/** @brief Whether @p button is the one held down - its face sunk, its shadow a pixel smaller all round. */
bool IsIconPressed(IconButton button)
{
	return PressedIcon.kind != IconButtonKind::None && PressedIcon == button;
}

/** @brief The sink for @p button's face: the press offset while it is the one held down, else none. */
Displacement PressSinkFor(IconButton button)
{
	return (PressedIcon.kind != IconButtonKind::None && PressedIcon == button) ? IconPressSink : Displacement { 0, 0 };
}

/** @brief A click landed on @p button: it sinks until the release, and the click sounds. */
void PressIconButton(IconButton button)
{
	PressedIcon = button;
	oracool::PlayUiMoveSound();
}

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
	case AbilitySheet::ClassTree3:
		// Not behind ClassAbilitySheetsHidden: unlike the list it replaced, the tree is spendable
		// and its skills act, so there is nothing inert to hide.
		//
		// ClassTree3 - the Passive Skills page - IS entirely inert today, which is the one case that
		// argument does not cover. It is shown anyway because the user asked for the page as a
		// visible placeholder, and because the rows carry the same honest red X every other unbuilt
		// row in the tree wears. A page that says "not yet" is not the same as a page that lies.
		return oracool::ClassHasTree(InspectPlayer->_pClass);
	}
	return true;
}

// AvailableSheetCount lived here until 2026-09-12. Its only caller was the guard that hid the nav
// arrows on a window with nowhere to turn; the tab plates guard themselves, one plate at a time,
// through IsSheetAvailable.

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
	case AbilitySheet::ClassTree3:
		// Named by the tree, not here: each class calls its first three pages something different.
		// The fourth is "PASSIVE SKILLS" for everyone, and the tree answers that too.
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

constexpr int TreeIconSize = SheetIconSize;
constexpr int TreeColumns = 3;
// 94, down from 100 (2026-08-17): three 56px columns at pitch 100 span 256, and the measured
// interior is 246 wide - the old span could not fit without a column riding the bezel.
constexpr int TreeColPitch = 94;
/** Centres the three columns in the content width. */
// Centred within the INTERIOR, not within [0, limit] - centring from the panel's own left edge is
// what parked the first column on the bezel.
constexpr int TreeColX0 = AbilitiesInteriorLeft
    + (AbilitiesInteriorRight - AbilitiesInteriorLeft - (TreeColumns - 1) * TreeColPitch - TreeIconSize) / 2;
static_assert(TreeColX0 >= AbilitiesInteriorLeft
        && TreeColX0 + (TreeColumns - 1) * TreeColPitch + TreeIconSize <= AbilitiesInteriorRight,
    "the tree grid no longer fits the painted interior - tighten TreeColPitch");
/** The point counter under each icon, which doubles as the invest button. */
// 14, from 16 (2026-09-05): the two pixels paid for the six-pixel gap below, so a six-tier page still
// fits the list unscrolled - the static_assert under TreeRowPitch is the real statement.
constexpr int TreeBarHeight = 14;
constexpr int TreeBarGap = 4;
/**
 * @brief Air between one tier's counter and the next tier's icon.
 *
 * 6, down from 12 (2026-08-16). The nav row cost the list 26px, and at the old pitch a six-tier page
 * - which is every page the Paladin, Barbarian, Sorcerer and Rogue have - no longer fitted, so all
 * of them grew a scrollbar for the sake of their last row's counter. The static_assert below is the
 * real statement: a six-tier page must fit without scrolling.
 */
// 12 (user, 2026-09-05: "introduce 6px gaps between spells"): the frame reaches six below the icon,
// under the counter bar, so six of AIR between the bar and the next frame is a gap of twelve here.
constexpr int TreeRowGap = 12;
constexpr int TreeRowPitch = TreeIconSize + TreeBarGap + TreeBarHeight + TreeRowGap;
static_assert(AbilitiesListTop + 6 * TreeRowPitch <= AbilitiesContentSize.height,
    "a six-tier tree page no longer fits the list unscrolled - tighten TreeRowGap or the nav row");
/**
 * @brief Every page is a full three-by-six grid (user, 2026-09-12: "i want every ability tree to have
 * 3x6 skills"). The cells no skill holds are drawn as empty slots - see DrawTreePage. No row stands on
 * the seventh tier ClassTreeTierCount still allows since the two nineteenth passives moved into the
 * grid (2026-09-12).
 */
constexpr int TreeGridTiers = 6;
static_assert(TreeGridTiers <= oracool::ClassTreeTierCount, "the grid is deeper than any skill can be");

// ---------------------------------------------------------------------------------------------
// The Passive Skills page's own geometry (2026-08-25).
//
// It differs from the other three in two ways that turn out to solve each other. It has a band of
// four SLOTS above the grid, and its cells have no rank counter - because nothing on the page is
// bought, so there is no rank to count. Dropping the counter shortens every row by exactly the
// 20px the band needs, which is what lets a SEVEN-tier page carry a slot band and still fit
// unscrolled. That matters more than it sounds: with the slot-first gesture the user chose, the
// slot you are aiming at must never be scrolled off the top while you reach for the grid.
// ---------------------------------------------------------------------------------------------

/**
 * The four slots sit in a row above everything else - across the PANEL, since 2026-09-05, not the
 * painted interior. User: "spread passive slots 1-4 wider to introduce a mandatory 6px gap between
 * their vertical borders." Four 56px slots in their 6px frames with six of air between them are
 * 290 wide; the interior between the bezel ornaments is 246, which is why the band was at pitch 63
 * with the frames overlapping. So the band is centred on the panel's 340 instead and its outer
 * frames sit 21px onto the ornament either side - the one place in this window that does, and by
 * the user's own choice against the interior rule of 2026-08-17.
 */
constexpr int PassiveSlotSize = SheetIconSize;
constexpr int PassiveSlotPitch = PassiveSlotSize + 2 * oracool::GridBezelInset + 6;
constexpr int PassiveSlotX0 = (AbilitiesPanelSize.width
                                  - (static_cast<int>(oracool::PassiveSlotCount) - 1) * PassiveSlotPitch - PassiveSlotSize)
    / 2;
static_assert(PassiveSlotX0 - oracool::GridBezelInset >= AbilitiesMargin
        && PassiveSlotX0 + (static_cast<int>(oracool::PassiveSlotCount) - 1) * PassiveSlotPitch
                + PassiveSlotSize + oracool::GridBezelInset
            <= AbilitiesPanelSize.width - AbilitiesMargin,
    "the four passive slots and their frames no longer fit inside the panel margin");

/** The hint line ("Click a slot, then a passive") is drawn on the PANEL above the list since
 * 2026-09-05 (user: "move Click to select slot text above passive skill slot 1-4. at least 6 px
 * above the slots" and "there is enough room under the title, so dont move down") - see
 * DrawPassiveHintAboveList. The band stays at the top of the list, where it was. */
constexpr int PassiveHintHeight = 18;
constexpr int PassiveSlotBandTop = AbilitiesListTop;
/** The grid begins below the band - two frames and six of air, like every other gap here. */
constexpr int PassiveGridTop = PassiveSlotBandTop + PassiveSlotSize + 2 * oracool::GridBezelInset + 6;
/** No counter row, so a passive row is its icon plus air. */
// 18, from 4 (user, 2026-09-05: "introduce 6px gaps between all skills in passive skills"): twelve of
// frame and six of air between rows. Seven tiers at this pitch are 606px against a 523px list, so
// the passive page SCROLLS now, band and all - the "fits unscrolled" rule of 2026-08-25 gave way to
// the gap. The band is 56px at the top, so it is off the screen only for the last tier's rows.
constexpr int PassiveRowGap = 2 * oracool::GridBezelInset + 6;
constexpr int PassiveRowPitch = TreeIconSize + PassiveRowGap;

bool IsPassivePage(int page)
{
	return page == oracool::PassiveSkillsPage;
}

Rectangle TreeIconRect(int page, int column, int tier)
{
	if (IsPassivePage(page)) {
		return { { TreeColX0 + column * TreeColPitch, PassiveGridTop + tier * PassiveRowPitch },
			{ TreeIconSize, TreeIconSize } };
	}
	return { { TreeColX0 + column * TreeColPitch, AbilitiesListTop + tier * TreeRowPitch },
		{ TreeIconSize, TreeIconSize } };
}

Rectangle TreeBarRect(int page, int column, int tier)
{
	// The passive page has no counter at all, so it gets an empty rect rather than a hidden one -
	// Rectangle::contains is false for a zero-size rect, which makes the hit test agree with the
	// draw without either of them having to know about the other.
	if (IsPassivePage(page))
		return { { 0, 0 }, { 0, 0 } };
	return { { TreeColX0 + column * TreeColPitch, AbilitiesListTop + tier * TreeRowPitch + TreeIconSize + TreeBarGap },
		{ TreeIconSize, TreeBarHeight } };
}

/** @brief The slot under @p localPoint, or -1. Only meaningful on the passive page. */
int PassiveSlotAt(Point localPoint)
{
	for (int slot = 0; slot < static_cast<int>(oracool::PassiveSlotCount); slot++) {
		const Rectangle rect { { PassiveSlotX0 + slot * PassiveSlotPitch, AbilitiesListTop },
			{ PassiveSlotSize, PassiveSlotSize } };
		if (rect.contains(localPoint))
			return slot;
	}
	return -1;
}

/**
 * @brief The slot waiting to be filled, or -1.
 *
 * The user chose slot-first (2026-08-25): click a slot to arm it, then click a passive to put that
 * passive in it. File-local rather than per-player because it is a gesture in progress, not state -
 * it must not survive closing the window, changing sheet, or the character being saved.
 */
int ArmedPassiveSlot = -1;

/** @brief How many rows @p sheet has right now. */
size_t GetRowCount(AbilitySheet sheet)
{
	SpellID rows[MaxSpellRows];
	switch (sheet) {
	case AbilitySheet::Spells:
		return BuildSpellRows(rows);
	case AbilitySheet::ClassTree0:
	case AbilitySheet::ClassTree1:
	case AbilitySheet::ClassTree2:
	case AbilitySheet::ClassTree3: {
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
		// ...and never shorter than the grid, whose empty slots reach its last row (2026-09-12).
		deepest = std::max(deepest, TreeGridTiers - 1);
		if (IsPassivePage(*page))
			return PassiveGridTop + (deepest + 1) * PassiveRowPitch;
		// AbilitiesListTop is already inside PassiveGridTop, which is why only this branch adds it.
		return AbilitiesListTop + (deepest + 1) * TreeRowPitch;
	}
	// Spells is the only list sheet left, and it is a uniform stride.
	return AbilitiesListTop + static_cast<int>(GetRowCount(sheet)) * RowHeightFor(sheet);
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
	const SpellMask known = player._pMemSpells | player._pISpells | player._pAblSpells;
	return (known & GetSpellBitmask(sn)) != 0;
}

/** @brief The second line of a spell/skill row: what it costs, does, or that it is not yet known. */
std::string GetSpellDetail(SpellID sn, bool known)
{
	Player &player = *InspectPlayer;
	if (!known) {
		// An unlearned spell says WHAT IT WANTS rather than only that it is unlearned (user,
		// 2026-08-19: the level bands). Below the band the requirement is the whole story; at or
		// above it, the spell is simply waiting for a book.
		if (const int band = oracool::SpellRequiredLevel(sn); band > 0 && player._pLevel < band)
			return fmt::format(fmt::runtime(_("Requires level {:d}")), band);
		return std::string(_(/* TRANSLATORS: UI constraints, keep short please.*/ "Not learned"));
	}

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

	// "Unusable" is gone. A level-0 spell still has a cost and a damage range - the ones it would
	// have at level 1 - and showing them is the whole point of the third row (user, 2026-08-30:
	// "Damage Range at current level (level 1 if not learned yet)"). Saying only "Unusable" told the
	// player what they already knew from the greyed-out name.
	return fmt::format(fmt::runtime(pgettext("spellbook", "Mana: {:d}")), GetManaAmount(player, sn) >> 6);
}

/**
 * @brief The THIRD row: what the spell does, at the level it is actually at.
 *
 * Split out of GetSpellDetail so the row can be Name / Mana / Damage rather than a name and one
 * crowded line (user, 2026-08-30). Empty for a utility spell, which is what tells the caller to
 * draw nothing rather than an empty label.
 *
 * An UNLEARNED spell is quoted at level 1 - the numbers it would have the moment a book is read.
 * GetDamageAmt reads the player's current level for this spell, which is 0, and several formulas
 * take that as a real term; GetDamageAmtAtLevel takes the level as an argument, so it can be asked
 * about a level the character does not have yet.
 */
std::string GetSpellDamageLine(SpellID sn, bool known)
{
	const Player &player = *InspectPlayer;
	switch (GetSBookTrans(sn, false)) {
	case SpellType::Skill:
	case SpellType::Charges:
		return {}; // their own line already says what they are
	default:
		break;
	}
	if (sn == SpellID::BoneSpirit)
		return std::string(_(/* TRANSLATORS: UI constraints, keep short please.*/ "Dmg: 1/3 target hp"));

	const int level = std::max(known ? player.GetSpellLevel(sn) : 1, 1);
	int min = -1;
	int max = -1;
	GetDamageAmtAtLevel(sn, level, &min, &max);
	if (min == -1)
		return {}; // a utility spell - it has no damage to report, so it says nothing
	if (sn == SpellID::Healing || sn == SpellID::HealOther)
		return fmt::format(fmt::runtime(_("Heals: {:d} - {:d}")), min, max);
	return fmt::format(fmt::runtime(_("Damage: {:d} - {:d}")), min, max);
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
 * @brief Screen rect of tab plate @p index.
 *
 * The plates are painted into the canvas, so this is a hit box over artwork rather than a button the
 * code draws - which is why the numbers are measured (see TabPlates) and not derived from a pitch.
 */
Rectangle GetTabPlateRect(size_t index)
{
	const Rectangle panel = GetSpellBookPanelRect();
	const TabPlate &plate = TabPlates[index];
	return { { panel.position.x + plate.x, panel.position.y + TabRowTop }, { plate.width, TabRowHeight } };
}

/**
 * @brief The five tab plates: which sheet is open, which is under the cursor, which is held down.
 *
 * The plates themselves are part of the canvas, so all three states are drawn as overlays INSIDE
 * each plate's gold bevel - TabPlateBevel in from every edge. Painting over the bevel would erase
 * the thing that makes the row read as buttons at all.
 *
 * PAL16 ramps run light to dark as the offset grows, so the pressed blend is the dark one and the
 * hover blend is the light one: a press reads as the plate sinking, a hover as it catching light.
 */
void DrawTabPlates(const Surface &out)
{
	// Without the canvas there is no painted plate to light up, and with the arrows gone that would
	// leave the window with no way between sheets at all - so the fallback draws the row itself,
	// from the same primitives the arrows used to be built from. The art stays droppable.
	const bool painted = oracool::HasAbilitiesPanelArt();

	for (size_t i = 0; i < AbilitySheetCount; i++) {
		const TabPlate &plate = TabPlates[i];
		// A sheet this character cannot reach is drawn plain and never lights up. No class is short
		// one today, but IsSheetAvailable exists and a dead plate that still answered the cursor
		// would be a control that lies - the same reason the arrows used to be guarded.
		if (!IsSheetAvailable(plate.sheet))
			continue;

		const Rectangle hit = GetTabPlateRect(i);
		const Rectangle inner { { hit.position.x + TabPlateBevel, hit.position.y + TabPlateBevel },
			{ hit.size.width - 2 * TabPlateBevel, hit.size.height - 2 * TabPlateBevel } };

		if (!painted) {
			oracool::DrawLegacyTextBox(out, hit,
			    hit.contains(MousePosition) ? oracool::LegacyTextBoxHoverFill : oracool::LegacyTextBoxFill);
			oracool::DrawOutlinedString(out, plate.label, hit,
			    UiFlags::ColorWhitegold | UiFlags::FontSize24 | UiFlags::AlignCenter | UiFlags::VerticalCenter);
		}

		if (PressedTab == static_cast<int>(i)) {
			DrawHalfTransparentRectTo(out, inner.position.x, inner.position.y,
			    inner.size.width, inner.size.height, PAL16_GRAY + 13);
		} else if (hit.contains(MousePosition)) {
			DrawHalfTransparentRectTo(out, inner.position.x, inner.position.y,
			    inner.size.width, inner.size.height, PAL16_GRAY + 2);
		}

		// The OPEN sheet wears a gold ring rather than a wash, so it still reads as selected while
		// the cursor is over it, and the plate's painted glyph is never tinted out from under its
		// own label.
		if (plate.sheet == CurrentSheet)
			UnsafeDrawBorder2px(out, hit, PAL16_YELLOW + 2);
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
	return SheetIconSize;
}

int RowTextX()
{
	return AbilitiesIconX + RowIconColumnWidth() + AbilitiesTextGap;
}

// The assignment rings are GONE (user, 2026-09-02: "remove red and yellow outlines from abilities
// windows"). They were the 2026-08-15 request - a red ring for the left button, yellow for the
// right, split by edge when one ability wore both - and they have been redundant since the badges
// took over that job: the F-key badge already says which button holds an ability, in the corner
// convention the rings themselves established (top-left for the left button, top-right for the
// right). Two markers saying one thing, and the louder of the two was the one that fought the art.
//
// oracool::DrawSplitOutline stays: the stash still draws its weighted edge with it.

// The spend controls are gone entirely (user, 2026-08-20). First the right-edge "+N" group
// went (2026-08-19), then the corner glyphs that replaced it: left click invests and right
// click refunds on the whole cell now, so there is no second target to carve out and nothing
// to label. What is left is the eligibility outline in DrawTreeCell.

void DrawFKeyBadge(const Surface &out, Rectangle iconRect, SpellID sn); // defined with the tree helpers below

/** @brief The row's 56x56 icon square, in content-local coordinates. */
Rectangle SpellRowIconRect(int top)
{
	return { { AbilitiesIconX, top + (SpellRowHeight - SheetIconSize) / 2 }, { SheetIconSize, SheetIconSize } };
}

void DrawSpellRow(const Surface &content, size_t index, SpellID sn, int top)
{
	Player &player = *InspectPlayer;
	const int textX = RowTextX();
	const int textWidth = AbilitiesContentRightLimit - textX;
	const bool known = IsSpellKnown(sn);

	// The whole "unlearned" treatment is this one line: SpellType::Invalid's translation table maps
	// the icon onto PAL16_GRAY, which is exactly the desaturated, inactive reading wanted - no
	// second set of art, and it is the same grey the game already uses for a spell you cannot cast.
	SetSpellTrans(known ? GetSBookTrans(sn, true) : SpellType::Invalid);

	// Scaled to the sheet's icon square. The engine's spell icons carry their own bevelled plate in
	// the art, so background and symbol scale together and there is nothing to draw underneath -
	// unlike a tree cell, where the plate is a separate sprite behind a strip icon.
	Rectangle iconRect = SpellRowIconRect(top); // shifted below while pressed - see IconButton
	// The carved 2x2 slot frame behind the icon (user, 2026-09-02: "use the item.slot.2x2 frames
	// behind all icons in the abilities window"). It fits without a single number changing: this
	// window's icons are 56px, the equipment slots are 2x28, and the bezel family is keyed by exactly
	// that content size - so DrawGridBezel finds the 2x2 plate and lands its interior on the icon.
	//
	// Rows are 8px apart and the frame reaches 6px past the icon, so consecutive frames overlap by
	// four pixels and read as a shared rail rather than as separate plates. That is the same look the
	// backpack's own bezel has between its cells, so it is left alone rather than paid for in row
	// height - a taller row costs the page its last entry.
	oracool::DrawDropShadow(content, iconRect, oracool::GridBezelInset, IsIconPressed({ IconButtonKind::SpellRow, static_cast<int>(index) })); // the slot shadow (2026-09-05) - back after a misread "remove shadows": the ring was the icon's, not this
	iconRect.position += PressSinkFor({ IconButtonKind::SpellRow, static_cast<int>(index) }); // the face sinks while pressed
	oracool::DrawGridBezel(content, iconRect);
	// Oracool: Charge draws its OWN art here, from the Paladin strip through TryDrawSkillSpellIcon -
	// the call the skill wells and the speedbook already made first (2026-09-11: the book alone drew
	// FuriousChargeIcon, the bare plate, though Charge's art had shipped). The plate is the fallback.
	// The 56px sheet as it is (user, 2026-09-05: "always use the 56x56 icons ... they are much more
	// detailed"): the cell is 56, so the frame lands without resampling.
	if (!(oracool::IsFuriousChargeSpell(sn) && oracool::TryDrawSkillSpellIcon(content, iconRect, sn)))
		DrawSpellIconFittedTo(content, iconRect,
		    oracool::IsFuriousChargeSpell(sn) ? oracool::FuriousChargeIcon : sn);
	if (known) {
		DrawFKeyBadge(content, iconRect, sn);
	}
	// ...and the staff's charges under it, on the row for a spell held that way (user, 2026-09-03).
	oracool::DrawStaffChargeBadge(content, iconRect, player, sn);

	const UiFlags nameColor = known ? UiFlags::ColorWhitegold : UiFlags::ColorUiSilverDark;
	const UiFlags detailColor = known ? UiFlags::ColorWhite : UiFlags::ColorUiSilverDark;
	// THREE lines now - name, mana, damage (user, 2026-08-30). The damage line is empty for a
	// utility spell, a skill or a staff, and those rows centre their two lines exactly as before
	// rather than leaving a gap where a third would have gone.
	// The description lines cast the character sheet's drop shadow (user, 2026-09-05: "introduce text
	// shadows under description of spells in spells abilities window for easier reading") - the
	// small grey text on the stone was the hardest read in the window. The name line followed the
	// same night ("add text shadows also under spell names").
	constexpr UiFlags SpellDescriptionShadow = UiFlags::Shadowed;
	const std::string damage = GetSpellDamageLine(sn, known);
	const int lines = damage.empty() ? 2 : 3;
	const int textTop = top + (SpellRowHeight - lines * AbilitiesLineHeight) / 2;
	DrawString(content, oracool::GetSpellDisplayName(sn),
	    { { textX, textTop }, { textWidth, AbilitiesLineHeight } },
	    { nameColor | UiFlags::VerticalCenter | SpellDescriptionShadow });
	DrawString(content, GetSpellDetail(sn, known),
	    { { textX, textTop + AbilitiesLineHeight }, { textWidth, AbilitiesLineHeight } },
	    { detailColor | UiFlags::VerticalCenter | SpellDescriptionShadow });
	if (!damage.empty()) {
		DrawString(content, damage,
		    { { textX, textTop + 2 * AbilitiesLineHeight }, { textWidth, AbilitiesLineHeight } },
		    { detailColor | UiFlags::VerticalCenter | SpellDescriptionShadow });
	}

	// The spend controls live ON the icon now, exactly as they do on a tree cell (user, 2026-08-19):
	// green plus bottom-right, red minus bottom-left, the invested level between them on the bottom
	// edge. The old [+N][+] group at the row's right edge is gone with them - two spellings of the
	// same control on two sheets was the inconsistency.
	if (IsInspectingPlayer())
		return;
	const Player &me = *MyPlayer;
	const int invested = me._pSkillInvestment[static_cast<size_t>(sn)];
	// The rank badge, bottom-RIGHT and on a plate (user, 2026-09-02: "move skell level badges to
	// botom right corner of icons and put a dark semi-transparent backing behind them everywhere
	// badges exist, including abilities window"). It was bare whitegold text laid across the icon's
	// bottom edge - the last badge in the game still solving legibility its own way, and the one
	// most often over a bright plate. The corner keeps it clear of the F-key badges, which own the
	// two top ones. See oracool/badge.h.
	if (invested > 0)
		oracool::DrawBadge(content, iconRect, oracool::BadgeCorner::BottomRight, StrCat(invested));
	// No spend controls of any kind on this sheet. A spell's level comes from its books and its
	// items; there has been nothing to spend here since the 2026-08-20 rule, so there is nothing to
	// draw either.
}


/**
 * @brief The castable ability under the cursor in this window, refreshed by DrawHoverFeedback
 * every frame the window draws. What an F-key press binds to.
 */
SpellID HoveredAbilitySpell = SpellID::Invalid;

/** @brief Which F-key @p sn is bound to on @p leftButton's side (1-8), or 0. Lowest slot wins. */
int AssignedFKeyNumber(SpellID sn, bool leftButton)
{
	// An UNBOUND slot holds SpellID::Invalid, and so does any entry that is not a spell at all - a
	// basic attack, an aura. Without this guard the two match each other and an unbindable icon
	// reports the first empty slot as its own key: every attack in the right-button quick list wore
	// an "F1" badge that nothing had put there. Guarded here rather than at the call sites because
	// there are now two of them and the next one would make the same assumption.
	if (!IsValidSpell(sn))
		return 0;
	const SpellID *keys = leftButton ? MyPlayer->_pSplLHotKey : MyPlayer->_pSplHotKey;
	for (size_t i = 0; i < AbilityFKeyCount; i++) {
		if (keys[i] == sn)
			return static_cast<int>(i) + 1;
	}
	return 0;
}

/**
 * @brief The "F1".."F8" badges in 13x13 invisible frames at the icon's top corners.
 *
 * TOP-RIGHT is the right button's binding, TOP-LEFT the left's (user, 2026-08-18) - the same
 * left-is-left, right-is-right convention the assignment rings already use, so a glance at an icon
 * reads the same way in both markers. Both can be present at once: a skill may be on F3 for the
 * right hand and F5 for the left, and the two corners say so without either having to be a compound
 * label.
 *
 * Both are WHITE on a dark plate since v1.9.154 (user: "put a dark, transparent backing on the
 * badge, and use white font"). They used to be the assignment rings' own colours - red for the left
 * button, yellow for the right - and that distinction now rests entirely on which corner the badge
 * sits in, which is what the corners were for in the first place. The rings keep their colours.
 */
void DrawFKeyBadge(const Surface &out, Rectangle iconRect, SpellID sn)
{
	if (const int right = AssignedFKeyNumber(sn, /*leftButton=*/false); right != 0)
		oracool::DrawBadge(out, iconRect, oracool::BadgeCorner::TopRight, fmt::format("F{:d}", right));
	if (const int left = AssignedFKeyNumber(sn, /*leftButton=*/true); left != 0)
		oracool::DrawBadge(out, iconRect, oracool::BadgeCorner::TopLeft, fmt::format("F{:d}", left));
}

/**
 * @brief One cell of a tree page: the icon, and under it the point counter that doubles as the
 * invest button.
 *
 * The counter is always drawn, so a page reads as a spend sheet even before the first point; it
 * turns gold and grows a "+" only while the character actually holds a point this cell can take.
 * The burning aura keeps the gold ring on its icon.
 */
/**
 * @brief A thick red X across @p icon - "this skill is listed but not built yet".
 *
 * User request (2026-08-18): "i need visual feedback which skill are not developed. put a big red X
 * across their icon. i will then know these need work and design input from me in future." 36 of the
 * 161 class-tree rows are still `implemented = false`, and until now the only thing separating them
 * from a merely locked skill was the tooltip's "Not yet built." - the plate went grey either way, so
 * an empty tree page and an unfinished one looked identical.
 *
 * The stroke itself moved to oracool/hud_art.h on 2026-09-03, when a spent staff wanted the same
 * mark. This stays as the name the window calls it by.
 */
void DrawUnbuiltCross(const Surface &out, Rectangle icon)
{
	oracool::DrawRedCross(out, icon);
}

void DrawTreeCell(const Surface &content, oracool::ClassTreeSkill skill, int scroll)
{
	const oracool::ClassTreeSkillData &data = oracool::GetClassTreeSkillData(skill);
	const Player &player = *InspectPlayer;
	const bool unlocked = oracool::IsClassTreeSkillUnlocked(player, skill);
	const int invested = oracool::ClassTreeInvestment(player, skill);

	Rectangle icon = TreeIconRect(data.page, data.column, data.tier);
	icon.position.y -= scroll;
	Rectangle bar = TreeBarRect(data.page, data.column, data.tier);
	bar.position.y -= scroll;

	// An unbuilt skill is drawn like a locked one even at level: "listed but inert" and "not yet
	// earned" are both "you cannot use this", and a bright icon that does nothing is the lie.
	//
	// Three states, not two (user, 2026-08-17: "Unlocked skills with 0 points in them are unavailable
	// and inactive, ergo need to have red background, not green"). Green now means the skill has
	// something in it; red means it is yours to fill and empty; grey means it is not yours yet.
	// A Passive Skills row is judged by its SLOT, not by its points - it has none and can have none.
	// It is also usable while unbuilt, which is the one place this page departs from "unbuilt reads
	// as locked": slotting is the mechanism being shipped, and it genuinely works. The red X still
	// goes on, because the EFFECT genuinely does not.
	const bool isPassiveRow = oracool::IsPassiveSkillRow(skill);
	const bool slotted = isPassiveRow && oracool::PassiveSlotOf(player, skill) >= 0;
	// A BOOK row (the Sorceress's Fire and Lightning pages, and a few elsewhere) is listed but takes
	// no points - see BuildClassTreePage. Its state is not "how much have you spent" but "do you
	// have this spell", so it is read off the memorised set rather than off the investment array.
	const bool bookRow = oracool::IsClassTreeRowRetiredAsSpell(skill);
	const SpellID rowSpell = oracool::ClassTreeSpellId(skill);
	const bool bookKnown = bookRow && IsValidSpell(rowSpell)
	    && (player._pMemSpells & GetSpellBitmask(rowSpell)) != 0;
	const bool usable = bookRow ? bookKnown : (unlocked && (isPassiveRow || data.implemented));
	// A passive has its own two lit states (user, 2026-09-12: "make unlocked passive skills gold, and
	// the assigned ones green"): earned is the gold plate, slotted the green one. Every other row keeps
	// gold for "has something in it" and grey for "yours to fill".
	oracool::SkillPlateTint tint = oracool::SkillPlateTint::Locked;
	if (usable && isPassiveRow)
		tint = slotted ? oracool::SkillPlateTint::Green : oracool::SkillPlateTint::Ready;
	else if (usable)
		tint = (bookRow || invested > 0) ? oracool::SkillPlateTint::Ready : oracool::SkillPlateTint::Unspent;
	oracool::DrawDropShadow(content, icon, oracool::GridBezelInset, IsIconPressed({ IconButtonKind::TreeCell, static_cast<int>(skill) })); // the slot shadow (2026-09-05) - back after a misread "remove shadows": the ring was the icon's, not this
	// The face sinks while pressed; the shadow above was cast from the resting rect (see IconButton).
	icon.position += PressSinkFor({ IconButtonKind::TreeCell, static_cast<int>(skill) });
	oracool::DrawGridBezel(content, icon);
	// A LEGACY spell keeps its own icon here too, not the class strip's (user, 2026-09-03) - the
	// same rule the wells and the speedbook now follow.
	if (IsValidSpell(rowSpell) && IsLegacySpell(rowSpell)) {
		oracool::DrawLegacySpellIconInCell(content, icon, rowSpell, tint);
	} else {
		oracool::DrawClassTreeIconOutlined(content, icon, player._pClass, oracool::ClassTreeIconIndex(skill),
		    usable, tint);
	}

	// Struck out if the row is listed but not built. AFTER the icon so it reads as a mark made ON the
	// skill, and before the assignment rings and badges so those stay legible on top of it.
	if (!data.implemented)
		DrawUnbuiltCross(content, icon);

	if (isPassiveRow) {
		// A slotted passive wears the same outline a burning aura does, and for the same reason:
		// both mean "this one is live right now".
		if (slotted)
			oracool::DrawHoverOutline(content, icon);
	} else if (data.kind == oracool::ClassTreeKind::Aura) {
		if (oracool::GetActiveClassAura(player) == skill)
			oracool::DrawHoverOutline(content, icon);
	} else if (const SpellID slot = oracool::ClassTreeSpellId(skill); IsValidSpell(slot)) {
		DrawFKeyBadge(content, icon, slot);
	}

	// The rank badge, bottom-right on a dark plate (user, 2026-09-02), where it used to be bare
	// whitegold text along the icon's bottom edge, in the gap the two spend glyphs left behind when
	// they went in 2026-08-20. Same badge, same corner, same plate as the quick lists, the wells and
	// the sheet rows - which is the whole of the request.
	if (IsInspectingPlayer())
		return;
	const Player &me = *MyPlayer;
	if (isPassiveRow) {
		// The one number a passive has is the level it arrives at, and it is worth showing on a
		// LOCKED cell precisely because the tier no longer answers that question - three cells share
		// a row and open two levels apart.
		if (!unlocked) {
			// On the plate like every other number stuck to an icon, and it gives up its red to get
			// there - the badge template is white on dark, always (oracool/badge.h). Nothing is lost:
			// a locked cell is already drawn grey and struck through where it applies, so the red was
			// restating the state rather than carrying it.
			oracool::DrawBadge(content, icon, oracool::BadgeCorner::BottomRight,
			    StrCat(oracool::PassiveSkillRequiredLevel(skill)));
		} else if (ArmedPassiveSlot >= 0) {
			// A slot is waiting: light every passive that could go into it, so the second half of
			// the gesture has somewhere obvious to land.
			if (!slotted)
				oracool::DrawColoredOutline(content, icon, EligibleForPointColor);
		}
		return;
	}
	if (bookRow) {
		// The book's own level, in the same corner every other number lives in. Nothing when the
		// spell is not learned - the grey plate already says that, and a "0" would read as a rank.
		if (bookKnown)
			oracool::DrawBadge(content, icon, oracool::BadgeCorner::BottomRight,
			    StrCat(player.GetSpellLevel(rowSpell)));
		return;
	}
	if (invested > 0)
		oracool::DrawBadge(content, icon, oracool::BadgeCorner::BottomRight, StrCat(invested));
	// The green plus and red minus are GONE (user, 2026-08-20: "We remover the + and - symbols.
	// Skills eligible for bump just lit brighter than the rest").
	//
	// They were never really glyphs - they were two separate click targets carved out of the icon,
	// needed only because the icon itself meant "ready this skill" and the two actions had to be
	// kept apart by geometry. Left-invest / right-refund makes the whole cell one target, so what
	// is left to say is not "here is a button" but "this one will take a point" - which is a state,
	// and states are drawn, not labelled.
	//
	// Same predicate as before, so eligibility means exactly what it always meant; only the way it
	// is shown has changed.
	if (data.implemented && oracool::CanInvestClassTreePoint(me, skill))
		oracool::DrawColoredOutline(content, icon, EligibleForPointColor);
}

/**
 * @brief The four slots and the line of guidance under them.
 *
 * The hint line is not decoration. Slot-first is the gesture the user chose, and its one weakness
 * is that a player who clicks a passive without arming a slot gets nothing - so the window says,
 * at all times, which half of the gesture it is waiting for.
 */
void DrawPassiveSlotBand(const Surface &content, int scroll)
{
	const Player &player = *InspectPlayer;
	for (int slot = 0; slot < static_cast<int>(oracool::PassiveSlotCount); slot++) {
		Rectangle rect { { PassiveSlotX0 + slot * PassiveSlotPitch, PassiveSlotBandTop - scroll },
			{ PassiveSlotSize, PassiveSlotSize } };
		const bool open = player._pLevel >= oracool::PassiveSlotRequiredLevel(slot);
		const oracool::ClassTreeSkill held = oracool::PassiveInSlot(player, slot);
		const bool filled = held != oracool::ClassTreeSkill::None;

		const oracool::SkillPlateTint tint = !open
		    ? oracool::SkillPlateTint::Locked
		    : (filled ? oracool::SkillPlateTint::Green : oracool::SkillPlateTint::Unspent); // assigned: green (2026-09-12)
		// The slot frame, on the band as on the grid - these ARE slots, and they are the four cells
		// in this window that most want to look like sockets. The band's pitch is 63 to the icon's
		// 56, so neighbouring frames overlap by five pixels, the same shared-rail reading the sheet
		// rows have.
		oracool::DrawDropShadow(content, rect, oracool::GridBezelInset, IsIconPressed({ IconButtonKind::PassiveSlot, slot })); // the slot shadow (2026-09-05) - back after a misread "remove shadows": the ring was the icon's, not this
		rect.position += PressSinkFor({ IconButtonKind::PassiveSlot, slot }); // the face sinks while pressed
		oracool::DrawGridBezel(content, rect);
		if (filled) {
			oracool::DrawClassTreeIconOutlined(content, rect, player._pClass,
			    oracool::ClassTreeIconIndex(held), /*unlocked=*/true, tint);
		} else {
			// An empty slot is the plate alone - the same empty plate a passive with no art yet
			// draws, which is the window being consistent rather than a shortcut.
			oracool::DrawClassTreeIconOutlined(content, rect, player._pClass,
			    /*skillIndex=*/-1, open, tint);
		}
		if (!open) {
			DrawString(content, fmt::format(fmt::runtime(_("Lv{:d}")),
			                 oracool::PassiveSlotRequiredLevel(slot)),
			    { { rect.position.x, rect.position.y + PassiveSlotSize - SpendBoxSize },
			        { PassiveSlotSize, SpendBoxSize } },
			    { UiFlags::ColorRed | UiFlags::AlignCenter | UiFlags::VerticalCenter });
		} else if (slot == ArmedPassiveSlot) {
			oracool::DrawColoredOutline(content, rect, EligibleForPointColor);
		}
	}

}

/**
 * @brief The passive page's one line of instruction, above the slots and outside the list.
 *
 * On the panel surface rather than the list's subregion, in the band between the title and the
 * arch's foot (y 56..101 of the panel), which the user pointed at as room enough. It ends six pixels
 * above the slots' frames - the frame is GridBezelInset above the slot, and the slot is
 * AbilitiesListTop below the content top - so the list itself did not have to move.
 */
void DrawPassiveHintAboveList(const Surface &out, const Rectangle &panel)
{
	if (IsInspectingPlayer())
		return;
	const string_view hint = ArmedPassiveSlot >= 0
	    // Short enough to FIT. Audit finding, 2026-08-26: the previous second line was 61 characters
	    // in a 246x18 box and DrawString clips rather than wraps, so the instruction explaining the
	    // gesture was itself cut off. The right-click hint it lost now lives in the slot's hover
	    // tooltip, where there is room for it.
	    ? _("Now pick a passive")
	    : _("Click a slot, then a passive");
	const int frameTop = panel.position.y + AbilitiesContentTop + PassiveSlotBandTop - oracool::GridBezelInset;
	// Two pixels above the slot frames, not six (user, 2026-09-12: "move the gold explanation text in
	// Passive skills windows a few px down close to the 4 slots. because now it is overlapping a
	// couple of pixels with the title").
	//
	// The cause was v1.11.062, in this file: the tab row pushed this window's title from y 28 down to
	// 57..95, and this line is positioned UPWARD from the slots, so the two grew into each other -
	// the box was 77..95, inside the title band outright. There are only six pixels of clear space
	// between the title's bottom and the frames at 101, so the line cannot be lifted clear; it moves
	// down onto the slots instead, which is where it belongs anyway - it is their instruction.
	constexpr int HintGapAboveFrames = 2;
	const int bottom = frameTop - HintGapAboveFrames;
	DrawString(out, hint,
	    { { panel.position.x + AbilitiesInteriorLeft, bottom - PassiveHintHeight },
	        { AbilitiesInteriorRight - AbilitiesInteriorLeft, PassiveHintHeight } },
	    { UiFlags::ColorWhitegold | UiFlags::AlignCenter | UiFlags::VerticalCenter | UiFlags::Shadowed });
}

/** @brief Draws a whole tree page. */
void DrawTreePage(const Surface &content, int page, int scroll)
{
	if (IsPassivePage(page))
		DrawPassiveSlotBand(content, scroll);
	oracool::ClassTreeSkill skills[oracool::ClassTreeSkillCount];
	const size_t count = oracool::BuildClassTreePage(InspectPlayer->_pClass, page, skills);
	// The empty slots first (user, 2026-09-12: "put empty place holders (skill slot without white icon)
	// for skills in each available skill slot"): every cell of the three-by-six grid that no skill holds
	// is the slot frame and the empty plate - exactly what an empty passive slot is. Drawn only:
	// TreeCellAt walks the skills, so a click or a hover on one finds nothing.
	bool held[TreeGridTiers][TreeColumns] {};
	for (size_t i = 0; i < count; i++) {
		const oracool::ClassTreeSkillData &data = oracool::GetClassTreeSkillData(skills[i]);
		if (data.tier >= 0 && data.tier < TreeGridTiers && data.column >= 0 && data.column < TreeColumns)
			held[data.tier][data.column] = true;
	}
	for (int tier = 0; tier < TreeGridTiers; tier++) {
		for (int column = 0; column < TreeColumns; column++) {
			if (held[tier][column])
				continue;
			Rectangle slot = TreeIconRect(page, column, tier);
			slot.position.y -= scroll;
			oracool::DrawDropShadow(content, slot, oracool::GridBezelInset);
			oracool::DrawGridBezel(content, slot);
			oracool::DrawClassTreeIconOutlined(content, slot, InspectPlayer->_pClass, /*skillIndex=*/-1,
			    /*unlocked=*/false, oracool::SkillPlateTint::Unspent);
		}
	}
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
		if (TreeIconRect(data.page, data.column, data.tier).contains(localPoint)) {
			onBar = false;
			return skills[i];
		}
		if (TreeBarRect(data.page, data.column, data.tier).contains(localPoint)) {
			onBar = true;
			return skills[i];
		}
	}
	return std::nullopt;
}

} // namespace

Rectangle GetSpellBookPanelRect()
{
	// Flush to the BOTTOM-right, mirroring the inventory - the two share the right-hand slot and are
	// never open at once, so they must occupy exactly the same space, including after the 2026-08-27
	// docking change.
	return { { gnScreenWidth - AbilitiesPanelSize.width, oracool::BottomDockedTop(AbilitiesPanelSize.height) },
		AbilitiesPanelSize };
}

Rectangle GetSpellBookContentRect()
{
	const Rectangle panel = GetSpellBookPanelRect();
	return { { panel.position.x, panel.position.y + AbilitiesContentTop }, AbilitiesContentSize };
}

/**
 * @brief Derives a binding's TYPE from the spell's identity, never from its momentary castability.
 *
 * GetSBookTrans folds "can you cast it right now" into its answer (audit, 2026-08-17): a memorized
 * spell bound while the mana happened to be short came back SpellType::Invalid, and the binding
 * stored it - so the key stayed dead after the mana returned, wearing its badge the whole time. A
 * binding outlives the moment it was made in. Charges joined in the external-audit round: a
 * staff-only spell typed as Spell casts down the memorized path and dies on the Fail_Level0 gate
 * while the staff sits charged in hand.
 */
SpellType BindingTypeFor(const Player &player, SpellID spell)
{
	const SpellMask bit = GetSpellBitmask(spell);
	if ((player._pAblSpells & bit) != 0)
		return SpellType::Skill;
	if ((player._pMemSpells & bit) != 0)
		return SpellType::Spell;
	if ((player._pISpells & bit) != 0)
		return SpellType::Charges;
	// A scroll is a scroll (audit, 2026-09-27): bound over a quick list's Scroll cell it fell to Spell, and on the left
	// button that cast the spell for mana, never reading the scroll, whenever an item raised its level above 0.
	if ((player._pScrlSpells & bit) != 0)
		return SpellType::Scroll;
	return SpellType::Spell;
}

/**
 * @brief Strips @p spell out of every hotkey slot on BOTH buttons.
 *
 * User request (2026-08-18): "a single hotkey can not be assigned on more than 1 skill and on more
 * than one skill slot." Enforced by clearing rather than by refusing: a key you press over a skill
 * should end up on that skill, and the cost of moving it is the old binding going quiet. Refusing
 * instead would mean silently doing nothing, which is the failure mode this whole item exists to
 * remove.
 *
 * Both arrays are swept, not just the one being written, so a skill cannot sit on F3-left and
 * F5-right at once and leave the player guessing which badge is authoritative.
 */
/** @brief The aura twin of ClearSpellFromHotkeys - one key, one thing, auras included. */
void ClearAuraFromHotkeys(Player &player, oracool::ClassTreeSkill aura)
{
	for (size_t i = 0; i < AbilityFKeyCount; i++) {
		if (player._pAuraHotKey[i] == static_cast<uint16_t>(aura))
			player._pAuraHotKey[i] = 0xFFFF;
	}
}

void ClearSpellFromHotkeys(Player &player, SpellID spell)
{
	for (size_t i = 0; i < AbilityFKeyCount; i++) {
		if (player._pSplHotKey[i] == spell) {
			player._pSplHotKey[i] = SpellID::Invalid;
			player._pSplTHotKey[i] = SpellType::Invalid;
		}
		if (player._pSplLHotKey[i] == spell) {
			player._pSplLHotKey[i] = SpellID::Invalid;
			player._pSplLTHotKey[i] = SpellType::Invalid;
		}
	}
}

/**
 * @brief Empties F-key @p slot on the right button, the left button and the aura array.
 *
 * User, 2026-09-07: "i assigned F2 to a skill in lmb picker but that didnt remove it from the skill
 * who used to use it in rmb picker. fix this. a hot key can only be assigned to a single skill on a
 * single picker. hard rule." Until now a bind swept the SKILL off every key (ClearSpellFromHotkeys)
 * but left the KEY's other side alone, so F2-left and F2-right could name two skills. This is the
 * other half of the sweep: one key, one thing, one button.
 */
void ClearHotkeySlotOnBothButtons(Player &player, size_t slot)
{
	player._pSplHotKey[slot] = SpellID::Invalid;
	player._pSplTHotKey[slot] = SpellType::Invalid;
	player._pSplLHotKey[slot] = SpellID::Invalid;
	player._pSplLTHotKey[slot] = SpellType::Invalid;
	player._pAuraHotKey[slot] = 0xFFFF;
}

int GetAbilityFKeyNumber(SpellID spell, bool leftButton)
{
	return AssignedFKeyNumber(spell, leftButton);
}

int GetAuraFKeyNumber(oracool::ClassTreeSkill aura)
{
	// No leftButton parameter, deliberately: an aura is the right button's occupant whichever list
	// it was bound from - see the aura/right-button rule in class_tree.h - so it has one binding,
	// not one per side.
	if (aura == oracool::ClassTreeSkill::None || MyPlayer == nullptr)
		return 0;
	for (size_t i = 0; i < AbilityFKeyCount; i++) {
		if (MyPlayer->_pAuraHotKey[i] == static_cast<uint16_t>(aura))
			return static_cast<int>(i) + 1;
	}
	return 0;
}

void BindAbilityHotkey(Player &player, size_t slot, SpellID spell, bool leftButton)
{
	if (slot >= AbilityFKeyCount || !IsValidSpell(spell))
		return;
	SpellID *keys = leftButton ? player._pSplLHotKey : player._pSplHotKey;
	SpellType *types = leftButton ? player._pSplLTHotKey : player._pSplTHotKey;
	if (keys[slot] == spell) {
		// The same key on the same skill takes it back off.
		keys[slot] = SpellID::Invalid;
		types[slot] = SpellType::Invalid;
		return;
	}
	// The hard rule, both halves: this KEY forgets whatever it held on either button (and any aura),
	// and this SKILL leaves every other key it sat on. Then the one binding is written.
	ClearHotkeySlotOnBothButtons(player, slot);
	ClearSpellFromHotkeys(player, spell);
	keys[slot] = spell;
	types[slot] = BindingTypeFor(player, spell);
}

bool HandleAbilityFKey(size_t slot, bool shift)
{
	if (slot >= AbilityFKeyCount)
		return false;
	Player &me = *MyPlayer;

	// Which button this press is about. Bare key = right, LShift+key = left (user, 2026-08-18) -
	// EXCEPT while a quick list is open, where the open list names the button and shift has nothing
	// left to say (user, 2026-08-30: "assignes/deassignes F1-F8 to that respective skill/spell in
	// that respective skill slot (lmb/rmb)"). Holding shift over the LMB list must not silently
	// write the RIGHT button's array.
	const bool forLeft = oracool::IsSkillPickerOpen() ? oracool::IsSkillPickerForLeftButton() : shift;

	// A quick list is open: the key EDITS that list's binding, exactly as it edits the Abilities
	// window's. Consumed either way - a bind key that fell through to casting mid-edit would ready
	// a skill on the very button the player is in the middle of assigning.
	if (oracool::IsSkillPickerOpen()) {
		// The hover as of THIS key press, not of the last draw (external audit, 2026-09-06: UI-01).
		oracool::RefreshSkillPickerHover();
		// AURAS FIRST, and they take a different road entirely (user, 2026-08-31: "i cant set them
		// on auras"). An aura row carries SpellID::Invalid - it is a toggle, not a cast - so it can
		// never live in the two SpellID arrays, and the check below would reject it forever. It goes
		// in _pAuraHotKey instead, named by its tree row.
		if (const oracool::ClassTreeSkill aura = oracool::GetSkillPickerHoveredAura();
		    aura != oracool::ClassTreeSkill::None) {
			if (me._pAuraHotKey[slot] == static_cast<uint16_t>(aura)) {
				me._pAuraHotKey[slot] = 0xFFFF; // the same key on the same aura takes it back off
			} else {
				// One key, one thing: an aura landing on a key clears whatever was there, on EITHER
				// button, exactly as a spell would.
				ClearHotkeySlotOnBothButtons(me, slot);
				ClearAuraFromHotkeys(me, aura);
				me._pAuraHotKey[slot] = static_cast<uint16_t>(aura);
			}
			oracool::ScheduleAutoSaveForSkillChange();
			oracool::PlayUiMoveSound(); // bound or unbound - either way the key moved
			RedrawEverything();
			return true;
		}

		const SpellID spell = oracool::GetSkillPickerHoveredSpell();
		if (!IsValidSpell(spell))
			return true; // over an attack or no cell at all - nothing a hotkey can hold
		BindAbilityHotkey(me, slot, spell, forLeft);
		oracool::ScheduleAutoSaveForSkillChange();
		oracool::PlayUiMoveSound();
		RedrawEverything();
		return true;
	}

	// With the window open, an F-key EDITS bindings rather than using them. The key is consumed even
	// when nothing is hovered - a bind key that fell through to casting mid-edit would be worse than
	// one that does nothing.
	if (sbookflag && !IsInspectingPlayer()) {
		const SpellID spell = HoveredAbilitySpell;
		if (!IsValidSpell(spell))
			return true;
		// Assign and remove are one gesture per button (user, 2026-08-18: "press hotkey again when
		// mouse hovering over"); BindAbilityHotkey does both. Audit finding, 2026-08-26: the BINDINGS
		// were the last readied-skill state with no save trigger, hence the schedule below.
		BindAbilityHotkey(me, slot, spell, forLeft);
		oracool::ScheduleAutoSaveForSkillChange();
		oracool::PlayUiMoveSound();
		RedrawEverything();
		return true;
	}

	// In play. WHICH BUTTON a press acts on is decided by where the binding actually is, not by
	// shift (user, 2026-08-31: "hotkeys only actually assign skill in the rmb slot if used on rmb
	// speedbook").
	//
	// Shift-for-left dates from 2026-08-18, when both bindings were made in the Abilities window and
	// the modifier was the only thing that could tell them apart. Binding moved into the quick lists
	// on 2026-08-30, and the list itself now says which button a key belongs to - so a key bound in
	// the LEFT list is a left-button key, and the player who put it there expects a plain press to
	// use it. Requiring shift as well meant a left binding looked like it had silently failed.
	//
	// Shift stays as a tiebreak for saves from before 2026-09-07, when F3-left and F3-right could
	// still name two different skills. A bind now empties the key on both buttons first
	// (ClearHotkeySlotOnBothButtons), so a fresh binding never has two sides to choose between.
	// An aura on this key toggles it, and takes precedence: nothing else can be on the same key,
	// because binding one clears both spell arrays for that slot.
	if (me._pAuraHotKey[slot] != 0xFFFF) {
		const auto aura = static_cast<oracool::ClassTreeSkill>(me._pAuraHotKey[slot]);
		// Re-validated at USE time, not just at load: a refund can take the aura away between the
		// binding and the press, and RefreshInnateSpells cannot clear this array because an aura
		// is not in _pAblSpells to begin with.
		if (oracool::ClassTreeInvestment(me, aura) > 0) {
			oracool::ToggleClassAura(me, aura);
			CalcPlrInv(me, false);
			oracool::ScheduleAutoSaveForSkillChange();
			RedrawEverything();
		}
		return true;
	}

	const bool rightBound = IsValidSpell(me._pSplHotKey[slot]);
	const bool leftBound = IsValidSpell(me._pSplLHotKey[slot]);
	const bool useLeft = shift ? leftBound : (leftBound && !rightBound);

	if (useLeft) {
		// Only what the hero has, by the binding's own kind - the check ToggleSpell makes for the right button (audit,
		// 2026-09-27). The left key readied whatever it held: a refunded skill, a spell never learned.
		if (!HeroHasBinding(me, me._pSplLHotKey[slot], me._pSplLTHotKey[slot]))
			return true;
		me._pLRSpell = me._pSplLHotKey[slot];
		me._pLRSplType = me._pSplLTHotKey[slot];
		// The left-hand twin of ToggleSpell, and it had to be written out rather than reused - so
		// it also missed the save the right-hand path schedules (audit, 2026-08-26).
		oracool::ScheduleAutoSaveForSkillChange();
		RedrawEverything();
		return true;
	}
	if (shift && !leftBound)
		return true; // shift asked for a left binding that is not there; the right one is not it

	// The vanilla quick-spell path: readies the bound ability on the right button, or casts outright
	// under quickCast. Also the no-binding case, where it correctly does nothing.
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
	// A gesture in progress does not survive leaving the page it was started on.
	ArmedPassiveSlot = -1;
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
	ArmedPassiveSlot = -1;
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
std::string BuildSpellStatBlock(SpellID sn, bool withNext)
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
	const bool isSpell = GetSBookTrans(sn, false) == SpellType::Spell;

	if (player._pMagic < required)
		line(fmt::format(fmt::runtime(_("Requires {:d} Magic")), required));

	// The numbers at one level: mana, then damage where the spell has any. Both from the AtLevel
	// functions the game itself runs, so the sheet and the cast cannot disagree. Mana FALLS as a
	// spell levels here - the adjustment is subtracted - so the next block's mana is a reason to
	// spend a point rather than a price for it, and D2 quotes it for the same reason.
	// Shared with the Abilities window's tree rows (oracool/skill_facts.h), so a spell reads the same on both.
	const auto levelLines = [&](int at) {
		return oracool::SpellLevelLines(player, sn, at);
	};

	// THE DIABLO II SHAPE (user, 2026-09-05): "Current Spell Level: N" over this level's numbers, a
	// gap, "Next Level" over the next's. An UNLEARNED spell says so and quotes its first level - the
	// numbers it would have the moment a book is read (user, 2026-08-30: "Damage Range at current
	// level (level 1 if not learned yet)"). Skills, scrolls and staves have no level ladder of their
	// own, so they quote the numbers as they are with no heading.
	if (isSpell) {
		if (level > 0) {
			line(fmt::format(fmt::runtime(_("Current Spell Level: {:d}")), level));
			line(levelLines(level));
		} else {
			line(std::string(_("Not learned")));
		}
	} else {
		line(levelLines(std::max(level, 1)));
	}
	if (withNext && (isSpell || level > 0)) {
		out += "\n";
		line(std::string(level == 0 ? _("First Level") : _("Next Level")));
		line(levelLines(level + 1));
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

	// A spell's panel is its prose and then its numbers, separated by a blank line. The tree pages
	// build their own block through ClassTreeEffectLine, which since v1.9.153 also quotes what the
	// current rank grants and what the next one would - so "an aura has no numbers yet", which stood
	// here until then, is no longer true of either sheet. A Paladin skill still carries its price
	// and gate on the row itself.
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
		// A slot has to name what it holds. Every passive draws an EMPTY PLATE today - the art is
		// deliberately not made yet - so without this a filled slot and an empty one differ only by
		// tint, and nothing anywhere tells you which four you are running.
		if (IsPassivePage(*page)) {
			if (const int slot = PassiveSlotAt(local); slot >= 0) {
				const Rectangle rect { { PassiveSlotX0 + slot * PassiveSlotPitch, AbilitiesListTop },
					{ PassiveSlotSize, PassiveSlotSize } };
				const oracool::ClassTreeSkill held = oracool::PassiveInSlot(*InspectPlayer, slot);
				const bool open = InspectPlayer->_pLevel >= oracool::PassiveSlotRequiredLevel(slot);
				PendingHoverTitle = fmt::format(fmt::runtime(_("Passive slot {:d}")), slot + 1);
				if (!open) {
					PendingHoverText = fmt::format(fmt::runtime(_("Opens at level {:d}")),
					    oracool::PassiveSlotRequiredLevel(slot));
				} else if (held == oracool::ClassTreeSkill::None) {
					PendingHoverText = std::string(_("Empty. Click to choose a passive for it."));
				} else {
					const oracool::ClassTreeSkillData &heldData = oracool::GetClassTreeSkillData(held);
					PendingHoverText = std::string(_(heldData.name)) + "\n"
					    + std::string(_(heldData.description)) + "\n\n"
					    + std::string(_("Right-click to empty this slot."));
				}
				PendingHoverAnchor = { { contentRect.position.x,
					                       contentRect.position.y + rect.position.y - scroll },
					{ AbilitiesContentRightLimit, rect.size.height } };
				HasPendingHover = true;
				FrameHoverIcon = { IconButtonKind::PassiveSlot, slot };
				// The hover is a deeper shadow under the slot, not a ring (user, 2026-09-05).
				oracool::DrawHoverShadow(content, { { rect.position.x, rect.position.y - scroll }, rect.size },
				    oracool::GridBezelInset, IsIconPressed({ IconButtonKind::PassiveSlot, slot }));
				return;
			}
		}
		const std::optional<oracool::ClassTreeSkill> hovered = TreeCellAt(*page, local, onBar);
		if (!hovered.has_value())
			return;
		FrameHoverIcon = { IconButtonKind::TreeCell, static_cast<int>(*hovered) };
		const oracool::ClassTreeSkillData &data = oracool::GetClassTreeSkillData(*hovered);
		const Rectangle cell = TreeIconRect(data.page, data.column, data.tier);
		// A tree cell is an F-key target only when a CLICK on it would do something - the hotkey and
		// the click must agree, or a key binds what the mouse refuses (user, 2026-08-18: neither
		// clicking nor hotkeying an unbuilt or unranked skill should be allowed).
		//
		// Auras are excluded too: they have no SpellID to bind, and the well only reports the lit one.
		if (data.implemented && data.kind != oracool::ClassTreeKind::Aura
		    && oracool::ClassTreeInvestment(*InspectPlayer, *hovered) > 0
		    && oracool::IsClassTreeSkillUnlocked(*InspectPlayer, *hovered)) {
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
		oracool::DrawHoverShadow(content, { { cell.position.x, cell.position.y - scroll }, cell.size },
		    oracool::GridBezelInset, IsIconPressed({ IconButtonKind::TreeCell, static_cast<int>(*hovered) })); // a deeper shadow under the cell, not a ring (user, 2026-09-05)
		return;
	}

	// Everything past the tree is the Spells list, a uniform stride.
	{
		const int height = RowHeightFor(CurrentSheet);
		// The list starts AbilitiesListTop pixels down, so the band above the first row belongs to
		// no row at all - without the guard a negative y divides to index 0 and the first row would
		// answer for the air above it.
		if (y < AbilitiesListTop)
			return;
		const size_t index = static_cast<size_t>((y - AbilitiesListTop) / height);
		SpellID rows[MaxSpellRows];
		const size_t rowCount = BuildSpellRows(rows);
		if (index < rowCount) {
			rowTop = AbilitiesListTop + static_cast<int>(index) * height;
			rowHeight = height;
			found = true;
			FrameHoverIcon = { IconButtonKind::SpellRow, static_cast<int>(index) };
			if (IsSpellKnown(rows[index]))
				HoveredAbilitySpell = rows[index];
			title = oracool::GetSpellDisplayName(rows[index]); // already translated
			description = spellInfo(rows[index]);
		}
	}

	if (!found)
		return;

	// Local to the content subregion, which is what clips it to the scrolling area.
	// From the interior's left edge, not the panel's - an outline spanning from x=0 drew its left
	// side across the bezel, which is the exact thing the interior bounds exist to prevent.
	// The row's width but the ICON's height (2026-09-05): the ring is measured outward from a slot,
	// and the slot here is the row's icon - ringing the whole 74px row put the ring twelve pixels
	// onto the rows above and below ("no overlapping with adjacent spells").
	// Under the ICON only (user, 2026-09-05: "draw it under the icons, not under the texts"): the
	// deeper hover shadow on the row's icon rect, never across the name and detail lines.
	const Rectangle iconOfRow = SpellRowIconRect(rowTop - scroll);
	oracool::DrawHoverShadow(content, iconOfRow, oracool::GridBezelInset, IsIconPressed(FrameHoverIcon));

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
		PendingHoverAnchor = { { contentRect.position.x + AbilitiesInteriorLeft, contentRect.position.y + rowTop - scroll },
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
	// Beside the WINDOW, 8px off its edge, never over it (user, 2026-09-05). Drawn from the frame's
	// above-everything slot, so over the bottom HUD it goes when a low row needs the room.
	oracool::DrawHoverPanel(out, PendingHoverTitle, PendingHoverText, PendingHoverAnchor, GetSpellBookPanelRect());
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
	//
	// Its OWN canvas since 2026-09-12 - the one with the five tab plates in its header. The shared
	// side panel is the first fallback and the procedural fill the second, so the window still draws
	// if either file is missing; it simply loses the buttons, which is why the plates are hit-tested
	// against the same table that draws them rather than against the artwork.
	if (oracool::HasAbilitiesPanelArt()) {
		oracool::DrawAbilitiesPanelArt(out, panel.position);
	} else if (oracool::HasSidePanelArt()) {
		oracool::DrawSidePanelArt(out, panel.position);
	} else {
		oracool::DrawThemedFill(out, panel);
		oracool::DrawOrnateBorder(out, panel);
	}

	// The sheet buttons, before the title so the title's outline is never drawn under an overlay.
	DrawTabPlates(out);

	// UNDER the tab row, not on the shared PanelTitleTop the other five windows use - see
	// AbilitiesTitleTop.
	const Rectangle labelArea { { panel.position.x + AbilitiesMargin, panel.position.y + AbilitiesTitleTop },
		{ panel.size.width - 2 * AbilitiesMargin, AbilitiesTitleHeight } };
	oracool::DrawOutlinedString(out, GetSheetTitle(CurrentSheet), labelArea,
	    UiFlags::ColorWhitegold | UiFlags::FontSize30 | UiFlags::AlignCenter | UiFlags::VerticalCenter);

	// The passive page's instruction line, between the title and the list (2026-09-05).
	if (const std::optional<int> page = TreePageOf(CurrentSheet); page.has_value() && IsPassivePage(*page))
		DrawPassiveHintAboveList(out, panel);

	// No nav row and no "Points:" readout here any more (2026-08-17): the unspent pool is a HUD
	// element now - the frame above the RMB well, where it is visible while playing rather than only
	// with this window open. See DrawUnspentPointsFrame in control.cpp. The arrows that replaced the
	// nav row are gone too (2026-09-12); the tab plates above are the way between sheets.

	UpdateScrollBounds();
	DrawScrollbar(out, panel);

	// Rows draw through a subregion covering only the scrolling area, so a row straddling its top
	// or bottom edge is clipped there rather than spilling onto the title band or the bottom bevel.
	const Rectangle contentRect = GetSpellBookContentRect();
	const Surface content = out.subregion(contentRect.position.x, contentRect.position.y,
	    contentRect.size.width, contentRect.size.height);

	const int rowHeight = RowHeightFor(CurrentSheet);
	const int scroll = CurrentScroll();

	// The hover sound: DrawHoverFeedback names the icon button under the cursor this frame, and the
	// frame it changes to one that was not under it last frame is the entry that sounds (see
	// IconButton). Sensed here because the hover walk is per frame and the window has no tick of its own.
	FrameHoverIcon = {};
	DrawHoverFeedback(out, content, contentRect, scroll);
	if (FrameHoverIcon.kind != IconButtonKind::None && FrameHoverIcon != LastHoverIcon)
		oracool::PlayUiMoveSound();
	LastHoverIcon = FrameHoverIcon;

	if (const std::optional<int> page = TreePageOf(CurrentSheet); page.has_value()) {
		DrawTreePage(content, *page, scroll);
		return;
	}

	// Spells: the one list sheet left, a uniform stride.
	SpellID rows[MaxSpellRows];
	const size_t rowCount = BuildSpellRows(rows);
	for (size_t i = 0; i < rowCount; i++) {
		const int top = AbilitiesListTop + static_cast<int>(i) * rowHeight - scroll;
		if (top + rowHeight <= 0 || top >= AbilitiesContentSize.height)
			continue;
		DrawSpellRow(content, i, rows[i], top);
	}
}

void CheckSBook(bool assignToRightButton)
{
	// The tab plates first, and outside the inspect guard - changing sheet is reading, not acting, so
	// it stays available while inspecting another player's abilities. Hit-tested only where a plate
	// is actually drawn, so an unreachable sheet has no invisible control to hit.
	for (size_t i = 0; i < AbilitySheetCount; i++) {
		if (!IsSheetAvailable(TabPlates[i].sheet))
			continue;
		if (!GetTabPlateRect(i).contains(MousePosition))
			continue;
		PressedTab = static_cast<int>(i);
		// Clicking the open sheet's own plate is a no-op rather than a reset: it must not throw
		// away the scroll position of the page you are already reading.
		if (TabPlates[i].sheet != CurrentSheet) {
			CurrentSheet = TabPlates[i].sheet;
			// The same two things CycleAbilitySheet does on the way out of a page: a gesture in
			// progress does not survive leaving the page it was started on, and the new sheet's
			// scroll bounds are its own.
			ArmedPassiveSlot = -1;
			UpdateScrollBounds();
			oracool::PlayUiMoveSound();
		}
		RedrawEverything();
		return;
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
	// The tree, and the tree is now purely a place to SPEND. Left click puts a point in, right click
	// takes one out; nothing here readies a skill or lights an aura any more, because selection
	// moved to the pickers the LMB and RMB wells open.
	if (const std::optional<int> page = TreePageOf(CurrentSheet); page.has_value()) {
		bool onBar = false;
		const Point local { MousePosition.x - content.position.x, y };

		// The passive page spends nothing, so left/right mean something else on it entirely: the
		// slot-first gesture the user chose on 2026-08-25.
		if (IsPassivePage(*page)) {
			Player &me = *MyPlayer;
			if (const int slot = PassiveSlotAt(local); slot >= 0) {
				PressIconButton({ IconButtonKind::PassiveSlot, slot }); // sinks and sounds whatever follows
				if (me._pLevel < oracool::PassiveSlotRequiredLevel(slot)) {
					EventPlrMsg(fmt::format(fmt::runtime(_("This slot opens at level {:d}.")),
					                oracool::PassiveSlotRequiredLevel(slot)),
					    UiFlags::ColorRed);
					return;
				}
				if (assignToRightButton) {
					// Right-click empties, which is the same thing the right button means on every
					// other page: take back what the left one put in.
					if (oracool::ClearPassiveSlot(me, slot)) {
						if (ArmedPassiveSlot == slot)
							ArmedPassiveSlot = -1;
						CalcPlrInv(me, false);
						RedrawEverything();
					}
					return;
				}
				// Clicking the armed slot again puts the gesture down. Without this the only way
				// out of a half-finished action would be to complete it.
				ArmedPassiveSlot = (ArmedPassiveSlot == slot) ? -1 : slot;
				RedrawEverything();
				return;
			}
			const std::optional<oracool::ClassTreeSkill> cell = TreeCellAt(*page, local, onBar);
			if (!cell.has_value())
				return;
			PressIconButton({ IconButtonKind::TreeCell, static_cast<int>(*cell) });
			if (assignToRightButton) {
				// Right-clicking a slotted passive pulls it out wherever it happens to be sitting,
				// so a player who wants it gone does not have to find which slot holds it.
				const int slot = oracool::PassiveSlotOf(me, *cell);
				if (slot >= 0 && oracool::ClearPassiveSlot(me, slot)) {
					CalcPlrInv(me, false);
					RedrawEverything();
				}
				return;
			}
			if (!oracool::IsClassTreeSkillUnlocked(me, *cell)) {
				EventPlrMsg(fmt::format(fmt::runtime(_("{:s} is learned at level {:d}.")),
				                std::string(_(oracool::GetClassTreeSkillData(*cell).name)),
				                oracool::PassiveSkillRequiredLevel(*cell)),
				    UiFlags::ColorRed);
				return;
			}
			if (ArmedPassiveSlot < 0) {
				// The gesture's one failure mode, answered rather than ignored. Clicking a passive
				// with no slot armed used to be the case where nothing happened and nothing said
				// why - which is the exact complaint that produced ClassTreeLockReason.
				EventPlrMsg(_("Click one of the four slots above first."), UiFlags::ColorRed);
				return;
			}
			// Clicking a passive that is ALREADY slotted takes it out.
			//
			// This used to say "already in slot 2" and stop, which is a dead end - and worse, it
			// left touch players with no way to unslot anything at all, because the touch handler
			// only ever raises the left-button path (audit, 2026-08-26). Making the left button a
			// toggle costs nothing: it replaces a message that did nothing with the obvious action,
			// and right-click still empties a slot for anyone who reaches for it.
			if (const int already = oracool::PassiveSlotOf(me, *cell); already >= 0) {
				if (oracool::ClearPassiveSlot(me, already)) {
					CalcPlrInv(me, false);
					RedrawEverything();
				}
				return;
			}
			if (oracool::SetPassiveSlot(me, ArmedPassiveSlot, *cell)) {
				ArmedPassiveSlot = -1;
				CalcPlrInv(me, false);
				RedrawEverything();
			}
			return;
		}

		const std::optional<oracool::ClassTreeSkill> hit = TreeCellAt(*page, local, onBar);
		if (!hit.has_value())
			return;
		PressIconButton({ IconButtonKind::TreeCell, static_cast<int>(*hit) }); // before every refusal below
		// POINTS ONLY, and the whole icon is the target. User, 2026-08-20: "in abilities window we
		// repurpose left/right clicks - left click ADDS point, right click SUBTRACTS."
		//
		// The spend corners are gone with the gesture that needed them. They were carved out of the
		// icon only because the icon itself meant "ready this skill", so the two actions had to be
		// kept apart by geometry. Readying moved to the LMB/RMB picker, which leaves the cell free to
		// be one target with two buttons - the simplification is a consequence of the split, not a
		// tidy-up on top of it.
		if (onBar)
			return; // the counter row reports the rank, it does not act
		const oracool::ClassTreeSkillData &data = oracool::GetClassTreeSkillData(*hit);
		// UNBUILT rows stay wholly inert (user, 2026-08-18). Taking points for something that does
		// nothing with them is exactly the trap that rule exists to prevent, and a struck-out cell
		// already says it is not a control.
		if (!data.implemented)
			return;
		// A BOOK row is listed for reference and binding, not for spending (user rule, 2026-08-20).
		// Silence here would look like a dead cell, which is the complaint that put these rows back
		// on the page in the first place.
		if (oracool::IsClassTreeRowRetiredAsSpell(*hit)) {
			EventPlrMsg(fmt::format(fmt::runtime(_("{:s} is raised by books, not by skill points.")),
			                _(data.name)),
			    UiFlags::ColorRed);
			return;
		}

		// Shift moves five points at a time, Ctrl as many as will go - to the cap or the empty pool, or all the way
		// back out (dev note, 2026-09-27: "add shift/ctrl+click (l/r) to assign/remove skill points").
		const SDL_Keymod mods = SDL_GetModState();
		const int count = (mods & KMOD_CTRL) != 0 ? oracool::MaxTreeInvestment : ((mods & KMOD_SHIFT) != 0 ? ShiftClickPoints : 1);
		const bool changed = assignToRightButton
		    ? oracool::RefundClassTreePoints(*MyPlayer, *hit, count) > 0
		    : oracool::InvestClassTreePoints(*MyPlayer, *hit, count) > 0;
		if (changed) {
			// The whole of "make it take effect": the aura provider and every ladder read the
			// investment on the next totals walk.
			CalcPlrInv(*MyPlayer, false);
			// The click already sounded at the press (PressIconButton); an invest may add the skill's
			// own learn cue inside InvestClassTreePoint, which alone knows whether it rang.
			RedrawEverything();
			return;
		}
		// A refused INVEST says why where there is something to say. The tier gate is the usual
		// reason and the one a player cannot deduce from the cell alone; a refused refund needs no
		// explanation, because "nothing invested" is what the cell already shows.
		if (!assignToRightButton && !oracool::IsClassTreeSkillUnlocked(*MyPlayer, *hit)) {
			const std::string reason = oracool::ClassTreeLockReason(*MyPlayer, *hit);
			if (!reason.empty())
				EventPlrMsg(reason, UiFlags::ColorRed);
		}
		return;
	}
	// Spells: the one list sheet left.
	SpellID sn = SpellID::Invalid;
	{
		SpellID rows[MaxSpellRows];
		const size_t rowCount = BuildSpellRows(rows);
		// The same guard the hover walk carries, and for the same reason: the strip above the first
		// row is not row zero.
		if (y < AbilitiesListTop)
			return;
		const size_t rowIndex = static_cast<size_t>((y - AbilitiesListTop) / RowHeightFor(CurrentSheet));
		if (rowIndex >= rowCount)
			return;
		sn = rows[rowIndex];
		PressIconButton({ IconButtonKind::SpellRow, static_cast<int>(rowIndex) }); // an unlearned row too
		// The spend corners are GONE from this sheet. User rule, 2026-08-20: "Spells cant be
		// affected by skill points, only by books. Vanila D1." A spell's level is its book level
		// plus item bonuses, and nothing on this list spends a point any more - so the whole row is
		// the ready target again, with no corner carved out of the icon to avoid.
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
		oracool::ClearClassAuraForRightButton(player);
		player._pRSpell = sn;
		player._pRSplType = st;
		oracool::ScheduleAutoSaveForSkillChange();
	} else {
		player._pLRSpell = sn;
		player._pLRSplType = st;
		oracool::ScheduleAutoSaveForSkillChange();
	}
	RedrawEverything(); // the click sounded at the press
}

void ReleaseSpellBookButtons()
{
	PressedTab = -1;
	PressedIcon = {};
}

} // namespace devilution
