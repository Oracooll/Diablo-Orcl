#include "oracool/sprite_scale.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <memory>
#include <vector>

#include "engine/point.hpp"
#include "engine/surface.hpp"
#include "utils/clx_decode.hpp"
#include "utils/endian_write.hpp"
#include "utils/surface_to_clx.hpp"

namespace devilution::oracool {

namespace {

/**
 * @brief One decoded sprite: row-major TOP-down pixels plus an exact opacity mask.
 *
 * The mask is the whole reason this decoder exists - index 0 is an opaque colour (shadows), so
 * transparency has to come from the run structure, never from a colour key.
 */
struct DecodedSprite {
	int width = 0;
	int height = 0;
	std::vector<uint8_t> pixels;
	std::vector<uint8_t> opaque;
};

DecodedSprite DecodeSprite(ClxSprite sprite)
{
	DecodedSprite out;
	out.width = sprite.width();
	out.height = sprite.height();
	out.pixels.assign(static_cast<size_t>(out.width) * out.height, 0);
	out.opaque.assign(static_cast<size_t>(out.width) * out.height, 0);

	// CL2 pixel order is rows BOTTOM-up, left to right, runs wrapping across row ends. `cursor`
	// walks that order linearly; the lambda maps it into the top-down buffer.
	const uint8_t *src = sprite.pixelData();
	const uint8_t *end = src + sprite.pixelDataSize();
	size_t cursor = 0;
	const size_t total = out.pixels.size();
	const auto put = [&](uint8_t color) {
		if (cursor >= total)
			return; // malformed data must not write out of bounds
		const size_t x = cursor % out.width;
		const size_t bottomUpRow = cursor / out.width;
		const size_t index = (static_cast<size_t>(out.height) - 1 - bottomUpRow) * out.width + x;
		out.pixels[index] = color;
		out.opaque[index] = 1;
		cursor++;
	};

	// The control alphabet comes from utils/clx_decode.hpp - the same helpers the renderer uses,
	// so this decode cannot drift from what the blitter actually draws: < 0x80 is a transparent
	// run; 0x80-0xBE is a FILL (one colour byte, repeated 0xBF - control times); >= 0xBF is a
	// literal run of -(int8)control colour bytes.
	while (src < end) {
		const uint8_t control = *src++;
		if (!IsClxOpaque(control)) {
			cursor += control;
			continue;
		}
		if (IsClxOpaqueFill(control)) {
			if (src >= end)
				break;
			const uint8_t color = *src++;
			for (unsigned i = GetClxOpaqueFillWidth(control); i > 0; i--)
				put(color);
		} else {
			for (unsigned i = GetClxOpaquePixelsWidth(control); i > 0 && src < end; i--)
				put(*src++);
		}
	}
	return out;
}

/** @brief A palette index the decoded frames never use OPAQUELY, to serve as the encode key. */
uint8_t PickUnusedIndex(const std::vector<DecodedSprite> &frames)
{
	bool used[256] = {};
	for (const DecodedSprite &frame : frames) {
		for (size_t i = 0; i < frame.pixels.size(); i++) {
			if (frame.opaque[i] != 0)
				used[frame.pixels[i]] = true;
		}
	}
	// Walk from the top: high indices are the likeliest to be free in monster art, and 0 (the
	// shadow colour) is the one we most want to avoid. If all 256 are somehow in use, 0 is the
	// least-bad fallback - and matches what the PNG import pipeline already assumes.
	for (int i = 255; i >= 0; i--) {
		if (!used[i])
			return static_cast<uint8_t>(i);
	}
	return 0;
}

} // namespace

OwnedClxSpriteList ScaleClxList(ClxSpriteList src, unsigned percent)
{
	percent = std::clamp(percent, 25U, 400U);
	const uint32_t numFrames = src.numSprites();

	std::vector<DecodedSprite> decoded;
	decoded.reserve(numFrames);
	int scaledWidth = 1;
	int scaledHeight = 1;
	for (uint32_t i = 0; i < numFrames; i++) {
		decoded.push_back(DecodeSprite(src[i]));
		// Frames of one list share dimensions in every sheet the game ships; take the max so a
		// hypothetical ragged list still gets a canvas everything fits on.
		scaledWidth = std::max<int>(scaledWidth, std::max(1, decoded.back().width * static_cast<int>(percent) / 100));
		scaledHeight = std::max<int>(scaledHeight, std::max(1, decoded.back().height * static_cast<int>(percent) / 100));
	}

	const uint8_t transparent = PickUnusedIndex(decoded);

	// SurfaceToClx wants the frames stacked vertically on one surface.
	OwnedSurface stacked(scaledWidth, scaledHeight * static_cast<int>(numFrames));
	for (uint32_t frame = 0; frame < numFrames; frame++) {
		const DecodedSprite &source = decoded[frame];
		const int frameTop = static_cast<int>(frame) * scaledHeight;
		for (int y = 0; y < scaledHeight; y++) {
			uint8_t *dst = &stacked[Point { 0, frameTop + y }];
			const int srcY = std::min(source.height - 1, y * source.height / scaledHeight);
			for (int x = 0; x < scaledWidth; x++) {
				const int srcX = std::min(source.width - 1, x * source.width / scaledWidth);
				const size_t index = static_cast<size_t>(srcY) * source.width + srcX;
				dst[x] = source.opaque[index] != 0 ? source.pixels[index] : transparent;
			}
		}
	}

	return SurfaceToClx(stacked, numFrames, transparent);
}

OwnedClxSpriteList TurnedClxList(ClxSprite src, unsigned percent, unsigned steps)
{
	percent = std::clamp(percent, 10U, 400U);
	steps = std::max(steps, 1U);
	const std::vector<DecodedSprite> decoded { DecodeSprite(src) };
	const DecodedSprite &source = decoded[0];
	const double scaledWidth = std::max(1.0, source.width * static_cast<double>(percent) / 100.0);
	const double scaledHeight = std::max(1.0, source.height * static_cast<double>(percent) / 100.0);
	// Square and wide enough for the sprite at any angle: its diagonal, rounded up to even so the centre is a whole pixel.
	int side = static_cast<int>(std::ceil(std::hypot(scaledWidth, scaledHeight)));
	side += side % 2;
	const uint8_t transparent = PickUnusedIndex(decoded);

	OwnedSurface stacked(side, side * static_cast<int>(steps));
	for (unsigned k = 0; k < steps; k++) {
		const double angle = 2.0 * 3.14159265358979323846 * k / steps;
		const double c = std::cos(angle);
		const double s = std::sin(angle);
		const int frameTop = static_cast<int>(k) * side;
		for (int y = 0; y < side; y++) {
			uint8_t *dst = &stacked[Point { 0, frameTop + y }];
			const double dy = y + 0.5 - side / 2.0;
			for (int x = 0; x < side; x++) {
				const double dx = x + 0.5 - side / 2.0;
				// Back through the turn into the scaled sprite, then down to the source pixel.
				const double sx = c * dx + s * dy + scaledWidth / 2.0;
				const double sy = -s * dx + c * dy + scaledHeight / 2.0;
				dst[x] = transparent;
				if (sx < 0 || sy < 0 || sx >= scaledWidth || sy >= scaledHeight)
					continue;
				const int srcX = std::min(source.width - 1, static_cast<int>(sx * source.width / scaledWidth));
				const int srcY = std::min(source.height - 1, static_cast<int>(sy * source.height / scaledHeight));
				const size_t at = static_cast<size_t>(srcY) * source.width + srcX;
				if (source.opaque[at] != 0)
					dst[x] = source.pixels[at];
			}
		}
	}
	return SurfaceToClx(stacked, steps, transparent);
}

OwnedClxSpriteSheet ScaleClxSheet(ClxSpriteSheet src, unsigned percent)
{
	std::vector<OwnedClxSpriteList> lists;
	lists.reserve(src.numLists());
	for (uint16_t i = 0; i < src.numLists(); i++)
		lists.push_back(ScaleClxList(src[i], percent));

	// Same glue CombineListsIntoSheet uses in sprite_import.cpp - inlined here rather than shared
	// because that one is file-local and three lines.
	const size_t headerSize = 4 * lists.size();
	size_t total = headerSize;
	for (const OwnedClxSpriteList &list : lists)
		total += ClxSpriteList(list).dataSize();

	std::unique_ptr<uint8_t[]> data { new uint8_t[total] };
	size_t offset = headerSize;
	for (size_t i = 0; i < lists.size(); i++) {
		const ClxSpriteList list { lists[i] };
		WriteLE32(&data[i * 4], static_cast<uint32_t>(offset));
		std::memcpy(&data[offset], list.data(), list.dataSize());
		offset += list.dataSize();
	}
	return OwnedClxSpriteSheet { std::move(data), static_cast<uint16_t>(lists.size()) };
}

} // namespace devilution::oracool
