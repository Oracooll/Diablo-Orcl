# The complete 273 skill icons (v1.9.88)

Date: 2026-08-27
Version: 1.9.88
Tests: 563/565 serially (the two standing baseline failures)

MPQ drop-zone sweep. One genuinely new package, and it closes a silent fault in five of the six
class trees.

## The sweep

`Oracool.MPQ/` holds one file newer than everything already consumed:
`colorful-skill-icons-complete-273.zip`, 633 MB, dated today. Its sha256 matches the accompanying
`.sha256`. Everything else in the folder is from 17–26 August and already in the game — the auras,
the bezels, the limestone HUD, the chest, the level-up and portal icons, the burger menu, and the
paladin combat batch consumed yesterday.

The package carries six ready-made class strips, 273 individual icons, an index CSV, a manifest and
an engine handoff note.

## The fault it fixes

`ClassTreeIconIndex` is a skill's position within its class block, used directly as the frame number.
So a strip's frame count has to reach the end of its class's block. It did not:

| class | strip had | block needs |
|---|---|---|
| Barbarian | 30 | 49 |
| Rogue | 30 | 49 |
| Sorceress | 30 | 48 |
| Bard | 21 | 39 |
| Monk | 21 | 39 |
| Paladin | 49 | 49 |

`DrawStripIcon` bounds-checks and returns early, so every skill past the end of its strip **drew
nothing at all** — no icon, no error, no log line. Exactly the failure mode `ClassTreeStrips`' own
comment warned about when five strips were once left unloaded: *a missing frame is indistinguishable
from a skill that has no icon*. Only the Paladin's was current, having been rebuilt yesterday.

Roughly ninety skills across five classes were drawing blank plates.

## The check before installing

A strip is only correct if its frame order matches the skill table's order — otherwise every skill
silently gets the wrong picture, which is worse than a blank one because it looks fine.

So the 273 names in `icon-index.csv` were diffed, in file order, against the 273 `N_("...")` names
extracted from `Skills[ClassTreeSkillCount]` in `class_tree.cpp`.

**Zero differences.** The set was generated against the table, and the per-class counts in the
handoff note match the per-class counts in the CSV, which match the code.

Cells are 56x56, RGBA with binary alpha and transparent margins preserved so the active/available/
locked plate backgrounds still show through — which is what `DrawStripIcon` composites against.

## What changed

Six PNGs replaced in `Packaging/resources/oracool_assets/ui/`, and the stale comment block above the
six `ArtAsset` declarations rewritten: it claimed 29 icons for the Paladin and 21 for the Bard and
Monk, numbers that had been wrong since those blocks grew.

**The MPQ was repacked** — `tools\build_oracool_mpq.cmd`, 416 files, 35.7 MB. A normal build does not
rebuild `oracool.mpq`, so without this the new art would sit in the source tree and never reach the
game.

## To look at in game

Open the Abilities window on a Barbarian, Rogue, Sorceress, Bard or Monk and scroll to the deeper
tiers. Those plates were empty; they should all carry icons now. The Paladin's should be unchanged.
