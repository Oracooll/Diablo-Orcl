/**
 * @file oracool/game_speed.h
 *
 * Oracool: user request (2026-08-18) - F9 and F10 step the game's speed down and up while playing.
 *
 * "Speed" here is the engine's tick rate: sgGameInitInfo.nTickRate, ticks per second, default 20.
 * Everything in the simulation is paced off it, so raising it makes the whole game run faster rather
 * than just animating faster.
 *
 * Its own module rather than a lambda in diablo.cpp because two callers need it: the key handler
 * that changes the speed, and the clock corner that reports it.
 */
#pragma once

namespace devilution::oracool {

/** @brief The engine's own default, and the bottom of the band F9/F10 move within. */
constexpr int MinGameSpeed = 20;
/** @brief Three times the default. Past this the game stops being playable rather than fast. */
constexpr int MaxGameSpeed = 60;
/** @brief One press. Twenty steps across the band - coarse enough to matter, fine enough to tune. */
constexpr int GameSpeedStep = 2;

/**
 * @brief Steps the tick rate by @p delta steps, clamped to the band. True if it actually moved.
 *
 * Single-player only: nTickRate is part of sgGameInitInfo, agreed when the game is created and
 * shared by every peer, so changing it mid-session in multiplayer would desync. V1 is single-player
 * anyway - this is a guard rather than a feature.
 */
bool AdjustGameSpeed(int delta);

/** @brief The current tick rate. */
int CurrentGameSpeed();

/** @brief Whether the speed changed within the last second - what the Blink readout asks. */
bool GameSpeedChangedRecently();

} // namespace devilution::oracool
