/**
 * @file gradual_healing.h
 *
 * Oracool: optional Diablo 2-style heal-over-time for partial Healing/Mana Potions. Instead of
 * an instant lump-sum restore, the potion's usual random amount is delivered gradually over a
 * few seconds - Full Healing/Full Mana Potions stay instant either way, mirroring how D2 keeps
 * its emergency-button Rejuvenation potions instant while throttling its common tiers. All state
 * here is runtime-only (module-level globals, like furious_charge.h's dash/cooldown tracking) -
 * it never touches the save format, so this is safe to toggle on/off freely and a save/load
 * mid-drip simply drops whatever hadn't been delivered yet, the same way any other short-lived
 * buff would.
 */
#pragma once

#include "player.h"

namespace devilution::oracool {

/**
 * @brief True if Gradual Healing is on and this is a single-player game - the only time a
 * partial Healing/Mana Potion should drip instead of applying instantly.
 */
bool IsGradualHealingEnabled();

/**
 * @brief Clears any pending gradual heal/mana. Call when a game session starts, so a potion drunk
 * in the last seconds of the previous session cannot drip onto the next character loaded.
 */
void ResetGradualHealing();

/**
 * @brief Queues `amount` (in the game's 1/64 HP fixed-point units) of life to be delivered
 * gradually instead of instantly. Adds to, rather than replaces, whatever's already pending.
 */
void QueueGradualHeal(int amount);

/**
 * @brief Same as QueueGradualHeal, for mana.
 */
void QueueGradualMana(int amount);

/**
 * @brief Call once per game logic tick for the local player. Drains whatever life/mana is
 * currently pending, a little at a time, redrawing the health/mana globe as it changes. A no-op
 * once nothing is pending, regardless of whether Gradual Healing is currently enabled - so
 * disabling the option mid-drip still lets an already-queued heal finish delivering.
 */
void ProcessGradualHealing(Player &player);

} // namespace devilution::oracool
