# Mystic Orbs and encounter items get real art; the signet does not

2026-09-12 — v1.11.086

## What arrived

Batches 23, 24 and 25 — all of RfA-09, delivered within about an hour of the brief. 15 icons, every
canvas exact: 28×28 runtimes, 112×112 previews, 896×112 and 672×224 contact sheets.

**Fourteen applied, one held.**

## Batch 23 — the eight Mystic Orbs

The brief's central demand was that shape carry the meaning, because the placeholders were eight
identical spheres and two pairs were near-identical *by design*: Might (190,72,56) against Fury
(176,46,46), both red, and Fortune (206,190,92) against Avarice (214,174,54), both gold.

Delivered as eight different vessels, each holding its stat's symbol:

| Orb | Vessel | Symbol |
|---|---|---|
| Might | squat thick sphere | clenched fist |
| Grace | tall slender teardrop | feather |
| Insight | faceted octagonal flask | open eye |
| Vigour | wide round-bottomed flask | heart |
| Warding | hexagonal shield-shaped vessel | ward sigil |
| Fury | cracked jagged sphere | splintered blade |
| Fortune | four-lobed clover | star |
| Avarice | short fat coin-purse | stacked coins |

Both collisions are resolved by silhouette rather than hue, which was the point. Warding is steel
blue with a bone rim, not the placeholder's purple.

## Batch 24 — three maps and three reward charms

Maps share one scroll family and are told apart by an **edge mark** — arched window, closed ring,
flame — so they survive a greyscale read. The charms are three unrelated silhouettes: an arched
reliquary casket, an iron ring on a cord, a brass wax-seal stamp. A map is never mistaken for the
charm it pays out. Mourning is dusty rose, not purple.

## Batch 25 — the signet, held back

**Not applied.** The brief asked for "a heavy gold signet ring seen at a three-quarter angle, its
flat bezel face turned toward the viewer and engraved with a single rune", and named the check: "the
bezel face is legible as an engraved flat surface at native size — that is the whole subject."

What arrived is a **lumpy gold mass**. At 8× there is no band, no bezel and no engraving; it reads
as a gold nugget. Compared against the placeholder side by side, the procedural version is the
clearer of the two *as a shape* — it is unmistakably a ring — even though its stone is a purple the
palette cannot hold.

So this one needed a decision rather than a copy, the same as batch 18's six plain charms. The
comparison went to the user; the art is filed under `02-concept-assets/delivered-packs/batch-25-signet/`
and applying it later is one copy. `GenSignets.ps1` is untouched and still draws the placeholder.

## The generators

Both wipe traps were **checked in the script, not inferred**: `GenMysticOrbs.ps1:48` and
`GenEncounterItems.ps1:32` each open with `Remove-Item -Recurse -Force $artDir`, so `$ArtDir` stays
in `%TEMP%` and a new `$SpecArtDir` points the specs at Resources, with a per-file fallback through
a `Resolve-SpecPath` helper. `GenEncounterItems` has two spec sites (maps, then charms) and both were
repointed. Reports `real art: 8 of 8` and `6 of 6`.

**The backslash trap bit again**, for the fifth time this session: a perl one-liner writing
`"..\Resources\01-in-use-assets\items\encounters"` produced
`"..Resources\x01-in-use-assetsitems\x1Bncounters"` — `\0`, `\01` and `\e` interpreted as escapes.
Caught by grepping the written line rather than trusting the exit code, and repaired with `\x5c`
hex escapes, which is what the memory note has said to do since the first time.

## Verification

- All 14 applied frames composited out of the cutter's own previews and viewed. Every colour here is
  palette-legal, so nothing washed out the way batch 19's green Set Engravings did.
- `oracool_items.cel`: 626 frames, 839,843 bytes.
- Both archives repacked; palette re-staged by the build script.
- **723/723 tests pass.** Debug and Release build clean; RTM refreshed with exe and archive.
- Resources root cleared; all three batches archived under `02-concept-assets/delivered-packs/`.

## Where the placeholders stand now

29 procedural icons at the start of the day; **1 left.**

| Family | Count | State |
|---|---|---|
| Jewels | 15 | real art, v1.11.079 |
| Growing charms | 3 | real art, v1.11.079 |
| Salvage + charms | 14 | real art, v1.11.085 |
| Mystic Orbs | 8 | real art, this version |
| Encounter items | 6 | real art, this version |
| **Signet of Learning** | **1** | **still procedural — delivery rejected** |

## Still open

- **Batches 21 and 22** — 12 ground-tumble sheets, verified but unwired. Needs 12 anim names, 12
  indices, and the cursor mapping for 250 uniques + 94 set pieces + the worn tiers, out of the
  generators rather than a hand-written list.
- **The signet** — awaiting the user's call on the delivered art, or a re-request.
