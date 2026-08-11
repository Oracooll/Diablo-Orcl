---
title: 2026-08-09 - Remove Auto Save Item Delay
date: 2026-08-09
tags: [dev-report]
summary: Removed the V0-era "Auto Save Item Delay Seconds" option; item pickup and store purchase now schedule an immediate save like every other trigger.
---

# Remove Auto Save Item Delay

## Context

While reviewing why [[2026-08-09 - Autosave-Only Play, Part 1]]'s `IsSafeToSave()` gates exist, the user asked whether a "time gate preventing more than 1 save in a few seconds" had been removed. Investigation showed no such floor exists — `SaveGame()` calls `NotifyGameSaved()` on every save, which resets the periodic-interval clock unconditionally, so nothing throttles how often qualifying events can trigger a save.

The one delay-related mechanism that *did* exist was `Auto Save Item Delay Seconds`: a per-trigger debounce, inherited from the V0 prototype (see [[Feature-Catalogue]], prototype v0.21), that pushed item-pickup and store-purchase saves back by a configurable number of seconds so rapid acquisitions would coalesce into one save. The user judged this a V0-era holdover that V1 doesn't need and asked for it to be removed.

## What changed

- [Source/options.h](../../../Source/options.h) — removed the `autoSaveItemDelaySeconds` field from `OracoolOptions`.
- [Source/options.cpp](../../../Source/options.cpp) — removed its constructor initializer, its `GetEntries()` list entry, and its `.ini` persistence line/comment.
- [Source/engine/demomode.cpp](../../../Source/engine/demomode.cpp) — removed the corresponding reset-to-default line in the demo-mode option override block.
- [Source/oracool/auto_save.cpp](../../../Source/oracool/auto_save.cpp) — `ScheduleAutoSaveForItemPickup()` and `ScheduleAutoSaveForStorePurchase()` now call `ScheduleAfterSeconds(0)` directly, matching `ScheduleAutoSaveForLevelChange/ExperienceGain/StatPointSpent/EquipmentChange`, which were already instant.
- Bumped `ORACOOL_VERSION` to `1.0.33` and rebuilt the Debug config clean (see [[Version Bump on Rebuild Policy]]).

## Why

With this option gone, every auto-save trigger now behaves identically: schedule an immediate save on the next safe tick. This matches the user's read that per-trigger debouncing was V0-specific scope-management that V1's simpler always-on trigger set doesn't need. It also removes a second, unrelated meaning of "delay" that was easy to confuse with the interval/safety-gate question that prompted this investigation.

## Verification

Debug build (`build\x64-Debug`, config Debug) completed with no errors — `DiabloOrcl.exe` and the full test suite (217/217 targets) linked successfully. Not yet manually tested in-game by the user.

## Not done / deliberately left alone

- The `Feature-Catalogue.md` v0.21 table still lists `Auto Save Item Delay Seconds=3` — left untouched, since that table is explicitly documented as "the historical 1.5.4 prototype configuration," not a living spec of V1's current behavior.
- No change was made to `IsSafeToSave()` or the periodic-interval logic; that mechanism was already confirmed correct during this investigation, not implicated in the delay option's removal.

## Related

- [[2026-08-09 - Autosave-Only Play, Part 1]]
- [[Feature-Catalogue]]
