/**
 * @file clx_render.hpp
 *
 * CL2 rendering.
 */
#pragma once

#include <cstdint>
#include <utility>

#ifdef DEBUG_CLX
#include <string>
#endif

#include "engine/clx_sprite.hpp"
#include "engine/point.hpp"
#include "engine/surface.hpp"

namespace devilution {

/**
 * @brief Apply the color swaps to a CLX sprite list;
 */
void ClxApplyTrans(ClxSpriteList list, const uint8_t *trn);
void ClxApplyTrans(ClxSpriteSheet sheet, const uint8_t *trn);

/**
 * @brief Blit CL2 sprite, to the back buffer at the given coordianates
 * @param out Output buffer
 * @param position Target buffer coordinate
 * @param clx CLX frame
 * @param frame CL2 frame number
 */
void ClxDraw(const Surface &out, Point position, ClxSprite clx);

/**
 * @brief Same as ClxDraw but position.y is the top of the sprite instead of the bottom.
 */
inline void RenderClxSprite(const Surface &out, ClxSprite clx, Point position)
{
	ClxDraw(out, { position.x, position.y + static_cast<int>(clx.height()) - 1 }, clx);
}

/**
 * @brief Blit a solid colder shape one pixel larger than the given sprite shape, to the given buffer at the given coordinates
 * @param col Color index from current palette
 * @param out Output buffer
 * @param position Target buffer coordinate
 * @param clx CLX frame
 */
void ClxDrawOutline(const Surface &out, uint8_t col, Point position, ClxSprite clx);

/**
 * @brief Same as `ClxDrawOutline` but considers colors with index 0 (usually shadows) to be transparent.
 *
 * @param col Color index from current palette
 * @param out Output buffer
 * @param position Target buffer coordinate
 * @param clx CLX frame
 */
void ClxDrawOutlineSkipColorZero(const Surface &out, uint8_t col, Point position, ClxSprite clx);

/**
 * @brief Blit CL2 sprite, and apply given TRN to the given buffer at the given coordinates
 * @param out Output buffer
 * @param position Target buffer coordinate
 * @param clx CLX frame
 * @param trn TRN to use
 */
void ClxDrawTRN(const Surface &out, Point position, ClxSprite clx, const uint8_t *trn);

/**
 * @brief Same as ClxDrawTRN but position.y is the top of the sprite instead of the bottom.
 */
inline void RenderClxSpriteWithTRN(const Surface &out, ClxSprite clx, Point position, const uint8_t *trn)
{
	ClxDrawTRN(out, { position.x, position.y + static_cast<int>(clx.height()) - 1 }, clx, trn);
}

/**
 * @brief Renderer stage 3 (v1.11): draws the sprite with every index resolved through @p rgbMap,
 * a 256-entry table of colour VALUES (XRGB8888). Only for a 32-bit target; an indexed one is left
 * untouched, so callers keep a TRN for offscreen surfaces and the golden tests.
 */
void ClxDrawRgbMap(const Surface &out, Point position, ClxSprite clx, const uint32_t *rgbMap);

/** @brief Same as ClxDrawRgbMap but position.y is the top of the sprite instead of the bottom. */
inline void RenderClxSpriteWithRgbMap(const Surface &out, ClxSprite clx, Point position, const uint32_t *rgbMap)
{
	ClxDrawRgbMap(out, { position.x, position.y + static_cast<int>(clx.height()) - 1 }, clx, rgbMap);
}

void ClxDrawBlendedTRN(const Surface &out, Point position, ClxSprite clx, const uint8_t *trn);

/**
 * @brief Blit CLX sprite with 50% transparency to the given buffer at the given coordinates.
 * @param out Output buffer
 * @param position Target buffer coordinate
 * @param clx CLX frame
 */
void ClxDrawBlended(const Surface &out, Point position, ClxSprite clx);

/**
 * Returns if cursor is within the CLX sprite (ignores shadow)
 */
bool IsPointWithinClx(Point position, ClxSprite clx);

/**
 * Returns a pair of X coordinates containing the start (inclusive) and end (exclusive)
 * of fully transparent columns in the sprite.
 */
std::pair<int, int> ClxMeasureSolidHorizontalBounds(ClxSprite clx);

/**
 * @brief Clears the CLX draw cache.
 *
 * Must be called whenever CLX sprites are freed.
 */
void ClearClxDrawCache();

#ifdef DEBUG_CLX
std::string ClxDescribe(ClxSprite clx);
#endif

} // namespace devilution
