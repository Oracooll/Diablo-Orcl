# The last borrowed art in the skill trees — and the delivery that had been sitting there

2026-09-12 — v1.11.092

## The delivery my own poll missed

Batch 26 arrived at 19:37, between a delivery poll that reported "nothing new" and the sweep that
followed it. The poll was not wrong when it ran; the **sweep** is what found the pack, sitting
unfiled at the `Resources/` root with its 13 glyphs and a `manifest.json` marked `"status":
"exported"`.

That is the loop working as intended, and an argument for it: a poll answers "has anything arrived
since I last looked", and a sweep answers "is anything not where it belongs". Only the second one
catches a delivery that lands in a gap between polls.

## Batch 26 — 13 Sorceress glyphs

Every class strip was pure glyph art except the Sorceress, at 35 of 48. Thirteen of her rows still
wore the *vanilla coloured spell-book icons* they borrowed before any glyph existed, so her two
spell pages were visibly a different art style from every other page in the game.

Verified against the build script's own rule before installing, rather than letting the build throw:
all 13 are exactly 56×56 and **every pixel is `(243,243,243)`, `(12,7,7)` or alpha-0**. Then looked
at the contact sheet — thirteen distinct symbols, correct house style.

After: **every strip 100% glyph, 0 legacy, 0 empty, `problems: 0`.**

## Two tool bugs this exposed

### 1. `BuildGlyphStrips.ps1` could not run at all

`$extraPacks` resolves against `01-in-use-assets\delivered-packs`, and `batch-13-skill-glyphs` was
in **`02-concept-assets`** — moved there by the 2026-09-11 Resources reorganisation. Line 36's
`Get-Content` is unguarded under `$ErrorActionPreference = 'Stop'`, so the script **threw before
writing a single strip.**

It had been broken for a day and nobody could tell, because the strips on disk were already built
from when batch-13 was in the right place. The classification was the error: batch-13's glyphs *are*
in the shipped strips, so by the ledger's own rule — "the master a cutter reads to produce something
shipped" — it is in-use art. Restored, and batch-26 filed beside it.

This is the fourth casualty of that reorganisation, after the `02-source-art` icon-spec paths, the
`ART` root in `build_item_icons.cmd`, and the stale `tools/town.pal`.

### 2. I truncated two strips, and caught it by measuring

`BuildGlyphStrips.ps1` had the **same regex bug** as `AuditGlyphStrips.ps1` — the page field matched
`(\d+)` only, so the four `RetiredFromTreePage` rows were dropped and Barbarian and Rogue parsed as
48 rows against their real 49.

In the audit script that produced a harmless false "MISMATCH". Here it was destructive:
`LoadStripEditable` sizes a **new** bitmap at `$mine.Count` frames, so the first run **truncated
both strips from 49 frames to 48** and threw the last frame away.

I did exactly that, then measured the files — 2744 → 2688 px — restored all three from git, fixed
the regex, and re-ran. Frame counts back to 49, and the retired rows now correctly keep whatever
frame they had rather than being skipped out of existence.

Worth naming the lesson: I fixed this regex in the audit script earlier today and did not think to
grep for the same pattern elsewhere. One `grep -l 'Pal|Bar|Sor|Rog|Bard|Monk'` would have found the
second copy.

## The palette bug in my own v1.11.090

The sweep also caught something I shipped an hour earlier. `LoadPngSpriteList` quantizes against
`levels\towndata\town.pal`, and its own comment explains why any level palette is fine — the shared
half is identical across town and every tileset. **That reasoning does not extend to the front end**,
which runs on `ui_art\diablo.pal`, sharing one of its 128 upper entries with town's.

So the Barbarian portrait was matched against one colour table and drawn through another — exactly
the mistake `ui_backgrounds.cpp` calls `UiLoadDefaultPalette` before `Build` to avoid.

**Measured before fixing, and it changed the conclusion.** Quantizing that portrait three ways —
town's shared half, diablo's shared half, diablo's full 256 — gives three images that are hard to
tell apart; nearest-match finds a close brown either way. So this was a **latent trap, not a visible
defect**, and I said so rather than dressing it up. The fix is worth making for the next asset whose
colours only one palette carries: `EnsurePalette` is now keyed on the path it loaded, and the
portrait call passes `ui_art\diablo.pal` explicitly.

My comment claiming "the PCX path would cost it for nothing" was also wrong on its own terms and is
corrected: the PCX path carries its own 256-entry palette, which is *more* than this path's 128
fixed entries.

## Verification

- 13 glyphs validated pixel-by-pixel against the two permitted colours before install.
- `AuditGlyphStrips.ps1`: all six strips 100% glyph, `problems: 0`, frame counts 48/49/48/49/39/39.
- Strip sizes measured after every build, which is what caught the truncation.
- **724/724 tests pass.** Debug and Release build clean; both archives repacked; RTM refreshed.
- Resources root cleared of batch-26.

## Ledger

The per-folder snapshot in `ASSET-LEDGER.md` was stale by five files and two whole folders (it still
said 464 and was labelled v1.11.063 while the table ran to v1.11.090). Rather than regenerate a
list that will drift again, it now states the real count, names the *Entered the game* table as the
authority, and records the known drift explicitly.

## Still open

- **Batches 21 and 22** — the 12 ground-tumble sheets. Delivered, verified, 1248×160 and
  byte-format-identical to the eight that ship, but still unfiled and unwired: no archive entry, no
  `ItemDropNames` row, no cursor mapping. They are the only outstanding work with art already in
  hand, and they currently drop wearing vanilla tumbles.
- No art request is outstanding. Every RfA from 07 to 11 is delivered and applied.
