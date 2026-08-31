---
title: 2026-08-09 - Autosave Cheat Command and Wider Event Coverage
date: 2026-08-09
tags: [dev-report]
summary: Fixed the givexp debug command bypassing the experience-gain autosave hook, then extended trigger coverage to store sell/repair/recharge/identify, shrine and fountain activation, book reading, and item breaking - matching the user's Diablo 3-style "save on any significant or insignificant event" goal.
---

# Autosave Cheat Command and Wider Event Coverage

## Context

Continuing from [[2026-08-09 - Autosave Trigger Coverage Gaps]]: that report couldn't explain why gaining a level didn't autosave, since `AddPlrExperience()` already had the hook. The user clarified they'd tested with the `givexp` debug console command, not by actually killing monsters - which pointed straight at the real cause.

The user then asked for ten more specific events to be checked: drop gold, pick up gold, transfer gold to Stash, draw gold from Stash, buy item, sell item, repair item, durability change in an item, read book, activate fountain or shrine.

## What changed

**`givexp` fix**: `debug.cpp`'s `DebugCmdLevelUp()` sends `CMD_CHEAT_EXPERIENCE`, handled by `OnCheatExperience()` in [Source/msg.cpp:2298](../../../Source/msg.cpp:2298). That handler sets `_pExperience`/`_pNextExper` and calls `NextPlrLevel()` **directly**, bypassing `AddPlrExperience()` entirely - so the experience-gain autosave hook never ran for it. Added `oracool::ScheduleAutoSaveForExperienceGain()` right after `NextPlrLevel()`, guarded on `pnum == MyPlayerId`.

**Ten-event audit** - four were already covered by prior work and needed no change:
- Drop gold, pick up gold - both route through the already-hooked `TryDropItem()` / `InvGetItem()`/`AutoGetItem()` paths (type-agnostic, gold included).
- Transfer gold to/from Stash - covered by [[2026-08-09 - Autosave Trigger Coverage Gaps]]'s Stash sweep (`CheckStashPaste`, `WithdrawGold`).
- Buy item - already covered via the shared `StoreAutoPlace()` helper used by every shop's buy path (Smith, Premium, Unique, Witch, Boy, Healer, consumables).

Six were genuinely uncovered and got new hooks, each behind its own toggle for consistency with the existing architecture:

- **Sell / repair / recharge / identify** - new `Auto Save on Store Transaction` option, new `ScheduleAutoSaveForStoreTransaction()`. Hooked individually at each mutation point rather than the shared `ConfirmEnter()` dispatcher, so this toggle stays independent from `Auto Save on Store Purchase`: [Source/stores.cpp](../../../Source/stores.cpp) `StoreSellItem()`, `SmithRepairItem()` (both return paths), `WitchRechargeItem()`, `StorytellerIdentifyItem()`.
- **Shrine / Goat Shrine / Cauldron / Fountain activation** - new `Auto Save on Shrine Activation` option, new `ScheduleAutoSaveForShrineActivation()`. Hooked once in `OperateShrine()` ([Source/objects.cpp:3400](../../../Source/objects.cpp:3400)) - covers Goat Shrine and Cauldron for free since both call through `OperateShrine()` internally - and once in `OperateFountains()` (guarded on its existing `applied` flag so a fountain that did nothing, e.g. Blood Fountain at full HP, doesn't trigger a save).
- **Read book** - new `Auto Save on Book Read` option, new `ScheduleAutoSaveForBookRead()`. Hooked in the shared `IMISC_BOOK` case of `UseItem()` ([Source/items.cpp:4822](../../../Source/items.cpp:4822)) - the single entry point every book-reading surface (inventory, belt, Stash) already funnels through.
- **Item break** - new `Auto Save on Item Break` option, new `ScheduleAutoSaveForItemBreak()`. Hooked in `BreakOrRemoveEquipment()` ([Source/inv.cpp:1511](../../../Source/inv.cpp:1511)) - the single choke point every durability-reaches-zero path already funnels through, regardless of source (melee, armor damage, trap).

**Durability loss itself** (as opposed to an item breaking) was deliberately *not* hooked - see Why below.

Bumped `ORACOOL_VERSION` to `1.0.37` and rebuilt the Debug config clean after each round.

## Why

Ordinary durability loss happens on nearly every successful melee hit (`DamageWeapon()`), so hooking it directly would mean a full save on almost every swing in combat - reintroducing, from a different angle, exactly the spam problem the just-removed `Auto Save Item Delay Seconds` (see [[2026-08-09 - Remove Auto Save Item Delay]]) used to paper over. Asked the user directly rather than guessing; they chose "only when an item breaks" - a rare, discrete, genuinely significant event (the item becomes disabled) versus a per-hit counter that isn't.

Store sell/repair/recharge/identify were hooked individually instead of at the shared `ConfirmEnter()` dispatcher specifically so `Auto Save on Store Transaction` and `Auto Save on Store Purchase` stay orthogonal - a user who wants save-on-buy but not save-on-sell (or vice versa) can still get that; a single dispatcher-level hook would have coupled them.

## Verification

Debug build completed with no errors after each change (111/111 targets). Not yet manually retested in-game by the user for any of these six new triggers or the `givexp` fix.

## Not done / deliberately left alone

- Durability loss (as distinct from item breaking) - explicitly excluded per the user's choice.
- No audit beyond the ten events the user listed plus the four already-covered ones confirmed in passing.

## Related

- [[2026-08-09 - Autosave Trigger Coverage Gaps]]
- [[2026-08-09 - Remove Auto Save Item Delay]]
- [[2026-08-09 - Autosave-Only Play, Part 1]]
