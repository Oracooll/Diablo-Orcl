/**
 * @file oracool/oil_tint.h
 *
 * Oracool: a colour per oil, so the ten can be told apart in a grid (user, 2026-09-05: "we need to
 * have them have distinctive colors, because now they are indistinguishible").
 *
 * All ten oils share one sprite - ICURS_OIL, a dark-grey glass flask with a small brown stopper. Its
 * glass sits entirely on the grey ramp (palette 240..255), which is in the half of the palette that
 * is identical in town and every dungeon, so a palette translation that moves the grey ramp onto a
 * coloured one recolours the flask the same way everywhere it is drawn, with no new art. The stopper
 * and the transparent key are left alone.
 */
#pragma once

#include <cstdint>

#include "items.h"

namespace devilution::oracool {

/**
 * @brief The palette translation that colours @p item's flask, or nullptr for anything that is not
 * one of the ten named oils (the generic "Oil" placeholder included). The table is static; do not
 * free it.
 */
const uint8_t *OilTRN(const Item &item);

/** @brief The translation for a misc id directly - what the item overload looks up. */
const uint8_t *OilTRN(item_misc_id miscId);

} // namespace devilution::oracool
