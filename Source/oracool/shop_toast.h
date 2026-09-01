/**
 * @file oracool/shop_toast.h
 *
 * Oracool: the shop's refusals, said without taking the screen away.
 *
 * Vanilla answers "you cannot afford this" by REPLACING the store with a full screen holding one
 * sentence, which the player then has to dismiss to get back to the shelf they were looking at. That
 * was reasonable when a store was a box of text lines and the message was the only way to say
 * anything. It is not reasonable against the grid shop, where the goods, the gold and the tabs are
 * all on screen at once: the answer to "can I buy this" arrives by hiding everything the player was
 * comparing, and costs a click to undo (user, 2026-08-31: "replace the vanilla 'not enogh gold'
 * during purchase with something more in line with the new shops design. try a pop up message which
 * doesnt require confirmation from my side").
 *
 * So: a banner over the shop panel that says what happened and then goes away on its own. Nothing is
 * closed, nothing is navigated, and the failed purchase is simply a purchase that did not happen.
 *
 * Timed on SDL_GetTicks rather than on game ticks, deliberately: a shop is one of the places the
 * game can sit idle, and a message that stops counting down while nothing moves would still be on
 * screen when the player comes back to it.
 */
#pragma once

#include <string>

#include "engine/rectangle.hpp"
#include "engine/surface.hpp"

namespace devilution::oracool {

/** @brief How long a toast stays up. Long enough to read a short sentence twice, short enough that
 * it is gone before it becomes something to wait for. */
constexpr uint32_t ShopToastDurationMs = 2200;

/** @brief Shows @p message over the shop panel, replacing any toast already up. */
void ShowShopToast(std::string message);

/** @brief Whether a toast is currently visible. Answers false once its time is up. */
bool IsShopToastVisible();

/**
 * @brief Where the banner sits: across the shop panel, in the band above its grid.
 *
 * Exported so the audit test can assert the property that matters and would otherwise rot quietly -
 * that the banner never covers the grid. The shop's layout has moved several times; a banner placed
 * by a literal would eventually be drawn over the item it is refusing.
 */
Rectangle GetShopToastRect();

/** @brief Draws the toast, if one is up. Call after the shop panel, so it reads on top of it. */
void DrawShopToast(const Surface &out);

/**
 * @brief Clears any toast, for game teardown.
 *
 * The message and its deadline are file-local statics keyed to SDL_GetTicks, so they outlive a GAME
 * rather than the process - the fault swept out of four other modules on 2026-08-31. Written with
 * the reset from the start rather than waiting to be found by the next audit.
 */
void ResetShopToastForNewGame();

} // namespace devilution::oracool
