#pragma once
/**
 * @file oracool/wirt_cart.h
 *
 * Oracool: Wirt's merchant cart - the scenery beside him in town (user, 2026-09-22, with a
 * paint.net composite showing it parked by the thatched house).
 *
 * PURE SCENERY. It cannot be hovered, named, clicked, walked into or shot; nothing in the game
 * reads it and nothing can be done to it. The only reason it is an object at all is that objects
 * are what the town draws in TILE order - a cart blitted at a screen position would sit in front of
 * the hero on one step and behind him on the next.
 *
 * It is an OBJ_STAND wearing objects\orclcart.cel, which is how the Roar, the Stonegate and the
 * Stonegate's portal arch are all built. That makes it the FOURTH stand in a town whose code still
 * asks "is this a stand that is not the gate?" to mean "is this Levski's Cube" - see
 * IsLevskiRoarObject. IsWirtCartObject is here so that question can exclude this one too.
 *
 * That gate wants inverting rather than extending: a third exception is a sign the test should name
 * what it IS looking for. Not done here, because the Cube's identity would have to survive a town
 * reload and that is a change to the Cube, not to a cart.
 */

#include "objects.h"

namespace devilution::oracool {

/** @brief Parks the cart beside Wirt. Town only, and silent when there is no room for it. */
void AddWirtCartObject();

/** @brief Whether @p object is the cart - by identity, never by type. */
bool IsWirtCartObject(const Object &object);

} // namespace devilution::oracool
