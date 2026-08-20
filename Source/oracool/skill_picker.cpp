#include "oracool/skill_picker.h"

#include <algorithm>
#include <string>
#include <vector>

#include "DiabloUI/ui_flags.hpp"
#include "control.h"
#include "engine/backbuffer_state.hpp"
#include "engine/render/primitive_render.hpp"
#include "engine/render/text_render.hpp"
#include "panels/spell_icons.hpp"
#include "player.h"
#include "spells.h"
#include "utils/language.h"
#include "utils/ui_fwd.h"

#include "oracool/attack_skills.h"
#include "oracool/class_tree.h"
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

/** The window ground: the border's own shadow tone, proven in the shared upper half of the
 * palette so it cannot recolour itself by tileset. Same value Levski's Roar paints with - see the
 * note there on why a named index beats guessing at a ramp. */
constexpr uint8_t PanelFillColor = 204;

constexpr int GridWidth = Columns * IconSize + (Columns - 1) * CellGap;
constexpr int WindowWidth = GridWidth + 2 * Padding;

/** @brief Whether @p player knows @p spell well enough for it to be worth offering. */
bool IsSpellKnownTo(const Player &player, SpellID spell)
{
	const uint64_t known = player._pMemSpells | player._pISpells | player._pAblSpells;
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
	const uint64_t mask = GetSpellBitmask(spell);
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
	uint64_t listedSpells = 0;
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
}

void CloseSkillPicker()
{
	PickerOpen = false;
	PickerScroll = 0;
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
			// rather than as more content below.
			if (cell.position.y < clipTop || cell.position.y + IconSize > clipBottom)
				continue;
			const Entry &entry = entries[first + i];
			switch (entry.kind) {
			case EntryKind::Attack:
				DrawAttackIconScaledTo(out, cell, entry.attackIcon,
				    static_cast<AttackIcon>(entry.attackIcon) == BasicAttackIcon(player),
				    SkillPlateTint::Green);
				break;
			case EntryKind::Tree:
				// An aura carries no SpellID - it is a toggle, not a cast - so it can only be drawn
				// from its own tree art. An invested active prefers the strip icon it wears
				// everywhere else, and falls back to the engine's spell icon.
				if (!IsValidSpell(entry.spell)
				    || !TryDrawSkillSpellIcon(out, cell, entry.spell, SkillPlateTint::Green)) {
					DrawClassTreeSkillInWell(out, cell, player._pClass,
					    ClassTreeIconIndex(entry.tree), SkillPlateTint::Green);
				}
				break;
			case EntryKind::Spell:
				SetSpellTrans(SpellTypeFor(player, entry.spell));
				DrawSmallSpellIconFittedTo(out, cell, entry.spell);
				break;
			}
			if (cell.contains(MousePosition))
				DrawHoverOutline(out, cell);
		}
		y += RowsFor(count) * IconSize + (RowsFor(count) - 1) * CellGap + SectionGap;
	};

	drawSection(N_("Skills"), 0, skills);
	drawSection(N_("Spells"), skills, entries.size() - skills);
}

bool CheckSkillPickerClick(Point mousePosition)
{
	if (!PickerOpen)
		return false;

	const Rectangle window = GetSkillPickerRect();
	if (!window.contains(mousePosition)) {
		// Outside: close, and let the click through to whatever it was aimed at. Swallowing it would
		// make cancelling cost two clicks, which is exactly the friction this window exists to
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
			if (!cell.contains(mousePosition))
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
				} else {
					// The aura and the right button are one slot - see ClearClassAuraForRightButton.
					ClearClassAuraForRightButton(player);
					player._pRSpell = entry.spell;
					player._pRSplType = type;
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
