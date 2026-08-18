#include "oracool/game_speed.h"

#include <SDL.h>

#include <algorithm>

#include "diablo.h" // gnTickDelay
#include "multi.h"  // sgGameInitInfo
#include "options.h"
#include "oracool/oracool.h" // IsSinglePlayer

namespace devilution::oracool {

namespace {

/** @brief SDL_GetTicks at the last successful change, or 0 if the speed has not moved this session. */
uint32_t LastChangeTick = 0;

/** @brief How long the Blink readout stays up after a change. One second, as asked for. */
constexpr uint32_t BlinkDurationMs = 1000;

} // namespace

bool AdjustGameSpeed(int delta)
{
	if (!IsSinglePlayer())
		return false;

	const int current = sgGameInitInfo.nTickRate;
	const int wanted = std::clamp(current + delta * GameSpeedStep, MinGameSpeed, MaxGameSpeed);
	if (wanted == current)
		return false; // already at an end of the band - report it so the caller can stay silent

	// The same three lines demo mode uses (engine/demomode.cpp), and all three are load-bearing:
	// nTickRate is what the rest of the engine reads, the option is what survives the session, and
	// gnTickDelay is what the main loop actually waits on. Setting only the first would change
	// nothing until the next level load.
	sgGameInitInfo.nTickRate = static_cast<uint8_t>(wanted);
	sgOptions.Gameplay.tickRate.SetValue(wanted);
	gnTickDelay = 1000 / wanted;

	LastChangeTick = SDL_GetTicks();
	return true;
}

int CurrentGameSpeed()
{
	return sgGameInitInfo.nTickRate;
}

bool GameSpeedChangedRecently()
{
	if (LastChangeTick == 0)
		return false;
	return SDL_GetTicks() - LastChangeTick < BlinkDurationMs;
}

} // namespace devilution::oracool
