#include "oracool/furious_charge.h"

#include <SDL.h>

#include "options.h"
#include "oracool/oracool.h"
#include "oracool/paladin_skills.h"
#include "player.h"
#include "utils/language.h"

namespace devilution::oracool {

namespace {

// Bounds how long a dash can visually persist even if some interruption path (getting hit,
// clicking elsewhere mid-charge) fails to call StopFuriousChargeDash - more than enough time to
// cross the screen at double speed, but short enough that a missed cleanup call is barely
// noticeable rather than a lasting speed leak.
constexpr uint32_t MaxDashDurationMs = 2000;
constexpr uint32_t CooldownDurationMs = 3000;

bool DashActive = false;
uint32_t DashStartTime = 0;

bool CooldownActive = false;
uint32_t CooldownStartTime = 0;

} // namespace

bool IsFuriousChargeEnabled()
{
	// Oracool: user request (2026-08-15) - reopened. It was closed on 2026-08-11 with the note that
	// this would become an ACQUIRABLE skill rather than a settings toggle, and that switching it on
	// again would be "a one-line change to whatever the Skills system uses to record what the player
	// has learned". That record is oracool/paladin_skills.h, and this is that line: a level gate
	// (12) rather than an INI entry, so the Paladin's slot is vanilla Item Repair - the skill he is
	// "gifted at birth" - right up until he earns the charge.
	//
	// MyPlayer rather than a passed-in player because the callers are rendering paths asking "does
	// this slot show as Charge", and the slot they draw is always the local player's own.
	return MyPlayer != nullptr && IsSinglePlayer()
	    && IsPaladinSkillUnlocked(*MyPlayer, PaladinSkill::Charge);
}

bool IsFuriousChargeSpell(SpellID spellId)
{
	return spellId == SpellID::ItemRepair && IsFuriousChargeEnabled();
}

string_view GetSpellDisplayName(SpellID spellId)
{
	// The name comes from the skill table rather than being spelled again here, so the Abilities
	// window's row and the readied-spell caption cannot disagree about what it is called. It is
	// "Charge" as of 2026-08-15, shortened from "Furious Charge" on the user's call.
	if (IsFuriousChargeSpell(spellId))
		return _(GetPaladinSkillData(PaladinSkill::Charge).name);
	return pgettext("spell", GetSpellData(spellId).sNameText);
}

void StartFuriousChargeDash()
{
	DashActive = true;
	DashStartTime = SDL_GetTicks();
}

void StopFuriousChargeDash()
{
	DashActive = false;
}

bool IsFuriousChargeDashing()
{
	if (!DashActive)
		return false;
	if (SDL_GetTicks() - DashStartTime >= MaxDashDurationMs) {
		DashActive = false;
		return false;
	}
	return true;
}

void StartFuriousChargeCooldown()
{
	CooldownActive = true;
	CooldownStartTime = SDL_GetTicks();
}

bool IsFuriousChargeOnCooldown()
{
	if (!CooldownActive)
		return false;
	if (SDL_GetTicks() - CooldownStartTime >= CooldownDurationMs) {
		CooldownActive = false;
		return false;
	}
	return true;
}

float GetFuriousChargeCooldownProgress()
{
	if (!IsFuriousChargeOnCooldown())
		return 1.0F;
	const uint32_t elapsed = SDL_GetTicks() - CooldownStartTime;
	return static_cast<float>(elapsed) / static_cast<float>(CooldownDurationMs);
}

} // namespace devilution::oracool
