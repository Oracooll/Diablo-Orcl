---
date: 2026-09-20
version: 1.12.082
tags: [dev-report, hud, ui, buttons, sound, rifts, debug]
---

# HUD wells and belt feel, portal over the monument, keystone command (v1.12.082)

## The asks

> "apply titlemov.wav on every hover over lmb/rmb/belt icons. lmb/rmb to adopt the 2,2 click relocation when clicked. (is it 2,2 or 2,-2 or -2,-2?) when rift portal is opened render it over the Rift monument, not behind. i need a debug command for Guardian Rift keys."

## 1. HUD wells and belt (`Source/oracool/attack_skills.cpp/.h`)

- **Hover sound:** `TrackHudButtonHover` runs once per frame from `DrawLmbSkillWell` (the HUD's own draw). It names the button under the cursor - LMB well, RMB well, or any of the seven belt cells (`BeltVisibleSlotCount`) - and plays the UI move sound on the frame that changes to a button that was not under the cursor last frame.
- **Press sink:** `PressHudWell(leftWell)` is called from the LMB well's click branch in diablo.cpp and from `DoPanBtn` (the RMB well, also a left click) before the shift-clear and the picker; `ReleaseHudWells` from both mouse buttons' release paths. While held, the well's icon and badges draw at the net rect shifted by the sink; the painted socket stays.
- **The answer to "2,2 or 2,-2 or -2,-2":** in screen coordinates, where y grows downward, "2px down and 2px left" is `Displacement { -2, +2 }` - x minus two, y plus two. The same constant the Act buttons and the abilities window use.

## 2. The portal over the Rift Monument (`Source/oracool/stonegate.cpp`)

The gate object now sets `_oPreFlag = true`, which moves it to the tile's before-characters draw pass; the portal missile on the same tile is drawn in the post pass, so it lands over the painting. v1.12.074's note about the monument hiding the portal's sides is reversed by this: the whole 86px portal shows.

## 3. The `keystone` debug command (`Source/debug.cpp`, `Source/oracool/rift.cpp/.h`)

`keystone [tier]` drops a Guardian Keystone at the hero's feet - of the given tier, or of the deepest floor's tier when none is given (what a Nephalem Rift would open at). It goes through the guardian's own drop path (`DropGuardianKeystone`, the exported face of `DropKeystone`), so it tumbles and is picked up like the real thing. The Orcl Debug Console page gains `rift` (missing since v1.12.062) and `keystone`: 71 commands, 29 Orcl.

## Build

Build 62, v1.12.082: clean, ctest 831/831. The Orcl Debug Console page republished (version 3).

## v1.12.083 - the monument moves to (31, 56); an arch over the town portal

> "Move the Rift Monument to 31:56 tile. Also Place one inactive copy of its asset on the tile the vanilla portal opens."

- **The gate** now tries (31, 56) first, then the tiles around it, with the same solid-tile and entry-tile checks as before (the entry tile is still gate + (1, 1); a fallback is logged in red).
- **The arch** (`AddPortalArch`, stonegate.cpp): a second OBJ_STAND wearing the same painting beside `WarpDrop[0]` = (57, 40), where `AddWarpMissile` stands the town portal - so the town portal opens inside an arch. v1.12.084: one tile SOUTH-EAST of the portal, (58, 41), after the user's grid screenshot showed the portal standing in the plinth on the shared tile (both sprites bottom-anchored; the portal's ink ends 14px above the anchor, the opening floor ~39px); (+1, +1) is the grid's pure 32px vertical step. Drawn in the FLOOR pass (`IsFloorPassObject`, with the waypoint platform) so the portal on the earlier tile still draws over it - "we dont move the portal. we move the monument. we keep the portal overlapping the monument." Unselectable (no hover, click or name), not solid (the hero walks into the portal on that tile and lands one tile past it), missiles pass (`_oMissFlag`), before-characters pass so the portal and the hero draw over it. `IsStonegateObject` answers for it too, which keeps `IsLevskiRoarObject` from taking it for the Cube. portal.h gains `TownPortalLandingTile(i)`, the accessor for the table.

Build 63 failed (C2872: `WarpDrop` is in portal.cpp's anonymous namespace, so an extern in the header clashed); build 64 with a `TownPortalLandingTile(i)` accessor instead: clean, ctest 831/831.

## v1.12.085 - the monument toned to worn grey stone

> "make the monument asset less bright and more worn-down stone grey-ish."

`tools/ScalePainting.ps1` gained `-Saturation`, `-Brightness` and `-CoolCast` (applied to the resampled frame alone; defaults leave a painting as painted). The Rift Monument is rebuilt at saturation 0.45, brightness 0.78 and a 4% cool cast, then through `build_stonegate_cel.cmd` as before (the script's header records the numbers). Checked against the town-palette preview: the warm tan is gone, the blocks read as weathered grey with the moss and dirt kept.

Build 66, v1.12.085: clean, ctest 831/831; the toned orclgate.cel packed.

## v1.12.086 - the town's own portal at 90% without its shadow; the monument in Tristram's blue-grey

> "we need to shrink the town portal asset in-town only, not in dungeons to 90% and remove its shadow. also try to recolour the rift monument to match the rocks scattered all over Tristram. they are very blue-ish."

- **The town portal, in town only:** a third sheet from `tools/BuildRiftPortals.ps1`, `missiles\portal_town.png` - vanilla's colours (hue -1 skips the shift), no centre fill, the ground shadow stripped, resampled to 90% like the rift portals. The shadow is the desaturated blue-grey pixels under the oval's foot (25,30,45 / 88,99,141 / 67,76,111 / 78,88,125 / 13,17,27) in the frame's bottom band (y >= 90 of 128); the ring's blues and its near-black rim shading are untouched - checked on a 4x preview. New `MissileGraphicID::TownPortalInTown` (86 / 11, PngOnly, two rows); `AddTownPortal`'s town branch switches the missile onto it with `SetMissAnim`, and since `SetMissDir` keeps the graphic, the opening-to-standing switch and `AddWarpMissile`'s sync stay on the sheet. The dungeon-side portal keeps vanilla's CL2.
- **The monument's colour:** `ScalePainting.ps1` gains `-TintRgb "R,G,B" -TintStrength` (a luminance-keeping cast). A first try at 70% toward 118,132,172 speckled blue in the town-palette preview: the global half has one blue-grey ramp (entries 178-188, 159,165,198 down to 37,43,65 - Tristram's rock tones) and a cast between it and the neutral greys made the nearest-colour quantiser alternate. So the cast is 100% toward 121,127,160 (entry 181's hue), which puts every stone pixel on that ramp's line; saturation 0.45 and brightness 0.78 as before.

Build 67, v1.12.086: clean, ctest 831/831; portal_town.png and the retoned orclgate.cel packed.
