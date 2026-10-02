#include "oracool/furious_charge.h"

#include <SDL.h>

#include "engine/path.h"
#include "levels/gendung.h"
#include "options.h"
#include "oracool/oracool.h"
#include "oracool/paladin_skills.h"
#include "player.h"
#include "utils/language.h"
#include <fmt/format.h>

namespace devilution::oracool {

namespace {

// Bounds how long a dash can visually persist even if some interruption path (getting hit,
// clicking elsewhere mid-charge) fails to call StopFuriousChargeDash - more than enough time to
// cross the screen at double speed, but short enough that a missed cleanup call is barely
// noticeable rather than a lasting speed leak.
//
// Game ticks (20 a second), not SDL_GetTicks (round 11 audit, v1.12.236): on the wall clock a pause, the Esc menu or
// a level load used up the dash and the cooldown, and at a faster game speed the cooldown shrank and the dash grew.
constexpr int MaxDashDurationTicks = 40;  // 2 s
constexpr int CooldownDurationTicks = 60; // 3 s

int DashTicksLeft = 0;
int CooldownTicksLeft = 0;

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
	// SpellID::Charge, its own id since 2026-08-15. It was SpellID::ItemRepair - Charge was a
	// behaviour substitution ON the Paladin's class-skill slot, which is what made it displace Repair
	// from the Class Skills sheet and what stopped it being assignable like anything else. Repair is
	// now just Repair everywhere, and this asks about a spell that means only one thing.
	return spellId == SpellID::Charge && IsFuriousChargeEnabled();
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

/** @brief Whether the swing in flight ended a dash (2026-09-12) - see SetChargeBlowArmed. */
bool ChargeBlowArmed = false;

void SetChargeBlowArmed(bool armed)
{
	ChargeBlowArmed = armed;
}

bool IsChargeBlowArmed()
{
	return ChargeBlowArmed;
}

void StartFuriousChargeDash()
{
	DashTicksLeft = MaxDashDurationTicks;
}

void StopFuriousChargeDash()
{
	DashTicksLeft = 0;
}

bool IsFuriousChargeDashing()
{
	return DashTicksLeft > 0;
}

void StartFuriousChargeCooldown()
{
	CooldownTicksLeft = CooldownDurationTicks;
}

bool IsFuriousChargeOnCooldown()
{
	return CooldownTicksLeft > 0;
}

float GetFuriousChargeCooldownProgress()
{
	if (!IsFuriousChargeOnCooldown())
		return 1.0F;
	return 1.0F - static_cast<float>(CooldownTicksLeft) / static_cast<float>(CooldownDurationTicks);
}

void TickFuriousCharge()
{
	if (DashTicksLeft > 0)
		DashTicksLeft--;
	if (CooldownTicksLeft > 0)
		CooldownTicksLeft--;
}

void ResetFuriousChargeForNewGame()
{
	DashTicksLeft = 0;
	CooldownTicksLeft = 0;
	ChargeBlowArmed = false;
}

std::string FuriousChargeFacts(int rank)
{
	std::string out;
	const auto line = [&out](const std::string &s) {
		if (!out.empty())
			out += '\n';
		out += s;
	};
	line(fmt::format(fmt::runtime(_("Arriving blow: +{:d}% damage")), ChargeBlowPercentAt(rank)));
	line(fmt::format(fmt::runtime(_("Dash: up to {:.1f} s")), MaxDashDurationTicks / 20.0)); // 20 game ticks a second
	line(fmt::format(fmt::runtime(_("Cooldown: {:.1f} s")), CooldownDurationTicks / 20.0));
	return out;
}

bool ChargePathIsDirect(const Player &player, Point target)
{
	if (!InDungeonBounds(target) || player.position.tile == target)
		return false;
	int8_t path[MaxPathLength];
	const int steps = FindPath([&player](Point position) { return PosOkPlayer(player, position); }, player.position.future, target, path);
	// The movers' cap (TeleportTo since round 52): no more than a tile past the straight distance.
	return steps > 0 && steps <= player.position.tile.WalkingDistance(target) + ChargePathSlackTiles;
}

} // namespace devilution::oracool
