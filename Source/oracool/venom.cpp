#include "oracool/venom.h"

#include <algorithm>
#include <array>

#include "misdat.h"
#include "player.h"

namespace devilution::oracool {

namespace {

struct VenomState {
	int ticks = 0;      // game ticks left
	int perTick = 0;    // damage per tick, in 64ths
	int remainder = 0;  // the dose not yet dealt, so a short dose is not rounded to nothing
};

/** Per player, for the session - cleared by ClearPlayerVenom from InitPlayer, like the cold armours. */
std::array<VenomState, MAX_PLRS> Venoms {};

VenomState &StateOf(const Player &player)
{
	return Venoms[player.getId()];
}

} // namespace

void PoisonPlayer(Player &player, int totalDamage, int ticks)
{
	if (totalDamage <= 0 || ticks <= 0)
		return;
	// Resisted as magic - the fork's rule for poison, which has no resistance channel of its own.
	const int resisted = totalDamage * (100 - std::clamp<int>(player._pMagResist, 0, 75)) / 100;
	if (resisted <= 0)
		return;
	VenomState &state = StateOf(player);
	// Refresh, never stack: the larger dose wins and the clock restarts.
	if (resisted > state.remainder) {
		state.remainder = resisted;
		state.ticks = ticks;
		state.perTick = std::max(1, resisted / ticks);
	} else {
		state.ticks = std::max(state.ticks, ticks);
	}
}

bool IsPlayerPoisoned(const Player &player)
{
	return StateOf(player).ticks > 0;
}

int PlayerPoisonTicks(const Player &player)
{
	return StateOf(player).ticks;
}

void TickPlayerVenom(Player &player)
{
	VenomState &state = StateOf(player);
	if (state.ticks <= 0)
		return;
	state.ticks--;
	const int dose = std::min(state.perTick, state.remainder);
	state.remainder -= dose;
	if (state.ticks == 0 || state.remainder <= 0) {
		state.ticks = 0;
		state.remainder = 0;
	}
	if (dose <= 0 || player._pHitPoints <= 0 || player._pInvincible)
		return;
	// Never the killing blow: a bleed leaves the player at a sliver, so the monster in front of them
	// is what kills them, not a number ticking in the background.
	ApplyPlrDamage(DamageType::Acid, player, 0, 1, dose);
}

void ClearPlayerVenom(const Player &player)
{
	StateOf(player) = {};
}

} // namespace devilution::oracool
