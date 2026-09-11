# RfA-02 applied: charm icons, shop plates, Paladin sounds (v1.11.030)

**Date:** 2026-09-11
**Branch:** renderer-32bit, local commits only
**Request:** `Resources\ChatGPT RfA\RfA-02 - Charms, Shop, Paladin Sounds.md`, batches 5-7, under the Gold asset loop

All three batches arrived together. Each passed its checks: exact canvas, alpha only 0 or 255, WAVs mono 22050 Hz 16-bit and inside their duration ranges, and every preview viewed.

## Batch 5: charm icons

Vigor, Embers, Storms, Fortune, Luck and Greed have their own icons, replacing the vanilla Blood Stone and Magic Rock.

**The item icon sheet can no longer be rebuilt from scratch as-is.** `tools/build_item_icons.cmd` reads generated inputs (growing charms, encounter items, item sets and more) from `%TEMP%` folders that have since been cleared. So the six frames were built alone with the same tool, in `asis` mode, from the delivered 28x28 icons. They were then **appended** to the existing sheet by a header-only rewrite, 495 to 501 frames. Every existing frame is byte-identical, so no icon ID moved.

Wiring:
- ICURS ids go in `oracool/charm_icons_curs.inc`, after the encounter items.
- Widths and heights go in `charm_icons_curs_widths.inc` / `_heights.inc`. cursor.cpp mixes line endings, so its includes were inserted byte-exact.
- `ICURS_ORACOOL_LAST` moved to `ICURS_ORACOOL_CHARM_GREED`, and the six itemdat rows point at the new ids.
- `charm_icons_icon_specs.txt` is added to the tool, with the art filed under `Resources\02-source-art\items\charms`, so a full rebuild includes them once the other generators have re-made their Temp inputs.

## Batch 6: shop controls

An agent wired it; I reviewed it.
- **Tabs** show idle, hover or active, active being the current screen.
- **Service and action buttons** show idle, hover or pressed. Pressed follows the engine's `sgbMouseDown` and moves the label down 1 px.
- **The gold line** sits on its recessed strip.
- **Fallback:** without the PNGs, the old flat fill and ornate border draw exactly as before.
- **Size checks:** static_asserts hold each plate to its rect.
- **Narrow buttons:** Griswold's three service buttons are 94 px wide, so they take the plate's left and right halves butted at the middle, 1:1 with no scaling. That seam needs a look in a screenshot.
- **New helper:** `DrawLoosePngPart` in hud_art.

## Batch 7: Paladin sounds

The 2026-09-03 rule applies: one sound per moment, replacing a sound, never stacked on one.
- **Hammer of Faith:** its cast cue plays **instead of** the swing whoosh (`PS_SWING`), only for a swing that carries an affordable Hammer of Faith (`PlayArmedSwingCue`). Its impact cue plays once per splash, after the mana is spent.
- **Blessed Shield:** its cast cue plays **instead of** the borrowed `IS_CAST2` on the throw (`PlayPaladinMissileSound`, in AddMissile's launch hook beside the cold one). Its impact cue plays at the burst, where nothing sounded before.

The rows come from `tools/skill_sounds_extra.csv`, in the manifest's own columns, beside the generator. The package zip stays as delivered. **The generator had not been able to run since 2026-08-25:**
1. The Diablo III passive page's "Fanaticism" row shares its name with the aura. The generator now keeps the first row's sounds and prints a warning instead of throwing.
2. The Paladin Holy Bolt row was removed on 2026-09-06, which left its package sounds unmatched. They are now on an explicit `$retired` list, so any other orphan is still an error.

The regenerated table differs from the committed one by exactly the four new rows. Its header now counts 306 and names the source path correctly.

## Verification

Debug build, ctest **699/699**. Release built. Both trees repacked. RTM refreshed with exe 1.11.030. Packages filed under `02-source-art\delivered-packs`.

Not seen or heard in play:
1. The charm icons in the backpack.
2. The shop tabs and buttons, especially the seam on Griswold's service row.
3. Hammer of Faith and Blessed Shield in combat.
