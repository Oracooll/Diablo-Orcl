#pragma once

#include <cstdint>

#include "engine/clx_sprite.hpp"
#include "engine/point.hpp"
#include "engine/size.hpp"
#include "engine/surface.hpp"
#include "spelldat.h"

#define SPLICONLENGTH 56

namespace devilution {

/**
 * Draw a large (56x56) spell icon onto the given buffer.
 *
 * @param out Output buffer.
 * @param position Buffer coordinates (bottom-left).
 * @param spell Spell ID.
 */
void DrawLargeSpellIcon(const Surface &out, Point position, SpellID spell);

/**
 * Draw a small (37x38) spell icon onto the given buffer.
 *
 * @param out Output buffer.
 * @param position Buffer coordinates (bottom-left).
 * @param spell Spell ID.
 */
void DrawSmallSpellIcon(const Surface &out, Point position, SpellID spell);

/**
 * @brief Oracool: the small spell icon's actual sprite dimensions, read from the loaded CEL.
 * The HUD's RMB well centres the readied-spell icon inside itself (see DrawSpell) and previously
 * assumed 37x38 - only the width is fixed by the load call, so the assumed height was wrong and
 * the icon sat off-centre. Only valid once LoadSmallSpellIcons() has run.
 */
Size GetSmallSpellIconSize();

/**
 * Draw an inset 2px border for a large (56x56) spell icon.
 *
 * @param out Output buffer.
 * @param position Buffer coordinates (bottom-left).
 * @param spell Spell ID.
 */
void DrawLargeSpellIconBorder(const Surface &out, Point position, uint8_t color);

/**
 * Draw an inset 2px border for a small (37x38) spell icon.
 *
 * @param out Output buffer.
 * @param position Buffer coordinates (bottom-left).
 * @param spell Spell ID.
 */
void DrawSmallSpellIconBorder(const Surface &out, Point position);

/**
 * @brief Set the color mapping for the `Draw(Small|Large)SpellIcon(Border)` calls.
 */
void SetSpellTrans(SpellType t);
/** @brief Oracool: the darker locked-plate grey - SetSpellTrans(Invalid) shifted four shades down the ramp. See the .cpp. */
void SetSpellTransDarkGrey();
/** @brief Oracool: the Skills-sheet green - plate ramps mapped onto the injected PAL8_GREEN ramp. */
void SetSpellTransGreen();
/** @brief Oracool: unlocked but UNSPENT - the plate ramps mapped onto the game's own PAL16_RED. */
void SetSpellTransRed();

/**
 * @brief Oracool: the blank plate, rescaled to cover @p cell and centred in it.
 *
 * The plate art is 37x38; a class-tree cell is 56x56. Drawn at its natural size the backing is
 * visibly smaller than the icon standing on it, which is what "make it fit the skill picture"
 * (2026-08-17) was about. Uses the current translation table, so callers set their tint first.
 */
void DrawSmallSpellIconScaledTo(const Surface &out, Rectangle cell);

/**
 * @brief As DrawSmallSpellIconScaledTo, but the plate is guaranteed to stay INSIDE @p cell.
 *
 * For the LMB/RMB wells, whose cell is the net opening between the bezels.
 */
void DrawSmallSpellIconFittedTo(const Surface &out, Rectangle cell, SpellID spell = SpellID::Null);

/**
 * @brief The 56px sheet's frame centred in @p cell with no resampling - for the Abilities window,
 * whose cells are the sheet's own size.
 */
void DrawLargeSpellIconCentredIn(const Surface &out, Rectangle cell, SpellID spell);

void LoadLargeSpellIcons();
void FreeLargeSpellIcons();

void LoadSmallSpellIcons();
void FreeSmallSpellIcons();

} // namespace devilution
