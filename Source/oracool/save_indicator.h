/**
 * @file save_indicator.h
 *
 * Oracool: a subtle top-left indicator used only for automatic saves. Manual saves via the game
 * menu keep the vanilla InitDiabloMsg(EMSG_GAME_SAVED) banner unchanged - see auto_save.cpp for
 * the autosave call site and gamemenu.cpp for the untouched manual-save one.
 */
#pragma once

#include "engine/surface.hpp"

namespace devilution::oracool {

/**
 * @brief Starts the indicator's blink-and-fade cycle. Call once, right after a successful
 * autosave. Safe to call again before the previous cycle finishes - it just restarts the timer.
 */
void TriggerSaveIndicator();

/**
 * @brief Draws the indicator if a cycle is currently active. No-op otherwise. Call once per
 * frame from the in-game UI overlay pass, alongside DrawDiabloMsg.
 */
void DrawSaveIndicator(const Surface &out);

} // namespace devilution::oracool
