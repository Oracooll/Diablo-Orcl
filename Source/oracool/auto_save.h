#pragma once

namespace devilution::oracool {

void ResetAutoSave();
void ProcessAutoSave();
void NotifyGameSaved();
void ScheduleAutoSaveForItemPickup();
void ScheduleAutoSaveForStorePurchase();
void ScheduleAutoSaveForLevelChange();

} // namespace devilution::oracool
