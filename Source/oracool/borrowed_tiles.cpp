/**
 * @file oracool/borrowed_tiles.cpp
 *
 * Tiles from every vanilla tileset on one level - see borrowed_tiles.h.
 */
#include "oracool/borrowed_tiles.h"

#include <algorithm>
#include <array>
#include <memory>
#include <unordered_map>
#include <vector>

#include <SDL_endian.h>

#include "engine/load_file.hpp"
#include "engine/palette.h"
#include "engine/render/dun_render.hpp"
#include "engine/surface.hpp"
#include "init.h" // gbIsHellfire
#include "lighting.h"
#include "oracool/colour_light.h" // SmoothLevelBrightness - the woods' and the borrowed tiles' one light
#include "player.h"
#include "utils/log.hpp"

namespace devilution::oracool {

namespace {

/** @brief One vanilla tileset's files, loaded on its first borrowed piece and kept for the session. */
struct Tileset {
	bool tried = false;
	bool ok = false;
	std::unique_ptr<byte[]> cels;
	std::vector<MICROS> micros;
	int microLen = 10;
	std::vector<TileProperties> sol;
	std::vector<MegaTile> megatiles;
	std::array<uint32_t, 256> palette {};
};

/** @brief A true-colour picture of part of a piece: 64 wide, the opaque rows only, each with its opaque span. */
struct Picture {
	int top = 0; // rows above the piece's bottom line (the tile's bottom row is 0)
	int height = 0;
	std::vector<uint32_t> px; // 0xFF000000 | rgb where opaque, 0 where not; 64 per row
	std::vector<int8_t> first;
	std::vector<int8_t> last;
};

struct Borrowed {
	dungeon_type set;
	uint16_t native;
	TileProperties sol;
	Picture floor;
	Picture cellWithTriangles;
	Picture cellWithoutTriangles;
};

constexpr int SetCount = static_cast<int>(DTYPE_LAST) + 1;
std::array<Tileset, SetCount> Sets;
std::vector<Borrowed> Pieces;
std::unordered_map<uint32_t, uint16_t> PieceIds;

/** @brief The set whose files a level of type @p type loads: the woods are Tristram's. */
dungeon_type FilesOf(dungeon_type type)
{
	return type == DTYPE_WOODS ? DTYPE_TOWN : type;
}

struct SetFiles {
	const char *cel;
	const char *min;
	const char *sol;
	const char *til;
	const char *pal;
	int microLen;
	int blocks;
};

SetFiles FilesFor(dungeon_type set)
{
	switch (set) {
	case DTYPE_TOWN:
		if (gbIsHellfire)
			return { "nlevels\\towndata\\town.cel", "nlevels\\towndata\\town.min", "nlevels\\towndata\\town.sol", "nlevels\\towndata\\town.til", "levels\\towndata\\town.pal", 16, 16 };
		return { "levels\\towndata\\town.cel", "levels\\towndata\\town.min", "levels\\towndata\\town.sol", "levels\\towndata\\town.til", "levels\\towndata\\town.pal", 16, 16 };
	case DTYPE_CATHEDRAL:
		return { "levels\\l1data\\l1.cel", "levels\\l1data\\l1.min", "levels\\l1data\\l1.sol", "levels\\l1data\\l1.til", "levels\\l1data\\l1_1.pal", 10, 10 };
	case DTYPE_CATACOMBS:
		return { "levels\\l2data\\l2.cel", "levels\\l2data\\l2.min", "levels\\l2data\\l2.sol", "levels\\l2data\\l2.til", "levels\\l2data\\l2_1.pal", 10, 10 };
	case DTYPE_CAVES:
		return { "levels\\l3data\\l3.cel", "levels\\l3data\\l3.min", "levels\\l3data\\l3.sol", "levels\\l3data\\l3.til", "levels\\l3data\\l3_1.pal", 10, 10 };
	case DTYPE_HELL:
		return { "levels\\l4data\\l4.cel", "levels\\l4data\\l4.min", "levels\\l4data\\l4.sol", "levels\\l4data\\l4.til", "levels\\l4data\\l4_1.pal", 12, 16 };
	case DTYPE_NEST:
		return { "nlevels\\l6data\\l6.cel", "nlevels\\l6data\\l6.min", "nlevels\\l6data\\l6.sol", "nlevels\\l6data\\l6.til", "nlevels\\l6data\\l6base1.pal", 10, 10 };
	case DTYPE_CRYPT:
		return { "nlevels\\l5data\\l5.cel", "nlevels\\l5data\\l5.min", "nlevels\\l5data\\l5.sol", "nlevels\\l5data\\l5.til", "nlevels\\l5data\\l5base.pal", 10, 10 };
	default:
		return { nullptr, nullptr, nullptr, nullptr, nullptr, 10, 10 };
	}
}

Tileset *LoadSet(dungeon_type set)
{
	const int index = static_cast<int>(set);
	if (index < 0 || index >= SetCount)
		return nullptr;
	Tileset &ts = Sets[index];
	if (ts.tried)
		return ts.ok ? &ts : nullptr;
	ts.tried = true;
	const SetFiles files = FilesFor(set);
	if (files.cel == nullptr)
		return nullptr;
	ts.cels = LoadFileInMem(files.cel);
	size_t minCount = 0;
	std::unique_ptr<uint16_t[]> min = LoadFileInMem<uint16_t>(files.min, &minCount);
	size_t solCount = 0;
	std::unique_ptr<uint8_t[]> sol = LoadFileInMem<uint8_t>(files.sol, &solCount);
	size_t tilCount = 0;
	std::unique_ptr<MegaTile[]> til = LoadFileInMem<MegaTile>(files.til, &tilCount);
	size_t palCount = 0;
	std::unique_ptr<uint8_t[]> pal = LoadFileInMem<uint8_t>(files.pal, &palCount);
	if (ts.cels == nullptr || min == nullptr || sol == nullptr || til == nullptr || pal == nullptr || palCount < 768) {
		LogWarn("Borrowed tiles: tileset {} is missing a file", index);
		return nullptr;
	}
	ts.microLen = files.microLen;
	const size_t blocks = static_cast<size_t>(files.blocks);
	ts.micros.assign(minCount / blocks, MICROS {});
	for (size_t i = 0; i < ts.micros.size(); i++) {
		const uint16_t *pieces = &min[blocks * i];
		// As SetDungeonMicros orders them: bottom pair first.
		for (size_t block = 0; block < blocks; block++)
			ts.micros[i].mt[block] = SDL_SwapLE16(pieces[blocks - 2 + (block & 1) - (block & 0xE)]);
	}
	ts.sol.resize(solCount);
	for (size_t i = 0; i < solCount; i++)
		ts.sol[i] = static_cast<TileProperties>(sol[i]);
	// The corrections LoadLevelSOLData makes to its own copy.
	if (set == DTYPE_CAVES && ts.sol.size() > 487) {
		ts.sol[170] |= TileProperties::BlockMissile;
		ts.sol[481] |= TileProperties::Solid;
		ts.sol[487] |= TileProperties::Solid;
	}
	if (set == DTYPE_HELL && ts.sol.size() > 210)
		ts.sol[210] = TileProperties::None;
	if (set == DTYPE_CRYPT && ts.sol.size() > 142)
		ts.sol[142] = TileProperties::None;
	ts.megatiles.assign(til.get(), til.get() + tilCount);
	for (int i = 0; i < 256; i++)
		ts.palette[i] = (static_cast<uint32_t>(pal[3 * i]) << 16) | (static_cast<uint32_t>(pal[3 * i + 1]) << 8) | pal[3 * i + 2];
	ts.ok = true;
	return &ts;
}

enum class Part : uint8_t {
	Floor,
	CellWithTriangles,
	CellWithoutTriangles,
};

/** @brief Renders @p part of piece @p native of @p ts, in its own cels and palette, into a cropped true-colour picture. */
Picture Render(Tileset &ts, uint16_t native, bool solid, Part part)
{
	Picture pic;
	if (native >= ts.micros.size())
		return pic;
	const MICROS &m = ts.micros[native];
	const int pairs = ts.microLen / 2;
	const int height = pairs * TILE_HEIGHT;
	OwnedSurface canvas = OwnedSurface::Rgb(TILE_WIDTH, height);
	const Surface surface = canvas;
	constexpr uint32_t Unwritten = 0xFF000000U; // a kernel writes XRGB, top byte 0
	for (int y = 0; y < height; y++) {
		uint32_t *row = surface.at<uint32_t>(0, y);
		std::fill_n(row, TILE_WIDTH, Unwritten);
	}
	// The set's own cels and palette for the drawing: the kernels read both from the globals.
	std::array<uint8_t, 256> identity;
	for (int i = 0; i < 256; i++)
		identity[i] = static_cast<uint8_t>(i);
	std::swap(pDungeonCels, ts.cels);
	const std::array<uint32_t, 256> savedPalette = PaletteRGB;
	PaletteRGB = ts.palette;
	const auto draw = [&](uint16_t micro, Point at, MaskType mask) {
		const LevelCelBlock block { micro };
		if (block.hasValue())
			RenderTile(surface, at, block, mask, identity.data());
	};
	const Point bottom { 0, height - 1 };
	const auto isTriangle = [](uint16_t micro) {
		const TileType type = LevelCelBlock { micro }.type();
		return type == TileType::LeftTriangle || type == TileType::RightTriangle;
	};
	if (part == Part::Floor) {
		// DrawFloor draws an open piece's two bottom micros whole; DrawWallFloorTriangles a wall's triangles only.
		for (int side = 0; side < 2; side++) {
			const uint16_t micro = m.mt[side];
			if (LevelCelBlock { micro }.hasValue() && (!solid || isTriangle(micro)))
				draw(micro, bottom + Displacement { side * TILE_WIDTH / 2, 0 }, MaskType::Solid);
		}
	} else {
		const bool withTriangles = part == Part::CellWithTriangles;
		for (int side = 0; side < 2; side++) {
			const uint16_t micro = m.mt[side];
			if (!LevelCelBlock { micro }.hasValue() || (!withTriangles && isTriangle(micro)))
				continue;
			const TileType type = LevelCelBlock { micro }.type();
			if (!solid) {
				// An open piece's bottom pair is DrawFloor's, but for its foliage - a tree's trunk, a bush's leaves.
				if (type == TileType::TransparentSquare)
					draw(micro, bottom + Displacement { side * TILE_WIDTH / 2, 0 }, side == 0 ? MaskType::LeftFoliage : MaskType::RightFoliage);
				continue;
			}
			draw(micro, bottom + Displacement { side * TILE_WIDTH / 2, 0 }, MaskType::Solid);
		}
		for (int i = 2; i + 1 < ts.microLen; i += 2) {
			const Point at = bottom - Displacement { 0, (i / 2) * TILE_HEIGHT };
			draw(m.mt[i], at, MaskType::Solid);
			draw(m.mt[i + 1], at + Displacement { TILE_WIDTH / 2, 0 }, MaskType::Solid);
		}
	}
	PaletteRGB = savedPalette;
	std::swap(pDungeonCels, ts.cels);

	// Cropped to the rows that hold anything; each row's opaque span.
	int firstRow = height;
	int lastRow = -1;
	for (int y = 0; y < height; y++) {
		const uint32_t *row = surface.at<uint32_t>(0, y);
		for (int x = 0; x < TILE_WIDTH; x++) {
			if (row[x] != Unwritten) {
				firstRow = std::min(firstRow, y);
				lastRow = y;
				break;
			}
		}
	}
	if (lastRow < 0)
		return pic;
	pic.height = lastRow - firstRow + 1;
	pic.top = (height - 1) - firstRow;
	pic.px.assign(static_cast<size_t>(pic.height) * TILE_WIDTH, 0);
	pic.first.assign(static_cast<size_t>(pic.height), 0);
	pic.last.assign(static_cast<size_t>(pic.height), 0);
	for (int y = 0; y < pic.height; y++) {
		const uint32_t *row = surface.at<uint32_t>(0, firstRow + y);
		int first = TILE_WIDTH;
		int last = 0;
		for (int x = 0; x < TILE_WIDTH; x++) {
			if (row[x] == Unwritten)
				continue;
			pic.px[static_cast<size_t>(y) * TILE_WIDTH + x] = 0xFF000000U | (row[x] & 0xFFFFFFU) | 0x010101U; // never pure black
			first = std::min(first, x);
			last = x + 1;
		}
		pic.first[y] = static_cast<int8_t>(std::min(first, last));
		pic.last[y] = static_cast<int8_t>(last);
	}
	return pic;
}

int BrightnessOf(int light)
{
	if (light < 0)
		return 256;
	if (light >= LightsMax)
		return 0;
	return SmoothLevelBrightness(static_cast<uint8_t>(light));
}

uint32_t ShadeRgb(uint32_t c, int brightness)
{
	const uint32_t r = std::max<uint32_t>(1, (((c >> 16) & 0xFF) * brightness) >> 8);
	const uint32_t g = std::max<uint32_t>(1, (((c >> 8) & 0xFF) * brightness) >> 8);
	const uint32_t b = std::max<uint32_t>(1, ((c & 0xFF) * brightness) >> 8);
	return (r << 16) | (g << 8) | b;
}

/** @brief @p pic with its bottom line at @p target's row; see-through inside the circle (@p holeX, @p holeY, @p radius). */
void Blit(const Surface &out, const Picture &pic, Point target, int brightness, int holeX = 0, int holeY = 0, int radius = 0)
{
	if (pic.height == 0 || brightness <= 0 || out.isIndexed())
		return;
	const int top = target.y - pic.top;
	const int r2 = radius * radius;
	const int y0 = std::max(0, -top);
	const int y1 = std::min(pic.height, out.h() - top);
	for (int y = y0; y < y1; y++) {
		const int sy = top + y;
		uint32_t *row = out.at<uint32_t>(0, sy);
		const uint32_t *src = &pic.px[static_cast<size_t>(y) * TILE_WIDTH];
		const int x0 = std::max<int>(pic.first[y], -target.x);
		const int x1 = std::min<int>(pic.last[y], out.w() - target.x);
		for (int x = x0; x < x1; x++) {
			const uint32_t c = src[x];
			if (c == 0)
				continue;
			const int sx = target.x + x;
			const uint32_t lit = brightness >= 256 ? (c & 0xFFFFFFU) : ShadeRgb(c, brightness);
			if (r2 > 0) {
				const int dx = sx - holeX;
				const int dy = (sy - holeY) * 3 / 2;
				const int d2 = dx * dx + dy * dy;
				if (d2 < r2) {
					// A third of the wall at the centre, all of it at the circle's edge - as the woods' trees.
					const int keep = 80 + 176 * d2 / r2;
					const uint32_t bg = row[sx];
					const auto mix = [&](int shift) { return ((((lit >> shift) & 0xFF) * keep + ((bg >> shift) & 0xFF) * (256 - keep)) >> 8) & 0xFF; };
					row[sx] = (static_cast<uint32_t>(mix(16)) << 16) | (static_cast<uint32_t>(mix(8)) << 8) | static_cast<uint32_t>(mix(0));
					continue;
				}
			}
			row[sx] = lit;
		}
	}
}

const Borrowed *Lookup(uint16_t piece)
{
	if (!IsBorrowedPiece(piece))
		return nullptr;
	const size_t index = piece - BorrowedPieceBase;
	return index < Pieces.size() ? &Pieces[index] : nullptr;
}

} // namespace

uint16_t BorrowPiece(dungeon_type set, uint16_t piece)
{
	set = FilesOf(set);
	if (set == FilesOf(leveltype))
		return piece; // the level's own
	const uint32_t key = (static_cast<uint32_t>(set) << 16) | piece;
	if (const auto found = PieceIds.find(key); found != PieceIds.end()) {
		SOLData[found->second] = Pieces[found->second - BorrowedPieceBase].sol; // whatever wrote SOLData since, the piece's own
		return found->second;
	}
	if (static_cast<int>(Pieces.size()) >= BorrowedPieceCapacity)
		return 0;
	Tileset *ts = LoadSet(set);
	if (ts == nullptr || piece >= ts->micros.size())
		return 0;
	Borrowed entry;
	entry.set = set;
	entry.native = piece;
	entry.sol = piece < ts->sol.size() ? ts->sol[piece] : TileProperties::None;
	const bool solid = HasAnyOf(entry.sol, TileProperties::Solid);
	entry.floor = Render(*ts, piece, solid, Part::Floor);
	entry.cellWithTriangles = Render(*ts, piece, solid, Part::CellWithTriangles);
	entry.cellWithoutTriangles = Render(*ts, piece, solid, Part::CellWithoutTriangles);
	const auto id = static_cast<uint16_t>(BorrowedPieceBase + Pieces.size());
	Pieces.push_back(std::move(entry));
	PieceIds.emplace(key, id);
	SOLData[id] = Pieces.back().sol;
	return id;
}

bool BorrowMegatile(dungeon_type set, int megatile, int x, int y)
{
	if (FilesOf(set) == FilesOf(leveltype)) {
		// The level's own: as FillTile places it.
		if (pMegaTiles == nullptr || megatile <= 0)
			return false;
		const MegaTile mega = pMegaTiles[megatile - 1];
		dPiece[x][y] = SDL_SwapLE16(mega.micro1);
		dPiece[x + 1][y] = SDL_SwapLE16(mega.micro2);
		dPiece[x][y + 1] = SDL_SwapLE16(mega.micro3);
		dPiece[x + 1][y + 1] = SDL_SwapLE16(mega.micro4);
		return true;
	}
	Tileset *ts = LoadSet(FilesOf(set));
	if (ts == nullptr || megatile <= 0 || static_cast<size_t>(megatile) > ts->megatiles.size())
		return false;
	const MegaTile &mega = ts->megatiles[static_cast<size_t>(megatile - 1)];
	const uint16_t ids[4] = { BorrowPiece(set, SDL_SwapLE16(mega.micro1)), BorrowPiece(set, SDL_SwapLE16(mega.micro2)),
		BorrowPiece(set, SDL_SwapLE16(mega.micro3)), BorrowPiece(set, SDL_SwapLE16(mega.micro4)) };
	for (const uint16_t id : ids) {
		if (id == 0)
			return false;
	}
	dPiece[x][y] = ids[0];
	dPiece[x + 1][y] = ids[1];
	dPiece[x][y + 1] = ids[2];
	dPiece[x + 1][y + 1] = ids[3];
	return true;
}

bool BorrowedMegatilePieces(dungeon_type set, int megatile, uint16_t (&pieces)[4])
{
	Tileset *ts = LoadSet(FilesOf(set));
	if (ts == nullptr || megatile <= 0 || static_cast<size_t>(megatile) > ts->megatiles.size())
		return false;
	const MegaTile &mega = ts->megatiles[static_cast<size_t>(megatile - 1)];
	pieces[0] = SDL_SwapLE16(mega.micro1);
	pieces[1] = SDL_SwapLE16(mega.micro2);
	pieces[2] = SDL_SwapLE16(mega.micro3);
	pieces[3] = SDL_SwapLE16(mega.micro4);
	return true;
}

int BorrowedMegatileCount(dungeon_type set)
{
	Tileset *ts = LoadSet(FilesOf(set));
	return ts == nullptr ? 0 : static_cast<int>(ts->megatiles.size());
}

void ApplyBorrowedSol()
{
	for (size_t i = 0; i < Pieces.size(); i++)
		SOLData[BorrowedPieceBase + i] = Pieces[i].sol;
}

void DrawBorrowedFloor(const Surface &out, uint16_t piece, Point target, int light)
{
	if (const Borrowed *b = Lookup(piece); b != nullptr)
		Blit(out, b->floor, target, BrightnessOf(light));
}

void DrawBorrowedCell(const Surface &out, Point tile, uint16_t piece, Point target, int light, bool withFloorTriangles)
{
	const Borrowed *b = Lookup(piece);
	if (b == nullptr)
		return;
	const Picture &pic = withFloorTriangles ? b->cellWithTriangles : b->cellWithoutTriangles;
	int holeX = 0;
	int holeY = 0;
	int radius = 0;
	// A wall or a fence in front of the hero (drawn after his row) and over him on screen lets him show through it.
	if (HasAnyOf(b->sol, TileProperties::Solid) && MyPlayer != nullptr && MyPlayer->isOnActiveLevel()) {
		const Point hero = MyPlayer->position.tile;
		const int ahead = (tile.x + tile.y) - (hero.x + hero.y);
		if (ahead > 0 && ahead <= 6) {
			const int footX = target.x + TILE_WIDTH / 2;
			const int footY = target.y - TILE_HEIGHT / 2 + 1;
			holeX = footX + ((hero.x - tile.x) - (hero.y - tile.y)) * (TILE_WIDTH / 2);
			holeY = footY + ((hero.x - tile.x) + (hero.y - tile.y)) * (TILE_HEIGHT / 2) - 36;
			radius = 64;
		}
	}
	Blit(out, pic, target, BrightnessOf(light), holeX, holeY, radius);
}

bool BorrowedPieceSourceForTest(uint16_t piece, dungeon_type &set, uint16_t &native)
{
	const Borrowed *b = Lookup(piece);
	if (b == nullptr)
		return false;
	set = b->set;
	native = b->native;
	return true;
}

void BorrowedPiecePixelsForTest(uint16_t piece, int &floorPixels, int &cellPixels)
{
	floorPixels = 0;
	cellPixels = 0;
	const Borrowed *b = Lookup(piece);
	if (b == nullptr)
		return;
	for (const uint32_t c : b->floor.px)
		floorPixels += c != 0 ? 1 : 0;
	for (const uint32_t c : b->cellWithTriangles.px)
		cellPixels += c != 0 ? 1 : 0;
}

} // namespace devilution::oracool
