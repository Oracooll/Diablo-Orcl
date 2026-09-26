# ChatGPT audit of v1.12.188: ten findings fixed

2026-09-26 — v1.12.189

## Why

The user had ChatGPT audit the code against a brief (`Resources\ChatGPT Code Audits\2026-09-26-v1.12.188-AUDIT-BRIEF.md`). The brief set out six tracks:

- saves and state
- skills
- items
- world
- interface
- build and tests

It came back with ten findings: 2 P1, 6 P2 and 2 P3. All ten were checked in the code and all were real. The per-finding table is in `06-Reference/External Audits/2026-09-26 ChatGPT audit of v1.12.188/99_RESPONSE_...md`.

## What changed

- **Stash sort is all-or-nothing** (`qol/stash.cpp`, ITEM-01, P1).
  - `SortStash` now returns `bool`.
  - It keeps a copy of the stash, and it checks the unit count at the end (one per item, a stack's count for a stack).
  - When the one-page-per-tier layout cannot hold everything, the sort puts the original back, and the button says "The stash is too full to sort".
  - Before this fix, a nearly full stash lost the items that no longer fitted.
- **Workshop custody** (SAVE-01, P1). The workshop now gets the same three protections the Levski Cube got:
  - It is closed before the exit save (`gamemenu.cpp`).
  - Autosave waits while it is open (`auto_save.cpp`).
  - `FreeGame` resets it.

  The bench now falls back to the stash, like the craft grid, and its "full" messages name both.
- **Essence** (`spells.cpp`, SKL-01). `SkillPaysThroughFacade` checks and pays Essence for the 17 Essence-priced rows through the facade that Rage already used.
- **Cold timers** (`oracool/chill`, `monster.cpp`, SKL-02). The new `ClearColdStateForMonster` runs on monster delete and init.
- **Mystic rebuild** (`items.cpp`, ITEM-02). `FindAffixRowForRecord` finds the row a record came from. `AffixUsesBothEndpoints` lists the nine powers whose two parameters go to two different fields. Those replay the row's own pair instead of a pinned 3-3.
- **Rift portals** (`oracool/stonegate.cpp`, WORLD-01). `EndRiftAndItsPortals` closes every town portal that leads into a rift level whenever the rift ends.
- **Guardian identity** (`oracool/rift.cpp`, WORLD-02). `IsRiftGuardian` requires `InRift()`.
- **Escape** (`diablo.cpp`, UI-01). The workshop has its own branch in `PressEscKey`.
- **Click-through** (`oracool/hud_layout.cpp`, UI-02). `IsPointOverFloatingWindow` includes `IsPointOverWorkshop`.
- **Tests** (QA-01). Eight `OracoolAuditV188.*` tests in `test/oracool_audit_test.cpp`, adapted from the audit's probes.

## Verified

- The v1.12.189 Debug build is clean.
- `OracoolAuditV188.*`: 8 of 8 pass. Seven are the audit's own probes, which failed on v1.12.188. The guardian test is new, and it only covers the case with no guardian spawned: the rift has no test seam for spawning one.
- Full ctest suite: 847 of 847 pass. OracoolAudit.DropOddsReport is disabled and Timedemo.WarriorLevel1to2 is skipped, as before.

Nothing here is seen in the game yet. Worth trying:

- Sort a very full stash.
- Right-click on the Mystic's window.
- Press Escape with the Mystic open.
- Cast a curse with no Essence.
- Leave the game with an item on the bench.
