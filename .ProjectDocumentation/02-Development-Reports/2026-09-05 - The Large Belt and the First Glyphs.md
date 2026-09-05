# The Large Belt and the First Glyphs (v1.9.250)

**Date:** 2026-09-05 · **Request:** "apply these" (the Paladin Combat glyph draft and the large-belt HUD), then "apply this when done" (the matched-belt revision of the same HUD), then "switch these two colors" (Ready and Unspent).

## What shipped

### The HUD: `oracool-hud-v6-matched-belt`

GPT delivered the compact HUD twice in one evening: `-large-belt` (belt holes 28 to 34px, outline 2 to 4px) and `-matched-belt` (identical geometry, the belt's outline redrawn as a nine-slice of the skill wells' carved border). Their manifests differ only in the notes, so the second one went in directly and the first is filed beside it, never built.

`tools/CutHudPlate.ps1` was retargeted by its manifest: master 1740 wide, plate band 1059 master px (belt-shadow's 939 plus the 120 the belt grew), the right well and the mana orb 120 further right, belt holes 102 master (34 native) at a 105 (35) pitch from y 210, belt bar top 198. The header came out as:

| Field | Value |
|---|---|
| PlateSize | 353 x 108 |
| LmbWell / RmbWell | (6,46) / (291,46), 56 x 56 |
| BeltCellX | 72, 107, 142, 177, 212, 247 |
| BeltCellY / BeltCellSize / BeltBarTop | 70 / 34 x 34 / 66 |
| Orbs | unchanged from belt-shadow |

Potions (28px sprites) centre in the 34 cells through the existing centring in `DrawInvBelt`. The burger and portal icons went back to the 31px the user asked for on 2026-09-05 morning (`CutBeltButtonIcons.ps1 -Cell 31`); 31 fits a 34 cell without touching the 1px dividers.

### The first glyphs: Paladin Combat, eleven of 255

The draft in `Oracool.MPQ\03-concepts\skill-glyphs\paladin-combat-draft-01` is an approval delivery: its README holds the standalone 56x56 exports back. But its native contact sheet is the real buffers composited 1:1 on a flat grey, and every glyph pixel is exactly white (243) or shadow (12,7,7). `tools/ApplyPaladinGlyphDraft.ps1` lifts them back out by colour, stamps each onto vanilla spelicon frame 26 (the blank plate) and writes it into `ui\paladin_tree_icons.png` at its ClassTreeSkill frame. The recovered white and shadow pixel counts match the draft's own `pixel-checks.json` for all eleven, so the extraction is lossless.

Frames: Sacrifice 0, Smite 1, Holy Bolt 2, Zeal 3, Charge 4, Vengeance 5, Blessed Hammer 6, Conversion 7, Fist of the Heavens 8, Hammer of Faith 29, Blessed Shield 30 (the two appended rows).

**What this means on screen.** These eleven are the first tree icons that wear a plate, baked in. The other 38 Paladin frames and every other class are still the coloured set with no plate, and the baked plate takes no colour coding (the tint is applied to the vanilla plate draw, not to strip icons). Both are expected for a pilot; the proper route once the set is complete is a plate-less glyph strip with the plate drawn and tinted by `DrawIconOnPlate`.

### Ready and Unspent swapped

| Tint | Now | Was (one build, v1.9.249) |
|---|---|---|
| Ready | GOLD, the plate as painted | light grey |
| Unspent | light grey | GOLD |

One switch statement in `ApplyPlateTint`; the enum names did not move.

## Verification

Debug build clean; 624/625 with the standing `Drlg_l1.CreateL5Dungeon_diablo_3_844660068`. The eleven glyphs are pixel-verified against the draft's manifest. The HUD is a cutter rerun on a manifest whose geometry is a stated delta of the previous one; look at the belt dividers under the 31px icons and the plate seam at x 116/469 in the game.
