---
title: 2026-08-11 - Asset Studio
date: 2026-08-11
tags: [dev-report, tooling]
summary: A WinForms app that converts source art to the Diablo palette and previews it in full and reduced light, so assets can be judged and tuned without a build-and-launch cycle.
---

# Asset Studio

`tools/OracoolAssetStudio.exe` - 25 KB, in-box `csc.exe`, no SDK and no bundled runtime, matching
the `OracoolPcxWatcher` precedent.

## What it is for

The engine renders 8-bit palettized. Full-colour PNG art is quantized to palette indices at load
time (`oracool/hud_art.cpp`), and that conversion can change how a piece looks in ways nothing in
an image editor will show. Every previous art pass in this project found that out by building,
launching, and looking. This closes that loop.

## The three things it mirrors from the engine

Accuracy here is the whole point; a preview that is merely plausible is worse than none.

1. **Matches only palette entries 128-255.** The global half is identical across town and every
   dungeon type, which is why one quantization pass works everywhere. Entries 0-127 are
   level-specific and colour-cycled and must never be matched against. Toggleable, but on by
   default because that is what HUD and UI art actually gets.
2. **Binary transparency.** The engine blits with `BlitFromSkipColorIndexZero` - index 0 is
   skipped and there is nothing in between. Soft alpha edges become hard ones. This is exactly the
   trap that produced the light band between the skill buttons and the orange fringe on the tab
   numerals; the alpha cutoff is a slider so the hard edge can be placed deliberately.
3. **Reduced light re-matches to the palette.** Rather than just darkening the output, each entry
   is scaled toward black and then re-matched to the nearest *available* entry, which is what the
   engine's light table does. This is what reveals colours collapsing onto each other in the dark
   - a failure that simple darkening would hide completely.

Note UI panels are drawn unlit, so for HUD and inventory art the lit preview is the one that
matters. The dark preview matters for anything drawn in the world.

## Palette

Loads `tools/town.pal`, staged beside the exe by the build script from the already-extracted
`Oracoo.MPQ/.../levels/towndata/town.pal`. Any `.pal` can be dropped on the window instead.

## Colour tools

Brightness, contrast, saturation, gamma and hue shift, applied *before* quantization and shown in
three panes: source-with-adjustments, converted at full light, converted at reduced light. Keeping
the adjusted source separate from the converted result is deliberate - it separates "what my edit
did" from "what the palette did to my edit", which are easy to confuse when tuning.

## Conversion report

Per image: dimensions, colours used, colours surviving at the current light level (flagged when it
collapses), transparent percentage, and the mean and worst RGB shift introduced by quantization.

## Verified

A headless harness ran the real shipped assets through the same code path:

| Asset | Colours | Mean shift | Worst shift | At 45% light | At 15% |
|---|---|---|---|---|---|
| `inventory_panel.png` | 41 | 9.1 | 34.4 | 18 | 8 |
| `inventory_tabs.png` | 69 | 19.4 | 60.0 | 34 | 12 |
| `health_orb.png` | 82 | 11.9 | 39.5 | 36 | 14 |

**Export round-trip is stable** on all three: exporting and re-quantizing gives `maxErr = 0` with
identical colour and transparency counts. That is the load-bearing guarantee - it means the
exported PNG is already palette-exact, the engine's own conversion is a no-op, and what the app
shows is what the game draws. Without it the previews would only be an estimate.

Two findings worth noting from that first run, both about art already shipped:

- The inventory panel resolves to only **41 distinct colours**. The stone texture is being crushed
  much harder than it looks.
- `inventory_tabs.png` has a worst-case shift of **60**, the highest of the three and right at the
  point where it is worth a second look. The gold numerals are the likely cause.

Neither is a defect, but neither was visible before this existed.

## Related

- [[2026-08-11 - Inventory Panel Geometry]]
