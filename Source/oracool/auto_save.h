#pragma once

namespace devilution::oracool {

void ResetAutoSave();
void ProcessAutoSave();
void NotifyGameSaved();
void ScheduleAutoSaveForItemPickup();
void ScheduleAutoSaveForStorePurchase();
void ScheduleAutoSaveForLevelChange();
void ScheduleAutoSaveForExperienceGain();
void ScheduleAutoSaveForStatPointSpent();
void ScheduleAutoSaveForEquipmentChange();
void ScheduleAutoSaveForItemDrop();
void ScheduleAutoSaveForStashChange();
void ScheduleAutoSaveForStoreTransaction();
void ScheduleAutoSaveForShrineActivation();
void ScheduleAutoSaveForBookRead();
void ScheduleAutoSaveForItemBreak();
void ScheduleAutoSaveForWaypointActivation();

/**
 * @brief Saves immediately when the player leaves the game via "Main Menu" or "Exit Game".
 *
 * Single-player never saved on the way out: diablo.cpp's exit-path pfile_write_hero() is wrapped
 * in `if (gbIsMultiplayer)`. Worse, the scheduled autosave cannot cover the gap either, because
 * IsSafeToSave() requires !gmenu_is_active() - so opening the menu to leave actively *blocks* the
 * save your last actions had just earned, and then the game loop exits and it is gone.
 *
 * Must be called before GamemenuNewGame() runs, which sets every player's _pmode to PM_QUIT and
 * clears MyPlayerIsDead - saving after that would persist a quitting player.
 */
void SaveOnExit();

} // namespace devilution::oracool
