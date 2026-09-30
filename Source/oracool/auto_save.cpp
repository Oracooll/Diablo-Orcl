#include "oracool/auto_save.h"
#include "oracool/readied_spells.h"

#include <string>
#include <algorithm>
#include <chrono>
#include <fmt/format.h>

#include "cursor.h"
#include "diablo.h"
#include "error.h" // InitDiabloMsg
#include "engine/demomode.h"
#include "gmenu.h"
#include "inv.h"
#include "loadsave.h"
#include "minitext.h"
#include "multi.h"
#include "options.h"
#include "oracool/save_status.h"
#include "oracool/event_log.h"
#include "oracool/levski_roar.h"
#include "oracool/workshop.h"
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
/**
 * @brief No save may be attempted before this, however many requests come in.
 *
 * Separate from PendingSaveTime because they answer different questions. PendingSaveTime is "the
 * player did something worth saving" and every trigger is entitled to move it to now.
 * RetryNotBefore is "the disk said no, wait" and no trigger may move it at all.
 *
 * Audit finding, 2026-08-26: with only PendingSaveTime, the two meanings shared one variable and
 * the wrong one won. A failing save scheduled a retry sixty seconds out, the player picked up an
 * item, and the pickup trigger overwrote the deadline with now - so the backoff that exists to
 * stop a full disk being hammered every frame was defeated by ordinary play.
 */
Clock::time_point RetryNotBefore;
/** @brief Consecutive failed autosaves, driving the retry backoff and the once-per-run message. */
int FailedSaveAttempts = 0;
constexpr int BaseRetrySeconds = 5;
constexpr int MaxRetrySeconds = 300;

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
	    // Levski's grid is an unsaved container (audit, 2026-09-19): a hero written while items sit
	    // in it would lose them to a crash before the window closes and returns them.
	    && !IsLevskiRoarOpen()
	    // The workshop's bench and craft grid likewise (external audit of v1.12.188, SAVE-01).
	    && !IsWorkshopOpen()
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
	// The belt takes potions only, the rule for every automatic placement (user, 2026-09-14; audit, 2026-09-29).
	if (!placed && player.HoldItem.isPotion())
		placed = AutoPlaceItemInBelt(player, player.HoldItem, /*persistItem=*/true);
	// The stash is tried whether or not it is OPEN. Audit finding, 2026-08-26, and the open case is
	// the one that matters most: lift an item OUT of the stash with a full inventory and belt, then
	// exit. The slot it came from is now vacant and is the obvious place for it - and the old
	// `!IsStashOpen` guard skipped exactly that, saved the removal, and dropped the item.
	//
	// The guard was there because writing to the stash under an open stash window looks unsafe. It
	// is not, here: this runs on the way out, after the last frame, so nothing will redraw from the
	// state being changed.
	if (!placed)
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
	// Never EARLIER than an outstanding retry deadline. A trigger may bring a save forward, but it
	// does not get to overrule a disk that has just refused one.
	PendingSaveTime = std::max(Clock::now() + std::chrono::seconds(std::max(seconds, 0)), RetryNotBefore);
}

/**
 * @brief The frequent triggers - every kill's experience, every pickup - save at most once in @p spacingSeconds (audit,
 * 2026-09-27). Each asked for a save at once, and a hero save rewrites and flushes the whole hero archive (and the stash
 * when gold went there) on the main thread: a hitch on nearly every kill at 300% density. Progress is still at most a
 * few seconds old, and a level change, a purchase or leaving the game saves at once as before. A save already due
 * sooner is kept.
 */
void ScheduleSpaced(int spacingSeconds)
{
	if (!IsEnabled())
		return;
	const auto when = std::max({ Clock::now(), LastSave + std::chrono::seconds(spacingSeconds), RetryNotBefore });
	if (!SavePending || when < PendingSaveTime)
		PendingSaveTime = when;
	SavePending = true;
}

constexpr int FrequentTriggerSpacingSeconds = 10;

} // namespace

void ResetAutoSave()
{
	LastSave = Clock::now();
	SavePending = false;
	// Cleared here too (audit, 2026-08-26). This is both "a save just succeeded" and "a new session
	// is starting", and a leftover failure count means either one inherits the previous run's
	// backoff rung - so the next failure waits minutes instead of five seconds and, because the
	// message is only printed on the FIRST failure of a run, says nothing at all while it does.
	FailedSaveAttempts = 0;
	RetryNotBefore = Clock::now();
}

void NotifyGameSaved()
{
	ResetAutoSave();
}

void ScheduleAutoSaveForItemPickup()
{
	if (*sgOptions.Oracool.autoSaveOnItemPickup)
		ScheduleSpaced(FrequentTriggerSpacingSeconds);
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
		ScheduleSpaced(FrequentTriggerSpacingSeconds);
}

void ScheduleAutoSaveForStatPointSpent()
{
	if (*sgOptions.Oracool.autoSaveOnStatPointSpent)
		ScheduleAfterSeconds(0);
}

