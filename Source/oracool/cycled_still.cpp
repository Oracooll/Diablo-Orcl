#include "oracool/cycled_still.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <map>
#include <string>
#include <vector>

#include <SDL.h>

#include "engine/render/primitive_render.hpp" // PackArgb, CompositeArgbOver
#include "utils/log.hpp"
#include "utils/png.h"

namespace devilution::oracool {

namespace {

constexpr double Pi = 3.14159265358979;
// The aura rings' cycle (aura_ground.cpp), so a mantra and a ring move alike.
constexpr int CycleMs = 2400;
constexpr int CycleCrests = 2;
constexpr double CycleSwing = 0.35;

struct Still {
	int w = 0;
	int h = 0;
	std::vector<uint32_t> rgb;   // 0x00RRGGBB
	std::vector<uint8_t> alpha;  // 0 = no pixel
	std::vector<uint8_t> around; // the pixel's turn round the picture's centre, in 256ths
	std::vector<uint8_t> height; // its height up the picture, 0 at the lowest opaque row, 255 at the highest
	int centreY = 0;             // the row through the mean of the opaque pixels: where a ring's far side ends
	bool usable = false;
};

std::map<std::string, Still> Stills;

const Still &StillFor(const char *path)
{
	auto [it, fresh] = Stills.try_emplace(path);
	Still &still = it->second;
	if (!fresh)
		return still;
	SDL_Surface *png = LoadPNG(path); // through the archives, so the packed file is found
	if (png == nullptr) {
		LogVerbose("Oracool cycled still: {:s} not found", path);
		return still;
	}
	SDL_Surface *rgba = SDL_ConvertSurfaceFormat(png, SDL_PIXELFORMAT_ABGR8888, 0);
	SDL_FreeSurface(png);
	if (rgba == nullptr)
		return still;
	still.w = rgba->w;
	still.h = rgba->h;
	const size_t n = static_cast<size_t>(still.w) * still.h;
	still.rgb.assign(n, 0);
	still.alpha.assign(n, 0);
	still.around.assign(n, 0);
	still.height.assign(n, 0);
	const auto *pixels = static_cast<const uint8_t *>(rgba->pixels);
	double sumX = 0, sumY = 0;
	int count = 0, top = still.h, bottom = 0;
	for (int y = 0; y < still.h; y++) {
		for (int x = 0; x < still.w; x++) {
			const uint8_t *p = pixels + static_cast<size_t>(y) * rgba->pitch + static_cast<size_t>(x) * 4;
			if (p[3] == 0)
				continue;
			const size_t at = static_cast<size_t>(y) * still.w + x;
			still.rgb[at] = PackArgb(0, p[0], p[1], p[2]);
			still.alpha[at] = p[3];
			sumX += x;
			sumY += y;
			count++;
			top = std::min(top, y);
			bottom = std::max(bottom, y);
		}
	}
	SDL_FreeSurface(rgba);
	if (count == 0)
		return still;
	// Round the mean of the opaque pixels, not the frame's centre: motes above a ring would pull a box's centre off it.
	const double cx = sumX / count;
	const double cy = sumY / count;
	still.centreY = static_cast<int>(cy + 0.5);
	for (int y = 0; y < still.h; y++) {
		for (int x = 0; x < still.w; x++) {
			const size_t at = static_cast<size_t>(y) * still.w + x;
			if (still.alpha[at] == 0)
				continue;
			// On the ellipse's own circle: a ring on the floor is twice as wide as it is tall, so y counts double.
			const double turn = (std::atan2(2.0 * (y - cy + 0.5), x - cx + 0.5) + Pi) / (2.0 * Pi);
			still.around[at] = static_cast<uint8_t>(std::min(255, static_cast<int>(turn * 256.0)));
			still.height[at] = static_cast<uint8_t>(std::clamp((bottom - y) * 255 / std::max(1, bottom - top), 0, 255));
		}
	}
	still.usable = true;
	return still;
}

} // namespace

bool DrawCycledStill(const Surface &out, const char *path, Rectangle dst, CycleShape shape, int opacityPercent, StillPart part)
{
	if (out.isIndexed() || dst.size.width <= 0 || dst.size.height <= 0)
		return false;
	const Still &still = StillFor(path);
	if (!still.usable)
		return false;

	// Two bands, one lap every CycleMs; the ring's run round the picture, the rod's run up it (and back down: the lap
	// over the height covers it twice, so the light rises and falls).
	const double phase = static_cast<double>(SDL_GetTicks() % CycleMs) / CycleMs;
	std::array<int, 256> factor;
	for (int a = 0; a < 256; a++)
		factor[a] = static_cast<int>(256.0 * (1.0 + CycleSwing * std::cos(2.0 * Pi * CycleCrests * (a / 256.0 - phase))));

	const int dyFrom = std::max(0, -dst.position.y);
	const int dyTo = std::min(dst.size.height, out.h() - dst.position.y);
	const int dxFrom = std::max(0, -dst.position.x);
	const int dxTo = std::min(dst.size.width, out.w() - dst.position.x);
	for (int dy = dyFrom; dy < dyTo; dy++) {
		const int sy = dy * still.h / dst.size.height;
		if ((part == StillPart::Back && sy >= still.centreY) || (part == StillPart::Front && sy < still.centreY))
			continue;
		uint32_t *row = out.at<uint32_t>(0, dst.position.y + dy);
		for (int dx = dxFrom; dx < dxTo; dx++) {
			const int sx = dx * still.w / dst.size.width;
			const size_t at = static_cast<size_t>(sy) * still.w + sx;
			const uint8_t a = still.alpha[at];
			if (a == 0)
				continue;
			const int f = factor[shape == CycleShape::Ring ? still.around[at] : still.height[at] / 2];
			const uint32_t own = still.rgb[at];
			const auto channel = [&](int shift) { return static_cast<uint32_t>(std::min(255, static_cast<int>((own >> shift) & 0xFF) * f / 256)) << shift; };
			const uint32_t lit = channel(16) | channel(8) | channel(0);
			uint32_t &pixel = row[dst.position.x + dx];
			pixel = CompositeArgbOver(lit | (static_cast<uint32_t>(a) << 24), pixel, opacityPercent);
		}
	}
	return true;
}

} // namespace devilution::oracool
