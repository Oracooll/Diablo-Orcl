#include "oracool/skill_picker.h"

#include <algorithm>
#include <optional>
#include <string>
#include <vector>

#include "DiabloUI/ui_flags.hpp"
#include "control.h"
#include "engine/backbuffer_state.hpp"
#include "engine/render/primitive_render.hpp"
#include "engine/render/text_render.hpp"
#include "panels/spell_book.hpp" // BuildSpellStatBlock - the hover shows the same numbers the sheet does
#include "panels/spell_icons.hpp"
#include "player.h"
#include "spells.h"
#include "utils/language.h"
#include "utils/str_cat.hpp"
#include "utils/ui_fwd.h"

#include "oracool/attack_skills.h"
#include "oracool/badge.h"
#include "oracool/auto_save.h"
#include "oracool/class_tree.h"
#include "oracool/cursor_tooltip.h" // ShowPanelStringsAsHintCard - the menus' card
#include "oracool/furious_charge.h" // GetSpellDisplayName
#include "oracool/hud_art.h"
#include "oracool/hud_layout.h"
#include "oracool/ornate_border.h"
#include "oracool/paladin_skills.h" // LacksShieldFor - a shield skill's red plate
#include "oracool/readied_spells.h"
#include "oracool/spell_ranks.h" // SpellRequiredLevel - spells sort as the Spells tab sorts them
#include "oracool/ui_sound.h"
#include "oracool/whirlwind.h" // WhirlwindRightButtonOnly
#include "oracool/window_close.h"

