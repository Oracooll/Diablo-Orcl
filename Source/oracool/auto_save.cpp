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
#include "oracool/save_indicator.h"
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
		ScheduleAfterSeconds(*sgOptions.Oracool.autoSaveItemDelaySeconds);
}

void ScheduleAutoSaveForStorePurchase()
{
	if (*sgOptions.Oracool.autoSaveOnStorePurchase)
		ScheduleAfterSeconds(*sgOptions.Oracool.autoSaveItemDelaySeconds);
}

void ScheduleAutoSaveForLevelChange()
{
	if (*sgOptions.Oracool.autoSaveOnLevelChange)
		ScheduleAfterSeconds(0);
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
	if (*sgOptions.Oracool.autoSaveNotification)
		TriggerSaveIndicator();
}

} // namespace devilution::oracool
