/**
 * @file oracool/inventory_layout.h
 *
 * Oracool V1 inventory panel geometry - the single source of truth for the 320x660
 * top-right inventory window, its equipment slots, its tab row and its item grid.
 *
 * This panel deliberately does NOT use SidePanelSize / GetRightPanel(). That 320x352
 * rect is shared with the spellbook, character sheet, quest log and stash, all of which
 * still want the old vertically-centred flyout placement; stretching it to 660 would drag
 * all four along with it. The inventory owns its own rect instead.
 *
 * Vertical budget (panel-relative, panel is 660 tall):
 *   18 - 340   equipment slots, helm down to boots
 *   368 - 396  tab row (10 tabs) + SORT button
 *   400 - 596  item grid, 7 rows of 28px
 *   596 - 660  decorative footer
 *
 * The footer exists so the grid clears the mana orb. The orb's top edge sits at screen y
 * 624; with the panel flush to the top of a 720-tall screen the grid ends at 596, so the
 * orb covers only the footer and never a live cell. Anything that moves the grid down, or
 * the orb up, breaks that and needs re-checking against OrbClearanceBottom below.
 */
#pragma once

#include "engine/point.hpp"
#include "engine/rectangle.hpp"
#include "engine/size.hpp"
#include "oracool/ornate_border.h" // OrnateBorderWidth - the grid reserves room for its own frame

