/**
 * @file xpbar.cpp
 *
 * Adds XP bar QoL feature
 *
 * Oracool: HUD art pass, compact redesign (2026-08-10) - the vanilla XP bar was retired when the
 * user's second plate design dropped the XP groove. The entry points were left as no-ops rather
 * than unwired from their call sites (scrollrt.cpp, control.cpp's CheckPanelInfo, diablo.cpp's
 * Init/Free), so that bringing a bar back would be a one-file change here rather than a call-site
 * hunt. 2026-08-30 is that day: "lets introduce a gold exp bar dead in the vertical middle between
 * the exp counter and the top edge of the belt. make it 8px thick with rounded corners and also
 * rounded indents every 10%."
 *
 * Nothing outside this file changed to bring it back, which is the note above paying off.
 */
#include "xpbar.h"

#include <algorithm>

#include "control.h" // talkflag - the bar hides while chat is open
#include "engine/palette.h"
#include "engine/render/primitive_render.hpp"
#include "options.h"
#include "oracool/hud_layout.h"
#include "oracool/xp_counter.h"
#include "player.h"
#include "playerdat.hpp" // ExpLvlsTbl - where this level began
#include "utils/ui_fwd.h"

namespace devilution {

namespace {

/** @brief The user's number: "make it 8px thick" (2026-08-30), then "make xp bar half as thick"
 * (2026-09-05). The top edge stayed where it was and the bottom half went, which is what "remove
 * the bottom half to increase gap between belt" asks - see GapAboveBelt. */
constexpr int BarHeight = 4;
/**
 * @brief How far each end row is pulled in, giving the rounded corners.
 *
 * Two rows of inset on an eight-tall bar - 2px on the outermost row, 1px on the next - which is as
 * round as a shape this thin can read without losing its ends entirely. Indexed by distance from
 * the nearest long edge, so the same table rounds the top and the bottom.
 */
constexpr int CornerInset[] = { 0 }; // square since the frame (2026-09-26): the frame's cut corners are the rounding now

// The ten-segment notches went with the halving (user, 2026-09-05: "remove the 10% indents") - a
// 2px bite out of a 4px bar would have cut it in two.

/** @brief Gold, from the palette's own yellow ramp - the colour the HUD's other gold text uses. */
constexpr uint8_t FilledColor = PAL16_YELLOW + 4;
/** @brief The unfilled groove: the ornate border's shadow tone, so the bar sits in the HUD's family. */
constexpr uint8_t EmptyColor = 204;
/** @brief The edge, a shade darker than the groove, so the bar has an outline at both states. */
constexpr uint8_t EdgeColor = 0;
/** @brief The frame round the bar and its ten-percent marks (user, 2026-09-26: "put a grey frame around the exp
 * bar + verticals on each 10%"). Grey from the palette's own ramp; the marks a shade darker, so the frame reads
 * as the edge and the marks as a scale inside it. */
constexpr uint8_t FrameColor = PAL16_GRAY + 7;
constexpr uint8_t TickColor = PAL16_GRAY + 10;

} // namespace

/**
 * @brief The bar's screen rect: the belt's width, centred in the gap above it.
 *
 * DERIVED, not placed. The top edge comes from the XP counter's own draw rect and the bottom from
 * the belt's first cell, so "dead in the vertical middle between the exp counter and the top edge
 * of the belt" stays true when either of them moves - and both have moved twice this week.
 *
 * The width is the belt RUN rather than the plate, because the belt is what the request anchors to
 * and because the plateless row is wider than the plate; spanning the plate would leave the bar
 * visibly narrower than the thing underneath it.
 */
Rectangle GetXPBarRect()
{
	// Since 2026-09-05 (user: "align xp bar closer to belt") the bar hangs from the belt run's top
	// edge - the painted belt bar on the plate, the cells on the plateless row - with one pixel of
	// air, and no longer from the counter, which is hidden now and drawn above the bar when it
	// shows (xp_counter.cpp). CELLS, not slots: the run is the row's geometry, whichever slot is
	// drawn where.
	const Rectangle firstCell = oracool::GetBeltCellRect(0);
	const Rectangle lastCell = oracool::GetBeltCellRect(oracool::BeltVisibleSlotCount - 1);
	const int left = firstCell.position.x;
	const int width = lastCell.position.x + lastCell.size.width - left;
	// 5 = the old 1px of air plus the four rows the bar lost when it was halved (2026-09-05): the
	// bar's TOP is where it was, and the gap to the belt grew by exactly the removed half.
	constexpr int GapAboveBelt = 5;
	const int top = oracool::GetBeltRunTop() - GapAboveBelt - BarHeight;
	return Rectangle { { left, top }, { width, BarHeight } };
}

namespace {

/** @brief How far along its current level the player is, as a fraction of the bar's width. */
int FilledWidth(const Player &player, int barWidth)
{
	if (player._pLevel >= static_cast<int>(MaxCharacterLevel))
		return barWidth; // nothing left to earn, so the bar is simply full
	const uint64_t span = oracool::GetLevelExperienceSpan(player);
	if (span == 0)
		return 0;
	const uint64_t into = player._pExperience >= ExpLvlsTbl[player._pLevel - 1]
	    ? player._pExperience - ExpLvlsTbl[player._pLevel - 1]
	    : 0;
	return static_cast<int>(std::min<uint64_t>(into * barWidth / span, static_cast<uint64_t>(barWidth)));
}

/** @brief One row of the bar, inset at both ends by however much the rounding asks for. */
void DrawBarRow(const Surface &out, Rectangle bar, int row, int filled)
{
	// Off the RECT's height, not the BarHeight constant - the two differ whenever the gap forced a
	// shorter bar, and reading the constant here would round the corners of a bar that is not that
	// tall (audit UI-01A).
	const int distanceFromEdge = std::min(row, bar.size.height - 1 - row);
	const int inset = distanceFromEdge < static_cast<int>(std::size(CornerInset))
	    ? CornerInset[distanceFromEdge]
	    : 0;
	const int x = bar.position.x + inset;
	const int width = bar.size.width - 2 * inset;
	if (width <= 0)
		return;

	const int y = bar.position.y + row;
	// The filled run is measured from the bar's own left edge, not this row's, so the fill's right
	// end stays vertical across all eight rows instead of stepping in with the rounding.
	const int filledEnd = bar.position.x + filled;
	const int filledHere = std::clamp(filledEnd - x, 0, width);
	if (filledHere > 0)
		DrawHorizontalLine(out, { x, y }, filledHere, FilledColor);
	if (filledHere < width)
		DrawHorizontalLine(out, { x + filledHere, y }, width - filledHere, EmptyColor);
}

} // namespace

void InitXPBar()
{
}

void FreeXPBar()
{
}

void DrawXPBar(const Surface &out)
{
	// talkflag restored 2026-08-30 (external audit UI-01B). The vanilla bar began with exactly this
	// guard and the revival dropped it, so opening chat painted the gold strip over the chat panel -
	// DrawTalkPan runs first and DrawXPBar second.
	if (!*sgOptions.Gameplay.experienceBar || talkflag || MyPlayer == nullptr)
		return;

	const Rectangle bar = GetXPBarRect();
	if (bar.size.width <= 0)
		return;

	const int filled = FilledWidth(*MyPlayer, bar.size.width);
	for (int row = 0; row < bar.size.height; row++)
		DrawBarRow(out, bar, row, filled);

	// The ten-percent marks, over the fill and the groove alike, at 10% to 90%. The 2026-09-05 notches were
	// bites out of the bar and cut a 4px bar in two; these are lines on it.
	for (int tenth = 1; tenth < 10; tenth++) {
		const int x = bar.position.x + bar.size.width * tenth / 10;
		DrawVerticalLine(out, { x, bar.position.y }, bar.size.height, TickColor);
	}
	// The frame, a pixel outside the bar so none of its four rows is lost, with the corners cut.
	const int left = bar.position.x - 1;
	const int right = bar.position.x + bar.size.width;
	const int top = bar.position.y - 1;
	const int bottom = bar.position.y + bar.size.height;
	DrawHorizontalLine(out, { left + 1, top }, right - left - 1, FrameColor);
	DrawHorizontalLine(out, { left + 1, bottom }, right - left - 1, FrameColor);
	DrawVerticalLine(out, { left, top + 1 }, bottom - top - 1, FrameColor);
	DrawVerticalLine(out, { right, top + 1 }, bottom - top - 1, FrameColor);
}

bool CheckXPBarInfo()
{
	return false;
}

} // namespace devilution
