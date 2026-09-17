/**
 * @file oracool/sprite_import.h
 *
 * Oracool: lets a player class ship its animations as PNG sheets instead of the original CL2s.
 *
 * The engine has no route from PNG back into player graphics - the UI loads PNGs, but characters are
 * CL2 sheets out of the archive. That is the gap between "we have art for a new class" and "the class
 * looks like itself", and it is the reason the Barbarian and the Bard are aliases of the Warrior and
 * the Rogue in `PlayersData` rather than classes with their own bodies.
 *
 * The sheet layout is exactly what `tools/oracool_sprite_export.cpp` writes, so a set can be exported,
 * edited or regenerated, and dropped straight back in:
 *
 *     8 rows, one per facing in Direction order (South first), by N columns, one per frame.
 *
 * Drop a set into oracool.mpq beside the CL2 path it replaces, with a .png extension and the class's
 * OWN name rather than the class it borrows sprites from - `plrgfx\barbarian\bhd\bhdst.png`. Anything
 * not supplied falls through to the CL2 the game already loads, so a class can be converted one
 * animation at a time.
 */
#pragma once

#include <cstdint>
#include <memory>
#include <optional>

#include <SDL.h>

#include "engine/clx_sprite.hpp"
#include "oracool/sprite_colours.h"
#include "player.h"
#include "utils/stdcompat/optional.hpp"

namespace devilution::oracool {

/** @brief The class's own sprite folder, ignoring the class it borrows CL2s from. */
const char *ClassSpriteFolder(HeroClass heroClass);

/**
 * @brief Loads @p path (a .png, without the extension) as a facing-per-row sprite sheet.
 *
 * @param frameWidth The width of one frame, which is what tells the loader where the columns are.
 * @return nullopt if the file is absent or its dimensions are not a whole number of 8 x frameWidth
 * cells - in both cases the caller should fall back to the CL2.
 *
 * Colours are quantized into the palette's SHARED half (128-255), the range that is identical in town
 * and every dungeon type. That is what lets one import look right everywhere rather than only on the
 * level whose palette happened to be loaded when it was read.
 */
OptionalOwnedClxSpriteSheet LoadPngSpriteSheet(const char *path, uint16_t frameWidth);

/**
 * @brief The conversion on its own, for a surface already in hand.
 *
 * Split out from LoadPngSpriteSheet so the export tool can round-trip its own output - write a sheet,
 * read it back, and check the frames and their sizes survived. A renderer that merely compiles has
 * been shown twice in this project to prove nothing.
 *
 * @p surface may be in any format; it is converted internally.
 */
OptionalOwnedClxSpriteSheet SpriteSheetFromSurface(SDL_Surface *surface, uint16_t frameWidth);

/** @brief An imported sheet and what its indices mean - see oracool/sprite_colours.h. */
struct ColouredSpriteSheet {
	OwnedClxSpriteSheet sheet;
	std::shared_ptr<const SpriteColours> colours;
};

/**
 * @brief LoadPngSpriteSheet in TRUE COLOUR (2026-09-17): the sheet keeps its own colours - up to 255 of them, as its
 * own palette - instead of being squeezed into the 128 shared entries of the level palette. The indices of the
 * result mean nothing without the colours that come with it; the nearest shared entry survives only as each
 * colour's fallback, for shading and for 8-bit targets.
 *
 * What a player class's PNG sheets load through since the 32-bit renderer reached DrawPlayer. The quantizing
 * loader above stays for the callers that still draw indices.
 */
std::optional<ColouredSpriteSheet> LoadPngSpriteSheetColoured(const char *path, uint16_t frameWidth);

/** @brief The conversion on its own, for a surface already in hand - the testable half. */
std::optional<ColouredSpriteSheet> ColouredSpriteSheetFromSurface(SDL_Surface *surface, uint16_t frameWidth);

/**
 * @brief The same import for a MISSILE: `missiles\<name>.png`, or nullopt to fall back to the CL2.
 *
 * Oracool: the Cold pack, 2026-09-03. Thirteen sheets arrived as 32-bit PNGs and the engine had no
 * route from a PNG into missile graphics - MissileFileData::LoadGFX reads .cl2 only - which is the
 * same gap sprite_import was written to close for player bodies, one directory over.
 *
 * @p rows is 16 for a projectile, which is drawn once per facing, and 1 for an impact, a ground
 * effect or an armour shell, which look the same from every side. It is the missile's own animFAmt:
 * the engine already uses that field to decide whether it is loading one sheet or sixteen, so a
 * caller cannot get the two answers out of step.
 *
 * @p frameWidth is the missile's animWidth, and it is what tells the loader where the columns are -
 * a sheet whose width is not a whole number of frames is refused rather than sliced wrongly.
 */
std::optional<OwnedClxSpriteListOrSheet> LoadPngMissileSheet(const char *name, uint16_t frameWidth, int rows);

/**
 * @brief The same import for an ITEM's ground-drop tumble: `items\<name>.png`, one row of frames.
 *
 * Oracool: batch 10 (2026-09-11) - gems, runes, charms, orbs and the signet got their own tumbles
 * as PNGs, and InitItemGFX read CELs only. Same quantization as the missile route; nullopt when the
 * file is absent or not a whole number of @p frameWidth columns, and the caller falls back to a CEL.
 */
OptionalOwnedClxSpriteList LoadPngItemDropSheet(const char *name, uint16_t frameWidth);

/**
 * @brief One row of @p frameWidth-wide frames from a PNG at an EXACT archive path.
 *
 * The general form of the two helpers above, for callers whose asset does not live under a fixed
 * prefix. `std::nullopt` when the file is absent or is not a whole number of @p frameWidth columns,
 * so a caller can fall back to whatever it loaded before.
 *
 * Added 2026-09-12 for the Barbarian's character-select portrait: the engine's hero-portrait
 * override hook asks for a PCX (`ui_art\hero5`) and every other asset this fork ships is a PNG.
 *
 * @p palettePath is the palette to QUANTIZE against, and it must be the palette the art will be
 * DRAWN through. Defaults to the level palette, which is right for everything in the dungeon; pass
 * `ui_art\diablo.pal` for front-end art, because the front end loads that and it shares only one of
 * its 128 upper entries with town's. Getting this wrong is quiet - nearest-match still finds a
 * plausible colour - so it will not look broken, it will look subtly off.
 */
OptionalOwnedClxSpriteList LoadPngSpriteList(const char *path, uint16_t frameWidth, const char *palettePath = nullptr);

/**
 * @brief @p sheet redrawn at @p percent of its size, nearest neighbour, or nullopt for 100 (or a
 * sheet it cannot take apart). Frames keep their feet on the floor: a sprite is drawn anchored at
 * its bottom and centred by its own width, so a larger frame grows upward and stays centred with no
 * offset work anywhere else. For the Barbarian's 120% body - see oracool/hero_look.h.
 */
OptionalOwnedClxSpriteSheet ScaleSpriteSheet(ClxSpriteSheet sheet, int percent);

} // namespace devilution::oracool
