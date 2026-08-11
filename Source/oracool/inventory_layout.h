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
constexpr Size InventoryPanelSize { 320, 660 };

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
constexpr Point GridOrigin { (InventoryPanelSize.width - GridSizeInCells.width * CellPx) / 2, 400 };
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
	return { { r.position.x - ActiveTabGrow, r.position.y - ActiveTabGrow },
		{ r.size.width + 2 * ActiveTabGrow, r.size.height + 2 * ActiveTabGrow } };
}

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

/**
 * @brief Inner edge of the panel's decorative bottom border. Measured off the background art:
 * scanning per-row mean luma up from the bottom, the carved frame occupies 651-659 and flat
 * stone resumes at 650.
 */
constexpr int PanelInnerBottom = 650;

/**
 * @brief The class sygil, centred in the footer between the grid and the panel's bottom border.
 *
 * Note this DOES sit partly under the mana orb at 960x720, where the orb covers everything
 * below y 624 - roughly the bottom half of the plaque. That is the requested placement:
 * centred in the footer proper, not in the orb-free part of it. At 1280 wide the orb clears
 * the panel entirely and the whole plaque is visible.
 */
constexpr int SygilBandTop = GridBottom;
constexpr int SygilBandBottom = PanelInnerBottom;
constexpr Size SygilSize { 163, 48 };
constexpr Point SygilPosition {
	(InventoryPanelSize.width - SygilSize.width) / 2,
	SygilBandTop + (SygilBandBottom - SygilBandTop - SygilSize.height) / 2
};
static_assert(SygilPosition.y >= GridBottom
        && SygilPosition.y + SygilSize.height <= PanelInnerBottom,
    "Sygil does not fit between the grid and the panel's bottom border");
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

constexpr int EquipColLeft = 34;
constexpr int EquipColCentre = (InventoryPanelSize.width - 2 * CellPx) / 2;
constexpr int EquipColRight = 230;

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
constexpr int BeltAndRingRowY = 182;

/** @brief Bottom edge of the gloves/bracers row, which the ring row is spaced beneath. */
constexpr int GlovesRowBottom = 104 + 2 * CellPx;

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
	{ EquipSlot::Helm, EquipRect(EquipColCentre, 18, 2, 2) },
	{ EquipSlot::Shoulders, EquipRect(ShouldersX, 36, 2, 2) },
	{ EquipSlot::Amulet, EquipRect(AmuletX, 48, 1, 1) },
	{ EquipSlot::Chest, EquipRect(EquipColCentre, 88, 2, 3) },
	{ EquipSlot::Gloves, EquipRect(EquipColLeft, 104, 2, 2) },
	{ EquipSlot::Bracers, EquipRect(EquipColRight, 104, 2, 2) },
	{ EquipSlot::RingLeft, EquipRect1x1(EquipColLeft, BeltAndRingRowY) },
	{ EquipSlot::RingRight, EquipRect1x1(EquipColRight, BeltAndRingRowY) },
	{ EquipSlot::Belt, EquipRect(EquipColCentre, BeltAndRingRowY, 2, 1) },
	{ EquipSlot::Weapon, EquipRect(EquipColLeft, WeaponRowY, 2, 3) },
	{ EquipSlot::Shield, EquipRect(EquipColRight, WeaponRowY, 2, 3) },
	{ EquipSlot::Legs, EquipRect(EquipColCentre, 220, 2, 2) },
	{ EquipSlot::Boots, EquipRect(EquipColCentre, 284, 2, 2) },
};

/** @brief Panel-relative rect of @p slot. */
constexpr Rectangle GetEquipSlotRect(EquipSlot slot)
{
	return EquipSlots[static_cast<int>(slot)].rect;
}

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
constexpr Size SortButtonSize { 28, 28 };
constexpr int SortButtonCentreX = EquipColLeft + CellPx; // centre of a 2-cell-wide column
constexpr int WeaponRowBottom = WeaponRowY + 3 * CellPx;
constexpr Rectangle GetSortButtonRect()
{
	return { { SortButtonCentreX - SortButtonSize.width / 2,
	             WeaponRowBottom + (TabRowY - WeaponRowBottom - SortButtonSize.height) / 2 },
		SortButtonSize };
}
static_assert(WeaponRowBottom + SortButtonSize.height <= TabRowY,
    "No room for the SORT button between the weapon slot and the tab row");

} // namespace oracool
} // namespace devilution
