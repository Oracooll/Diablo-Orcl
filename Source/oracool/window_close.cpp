#include "oracool/window_close.h"

#include "engine/render/primitive_render.hpp"

#include "oracool/ornate_border.h"

namespace devilution::oracool {

namespace {

/** Bright red from the 16-entry ramp. +4 rather than the ramp's brightest, which is close enough to
 * white at this size that the X stops reading as red at all. */
constexpr uint8_t CloseGlyphColor = PAL16_RED + 4;
/** The plate the glyph sits on, so the X is legible over a window's own contents as well as over
 * the dark panel ground. */
constexpr uint8_t ClosePlateColor = PAL16_RED + 13;
/** How thick each stroke of the X is. Two pixels: one reads as a hairline and disappears against a
 * busy background, three closes the gap in the middle of the glyph into a blob. */
constexpr int StrokeWidth = 2;
/** Gap between the glyph and the edge of its plate. */
constexpr int GlyphInset = 4;

} // namespace

Rectangle GetWindowCloseButtonRect(const Rectangle &window)
{
	// Inset by the border width so the button sits inside the frame rather than on top of it, and
	// so the same call gives the right answer for every window regardless of what it draws inside.
	return Rectangle {
		Point {
		    window.position.x + window.size.width - OrnateBorderWidth - WindowCloseButtonSize,
		    window.position.y + OrnateBorderWidth },
		Size { WindowCloseButtonSize, WindowCloseButtonSize }
	};
}

void DrawWindowCloseButton(const Surface &out, const Rectangle &window)
{
	const Rectangle button = GetWindowCloseButtonRect(window);

	// Plate first: a dark red square with a lighter outline, so the control reads as a button and
	// not as a decoration painted into whatever art is behind it.
	FillRect(out, button.position.x, button.position.y, button.size.width, button.size.height, ClosePlateColor);
	UnsafeDrawBorder2px(out, button, CloseGlyphColor);

	// The X. There is no diagonal-line primitive in the engine, so each stroke is drawn as a stack
	// of short horizontal runs - which is also what gives it its thickness for free.
	const int span = button.size.width - GlyphInset * 2;
	for (int step = 0; step < span; step++) {
		const int y = button.position.y + GlyphInset + step;
		DrawHorizontalLine(out, Point { button.position.x + GlyphInset + step, y }, StrokeWidth, CloseGlyphColor);
		DrawHorizontalLine(out, Point { button.position.x + GlyphInset + span - 1 - step, y }, StrokeWidth, CloseGlyphColor);
	}
}

bool CheckWindowCloseButtonClick(const Rectangle &window, Point mousePosition)
{
	return GetWindowCloseButtonRect(window).contains(mousePosition);
}

} // namespace devilution::oracool
