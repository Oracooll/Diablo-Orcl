#include "oracool/book_frame.h"

#include "engine/render/primitive_render.hpp"
#include "oracool/hud_art.h" // DrawLoosePng, GetLoosePngSize
#include "oracool/ornate_border.h"

namespace devilution::oracool {

namespace {

struct FrameSkin {
	const char *asset;
	Size size;
	/** The clear core, window-relative - measured off the painting (2026-09-05). */
	Rectangle core;
};

const FrameSkin &SkinOf(BookFrame frame)
{
	static const FrameSkin Wide { "ui\\book_frame_wide.png", { 944, 616 }, { { 21, 24 }, { 902, 568 } } };
	static const FrameSkin Tall { "ui\\book_frame_tall.png", { 420, 620 }, { { 21, 25 }, { 378, 570 } } };
	return frame == BookFrame::Wide ? Wide : Tall;
}

} // namespace

Size BookFrameSize(BookFrame frame)
{
	return SkinOf(frame).size;
}

Rectangle BookFrameCore(BookFrame frame, const Rectangle &window)
{
	const Rectangle &core = SkinOf(frame).core;
	return { window.position + Displacement { core.position.x, core.position.y }, core.size };
}

void DrawBookFrame(const Surface &out, BookFrame frame, const Rectangle &window)
{
	const FrameSkin &skin = SkinOf(frame);
	// The core first: twice, for the ~75% darkening the item tooltip's plate uses - one pass leaves
	// the dungeon reading straight through the text (user, 2026-09-05: "dark, transparent backing").
	const Rectangle core = BookFrameCore(frame, window);
	DrawHalfTransparentRectTo(out, core.position.x, core.position.y, core.size.width, core.size.height);
	DrawHalfTransparentRectTo(out, core.position.x, core.position.y, core.size.width, core.size.height);
	// Then the bezel. If the painting is missing the theme's own border stands in, so the window
	// still exists rather than vanishing into a dark rectangle.
	if (GetLoosePngSize(skin.asset).width == 0) {
		DrawOrnateBorder(out, window);
		return;
	}
	DrawLoosePng(out, skin.asset, window.position);
}

} // namespace devilution::oracool
