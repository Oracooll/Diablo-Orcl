#include "oracool/signets.h"

#include <algorithm>

#include "items.h"
#include "multi.h"
#include "oracool/event_log.h"
#include "oracool/item_sets.h"
#include "oracool/mystic_orbs.h"
#include "player.h"
#include "utils/language.h"
#include "utils/str_cat.hpp"

namespace devilution::oracool {

namespace {

/**
 * @brief The claimed mask and the consumed count.
 *
 * File-local rather than fields on Player, because Player is a big shared struct and these two are
 * read by one system. They are keyed by the player's index, which is what every other per-player
 * side table in this fork does - and V1 is single-player, so the table is one entry that matters.
 */
uint32_t ClaimedMask[MAX_PLRS] = {};
uint8_t ConsumedCount[MAX_PLRS] = {};

size_t IndexOf(const Player &player)
{
	// Clamped, because these are fixed arrays and a player id is not something this file controls.
	// The alternative is the exact bug found in the treasure-class test an hour ago: an index that
	// is almost always in range, writing off the end of a stack array on the day it is not.
	return std::min<size_t>(player.getId(), MAX_PLRS - 1);
}

} // namespace

const char *MilestoneName(Milestone milestone)
{
	switch (milestone) {
	case Milestone::Level20:
		return N_("Reach level 20");
	case Milestone::Level40:
		return N_("Reach level 40");
	case Milestone::Level60:
		return N_("Reach level 60");
	case Milestone::Level80:
		return N_("Reach level 80");
	case Milestone::SlayDreadBoss:
		return N_("Slay a Dread boss");
	case Milestone::CompleteRuneword:
		return N_("Complete a runeword");
	case Milestone::FillOrbCap:
		return N_("Fill an item with Mystic Orbs");
	case Milestone::WearSetBonus:
		return N_("Wear a set bonus");
	}
	return "";
}

bool IsMilestoneClaimed(const Player &player, Milestone milestone)
{
	return (ClaimedMask[IndexOf(player)] & (1U << static_cast<int>(milestone))) != 0;
}

bool ClaimMilestone(Player &player, Milestone milestone)
{
	if (IsMilestoneClaimed(player, milestone))
		return false;
	// The claim is recorded BEFORE the award, so a cap-blocked signet still marks the milestone
	// done. A milestone that stayed claimable because the pool was full would pay out later, out of
	// order, for something the player did hours ago - and would read as a bug.
	ClaimedMask[IndexOf(player)] |= 1U << static_cast<int>(milestone);

	const bool granted = ConsumeSignet(player);
	if (&player == MyPlayer) {
		LogEvent(granted
		        ? StrCat("Milestone: ", _(MilestoneName(milestone)), " - a Signet of Learning")
		        : StrCat("Milestone: ", _(MilestoneName(milestone)), " - but no signets remain"));
	}
	return true;
}

void CheckPassiveMilestones(Player &player)
{
	// Only the milestones whose condition is READABLE from player state. The event-driven ones - a
	// boss killed, a runeword completed - are claimed at the point the event happens, because there
	// is nothing on the player afterwards that says it did.
	if (player._pLevel >= 20)
		ClaimMilestone(player, Milestone::Level20);
	if (player._pLevel >= 40)
		ClaimMilestone(player, Milestone::Level40);
	if (player._pLevel >= 60)
		ClaimMilestone(player, Milestone::Level60);
	if (player._pLevel >= 80)
		ClaimMilestone(player, Milestone::Level80);
	if (AnySetBonusActive(player))
		ClaimMilestone(player, Milestone::WearSetBonus);

	// An item at its orb cap, anywhere the player is carrying or wearing one.
	const auto atCap = [](const Item &item) {
		return !item.isEmpty() && item._iOracoolOrbCount >= MaxOrbsPerItem;
	};
	for (const Item &worn : player.InvBody) {
		if (atCap(worn)) {
			ClaimMilestone(player, Milestone::FillOrbCap);
			return;
		}
	}
	for (int i = 0; i < player._pNumInv; i++) {
		if (atCap(player.InvList[i])) {
			ClaimMilestone(player, Milestone::FillOrbCap);
			return;
		}
	}
}

int SignetsUsed(const Player &player)
{
	return ConsumedCount[IndexOf(player)];
}

bool CanConsumeSignet(const Player &player)
{
	return SignetsUsed(player) < SignetLifetimeCap;
}

bool ConsumeSignet(Player &player)
{
	if (!CanConsumeSignet(player))
		return false;
	ConsumedCount[IndexOf(player)]++;
	// Into the UNSPENT pool, so the player decides where it lands. A signet is a point, not a
	// prescription - and it also means the point obeys every rule the ordinary pool already has.
	player._pStatPts++;
	return true;
}

uint32_t PackMilestones(const Player &player)
{
	return ClaimedMask[IndexOf(player)];
}

void ApplyMilestones(Player &player, uint32_t mask)
{
	// Masked to the milestones that EXIST. A save written by a later version with more milestones
	// would otherwise set bits this build cannot name, and they would be written straight back out
	// - which is fine - but would also read as claimed here, silently withholding a reward.
	constexpr uint32_t Known = (MilestoneCount >= 32) ? ~0U : ((1U << MilestoneCount) - 1U);
	ClaimedMask[IndexOf(player)] = mask & Known;
}

uint8_t PackSignetsUsed(const Player &player)
{
	return ConsumedCount[IndexOf(player)];
}

void ApplySignetsUsed(Player &player, uint8_t used)
{
	// Clamped on read like every other persisted count in this fork: a corrupted byte must cost the
	// player a signet or two, never hand them an unbounded pool or an unusable one.
	ConsumedCount[IndexOf(player)] = std::min<uint8_t>(used, SignetLifetimeCap);
}

} // namespace devilution::oracool