void ScheduleAutoSaveForSkillChange()
{
	// Audit finding, 2026-08-26: spending a skill point, slotting a passive, lighting an aura
	// and readying a skill were the only character changes with no trigger at all, so a crash
	// threw away a build the player had just spent minutes arranging.
	if (*sgOptions.Oracool.autoSaveOnSkillChange)
		ScheduleAfterSeconds(0);
	// The two mouse buttons are remembered for the NEXT character here rather than at each of the
	// half-dozen places that can change them (user, 2026-08-30: "remember what skills/spells have
	// been assigned to lmb/rmb and load them automatically on next new game"). Every one of those
	// places already calls this - that is what makes it the one hook that cannot be forgotten by
	// the next assignment path someone adds.
	//
	// Unconditional, unlike the save above: this is a preference, not a save, and a player with
	// autosave-on-skill-change turned off has not asked to stop being remembered.
	if (MyPlayer != nullptr)
		RememberReadiedSpells(*MyPlayer);
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

	// The Cube's grid and the workshop's bench and well hand their items back first (audit, 2026-09-27): they are not
	// save state, and FreeGame clears them after. The game menu closed them before calling this, but closing the window
	// (Alt+F4) comes straight here - and lost whatever was staged. Closing twice is harmless.
	CloseLevskiRoar();
	CloseWorkshop();

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
	SaveHeroAndStash(/*writeGameData=*/false);
	if (SaveAttemptFailed()) {
		// Said out loud rather than logged as success. MpqWriter::WriteFile keeps the previous
		// record when a write fails, so what is on disk is the last good save - which is worth
		// knowing before quitting on top of it.
		const std::string failed = fmt::format(fmt::runtime(_("SAVE FAILED - \"{:s}\" could not be written. "
		                                                      "Your last successful save is intact.")),
		    FailedSaveFileName());
		LogEvent(failed, UiFlags::ColorRed);
		InitDiabloMsg(failed); // on screen too: with the Event Log off the log was its only channel (round 11 audit)
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
	// The backoff gate, and it sits after the due checks deliberately so it covers BOTH of them.
	// The periodic interval is not a trigger the player controls, but it would otherwise walk
	// through a retry deadline just as readily as a pickup did.
	if (now < RetryNotBefore)
		return;

	// Character only, exactly as SaveOnExit writes it - and for exactly the same reason, which had
	// simply never been carried across to this function (audit, 2026-08-26).
	//
	// SaveGame() is pfile_write_hero(true) plus sfile_write_stash(); the `true` adds a world
	// snapshot - the whole dungeon level, its monsters, objects, ground items, portals. In V1
	// single-player NONE of that is ever read back, because LoadGame() is unreachable: every
	// character always starts a fresh dungeon. So the periodic save was serialising a level and
	// writing it to disk every few minutes to produce bytes nothing will ever open, which is a
	// frame hitch and write amplification bought for nothing.
	//
	// Everything durable still travels with the hero: stats, inventory, belt, equipment, gold, the
	// waypoint table, the extra inventory tabs and the chunk tail. The stash is its own file and
	// still needs its own write.
	BeginSaveAttempt();
	SaveHeroAndStash(/*writeGameData=*/false);
	if (SaveAttemptFailed()) {
		// BACKED OFF, not retried immediately. Audit finding, 2026-08-26, and it was my own doing:
		// the failure branch returned without touching LastSave or SavePending, and ProcessAutoSave
		// runs once per game-loop iteration - so a disk that stays full meant a full save attempt
		// and a red log line EVERY FRAME. The reporting turned a bad situation into an unplayable
		// one.
		//
		// The pending request is KEPT: the save still needs to happen, and giving up on it silently
		// is how the fix becomes a second bug. What changes is when to try again.
		FailedSaveAttempts++;
		const int backoffSeconds = std::min(BaseRetrySeconds << std::min(FailedSaveAttempts - 1, 6),
		    MaxRetrySeconds);
		LastSave = Clock::now();
		SavePending = true;
		RetryNotBefore = Clock::now() + std::chrono::seconds(backoffSeconds);
		PendingSaveTime = RetryNotBefore;

		// Said ONCE per run of failures rather than once per attempt. A player needs to know the
		// game cannot save; they do not need to be told sixty times a second, and a log that
		// scrolls itself is a log nobody reads.
		if (FailedSaveAttempts == 1) {
			const std::string failed = fmt::format(fmt::runtime(_("AUTO SAVE FAILED - \"{:s}\" could not be written. "
			                                                      "Your last successful save is intact.")),
			    FailedSaveFileName());
			LogEvent(failed, UiFlags::ColorRed);
			InitDiabloMsg(failed); // on screen too (round 11 audit)
		}
		return;
	}
	// A success ends the run of failures, so the next one is reported again.
	FailedSaveAttempts = 0;
	// What SaveGame() used to do for us on the way out: reset the interval and clear any pending
	// request, so the next save is timed from this one. Through NotifyGameSaved rather than by
	// touching LastSave and SavePending here, because that is the one function that owns them.
	gbValidSaveFile = true;
	NotifyGameSaved();
	LogEvent("Game saved (auto)", UiFlags::ColorWhite);
	if (*sgOptions.Oracool.autoSaveNotification)
		TriggerSaveIndicator();
}

} // namespace devilution::oracool
