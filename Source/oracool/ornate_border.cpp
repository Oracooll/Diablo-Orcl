#include "oracool/ornate_border.h"

#include <algorithm>
#include <string>

#include "engine/render/primitive_render.hpp"
#include "engine/render/text_render.hpp"

namespace devilution::oracool {

namespace {

// Sampled from 99-original-game-art/png/textbox/textbox_frame00.png at the mid-point of each edge,
// then matched back to palette indices. The frame is a bevel lit from the bottom-right: all four
// outer edges share a mid-gold, and the SECOND ring is what carries the lighting - dark on the top
// and left, bright on the bottom and right. The innermost ring is near-black all round, which is
// what separates the frame from whatever it surrounds.
//
// The original's edges vary by a shade or two along their length (201/202/203 on the outer ring,
// 197/198/200 on the lit middle one) - hand-painted texture rather than a rule. These are the
// modal values; reproducing the per-pixel noise would need the art itself, at which point it could
// not be resized.
constexpr uint8_t OuterColor = 202;      // (91, 81, 52)
constexpr uint8_t MidShadowColor = 204;  // (57, 49, 29) - top and left
constexpr uint8_t MidHighlightColor = 198; // (152, 139, 93) - bottom and right
constexpr uint8_t InnerColor = 223;      // (15, 5, 0)

void DrawRing(const Surface &out, Rectangle rect, uint8_t topLeftColor, uint8_t bottomRightColor)
{
	if (rect.size.width <= 0 || rect.size.height <= 0)
		return;

	const int x = rect.position.x;
	const int y = rect.position.y;
	const int width = rect.size.width;
	const int height = rect.size.height;

	DrawHorizontalLine(out, { x, y }, width, topLeftColor);
	DrawVerticalLine(out, { x, y }, height, topLeftColor);
	DrawHorizontalLine(out, { x, y + height - 1 }, width, bottomRightColor);
	DrawVerticalLine(out, { x + width - 1, y }, height, bottomRightColor);
}

Rectangle Inset(Rectangle rect, int by)
{
	return Rectangle { { rect.position.x + by, rect.position.y + by },
		{ rect.size.width - 2 * by, rect.size.height - 2 * by } };
}

} // namespace

void DrawOrnateBorderOutside(const Surface &out, Rectangle rect)
{
	// The bevel's three rings are drawn INSIDE whatever rect they are given, so framing content
	// without eating into it means handing them a rect grown by their own width.
	DrawOrnateBorder(out,
	    Rectangle { { rect.position.x - OrnateBorderWidth, rect.position.y - OrnateBorderWidth },
	        { rect.size.width + 2 * OrnateBorderWidth, rect.size.height + 2 * OrnateBorderWidth } });
}

void DrawOrnateBorder(const Surface &out, Rectangle rect)
{
	DrawRing(out, rect, OuterColor, OuterColor);
	DrawRing(out, Inset(rect, 1), MidShadowColor, MidHighlightColor);
	DrawRing(out, Inset(rect, 2), InnerColor, InnerColor);
}

void DrawOrnateSeparator(const Surface &out, Point from, int width)
{
	// The bevel's own vertical cross-section, top to bottom: the shadow it puts on a top edge, the
	// mid-gold body, then the highlight it puts on a bottom edge. Reusing those three indices is
	// what makes a rule drawn here read as part of the same frame rather than a line laid over it.
	DrawHorizontalLine(out, from, width, MidShadowColor);
	DrawHorizontalLine(out, { from.x, from.y + 1 }, width, OuterColor);
	DrawHorizontalLine(out, { from.x, from.y + 2 }, width, MidHighlightColor);
}

void DrawOrnateSeparatorVertical(const Surface &out, Point from, int height)
{
	// Left-to-right this time, matching how the bevel lights a left edge dark and a right edge
	// bright - so a vertical rule reads consistently with the frame's own sides.
	DrawVerticalLine(out, from, height, MidShadowColor);
	DrawVerticalLine(out, { from.x + 1, from.y }, height, OuterColor);
	DrawVerticalLine(out, { from.x + 2, from.y }, height, MidHighlightColor);
}

void DrawThemedFill(const Surface &out, Rectangle rect, int passes)
{
	if (rect.size.width <= 0 || rect.size.height <= 0)
		return;
	for (int i = 0; i < std::max(1, passes); i++)
		DrawHalfTransparentRectTo(out, rect.position.x, rect.position.y, rect.size.width, rect.size.height);
}

void DrawColoredOutline(const Surface &out, Rectangle rect, uint8_t color)
{
	if (rect.size.width <= 1 || rect.size.height <= 1)
		return;
	const int x = rect.position.x;
	const int y = rect.position.y;
	const int w = rect.size.width;
	const int h = rect.size.height;
	DrawHorizontalLine(out, { x, y }, w, color);
	DrawHorizontalLine(out, { x, y + h - 1 }, w, color);
	DrawVerticalLine(out, { x, y }, h, color);
	DrawVerticalLine(out, { x + w - 1, y }, h, color);
}

void DrawSplitOutline(const Surface &out, Rectangle rect, uint8_t leftTopColor, uint8_t rightBottomColor, int weight)
{
	if (weight <= 0 || rect.size.width <= 2 * weight || rect.size.height <= 2 * weight)
		return;
	const int x = rect.position.x;
	const int y = rect.position.y;
	const int w = rect.size.width;
	const int h = rect.size.height;

	// The VERTICALS run the full height and the horizontals stop short of them, so each corner
	// belongs to the side it is on: both left corners to the left edge's colour, both right corners
	// to the right edge's. Drawing all four full length instead would hand both mixed corners to
	// whichever colour was painted second, which reads as one colour bleeding into the other rather
	// than as a square split down the middle.
	const int innerX = x + weight;
	const int innerWidth = w - 2 * weight;
	for (int i = 0; i < weight; i++) {
		DrawHorizontalLine(out, { innerX, y + i }, innerWidth, leftTopColor);
		DrawHorizontalLine(out, { innerX, y + h - 1 - i }, innerWidth, rightBottomColor);
		DrawVerticalLine(out, { x + i, y }, h, leftTopColor);
		DrawVerticalLine(out, { x + w - 1 - i, y }, h, rightBottomColor);
	}
}

void DrawHoverOutline(const Surface &out, Rectangle rect)
{
	// MidHighlightColor, the frame's LIT gold, rather than OuterColor's dimmer one. Both are "gold"
	// and the dim one is the more literal reading of "subtle", but these rows sit on a
	// half-transparent panel over the dungeon, where 202 barely separates from the fill. 198 reads as
	// a highlight at a glance and still belongs to the same frame the window is built from.
	DrawColoredOutline(out, rect, MidHighlightColor);
}

void DrawHoverPanel(const Surface &out, string_view title, string_view text, Rectangle anchor)
{
	if (title.empty() && text.empty())
		return;

	// Wide enough for a sentence without becoming a paragraph, and narrow enough to sit beside the
	// 340px window rather than across the view.
	constexpr int MaxTextWidth = 240;
	constexpr int Padding = 10;
	constexpr int Gap = 8; // between the row being described and the panel

	const std::string wrapped = WordWrapString(text, MaxTextWidth, GameFont12, 1);
	const int lineHeight = 18;
	int lines = 1;
	for (const char c : wrapped) {
		if (c == '\n')
			lines++;
	}

	// Measured from the WRAPPED text, so a short description gets a short panel instead of always
	// reserving the full MaxTextWidth.
	int textWidth = 0;
	size_t start = 0;
	while (start <= wrapped.size()) {
		const size_t end = wrapped.find('\n', start);
		const string_view line = string_view(wrapped).substr(start, end == std::string::npos ? std::string::npos : end - start);
		textWidth = std::max(textWidth, GetLineWidth(line, GameFont12, 1));
		if (end == std::string::npos)
			break;
		start = end + 1;
	}

	// Oracool: user request (2026-08-15) - "Put Skill name with Gold letters in the pop-up window."
	// The name moved here because the rows themselves lost their text, so this is now the ONLY place
	// a skill says what it is called. Gold rather than the body's white to keep the two apart at a
	// glance, and measured into the panel's width so a long name widens the box rather than wrapping
	// under the description.
	const int titleWidth = title.empty() ? 0 : GetLineWidth(title, GameFont12, 1);
	const int titleHeight = title.empty() ? 0 : lineHeight;

	const int panelWidth = std::max(textWidth, titleWidth) + 2 * Padding;
	const int panelHeight = titleHeight + lines * lineHeight + 2 * Padding;

	// To the right of the row by default; flipped to its left when there is no room, so the panel
	// never leaves the screen and never covers the thing it is describing.
	int px = anchor.position.x + anchor.size.width + Gap;
	if (px + panelWidth > out.w())
		px = anchor.position.x - Gap - panelWidth;
	px = std::clamp(px, 0, std::max(0, out.w() - panelWidth));
	// Vertically centred on the row, then pulled back inside the screen.
	int py = anchor.position.y + (anchor.size.height - panelHeight) / 2;
	py = std::clamp(py, 0, std::max(0, out.h() - panelHeight));

	const Rectangle panel { { px, py }, { panelWidth, panelHeight } };
	// Two passes: one is too sheer to read text over when the dungeon behind it is bright.
	DrawThemedFill(out, panel, 2);
	DrawOrnateBorder(out, panel);
	const int innerWidth = panelWidth - 2 * Padding;
	if (!title.empty()) {
		DrawString(out, title, { { px + Padding, py + Padding }, { innerWidth, lineHeight } },
		    { UiFlags::ColorWhitegold | UiFlags::VerticalCenter });
	}
	if (!wrapped.empty()) {
		DrawString(out, wrapped,
		    { { px + Padding, py + Padding + titleHeight }, { innerWidth, lines * lineHeight } },
		    { UiFlags::ColorWhite, 1, lineHeight });
	}
}

void DrawOutlinedString(const Surface &out, string_view text, Rectangle area, UiFlags style)
{
	constexpr Displacement Offsets[] = { { -1, 0 }, { 1, 0 }, { 0, -1 }, { 0, 1 } };
	// Strip the caller's colour from the outline pass, keeping its size and alignment, so the
	// black sits exactly under the glyphs rather than at a different size or offset.
	constexpr UiFlags ColorMask = UiFlags::ColorUiGold | UiFlags::ColorUiSilver | UiFlags::ColorUiGoldDark
	    | UiFlags::ColorUiSilverDark | UiFlags::ColorDialogWhite | UiFlags::ColorDialogYellow
	    | UiFlags::ColorDialogRed | UiFlags::ColorYellow | UiFlags::ColorGold | UiFlags::ColorBlack
	    | UiFlags::ColorWhite | UiFlags::ColorWhitegold | UiFlags::ColorRed | UiFlags::ColorBlue
	    | UiFlags::ColorOrange | UiFlags::ColorButtonface | UiFlags::ColorButtonpushed;
	const UiFlags layout = style & ~ColorMask;
	for (const Displacement &d : Offsets) {
		Rectangle shifted = area;
		shifted.position += d;
		DrawString(out, text, shifted, { layout | UiFlags::ColorBlack });
	}
	DrawString(out, text, area, { style });
}

} // namespace devilution::oracool
