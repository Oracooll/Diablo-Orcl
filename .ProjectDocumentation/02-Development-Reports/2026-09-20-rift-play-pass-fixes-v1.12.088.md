---
date: 2026-09-20
version: 1.12.088
tags: [dev-report, rifts, play-pass, art]
---

# Rift play pass: six fixes (v1.12.088)

## The asks

> "Guardian rift portal is not purple and its core background is not purple either. fix it. When i click Leave button it closes Nephalem Rift if such is open. I dont want that. Leave is meant to close the Rift Monument UI. Nephalem rift stays open for 60 seconds after its boss has been killed. It cant be closed any other way. Only New Game is the option. There should be a countdown timer 60 to 0 seconds when the Boss has been killed. If the player is in the Nef Rift while countdown hits 0 player is teleported to new-game spawn location and the rift portal is now gone. Town portals are to be allowed in Guardian Rift. Diablo didn't spawn when i hit 100%. I reached 100% very fast. Make it require twice as many kills." And: "rift monument asset to be darker to match the rocks better."

## 1. The violet portal

**Cause:** every PNG missile sheet is quantised to the palette's shared half on load (`LoadPngMissileSheet` -> `NearestSharedIndex`), and that half has blue, gold, red and grey ramps but no violet - so the hue-shifted `portal_purple.png` landed on the blue ramp and drew blue, centre fill included.

**Fix:** the sheet is now built in vanilla's own BLUE (`BuildRiftPortals.ps1`, hue -1, a dark blue centre fill (0,0,60) that quantises into the same ramp), and `DrawMissilePrivate` draws `MissileID::RiftPortalPurple` on the 32-bit screen through `oracool::GuardianPortalRgbTable()` with `ClxDrawRgbMap`: the palette as it is, except the eight PAL8_BLUE entries (128-135) sent to a violet ramp. The green skill plate's mechanism, applied to a missile. Unlit on purpose.

## 2. Leave closes only the menu

`stonegate_menu.cpp`: the third row is "Leave" and does nothing but close the popup. No rift is closed from the menu any more (`CloseStonegate` stays for the code's own use).

## 3. The Nephalem closing clock

`RiftState::closeTicks`, set to `NephalemRiftCloseSeconds` (60) x 20 when a Nephalem guardian falls, counted down in `ProcessRift` wherever the hero is. The HUD label reads "cleared - closes in Ns". At zero: `CloseStonegate` (the portals come down, the rift ends, the closing sound) and, if the hero is still inside, `StartNewLvl(WM_DIABRETOWN, 0)` - the town's ENTRY_MAIN placement, the (57, 67) a new hero starts on. A Guardian Rift keeps its own clock and the way home as before.

## 4. Town portals in Guardian rifts

`RiftForbidsTownPortal` returns false everywhere (plan r9's refusal is history); the keystone's log line and the item's hover line no longer say "no town portal".

## 5. The guardian that did not rise

No single proven cause. Two changes: `SpawnRiftGuardian` searches out to forty tiles for open ground instead of twelve (a tight corridor could pass nothing within twelve), and `ProcessRift` logs "The guardian finds no ground to rise on near you - move to open floor" every five seconds while the bar is full and the spawn keeps failing, so a stuck spawn says so instead of looking like nothing.

## 6. Twice the kills

`PlaceRiftMonsters` places twice a floor's count (the cap at `MaxEnemyMonsters - 10` still holds). The bar stays 70% of the floor's total credit, so it now takes twice the kills and remains reachable; a share above 100% of one floor never could be.

## The monument: the user's own repaint

First darkened in code (brightness 0.42 against the earlier 0.78, checked against the user's screenshot of the rocks beside it); then the user repainted `ResourcesRift Monument.png` itself in blue slate ("use this new Rift Monument asset") and it is scaled AS PAINTED - no toning - into the one-frame CEL. `build_stonegate_cel.cmd`'s header records both.

## Build

v1.12.088: built clean twice (the code, then the repainted monument with its shadow and the 372 sprite width), ctest 831/831 both times.

## The monument, second pass: the user's repaint, the rocks' slate, a cast shadow

Mid-build the user replaced `Resources\Rift Monument.png` with a blue-slate repaint ("use this new Rift Monument asset"), then asked to "match it to the rocks colours" and to "draw a black shadow cast east-northeast from it. to look more natural".

- **Colour:** the repaint at brightness 0.7, cast fully onto the rocks' own dark slate (57,65,95 - the hue of the dark end of the town palette's blue-grey ramp, so the nearest-colour quantiser lands cleanly, 244 distinct colours in the CEL and no speckle).
- **Shadow:** a new pass in `ScalePainting.ps1` (`-ShadowLength 0.9 -ShadowRise 0.25 -ShadowBaseline 12`): every opaque pixel h above the ground line lands 0.9h to the right and 0.25h up, as an opaque near-black pixel (a translucent shadow would vanish at the CEL's alpha cut, and index 0 is transparent). Drawn under the painting. The canvas is padded 122px a side so the painting stays centred on its tile; `OracoolStonegateAnimWidth` is 372. Both the monument and the arch over the town portal share the frame.
- `build_stonegate_cel.cmd`'s header carries the whole recipe.
