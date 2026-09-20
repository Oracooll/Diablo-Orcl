---
date: 2026-09-20
version: 1.12.071
tags: [dev-report, waypoints, ui, act-buttons]
---

# Waypoint Act buttons (v1.12.071)

## The ask

> "I want to redesign the waypoints list a bit: i want to introduce three buttons to act as Acts Waypoints: Diablo Act button - hosts Diablo dungeons waypoints; Hellfire Act button - hosts Hellfire dungeons waypoints; Orcl Act button - Hosts future Orcl introduced areas waypoints. ChatGPT already made the glyphs - Resources\ChatGPT RfA\act-glyphs. You need to place three buttons and draw the glyphs on top. By buttons i mean the same buttons as in the abilities windows. the 56x56 button backing + framing + shadow + hover shadow. When an Act button is clicked recolour the backing into Red for Diablo Act, Orange for Hellfire Act and Purple for Orcl Act."

## What was built

### The buttons (`Source/oracool/waypoint_menu.cpp`)

Three 56x56 cells in a row between the "WAYPOINT" title and the list, each drawn with the abilities window's exact recipe:

1. `DrawDropShadow` (3px cast) at rest, `DrawHoverShadow` (6px, doubled) under the cursor. The selected button keeps the resting shadow - it is pressed.
2. `DrawGridBezel` - the carved 2x2 slot frame, 6px out on every side.
3. `DrawPlateIn` - the spell plate as the backing: light grey (`Unspent`) at rest, white under the cursor, and the act's colour while selected.
4. The ChatGPT label glyph (96x56) centred over the cell, overhanging the frame by 20px a side in transparent air. Falls back to the act name in FontSize12 if the PNG is missing.

Geometry: cells at x 30 / 142 / 254 (112px pitch, one cell of gap between cells), cell top at y 80 (title band ends at 66, 8px clearance for the bezel). The list top moved from 87 to 160 and the viewport dropped from twelve rows to ten (448px, ending at 608, still above the frieze at 625). The hit box is the cell plus its bezel.

### The colours

- Diablo Act: `SkillPlateTint::Blocked` - the existing red plate (`SetSpellTransRed`, the palette's own PAL16_RED ramp).
- Hellfire Act: new `SkillPlateTint::Orange` - `SetSpellTransOrange`, a warm amber ramp as colour VALUES (the palette's PAL16_ORANGE is what the gold plate already shades with, so mapping onto it read as dirty gold).
- Orcl Act: new `SkillPlateTint::Purple` - `SetSpellTransPurple`, values again; the palette has no purple, PAL16_BLUE stands in on an indexed surface.

Both new tints go through a new file-local `SetSpellTransValueRamp(ramp, fallback16)` in `Source/panels/spell_icons.cpp`, which is the green's mechanism (the `SplGreenOverride` value table) generalised to any eight-shade ramp. The green itself is untouched.

### The lists

One table per act, replacing the single depth-interleaved list of 2026-09-12:

| Act | Rows | Levels |
|---|---|---|
| Diablo | 17 | Tristram, 1-16 |
| Hellfire | 9 | Tristram, 17-24 (Nest, Crypt) |
| Orcl | 1 | Tristram - "No Orcl areas yet" under it |

Tristram heads every act so no tab is a one-way trip. Outside a Hellfire game the Hellfire act is Tristram alone with "Requires Hellfire" under it. Within an act, depth and dungeon level agree again, so the Nest no longer interleaves with the Caves.

Opening the menu from a dungeon sigil selects that level's act (the Crypt opens the Hellfire tab); in town the last chosen act stays. A click on an Act button switches the list from the top with the UI move sound and keeps the menu open. `ResetWaypointMenuForNewGame` returns the tab to Diablo.

New header API for the tests and callers: `WaypointAct`, `ActiveWaypointAct`, `SelectWaypointAct`, `WaypointActRowCount`, `WaypointActLevelAt`, `WaypointActOfLevel`.

### Assets

`Resources/ChatGPT RfA/act-glyphs/{diablo,hellfire,orcl}-act.png` copied to `Packaging/resources/oracool_assets/ui/act_{diablo,hellfire,orcl}.png` and filed under `Resources/01-in-use-assets/ui/act-glyphs/`. The 288x56 strip is filed, not shipped.

### Tests (`test/oracool_audit_test.cpp`)

- `OracoolWaypointActs.EveryDungeonLevelSitsOnExactlyOneAct` - row counts, Tristram heading each act, every level 1-24 listed exactly once on the act `WaypointActOfLevel` names, the Hellfire act collapsing outside Hellfire.
- `OracoolWaypointActs.SelectingAnActShowsItsListAndANewGameReturnsToDiablo`.

## Build

Build 51, v1.12.071: configure, build and link clean; ctest 831/831 passed (the two new act tests included). The act glyphs went into oracool.mpq on this build's pack. Awaits the user's look: the three buttons under the title, red/orange/purple when pressed, the hover shadow, and the ten-row list under them.

## v1.12.072 - three fixes from the user's look

1. **The Act backings were the plate's natural 37x38, not 56x56** ("now i think you are using small backings around 28x28"). `DrawSmallSpellIconCoveringClipped` capped its scale at 100%, so a cell larger than the plate got the plate centred and unfilled. The cap is now ScaleClxList's own 400%, so `DrawPlateIn` covers any cell in both directions. The HUD menu (37x38) and the inventory tabs (28x28) are at or under 100% and are unchanged.
2. **The Stonegate keeps its plain painted frame in every state** ("use uncut version of the stonegate asset and just overlap it with portal asset when user selects one of the portals"). The sixteen baked gold/violet glow frames of batch 45 stay in `orclgate.cel` but are never selected; an open rift is shown by the portal missile alone, standing in the arch. `ProcessStonegate` no longer animates and pins frame 1. No asset rebuild. There is no separate uncut master on disk - batch 45 delivered the seventeen cut frames only - so "uncut" here means the closed painting without the lit variants; if a master painting exists it can replace frame 1 through `build_stonegate_cel.cmd`.
3. **The rift kill bar and clock sit under the mini-map** ("put these under the minimap, not next to it"): the map's own width, label centred, bar under it, 4px below the map's frame. Hidden while the event log is open, since the log takes that column.

## v1.12.073 - the Act buttons' second cut

Three changes from the user's look:

1. **Backing = vanilla's spell plate frame 26** (`Resources/00-original-game-art/spelicon/spelicon_frame26.png`, 56x56) shipped as `ui\act_plate.png`, drawn through the new `DrawLoosePngScaledTo`. The tinted 37x38 spell plate is only the fallback when the file is missing.
2. **Selected acts keep the hover shadow** - the doubled 6px cast stays under the pressed button, so it reads lifted. The selected plate is the same frame recoloured with `TintRectRgb` (red 0xC82828 / orange 0xE88020 / purple 0x8A3FC8, floor 20%); the Orange and Purple `SkillPlateTint`s from v1.12.071 remain for the fallback path.
3. **The cell is stretched a quarter to 70x70.** Measured ink of the labels: Diablo 56px, Hellfire 66px, Orcl 38px wide, all 37px tall, so the 56 cell cut "Hellfire Act" and 70 clears it by two pixels a side (a static_assert pins that). Cells at x 30 / 135 / 240, list top at 174, still ten rows ending at 622. The 2x2 bezel has no 70px member, so `DrawGridBezelScaledTo` (new, hud_art) resamples the 56 frame's art whole to 82x82 and the plate covers its interior.

Build 53, v1.12.073: clean, ctest 831/831; act_plate.png packed.
