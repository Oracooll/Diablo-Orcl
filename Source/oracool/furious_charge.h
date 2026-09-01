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
#include "utils/stdcompat/string_view.hpp"

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
 * @brief The icon Charge's slot draws everywhere it appears - the skill wells, the SpeedBook list
 * and the Abilities window.
 *
 * Oracool: user request (2026-08-15) - "Heal Other icon to be gone. it is not correct to be visible
 * in the RMB or LMB." It was Heal Other: a borrowed icon chosen back when Charge had no art of its
 * own, picked over a Hellfire-exclusive spell so it would render with only the base Diablo MPQs. The
 * borrowing was always a placeholder and it read as the wrong ability, which is precisely the
 * complaint.
 *
 * SpellID::Null is the engine's EMPTY plate (frame 26 of spelli2, the same square every skill icon
 * now sits on), so Charge shows a bare slot rather than another spell's symbol. That is the honest
 * state while the user redraws the skill icons: nothing claimed rather than something wrong. When
 * Charge's own art ships in ui\paladin_skill_icons.png, this is where it gets pointed at it.
 */
inline constexpr SpellID FuriousChargeIcon = SpellID::Null;

/**
 * @brief Oracool: user request - "Furious Charge" (translated) wherever a UI would otherwise show
 * this spell slot's real name ("Item Repair"), otherwise the spell's own translated name
 * unchanged. Every UI spot that displays a spell/skill's name by name should route through this
 * instead of reading GetSpellData(spellId).sNameText directly, so the substitute name follows
 * automatically wherever the real name would otherwise show.
 */
string_view GetSpellDisplayName(SpellID spellId);

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

/**
 * @brief Clears the dash and the cooldown, for game teardown.
 *
 * Both are file-local statics keyed to SDL_GetTicks, so they outlive a GAME rather than the process.
 * Quit mid-charge, or inside the three seconds after one, and the next character in the same session
 * inherits it: a dash that is already running, or a Furious Charge that reports itself on cooldown
 * with the icon half-filled before they have swung at anything.
 *
 * Both self-expire on their own timers, so the leak is bounded by CooldownDurationMs rather than
 * permanent - this is hardening in the same spirit as ResetZealChain, not a reported fault. It costs
 * two assignments to make the window zero instead of three seconds.
 */
void ResetFuriousChargeForNewGame();

} // namespace devilution::oracool
