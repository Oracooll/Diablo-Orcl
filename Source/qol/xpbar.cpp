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

/** @brief The user's number: "make it 8px thick". */
constexpr int BarHeight = 8;
/**
 * @brief How far each end row is pulled in, giving the rounded corners.
 *
 * Two rows of inset on an eight-tall bar - 2px on the outermost row, 1px on the next - which is as
 * round as a shape this thin can read without losing its ends entirely. Indexed by distance from
 * the nearest long edge, so the same table rounds the top and the bottom.
 */
constexpr int CornerInset[] = { 2, 1 };

/** @brief A tenth of the bar gets a notch, so the bar reads as ten segments. */
constexpr int SegmentCount = 10;
/** @brief Each notch is this wide, and bitten this deep into the top and bottom edges. */
constexpr int NotchWidth = 3;
constexpr int NotchDepth = 2;

/** @brief Gold, from the palette's own yellow ramp - the colour the HUD's other gold text uses. */
constexpr uint8_t FilledColor = PAL16_YELLOW + 4;
/** @brief The unfilled groove: the ornate border's shadow tone, so the bar sits in the HUD's family. */
constexpr uint8_t EmptyColor = 204;
/** @brief The edge, a shade darker than the groove, so the bar has an outline at both states. */
constexpr uint8_t EdgeColor = 0;

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
	const Rectangle counter = oracool::GetXpCounterDrawRect();
	const Rectangle firstCell = oracool::GetBeltSlotRect(0);
	const Rectangle lastCell = oracool::GetBeltSlotRect(oracool::BeltVisibleSlotCount - 1);

	const int gapTop = counter.position.y + counter.size.height;
	const int gapBottom = firstCell.position.y;
	const int left = firstCell.position.x;
	const int width = lastCell.position.x + lastCell.size.width - left;

	// The height is what the gap ALLOWS, never more (external audit UI-01A, 2026-08-30). The
	// requested 8px is the maximum, not a guarantee: with plate art off - the default - the band
	// between the counter and the belt is only 6px, and an 8px rectangle centred in it hung two
	// pixels into the belt backing. The bar is drawn after the belt, so those two rows were painted
	// over rather than hidden.
	//
	// The old comment here claimed the clamp "pins the bar to the belt's top edge" in that case. It
	// could not: clamping the TOP of a fixed-height rect cannot shorten it, so the bottom simply
	// went past the belt. Clamping the height is what actually holds the invariant, and
	// OracoolAudit.TheXpBarFitsBetweenTheCounterAndTheBelt pins it in both HUD modes.
	const int gap = gapBottom - gapTop;
	if (gap <= 0)
		return Rectangle { { left, gapTop }, { width, 0 } };
	const int height = std::min(BarHeight, gap);
	const int top = gapTop + (gap - height) / 2;
	return Rectangle { { left, top }, { width, height } };
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

/**
 * @brief Bites a rounded notch out of the top and bottom edges at each tenth.
 *
 * Drawn AFTER the fill rather than as a gap in it, because a notch is a hole in the bar rather than
 * a hole in the progress - it has to read the same whether the segment behind it is full or empty.
 * The depth tapers by one pixel at each end of the notch, which is what makes it read as round
 * rather than as a rectangular bite.
 */
void DrawNotches(const Surface &out, Rectangle bar)
{
	for (int segment = 1; segment < SegmentCount; segment++) {
		const int centre = bar.position.x + bar.size.width * segment / SegmentCount;
		for (int i = 0; i < NotchWidth; i++) {
			const int x = centre - NotchWidth / 2 + i;
			if (x < bar.position.x || x >= bar.position.x + bar.size.width)
				continue;
			// Deepest in the middle of the notch, one shallower at each edge.
			const int depth = NotchDepth - std::abs(i - NotchWidth / 2);
			for (int d = 0; d < depth; d++) {
				out.SetPixel({ x, bar.position.y + d }, EdgeColor);
				out.SetPixel({ x, bar.position.y + bar.size.height - 1 - d }, EdgeColor);
			}
		}
	}
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
	DrawNotches(out, bar);
}

bool CheckXPBarInfo()
{
	return false;
}

} // namespace devilution
