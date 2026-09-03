#include "oracool/skill_picker.h"

#include <algorithm>
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
#include "oracool/furious_charge.h" // GetSpellDisplayName
#include "oracool/hud_art.h"
#include "oracool/hud_layout.h"
#include "oracool/ornate_border.h"
#include "oracool/readied_spells.h"
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
	/** A known spell. */
	Spell,
};

struct Entry {
	EntryKind kind;
	int attackIcon = 0;
	ClassTreeSkill tree = ClassTreeSkill::None;
	SpellID spell = SpellID::Invalid;
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

/** @brief Whether @p player knows @p spell well enough for it to be worth offering. */
bool IsSpellKnownTo(const Player &player, SpellID spell)
{
	const SpellMask known = player._pMemSpells | player._pISpells | player._pAblSpells;
	return (known & GetSpellBitmask(spell)) != 0;
}

/**
 * @brief How @p player holds @p spell - which decides both its icon's palette and what the button
 * ends up storing.
 *
 * Computed here rather than borrowed from the Abilities window's GetSBookTrans, which is file-local
 * there. The order matters and matches the engine's: an innate ability outranks a staff charge,
 * which outranks a memorised spell, because that is the cheapest way to cast it.
 */
SpellType SpellTypeFor(const Player &player, SpellID spell)
{
	const SpellMask mask = GetSpellBitmask(spell);
	if ((player._pAblSpells & mask) != 0)
		return SpellType::Skill;
	if ((player._pISpells & mask) != 0)
		return SpellType::Charges;
	return SpellType::Spell;
}

/**
 * @brief Everything @p player can put on a mouse button right now, attacks first, then skills, then
 * spells.
 *
 * The filter is the whole reason this window is small. A tree row qualifies only if it is BUILT,
 * UNLOCKED and has a rank in it - which is the same three-part answer the Abilities window already
 * uses to decide whether a cell is live - and passives never qualify at all, because a passive
 * cannot be readied and an entry that does nothing when clicked is worse than an absent one.
 */
void BuildEntries(const Player &player, std::vector<Entry> &out, size_t &attackCount, size_t &treeCount)
{
	out.clear();

	for (size_t i = 0; i < AttackIconCount; i++)
		out.push_back({ EntryKind::Attack, static_cast<int>(AttackIconDisplayOrder[i]) });
	attackCount = out.size();

	// Deduplicated against the spell list below: a Paladin's Zeal is both a tree row and an ability,
	// and listing it twice would make the same click land in two places.
	SpellMask listedSpells;
	for (size_t i = 0; i < ClassTreeSkillCount; i++) {
		const auto skill = static_cast<ClassTreeSkill>(i);
		const ClassTreeSkillData &data = GetClassTreeSkillData(skill);
		if (data.heroClass != player._pClass)
			continue;
		if (!data.implemented || IsClassTreeRowRetiredAsSpell(skill))
			continue;
		if (data.kind == ClassTreeKind::Passive)
			continue;
		if (!IsClassTreeSkillUnlocked(player, skill) || ClassTreeInvestment(player, skill) <= 0)
			continue;
		const SpellID slot = ClassTreeSpellId(skill);
		if (IsValidSpell(slot))
			listedSpells |= GetSpellBitmask(slot);
		out.push_back({ EntryKind::Tree, 0, skill, slot });
	}
	treeCount = out.size() - attackCount;

	for (size_t i = 1; i < MAX_SPELLS; i++) {
		const auto spell = static_cast<SpellID>(i);
		if (!IsSpellKnownTo(player, spell))
			continue;
		if ((listedSpells & GetSpellBitmask(spell)) != 0)
			continue;
		out.push_back({ EntryKind::Spell, 0, ClassTreeSkill::None, spell });
	}
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
		const std::string line = block.substr(start, end == std::string::npos ? std::string::npos : end - start);
		if (!line.empty())
			AddPanelString(line);
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

int ContentHeight(size_t attacks, size_t trees, size_t spells)
{
	int h = TitleHeight;
	int sections = 0;
	for (const size_t n : { attacks + trees, spells }) {
		if (n == 0)
			continue;
		h += SectionHeight(n);
		sections++;
	}
	return h + std::max(0, sections - 1) * SectionGap;
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
	size_t attacks = 0;
	size_t trees = 0;
	BuildEntries(*MyPlayer, entries, attacks, trees);
	const size_t spells = entries.size() - attacks - trees;

	const int needed = ContentHeight(attacks, trees, spells) + 2 * Padding;
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
	size_t attacks = 0;
	size_t trees = 0;
	BuildEntries(*MyPlayer, entries, attacks, trees);
	const int content = ContentHeight(attacks, trees, entries.size() - attacks - trees);
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
	size_t attacks = 0;
	size_t trees = 0;
	BuildEntries(player, entries, attacks, trees);
	const size_t skills = attacks + trees;

	// Cleared every frame, so moving off a cell un-hovers it. Without this an F-key would keep
	// binding whatever the cursor last touched, long after it left the window's cells.
	HoveredPickerSpell = SpellID::Invalid;
	HoveredPickerAura = ClassTreeSkill::None;

	const Rectangle window = GetSkillPickerRect();
	// Opaque backing first. The concept mock drew icons straight onto the dungeon and the world read
	// through them as noise; every other window in this fork gets a fill and the ornate frame.
	FillRect(out, window.position.x, window.position.y, window.size.width, window.size.height,
	    PanelFillColor);
	DrawOrnateBorder(out, window);
	DrawWindowCloseButton(out, window);

	DrawString(out, PickerForLeft ? _("Left button") : _("Right button"),
	    { { window.position.x + Padding, window.position.y + Padding },
	        { window.size.width - 2 * Padding - WindowCloseButtonSize, TitleHeight } },
	    { UiFlags::ColorWhitegold | UiFlags::FontSize12 | UiFlags::VerticalCenter });

	int y = window.position.y + Padding + TitleHeight - PickerScroll;
	const int clipTop = window.position.y + Padding + TitleHeight;
	const int clipBottom = window.position.y + window.size.height - Padding;

	const auto drawSection = [&](const char *label, size_t first, size_t count) {
		if (count == 0)
			return;
		if (y + HeaderHeight > clipTop && y < clipBottom) {
			DrawString(out, _(label),
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
			const SkillPlateTint tint = dimmed ? SkillPlateTint::Grey : SkillPlateTint::Green;
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
			}

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
				AddPanelLines(entry.spell != SpellID::Invalid
				        ? BuildSpellStatBlock(entry.spell)
				        : (entry.tree != ClassTreeSkill::None
				                  ? ClassTreeEffectLine(*InspectPlayer, entry.tree)
				                  : std::string {}));
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

	drawSection(N_("Skills"), 0, skills);
	drawSection(N_("Spells"), skills, entries.size() - skills);
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
	size_t attacks = 0;
	size_t trees = 0;
	BuildEntries(player, entries, attacks, trees);
	const size_t skills = attacks + trees;

	// The same walk the draw does, in the same order - one geometry, asked twice, so a cell cannot
	// be drawn in one place and clicked in another.
	int y = window.position.y + Padding + TitleHeight - PickerScroll;
	for (int section = 0; section < 2; section++) {
		const size_t first = section == 0 ? 0 : skills;
		const size_t count = section == 0 ? skills : entries.size() - skills;
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
			// A cell scrolled out of the viewport is not there to be clicked, however much its
			// coordinates still say it is - see IsCellVisible.
			if (!IsCellVisible(window, cell) || !cell.contains(mousePosition))
				continue;
			const Entry &entry = entries[first + i];
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
				break;
			case EntryKind::Tree:
				if (GetClassTreeSkillData(entry.tree).kind == ClassTreeKind::Aura) {
					// An aura is a toggle, not a binding: either button lights it, and clicking a
					// burning one puts it out. It has no business on the left button and never
					// displaces what is there.
					ToggleClassAura(player, entry.tree);
					CalcPlrInv(player, false);
					break;
				}
				[[fallthrough]];
			case EntryKind::Spell: {
				if (!IsValidSpell(entry.spell))
					break;
				const SpellType type = SpellTypeFor(player, entry.spell);
				if (PickerForLeft) {
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
				break;
			}
			}
			CloseSkillPicker();
			RedrawEverything();
			return true;
		}
		y += RowsFor(count) * IconSize + (RowsFor(count) - 1) * CellGap + SectionGap;
	}

	// Inside the window but on no cell: absorbed, deliberately. A click that lands on the frame or
	// on a gap must not reach the world behind it.
	return true;
}

} // namespace devilution::oracool
