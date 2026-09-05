/**
 * @file oracool/item_tint.h
 *
 * Oracool: a colour per consumable that would otherwise be told apart only by its name (user,
 * 2026-09-05: "we need to have them have distinctive colors, because now they are indistinguishible",
 * then "do the same for the hellfire runes").
 *
 * The ten oils share one sprite - ICURS_OIL, a dark-grey glass flask with a small brown stopper. The
 * five Hellfire trap runes have a sprite each, but all five are stone tablets in dull earth tones.
 * Every one of them paints on ramps in the half of the palette that is identical in town and every
 * dungeon, so a palette translation that moves the sprite's ramp onto a vivid one recolours it the
 * same way everywhere it is drawn, with no new art. Whatever is not on the moved ramp - the flask's
 * stopper, the transparent key - is left alone.
 */
#pragma once

#include <cstdint>

#include "items.h"

namespace devilution::oracool {

/**
 * @brief The palette translation that colours @p item, or nullptr for anything without one. The
 * table is static; do not free it.
 */
const uint8_t *ItemTRN(const Item &item);

/** @brief The translation for a misc id directly - what the item overload looks up. */
const uint8_t *ItemTRN(item_misc_id miscId);

} // namespace devilution::oracool
