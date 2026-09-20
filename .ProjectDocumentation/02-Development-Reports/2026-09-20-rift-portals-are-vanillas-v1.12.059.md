# 2026-09-20 - The rift portals are vanilla's, recoloured; the Stonegate moves beside the town portal (v1.12.059)

**Date:** 2026-09-20 (morning) - Debug only - green: 821/821 (build 32). User: "for nephalem and guardian
rifts i want to use vanilla portal animation. rfa for a monument asset that will fit vanilla portal animation in.
animation will be recoloured according to the colors i specified earlier. this monument to be next to the default
town portal spawning location in tristram, just a couple or three tiles southeast of it, to avoid overlapping."

## What was found

The exported vanilla strip `Resources/00-original-game-art/missiles/portal.png` is 1536 x 256. RfA-19 read it as
sixteen frames of 96 x 256 and sized the monument's opening (72 x 236) to a 55 x 220 oval. It is TWO ROWS of
96 x 128: the exporter writes one row per file, and the town portal has two files - `portal1.cel`, the opening
blossom (a line widening into the oval), and `portal2.cel`, the standing loop that `ProcessTownPortal` switches
to by `SetMissDir(missile, 1)` once its countdown reaches `var1`. The standing portal's oval is **56 x 92**
(x 19..74, y 23..114 of the 128-tall frame), its bottom 13 px above the ground point. The batch-42 monument is
three times too tall for it (composite in the session's scratchpad, sent to the user).

## What changed

- `tools/BuildRiftPortals.ps1`: reads the vanilla strip and writes `missiles/portal_gold.png` (hue 42, +25%
  saturation) and `missiles/portal_purple.png` (hue 278, +15%) into packaging - every coloured pixel takes the
  target hue and keeps its lightness and alpha; greys, whites and blacks are untouched, so the animation is
  vanilla's to the frame. Done in RGB on the 32-bit renderer because the palette has no purple ramp. GPT's
  painted portals from batch 42 are replaced in packaging (they stay in the delivery folder).
- `misdat.cpp`: the PNG sheet loader now takes `animFAmt` rows (it was `1 : 16`), which is what the exporter
  writes - one row per file or direction; the two rift rows have `animFAmt = 2`.
- `missiles.cpp`: `ProcessRiftPortal` switches from file 0 to file 1 on the opening's last frame, so a rift
  portal blossoms open like the town portal and then stands.
- `oracool/stonegate.cpp`: the gate's first candidate tile is (60, 40) - `WarpDrop[0]` in portal.cpp is
  (57, 40) and south-east is +x on this map - then (60,41), (61,41), (60,39), (62,41), (62,42); the other players'
  portal slots (59,40), (61,40), (63,40) are avoided.
- `Resources/ChatGPT RfA/RfA-21 - The Stonegate, second cut.md` (+ standing instruction): the monument only,
  frames 128 x 160, an opening ellipse 66 x 102 centred with its bottom 9 px above the frame bottom (the portal
  then sits inside with a 5 px margin), seventeen frames as before, baked shadows as the user asked; delivery
  `batch-44-stonegate-second-cut/`. When it lands: `OracoolStonegateAnimWidth` 192 -> 128, rebuild
  `orclgate.cel` with tools/build_stonegate_cel.cmd (check its frame size), repack.

## Also

- Build 31 died on LNK1168: a windowless `DiabloOrcl.exe` from 05:02 (the game does not exit cleanly - roadmap
  card) was stopped and the build re-run as build 32.
- The batch-43/44 delivery sweep runs every five minutes (43d sounds + RfA-20 report, batch-44 + RfA-21 report).

## v1.12.060 - batch 44 applied, minutes later

RfA-21's package arrived at 09:49: seventeen 128 x 160 frames, the opening exactly x 31..96, y 49..150, plus
the guide, the preview and a validation.json. GPT derived it from the batch-42 painting compressed to the corrected
scale rather than repainting, and its preview composites the real standing portal in the opening: it fits with the
margins the brief asked for. `tools/build_stonegate_cel.cmd` now reads batch 44 and wrote `orclgate.cel`
(17 frames, 128x160, 117 KB); `OracoolStonegateAnimWidth` is 128. Build 33 green: 821/821.
