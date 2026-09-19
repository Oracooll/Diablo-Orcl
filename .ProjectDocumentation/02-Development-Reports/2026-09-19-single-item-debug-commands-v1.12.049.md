# One debug command per item type (v1.12.049)

**Date:** 2026-09-19 - Debug only - **819 of 819 tests**. User: "i need debug commands to spawn a single random
item of each type. one debug command per item type" - then "build them". Nothing seen in play yet.

## The audit that preceded it

The user's list: basic, magic, rare, unique, primal, set, ethereal, socketed, runeword. One type missing from it:
**Buffed Unique**, a tier of its own beside the vanilla unique. Of the ten:

| Type | Before | After |
|---|---|---|
| Rare, Buffed Unique, Primal | `giverare` / `giveunique` / `giveprimal` - random base, forced tier | unchanged |
| Basic | none (`drop` rolls any quality; `givebset` drops 13) | **`givebasic ({name})`** |
| Magic | none (`givemset` drops 13) | **`givemagic ({name})`** |
| Unique (vanilla) | `dropu` needs a name; the empty string matched the FIRST unique every time | `dropu` with no name is a random unique |
| Set | none (`givesset` 13, `giveitemset` a whole set) | **`giveset ({name})`** - one random piece; the name matches the piece or its set |
| Ethereal | `giveethereal` always the first durable base in the table | random among the candidates |
| Socketed | `givesockets` always the first socketable base | random among the candidates |
| Runeword | none | **`giverw ({name})`** - a formed runeword on a random fitting base |

## What was built (items.cpp, the `#ifdef _DEBUG` block)

- **`DebugSpawnQualityItem(name, quality)`**: DebugSpawnTieredItem's search loop with the acceptance test
  "exactly this `_iMagical`, no Oracool tier, not ethereal, and a worn or wielded base" (ILOC_UNEQUIPABLE and
  ILOC_BELT are skipped - a potion is ITEM_QUALITY_NORMAL too).
- **`DebugSpawnSetPiece(name)`**: collects every set piece with a base item (and matching the name against the
  piece or the set), draws one, builds it as givesset does (InitializeItem + MakeSetItem), drops it through
  FinishOracoolDrop because nothing here ran SetupAllItems.
- **`DebugSpawnRuneword(name)`**: draws a word from `RunewordAt` (filtered by name), collects the plain bases
  whose `RunewordHostForItemType` matches the word's host and whose `MaxSocketsForItem` holds its runes, draws one,
  seats the runes in order into `_iSocketed`, and lets `TryCompleteRuneword` name it - the same check the socket
  window runs, so it cannot make a word the player could not.
- `DebugSpawnEthereal`, `DebugSpawnSocketedBase`: two passes now - collect the candidates (probe item per base),
  draw one. `DebugSpawnUniqueItem`: an empty name draws a random available unique and continues as before.
- debug.cpp: four handlers and four table rows; the help text of `dropu`, `giveethereal`, `givesockets` says
  "random". The table is 69 commands (42 vanilla, 27 Orcl).

## Verification

- 819 of 819. No new unit test: the spawners need a live level, a player and the item table, as the existing
  ones do; none of those has a test either.
- Build 17 failed at CONFIGURE: OneDrive had renamed `test/inv_test.cpp` to a `-SLStudio2` conflict copy AGAIN
  (byte-identical to HEAD); `git checkout -- test/inv_test.cpp` restored it and build 18 was the clean run.
- A windowless DiabloOrcl.exe (started 11:12, 39 MB, no main window) was terminated before the build per the
  user's standing rule on ghosts.

## To look at in play

`givebasic`, `givemagic`, `giveset`, `giverw` once each; `dropu`, `giveethereal`, `givesockets` twice each to see
the draw change. The Debug Console page (https://claude.ai/artifact/SxsdXJXjupjQtjfartt3KL) lists them.

## Related

- [[2026-09-19-vanilla-tint-backings-v1.12.048]] - the previous build.