namespace devilution::oracool {

namespace {

bool PickerOpen = false;
/** Which button the open picker will bind. */
bool PickerForLeft = false;
/** Pixels the list is scrolled by; only ever non-zero when the content outgrows the screen. */
int PickerScroll = 0;
/** The spell under the cursor as of the last draw - what an F-key press binds. Invalid over none. */
SpellID HoveredPickerSpell = SpellID::Invalid;
/**
 * @brief The AURA under the cursor, or ClassTreeSkill::None.
 *
 * Tracked separately from the spell because an aura has no SpellID to be named by - its row carries
 * SpellID::Invalid by construction, an aura being a toggle rather than a cast. That is precisely why
 * F-keys refused auras until 2026-08-31 ("i cant set them on auras").
 */
ClassTreeSkill HoveredPickerAura = ClassTreeSkill::None;

/** What one cell stands for. Three kinds because three different things draw and bind differently. */
enum class EntryKind : uint8_t {
	/** Regular or Fist attack. Binding one means CLEARING the readied spell - see attack_skills.h. */
	Attack,
	/** A class-tree row: an invested active, or an aura. Auras toggle instead of binding. */
	Tree,
	/** A known spell - memorised from a book, or innate. Cast from mana. */
	Spell,
	/**
	 * The same spell as held by an equipped STAFF, which is a different thing to ready: it casts
	 * from the staff's charges rather than from mana, and it is gone when the staff is unequipped.
	 *
	 * Its own kind rather than a flag on Spell, because a character can hold a spell BOTH ways and
	 * the two must be separately bindable (user, 2026-09-03: "staff spells and learned spells
	 * should not overlap in one icon as they do now. they need to coexist with respective backing
	 * color"). They used to collapse into one cell, typed Charges, so a Sorceress who had read
	 * Fire Ball and picked up a Staff of Fire Ball lost the ability to bind the learned one.
	 */
	Staff,
	/**
	 * A spell the player holds a SCROLL of, castable from the scroll (user, 2026-09-05: "scrolls
	 * based spells. to have their own tier in the picker menu"). Its own kind for the same reason
	 * Staff is: the same spell can be known, on a staff and on a scroll at once, and each is a
	 * different thing to ready. Wears the engine's beige, the scroll's own colour.
	 */
	Scroll,
};

struct Entry {
	EntryKind kind;
	int attackIcon = 0;
	ClassTreeSkill tree = ClassTreeSkill::None;
	SpellID spell = SpellID::Invalid;
};

/**
 * @brief One heading and its run of entries (dev note, 2026-09-27: "subgroups 1/2/3/spells/staff/etc"): the attacks, the
 * Abilities window's three tree pages under their tab numbers, then spells, scrolls and the staff. Built with the entries,
 * and walked by the draw, the click, the hover and the scroll alike - one geometry.
 */
struct Section {
	std::string label;
	size_t first;
	size_t count;
};

// Geometry. 38 is not a free choice: the RMB well alternates between a strip icon and the engine's
// readied-spell icon at 37x38, and a picker whose cells disagreed with the well they fill would
// make the slot look like it resized on selection.
constexpr int IconSize = 38;
constexpr int Columns = 5;
constexpr int CellGap = 6;
constexpr int Padding = 14;
constexpr int HeaderHeight = 14;
constexpr int SectionGap = 8;
constexpr int TitleHeight = 18;
constexpr int ScreenMargin = 8;
/** Height of the rank badge band across the bottom of a cell. */
constexpr int LevelBandHeight = 12;

/** The window ground: the border's own shadow tone, proven in the shared upper half of the
 * palette so it cannot recolour itself by tileset. Same value Levski's Roar paints with - see the
 * note there on why a named index beats guessing at a ramp. */
constexpr uint8_t PanelFillColor = 204;

constexpr int GridWidth = Columns * IconSize + (Columns - 1) * CellGap;
constexpr int WindowWidth = GridWidth + 2 * Padding;

/** @brief Whether @p player knows @p spell from a book or by nature - NOT from a staff. */
bool IsSpellKnownTo(const Player &player, SpellID spell)
{
	// _pISpells is deliberately absent: a staff spell gets its own section and its own cells now,
	// so counting it here would list it twice in the same list under two different rules.
	const SpellMask known = player._pMemSpells | player._pAblSpells;
	return (known & GetSpellBitmask(spell)) != 0;
}

/** @brief Whether @p player has @p spell on an equipped staff right now. */
bool IsStaffSpellOf(const Player &player, SpellID spell)
{
	return (player._pISpells & GetSpellBitmask(spell)) != 0;
}

/**
 * @brief How @p player holds the spell in @p entry - which decides both its icon's palette and what
 * the button ends up storing.
 *
 * A Staff entry is Charges by construction; that is what the cell means. For everything else an
 * innate ability outranks a memorised spell, because that is the cheapest way to cast it. Charges
 * no longer appear in this answer at all - they cannot, or a spell held both ways would bind to the
 * staff from the cell that says "learned".
 */
SpellType SpellTypeFor(const Player &player, SpellID spell, bool fromStaff = false, bool fromScroll = false)
{
	if (fromStaff)
		return SpellType::Charges;
	if (fromScroll)
		return SpellType::Scroll;
	if ((player._pAblSpells & GetSpellBitmask(spell)) != 0)
		return SpellType::Skill;
	return SpellType::Spell;
}

/**
 * @brief Everything @p player can put on a mouse button right now, in sections: the attacks, the tree's three pages as the
 * Abilities window lays them out, then spells, scrolls and the staff.
 *
 * The filter is the whole reason this window is small. A tree row qualifies only if it is BUILT,
 * UNLOCKED and has a rank in it - which is the same three-part answer the Abilities window already
 * uses to decide whether a cell is live - and passives never qualify at all, because a passive
 * cannot be readied and an entry that does nothing when clicked is worse than an absent one.
 */
void BuildEntries(const Player &player, std::vector<Entry> &out, std::vector<Section> &sections)
{
	out.clear();
	sections.clear();
	size_t first = 0;
	const auto closeSection = [&](string_view label) {
		if (out.size() > first)
			sections.push_back({ std::string(label), first, out.size() - first });
		first = out.size();
	};

	for (size_t i = 0; i < AttackIconCount; i++)
		out.push_back({ EntryKind::Attack, static_cast<int>(AttackIconDisplayOrder[i]) });
	closeSection(_("ATTACKS"));

	const auto qualifies = [&](ClassTreeSkill skill) {
		const ClassTreeSkillData &data = GetClassTreeSkillData(skill);
		return data.heroClass == player._pClass && data.implemented && !IsClassTreeRowRetiredAsSpell(skill)
		    && data.kind != ClassTreeKind::Passive && IsClassTreeSkillUnlocked(player, skill)
		    && ClassTreeInvestment(player, skill) > 0;
	};
	// Deduplicated against the spell list below: a Paladin's Zeal is both a tree row and an ability,
	// and listing it twice would make the same click land in two places.
	SpellMask listedSpells;
	std::vector<bool> listedRows(ClassTreeSkillCount, false);
	const auto addRow = [&](ClassTreeSkill skill) {
		const SpellID slot = ClassTreeSpellId(skill);
		if (IsValidSpell(slot))
			listedSpells |= GetSpellBitmask(slot);
		listedRows[static_cast<size_t>(skill)] = true;
		out.push_back({ EntryKind::Tree, 0, skill, slot });
	};
	// The Abilities window's tabs 1, 2 and 3, each in that window's reading order: top-left to bottom-right.
	std::vector<ClassTreeSkill> page(MaxSkillsPerClass);
	for (int p = 0; p < 3; p++) {
		const size_t n = BuildClassTreePage(player._pClass, p, page.data());
		for (size_t i = 0; i < n; i++) {
			if (qualifies(page[i]))
				addRow(page[i]);
		}
		closeSection(StrCat(p + 1, " - ", GetClassTreePageName(player._pClass, p)));
	}
	// A readiable row no page lays out still gets its cell.
	for (size_t i = 0; i < ClassTreeSkillCount; i++) {
		const auto skill = static_cast<ClassTreeSkill>(i);
		if (!listedRows[i] && qualifies(skill))
			addRow(skill);
	}
	closeSection(_("SKILLS"));

	// Spells, scrolls and the staff in the Spells tab's order: level band, then name.
	const auto sortSpells = [&]() {
		std::sort(out.begin() + static_cast<std::ptrdiff_t>(first), out.end(), [](const Entry &a, const Entry &b) {
			const int bandA = SpellRequiredLevel(a.spell);
			const int bandB = SpellRequiredLevel(b.spell);
			if (bandA != bandB)
				return bandA < bandB;
			return GetSpellDisplayName(a.spell) < GetSpellDisplayName(b.spell);
		});
	};
	for (size_t i = 1; i < MAX_SPELLS; i++) {
		const auto spell = static_cast<SpellID>(i);
		if (!IsSpellKnownTo(player, spell))
			continue;
		if ((listedSpells & GetSpellBitmask(spell)) != 0)
			continue;
		out.push_back({ EntryKind::Spell, 0, ClassTreeSkill::None, spell });
	}
	sortSpells();
	closeSection(_("SPELLS"));

	// THE SCROLLS, their own section (user, 2026-09-05). Undeduplicated like the staff: a spell held
	// as a scroll is a different thing to ready from the same spell known, and the two coexist.
	for (size_t i = 1; i < MAX_SPELLS; i++) {
		const auto spell = static_cast<SpellID>(i);
		if ((player._pScrlSpells & GetSpellBitmask(spell)) != 0)
			out.push_back({ EntryKind::Scroll, 0, ClassTreeSkill::None, spell });
	}
	sortSpells();
	closeSection(_("SCROLLS"));

	// THE STAFF, last and undeduplicated. Not filtered against anything above: a spell held both
	// ways is two cells on purpose, and the staff cell is the one that spends charges. Its heading
	// shows only while a staff with a spell is equipped (user, 2026-09-03).
	for (size_t i = 1; i < MAX_SPELLS; i++) {
		const auto spell = static_cast<SpellID>(i);
		if (IsStaffSpellOf(player, spell))
			out.push_back({ EntryKind::Staff, 0, ClassTreeSkill::None, spell });
	}
	sortSpells();
	closeSection(_("STAFF SPELLS"));
}

/**
 * @brief Whether @p entry can go on the LEFT button.
 *
 * Only auras cannot. An aura is a toggle rather than a cast, it has no SpellID to store, and it has
 * always shown on the RMB well - so there is nothing for the left button to hold.
 *
 * The LMB picker still LISTS them, greyed (user, 2026-08-20: "either we filter out the unassignable
 * or we make their background dark gray. I say we make it dark gray, because that still allows us to
 * assign it directly to RMB to save time"). Filtering would have made the two pickers different
 * lists, and hunting for an aura would then mean closing one popup and opening the other.
 */
bool IsAssignableToLeft(const Entry &entry)
{
	if (WhirlwindRightButtonOnly(entry.spell))
		return false; // held on the right button (2026-09-29)
	return entry.kind != EntryKind::Tree
	    || GetClassTreeSkillData(entry.tree).kind != ClassTreeKind::Aura;
}

/** @brief What the hover popup calls @p entry. */
/**
 * @brief Adds a newline-separated block to the hover tooltip, one AddPanelString per line.
 *
 * The stat builders return a block because their own panels render one; the cursor tooltip takes a
 * line at a time and colours each. Splitting here rather than making the builders return a vector
 * keeps the sheets untouched.
 */
void AddPanelLines(const std::string &block)
{
	size_t start = 0;
	while (start < block.size()) {
		const size_t end = block.find('\n', start);
		std::string line = block.substr(start, end == std::string::npos ? std::string::npos : end - start);
		// The block's headings in gold, as the Abilities window draws them (user, 2026-09-05).
		if (!line.empty()) {
			const UiFlags color = IsHoverHeadingLine(line) ? UiFlags::ColorGold : UiFlags::ColorWhite;
			AddPanelString(std::move(line), color);
		}
		if (end == std::string::npos)
			break;
		start = end + 1;
	}
}

std::string EntryName(const Entry &entry)
{
	switch (entry.kind) {
	case EntryKind::Attack:
		return std::string(_(AttackIconName(static_cast<AttackIcon>(entry.attackIcon))));
	case EntryKind::Tree:
		return std::string(_(GetClassTreeSkillData(entry.tree).name));
	case EntryKind::Spell:
	case EntryKind::Staff:
	case EntryKind::Scroll:
		return std::string(GetSpellDisplayName(entry.spell));
	}
	return {};
}

/**
 * @brief The number in a cell's bottom band, or 0 to draw nothing.
 *
 * A tree row reports the points sunk into it; a spell reports its effective level, which is books
 * plus items and - since the 2026-08-20 rule - never skill points. The two are different quantities
 * wearing the same badge on purpose: both answer "how strong is this for me right now".
 */
int EntryLevel(const Player &player, const Entry &entry)
{
	switch (entry.kind) {
	case EntryKind::Attack:
		return 0; // a swing has no rank
	case EntryKind::Tree:
		return ClassTreeInvestment(player, entry.tree);
	case EntryKind::Spell:
	case EntryKind::Staff:
	case EntryKind::Scroll:
		return player.GetSpellLevel(entry.spell);
	}
	return 0;
}

int RowsFor(size_t count)
{
	return static_cast<int>((count + Columns - 1) / Columns);
}

/** @brief Height a section of @p count entries needs, header included. Zero when it is empty. */
int SectionHeight(size_t count)
{
	if (count == 0)
		return 0;
	const int rows = RowsFor(count);
	return HeaderHeight + rows * IconSize + (rows - 1) * CellGap;
}

/**
 * @brief Whether @p cell is wholly inside @p window's scrolling viewport.
 *
 * Shared by the draw and the click walks, and that sharing is the point. The draw skipped cells
 * outside the viewport while the click walk did not, so a cell scrolled up behind the title band
 * was invisible and still answered a click - you could bind something you could not see by clicking
 * the title. Two walks over one geometry that disagreed about a rule only one of them knew: the
 * exact failure mode this fork keeps re-learning, caught in the audit before the build shipped.
 */
bool IsCellVisible(const Rectangle &window, const Rectangle &cell)
{
	const int clipTop = window.position.y + Padding + TitleHeight;
	const int clipBottom = window.position.y + window.size.height - Padding;
	return cell.position.y >= clipTop && cell.position.y + IconSize <= clipBottom;
}

int ContentHeight(const std::vector<Section> &sections)
{
	int h = TitleHeight;
	for (const Section &section : sections)
		h += SectionHeight(section.count);
	return h + std::max(0, static_cast<int>(sections.size()) - 1) * SectionGap;
}

} // namespace

bool IsSkillPickerOpen() { return PickerOpen; }

void OpenSkillPicker(bool forLeftButton)
{
	PickerOpen = true;
	PickerForLeft = forLeftButton;
	PickerScroll = 0;
	HoveredPickerSpell = SpellID::Invalid;
	HoveredPickerAura = ClassTreeSkill::None;
}

void CloseSkillPicker()
{
	PickerOpen = false;
	PickerScroll = 0;
	HoveredPickerSpell = SpellID::Invalid;
	HoveredPickerAura = ClassTreeSkill::None;
}

bool IsSkillPickerForLeftButton()
{
	return PickerForLeft;
}

SpellID GetSkillPickerHoveredSpell()
{
	return PickerOpen ? HoveredPickerSpell : SpellID::Invalid;
}

ClassTreeSkill GetSkillPickerHoveredAura()
{
	return PickerOpen ? HoveredPickerAura : ClassTreeSkill::None;
}

Rectangle GetSkillPickerRect()
{
	if (!PickerOpen)
		return Rectangle { { 0, 0 }, { 0, 0 } };

	std::vector<Entry> entries;
	std::vector<Section> sections;
	BuildEntries(*MyPlayer, entries, sections);

	const int needed = ContentHeight(sections) + 2 * Padding;
	// Anchored to the plate, growing UPWARD, because that is the direction the button it belongs to
	// is in. Clamped at the top so a long list is shortened rather than run off the screen; the
	// wheel reaches whatever the clamp cut off.
	const Rectangle plate = GetMiddleHudRect();
	const int bottom = plate.position.y - CellGap;
	const int height = std::min(needed, bottom - ScreenMargin);

	// Horizontally over its own well, then clamped to the screen - the popup must read as belonging
	// to the button that opened it, but not at the price of hanging off an edge.
	const Rectangle well = PickerForLeft ? GetLmbSkillButtonRect() : GetRmbSkillButtonRect();
	int x = well.position.x + (well.size.width - WindowWidth) / 2;
	x = std::clamp(x, ScreenMargin, std::max(ScreenMargin, gnScreenWidth - WindowWidth - ScreenMargin));

	return Rectangle { { x, bottom - height }, { WindowWidth, height } };
}

void ScrollSkillPicker(int notches)
{
	if (!PickerOpen)
		return;

	// Bounded at BOTH ends. Without the upper bound the wheel would keep pushing the list past its
	// own end into an empty window - the failure that looks like the picker lost its contents.
	std::vector<Entry> entries;
	std::vector<Section> sections;
	BuildEntries(*MyPlayer, entries, sections);
	const int content = ContentHeight(sections);
	const Rectangle window = GetSkillPickerRect();
	const int visible = window.size.height - 2 * Padding - TitleHeight;
	const int maxScroll = std::max(0, content - TitleHeight - visible);

	PickerScroll = std::clamp(PickerScroll - notches * (IconSize + CellGap), 0, maxScroll);
}

void DrawSkillPicker(const Surface &out)
{
	if (!PickerOpen)
		return;

	const Player &player = *MyPlayer;
	std::vector<Entry> entries;
	std::vector<Section> sections;
	BuildEntries(player, entries, sections);

	// Cleared every frame, so moving off a cell un-hovers it. Without this an F-key would keep
	// binding whatever the cursor last touched, long after it left the window's cells.
	HoveredPickerSpell = SpellID::Invalid;
	HoveredPickerAura = ClassTreeSkill::None;

	const Rectangle window = GetSkillPickerRect();
	// The dark translucent backing the item tooltip and the books use (user, 2026-09-05: "background
	// of skill picker to be the transparent dark one, not the solid gold it is now"): two half
	// passes, so the icons read while the world still shows through. Was an opaque stone fill.
	DrawHalfTransparentRectTo(out, window.position.x, window.position.y, window.size.width, window.size.height);
	DrawHalfTransparentRectTo(out, window.position.x, window.position.y, window.size.width, window.size.height);
	DrawOrnateBorder(out, window);
	DrawWindowCloseButton(out, window);

	DrawString(out, PickerForLeft ? _("Left button") : _("Right button"),
	    { { window.position.x + Padding, window.position.y + Padding },
	        { window.size.width - 2 * Padding - WindowCloseButtonSize, TitleHeight } },
	    { UiFlags::ColorWhitegold | UiFlags::FontSize12 | UiFlags::VerticalCenter });

	int y = window.position.y + Padding + TitleHeight - PickerScroll;
	const int clipTop = window.position.y + Padding + TitleHeight;
	const int clipBottom = window.position.y + window.size.height - Padding;

	const auto drawSection = [&](const std::string &label, size_t first, size_t count) {
		if (count == 0)
			return;
		if (y + HeaderHeight > clipTop && y < clipBottom) {
			DrawString(out, label,
			    { { window.position.x + Padding, y }, { GridWidth, HeaderHeight } },
			    { UiFlags::ColorUiSilver | UiFlags::FontSize12 });
		}
		y += HeaderHeight;
		for (size_t i = 0; i < count; i++) {
			const int row = static_cast<int>(i) / Columns;
			const int column = static_cast<int>(i) % Columns;
			const Rectangle cell {
				{ window.position.x + Padding + column * (IconSize + CellGap),
				    y + row * (IconSize + CellGap) },
				{ IconSize, IconSize }
			};
			// Whole cells only: a half-drawn icon at the clip edge reads as a rendering fault
			// rather than as more content below. The click walk applies the SAME test.
			if (!IsCellVisible(window, cell))
				continue;
			const Entry &entry = entries[first + i];
			// An entry this button cannot take is greyed rather than hidden - the LMB picker's
			// auras. Grey is already this fork's "you cannot use this" plate everywhere else, so it
			// needs no new vocabulary. Clicking one still works: it toggles the aura, which lands on
			// the right button, and saving that trip is the whole reason they are listed here.
			const bool dimmed = PickerForLeft && !IsAssignableToLeft(entry);
			// A shield skill with no shield in hand is red, as on the wells (user, 2026-09-29).
			const SkillPlateTint tint = dimmed         ? SkillPlateTint::Locked
			    : LacksShieldFor(player, entry.spell) ? SkillPlateTint::Blocked
			                                          : SkillPlateTint::Ready;
			switch (entry.kind) {
			case EntryKind::Attack:
				DrawAttackIconScaledTo(out, cell, entry.attackIcon,
				    static_cast<AttackIcon>(entry.attackIcon) == BasicAttackIcon(player), tint);
				break;
			case EntryKind::Tree:
				// An aura carries no SpellID - it is a toggle, not a cast - so it can only be drawn
				// from its own tree art. An invested active prefers the strip icon it wears
				// everywhere else, and falls back to the engine's spell icon.
				if (!IsValidSpell(entry.spell)
				    || !TryDrawSkillSpellIcon(out, cell, entry.spell, tint)) {
					DrawClassTreeSkillInWell(out, cell, player._pClass,
					    ClassTreeIconIndex(entry.tree), tint);
				}
				break;
			case EntryKind::Spell:
				SetSpellTrans(SpellTypeFor(player, entry.spell));
				DrawSmallSpellIconFittedTo(out, cell, entry.spell);
				break;
			case EntryKind::Staff:
				// The engine's own orange charge ramp (user, 2026-09-03: "staff spells to use
				// legacy orange backing"). Asked for explicitly rather than derived, because the
				// whole point of this cell is that it means the staff even when the same spell is
				// also learned two sections above.
				SetSpellTrans(SpellType::Charges);
				DrawSmallSpellIconFittedTo(out, cell, entry.spell);
				break;
			case EntryKind::Scroll:
				// The engine's beige, a scroll's own colour (2026-09-05 coding).
				SetSpellTrans(SpellType::Scroll);
				DrawSmallSpellIconFittedTo(out, cell, entry.spell);
				break;
			}

			// The staff's remaining charges, bottom-left, wherever a spell icon is drawn (user,
			// 2026-09-03). Draws nothing for a cell that is not a staff holding.
			if (entry.kind == EntryKind::Staff)
				DrawStaffChargeBadge(out, cell, player, entry.spell);

			// The rank, bottom-RIGHT since 2026-09-02 (it was bottom-centre from 2026-08-20). Drawn
			// over the icon's own art rather than beside it, because the cell is 38px and a band
			// outside it would cost a row.
			if (const int level = EntryLevel(player, entry); level > 0)
				DrawBadge(out, cell, BadgeCorner::BottomRight, StrCat(level));

			// The F-key badge, top-right, for THIS list's button only - the Abilities window shows
			// both buttons in its two corners, but a quick list binds one button and showing the
			// other's key here would invite pressing it in the wrong window.
			const int auraKey = entry.kind == EntryKind::Tree
			        && GetClassTreeSkillData(entry.tree).kind == ClassTreeKind::Aura
			    ? GetAuraFKeyNumber(entry.tree)
			    : 0;
			if (const int fkey = auraKey != 0 ? auraKey : GetAbilityFKeyNumber(entry.spell, PickerForLeft);
			    fkey != 0) {
				DrawBadge(out, cell, BadgeCorner::TopRight, StrCat("F", fkey));
			}

			if (cell.contains(MousePosition)) {
				DrawHoverOutline(out, cell);
				// What an F-key press binds while this window is open (user, 2026-08-30: "we are
				// making F1-F8 hotkeys assignable from the quicklists, not from the abilities
				// windows"). Read from the draw for the same reason the Abilities window reads its
				// own hover from the draw: the cell rects are laid out here, and a second copy of
				// that walk is a second place for the two to disagree.
				HoveredPickerSpell = entry.spell;
				// And the aura, which has no SpellID to be carried by the line above.
				if (entry.kind == EntryKind::Tree
				    && GetClassTreeSkillData(entry.tree).kind == ClassTreeKind::Aura) {
					HoveredPickerAura = entry.tree;
				}
				// The hover popup rides the existing cursor tooltip, which renders InfoString after
				// every window in the draw order - so naming the entry here is the whole feature.
				// CheckCursMove leaves InfoString alone while the cursor is over a floating window,
				// which is exactly what makes this safe to set from a draw.
				SetPanelString(EntryName(entry), UiFlags::ColorWhite);
				if (dimmed)
					AddPanelString(_("Right button only - click to light it"));
				// The NUMBERS, not just the name (user, 2026-08-28: "i want more information in the
				// hover opoups of skills/spells/auras - include the benefits/bonuses current level
				// is providing and the bonuses/benefits the next level will provide").
				//
				// Both blocks are the ones their own sheets already show, called rather than
				// reimplemented: BuildSpellStatBlock for a spell (mana, damage now, damage at the
				// next level) and ClassTreeEffectLine for a tree row (points, what the next point
				// costs in character levels, an aura's radius now and next). A second copy of
				// either would be a second place for the next formula change to be forgotten.
				// The CURRENT rank only (user, 2026-09-05: "the other places can be truncated to
				// Name or Name, Current Level Stats") - the next rank is the Abilities window's.
				// A tree row wins over its SpellID (2026-09-26): the tree block is the spell's block plus
				// the level-up stat, which the spell block alone left out.
				AddPanelLines(entry.tree != ClassTreeSkill::None
				        ? ClassTreeEffectLine(*InspectPlayer, entry.tree, /*withNext=*/false)
				        : (entry.spell != SpellID::Invalid
				                  ? BuildSpellStatBlock(entry.spell, /*withNext=*/false)
				                  : std::string {}));
				// The vendors' gold card (dev note, 2026-09-28: "the tooltip design for griswold buttons - apply to
				// tooltips for lmb/rmb menus and hud tooltips").
				ShowPanelStringsAsHintCard();
			}
		}
		y += RowsFor(count) * IconSize + (RowsFor(count) - 1) * CellGap + SectionGap;
	};

	// While the cursor is inside this window the world's hover text is STALE, not absent:
	// CheckCursMove returns early over a floating window (IsPointOverFloatingWindow), so pcursmonst
	// and friends keep whatever they last saw and UpdateInfoString happily reprints it. Clearing
	// here - before the cells get their chance to name themselves - means a gap between two icons
	// says nothing rather than naming a monster somewhere behind the panel.
	//
	// Scoped to the window's own rect on purpose: outside it CheckCursMove runs normally, and
	// wiping the string unconditionally would kill the tooltip for anything the picker is not
	// covering.
	if (window.contains(MousePosition))
		ClearPanelStrings();

	for (const Section &section : sections)
		drawSection(section.label, section.first, section.count);
}

namespace {

/**
 * @brief Walks every VISIBLE cell of the open picker in draw order, calling @p visit(index, cell)
 * until it returns true. One geometry for the click, the F-key hover and the test.
 */
template <typename Visit>
void ForEachPickerCell(const Rectangle &window, const std::vector<Section> &sections, Visit &&visit)
{
	int y = window.position.y + Padding + TitleHeight - PickerScroll;
	for (const Section &section : sections) {
		const size_t first = section.first;
		const size_t count = section.count;
		if (count == 0)
			continue;
		y += HeaderHeight;
		for (size_t i = 0; i < count; i++) {
			const int row = static_cast<int>(i) / Columns;
			const int column = static_cast<int>(i) % Columns;
			const Rectangle cell {
				{ window.position.x + Padding + column * (IconSize + CellGap),
				    y + row * (IconSize + CellGap) },
				{ IconSize, IconSize }
			};
			// A cell scrolled out of the viewport is not there to be hit, however much its
			// coordinates still say it is - see IsCellVisible.
			if (!IsCellVisible(window, cell))
				continue;
			if (visit(first + i, cell))
				return;
		}
		y += RowsFor(count) * IconSize + (RowsFor(count) - 1) * CellGap + SectionGap;
	}
}

/** @brief The entry under @p point in the open picker, or nullopt. */
std::optional<size_t> PickerEntryAt(Point point, const Rectangle &window, const std::vector<Section> &sections)
{
	std::optional<size_t> hit;
	ForEachPickerCell(window, sections, [&](size_t index, const Rectangle &cell) {
		if (!cell.contains(point))
			return false;
		hit = index;
		return true;
	});
	return hit;
}

} // namespace

void RefreshSkillPickerHover()
{
	// The draw records the hovered cell as of the LAST frame. A key event between two draws - move
	// from A to B and press F1 before the next frame - bound A (external audit, 2026-09-06: UI-01).
	// This asks the same geometry the click asks, at the moment the key is pressed.
	HoveredPickerSpell = SpellID::Invalid;
	HoveredPickerAura = ClassTreeSkill::None;
	if (!PickerOpen || MyPlayer == nullptr)
		return;
	std::vector<Entry> entries;
	std::vector<Section> sections;
	BuildEntries(*MyPlayer, entries, sections);
	const Rectangle window = GetSkillPickerRect();
	if (!window.contains(MousePosition))
		return;
	const std::optional<size_t> hit = PickerEntryAt(MousePosition, window, sections);
	if (!hit.has_value())
		return;
	const Entry &entry = entries[*hit];
	HoveredPickerSpell = entry.spell;
	if (entry.kind == EntryKind::Tree && GetClassTreeSkillData(entry.tree).kind == ClassTreeKind::Aura)
		HoveredPickerAura = entry.tree;
}

Point GetSkillPickerCellCenter(size_t entryIndex)
{
	Point center { -1, -1 };
	if (!PickerOpen || MyPlayer == nullptr)
		return center;
	std::vector<Entry> entries;
	std::vector<Section> sections;
	BuildEntries(*MyPlayer, entries, sections);
	ForEachPickerCell(GetSkillPickerRect(), sections, [&](size_t index, const Rectangle &cell) {
		if (index != entryIndex)
			return false;
		center = cell.position + Displacement { cell.size.width / 2, cell.size.height / 2 };
		return true;
	});
	return center;
}

bool CheckSkillPickerClick(Point mousePosition)
{
	if (!PickerOpen)
		return false;

	const Rectangle window = GetSkillPickerRect();
	if (!window.contains(mousePosition)) {
		// The well that OWNS this picker toggles it, and consumes the click to do so. Without this
		// the click closed the picker here and the well's own handler - which runs immediately
		// afterwards - opened it straight back up, so the same well could never shut its own popup
		// and every attempt silently reset the scroll position.
		//
		// Only the owning well. A click on the OTHER well still falls through, so it switches
		// buttons in one click rather than needing one to dismiss and another to open.
		const Rectangle ownWell = PickerForLeft ? GetLmbSkillButtonRect() : GetRmbSkillButtonRect();
		if (ownWell.contains(mousePosition) && (SDL_GetModState() & KMOD_SHIFT) == 0) {
			CloseSkillPicker();
			PlayUiMoveSound(); // the well's own toggle, the same click that opened it
			return true;
		}
		// SHIFT is excluded from the toggle above (self-audit, 2026-08-21). Shift-clicking a well
		// CLEARS that button - the only way back to a plain attack - and consuming the click to
		// toggle would have swallowed that shortcut whenever the picker happened to be open. Falling
		// through closes the picker and lets the well's own handler do the clearing; it will not
		// reopen, because that handler returns after clearing.
		// Anywhere else: close, and let the click through to whatever it was aimed at. Swallowing it
		// would make cancelling cost two clicks, which is exactly the friction this window exists to
		// remove.
		CloseSkillPicker();
		return false;
	}

	if (CheckWindowCloseButtonClick(window, mousePosition)) {
		CloseSkillPicker();
		return true;
	}

	Player &player = *MyPlayer;
	std::vector<Entry> entries;
	std::vector<Section> sections;
	BuildEntries(player, entries, sections);

	// The same walk the draw does, in the same order, through PickerEntryAt - one geometry, so a
	// cell cannot be drawn in one place and clicked in another (or hovered in a third: the F-key
	// hover asks the same function, see RefreshSkillPickerHover).
	if (const std::optional<size_t> hit = PickerEntryAt(mousePosition, window, sections); hit.has_value()) {
		{
			const Entry &entry = entries[*hit];
			switch (entry.kind) {
			case EntryKind::Attack:
				// Readying the basic attack IS clearing the readied spell - see attack_skills.h.
				if (PickerForLeft) {
					player._pLRSpell = SpellID::Invalid;
					player._pLRSplType = SpellType::Invalid;
				} else {
					ClearReadiedSpell(player);
				}
				// Audit finding, 2026-08-26. Every OTHER branch of this switch schedules a save and
				// this one did not, on the unspoken assumption that going back to the basic attack
				// is not really a change. It is exactly as much of a change as the assignment that
				// put the skill there - and the one the player is most likely to make right before
				// a fight, which is right before a crash costs them it.
				oracool::ScheduleAutoSaveForSkillChange();
				PlayUiSelectSound();
				break;
			case EntryKind::Tree:
				if (GetClassTreeSkillData(entry.tree).kind == ClassTreeKind::Aura) {
					// An aura is lit, not bound: either button lights it, and picking the one already burning
					// keeps it burning (dev note, 2026-09-28). It has no business on the left button and never
					// displaces what is there.
					//
					// No interface click: lighting plays the aura's own start cue.
					SelectClassAura(player, entry.tree);
					CalcPlrInv(player, false);
					break;
				}
				[[fallthrough]];
			case EntryKind::Staff:
			case EntryKind::Scroll:
			case EntryKind::Spell: {
				if (!IsValidSpell(entry.spell))
					break;
				// A Staff cell binds as Charges; the Spell cell beside it binds as mana. That is the
				// whole of "they need to coexist".
				const SpellType type = SpellTypeFor(player, entry.spell, entry.kind == EntryKind::Staff, entry.kind == EntryKind::Scroll);
				// Whirlwind lands on the right button from either menu - it is held there (2026-09-29), as an aura is lit.
				if (PickerForLeft && !WhirlwindRightButtonOnly(entry.spell)) {
					player._pLRSpell = entry.spell;
					player._pLRSplType = type;
					oracool::ScheduleAutoSaveForSkillChange();
				} else {
					// The aura and the right button are one slot - see ClearClassAuraForRightButton.
					ClearClassAuraForRightButton(player);
					player._pRSpell = entry.spell;
					player._pRSplType = type;
					oracool::ScheduleAutoSaveForSkillChange();
				}
				PlayUiSelectSound();
				break;
			}
			}
			CloseSkillPicker();
			RedrawEverything();
			return true;
		}
	}

	// Inside the window but on no cell: absorbed, deliberately. A click that lands on the frame or
	// on a gap must not reach the world behind it.
	return true;
}

} // namespace devilution::oracool
