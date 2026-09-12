#include "oracool/waypoint_menu.h"

#include <algorithm>
#include <array>
#include <string>

#include "DiabloUI/ui_flags.hpp"
#include "control.h"
#include "diablo.h" // MousePosition, for the hover highlight
#include "engine/rectangle.hpp"
#include "engine/render/clx_render.hpp"
#include "engine/render/primitive_render.hpp" // DrawHalfTransparentRectTo
#include "engine/render/text_render.hpp"
#include "oracool/area_level.h" // AreaLevel - the number in a waypoint's name
#include "oracool/ornate_border.h"
#include "init.h" // gbIsHellfire, for whether the Nest and Crypt rows exist at all
#include "interfac.h"
#include "inv.h"              // CloseInventory - see OpenWaypointMenu
#include "oracool/hud_menu.h" // CloseHudMenu - ditto
#include "levels/gendung.h"
#include "multi.h"
#include "oracool/hud_art.h"
#include "oracool/ui_sound.h"
#include "player.h"
#include "quests.h"
#include "utils/language.h"
#include "utils/str_cat.hpp" // StrCat - the derived waypoint names

namespace devilution::oracool {

namespace {

// Oracool: user request - the waypoints, worded exactly as specified. Index i (1-24) corresponds to
// dungeon level i, matching currlevel numbering - see Player::_pWaypointUnlocked and
// OperateWaypoint's _oVar1 usage. All are placed (AddWaypointSigilObject, objects.cpp): town's is
// fixed, each dungeon level's is a fresh random floor tile every visit.
//
// The ordinal prefix these carried ("6. Catacombs Level 5") was dropped on the user's call when the
// rows moved to FontSize24 - see the DrawString below. It cost 31px on the widest name, which was
// the difference between fitting the text column and wrapping. Nothing was lost with it: the row's
// position in the list already gave the ordinal, and the trailing number - the one that matters -
// is still there. Purely a display string; no code parses it.
//
// The ROW's position no longer carries the level either, since the list was reordered by depth on
// 2026-09-12 - LevelOfRow below is that mapping.
//
// That trailing number is the AREA LEVEL, and it is DERIVED rather than written out (user,
// 2026-09-12: "make a formula for naming waypoints - the level number in the name to be a variable
// equal to its Area level"). For floors 1-16 it is the floor, which is why the Catacombs start at 5
// and not at 1. For Hellfire's two regions it is not: the Nest reads 9-12 and the Crypt 13-16,
// because that is the depth they actually are - AreaLevel side-steps floors 17-24 onto the Caves'
// and Hell's rungs (see oracool/area_level.h, v1.11.060).
//
// So "Caves Level 9" and "Nest Level 9" both exist, and that is the point: they are the same depth
// in two different places, and the region word is what tells them apart.
constexpr size_t WaypointLevelCount = 25;

/**
 * @brief The region a dungeon level belongs to - the word in front of the number.
 *
 * Through AreaNameOfFloor rather than a table of its own: area_level.cpp already holds the six names
 * in floor order and is where the ladder lives, so a second copy here would be the thing that goes
 * stale if an area is ever renamed. Only Tristram is this file's, because it is not a dungeon floor
 * and AreaNameOfFloor clamps level 0 up to the Cathedral.
 */
const char *WaypointRegionName(int level)
{
	return level <= 0 ? "Tristram" : AreaNameOfFloor(level);
}

/**
 * @brief "<Region> Level <area level>" for dungeon level @p level, or "Tristram" for 0.
 *
 * Built once and kept, rather than formatted per frame: the list redraws every frame while it is
 * open and these strings never change within a run.
 *
 * DIFF_NORMAL deliberately. The ladder adds a flat block per difficulty, so Normal's value IS the
 * rung; a name that climbed with the difficulty would be describing the run rather than the place,
 * and the row would rename itself under the player between games.
 */
string_view WaypointName(int level)
{
	static std::array<std::string, WaypointLevelCount> names;
	if (names[0].empty()) {
		names[0] = WaypointRegionName(0);
		for (size_t i = 1; i < names.size(); i++)
			names[i] = StrCat(WaypointRegionName(static_cast<int>(i)), " Level ", AreaLevel(static_cast<int>(i), DIFF_NORMAL));
	}
	return names[static_cast<size_t>(level)];
}

/**
 * @brief The dungeon level each row shows, in the order the rows appear.
 *
 * Ordered by AREA LEVEL, not by dungeon level (user, 2026-09-12: "nest waypoints to be [...] placed
 * between Hell and Caves waypoints", "Crypt to be [...] placed after Hell waypoints"). The Nest
 * shares the Caves' rungs and the Crypt shares Hell's, so sorting by depth interleaves them:
 * Caves 9-12, Nest 9-12, Hell 13-16, Crypt 13-16. Dungeon levels 17-24 are therefore NOT in numeric
 * order here.
 *
 * This is why the row index is no longer the destination level. LevelOfRow is the single mapping,
 * and Player::_pWaypointUnlocked and OperateWaypoint's _oVar1 stay indexed by DUNGEON level - which
 * is exactly what this table exists to keep separate from the display.
 */
constexpr std::array<uint8_t, WaypointLevelCount> HellfireRowLevels { {
    0,
    1, 2, 3, 4,     // Cathedral, rungs 1-4
    5, 6, 7, 8,     // Catacombs, rungs 5-8
    9, 10, 11, 12,  // Caves,     rungs 9-12
    17, 18, 19, 20, // Nest,      rungs 9-12 as well
    13, 14, 15, 16, // Hell,      rungs 13-16
    21, 22, 23, 24, // Crypt,     rungs 13-16 as well
} };

/**
 * @brief The same list for a plain Diablo game, where the Nest and the Crypt do not exist.
 *
 * Not a prefix of the table above - with the Nest interleaved, the first seventeen rows there are no
 * longer levels 0-16 - so the shorter order is written out rather than sliced off. It needs no
 * reordering at all, because without Hellfire's two regions depth and dungeon level agree.
 */
constexpr std::array<uint8_t, 17> DiabloRowLevels { {
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16,
} };

/** @brief The dungeon level at list row @p row, or -1 if the row does not exist. */
int LevelOfRow(size_t row)
{
	if (gbIsHellfire)
		return row < HellfireRowLevels.size() ? static_cast<int>(HellfireRowLevels[row]) : -1;
	return row < DiabloRowLevels.size() ? static_cast<int>(DiabloRowLevels[row]) : -1;
}

/**
 * @brief How many rows the list actually offers.
 *
 * The storage behind it is always 25 (see Player::_pWaypointUnlocked), but a plain Diablo game has
 * no levels past 16 and AddWaypointSigilObject places no sigil there, so listing the Nest and the
 * Crypt outside Hellfire would be eight rows that can never light up and can never be travelled
 * to. Keyed on `gbIsHellfire` rather than on HaveMonk(): these are levels, and levels are exactly
 * what hellfire.mpq brings.
 */
size_t VisibleWaypointCount()
{
	return gbIsHellfire ? HellfireRowLevels.size() : DiabloRowLevels.size();
}

// Oracool V1 waypoint list geometry.
//
//   0..24     top margin
//   24..74    label band, "WAYPOINT"
//   74..77    separator rule
//   77..101   gap below the rule
//   87..625   the scrolling list viewport (538px, twelve 43px rows on a 45px pitch)
//   625..720  the limestone panel's frieze - deliberately left to the art
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
// Width stays 340 even though the rows moved up to FontSize24, because the names were shortened to
// suit rather than the panel widened to fit them - the user's call, and the better trade: 28px of
// screen is worth more than an ordinal the row's own position already tells you.
//
// MEASURED, not estimated. The glyph widths were read out of fonts\24-00.clx and summed the way
// GetLineWidth does, per name. The text column is PanelSize.width - 102 (RightPad + ScrollbarWidth
// + ScrollbarGap + textX) = 238px, and at FontSize24 the widest of the 25 names is "Catacombs
// Level 5" at 224px - 14px of slack. With the old "6. " prefixes it was 255px and eight names would
// have wrapped to a clipped second line.
constexpr Size PanelSize { 340, 720 };
constexpr int PanelMargin = 24;
constexpr int LabelHeight = 50;
constexpr int SeparatorHeight = OrnateBorderWidth;
constexpr int SeparatorGap = PanelMargin; // deliberately equal - see the symmetry note above
// Oracool: user request (2026-08-14) - "make wp row 43px + 2px gap between two rows. make the wp
// picture fit 43x43 invisible frame as to its diameter tangents on the 43px row borders."
//
// 43 is also exactly what a vanilla spellbook row was (SpellBookDescription's height), so the pad
// and the name sit on the same rhythm the rest of the game's lists use. The pad fills the row
// outright rather than being inset - ui\waypoint_icons.png is cut at 43x43 per cell for this, see
// tools/CutWaypointIcons.ps1.
constexpr int RowHeight = 43;
/** @brief Air between one row and the next. Not part of either row's hit box. */
constexpr int RowGap = 2;
constexpr int RowPitch = RowHeight + RowGap;
/**
 * @brief Top of the scrolling viewport.
 *
 * Lifted off the margin stack (user request, 2026-08-18: "move the scrollable window 10px up to step
 * away from overlapping the Limestone Theme footer"). The lift is 14 rather than 10, because 14 is
 * what makes twelve FULL rows end exactly on the frieze's first row at y 625 - the stated reason for
 * moving it at all. At 10 the twelfth row would have been clipped four pixels short, which is the
 * overlap the request was trying to remove.
 */
constexpr int ListLift = 14;
constexpr int ListTop = PanelMargin + LabelHeight + SeparatorHeight + SeparatorGap - ListLift;
constexpr int IconGap = 8;   // from the inner edge of the margin to the sigil
constexpr int TextGap = 10;  // from the sigil to the name

/**
 * @brief The list's visible window - TWELVE rows (user request, 2026-08-18). 25 rows are 1125px
 * tall, so the list scrolls inside this.
 *
 * Twelve full rows rather than "whatever fits above the bottom margin", which was 595px - thirteen
 * rows and a sliver, running to y 696 and straight across the limestone panel's frieze.
 */
constexpr int ViewportHeight = 12 * RowPitch - RowGap;
static_assert(ViewportHeight > 0, "the waypoint list viewport must fit between the rule and the bottom margin");
static_assert(ListTop + ViewportHeight <= 625,
    "the waypoint list now runs across the limestone panel's frieze - raise ListTop or drop a row");

// The scrollbar is the Abilities window's, to the pixel - see DrawScrollbar in panels/spell_book.cpp.
// Two scrolling lists in the same game that disagree about how a scrollbar looks is one list too many.
constexpr int ScrollbarWidth = OrnateBorderWidth;
constexpr int ScrollbarMinThumb = 24;
constexpr int ScrollbarGap = 6;
constexpr int RightPad = 8;
/** @brief Right edge available to a row's text - short of the scrollbar, not of the panel. */
constexpr int ContentRightLimit = PanelSize.width - RightPad - ScrollbarWidth - ScrollbarGap;

/**
 * @brief Screen rect of the waypoint list: flush to the top-left corner.
 *
 * Deliberately its own rect rather than GetLeftPanel()'s. That one is 320x352 and shared with the
 * character sheet and quest log, exactly as the inventory found when it grew to 660 - stretching it
 * would drag those along. Top-left mirrors the inventory's top-right placement.
 */
Rectangle PanelRect()
{
	// Was pinned at the top-left corner. Bottom-docked with every other limestone window
	// (user, 2026-08-27).
	return { { 0, BottomDockedTop(PanelSize.height) }, PanelSize };
}

bool WaypointMenuOpen = false;
Point OpenedFromPosition;

/** @brief Pixels of list scrolled above the top of the viewport. Reset each time the menu opens. */
int ScrollOffset = 0;
int MaxScrollOffset = 0;
int ListHeight = 0;

// Oracool: user request - "spawn at the waypoint" flag, set right before a warp and consumed
// once by AddWaypointSigilObject() (objects.cpp) after it places the destination's sigil - see
// RequestSpawnAtWaypoint's doc comment for why it can't just set ViewPosition here instead.
bool WaypointSpawnRequested = false;

/** @brief Recomputes the scroll extent from the current row count and re-clamps the offset. */
void UpdateScrollBounds()
{
	ListHeight = static_cast<int>(VisibleWaypointCount()) * RowPitch;
	MaxScrollOffset = std::max(0, ListHeight - ViewportHeight);
	ScrollOffset = std::clamp(ScrollOffset, 0, MaxScrollOffset);
}

int MouseToEntry(Point mousePosition)
{
	const Rectangle panel = PanelRect();
	if (!panel.contains(mousePosition))
		return -1;
	// Above the first row - the label band and the top border are not clickable - or past the
	// bottom of the viewport, in the panel's bottom margin.
	const int y = mousePosition.y - (panel.position.y + ListTop);
	if (y < 0 || y >= ViewportHeight)
		return -1;

	const int listY = y + ScrollOffset;
	// The gap between two rows belongs to neither, so a click landing in it misses rather than
	// being rounded into whichever row happens to be above.
	if (listY % RowPitch >= RowHeight)
		return -1;
	const int index = listY / RowPitch;
	if (index < 0 || static_cast<size_t>(index) >= VisibleWaypointCount())
		return -1;
	return index;
}

/** @brief The theme's scrollbar: a recessed groove in the right margin with a bevelled thumb. */
void DrawScrollbar(const Surface &out, const Rectangle &panel)
{
	if (MaxScrollOffset <= 0)
		return;

	const int x = panel.position.x + PanelSize.width - RightPad - ScrollbarWidth;
	const int top = panel.position.y + ListTop;
	DrawThemedFill(out, { { x, top }, { ScrollbarWidth, ViewportHeight } }, 2);

	const int thumbHeight = std::max(ScrollbarMinThumb, ViewportHeight * ViewportHeight / ListHeight);
	const int travel = ViewportHeight - thumbHeight;
	const int thumbY = top + travel * ScrollOffset / MaxScrollOffset;
	DrawOrnateSeparatorVertical(out, { x, thumbY }, thumbHeight);
}

} // namespace

Rectangle GetWaypointMenuRect()
{
	return PanelRect();
}

bool IsWaypointMenuOpen()
{
	return WaypointMenuOpen;
}

void OpenWaypointMenu(Point sigilPosition)
{
	// Oracool: user request - clicking a sigil with the quest log or character sheet open used to
	// leave them stacked on top of the travel list, since both live on the left of the screen. The
	// list now takes the screen for itself, like every other window in this build.
	//
	// The left-panel siblings come from TakeLeftPanelSlot rather than a hand-written list. The list
	// that used to be here closed the two windows ABOVE this one in precedence but not the stash,
	// which is also above it - so clicking a sigil with the stash open opened this menu invisibly
	// behind it (audit, 2026-08-31, alongside the reported stash-behind-character-sheet case).
	//
	// It deliberately does not take the whole screen the way ClosePanels would: the inventory and
	// spellbook are the RIGHT panel, not this slot, and closing them is this window's own choice.
	TakeLeftPanelSlot(LeftPanelContent::WaypointMenu);
	CloseInventory();
	sbookflag = false;
	CloseHudMenu();

	WaypointMenuOpen = true;
	OpenedFromPosition = sigilPosition;
	// Back to Tristram at the top every time, the same reset-on-open the event log does. Reopening
	// where you last scrolled to would be a small surprise every single time.
	ScrollOffset = 0;
}

// One row per wheel notch, so the list moves by the thing it is made of rather than by a pixel
// count that happens to feel right. UpdateScrollBounds first because the row count depends on
// gbIsHellfire and the wheel can arrive before the first draw has run.
void ScrollWaypointMenuUp()
{
	UpdateScrollBounds();
	ScrollOffset = std::max(0, ScrollOffset - RowPitch);
}

void ScrollWaypointMenuDown()
{
	UpdateScrollBounds();
	ScrollOffset = std::min(MaxScrollOffset, ScrollOffset + RowPitch);
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

	// Oracool (2026-08-16): the shared painted side-panel background - see quests.cpp for the note.
	// The half-transparent fill and bevel stay as the fallback, so the art is droppable.
	if (HasSidePanelArt()) {
		DrawSidePanelArt(out, panel.position);
	} else {
		DrawHalfTransparentRectTo(out, panel.position.x, panel.position.y, panel.size.width, panel.size.height);
		DrawOrnateBorder(out, panel);
	}

	// The title used to be baked into the panel art; with that gone it is drawn here, in FontSize30 -
	// the face the NPC gossip overlay uses (minitext.cpp's DrawQTextContent). Applied to the title
	// only; the rows keep the default face.
	const Rectangle labelArea { { panel.position.x + PanelMargin, panel.position.y + PanelTitleTop },
		{ panel.size.width - 2 * PanelMargin, PanelTitleHeight } };
	DrawOutlinedString(out, "WAYPOINT", labelArea,
	    UiFlags::ColorWhitegold | UiFlags::FontSize30 | UiFlags::AlignCenter | UiFlags::VerticalCenter);

	UpdateScrollBounds();
	DrawScrollbar(out, panel);

	// Rows draw through a subregion covering only the scrolling area, so a row straddling its top
	// or bottom edge is clipped there rather than spilling onto the separator above or the bottom
	// bevel below. Everything from here on is in viewport-local coordinates, not screen ones.
	const Surface content = out.subregion(panel.position.x, panel.position.y + ListTop,
	    panel.size.width, ViewportHeight);

	const Size iconSize = GetWaypointIconSize();
	const int iconX = PanelMargin + IconGap;
	const int textX = iconX + iconSize.width + TextGap;

	// Hit-tested with the same function the click handler uses, so what lights up under the cursor
	// and what a click actually resolves to can never disagree - including after scrolling, which
	// is exactly the case where two separate implementations would drift apart.
	const int hovered = MouseToEntry(MousePosition);

	const size_t count = VisibleWaypointCount();
	for (size_t i = 0; i < count; i++) {
		const int rowTop = static_cast<int>(i) * RowPitch - ScrollOffset;
		// Wholly outside the viewport. The subregion would clip it anyway; skipping saves drawing
		// twenty of the twenty-five rows every frame.
		if (rowTop + RowHeight <= 0 || rowTop >= ViewportHeight)
			continue;

		// The row's DUNGEON level - not the row index, which is only where it sits on screen since
		// the list was reordered by depth. See LevelOfRow.
		const int level = LevelOfRow(i);
		const bool unlocked = IsWaypointUnlocked(level);
		const bool isHovered = (hovered == static_cast<int>(i));

		// Oracool: user request (2026-08-15) - the same gold outline the Abilities window marks its
		// hovered row with; since 2026-09-05 the heavy one ("apply same hover rectangle theme in
		// Waypoints list"): three pixels with a half-transparent shadow, on the row's own rect, since
		// a row here wears no bezel and casts no shadow to clear. Before the icon and the name, so it
		// frames them rather than striking through. Spans the row's clickable width, stopping short
		// of the scrollbar, so what lights up is exactly what a click would take.
		if (isHovered)
			DrawHoverOutlineHeavy(content, { { PanelMargin, rowTop }, { ContentRightLimit - PanelMargin, RowHeight } }, /*clearanceX=*/0, /*clearanceY=*/0);

		// The pad is the waypoint's own art: lit for a waypoint the player has reached, dormant
		// otherwise - the same two states the in-world sigil uses. It is cut to the row's exact
		// height (43x43, see tools/CutWaypointIcons.ps1), so this centring term is zero today and
		// stays correct if the art is ever recut smaller.
		if (iconSize.height > 0)
			DrawWaypointIcon(content, { iconX, rowTop + (RowHeight - iconSize.height) / 2 }, unlocked);

		// Vertically centre the name in its row rather than sitting it on the row's top edge, so it
		// lines up with the sigil beside it. Stops short of the scrollbar, not of the panel edge.
		const Rectangle textArea { { textX, rowTop }, { ContentRightLimit - textX, RowHeight } };

		// Oracool: user request - gold for LOCKED, white for unlocked. The inverse of what this was,
		// and of the usual instinct to highlight what you can use. It reads better here because of
		// what the two states mean on this screen: an unlocked row is somewhere you can go now and a
		// locked one is somewhere you have not been, so the gold marks what is left to find rather
		// than what is already done.
		//
		// Hover does NOT change it (user request, 2026-08-18: "remove text colour alternation when
		// hovering over the WPs"). Swapping the pair made the hovered row report the OTHER state's
		// colour, so a moment's glance at a hovered row read it backwards. The gold rectangle drawn
		// around the row is the hover cue, and it is unambiguous.
		// WHITE and RED (user, 2026-09-03: "revert blue waypoints to white. i wanna see it with
		// shadows"). The pair was white/whitegold until 2026-09-02, then blue/red for one version.
		//
		// Two changes went in together that day and only one of them was the fix. The colours were
		// hard to read on the new mid-grey stone, and RED sorted the locked rows out - it is the one
		// of the three that is not a pale colour on a mid tone. But the unlocked row is meant to be
		// the plain one, and dark blue made it the loud one; the shadow added the next version is
		// what white was missing all along, since a pale glyph on grey loses its edges rather than
		// its brightness.
		const UiFlags color = unlocked ? UiFlags::ColorWhite : UiFlags::ColorRed;

		// No outline on the rows - it was there to hold contrast against the stone panel, and that
		// panel is gone; over the half-transparent fill it only thickened the glyphs. The title
		// keeps its outline.
		//
		// FontSize24, NOT the 22px FontSizeDialog these rows used until now, and the reason is the
		// colours above rather than the size. MEASURED off a screenshot: at FontSizeDialog a locked
		// row, an unlocked row and the hovered row all rendered the identical (255,189,189), while
		// the panel's own FontSize30 title in the same frame came out gold (221,196,126). The 22px
		// face is a separate asset (FontSizes = {12,24,30,42,46,22} -> fonts\22-XX.clx) and its ink
		// does not sit in the 192-207 range every colour .trn remaps, so EVERY colour flag was a
		// no-op on it. These rows have never actually been two colours - the old comment here
		// claiming "gold for reached, plain white for not" described an intention, not the screen.
		//
		// 12 and 24 are the only other faces small enough for a 43px row (LineHeights: 26px at 24
		// against a 43px row) and 12 is half the size, so 24 is the choice. It only fits because the
		// names lost their ordinal prefix at the same time - see WaypointName and PanelSize.
		// Shadowed (user, 2026-09-03), which is what the blue and red of the day before actually
		// needed to work: a DARK colour on mid-grey stone loses its edges rather than its brightness,
		// and a black offset copy under it is what gives the glyph a boundary again. The outline this
		// row used to wear was dropped for thickening the letters; a shadow sits under them instead
		// of around them, so it buys the contrast without the weight.
		DrawString(content, WaypointName(level), textArea,
		    { color | UiFlags::FontSize24 | UiFlags::VerticalCenter | UiFlags::Shadowed });
	}
}

void CheckWaypointMenuClick(Point mousePosition)
{
	const int entry = MouseToEntry(mousePosition);
	if (entry < 0)
		return;
	// The row index STOPPED being the destination level when the list was reordered by depth
	// (2026-09-12): row 13 is the Nest's first floor, dungeon level 17. Everything past this line
	// speaks in dungeon levels, which is what _pWaypointUnlocked and StartNewLvl both want.
	const int level = LevelOfRow(static_cast<size_t>(entry));
	if (level < 0)
		return;
	if (!IsWaypointUnlocked(level))
		return; // locked entry - no-op

	// Travelling and "already there" are both a row chosen, and both close the list.
	PlayUiSelectSound();
	CloseWaypointMenu();

	if (!setlevel && MyPlayer->isOnLevel(level))
		return; // already there

	WaypointSpawnRequested = true;
	StartNewLvl(*MyPlayer, WM_DIABNEXTLVL, level);
}

bool IsWaypointUnlocked(int index)
{
	if (index == 0)
		return true; // Tristram - always unlocked, regardless of what's stored
	if (index < 0 || static_cast<size_t>(index) >= WaypointLevelCount)
		return false;
	return MyPlayer->_pWaypointUnlocked[sgGameInitInfo.nDifficulty][index];
}

void UnlockWaypoint(int index)
{
	if (index <= 0 || static_cast<size_t>(index) >= WaypointLevelCount)
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

void SetWaypointSpawnRequestForTest()
{
	WaypointSpawnRequested = true;
}

void ResetWaypointMenuForNewGame()
{
	WaypointMenuOpen = false;
	ScrollOffset = 0;
	// The one that is not merely cosmetic: an unconsumed request survives into the next character's
	// first level load and moves them onto its waypoint. See the header for how it is left unconsumed.
	WaypointSpawnRequested = false;
}

} // namespace devilution::oracool
