/**
 * @file oracool/grid_bezel.h
 *
 * Oracool: the delivered stone bezel family (MPQ drop-zone sweep 2026-08-18, unit C) - carved
 * limestone frames that replace the procedural 3px bevel around every item slot and item grid.
 *
 * Six configurations ship, and every one of them has an exact home in this fork, which is the
 * strongest sign the pack was measured against the real build rather than guessed at:
 *
 *   1x1  40x40    the amulet and the two rings
 *   2x1  68x40    the equipment belt slot
 *   2x2  68x68    helm, shoulders, gloves, bracers, legs, boots
 *   2x3  68x96    chest, weapon, shield
 *   10x7 292x208  the backpack grid (oracool::GridSizeInCells)
 *   10x16 292x460 the stash grid (StashGridColumns x StashGridRows)
 *
 * The rule that ties them together is `outer = cells * 28 + 12`: a six-pixel frame on every side of
 * a grid whose cell pitch is CellPx. So a bezel is positioned six pixels up and left of the logical
 * rect it frames, and its (6,6) pixel lands on that rect's top-left corner. Cell geometry, hit
 * testing and item placement are all untouched - only the frame around them changes.
 *
 * The art is opaque all the way through, interior included (a near-black warm brown already
 * quantised against town.pal). It is therefore a complete recess plate, not just an outline: it
 * replaces the DrawThemedFill + DrawOrnateBorder pair these slots used to draw, rather than being
 * layered over them.
 *
 * Declared here rather than in hud_art.h, but IMPLEMENTED in hud_art.cpp: the asset loading,
 * palette quantisation and blitting machinery is all file-local there, and duplicating it for six
 * more PNGs would be worse than the split. The alternative - declaring it in hud_art.h - would mean
 * inventory_layout.h including hud_art.h for the inset constant, dragging player.h into a header
 * that is otherwise pure geometry.
 */
#pragma once

#include "engine/rectangle.hpp"
#include "engine/size.hpp"
#include "engine/surface.hpp"

namespace devilution::oracool {

/**
 * @brief How far the bezel extends beyond the rect it frames, on every side.
 *
 * Six, from the pack's own `outer = cells * 28 + 12`. Twice the 3px OrnateBorderWidth it replaces,
 * which is why the grids that clear the orbs had to be raised by another three pixels - see
 * GridOrigin in inventory_layout.h and the StashGridBottom assert in qol/stash.cpp.
 */
constexpr int GridBezelInset = 6;

/** @brief Whether a bezel exists for a content rect of @p contentSize. */
bool HasGridBezel(Size contentSize);

/**
 * @brief Draws the bezel that frames @p contentRect, positioned so its interior lands exactly on it.
 *
 * @p contentRect is the LOGICAL rect - the cells themselves, in screen coordinates. Silently draws
 * nothing if no bezel matches that size, so callers pair it with HasGridBezel and keep their old
 * frame as the fallback rather than losing their border outright.
 */
void DrawGridBezel(const Surface &out, Rectangle contentRect);

/**
 * @brief The 1x1 (ring and amulet) bezel stretched round a button's @p face, six pixels outside it.
 *
 * Every vendor and artisan button sits in a slot frame (user, 2026-10-02: "i want all buttons in
 * vendors/artisans to be put in a frame - the frame we use for skill buttons or item slots in inventory
 * screen"). Corners 1:1, bands stretched along their length, so the frame keeps the slots' thickness at
 * any button size. Frame only; draws nothing if the 1x1 bezel did not load.
 */
void DrawButtonBezel(const Surface &out, Rectangle face);

/** @brief The 2px down-left sink every held button in the mod wears (feedback_button_press_and_sound). */
constexpr Displacement ButtonSlotSink { -2, 2 };

/**
 * @brief Everything UNDER a framed button: the inventory slot's drop shadow, cast from @p rest and a pixel
 * smaller on every side while @p sunk, then DrawButtonBezel round the face - which has sunk with it.
 *
 * Call it before the face. Where buttons stand closer than two bezels apart, call it for the whole row first
 * and draw the faces after, so a neighbour's frame never lands on a face.
 */
void DrawButtonSlotGround(const Surface &out, Rectangle rest, bool sunk);

} // namespace devilution::oracool
