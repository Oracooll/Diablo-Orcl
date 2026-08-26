#include "oracool/gradual_healing.h"

#include <algorithm>

#include "engine/backbuffer_state.hpp"
#include "options.h"
#include "oracool/oracool.h"
#include "player.h"

namespace devilution::oracool {

namespace {

// ~3 seconds at the default 20-ticks/sec game speed. Expressed in game logic ticks rather than
// wall-clock time so this scales with the player's own game-speed setting exactly like every
// other timed effect (attack speed, monster AI, etc.), and naturally pauses whenever the game
// simulation itself pauses.
constexpr int GradualEffectDurationTicks = 60;

int HealAmountRemaining = 0; // 1/64 HP units, matching Player::_pHitPoints's own fixed-point scale
int HealTicksRemaining = 0;

int ManaAmountRemaining = 0; // 1/64 mana units
int ManaTicksRemaining = 0;

} // namespace

bool IsGradualHealingEnabled()
{
	return *sgOptions.Oracool.gradualHealing && IsSinglePlayer();
}

void ResetGradualHealing()
{
	// Called when a game session starts (self-audit, 2026-08-15). These four are file-scope and
	// nothing cleared them: a potion drunk in the last three seconds before quitting to the main
	// menu left its remainder pending, and the next character loaded - a DIFFERENT character -
	// inherited the tail of the heal. Small in magnitude, but health appearing from a previous
	// hero's potion is exactly the kind of leak that surfaces as an unreproducible report later.
	HealAmountRemaining = 0;
	HealTicksRemaining = 0;
	ManaAmountRemaining = 0;
	ManaTicksRemaining = 0;
}

void QueueGradualHeal(int amount)
{
	HealAmountRemaining += amount;
	HealTicksRemaining += GradualEffectDurationTicks;
}

void QueueGradualMana(int amount)
{
	ManaAmountRemaining += amount;
	ManaTicksRemaining += GradualEffectDurationTicks;
}

void ProcessGradualHealing(Player &player)
{
	if (HealTicksRemaining > 0) {
		const int amount = HealAmountRemaining / HealTicksRemaining;
		HealAmountRemaining -= amount;
		HealTicksRemaining -= 1;
		if (amount > 0 && player._pHitPoints > 0) {
			player._pHitPoints = std::min(player._pHitPoints + amount, player._pMaxHP);
			player._pHPBase = std::min(player._pHPBase + amount, player._pMaxHPBase);
			RedrawComponent(PanelDrawComponent::Health);
		}
	}

	if (ManaTicksRemaining > 0) {
		const int amount = ManaAmountRemaining / ManaTicksRemaining;
		ManaAmountRemaining -= amount;
		ManaTicksRemaining -= 1;
		// `_pHitPoints > 0` matches the health branch above, and its absence here was the whole bug
		// (audit, 2026-08-26): a queued mana potion went on filling a corpse's orb. The ticks still
		// count down while dead, exactly as they do for health, so nothing lingers past revival.
		if (amount > 0 && player._pHitPoints > 0 && HasNoneOf(player._pIFlags, ItemSpecialEffect::NoMana)) {
			player._pMana = std::min(player._pMana + amount, player._pMaxMana);
			player._pManaBase = std::min(player._pManaBase + amount, player._pMaxManaBase);
			RedrawComponent(PanelDrawComponent::Mana);
		}
	}
}

} // namespace devilution::oracool
