#include "oracool/chill.h"

#include <algorithm>
#include <array>

#include "monster.h"

namespace devilution::oracool {

namespace {

/**
 * Ticks of chill remaining, indexed by monster id. Zero is "not chilled".
 *
 * Parallel to Monsters[], not part of it - see the header for why a status effect that must not
 * survive a level change does not belong in the level save.
 */
std::array<uint16_t, MaxMonsters> ChillTicks {};

} // namespace

void ChillMonster(const Monster &monster, int ticks)
{
	if (ticks <= 0)
		return;
	const size_t id = monster.getId();
	if (id >= ChillTicks.size())
		return;
	// EXTENDS, never adds - the header says why.
	ChillTicks[id] = static_cast<uint16_t>(std::max<int>(ChillTicks[id], ticks));
}

bool IsMonsterChilled(const Monster &monster)
{
	const size_t id = monster.getId();
	return id < ChillTicks.size() && ChillTicks[id] > 0;
}

bool ChillTakesThisTick(const Monster &monster)
{
	const size_t id = monster.getId();
	if (id >= ChillTicks.size() || ChillTicks[id] == 0)
		return false;

	// The chill ages on EVERY tick, including the ones it takes. Counting only the ticks the monster
	// got to act would make a two-second chill last four seconds of wall clock, and every duration in
	// this file would quietly mean twice what it says.
	ChillTicks[id]--;

	// Alternate ticks, and the counter's own parity is what alternates them - no clock needed. The
	// monster's id is added so a room full of chilled monsters staggers as individuals rather than
	// all freezing on the same tick, which reads as the game stuttering rather than as an effect.
	return ((ChillTicks[id] + id) & 1U) == 1;
}

void ClearChills()
{
	ChillTicks.fill(0);
}

} // namespace devilution::oracool
