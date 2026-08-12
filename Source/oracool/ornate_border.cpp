#include "oracool/ornate_border.h"

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
