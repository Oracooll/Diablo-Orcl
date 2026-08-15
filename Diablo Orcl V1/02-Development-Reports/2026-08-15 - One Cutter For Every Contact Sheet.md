---
date: 2026-08-15
version: 1.5.66
area: Asset pipeline / attack icons
---

# One Cutter For Every Contact Sheet

> take Fist and Regular attack icon from the mpq folder and place them as well.

`Fist and Regular Attacks.png` arrived in exactly the format `Paladin Skills.png` did — white glyphs
on a green key, a grid, a printed label under each. Second sheet, same shape. So this pass shipped
the icons *and* stopped there being two cutters.

## Why factor rather than copy

The obvious move was to paste the 150-line pixel cutter into `CutAttackIcons.ps1` and change three
constants. That would have worked today and been wrong by next week: two copies of the same
band-scanning, bbox-taking, pad-to-square logic drift the first time one of them is fixed, and the
symptom would be two icon strips at subtly different weights sitting in the same list.

`tools/CutLabelledIconSheet.ps1` now owns all of it, parameterised by source, grid, layout and output
name. `CutPaladinSkills.ps1` and `CutAttackIcons.ps1` are each about thirty lines, almost all of it
the layout table and the reasoning specific to that sheet.

## Proving the refactor changed nothing

A refactor of code that emits a binary is only safe if you check the binary. The committed Paladin
strip hashed to:

```
a2187dbbe24975e18af062341c9879b3caf90e565ffa2d40beace5111f18983e
```

Re-cutting it through the shared script produced the same hash, and `git diff` on the asset tree came
back empty. Not "it looks the same" — byte-identical.

## What generalising cost

Almost nothing, because the original was already written against measurements rather than constants:

- The band count check became `Rows * 2` instead of a hardcoded 4. The attack sheet has one row, so
  two bands; it is checked just as strictly.
- The column count check takes `Columns`.
- The layout table is a parameter.

Everything else — scanning for the bands, ignoring the label band, one common padding box across the
whole strip so the glyphs keep their relative sizes — was already sheet-agnostic.

The attack sheet's numbers, for the record:

```
  icon band y 348..654
  Regular Attack   glyph 304x307 at +231,+348
  Fist Attack      glyph 242x301 at +938,+348
```

## What changes in game

`ui\attack_icons.png` was not shipping at all — the user cleared the placeholder icon set to make room
for their own art, and the wells and the Skills sheet had been falling back to the engine's small
spell icon since. The strip is back, so:

- The Skills sheet's Regular and Fist Attack rows draw the real icons.
- Both HUD wells draw a real basic-attack icon again when no spell is readied. `WellIconSize` returns
  the loaded 38×38 rather than its fallback, and the run-time assert that `SkillWellIconSize` matches
  the shipped cell now has something to check.

Both icons are drawn in the same hand as the Paladin skills, which is the point of cutting them with
the same code: the Skills sheet reads as one set from the attack rows down to Blessed Hammer.

## State

352/354, the standing baseline. `oracool.mpq` repacked — 71 files, one more than before, which is
`attack_icons.png` returning.

Not tested in-game — the user runs the game.
