#include "oracool/cursor_tooltip.h"

#include <algorithm>
#include <cassert>

#include "DiabloUI/ui_flags.hpp"
#include "control.h"
#include "cursor.h"
#include "engine/point.hpp"
#include "engine/render/primitive_render.hpp"
#include "engine/render/text_render.hpp"
#include "inv.h"
#include "oracool/ornate_border.h" // ThemeEdgeColor
#include "qol/stash.h"
#include "utils/ui_fwd.h"

namespace devilution::oracool {

namespace {

// Oracool: user request (2026-08-11) - the tooltip lost its dark backing plate and now sits
// centred just above the cursor rather than trailing below-right of it. Without a backdrop the
// text is outlined instead, which is what keeps it legible over bright dungeon floors.
constexpr int GapAboveCursor = 6;

// Oracool: user request (2026-08-12) - items are the exception to the above. An item's block runs
// to a dozen lines of stats, which as bare outlined text over a dungeon floor is unreadable, so
// item hovers get a real panel: padded, darkened, bordered. This replaces BOTH the outlined
// tooltip and the fixed "item stats" box that used to sit beside the inventory - one panel, at the
// cursor, sized to its contents.
constexpr int PanelPaddingX = 10;
constexpr int PanelPaddingY = 7;
/** Extra air between the panel's rows, on top of the font's own line height. */
constexpr int PanelLineGap = 4;
constexpr int PanelBorderWidth = 2;
/** Deep gold, shared with the class silhouette's outline so the two edges cannot drift apart -
 * see oracool::ThemeEdgeColor for why that is one constant rather than two literals. */
constexpr uint8_t PanelBorderColor = ThemeEdgeColor;

Rectangle PrevTooltipRect;

/** @brief Widest line and line count of a possibly multi-line string. */
void MeasureText(string_view text, int &maxWidth, int &lineCount)
{
	maxWidth = 0;
	lineCount = 0;
	size_t start = 0;
	while (true) {
		const size_t newline = text.find('\n', start);
		const string_view line = (newline == string_view::npos)
		    ? text.substr(start)
		    : text.substr(start, newline - start);
		maxWidth = std::max(maxWidth, GetLineWidth(line, GameFont12, 1));
		lineCount++;
		if (newline == string_view::npos)
			break;
		start = newline + 1;
	}
}

/**
 * @brief Whether what is under the cursor right now is an item, and so wants the panel treatment.
 *
 * Derived from the hover globals rather than from a flag the hover code sets, deliberately: every
 * one of these is already cleared and repopulated once per frame by the cursor/hover pass, so
 * there is no way for this to go stale, and no new plumbing threaded through inv.cpp, stash.cpp
 * and control.cpp for something all three already record.
 *
 * `pcursinvitem` covers the inventory grid, the equipment slots and the belt; `ActiveTabItemHovered`
 * covers the extra inventory tabs, whose items have no pcursinvitem encoding (see CheckInvHLight).
 * A held item is not included - it is a single line, and a plate following a dragged item around
 * would be in the way.
 */
bool IsHoveringItem()
{
	return pcursitem != -1
	    || pcursinvitem != -1
	    || pcursstashitem != StashStruct::EmptyCell
	    || ActiveTabItemHovered;
}

} // namespace

Rectangle GetPrevCursorTooltipRect()
{
	return PrevTooltipRect;
}

void DrawCursorTooltip(const Surface &out)
{
	PrevTooltipRect = {};

	if (talkflag || InfoString.empty())
		return;

	// Sized to the text itself rather than a fixed box, so "centred on the cursor" actually
	// centres the glyphs and short labels don't sit adrift inside a wide invisible rectangle.
	int maxWidth = 0;
	int lineCount = 0;
	MeasureText(InfoString.str(), maxWidth, lineCount);
	const int lineHeight = GetLineHeight(InfoString.str(), GameFont12);

	const bool asPanel = IsHoveringItem();

	// Oracool: user request - an item's stat block reads cramped at the font's own line height, so
	// the panel opens the rows up. Only the panel: a one-line hover has no rows to space out.
	//
	// The gap goes BETWEEN rows, not below each of them - the height is n line boxes plus n-1 gaps.
	// Adding it to every row instead would leave a trailing gap under the last line, making the
	// panel's bottom padding visibly deeper than its top.
	const int lineGap = asPanel ? PanelLineGap : 0;
	const int lineStride = lineHeight + lineGap;
	const Size textSize { maxWidth, lineCount * lineHeight + (lineCount - 1) * lineGap };

	const int padX = asPanel ? PanelPaddingX + PanelBorderWidth : 0;
	const int padY = asPanel ? PanelPaddingY + PanelBorderWidth : 0;
	const Size boxSize { textSize.width + 2 * padX, textSize.height + 2 * padY };

	// The upper bounds are floored at 0 rather than used raw: std::clamp is undefined when hi < lo,
	// which is what a box wider or taller than the screen would produce. Unlikely with a 12pt font
	// on a 960-wide canvas, but the item panel made it reachable in a way the one-line tooltip
	// never was, and UnsafeDrawBorder2px below does no clipping of its own.
	const int maxX = std::max(0, static_cast<int>(gnScreenWidth) - boxSize.width);
	const int maxY = std::max(0, static_cast<int>(gnScreenHeight) - boxSize.height);

	Point origin { MousePosition.x - boxSize.width / 2, MousePosition.y - boxSize.height - GapAboveCursor };
	origin.x = std::clamp(origin.x, 0, maxX);
	// Near the top of the screen there is no room above the cursor, so fall below it instead of
	// letting the text sit on top of what is being hovered. A tall item panel hits this often.
	if (origin.y < 0)
		origin.y = std::min(MousePosition.y + GapAboveCursor, maxY);
	origin.y = std::clamp(origin.y, 0, maxY);

	const Rectangle box { origin, boxSize };
	const bool boxFitsOnScreen = boxSize.width <= static_cast<int>(gnScreenWidth) && boxSize.height <= static_cast<int>(gnScreenHeight);
	if (asPanel && boxFitsOnScreen) {
		// Twice, for ~75% darkening: one pass leaves the floor tiles reading straight through the
		// stat lines, which is the readability problem this panel exists to solve.
		DrawHalfTransparentRectTo(out, box.position.x, box.position.y, box.size.width, box.size.height);
		DrawHalfTransparentRectTo(out, box.position.x, box.position.y, box.size.width, box.size.height);
		UnsafeDrawBorder2px(out, box, PanelBorderColor);
	}

	const Rectangle textArea { origin + Displacement { padX, padY }, textSize };
	const UiFlags sharedFlags = UiFlags::AlignCenter | UiFlags::KerningFitSpacing
	    | (asPanel ? UiFlags::None : UiFlags::Outlined);
	// The same flags WITHOUT centring, for the lines drawn in two runs. Those are positioned by hand
	// below, and DrawString centring each run inside its own rectangle is precisely what broke them.
	const UiFlags runFlags = UiFlags::KerningFitSpacing
	    | (asPanel ? UiFlags::None : UiFlags::Outlined);

	// Oracool: an item's block is several KINDS of information - its name, what it is, what was
	// rolled onto it, what it demands of you - and each line carries its own colour (see
	// control.h's InfoStringLineColors). Drawing per line is the only way to honour that, since
	// DrawString takes one colour for the whole string.
	//
	// An empty colour list is normal: hovers that are a single line - a monster, an NPC, a shrine -
	// assign InfoString directly and keep the single-colour path exactly as before.
	//
	// A list that is non-empty but the WRONG size is not normal, it is the signature of a bug:
	// somebody appended coloured lines onto text that was assigned without registering its own
	// colour. That is precisely how the item panel shipped in one colour - the inventory, stash and
	// held-item hovers each set the name with a bare assignment, leaving the list one short of the
	// block PrintItemDetails then built, and this check quietly refused all of it. Silence is what
	// made it hard to see, so it asserts now.
	string_view text = InfoString.str();
	const bool perLineColors = InfoStringLineColors.size() == static_cast<size_t>(lineCount);
	assert((InfoStringLineColors.empty() || perLineColors)
	    && "InfoString gained lines whose colours were never recorded - use SetPanelString/AddPanelString");
	if (!perLineColors) {
		// lineStride, not lineHeight: DrawString's lineHeight option IS the row-to-row step, so
		// this is where the gap gets applied on the single-colour path.
		DrawString(out, text, textArea, { InfoColor | sharedFlags, 1, lineStride });
	} else {
		size_t start = 0;
		for (int i = 0; i < lineCount; i++) {
			const size_t newline = text.find('\n', start);
			const string_view line = (newline == string_view::npos)
			    ? text.substr(start)
			    : text.substr(start, newline - start);
			// Stepped by the stride, but each row's box is one line tall - the gap is the space
			// between boxes, not part of them.
			const Rectangle lineArea { textArea.position + Displacement { 0, i * lineStride },
				{ textArea.size.width, lineHeight } };
			// A line may be drawn in TWO runs: the head in its own colour and a white tail from
			// InfoStringLineTailStart. The set panel's item list needs it - each piece's name is
			// green or red while its slot "(helm)" is white (user request, 2026-08-16) - and one
			// colour per line cannot say that. Zero, the value every other producer records, takes
			// the single-run path below unchanged.
			const size_t tailStart = i < static_cast<int>(InfoStringLineTailStart.size())
			    ? InfoStringLineTailStart[i]
			    : 0;
			if (tailStart > 0 && tailStart < line.size()) {
				const string_view head = line.substr(0, tailStart);
				const string_view tail = line.substr(tailStart);
				// Bug (fixed 2026-08-17, user: "white letters overlap the green one, when there is
				// obviously enough space to avoid it"). Both runs were drawn with the shared
				// AlignCenter flag: the head centred inside the FULL line box, the tail centred
				// inside whatever was left of it to the right. Two independent centrings, so the
				// white slot label landed on top of the green item name every time - and the space
				// the user could see going spare was the gap those two centrings left at the edges.
				//
				// The pair is centred as ONE line now, then laid out left to right from that
				// origin. Centred by the whole LINE's width rather than by the sum of the two runs',
				// so this agrees with how MeasureText sized the panel and puts the text exactly
				// where a plain centred DrawString would have put it.
				const int left = lineArea.position.x
				    + std::max(0, (lineArea.size.width - GetLineWidth(line)) / 2);
				// Measured rather than assumed: the head is a translated item name, so its width is
				// only knowable from the font that will actually draw it.
				const int headWidth = GetLineWidth(head);
				const Rectangle headArea { { left, lineArea.position.y }, { headWidth, lineHeight } };
				const Rectangle tailArea { { left + headWidth, lineArea.position.y },
					{ GetLineWidth(tail), lineHeight } };
				DrawString(out, head, headArea, { InfoStringLineColors[i] | runFlags, 1, lineHeight });
				DrawString(out, tail, tailArea, { UiFlags::ColorWhite | runFlags, 1, lineHeight });
				if (newline == string_view::npos)
					break;
				start = newline + 1;
				continue;
			}
			DrawString(out, line, lineArea,
			    { InfoStringLineColors[i] | sharedFlags, 1, lineHeight });
			if (newline == string_view::npos)
				break;
			start = newline + 1;
		}
	}

	// The outline bleeds a pixel past the glyphs, so the region the dirty-rect path has to erase
	// is slightly larger than the text box itself. The panel's border is already inside `box`.
	const int bleed = asPanel ? 0 : 2;
	PrevTooltipRect = { { origin.x - bleed, origin.y - bleed },
		{ boxSize.width + bleed * 2, boxSize.height + bleed * 2 } };
}

} // namespace devilution::oracool
