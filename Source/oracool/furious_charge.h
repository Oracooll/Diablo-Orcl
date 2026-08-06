/**
 * @file furious_charge.h
 *
 * Oracool: replaces the Warrior's free Item Repair skill with a charge attack. Right-clicking a
 * monster with it readied rushes the player toward the target at double walking speed, then
 * automatically swings on arrival, followed by a 3-second cooldown. Purely a behavior/rendering
 * substitution on top of the existing SpellID::ItemRepair slot - see furious_charge.cpp for why
 * this never touches the SpellID enum, save format, or Player struct.
 */
#pragma once

#include "spelldat.h"

namespace devilution::oracool {

/**
 * @brief True if the Furious Charge option is on and this is a single-player game - the only
 * time SpellID::ItemRepair should render/behave as Furious Charge instead of vanilla repair.
 */
bool IsFuriousChargeEnabled();

/**
 * @brief True if this SpellID slot should currently render/behave as Furious Charge. Only ever
 * true for SpellID::ItemRepair (the Warrior's own class-ability slot) while the option is on.
 */
bool IsFuriousChargeSpell(SpellID spellId);

/**
 * @brief Marks the approach-and-attack currently under way as a furious charge, so
 * StartWalkAnimation can double its speed. Call right before dispatching the attack-move command.
 */
void StartFuriousChargeDash();

/**
 * @brief Ends the dash (target reached, target died, or the attack landed). Safe to call even
 * when not dashing.
 */
void StopFuriousChargeDash();

/**
 * @brief True while a dash is in progress. Auto-expires after a few seconds as a safety net in
 * case some interruption path fails to call StopFuriousChargeDash.
 */
bool IsFuriousChargeDashing();

/**
 * @brief Starts the 3-second cooldown. Call once the charge resolves into an actual swing.
 */
void StartFuriousChargeCooldown();

/**
 * @brief True while still cooling down from the last charge.
 */
bool IsFuriousChargeOnCooldown();

/**
 * @brief 0.0 right after a swing, ramping linearly to 1.0 once the cooldown finishes (and staying
 * at 1.0 whenever the skill is actually ready). Drives the icon's bottom-to-top fill.
 */
float GetFuriousChargeCooldownProgress();

} // namespace devilution::oracool
