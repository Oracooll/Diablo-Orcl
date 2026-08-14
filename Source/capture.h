/**
 * @file capture.h
 *
 * Interface of the screenshot function.
 */
#pragma once

namespace devilution {

/**
 * @brief Saves the current game frame as a PNG under Screenshots/, then flashes the screen red.
 */
void CaptureScreen();

/**
 * @brief CaptureScreen's front-end twin, for the title, menu, settings and character screens.
 *
 * Oracool: user report - "you need to make possible screenshot taking while in menus. i cant take ss
 * now." Correct, and for two reasons at once. The Screenshot action lives in the in-game Keymapper,
 * which the front-end event loop never consults, so the key did nothing there; and CaptureScreen
 * begins with DrawAndBlit, which renders the dungeon view - not something a menu can be asked for.
 * This captures the UI surface as it already stands. Bound in DiabloUI/diabloui.cpp's UiHandleEvents.
 */
void CaptureUiScreen();

} // namespace devilution
