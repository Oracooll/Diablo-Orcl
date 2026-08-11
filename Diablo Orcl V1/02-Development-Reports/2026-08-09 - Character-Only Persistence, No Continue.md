---
title: 2026-08-09 - Character-Only Persistence, No Continue
date: 2026-08-09
tags: [dev-report, gameplay, save-system]
summary: Removed the "Continue"/"New Game" choice from single-player character selection entirely — selecting an existing character now always goes straight to difficulty selection and starts a fresh dungeon, since the project's goal is building a character, not resuming a game session.
---

# Character-Only Persistence, No Continue

## Context

Immediately after [[2026-08-09 - Autosave-Only Play, Part 1]] shipped, the project owner redirected: the "Character Exists" / "Continue" dialog that work introduced still shouldn't exist at all, even relabeled. The reasoning is a philosophy statement, not a UI nitpick — V1's goal is building the strongest possible character through repeated play, not progressing toward finishing a single game session. Game-state save/load (which level you're on, mid-dungeon position) serves a "beat the game once" goal; this project explicitly doesn't have that goal, only "build the character." So every character selection should go straight to difficulty selection (Normal/Nightmare/Hell/Torment) and always start a fresh dungeon — never resume where a previous session left off.

## What changed

Investigated how "New Game" already behaved for an *existing* character before changing anything, since the fix turned out to already exist in the codebase, just needed to become the only path:

- `SelheroListSelect()` in `Source/DiabloUI/hero/selhero.cpp` calls `SelheroLoadSelect(1)` when a character has never been saved before ("New Game" implicitly). `SelheroLoadSelect(1)`, for single-player, already shows the difficulty picker (`selgame_GameSelection_Select(0)`) and sets `selhero_result = SELHERO_NEW_DUNGEON`.
- `Source/menu.cpp:59`: `StartGame(type != SELHERO_CONTINUE, ...)` — `SELHERO_NEW_DUNGEON` means `bNewGame = true`, which makes `StartGame()` call `InitLevels()`/`InitQuests()`/`InitPortals()` (fresh dungeon and quest state) — but the character itself (level, stats, inventory, gold) was already loaded from `selhero_heros[]` when the hero was picked from the list, independent of this choice. So "New Game" for an existing character was never a character wipe — it already meant "keep the character, start a fresh dungeon," exactly what was being asked for.

The only thing gating this behind a choice was the `if (selhero_heroInfo.hassaved) { ...dialog... }` block. Changed the condition to `if (selhero_isMultiPlayer && selhero_heroInfo.hassaved)` — single-player now always falls through past it straight to `SelheroLoadSelect(1)`. Multiplayer keeps the dialog: resuming a co-op session together is a real, distinct choice there (friends who left a dungeon mid-way want to pick up together), unlike single-player's character-building focus.

## Why this is safe

- **The character is unaffected**: it's loaded from the hero list before this branch is even reached, not from whatever path `SelheroLoadSelect` takes.
- **`LoadGame()`** (the function that restores an in-progress dungeon/quest state from disk, patched in the previous report to reset quests to fresh) becomes unreachable for single-player as a side effect — confirmed via `Source/interfac.cpp:338`, the only call site, which only fires on `WM_DIABLOADGAME`, which now only happens when `gbLoadGame` is true, which now only happens via multiplayer's still-intact Continue path. Not dead code — still load-bearing for multiplayer, and the quest-reset patch inside it remains correct and necessary there.
- Single-player instead always takes the `WM_DIABNEWGAME` → `LoadGameLevel(true, ENTRY_MAIN)` path — the same fresh-start path "New Game" already used, now just unconditional.

## Verification

- Bumped `ORACOOL_VERSION` to 1.0.4, rebuilt both `build/x64-Debug` and `build/x64-Release` — both compiled and linked cleanly (8/8 objects on the incremental Release build), no new warnings.
- Launched `DiabloOrcl.exe` (with the project owner's go-ahead after a 20-second warning, per the standing rule) and confirmed in-game: selecting the existing "Oracool" character goes directly to **Select Difficulty** (Normal/Nightmare/Hell/Torment) — no "Character Exists"/"Continue" screen appears at all. Choosing Normal starts a fresh town instance with the character's stats intact (70/70 HP, 10/10 mana, matching before).

## Not done / deliberately left alone

Multiplayer's Continue/New Game dialog (now showing "Continue"/"New Game" under "Character Exists", from the previous report's rename) was left completely functional and unchanged in behavior — only single-player's path to it was removed. The "all quests unlocked in the log immediately" item from the original 5-point spec is still deferred, per the project owner's explicit instruction to tackle it later.

## Related

- [[2026-08-09 - Autosave-Only Play, Part 1]]
- [[Idea-Backlog]]
