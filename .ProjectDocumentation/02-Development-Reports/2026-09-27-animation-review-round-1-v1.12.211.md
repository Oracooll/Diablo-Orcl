# 2026-09-27 - The animation review's first round, in the game (v1.12.211)

**Date:** 2026-09-27. Debug only. The user reviewed every ChatGPT effect sheet on the ChatGPT Animation Review page and said "go ahead with both":
- build what they approved;
- write round-2 briefs for what they sent back.

The round-2 briefs are RfA-52 to RfA-60 (in `Resources/02. Oracooll Assets/ChatGPT RfA`). Their deliveries go onto the page's RfA tabs for review. None of them is in this build.

## Effect sheets draw in true colour

Until now, `LoadPngMissileSheet` quantised every PNG effect sheet to the town palette:
- no sheet could be green;
- a sheet's gradients banded.

**The new path:**
- **Loading:** `LoadPngMissileSheetColoured` (`sprite_import.cpp`) keeps each sheet's own table of up to 255 colours (`SpriteColours`), the same way the dyed hero sheets have kept theirs since v1.12.207. `MissileFileData::colours` holds it.
- **Carrying it:** `Missile::oracoolColours` carries the table from `SetMissAnim`. Every site that makes a missile borrow other sprites clears it:
  - an item tumble;
  - a monster's missile;
  - the Rhino's charge.
- **Drawing:** `DrawMissilePrivate` draws through the sheet's table on the 32-bit screen and falls back to its indices on the 8-bit one. The hero overlays (`DrawPlayerIconHelper`) do the same.
- **Tints:** a palette translation (TRN) would scramble a sheet's own indices. Tints are therefore a table-to-table recolour, `oracool/missile_tint`:
  - `Hue`;
  - `HueCycle`;
  - `Earthquake`;
  - `Ice`;
  - `Astral`;
  - `Mend`.
- **Light level 0:** `LitPaletteTable(0)` is now the palette itself. Level 0's light table sends white to black, which `ClxDrawLight` avoids by skipping it.

## Approved: 39 sheets

The 39 approved sheets are copied into `Packaging/resources/oracool_assets/missiles`, including the one-frame `mantra_of_retribution.png`. Its `misdat` row is now `AnimLen_1`.

## "Other": vanilla art and colour, in place of a sheet

| Skill | Now |
|---|---|
| Seismic Slam | Three Fire Wall flames, tinted gold, rolling out to the slam's reach (`GoldenFlameWave`). |
| Ember Mine | Goes off as Apocalypse's explosion. |
| Flame Ring | Fire Wall flames on every open tile of the ring (`RingOfFireWall`). |
| Army of the Dead | Three green skeletons a pulse charge in from five tiles out and burst into green bone (`ArmyCharge`, `AddCreatureBolt`). They use a skeleton type loaded on the level; where none is loaded, the bone bursts appear alone. |
| Death Nova | Vanilla's Flash, recoloured to the blight's yellow-green, split into halves that draw behind and in front of the hero. |
| Earthquake | The quake's cracked ground runs brown to dark orange and back, with bands rolling through it. |
| Astral Projection | A pale violet shimmer on the hero, instead of the overlay (`DrawPlayerTinted`). |
| Ice armours | A cold blue glaze on the hero instead of the shell, and no break sheet when the armour ends. Its stop sound still plays. |
| Dark Mending | A lavender glow fades off each healed minion over a second (`MinionMendGlow`, drawn in `DrawMonster`). |
| Serenity | An aura ring (`aura_cleansing.png`) rises from the feet over the head and sinks back over two seconds, colour-cycled. It is drawn in two halves, the far one behind the hero and the near one in front. |
| Mantra of Retribution | The approved still, colour-cycled round its ring, drawn in two halves the same way. |
| Bone Armor, Frenzy of the Dead | Kept as they were. |

**Also from the first review:**
- **Frost Nova:** the ring is drawn at 250%, so it covers its three tiles (`ScaleFrostNova`, built once like Blessed Shield's flash).
- **War-cry ring:** tinted for the skill that raised it and colour-cycled as it grows (`RingHueForSkill`, `Tint::HueCycle`).

**New code:**
- **`oracool/cycled_still`:** draws one PNG colour-cycled, either round it or up and down it. It uses the same two bands, 2.4 s lap and 35% swing as the aura rings, and can draw a picture's back or front half.
- **Arrival tint:** an art bolt's arrival sheet now wears the bolt's tint.

## Round 2 on the review page

All nine (RfA-52 to RfA-60) arrived and are on their tabs, waiting for the user's approval.

Nine directional sheets are wound the wrong way:
- anchor_javelin;
- ice_needle;
- ice_lance;
- bone_spear;
- ride_the_lightning;
- aegis_slam;
- sweep_arc;
- dragons_wrath_wave;
- heaven_splitter_wave.

In each, row 0 faces East and the rows run counter-clockwise; the game starts at South and runs clockwise. It is one consistent mirror (row k takes row (12-k) mod 16), and the cards say so.

The earlier note on aegis_slam and sweep_arc called this a quarter turn. That was wrong, and the note is corrected.

## Test

- **Build:** Debug built clean.
- **ctest:** 878 of 878 passed; 14 are disabled previews and one timedemo is skipped.
- **oracool.mpq:** repacked with the 39 sheets.
- **Build trouble:** the first run stopped at a missing forward declaration (`LineEnd`). A rerun started while that run was still compiling (`-k 0`), and its orphaned test processes held `libdevilutionx_so.dll` (LNK1168). Everything was stopped and built once, cleanly.
