---
title: 2026-08-12 - Deleting the Monster Lore Functions
date: 2026-08-12
tags: [dev-report]
summary: PrintMonstHistory and PrintUniqueHistory lost their only caller when monster hovers stopped feeding the cursor panel. Both are gone, along with monster.cpp's control.h include - and with them a Torment fix that turns out to have been documented as a feature.
---

# Deleting the Monster Lore Functions

Follow-up cleanup to commit `300bdb3`, which removed the dungeon-monster branch of `UpdateInfoString` because the health bar across the top of the screen already names the hovered monster. That branch was the only caller of `monster.cpp`'s `PrintMonstHistory` and `PrintUniqueHistory`.

## What was checked before deleting

Grepped the whole tree, not just the obvious callers - including `plrctrls.cpp` and the controller hover paths, which have their own targeting logic and could plausibly have populated the panel separately. They do not. The only remaining references anywhere were two comments and two vault documents.

The three globals these functions touched all stay, because each has other users:

| Symbol | Also used by |
|---|---|
| `GetMonsterTypeText` | `qol/monhealthbar.cpp` |
| `sgOptions.Gameplay.showMonsterType` | `qol/monhealthbar.cpp` |
| `MonsterKillCounts` | `loadsave.cpp` (**save format**), `monhealthbar.cpp`, and the kill counter in `MonsterDeath` |

`MonsterKillCounts` is worth calling out: it is written to and read from the save file, so touching it would have been save-breaking for no benefit. Nothing about it was touched.

## One include did become dead

After the deletion, `monster.cpp` uses nothing from `control.h` - `AddPanelString` was its only reason to include it. Removed. `cursor.h` stays (`pcursmonst` is still cleared in `DeleteMonster`), and `utils/str_cat.hpp` stays (`StrCat` and `BufCopy` are used elsewhere in the file) even though `AppendStrView` went with the resistance-list builders.

## Something was lost that had been documented as a feature

`PrintMonstHistory` was not vanilla code untouched. It carried an Oracool fix: when Torment difficulty was added, its HP-range display had the same Normal/Nightmare/Hell-only branches as the real stat formulas, so the panel would have shown *Hell's* numbers while the player was actually on Torment. That was found, fixed, and written up in `05-Gameplay-and-Features/Gameplay-Changes.md`.

Deleting the function deletes that branch. This is still the right call - the code was unreachable, and unreachable code that looks maintained is worse than no code - but the vault entry has been updated rather than left describing behaviour that no longer exists, and it now records where the formula came from (`InitMonster`'s) in case a bestiary is ever built as a screen of its own.

The comment left at the old call site in `control.cpp` says the same thing in one line: restoring monster hover text means writing these again, not just re-adding a call.

## Verification

Debug build clean at `ORACOOL_VERSION` **1.1.9**, version string confirmed in the exe. Tests **347/349** - `Drlg_l1.CreateL5Dungeon_diablo_3_844660068` and `Timedemo.WarriorLevel1to2` only, both pre-existing and unrelated.

Nothing here changes runtime behaviour, so there is nothing to play-test.

## Related

- [[2026-08-12 - One Item Panel, Quieter Hovers, Waypoint Behind Everything]]
