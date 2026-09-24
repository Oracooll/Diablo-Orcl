# Second /dev batch: rifts, gold, tips — v1.12.173

2026-09-24

> check the dev notes and the screenshot and process.

Thirteen in-game notes (eleven items), coded in full and built once. Each note's outcome is in
`development-archive.md`.

## The rift close stranded the hero on dungeon level 9

Screenshot: "Level: 9, Area Level: 9", a black cave, no way out. The cleared Nephalem Rift's
closing clock sent `WM_DIABRETOWN`, whose handler loads `myPlayer.plrlevel`. Inside a set level
`plrlevel` holds the **set level's id** (`StartNewLvl` → `player.setLevel(setlvlnum)`), and
`SL_RIFT_NEPHALEM` is 9, so the "return to town" loaded dungeon level 9. The close now sends
`WM_DIABRTNLVL` with `GetMapReturnLevel()`, as the way home does.

The same trace found the v1.12.169 monument return was hollow: `GetMapReturnPosition` asked
`InRift()` and the gate object, but `WM_DIABRTNLVL` clears `setlevel` before `LoadGameLevel` asks,
and the gate object belongs to a town not yet built. It now asks `IsRiftLevel(setlvlnum)` — the
level being left — and answers with the monument's entry tile, remembered from the last town build
(`StonegateLastEntryTile`, falling back to (32,57)).

## Rift entry and exit

- **Entry is a click.** `TryEnterRiftFromTown` no longer fires on standing on the entry tile. A
  world click records whether it landed on the portal (`NoteWorldClickForRift`, fed by the cursor's
  hover flag); the hero goes in once within `RiftEntryReach` (2) tiles of the portal's tile. Any
  other click clears the request. `entryArmed` retired.
- **A way out from the start.** `RiftLevelPopulated` lays the exits on every visit: one on a
  walkable neighbour of the arrival spot (chosen once per rift), and the guardian's way home once he
  has fallen. `RiftNoteReturnHome` ends the rift only when it is cleared, so an early exit is a trip
  to town and the rift waits.
- **No stairs.** `oracool/stairless` (written by a subagent, reviewed): each DRLG snapshots
  `dungeon[][]` before `PlaceStairs` and restores every changed megatile after the stairs are
  accepted, marking them Protected. Off unless `GeneratingStairlessLevel`, which `BuildRiftLevel`
  sets around `CreateDungeon` only — normal floors are byte-identical. Crypt and Caves/Nest landings
  step `+{0,2}` onto floor; `OpenFloorNear` is the safety net (the palette and SOL load moved ahead
  of it).
- **Area level: the hero's.** User chose "hero level only": block base + clamp(clvl/3, 1, 16). The
  r3 test was rewritten to the new rule. Monster scaling still nets 100% on a Nephalem Rift, since
  the band floor is the tier's rung.
- **Sounds.** Rises: `USFX_SKING1` / `USFX_CLEAVER` / `PS_DIABLVLINT` (Diablo, Na-Krul) over the
  Milestone chime. Falls: `LS_APOC` over the cleared chime, plus `USFX_DIABLOD` for Diablo.

## Town and inventory

- Withdraw box: the red X in its corner; the gold pile toggles it. Both are asked before the modal
  prompt swallows the click (`CheckGoldWithdrawPromptPress`).
- Split tips, matching the mechanic exactly: "Shift + right-click to split the stack" on stackable
  consumables of two or more (single-player, mouse); "Right-click to split the pile" on gold of two
  or more on backpack page one. **Bug found:** gold right-clicked on pages 2–10 reached
  `StartGoldDrop` with `pcursinvitem` at -1 and read `InvList[-1 - INVITEM_INV_FIRST]`; refused now.
- Rift Monument: a yellow 4px square on the mini-map.
- Event log: the 18-line cap and a spare empty row removed; the window's height is the limit.

Debug build and tests at the end of the batch.
