#include "oracool/waypoint_menu.h"

#include <array>

#include "DiabloUI/ui_flags.hpp"
#include "control.h"
#include "diablo.h" // MousePosition, for the hover highlight
#include "engine/rectangle.hpp"
#include "engine/render/clx_render.hpp"
#include "engine/render/primitive_render.hpp" // DrawHalfTransparentRectTo
#include "engine/render/text_render.hpp"
#include "oracool/ornate_border.h"
#include "interfac.h"
#include "levels/gendung.h"
#include "multi.h"
#include "oracool/hud_art.h"
#include "player.h"
#include "quests.h"
#include "utils/language.h"

namespace devilution::oracool {

namespace {

// Oracool: user request - the 17 waypoints, numbered and worded exactly as specified. Index i
// (1-16) corresponds to dungeon level i, matching currlevel numbering - see
// Player::_pWaypointUnlocked and OperateWaypoint's _oVar1 usage. All 17 are placed
// (AddWaypointSigilObject, objects.cpp): town's is fixed, each dungeon level's is a fresh random
// floor tile every visit.
constexpr std::array<const char *, 17> WaypointNames { {
    "1. Tristram",
    "2. Cathedral Level 1",
    "3. Cathedral Level 2",
    "4. Cathedral Level 3",
    "5. Cathedral Level 4",
    "6. Catacombs Level 5",
    "7. Catacombs Level 6",
    "8. Catacombs Level 7",
    "9. Catacombs Level 8",
    "10. Caves Level 9",
    "11. Caves Level 10",
    "12. Caves Level 11",
    "13. Caves Level 12",
    "14. Hell Level 13",
    "15. Hell Level 14",
    "16. Hell Level 15",
    "17. Hell Level 16",
} };

// Oracool V1 waypoint list geometry.
//
//   0..24     top margin
//   24..74    label band, "WAYPOINT"
//   74..77    separator rule
//   77..101   gap below the rule
//   101..696  seventeen 35px rows
//   696..720  bottom margin
//
// The window is drawn the way the event log is - a half-transparent fill under the ornate
// textbox_frame00 bevel - rather than from the composed stone panel it used at first, which read as
// too heavy. ui\waypoint_panel.png is therefore no longer loaded; tools/BuildWaypointPanel.ps1
// still builds it if the heavier treatment is ever wanted back.
//
// PanelMargin is inset, not a drawn border - the ornate bevel is only OrnateBorderWidth (3px)
// thick, so the margin is what keeps rows clear of the frame.
//
// The window is full screen height. Growing 660 -> 720 freed 60px, spent on symmetry rather than
// on more rows: the top margin, the gap under the separator and the bottom margin are all one
// PanelMargin, so the list sits in an evenly inset block. The label band also grew 29 -> 50, which
// is what lets the FontSize30 title sit in its own rect instead of overflowing upward as it did
// when the band was shorter than the face.
constexpr Size PanelSize { 340, 720 };
constexpr int PanelMargin = 24;
constexpr int LabelHeight = 50;
constexpr int SeparatorHeight = OrnateBorderWidth;
constexpr int SeparatorGap = PanelMargin; // deliberately equal - see the symmetry note above
constexpr int RowHeight = 35;
constexpr int ListTop = PanelMargin + LabelHeight + SeparatorHeight + SeparatorGap;
constexpr int IconGap = 8;   // from the inner edge of the margin to the sigil
constexpr int TextGap = 10;  // from the sigil to the name

static_assert(ListTop + 17 * RowHeight + PanelMargin == PanelSize.height,
    "Waypoint list no longer fits its panel - the seventeen rows must span ListTop..PanelSize.height-PanelMargin");

/**
 * @brief Screen rect of the waypoint list: flush to the top-left corner.
 *
 * Deliberately its own rect rather than GetLeftPanel()'s. That one is 320x352 and shared with the
 * character sheet and quest log, exactly as the inventory found when it grew to 660 - stretching it
 * would drag those along. Top-left mirrors the inventory's top-right placement.
 */
Rectangle PanelRect()
{
	return { { 0, 0 }, PanelSize };
}

bool WaypointMenuOpen = false;
Point OpenedFromPosition;

// Oracool: user request - "spawn at the waypoint" flag, set right before a warp and consumed
// once by AddWaypointSigilObject() (objects.cpp) after it places the destination's sigil - see
// RequestSpawnAtWaypoint's doc comment for why it can't just set ViewPosition here instead.
bool WaypointSpawnRequested = false;

int MouseToEntry(Point mousePosition)
{
	const Rectangle panel = PanelRect();
	if (!panel.contains(mousePosition))
		return -1;
	// Above the first row - the label band and the top border are not clickable.
	const int y = mousePosition.y - (panel.position.y + ListTop);
	if (y < 0)
		return -1;
	const int index = y / RowHeight;
	if (index < 0 || static_cast<size_t>(index) >= WaypointNames.size())
		return -1;
	return index;
}

} // namespace

bool IsWaypointMenuOpen()
{
	return WaypointMenuOpen;
}

void OpenWaypointMenu(Point sigilPosition)
{
	WaypointMenuOpen = true;
	OpenedFromPosition = sigilPosition;
}

void CloseWaypointMenu()
{
	WaypointMenuOpen = false;
}

void DrawWaypointMenu(const Surface &out)
{
	// Oracool: user request - close instead of drawing once the player has walked away from the
	// sigil that opened this menu. See OpenWaypointMenu's doc comment for why this exists.
	if (MyPlayer->position.tile.WalkingDistance(OpenedFromPosition) > 1) {
		CloseWaypointMenu();
		return;
	}

	const Rectangle panel = PanelRect();

	// Same treatment as the event log: a half-transparent fill under the ornate bevel. No art asset
	// is involved, so there is nothing to fall back to and nothing to keep in step with a PNG.
	DrawHalfTransparentRectTo(out, panel.position.x, panel.position.y, panel.size.width, panel.size.height);
	DrawOrnateBorder(out, panel);

	// The title used to be baked into the panel art; with that gone it is drawn here, in FontSize30 -
	// the face the NPC gossip overlay uses (minitext.cpp's DrawQTextContent). Applied to the title
	// only; the rows keep the default face. The band is now taller than the face, so unlike the
	// first pass this rect no longer has to grow upward to avoid clipping.
	const Rectangle labelArea { { panel.position.x + PanelMargin, panel.position.y + PanelMargin },
		{ panel.size.width - 2 * PanelMargin, LabelHeight } };
	DrawOutlinedString(out, "WAYPOINT", labelArea,
	    UiFlags::ColorWhitegold | UiFlags::FontSize30 | UiFlags::AlignCenter | UiFlags::VerticalCenter);

	// Rule under the title, in the frame's own bevel colours. Spans the same inset as the rows, so
	// its ends line up with the text column rather than running edge to edge.
	DrawOrnateSeparator(out, { panel.position.x + PanelMargin, panel.position.y + PanelMargin + LabelHeight },
	    panel.size.width - 2 * PanelMargin);

	const Size iconSize = GetWaypointIconSize();
	const int iconX = panel.position.x + PanelMargin + IconGap;
	const int textX = iconX + iconSize.width + TextGap;

	// Hit-tested with the same function the click handler uses, so what lights up under the cursor
	// and what a click actually resolves to can never disagree.
	const int hovered = MouseToEntry(MousePosition);

	for (size_t i = 0; i < WaypointNames.size(); i++) {
		const bool unlocked = IsWaypointUnlocked(static_cast<int>(i));
		const bool isHovered = (hovered == static_cast<int>(i));
		const int rowTop = panel.position.y + ListTop + static_cast<int>(i) * RowHeight;

		// The pad is the waypoint's own art: lit for a waypoint the player has reached, dormant
		// otherwise - the same two states the in-world sigil uses.
		if (iconSize.height > 0)
			DrawWaypointIcon(out, { iconX, rowTop + (RowHeight - iconSize.height) / 2 }, unlocked);

		// Vertically centre the name in its row rather than sitting it on the row's top edge, so it
		// lines up with the sigil beside it.
		const Rectangle textArea { { textX, rowTop }, { panel.size.width - PanelMargin - (textX - panel.position.x), RowHeight } };

		// Gold for reached, plain white for not. Hover SWAPS the two rather than introducing a third
		// colour, so the row visibly reacts whichever state it is in. The sigil beside the name
		// carries the real state cue; colour is reinforcement.
		UiFlags color = unlocked ? UiFlags::ColorWhitegold : UiFlags::ColorWhite;
		if (isHovered)
			color = unlocked ? UiFlags::ColorWhite : UiFlags::ColorWhitegold;

		// No outline on the rows - it was there to hold contrast against the stone panel, and that
		// panel is gone; over the half-transparent fill it only thickened the glyphs. The title
		// keeps its outline. Default face too; the gossip font is title-only.
		DrawString(out, WaypointNames[i], textArea, { color | UiFlags::VerticalCenter });
	}
}

void CheckWaypointMenuClick(Point mousePosition)
{
	int entry = MouseToEntry(mousePosition);
	if (entry < 0)
		return;
	if (!IsWaypointUnlocked(entry))
		return; // locked entry - no-op

	CloseWaypointMenu();

	// Oracool: entry index doubles as the destination dungeon level (0 = town), matching
	// currlevel numbering - see WaypointNames' comment and OperateWaypoint's _oVar1 usage.
	if (!setlevel && MyPlayer->isOnLevel(entry))
		return; // already there

	WaypointSpawnRequested = true;
	StartNewLvl(*MyPlayer, WM_DIABNEXTLVL, entry);
}

bool IsWaypointUnlocked(int index)
{
	if (index == 0)
		return true; // Tristram - always unlocked, regardless of what's stored
	if (index < 0 || static_cast<size_t>(index) >= WaypointNames.size())
		return false;
	return MyPlayer->_pWaypointUnlocked[sgGameInitInfo.nDifficulty][index];
}

void UnlockWaypoint(int index)
{
	if (index <= 0 || static_cast<size_t>(index) >= WaypointNames.size())
		return;
	MyPlayer->_pWaypointUnlocked[sgGameInitInfo.nDifficulty][index] = true;
}

bool ConsumeWaypointSpawnRequest()
{
	if (!WaypointSpawnRequested)
		return false;
	WaypointSpawnRequested = false;
	return true;
}

} // namespace devilution::oracool