namespace devilution {
namespace oracool {

/** @brief Size of the inventory window. Matches the source art, 1:1, no scaling. */
constexpr Size InventoryPanelSize { 340, 720 };

/** @brief Inset from the panel edge, matching the waypoint list and quest log's PanelMargin. */
constexpr int PanelMargin = 24;

/**
 * @brief Grid cell pitch. Locked to INV_SLOT_SIZE_PX (28) because item icons are fixed-size
 * sprites cut at 28px per cell in the original art - a 1x1 is 28x28, a 2x3 is 56x84. The
 * frame art is scaled to wrap this, not the other way round.
 */
constexpr int CellPx = 28;
constexpr Size CellSize { CellPx, CellPx };

constexpr Size GridSizeInCells { 10, 7 };
constexpr int GridCellCount = GridSizeInCells.width * GridSizeInCells.height;

/** @brief Panel-relative top-left of the item grid. Horizontally centred: (320 - 10*28) / 2. */
/**
 * @brief Panel-relative top-left of the item grid.
 *
 * Horizontally centred: (340 - 10*28) / 2. Vertically pinned so the grid's BOTTOM lands exactly on
 * OrbClearanceBottom - at 720 tall the panel now reaches the bottom of the screen, so the grid can
 * no longer simply sit where it fits; it has to stop where the mana orb starts.
 */
constexpr Point GridOrigin { (InventoryPanelSize.width - GridSizeInCells.width * CellPx) / 2,
	624 - OrnateBorderWidth - GridSizeInCells.height * CellPx };
constexpr int GridBottom = GridOrigin.y + GridSizeInCells.height * CellPx;

/**
 * @brief Tab row. One tab per grid column, butted together with no gap, so the ten tabs span
 * exactly the grid's width and each tab sits directly above its column.
 */
constexpr int TabCount = 10;
constexpr Size TabSize { CellPx, CellPx };
constexpr int GridWidth = GridSizeInCells.width * CellPx;
constexpr int TabRowX = GridOrigin.x;

/** @brief The row's bottom edge is flush with the top border of the grid's first row. */
constexpr int TabRowY = GridOrigin.y - TabSize.height;

static_assert(TabCount * TabSize.width == GridWidth,
    "Tab row no longer spans exactly the grid width");
static_assert(TabCount == GridSizeInCells.width,
    "There is no longer one tab per grid column");

/**
 * @brief How far the selected tab grows beyond an unselected one, on every side.
 *
 * Zero: every tab is the same size, and the selected one is distinguished by colour alone
 * (gold against silver). The growth machinery below is kept because it costs nothing while
 * this is 0 - GetActiveTabRect() collapses onto GetTabRect() - and raising it is the whole
 * change if a raised selected tab is ever wanted again.
 */
/**
 * @brief How far the selected tab grows beyond an unselected one.
 *
 * The selected tab is 32x31 against the others' 28x28, and it grows ASYMMETRICALLY: two pixels left
 * and right, three upward, and none downward. Keeping the bottom edge fixed is the point - all ten
 * tabs stay seated on the same line above the grid, and only the open one stands proud of it.
 */
constexpr int ActiveTabGrowSides = 2;
constexpr int ActiveTabGrowTop = 3;

/**
 * @brief Label drawn in tab @p index. All ten positions are storage tabs.
 *
 * Oracool: user request - the last position used to be "S", the SORT button, which meant the tab row
 * only opened nine of the ten pages. SORT is a text button in the footer now (GetSortButtonRect) and
 * this position is tab 10.
 *
 * That also un-strands a page. Storage was always ten deep - the vanilla backpack plus
 * Player::NumExtraInventoryTabs (9) - and the tenth had no way to be viewed, only drained by SORT.
 * Nothing about the save changes here; the page was being written all along.
 */
constexpr const char *TabLabel(int index)
{
	constexpr const char *Labels[TabCount] = { "1", "2", "3", "4", "5", "6", "7", "8", "9", "10" };
	return Labels[index];
}

constexpr int ActiveTabGrow = 0;

/** @brief Panel-relative rect of tab @p index (0 = the original backpack, 1-9 = extra tabs). */
constexpr Rectangle GetTabRect(int index)
{
	return { { TabRowX + index * TabSize.width, TabRowY }, TabSize };
}

/** @brief Rect of tab @p index when it is the selected one - inflated on all four sides. */
constexpr Rectangle GetActiveTabRect(int index)
{
	const Rectangle r = GetTabRect(index);
	return { { r.position.x - ActiveTabGrowSides, r.position.y - ActiveTabGrowTop },
		{ r.size.width + 2 * ActiveTabGrowSides, r.size.height + ActiveTabGrowTop } };
}

// The one invariant the growth must never break: all ten tabs stay seated on the same line, and
// only the open one stands proud of it.
static_assert(GetActiveTabRect(0).position.y + GetActiveTabRect(0).size.height
        == GetTabRect(0).position.y + GetTabRect(0).size.height,
    "Selected tab no longer shares its bottom edge with the unselected tabs");

// The tab row is inset from the panel margin by the slack the grid centring leaves (6px each side
// at 10 columns of 28 in a 340 panel), and the end tabs eat into that slack as they grow. These
// bound the growth against the margin rather than restating the size, which an earlier "is it
// 30x30?" assert did - that one only ever repeated GetActiveTabRect's own arithmetic back at it,
// so it could not catch anything, and it had to be edited every time the size was tuned.
static_assert(GetActiveTabRect(0).position.x >= PanelMargin,
    "Selected first tab now grows past the panel's left margin");
static_assert(GetActiveTabRect(TabCount - 1).position.x + GetActiveTabRect(TabCount - 1).size.width
        <= InventoryPanelSize.width - PanelMargin,
    "Selected last tab now grows past the panel's right margin");

/**
 * @brief Source cell size in the tab atlas, and where a cell sits relative to its logical tab.
 *
 * The reliquary-chest atlas (ui\inventory_tabs_chest.png) is three 34x31 frames - inactive, hover,
 * active - and every frame is the same size. The two unselected states carry their extra area as
 * transparent padding, which is what lets one blit position serve all three: the difference between
 * a flat tab and a raised one falls out of the artwork rather than out of placement maths.
 *
 * The cell is wider and taller than the 28x28 logical tab because the open tab's lip overhangs its
 * neighbours by 3px on each side and 3px above, with the bottom edge pinned - so all ten stay
 * seated on one line and only the open one stands proud. Hit-testing never sees the overhang; it
 * stays on GetTabRect.
 */
constexpr Size TabCellSize { 34, 31 };
constexpr Displacement TabCellOffset { -3, -3 };

static_assert(TabCellSize.width == TabSize.width - 2 * TabCellOffset.deltaX,
    "Tab atlas cell no longer matches the logical tab plus its overhang on both sides");
static_assert(TabCellSize.height == TabSize.height - TabCellOffset.deltaY,
    "Tab atlas cell no longer matches the logical tab plus its overhang above (bottom edge pinned)");

/** @brief Where to blit a tab cell so its logical 28x28 content lands on GetTabRect(index). */
constexpr Point GetTabCellOrigin(int index)
{
	return GetTabRect(index).position + TabCellOffset;
}

/**
 * @brief Screen y below which the mana orb draws over the panel. The grid must end above
 * this; see the file header. Static-asserted below rather than left as a comment.
 */
constexpr int OrbClearanceBottom = 624;

// The sygil constants that lived here are gone with the composed stone panel: the sygil was baked
// into ui\inventory_panel.png, which the shared theme replaced. The band it occupied is now the
// gold readout's - see GetGoldRowRect below.

// The grid's FRAME is what has to clear the orb, not the cells: the bevel is drawn outside the cell
// area now (DrawOrnateBorderOutside), so it reaches OrnateBorderWidth past GridBottom.
static_assert(GridBottom + OrnateBorderWidth <= OrbClearanceBottom,
    "Inventory grid or its frame extends into the mana orb - move the grid up or shorten it");
static_assert(GridBottom <= InventoryPanelSize.height,
    "Inventory grid extends past the bottom of the panel");
static_assert(TabRowY + TabSize.height == GridOrigin.y,
    "Tab row is no longer flush with the top of the grid's first row");
static_assert(GridWidth <= InventoryPanelSize.width,
    "Tab row is wider than the panel");

/**
 * @brief Screen rect of the inventory window: flush to the top-right corner.
 *
 * Flush rather than inset because 660 of a 720-tall screen leaves no room to inset - any
 * top margin pushes the bottom further into the orb.
 */
Rectangle GetInventoryPanelRect();

/**
 * @brief The thirteen paperdoll slots, in the order they are drawn and hit-tested.
 *
 * Seven map onto existing inv_body_loc values and carry real items today. The other six
 * (shoulders, gloves, bracers, belt, legs, boots) have no item types behind them yet - they
 * draw their frame and reject drops until those types exist. Kept in one enum so the layout
 * table stays a single block; EquipSlotBodyLoc() is what separates live from inert.
 */
enum class EquipSlot : uint8_t {
	Helm,
	Shoulders,
	Amulet,
	Chest,
	Gloves,
	Bracers,
	RingLeft,
	RingRight,
	Belt,
	Weapon,
	Shield,
	Legs,
	Boots,
	Count,
};
constexpr int EquipSlotCount = static_cast<int>(EquipSlot::Count);

/**
 * @brief Panel-relative equipment slot rects, sized in grid cells so they line up with the
 * item sprites that go in them (a 2x3 weapon is 56x84, exactly as the art expects).
 *
 * Three columns: left at x 34, centre at x 132 (a 2-wide slot centred in 320), right at
 * x 230. One-cell slots (the rings and the amulet) are centred within their column.
 */
struct EquipSlotLayout {
	EquipSlot slot;
	Rectangle rect;
};

// Widened 320 -> 340, so the side columns move out by half that each to stay symmetric about the
// centre column, which derives from the panel width and therefore moved on its own.
constexpr int EquipColLeft = 44;
constexpr int EquipColCentre = (InventoryPanelSize.width - 2 * CellPx) / 2;
constexpr int EquipColRight = 240;
static_assert(EquipColLeft + 2 * CellPx < EquipColCentre
        && EquipColCentre + 2 * CellPx < EquipColRight,
    "Equipment columns overlap");
static_assert(EquipColRight + 2 * CellPx <= InventoryPanelSize.width - PanelMargin,
    "Right equipment column runs into the panel margin");

/**
 * @brief Horizontal gap between the centre column (armor) and the side columns (gloves,
 * bracers). Everything else that needs a "one column away" measurement derives from this.
 */
constexpr int ArmorToSideGap = EquipColCentre - (EquipColLeft + 2 * CellPx);

/**
 * @brief Shoulders and amulet sit half that gap from the helm rather than a full one, so they
 * flank the head closely instead of lining up with the gloves/bracers column below them. This
 * is deliberate asymmetry: those two slots are inset relative to the rest of their column.
 */
constexpr int HeadFlankGap = ArmorToSideGap / 2;
constexpr int ShouldersX = EquipColCentre - HeadFlankGap - 2 * CellPx;
constexpr int AmuletX = EquipColCentre + 2 * CellPx + HeadFlankGap;

/**
 * @brief The rings share the belt's row - all three are one cell tall, so they line up exactly.
 */
// Every equipment row shifted down by EquipRowShift when the panel grew 660 -> 720: the block used
// to span 18..340 in a 320x660 panel and now sits centred in the 24..TabRowY space above the tabs.
constexpr int EquipRowShift = 33;
constexpr int BeltAndRingRowY = 182 + EquipRowShift;

/** @brief Bottom edge of the gloves/bracers row, which the ring row is spaced beneath. */
constexpr int GlovesRowBottom = 104 + EquipRowShift + 2 * CellPx;

/**
 * @brief Vertical gap above the ring row. Weapon and shield then sit the same distance below
 * it, so the left and right columns read as evenly spaced: gloves, gap, ring, gap, weapon.
 */
constexpr int RingRowGap = BeltAndRingRowY - GlovesRowBottom;
constexpr int WeaponRowY = BeltAndRingRowY + CellPx + RingRowGap;

/** @brief Builds a slot rect from a column x, a y, and a size in cells. */
constexpr Rectangle EquipRect(int x, int y, int cellsWide, int cellsHigh)
{
	return { { x, y }, { cellsWide * CellPx, cellsHigh * CellPx } };
}

/** @brief Centres a 1x1 slot inside a 2-cell-wide column. */
constexpr Rectangle EquipRect1x1(int columnX, int y)
{
	return { { columnX + CellPx / 2, y }, CellSize };
}

constexpr EquipSlotLayout EquipSlots[EquipSlotCount] = {
	{ EquipSlot::Helm, EquipRect(EquipColCentre, 18 + EquipRowShift, 2, 2) },
	{ EquipSlot::Shoulders, EquipRect(ShouldersX, 36 + EquipRowShift, 2, 2) },
	{ EquipSlot::Amulet, EquipRect(AmuletX, 48 + EquipRowShift, 1, 1) },
	{ EquipSlot::Chest, EquipRect(EquipColCentre, 88 + EquipRowShift, 2, 3) },
	{ EquipSlot::Gloves, EquipRect(EquipColLeft, 104 + EquipRowShift, 2, 2) },
	{ EquipSlot::Bracers, EquipRect(EquipColRight, 104 + EquipRowShift, 2, 2) },
	{ EquipSlot::RingLeft, EquipRect1x1(EquipColLeft, BeltAndRingRowY) },
	{ EquipSlot::RingRight, EquipRect1x1(EquipColRight, BeltAndRingRowY) },
	{ EquipSlot::Belt, EquipRect(EquipColCentre, BeltAndRingRowY, 2, 1) },
	{ EquipSlot::Weapon, EquipRect(EquipColLeft, WeaponRowY, 2, 3) },
	{ EquipSlot::Shield, EquipRect(EquipColRight, WeaponRowY, 2, 3) },
	{ EquipSlot::Legs, EquipRect(EquipColCentre, 220 + EquipRowShift, 2, 2) },
	{ EquipSlot::Boots, EquipRect(EquipColCentre, 284 + EquipRowShift, 2, 2) },
};


/** @brief Panel-relative rect of @p slot. */
constexpr Rectangle GetEquipSlotRect(EquipSlot slot)
{
	return EquipSlots[static_cast<int>(slot)].rect;
}

static_assert(GetEquipSlotRect(EquipSlot::Helm).position.y >= PanelMargin,
    "Equipment block starts above the panel margin");

static_assert(GetEquipSlotRect(EquipSlot::Boots).position.y
        + GetEquipSlotRect(EquipSlot::Boots).size.height
    <= TabRowY,
    "Boots slot runs into the tab row");

// The rings share the belt's row exactly.
static_assert(GetEquipSlotRect(EquipSlot::RingLeft).position.y == GetEquipSlotRect(EquipSlot::Belt).position.y
        && GetEquipSlotRect(EquipSlot::RingRight).position.y == GetEquipSlotRect(EquipSlot::Belt).position.y,
    "Ring slots are no longer aligned with the belt row");

// The ring row is evenly spaced between the gloves above it and the weapon below it.
static_assert(BeltAndRingRowY - GlovesRowBottom == WeaponRowY - (BeltAndRingRowY + CellPx),
    "Ring row is no longer evenly spaced between the gloves and weapon rows");

// Shoulders and amulet flank the helm at half the armor-to-side gap, and must not collide
// with it.
static_assert(ShouldersX + 2 * CellPx < EquipColCentre
        && AmuletX > EquipColCentre + 2 * CellPx,
    "Shoulders or amulet overlaps the helm slot");

/**
 * @brief SORT button: centred on the weapon column, midway between the bottom of the weapon
 * slot and the top of the tab row.
 *
 * Expressed relative to the weapon slot rather than as a literal position, so it follows if the
 * weapon row moves - which it already did once, when the ring row was aligned to the belt.
 * Declared here, after the equipment block, because it depends on those constants.
 */
constexpr int WeaponRowBottom = WeaponRowY + 3 * CellPx;

/**
 * @brief The footer: the SORT button under the grid, the gold readout under that.
 *
 * The band exists because the grid stops at OrbClearanceBottom while the panel runs the full 720 - so
 * all of it is vertically behind the mana orb at 960x720. Horizontally it depends on the LABEL: the
 * orb spans screen x 658..755 and these rows are centred at x 790, so a short one is clear of it and
 * a long one is not. The note here used to claim "clear of it" flatly, which was true of the rect and
 * false of the text inside it - "SORT INVENTORY" ran back to about x 735 and printed itself across
 * the glass. The label is "SORT" now, and the orb draws over these rows rather than under them
 * (see DrawInventoryFooter), so the two answers are belt and braces.
 *
 * SORT goes first, tucked under the grid where the user marked it, with gold below.
 */
constexpr int FooterRowHeight = 24;
constexpr int FooterRowGap = 8;
constexpr int GoldRowHeight = FooterRowHeight;

// The title band is oracool::PanelTitleTop / PanelTitleHeight, in ornate_border.h - shared by all
// five side panels rather than owned by this one.

/**
 * @brief SORT and the gold readout moved ABOVE the tab row (user request, 2026-08-16).
 *
 * They used to sit in a footer under the grid, which put the two things the player reads most at
 * the very bottom of a 720px panel. They now share one line 12px above the tabs: SORT over tabs
 * 1-2, the gold count over tabs 3-8, both on the same baseline.
 *
 * Splitting the line by TAB COLUMNS rather than by fractions is what keeps them aligned with the
 * grid beneath - each label starts exactly where a tab does, so nothing floats between columns.
 */
/**
 * @brief Clearance between the footer row's BOX and the logical tab row. 2, not 8 - user request
 * (2026-08-16): "move SORT and GOLD COUNTER 6px down. They are way up."
 *
 * Deliberately smaller than the open tab's 3px raised lip, which reaches TabRowY - 3: the box
 * overlaps that lip by a pixel. Harmless, because a box is not ink - both labels are the default
 * 12px face vertically centred in a 24px row, so roughly 6px of the box below the glyphs is empty
 * and the text itself stays clear of the tab art.
 */
constexpr int FooterAboveTabsGap = 2;
constexpr int FooterRowY = TabRowY - FooterAboveTabsGap - FooterRowHeight;

/** @brief The x of tab @p index's left edge, used to span the row above by whole tabs. */
constexpr int TabColumnX(int index)
{
	return TabRowX + index * TabSize.width;
}

/** @brief Oracool: user request - "a gold SORT INVENTORY text button", where tab "S" used to be. */
constexpr Rectangle GetSortButtonRect()
{
	// Tabs 1 and 2.
	return { { TabColumnX(0), FooterRowY }, { 2 * TabSize.width, FooterRowHeight } };
}

constexpr Rectangle GetGoldRowRect()
{
	// Tabs 3 through 8, on SORT's line.
	return { { TabColumnX(2), FooterRowY }, { 6 * TabSize.width, GoldRowHeight } };
}

static_assert(GetSortButtonRect().position.y + FooterRowHeight <= TabRowY,
    "SORT button overlaps the tab row");
static_assert(GetGoldRowRect().position.y == GetSortButtonRect().position.y,
    "Gold row is no longer on SORT's baseline");
static_assert(GetGoldRowRect().position.x >= GetSortButtonRect().position.x + GetSortButtonRect().size.width,
    "Gold row overlaps the SORT button");
static_assert(GetGoldRowRect().position.x + GetGoldRowRect().size.width <= TabColumnX(TabCount),
    "Gold row runs past the tab row it is aligned to");
static_assert(FooterRowY > 0, "The footer row was pushed off the top of the panel");

} // namespace oracool
} // namespace devilution
