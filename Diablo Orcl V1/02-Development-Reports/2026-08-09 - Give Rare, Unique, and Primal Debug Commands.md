---
title: 2026-08-09 - Give Rare, Unique, and Primal Debug Commands
date: 2026-08-09
tags: [dev-report]
summary: Added giverare/giveunique/giveprimal debug console commands that force-generate an item of the requested Oracool tier, instead of leaving it to the normal drop-chance roll.
---

# Give Rare, Unique, and Primal Debug Commands

## Context

The user asked to add three previously-discussed debug commands for testing the Oracool item tier system (Rare / Buffed Unique / Primal - see the tier definitions in [Source/items.h:73](../../../Source/items.h:73)): `giverare`, `giveunique` (Buffed Unique), and `giveprimal`. Vanilla `dropu` already covers genuine vanilla Uniques, so "Give Unique" here maps to the Oracool `BuffedUnique` tier, not a duplicate of `dropu`.

## What changed

- **`SetupAllItems()`** ([Source/items.cpp:1665](../../../Source/items.cpp:1665)) gained an optional `std::optional<OracoolItemTier> forcedTier = std::nullopt` parameter. When set, it skips the normal `CheckUnique`/probabilistic-tier fork entirely and calls the requested tier's `GetRareItemAffixes`/`GetBuffedUniqueItemAffixes`/`GetPrimalItemAffixes` directly (with `ignoreLevelLimits=true`, since this is a cheat/debug path, not a natural drop) - falling back to a plain magic roll if the picked base item type can't carry tiered affixes at all (e.g. a potion). Every existing call site is unaffected: the parameter defaults to `std::nullopt`, and the original probabilistic branch is untouched, just moved into an `else if`.
- **`DebugSpawnTieredItem(std::string itemName, OracoolItemTier tier)`** ([Source/items.cpp](../../../Source/items.cpp), declared in [Source/items.h:810](../../../Source/items.h:810)) - new function mirroring `DebugSpawnItem`'s random-base-item search loop, but calling `SetupAllItems` with the forced tier and retrying whenever the roll doesn't stick (tier-ineligible item type, or the optional name filter doesn't match).
- **`debug.cpp`**: three new commands, `giverare`, `giveunique`, `giveprimal`, each taking an optional `{name}` substring filter exactly like `drop`/`dropu` already do.

Bumped `ORACOOL_VERSION` to `1.0.39` and rebuilt the Debug config clean.

## Why

Forcing the tier *inside* `SetupAllItems` (rather than rolling a normal item via the existing `DebugSpawnItem` and patching tier affixes onto it afterward) was a deliberate choice: the normal generation order is roll-tier → `ItemRndDur` → the Primal-specific full-durability override → `SetupItem` (which finalizes the display name, price, and other derived fields). Applying tier affixes after that sequence already ran would leave those derived fields stale. Reusing the same function guarantees a forced-tier debug item is assembled in exactly the same order as a naturally-rolled one.

## Verification

Debug build completed with no errors (176 targets, `DiabloOrcl.exe` linked). Not yet manually tested in-game by the user.

## Related

- [[2026-08-09 - Autosave on Waypoint Activation]]
