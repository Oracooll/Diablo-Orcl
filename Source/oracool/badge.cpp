#include "oracool/badge.h"

#include <algorithm>

#include "DiabloUI/ui_flags.hpp"
#include "engine/render/primitive_render.hpp"

namespace devilution::oracool {

Size BadgeSize(string_view text)
{
	if (text.empty())
		return { 0, 0 };
	return { GetLineWidth(text, GameFont12) + BadgePadX * 2, GetLineHeight(text, GameFont12) + BadgePadY * 2 };
}

Rectangle DrawBadge(const Surface &out, Rectangle host, BadgeCorner corner, string_view text)
{
	if (text.empty())
		return Rectangle { host.position, { 0, 0 } };

	const Size size = BadgeSize(text);
	// Clamped rather than allowed to overhang: the badge is a label ON the icon, and one hanging off
	// the edge reads as belonging to whatever is next to it.
	const int width = std::min(size.width, host.size.width);
	const int height = std::min(size.height, host.size.height);
	const int right = host.position.x + host.size.width - width;
	const int bottom = host.position.y + host.size.height - height;

	Point at { host.position.x, host.position.y };
	switch (corner) {
	case BadgeCorner::TopLeft:
		break;
	case BadgeCorner::TopRight:
		at.x = right;
		break;
	case BadgeCorner::BottomLeft:
		at.y = bottom;
		break;
	case BadgeCorner::BottomRight:
		at = { right, bottom };
		break;
	case BadgeCorner::BottomCentre:
		at = { host.position.x + (host.size.width - width) / 2, bottom };
		break;
	}

	const Rectangle plate { at, { width, height } };
	// The engine's own half-transparency, which darkens what is behind it rather than painting a
	// colour over it. That is what keeps the badge readable on a bright sprite and on a dark one
	// without needing to know which it is sitting on - a fixed dark fill would swallow the icon
	// under it on the dark half of the palette and still be a hole in the art on the light half.
	DrawHalfTransparentRectTo(out, plate.position.x, plate.position.y, plate.size.width, plate.size.height);
	// White, always. See the header: the left/right distinction the Abilities window used to carry
	// in the badge's colour now lives in which corner it sits in, and in the assignment rings.
	DrawString(out, text, plate,
	    { UiFlags::ColorWhite | UiFlags::FontSize12 | UiFlags::AlignCenter | UiFlags::VerticalCenter });
	return plate;
}

} // namespace devilution::oracool
