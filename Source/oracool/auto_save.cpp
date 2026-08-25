#include "oracool/auto_save.h"

#include <string>
#include <algorithm>
#include <chrono>
#include <fmt/format.h>

#include "cursor.h"
#include "diablo.h"
#include "engine/demomode.h"
#include "gmenu.h"
#include "inv.h"
#include "loadsave.h"
#include "minitext.h"
#include "multi.h"
#include "options.h"
#include "oracool/save_status.h"
#include "oracool/event_log.h"
#include "oracool/save_indicator.h"
#include "pfile.h"
#include "player.h"
#include "qol/stash.h"
#include "utils/language.h"
#include "stores.h"

namespace devilution::oracool {
namespace {

using Clock = std::chrono::steady_clock;

Clock::time_point LastSave = Clock::now();
Clock::time_point PendingSaveTime;
bool SavePending = false;

bool IsEnabled()
{
	return !gbIsMultiplayer && *sgOptions.Oracool.autoSave;
}

/**
 * @brief Whether saving happens AT ALL for this session, regardless of the periodic preference.
 *
 * Audit finding, 2026-08-26. SaveOnExit was gated on IsEnabled(), which includes the user's Auto
 * Save preference - so turning that option OFF did not merely stop the periodic autosave, it
 * removed the only remaining way a single-player character was ever written to disk. The option
 * reads as "save every N minutes"; it silently meant "never save".
 *
 * Exit saving is not a convenience feature and does not belong behind that switch. The multiplayer
 * exclusion stays: multiplayer keeps its own persistence and V1 does not use it anyway.
 */
bool SavingIsPossible()
{
	return !gbIsMultiplayer;
}

bool IsSafeToSave()
{
	return IsEnabled()
	    && gbRunGame
	    && MyPlayer != nullptr
	    && !MyPlayerIsDead
	    && MyPlayer->_pmode != PM_DEATH
	    && PauseMode == 0
	    && !gmenu_is_active()
	    && stextflag == TalkID::None
	    && !qtextflag
	    && pcurs == CURSOR_HAND
	    && !demo::IsRunning()
	    && !demo::IsRecording();
}

/**
 * @brief Puts a cursor-held item somewhere it will be saved, before the save happens.
 *
 * Tried in the order a player would expect to find it again: inventory, then belt, then stash.
 * Dropping it on the ground is NOT an option here - this path writes no world snapshot, so the
 * floor is the one place it would certainly not survive.
 *
 * If every container is full the item stays on the cursor and the save proceeds without it. That
 * is a real loss and it is reported rather than hidden; refusing to exit instead would trap a
 * player with no free space in a menu they cannot leave, which is worse. In practice it requires
 * inventory, belt AND stash to be simultaneously full.
 */
void ReturnHeldItemBeforeSaving(Player &player)
{
	if (player.HoldItem.isEmpty())
		return;

	const std::string name = player.HoldItem._iIName;
	bool placed = AutoPlaceItemInInventory(player, player.HoldItem, /*persistItem=*/true);
	if (!placed)
		placed = AutoPlaceItemInBelt(player, player.HoldItem, /*persistItem=*/true);
	if (!placed && !IsStashOpen)
		placed = AutoPlaceItemInStash(player, player.HoldItem, /*persistItem=*/true);

	if (placed) {
		player.HoldItem.clear();
		if (&player == MyPlayer)
			NewCursor(CURSOR_HAND);
		LogEvent(fmt::format(fmt::runtime(_("{:s} returned to your pack before saving.")), name),
		    UiFlags::ColorWhitegold);
		return;
	}

	LogEvent(fmt::format(fmt::runtime(_("No room to put {:s} away - it will not be saved.")), name),
	    UiFlags::ColorRed);
}

void ScheduleAfterSeconds(int seconds)
{
	if (!IsEnabled())
		return;
	SavePending = true;
	PendingSaveTime = Clock::now() + std::chrono::seconds(std::max(seconds, 0));
}

} // namespace

void ResetAutoSave()
{
	LastSave = Clock::now();
	SavePending = false;
}

void NotifyGameSaved()
{
	ResetAutoSave();
}

void ScheduleAutoSaveForItemPickup()
{
	if (*sgOptions.Oracool.autoSaveOnItemPickup)
		ScheduleAfterSeconds(0);
}

void ScheduleAutoSaveForStorePurchase()
{
	if (*sgOptions.Oracool.autoSaveOnStorePurchase)
		ScheduleAfterSeconds(0);
}

void ScheduleAutoSaveForLevelChange()
{
	if (*sgOptions.Oracool.autoSaveOnLevelChange)
		ScheduleAfterSeconds(0);
}

void ScheduleAutoSaveForExperienceGain()
{
	if (*sgOptions.Oracool.autoSaveOnExperienceGain)
		ScheduleAfterSeconds(0);
}

void ScheduleAutoSaveForStatPointSpent()
{
	if (*sgOptions.Oracool.autoSaveOnStatPointSpent)
		ScheduleAfterSeconds(0);
}

void ScheduleAutoSaveForEquipmentChange()
{
	if (*sgOptions.Oracool.autoSaveOnEquipmentChange)
		ScheduleAfterSeconds(0);
}

void ScheduleAutoSaveForItemDrop()
{
	if (*sgOptions.Oracool.autoSaveOnItemDrop)
		ScheduleAfterSeconds(0);
}

void ScheduleAutoSaveForStashChange()
{
	if (*sgOptions.Oracool.autoSaveOnStashChange)
		ScheduleAfterSeconds(0);
}

void ScheduleAutoSaveForStoreTransaction()
{
	if (*sgOptions.Oracool.autoSaveOnStoreTransaction)
		ScheduleAfterSeconds(0);
}

void ScheduleAutoSaveForShrineActivation()
{
	if (*sgOptions.Oracool.autoSaveOnShrineActivation)
		ScheduleAfterSeconds(0);
}

void ScheduleAutoSaveForBookRead()
{
	if (*sgOptions.Oracool.autoSaveOnBookRead)
		ScheduleAfterSeconds(0);
}

void ScheduleAutoSaveForItemBreak()
{
	if (*sgOptions.Oracool.autoSaveOnItemBreak)
		ScheduleAfterSeconds(0);
}

void ScheduleAutoSaveForWaypointActivation()
{
	if (*sgOptions.Oracool.autoSaveOnWaypointActivation)
		ScheduleAfterSeconds(0);
}

void SaveOnExit()
{
	if (!SavingIsPossible() || !gbRunGame || MyPlayer == nullptr)
		return;
	if (demo::IsRunning() || demo::IsRecording())
		return;

	// Deliberately does NOT go through IsSafeToSave(). That predicate requires !gmenu_is_active(),
	// and by the time the player has chosen "Main Menu" or "Exit Game" the menu is by definition
	// open - reusing it here would make this function a silent no-op, which is exactly the failure
	// it exists to fix. Its other conditions (no store open, cursor not holding an item, not
	// paused) guard against saving a *mid-interaction* state that play would continue from; here
	// the game is ending and only the character is kept, so none of them apply.

	Player &player = *MyPlayer;

	// Saving a dead character does not damage the file - UnPackPlayer floors _pHPBase at 64
	// (1 HP) rather than loading a corpse. But coming back on 1 HP is a miserable restart for no
	// design reason, so health is restored here instead. Death still costs what death costs; it
	// just does not additionally strand you at one hit point.
	//
	// This ordering matters: it must happen before the save, and the whole call must happen before
	// GamemenuNewGame() sets _pmode = PM_QUIT.
	if (MyPlayerIsDead || player._pmode == PM_DEATH) {
		player._pHPBase = player._pMaxHPBase;
		player._pHitPoints = player._pMaxHP;
	}

	// Character progress only, rather than SaveGame(). SaveGame() is pfile_write_hero(true) plus
	// sfile_write_stash(); the `true` adds the world snapshot - current dungeon level and position,
	// monsters, objects, ground items, in-progress quests, portals. In V1 single-player none of
	// that is ever read back, because LoadGame() is unreachable: every character always starts a
	// fresh dungeon (see the Character-Only Persistence work). Everything durable travels with the
	// hero regardless - stats, inventory, belt, equipment, gold, the waypoint unlock table (packed
	// into PlayerPack) and the extra inventory tabs (their own sub-file, written alongside
	// SaveHeroItems). The stash is a separate file and still needs its own write.
	// An item on the cursor belongs to NOBODY until it is put down. Audit finding, 2026-08-26:
	// PlayerPack does not carry HoldItem and neither does SaveHeroItems - only SaveGame's world
	// snapshot does, and this path deliberately does not write one. So picking an item up, closing
	// the inventory (which leaves it held) and exiting destroyed it, silently, at the moment of
	// saving.
	//
	// Put down rather than persisted: the item belongs in a container, and adding it to the hero
	// format would mean a character who loads with something stuck to the cursor.
	ReturnHeldItemBeforeSaving(player);

	BeginSaveAttempt();
	pfile_write_hero(/*writeGameData=*/false);
	sfile_write_stash();
	if (SaveAttemptFailed()) {
		// Said out loud rather than logged as success. MpqWriter::WriteFile keeps the previous
		// record when a write fails, so what is on disk is the last good save - which is worth
		// knowing before quitting on top of it.
		LogEvent(fmt::format(fmt::runtime(_("SAVE FAILED - \"{:s}\" could not be written. "
		                                    "Your last successful save is intact.")),
		             FailedSaveFileName()),
		    UiFlags::ColorRed);
		return;
	}
	NotifyGameSaved();
	LogEvent("Game saved (exit)", UiFlags::ColorWhite);
}

void ProcessAutoSave()
{
	if (!IsEnabled()) {
		SavePending = false;
		return;
	}

	const auto now = Clock::now();
	const int intervalMinutes = std::max(*sgOptions.Oracool.autoSaveIntervalMinutes, 1);
	const bool intervalElapsed = now - LastSave >= std::chrono::minutes(intervalMinutes);
	const bool pendingIsDue = SavePending && now >= PendingSaveTime;
	if ((!intervalElapsed && !pendingIsDue) || !IsSafeToSave())
		return;

	SaveGame();
	LogEvent("Game saved (auto)", UiFlags::ColorWhite);
	if (*sgOptions.Oracool.autoSaveNotification)
		TriggerSaveIndicator();
}

} // namespace devilution::oracool
