/**
 * @file furious_charge.h
 *
 * Oracool: the Paladin's Charge. Right-clicking a monster with it readied rushes the player toward
 * the target at double walking speed, then automatically swings on arrival, followed by a 3-second
 * cooldown. It began as a behavior/rendering substitution on the SpellID::ItemRepair slot, which kept
 * it out of the SpellID enum, save format and Player struct; since 2026-08-15 it is its own
 * SpellID::Charge, unlocked through oracool/paladin_skills.h, and Item Repair is plain Item Repair.
 */
#pragma once

#include "spelldat.h"
#include "utils/stdcompat/string_view.hpp"

namespace devilution::oracool {

/**
 * @brief True if this is a single-player game and the local player has unlocked the Paladin's
 * Charge - the only time SpellID::Charge should render/behave as Charge.
 */
bool IsFuriousChargeEnabled();

/**
 * @brief True if this SpellID slot should currently render/behave as Furious Charge. Only ever
 * true for SpellID::Charge (SpellID::ItemRepair until 2026-08-15) while IsFuriousChargeEnabled.
 */
bool IsFuriousChargeSpell(SpellID spellId);

/**
 * @brief The FALLBACK icon for Charge's slot - what the skill wells, the SpeedBook list and the
 * Abilities window draw only when Charge's own art is missing.
 *
 * Oracool: user request (2026-08-15) - "Heal Other icon to be gone. it is not correct to be visible
 * in the RMB or LMB." It was Heal Other: a borrowed icon chosen back when Charge had no art of its
 * own, picked over a Hellfire-exclusive spell so it would render with only the base Diablo MPQs. The
 * borrowing was always a placeholder and it read as the wrong ability, which is precisely the
 * complaint.
 *
 * SpellID::Null is the engine's EMPTY plate (frame 26 of spelli2, the same square every skill icon
 * now sits on), so missing art shows a bare slot rather than another spell's symbol: nothing claimed
 * rather than something wrong. Charge's own art has since shipped - frame 3 of
 * ui\paladin_tree_icons.png, and in ui\paladin_skill_icons.png - and every draw site asks
 * oracool::TryDrawSkillSpellIcon (or its Large twin) for it first; this plate is what is left.
 */
inline constexpr SpellID FuriousChargeIcon = SpellID::Null;

/**
 * @brief Oracool: user request - Charge's name from the Paladin skill table ("Charge" since
 * 2026-08-15, once "Furious Charge" shown over "Item Repair"), otherwise the spell's own translated
 * name unchanged. Every UI spot that displays a spell/skill's name by name should route through this
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
 * @brief The dash's walk-frame skip: 2 ticks a tile, 0.1 s at normal speed (dev note, 2026-09-27).
 *
 * A chained stride costs 6 - skip ticks, not 8 - skip: the next StartWalk comes with pmWillBeCalled (+1 frame) and
 * DoWalk runs again in the same tick. So a plain walk (-2) is 8 ticks, the run (2) is 4, and this is 4. It was 6,
 * which started every chained step on the walk's last frame: the hero crossed the whole approach in one tick and
 * snapped onto the monster (round 4 audit, v1.12.229).
 */
constexpr int ChargeDashSkipFrames = 4;

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

/** @brief Charge's arriving blow at @p rank, in percent more damage: 20 a level (2026-09-12). */
constexpr int ChargeBlowPercentAt(int rank)
{
	return 20 * (rank < 1 ? 1 : rank);
}

/** @brief Marks the swing now starting as a dash's arriving blow, or not. Set at every swing's start. */
void SetChargeBlowArmed(bool armed);

/** @brief Whether the swing in flight is a dash's arriving blow - the one that carries ChargeBlowPercentAt. */
bool IsChargeBlowArmed();

/** @brief Charge's arriving blow at @p rank, its dash and its cooldown, one per line. For the tooltip. */
std::string FuriousChargeFacts(int rank);

} // namespace devilution::oracool
