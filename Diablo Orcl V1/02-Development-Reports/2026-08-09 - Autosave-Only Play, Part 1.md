---
title: 2026-08-09 - Autosave-Only Play, Part 1
date: 2026-08-09
tags: [dev-report, gameplay, save-system]
summary: Made item/gold/XP/stat-point/equipment changes trigger an instant autosave, removed every manual Save/Load mention from the game's menus, and made quest progression session-only. Held back the riskiest part of the request (forcing every quest visible in the log immediately) pending confirmation.
---

# Autosave-Only Play, Part 1

## Context

Picked up [[Idea-Backlog]]'s "Autosave-only play" entry with a detailed 5-point specification from the project owner, modeled on Diablo 3's behavior: item pickups and experience gains persist the instant they happen, any character-state change should be saved immediately, every Save/Load mention should disappear from the game's menus, and quest progression should be session-only — always reset to fresh/unlocked when a character is loaded, never persisted.

## What changed

### Instant autosave triggers (requirements 1-3)

[`Source/oracool/auto_save.h`](../../Source/oracool/auto_save.h) / [`auto_save.cpp`](../../Source/oracool/auto_save.cpp) — added three new immediate (zero-delay) triggers, mirroring the existing `ScheduleAutoSaveForLevelChange()` pattern: `ScheduleAutoSaveForExperienceGain()`, `ScheduleAutoSaveForStatPointSpent()`, `ScheduleAutoSaveForEquipmentChange()`. Each is gated by its own new Oracool option (`autoSaveOnExperienceGain`, `autoSaveOnStatPointSpent`, `autoSaveOnEquipmentChange`, all defaulting to enabled), added to `Source/options.h`/`options.cpp` following the existing autosave-options pattern (in-game label/description plus a matching `diablo.ini` comment block).

Wired the new triggers in:
- [`Source/player.cpp`](../../Source/player.cpp) — `AddPlrExperience()` calls `ScheduleAutoSaveForExperienceGain()` whenever experience actually increases (covers level-ups too, since they happen inside the same function).
- [`Source/msg.cpp`](../../Source/msg.cpp) — `OnAddStrength`/`OnAddMagic`/`OnAddDexterity`/`OnAddVitality()` (the four command handlers stat-point-allocation UI routes through, even in single-player) each call `ScheduleAutoSaveForStatPointSpent()`.
- [`Source/inv.cpp`](../../Source/inv.cpp) — `ChangeEquipment()` and `RemoveEquipment()` call `ScheduleAutoSaveForEquipmentChange()`, covering both manual and auto-equip/unequip.

Also widened the existing item-pickup trigger to cover gold, which was previously excluded (`item._itype != ItemType::Gold` removed from both pickup code paths in `inv.cpp`) — gold changes are as much "character info" as any other pickup. Changed `Auto Save Item Delay Seconds`' default from 3 to 0, so item pickups and store purchases are instant by default too, while staying configurable if the project owner ever wants pickups debounced again.

### Removing every Save/Load mention (requirement 4)

Investigated every place "Save Game"/"Load Game" appeared in the UI before touching anything:

- [`Source/gamemenu.cpp`](../../Source/gamemenu.cpp)/`.h` — removed "Save Game" and "Load Game" from the pause menu (`sgSingleMenu`) and "Load Game" from its death variant (`sgSingleMenuOnDeath`), leaving Main Menu/Options/Exit Game (and Respawn In Town on the death variant). Deleted the now-fully-unused `gamemenu_save_game()`/`gamemenu_load_game()` wrapper functions entirely (confirmed zero remaining callers first), along with `GamemenuUpdateSingle()` (existed only to enable/disable the now-deleted Load Game entry — `gmenu_set_items()` already accepts a `nullptr` callback, confirmed by an existing call site). Cleaned up five includes (`error.h`, `loadsave.h`, `oracool/event_log.h`, `pfile.h`, `qol/floatingnumbers.h`) that were only used by the deleted functions.
- [`Source/diablo.cpp`](../../Source/diablo.cpp) — removed the `QuickSave`/`QuickLoad` Keymapper (F2/F3) and Padmapper bindings, which called the now-deleted wrapper functions.
- [`Source/DiabloUI/hero/selhero.cpp`](../../Source/DiabloUI/hero/selhero.cpp) — the "Save File Exists" dialog (shown when selecting a character with existing progress) renamed to "Character Exists", and its "Load Game" choice renamed to "Continue" — the only choice that still makes sense once there's no separate save/load concept, just resuming a character's always-current state. Left "New Game" (restart this character from scratch) alone — that's a distinct feature, not a save/load mention.

