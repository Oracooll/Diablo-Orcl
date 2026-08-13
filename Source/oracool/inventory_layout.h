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
	624 - GridSizeInCells.height * CellPx };
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
 * The selected tab is 30x30 against the others' 28x28, and it grows ASYMMETRICALLY: one pixel left
 * and right, two upward, and none downward. Keeping the bottom edge fixed is the point - all ten
 * tabs stay seated on the same line above the grid, and only the open one stands proud of it.
 */
constexpr int ActiveTabGrowSides = 1;
constexpr int ActiveTabGrowTop = 2;

/** @brief Index of the tab position that is the SORT button rather than a storage tab. */
constexpr int SortTabIndex = TabCount - 1;

/** @brief Label drawn in tab @p index: "1".."9" for the storage tabs, "S" for the sort button. */
constexpr const char *TabLabel(int index)
{
	constexpr const char *Labels[TabCount] = { "1", "2", "3", "4", "5", "6", "7", "8", "9", "S" };
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

// The selected tab is 30x30 and shares its bottom edge with the unselected ones.
static_assert(GetActiveTabRect(0).size.width == 30 && GetActiveTabRect(0).size.height == 30,
    "Selected tab is no longer 30x30");
static_assert(GetActiveTabRect(0).position.y + GetActiveTabRect(0).size.height
        == GetTabRect(0).position.y + GetTabRect(0).size.height,
    "Selected tab no longer shares its bottom edge with the unselected tabs");

/**
 * @brief Source cell size in the tab strip asset. Every cell is the *active* size; the two
 * unselected states are drawn inset within their cell, padded with transparency. That way a
 * tab is always blitted from a uniform grid at one position - GetTabRect() offset by
 * -ActiveTabGrow - and the size difference falls out of the artwork instead of needing
 * per-state placement maths at the call site.
 */
constexpr Size TabCellSize { TabSize.width + 2 * ActiveTabGrow, TabSize.height + 2 * ActiveTabGrow };

/** @brief Where to blit a tab cell so its inset content lands on GetTabRect(index). */
constexpr Point GetTabCellOrigin(int index)
{
	return GetActiveTabRect(index).position;
}

/**
 * @brief Screen y below which the mana orb draws over the panel. The grid must end above
 * this; see the file header. Static-asserted below rather than left as a comment.
 */
constexpr int OrbClearanceBottom = 624;

// The sygil constants that lived here are gone with the composed stone panel: the sygil was baked
// into ui\inventory_panel.png, which the shared theme replaced. The band it occupied is now the
// gold readout's - see GetGoldRowRect below.

static_assert(GridBottom <= OrbClearanceBottom,
    "Inventory grid extends into the mana orb - move the grid up or shorten it");
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
 * @brief The gold readout, centred in the band between the grid's bottom and the panel's.
 *
 * Replaces the standalone SORT button, which is now tab position SortTabIndex. The band exists
 * because the grid stops at OrbClearanceBottom while the panel runs to 720 - so this sits in space
 * the mana orb already covers at 960x720, and is fully visible at wider resolutions where the orb
 * clears the panel.
 */
constexpr int GoldRowHeight = 24;
constexpr Rectangle GetGoldRowRect()
{
	// Centred in the band between the grid's bottom edge and the panel's, rather than tucked just
	// under the grid.
	return { { PanelMargin, GridBottom + (InventoryPanelSize.height - GridBottom - GoldRowHeight) / 2 },
		{ InventoryPanelSize.width - 2 * PanelMargin, GoldRowHeight } };
}
static_assert(GetGoldRowRect().position.y + GoldRowHeight <= InventoryPanelSize.height,
    "Gold row runs past the bottom of the panel");

} // namespace oracool
} // namespace devilution
