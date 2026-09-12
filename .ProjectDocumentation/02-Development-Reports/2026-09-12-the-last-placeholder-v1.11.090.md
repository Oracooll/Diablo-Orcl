# The last placeholder icon, the Barbarian's own face, and two paintings

2026-09-12 — v1.11.090

## What arrived

Batches 27, 28 and 29 — all of RfA-10's signet re-request and all of RfA-11 — inside the hour.
Every canvas exact: 28×28 + preview, 180×76, and two 1280×720.

**All three applied.** Nothing failed its checks, which is the first time today that has been true
of a whole poll.

## Batch 27 — the signet, and a check that was worth writing mechanically

Batch 25 was rejected for reading as a gold nugget: no band, no bezel, no hole. RfA-10 put the
failure first and made it the batch's own check — *"the ring's interior hole must be VISIBLY
TRANSPARENT at native size"*.

That is a check a machine can run, so I ran it rather than squinting: flood-fill the transparent
pixels inward from the border, and count the transparent pixels the fill cannot reach. Those are
enclosed — the hole.

```
opaque 264   enclosed transparent 113   PASS
```

Then again **after** the cut, because the palette match and the island sweep both sit between the
PNG and the game, and the island sweep is the thing that erased Knellbranch one version ago:

```
CUT signet: opaque 264   enclosed hole 113   PASS
```

Identical. The band, the bezel and its engraved mark all read at native size.

**This was the last procedurally-drawn icon in the game.** 29 this morning — jewels, growing charms,
salvage, the orbs, the encounter items, and this — and now none. `GenSignets.ps1` keeps drawing its
placeholder as a per-file fallback, and the `%TEMP%`-folder fragility that could make a full sheet
rebuild fail is no longer load-bearing for anything.

## Batch 28 — the Barbarian stops wearing the Warrior's face

The engine's hero-portrait override hook had **never been used**, and the reason turned out to be
the interesting part: it only accepts a PCX, and every asset this fork ships is a PNG.

Meanwhile the sheet has six frames, so the block at `diabloui.cpp:813` reassigns Monk and Bard but
only touches the Barbarian's slot when a **seventh** exists — and it never does. So two classes wore
one face on the screen where you choose between them.

Rather than convert a full-colour painting down to an 8-bit PCX to satisfy a loader, the hook now
tries `ui_art\hero<i>.png` first and falls back to the PCX, so a hand-made PCX override still works.
`oracool::LoadPngSpriteList` is the general form of the two existing PNG importers — the item-drop
one now calls it, so there is one implementation rather than two.

**A correction I owe the brief:** I told the artist the differentiator was "bare head versus helmet,
where the Warrior is a helmeted knight". The vanilla Warrior portrait is bare-headed. I wrote that
without opening the sheet. It cost nothing — the delivered Barbarian is unmistakable anyway on beard,
fur mantle and build — but the brief asserted a fact about art I had not looked at.

Worth noting for judgement rather than as a defect: the five vanilla portraits share a dark blue
night-sky backdrop and this one is warm brown. Only one portrait is on screen at a time, so it shows
as a tone shift when switching classes rather than as a mismatch in a row.

## Batch 29 — the Hive and the Crypt

Nine loading screens were true-colour paintings; `CutLevel5` and `CutLevel6` still went through the
8-bit CEL path, so entering Hellfire's two branches showed a 1996 screen between nine modern ones.

Delivered as the opposites the brief asked for — a wet organic warren in sick yellows against a dry
stone tomb hall in cold greys with a single light shaft — and checked against three of the shipped
nine for mood and contrast. **No code change was needed:** `LoadCutscenePng` already appends `.png`
and already routes both names, so the files simply started being found. The two dead slots had been
waiting on nothing but art.

## Verification

- Signet hole measured before and after the cut, as above.
- Portrait composited against the vanilla Warrior at 3× and against all five at native size.
- Both paintings viewed beside three shipped screens.
- Archive grew 466 → **469 files**; the packer's recursive walk picked up the two new folders
  (`nlevels/`, `ui_art/`) with no change to it.
- **724/724 tests pass.** Debug and Release build clean; RTM refreshed with exe and archive.
- Resources root cleared; all three batches archived under `02-concept-assets/delivered-packs/`.

## Still open

- **Batch 26** — the 13 Sorceress glyphs, the only outstanding request. `sorc_tree_icons.png` is
  still 35 glyph / 13 legacy, the last borrowed art in the skill trees.
- **Batches 21 and 22** — 12 ground-tumble sheets, delivered and verified, awaiting the drop-anim
  registration and the 344-item cursor mapping.
