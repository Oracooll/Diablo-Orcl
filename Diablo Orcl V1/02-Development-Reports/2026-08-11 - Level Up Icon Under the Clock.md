---
title: 2026-08-11 - Level Up Icon Under the Clock
date: 2026-08-11
tags: [dev-report]
summary: The level-up indicator grows from 32x32 to 40x60 and hangs centred under the clock's colon rather than under the clock's box, with the "Level Up" caption dropped. Also fixes a pre-existing inv_test that only passed when another test ran first.
---

# Level Up Icon Under the Clock

## What changed

The user asked for the level-up icon at "around 40x60 pixels aligned vertically center under the `:` of the clock", and gave permission to drop the caption. All three landed.

`LevelUpIconSize` in `oracool/hud_layout.h` goes from `{ 32, 32 }` to `{ 40, 60 }`. At that size the emblem carries its own meaning, so `DrawLevelUpIcon` (`control.cpp`) no longer prints the gold "Level Up" string beneath it - which also removes the only thing forcing the indicator to reserve horizontal space wider than itself.

## Why the colon, not the box

The obvious anchor is the clock's rect, and it is the wrong one. `DrawGameClock` draws **left-aligned** inside a fixed 72px box sized for the longest form ("12:45 PM"), so the text's own centre slides as the hour changes width and again when the 12/24-hour option is toggled. Centring the icon on the box would leave it visibly off-centre under the digits for most of the day.

The colon is the stable landmark, so `game_clock.cpp` gained:

```cpp
int GetClockColonCentreX();
```

which formats the clock string exactly as the draw call does, finds the `:`, and measures the real glyph runs with `GetLineWidth(..., GameFont12, 1)` - the same font and spacing the clock renders with, so it tracks actual metrics rather than an estimate. `GetLevelUpIconRect()` then subtracts half the icon width from that.

`GetLevelUpIconRect()` lives in `hud_layout.cpp`, not `game_clock.cpp`, because `control.cpp` needs the rect for hit-testing (`CheckLvlBtn`, `ReleaseLvlBtn`) as well as for drawing. One function, so the two can never disagree.

## Re-cutting the asset

`tools/HudIconCut.cs`'s `CutLevelUp` previously assumed a square cell. Cutting a 40x60 cell out of a square source by stretching would have distorted the emblem, so the cutter now takes `(cellW, cellH)`, computes one shared source box across all three states (so the states don't drift relative to each other), and fits it into the cell with aspect preserved - a "contain" fit, letterboxed. Output `level_up_icon.png` is 120x60, three states resting/hover/pressed, mirrored into both `build/x64-Debug/assets/ui` and `Packaging/resources/oracool_assets/ui`, and repacked into `oracool.mpq` (11 files, 541,394 bytes).

## Unrelated fix: an inv_test that needed a neighbour

`InvTest.RemoveMatchingInventoryOrExtraTabItem_FindsItemInInvList` crashed with an access violation whenever ctest ran it - but passed when the whole `inv_test.exe` ran unfiltered. ctest runs each case under its own `--gtest_filter`, which is what exposed it.

The InvList branch ends in `Player::RemoveInvItem`, which network-syncs the removal via `CMD_DELINVITEMS` when the owner is `MyPlayer`. With no provider initialised that dereferences a null connection. It only ever passed because an earlier test in the same binary had already called `SNetInitializeProvider`. The extra-tab sibling test needs no provider (extra tabs are single-player, no packet format), which is why only one of the pair failed. Fixed by initialising the loopback provider in the test itself, matching the pattern the other tests in the file already use.

## Verification

Debug build clean at `ORACOOL_VERSION` **1.1.2**, version string confirmed present in `DiabloOrcl.exe`. Test suite **347/349**:

- `Drlg_l1.CreateL5Dungeon_diablo_3_844660068` - pre-existing, dungeon stairs land at (67,72) instead of (67,52); suspected fallout from the waypoint work in `objects.cpp`. Not touched by this change.
- `Timedemo.WarriorLevel1to2` - fails with "Unable to load character". Its reference demo save predates the accepted save-breaking inventory change (40 to 70 grid cells), so it can no longer be loaded. The demo needs re-recording against the current save format.

Still needs a play-test: level up a character and confirm the icon's size and its alignment under the colon in both 24-hour and 12-hour clock modes.

## Related

- [[2026-08-11 - Burger Menu Icon Row]]
- [[2026-08-11 - Panel Darkening and HUD Recolour]]
