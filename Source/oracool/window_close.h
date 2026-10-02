/**
 * @file oracool/window_close.h
 *
 * Oracool: the standing rule that every window the player can open, the player can close - with a
 * small red X in its own top-right corner, clickable with the mouse.
 *
 * This exists as one shared helper rather than a convention because a convention is what Levski's
 * Roar broke: it shipped with no close control at all, and the only way out was quitting to the
 * main menu. A rule enforced by remembering to copy three lines is not a rule. Every window's draw
 * calls DrawWindowCloseButton and every window's click handler calls CheckWindowCloseButtonClick,
 * both taking the window's own rect, so the button cannot be in the wrong place and cannot be
 * forgotten without the omission being visible in the diff.
 *
 * The button sits INSIDE the window's top-right corner, inset by the ornate border's own width so
 * it never overlaps the frame it sits in.
 */
#pragma once

#include <functional>

#include "engine/point.hpp"
#include "engine/rectangle.hpp"
#include "engine/surface.hpp"

namespace devilution::oracool {

/** @brief The close button's size, square. Small enough not to crowd a title, large enough to hit
 * without aiming - the same reasoning the belt buttons use. */
constexpr int WindowCloseButtonSize = 18;

/** @brief Where the close button sits for a window occupying @p window. */
Rectangle GetWindowCloseButtonRect(const Rectangle &window);

/** @brief Draws the red X in @p window's top-right corner. Call from the window's own draw, after
 * its background and border so the button reads on top of them. */
void DrawWindowCloseButton(const Surface &out, const Rectangle &window);
/**
 * @brief The same red X drawn at @p button itself - for a window whose skin says where its close
 * button goes (Levski's Roar, whose painted frame's corner is not the rect's corner).
 */
void DrawWindowCloseButtonAt(const Surface &out, const Rectangle &button);
/**
 * @brief The close button's plate-and-X shape in any two colours - for a control that should read
 * as the X's twin (the Runeword book's yellow "possible runewords" toggle, mirrored to the top-left).
 */
void DrawWindowCloseButtonStyled(const Surface &out, const Rectangle &button, uint8_t glyphColor, uint8_t plateColor);

/**
 * @brief The PRESS half of @p window's close button: true when @p mousePosition is on it, so the caller stops routing
 * the click. The face sinks and the click sounds (IS_TITLEMOV), but nothing closes yet: @p close runs from
 * ReleaseWindowCloseButton, and only when the release lands back inside the same X (the press/release default,
 * 2026-09-21; the X kept acting on the press until round 75). The caller says how to close - this helper deliberately
 * does not know how, so it works for every window regardless of what closing one involves.
 *
 * @p close must be safe to run on a window already closed some other way (Esc while the button is held): test the
 * window's own open flag in it. A click handler, not a hit test: never ask it from a hover or a draw.
 */
bool CheckWindowCloseButtonClick(const Rectangle &window, Point mousePosition, std::function<void()> close);
/** @brief As CheckWindowCloseButtonClick, for an X placed at @p button itself (see DrawWindowCloseButtonAt). */
bool PressWindowCloseButtonAt(const Rectangle &button, Point mousePosition, std::function<void()> close);
/** @brief LeftMouseUp: the pressed X springs back, and its window closes if the release is inside it. Safe when none is pressed. */
void ReleaseWindowCloseButton(Point mousePosition);
/** @brief Whether an X is held down right now (tests). */
bool IsWindowCloseButtonPressed();

} // namespace devilution::oracool
