#include "oracool/waypoint_menu.h"

#include <array>

#include "DiabloUI/ui_flags.hpp"
#include "control.h"
#include "engine/rectangle.hpp"
#include "engine/render/clx_render.hpp"
#include "engine/render/text_render.hpp"
#include "interfac.h"
#include "levels/gendung.h"
#include "multi.h"
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

constexpr Rectangle InnerPanel { { 32, 26 }, { 280, 300 } };
constexpr int LineHeight = 16;

bool WaypointMenuOpen = false;
Point OpenedFromPosition;

// Oracool: user request - "spawn at the waypoint" flag, set right before a warp and consumed
// once by AddWaypointSigilObject() (objects.cpp) after it places the destination's sigil - see
// RequestSpawnAtWaypoint's doc comment for why it can't just set ViewPosition here instead.
bool WaypointSpawnRequested = false;

int MouseToEntry(Point mousePosition)
{
	Rectangle innerArea = InnerPanel;
	innerArea.position += Displacement(GetLeftPanel().position.x, GetLeftPanel().position.y);
	if (!innerArea.contains(mousePosition))
		return -1;
	int y = mousePosition.y - innerArea.position.y;
	int index = y / LineHeight;
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

	// Oracool: reuses the Quest Log's own parchment panel art (pQLogCel, quests.cpp/quests.h) -
	// already loaded for the whole session regardless of whether the Quest Log itself is
	// currently open, and this panel is never shown at the same time as the real Quest Log.
	ClxDraw(out, GetPanelPosition(UiPanels::Quest, { 0, 351 }), (*pQLogCel)[0]);

	const int x = InnerPanel.position.x;
	int y = InnerPanel.position.y;
	for (size_t i = 0; i < WaypointNames.size(); i++) {
		const bool unlocked = IsWaypointUnlocked(static_cast<int>(i));
		const UiFlags color = unlocked ? UiFlags::ColorWhite : UiFlags::ColorWhitegold;
		DrawString(out, WaypointNames[i], GetPanelPosition(UiPanels::Quest, { x, y }), { color });
		y += LineHeight;
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
