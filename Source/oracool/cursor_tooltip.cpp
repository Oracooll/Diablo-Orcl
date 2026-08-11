#include "oracool/cursor_tooltip.h"

#include <algorithm>

#include "DiabloUI/ui_flags.hpp"
#include "control.h"
#include "cursor.h"
#include "engine/point.hpp"
#include "engine/render/primitive_render.hpp"
#include "engine/render/text_render.hpp"
#include "utils/ui_fwd.h"

namespace devilution::oracool {

namespace {

// Oracool: user request (2026-08-11) - the tooltip lost its dark backing plate and now sits
// centred just above the cursor rather than trailing below-right of it. Without a backdrop the
// text is outlined instead, which is what keeps it legible over bright dungeon floors.
constexpr int GapAboveCursor = 6;

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
	const Size textSize { maxWidth, lineCount * lineHeight };

	Point origin { MousePosition.x - textSize.width / 2, MousePosition.y - textSize.height - GapAboveCursor };
	origin.x = std::clamp(origin.x, 0, static_cast<int>(gnScreenWidth) - textSize.width);
	// Near the top of the screen there is no room above the cursor, so fall below it instead of
	// letting the text sit on top of what is being hovered.
	if (origin.y < 0)
		origin.y = std::min(MousePosition.y + GapAboveCursor, static_cast<int>(gnScreenHeight) - textSize.height);
	origin.y = std::clamp(origin.y, 0, static_cast<int>(gnScreenHeight) - textSize.height);

	const Rectangle textArea { origin, textSize };
	DrawString(out, InfoString, textArea,
	    { InfoColor | UiFlags::AlignCenter | UiFlags::KerningFitSpacing | UiFlags::Outlined, 1, lineHeight });

	// The outline bleeds a pixel past the glyphs, so the region the dirty-rect path has to erase
	// is slightly larger than the text box itself.
	constexpr int OutlineBleed = 2;
	PrevTooltipRect = { { origin.x - OutlineBleed, origin.y - OutlineBleed },
		{ textSize.width + OutlineBleed * 2, textSize.height + OutlineBleed * 2 } };
}

} // namespace devilution::oracool
