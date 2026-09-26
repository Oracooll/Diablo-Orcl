# Rift panel in town, rift guardians drop random items — v1.12.187

2026-09-26 (dev notes batch)

> "i want to keep seeing the nephalem rift and guardian rift fill bars on when in town and same applied to countdown timers."
> "countdown timers need to tick while i am in town and i need to be able to see it."
> "rift guardians/bosses to drop random items, not the uniques they drop when killed in quest."

## The rift panel in town

`DrawRiftHud` (oracool/rift.cpp) drew only `InRift()`. It now draws whenever a rift is open (`State.kind != None`) and
has not ended on the walk home (`returnedHome`): name, fill bar, the Guardian Rift's clock and a cleared Nephalem
Rift's closing countdown, under the mini-map as inside.

The clocks themselves already ran in town: `ProcessRift()` is called every GameLogic tick before the town branch
(diablo.cpp), and the Guardian clock and the Nephalem close clock count down "wherever the hero is". Only the panel was
missing - so the second note is answered by the first fix, and the note's premise was checked in code rather than
changed.

## Rift guardians drop random items

- `SpawnItem` (items.cpp): a monster's special treasure (T_UNIQ - the Undead Crown, the Cleaver) is skipped for
  `oracool::IsRiftGuardian`, so the guardian takes the unique-monster random roll (RndUItem base, uper 15).
- `SpawnLoot` (monster.cpp): a rift's Na-Krul no longer takes his fixed branch (three weapons, Apocalypse book); every
  rift guardian drops `RiftGuardianItemCount()` items - 4 in a Nephalem Rift, 6 in a Guardian Rift.
- Unchanged: the keystone, the set-item, named-set, gem and necro-base rolls; quest kills outside rifts.

## Build

v1.12.187 Debug. ctest 826/828 on the run; the 2 were oracool_hero_look_test blocked by Smart App Control at
discovery - relinked, 12/12 pass. Every test passes.
