/**
 * @file xp_gain_indicator.h
 *
 * Oracool: a brief "+N (x.x%)" flash shown directly above the XP Counter whenever the player gains
 * experience - the percentage being that gain against the current level's whole experience span
 * (user request, 2026-08-16). Mirrors save_indicator.h's trigger-once/draw-per-frame pattern.
 */
#pragma once

#include <cstdint>

#include "engine/surface.hpp"

namespace devilution::oracool {

/**
 * @brief Starts the indicator's brief display cycle showing the given amount. Call once, right
 * after AddPlrExperience actually increases the local player's experience. Safe to call again
 * before the previous cycle finishes - it just restarts the timer with the new amount.
 */
void TriggerXpGainIndicator(uint64_t amount);

/**
 * @brief Draws the indicator if a cycle is currently active. No-op otherwise. Call once per
 * frame from the in-game UI overlay pass, alongside DrawXpCounter.
 */
void DrawXpGainIndicator(const Surface &out);

} // namespace devilution::oracool
