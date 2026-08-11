#include "oracool/auto_save.h"

#include <algorithm>
#include <chrono>

#include "cursor.h"
#include "diablo.h"
#include "engine/demomode.h"
#include "gmenu.h"
#include "loadsave.h"
#include "minitext.h"
#include "multi.h"
#include "options.h"
#include "oracool/event_log.h"
#include "oracool/save_indicator.h"
#include "pfile.h"
#include "player.h"
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
	if (!IsEnabled() || !gbRunGame || MyPlayer == nullptr)
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
	pfile_write_hero(/*writeGameData=*/false);
	sfile_write_stash();
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
