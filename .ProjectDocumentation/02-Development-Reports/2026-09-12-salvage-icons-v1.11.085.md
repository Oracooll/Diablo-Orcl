# The salvage materials get real art, and the palette has no green

2026-09-12 — v1.11.085

## What arrived

The 15-minute delivery monitor's first poll found **four** batches in the Resources root — the whole
of RfA-07 and the whole of RfA-08, delivered inside the hour.

| Batch | RfA | Contents | Spec |
|---|---|---|---|
| 19 | 07 | 7 salvage materials | 28×28 + 112×112 previews + 784×112 sheet |
| 20 | 07 | 7 Charms of Salvaging | same |
| 21 | 08 | 6 worn-gear tumbles | 1248×160, 13 frames of 96×160 |
| 22 | 08 | 6 exotic-base tumbles | same |

Every dimension matched to the pixel. **This report covers batches 19 and 20 only**; the twelve
tumble sheets need the drop-anim registration and the 344-item cursor mapping, which is its own unit
of work.

No collision check was skipped: none of the 26 filenames existed, so nothing here remakes art
already in the game and no before/after comparison was owed.

## What they replace

The 14 salvage icons had **never had art**. `GenSalvageMaterials.ps1` drew them from a 2026-08-20
placeholder request that asked only for "some orb looking sprites" — seven identical spheres and
seven identical rounded tablets, carrying no information but hue. They now read as what they are:
flaked scales, a powder heap, bound fibres, gold nodules, a coiled thorned vine, engraved plaques,
glassy shards. The charms share one hooked iron implement, each binding a token of its tier's
material, so the family reads as a set and never as the substance it produces.

## The generator

Same split as v1.11.079's jewels: `$specArtDir` says where the specs point, `$artDir` keeps its
`%TEMP%` placeholders as a per-file fallback, and a `Resolve-SpecPath` helper picks real art when it
exists. Reports `real art: 14 of 14`.

**The wipe trap was checked per script, not inferred.** `GenJewels.ps1` and `GenGrowingCharms.ps1`
open with `Remove-Item -Recurse -Force $artDir`, so repointing *their* art dir at Resources would
delete the delivery. `GenSalvageMaterials.ps1` only creates the folder if absent and never wipes it
— verified by grep before editing, and the comment now says so, because "the other two do X" is
exactly the reasoning that would eventually delete a batch.

## The real finding: there is no green in the palette

Set Engravings was specified as "muted sage green", delivered green, and **came out bone-tan in the
game**. Measured rather than guessed: of the palette's 256 entries, **zero** have green as their
dominant channel.

An inventory icon is indexed CEL art. It can only hold the 256 palette colours, so a green pixel is
matched to the nearest non-green. The fork *does* have a sage green — `ColorOracoolGreen`'s band —
but only as an **RGB value drawn on the 32-bit screen**, for text and for `FillRectRgb` fills like
the runeword border and the set-item backing fixed one version ago. Those two facts sit one line
apart in the same file and I had conflated them.

So **my instruction to the artist was wrong**: RfA-07 and RfA-08 both listed "one muted sage green"
as a usable ramp. Both are corrected, RfA-09 now states the rule with the measurement, and a memory
note records it. The Set Engravings icon is left as delivered: its silhouette — overlapping engraved
plaques — is what distinguishes it, which is exactly what the brief's shape-first rule was for, and
a recolour is available if the tan reads badly in play.

This is the same class of error as the three found earlier today (the grey fire text, the stale
`vcvars` path, the stale `ART` root): a name — `PAL8_GREEN` — that outlived the thing it described.
`engine/palette.h` says in place that those entries are orange at runtime. I had read that sentence
two versions earlier and still wrote green into a brief.

## Verification

- Ten cut frames composited out of the cutter's own previews and looked at — all correct through the
  palette match; that is how the Set Engravings tan was caught rather than shipped unnoticed.
- `oracool_items.cel`: 626 frames, 841,992 bytes.
- Both archives repacked; the palette re-staging line printed, as v1.11.084 made it.
- **723/723 tests pass.** Debug and Release build clean; RTM refreshed with exe and archive.
- Resources root cleared of batches 19 and 20; previews, contact sheets and notes filed under
  `02-concept-assets/delivered-packs/`.

## Still open

- **Batches 21 and 22** — 12 tumble sheets, applied next. The art is verified (the belt sheet's arc,
  timing and at-rest frame match `gemflip.png` exactly), but wiring needs 12 anim names, 12 indices,
  and the cursor mapping for 250 uniques + 94 set pieces + the worn tiers, which must come out of
  the generators rather than a hand-written list.
- **RfA-09** (Mystic Orbs 8, encounter items 6, Signet 1) outstanding; the monitor is watching for
  batches 23–25.
