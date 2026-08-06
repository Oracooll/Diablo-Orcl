#include "oracool/furious_charge.h"

#include <SDL.h>

#include "oracool/oracool.h"
#include "options.h"

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
	return *sgOptions.Oracool.furiousCharge && IsSinglePlayer();
}

bool IsFuriousChargeSpell(SpellID spellId)
{
	return spellId == SpellID::ItemRepair && IsFuriousChargeEnabled();
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
