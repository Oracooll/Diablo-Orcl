# A summon in town crashed the game

2026-09-14 — v1.12.012

## Why

> "i tried casting valkyrie - game crashed."

## Finding it

Windows Error Reporting logged the crash: Debug `DiabloOrcl.exe` 1.12.11, access violation `0xc0000005` at offset
`0x1af328`. That RVA was symbolized against the Debug PDB through `dbghelp.dll` (no debugger is installed here; the
script is `symbolize.ps1` in the session scratchpad).

The crash was in `Monster::exp`, `monster.cpp:5203`, at `data().exp`: a monster with no type data. The monster
health bar and the XP counter both call it when a monster is shown.

## Cause

- **No slot in town.** Every spell is castable in town (an older user request), but town never calls
  `InitGolems`. A quest's set level calls it, but adds no slot while `setlevel` is true. On both, `Monsters[playerId]`
  is not a golem.
- **The spawn.** `AddGolem` spawned into that slot all the same: the position was set and the monster drawn, but it
  had no type.
- **Every summon.** Valkyrie, Decoy and every 30-second spirit summon go through that path, so all of them would
  crash in town. The Golem spell itself can too, now that it is town-castable.

## Fix

- **The rule.** `LevelHasGolemSlots()` (`monster.cpp`) returns `leveltype != DTYPE_TOWN && !setlevel`, the same
  two conditions under which InitGolems adds the slots.
- **Root guard.** `AddGolem` returns at once without a slot. This covers the Golem spell and the network command
  too.
- **The refusal.** `CanSummonHere` (`rfa12_actives.cpp`) answers for Valkyrie and `Summon`, which also serves Decoy.
  Without a slot the hero says "I can't cast that here" and the cast fizzles.

## Tests

- `OracoolCensusNotes.NoSummonWhereThereIsNoGolemSlot`: town has no slot, the Cathedral does, and a set level does
  not.

## Not verified here

This build was not run in the game. In a dungeon, Valkyrie should appear; in town, the Rogue should say she can't
cast it.
