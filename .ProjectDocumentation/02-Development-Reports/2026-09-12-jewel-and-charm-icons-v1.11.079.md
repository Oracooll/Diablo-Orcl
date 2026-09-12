# Jewels and growing charms get real art, and the icon sheet could not be rebuilt at all

2026-09-12 — v1.11.079

## What arrived

Batches 17 and 18 against RfA-06, written three hours earlier. Both landed in the Resources
**root** — which stopped being a drop zone yesterday, so they were filed and the root cleared again.

- **Batch 17**, 15 jewel icons: five affix families (fervor, focus, aegis, ruin, warding) × three
  grades (flawed, plain, radiant), 28×28, plus 4× previews and a contact sheet.
- **Batch 18**, 9 charm icons: the six plain charms and the three growing ones, same shape.

Both packages claimed numeric self-verification — exact size, binary alpha, 1px clearance, palette
conformance, unique hashes — and neither claimed any in-game verification, correctly.

## What was applied, and what was not

The 15 jewels and the **three growing charms** went in. That is 18 icons that had never had art:
both families were drawn procedurally into `%TEMP%` by their generators until today.

The **six plain charms were held back.** Batch 18 delivered remakes of icons already in the game,
and comparing them side by side at 4× said the incumbents are better: brighter, and much more
readable at 28 px. The delivered Storms charm is a murky forked nail where the one in the game is a
clean stamped coin with a bolt; several others collapse into indistinct dark clusters. The package's
own note says "existing in-use artwork not overwritten", which is true of the package and not of
applying it — so this needed a decision rather than a copy. They are filed as concept art and the
comparison image went to the user; reversing this is one copy command if they disagree.

A note for the next batch: "apply the delivery" and "overwrite what is already shipping" are
different acts, and a delivery that silently contains the second needs looking at before copying.

## The generators: two roles in one variable

The handoff notes asked for the specs to be repointed from `%TEMP%` to the real art. Doing that the
obvious way would have destroyed the delivery. Both generators open with:

```powershell
$artDir = $ArtDir
if (Test-Path $artDir) { Remove-Item -Recurse -Force $artDir }
```

So `$ArtDir` is not "where the art is" — it is "the folder this script owns and wipes". Repointing
it at `Resources\01-in-use-assets\items\jewels` would have **recursively deleted batch 17** on the
next run, and the run after that would have quietly produced a sheet full of procedural lozenges
with nothing anywhere reporting a problem. Caught by reading, before running.

The fix splits the two roles:

- `$ArtDir` keeps its `%TEMP%` default and its wipe, and still draws placeholders.
- a new `$SpecArtDir` says where the **specs point**, defaulting to the real folder.
- per file: real art if it exists, else that file's placeholder — so a partial delivery still
  builds a whole sheet instead of failing at the first gap.
- the spec keeps the **relative** path (`..\Resources\...`) though existence is checked against the
  resolved one, because `build_item_icons.cmd` runs from the repository root and a machine-specific
  absolute path in a tracked file is a path that breaks at the next folder move.

Both now report what they used: `real art: 15 of 15`, `real art: 3 of 3`. The `.inc` tables did not
change — git confirms only the two spec files moved, which is what the handoff asked for.

## The real finding: the icon sheet was unbuildable

`build_item_icons.cmd` died with:

```
Unhandled Exception: System.ArgumentException: Parameter is not valid.
   at System.Drawing.Bitmap..ctor(String filename)
```

A missing-file error that never names a file. Checking every spec input for existence found **183
missing** — every composite sheet cut in the file. Cause: `set ART=..\Resources\01-in-use-assets\items`,
a leftover of the 2026-09-11 reorganisation. `items\` holds only the per-icon subfolders
(`charms`, `jewels`, `unqbase`) that generated specs address by their own full paths; the composite
sheets — the worn slots, all fifteen set tiers, the gems, the runes — live in `item-sets\`.

This is a **pre-existing break, and mine**. Yesterday I repaired three icon-spec `.txt` files after
the same folder move and reported the paths fixed. I did not check the build script's own `ART`
root, so item icons could not be rebuilt at all between then and now. Nothing regressed in the
shipped game, because `oracool_items.cel` was already built and is tracked — the damage was that the
next person to rebuild it, which turned out to be me, would hit a nameless exception.

The one-line fix carries a comment naming the failure mode, so the next nameless `Bitmap..ctor`
death is diagnosed by checking spec inputs rather than by reading the `ART` line and believing it.

## The placeholder inventory (asked mid-task)

Six icon families are drawn procedurally, with no real art:

| Family | Icons | Generator | After today |
|---|---|---|---|
| Jewels | 15 | `GenJewels.ps1` | **real art** |
| Growing charms | 3 | `GenGrowingCharms.ps1` | **real art** |
| Salvage materials | 14 | `GenSalvageMaterials.ps1` | placeholder |
| Mystic Orbs | 8 | `GenMysticOrbs.ps1` | placeholder |
| Encounter items | 6 | `GenEncounterItems.ps1` | placeholder |
| Signet of Learning | 1 | `GenSignets.ps1` | placeholder |

**29 icons still placeholder.** And the fragility is not theoretical: all four remaining `%TEMP%`
folders were **empty** when checked, so those generators had to be re-run before the sheet could be
cut. Salvage (14) is the largest and the most seen — every item breakdown — and is the obvious
RfA-07.

## Verification

- 8 of the 18 new frames composited out of the cutter's own previews and looked at: five jewel
  families and all three growing charms survive the `town.pal` quantization.
- `oracool_items.cel`: 626 frames, 844,387 bytes.
- `oracool.mpq` repacked for both trees (466 files) — a normal build does not repack it.
- **720/720 tests pass**, one timedemo skipped as always.
- Debug and Release both build clean; RTM refreshed (exe and archive).

## Still open

Everything here is icon art, which no test can judge. Worth a look in play: a jewel of each family
in the inventory, and the three growing charms, which should read as records rather than trinkets.
