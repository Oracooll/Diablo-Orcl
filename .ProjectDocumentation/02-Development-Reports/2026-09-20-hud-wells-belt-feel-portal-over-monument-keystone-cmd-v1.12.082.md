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
