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
		if (amount > 0 && HasNoneOf(player._pIFlags, ItemSpecialEffect::NoMana)) {
			player._pMana = std::min(player._pMana + amount, player._pMaxMana);
			player._pManaBase = std::min(player._pManaBase + amount, player._pMaxManaBase);
			RedrawComponent(PanelDrawComponent::Mana);
		}
	}
}

} // namespace devilution::oracool
