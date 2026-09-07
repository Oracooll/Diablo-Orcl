#include "oracool/socket_overlay.h"

#include "cursor.h"
#include "engine/clx_sprite.hpp"
#include "engine/palette.h"
#include "engine/render/clx_render.hpp"
#include "engine/render/primitive_render.hpp"
#include "items.h"
#include "oracool/inventory_layout.h"

namespace devilution::oracool {

namespace {

/** @brief The user's numbers: a 24px ring centred in each 28x28 grid cell. */
constexpr int RingDiameter = 24;
constexpr int RingRadius = RingDiameter / 2;
/** @brief Ring thickness. Two pixels survives the ring being drawn over a busy sprite; one does not. */
constexpr int RingThickness = 2;

/**
 * @brief The empty socket's gold.
 *
 * PAL16 ramps run LIGHT to DARK as the offset grows (engine/palette.h), so a low offset is a bright
 * gold - which is the point: this sits ON TOP of an item sprite and has to win against it.
 */
constexpr uint8_t EmptySocketColor = PAL16_YELLOW + 2;

/**
 * @brief Draws a filled ring of @p thickness centred in @p box.
 *
 * Squared-distance comparison per pixel rather than a midpoint walk: the shape is 24px across and
 * drawn at most six times per frame, and this way the thickness is exact at every angle instead of
 * thinning at the diagonals the way a stroked midpoint circle does.
 */
void DrawRing(const Surface &out, Rectangle box, uint8_t color)
{
	const int cx = box.position.x + box.size.width / 2;
	const int cy = box.position.y + box.size.height / 2;
	const int outer = RingRadius * RingRadius;
	const int inner = (RingRadius - RingThickness) * (RingRadius - RingThickness);

	for (int y = -RingRadius; y <= RingRadius; y++) {
		for (int x = -RingRadius; x <= RingRadius; x++) {
			const int d = x * x + y * y;
			if (d > outer || d < inner)
				continue;
			const Point p { cx + x, cy + y };
			// Clipped per pixel. The overlay is drawn over panels that are themselves clipped
			// surfaces, and a socket cell can sit right on the edge of one.
			if (p.x < 0 || p.y < 0 || p.x >= out.w() || p.y >= out.h())
				continue;
			out.SetPixelUnchecked(p, color); // an index, resolved by the surface (v1.11)
		}
	}
}

/** @brief The stone in a socket, centred in @p box at its natural size. */
void DrawStone(const Surface &out, uint16_t stoneIdx, Rectangle box)
{
	const int cursId = AllItemsList[stoneIdx].iCurs + CURSOR_FIRSTITEM;
	const ClxSprite sprite = GetInvItemSprite(cursId);
	// Item sprites anchor at their BOTTOM-left, so the y is the cell's bottom, not its top. Getting
	// this wrong is the runeword book's own 1.8.60 bug - every icon floated a cell too high.
	const Point position {
		box.position.x + (box.size.width - static_cast<int>(sprite.width())) / 2,
		box.position.y + (box.size.height + static_cast<int>(sprite.height())) / 2
	};
	ClxDraw(out, position, sprite);
}

} // namespace

bool HasSocketOverlay(const Item &item)
{
	return !item.isEmpty() && item._iSocketCount > 0;
}

void DrawSocketOverlay(const Surface &out, const Item &item, Point spriteBottomLeft, Size cells)
{
	if (!HasSocketOverlay(item))
		return;

	// Back up from the sprite's own anchor to the footprint's top-left, so the cells land exactly on
	// the grid squares the item occupies.
	const Point origin { spriteBottomLeft.x, spriteBottomLeft.y - cells.height * CellPx };

	const int columns = std::max(1, cells.width);
	for (int i = 0; i < item._iSocketCount; i++) {
		const int column = i % columns;
		const int row = i / columns;
		// A socket can only run past the footprint if the socket cap has been broken elsewhere;
		// stop rather than paint outside the item.
		if (row >= cells.height)
			break;
		const Rectangle cell { { origin.x + column * CellPx, origin.y + row * CellPx }, { CellPx, CellPx } };

		const uint16_t stone = item._iSocketed[i];
		if (stone == Item::EmptySocket)
			DrawRing(out, cell, EmptySocketColor);
		else
			DrawStone(out, stone, cell);
	}
}

} // namespace devilution::oracool