Confirmed via search that `SaveGame()`/`LoadGame()` themselves (the actual persistence functions, still called by autosave and by the resume flow) were untouched, and that no "Quick Save"/"Quick Load"/"Save Game"/"Load Game" string remains anywhere in `Source/`.

### Quest progression made session-only (requirement 5, safe part)

[`Source/loadsave.cpp`](../../Source/loadsave.cpp) — `LoadGame()` still reads every quest's bytes from the save file (needed to keep the file cursor correct for `LoadPortal` and everything after), but immediately discards them by calling `InitQuests()` instead of the previous `SyncQuestLogState()`. `InitQuests()` is the same function a genuinely new character starts with — it reinitializes every quest to a fresh, unstarted state (respecting this save's own quest-pool randomization, since `glSeedTbl` is already loaded by that point in `LoadGame()`). Combined with `StartGame()`'s existing behavior (calls `InitQuests()` for new games, never touches quests when resuming), both paths now consistently produce fresh quest state, satisfying "we don't care about preserving quest state between sessions." `SaveGame()`'s quest-writing loop was deliberately left untouched — this keeps the save file format 100% unchanged, so the change is purely about what happens on load, not a save-format break.

## Why the boundaries drawn on "the instant a byte changes" (requirement 3)

Interpreted "the instant a byte of information changes in my character it should be saved" as covering discrete, meaningful progress mutations (items, gold, XP, stat points, equipment) — not continuous per-tick state like current HP/mana during combat or player position while walking. Triggering an autosave on every combat tick would mean dozens of disk writes per second during a fight, which isn't what Diablo 3 actually does either (it checkpoints at sensible points, it doesn't write on every point of damage taken) and risks real performance hitching. Flagging this boundary explicitly rather than silently assuming it's fine.

## Verification

- Rebuilt both `build/x64-Debug` and `build/x64-Release` after every batch of changes — all 68 compilation units built cleanly, only pre-existing unrelated `C4267`/`C4244` warnings.
- Launched `DiabloOrcl.exe` and drove it via synthetic input (with the project owner's go-ahead) to confirm in-game:
  - Main menu shows `Oracool Edition v1.0.3`.
  - Selecting the existing "Oracool" character shows **Character Exists** with **Continue** / **New Game** (not "Save File Exists" / "Load Game").
  - Continuing loads the character into town successfully (70/70 HP, 10/10 mana, gold intact) — confirms the quest-reset-on-load change doesn't break loading a save.
  - The character panel shows **PALADIN** as the class (confirms the earlier Warrior→Paladin rename is still intact and unaffected by this session's changes).
  - The pause menu (opened via the MENU button) shows exactly **Main Menu / Options / Exit Game** — no Save Game, no Load Game.
- Did not exercise the stat-point-spend or equipment-change triggers live in this session (the test character had no stat points available at level 1, and no spare item to swap), so those two are verified by code review and by matching the same proven pattern as the level-change/XP-gain triggers, not by an in-game screenshot. Flagging this rather than claiming full live coverage.

## Not done / deliberately held back

**The riskier half of requirement 5** — "all quests should be unlocked in the questlog" from the very start of a session, not progressively revealed as vanilla does — was investigated but not implemented. `_qlog` (the flag controlling quest-log visibility) turned out to be tightly coupled with dialogue gating throughout `towners.cpp`, `stores.cpp`, `objects.cpp`, and `monster.cpp` (e.g. `stores.cpp:1644`: NPC quest dialogue only triggers when `_qactive == QUEST_ACTIVE && _qlog`), not just a display toggle — forcing every quest to `QUEST_ACTIVE`/`_qlog = true` immediately would also make every quest's dialogue and reveal chain available at once, including some quests whose first dialogue step may itself depend on starting from `QUEST_INIT` rather than `QUEST_ACTIVE`. This needs a real design pass or explicit confirmation before touching it, given the risk of breaking quest trigger sequences that have worked since the original game. Session-only persistence (the safe half of requirement 5) is done; "all quests visible immediately" is not.

## Related

- [[Idea-Backlog]]
- [[2026-08-09 - Rename Warrior to Paladin]]
