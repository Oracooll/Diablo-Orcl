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

#include <SDL.h>

#include "engine/clx_sprite.hpp"
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

} // namespace devilution::oracool
