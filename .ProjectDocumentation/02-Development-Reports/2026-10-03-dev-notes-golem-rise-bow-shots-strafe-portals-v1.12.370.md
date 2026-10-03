# 2026-10-03 - Dev notes: golem rise, bow skills shot, Strafe spin, arrow sizes, portal shadows (v1.12.370)

**Date:** 2026-10-03. Debug only. All 937 tests pass. The Debug `diablo.ini` was unchanged by ctest (md5 7d1fdbaa, new since the user's play). oracool.mpq repacked. Commit 1eb87c33.

Seven `/dev` notes, all logged at v1.12.369, all done and archived in `development-archive.md`.

## Golems: the rise and the still stand
- **Rise:** `SummonMinion` puts every golem into `M_StartRise` (new; `StartSpecialStand`). That plays the vanilla golem's build-up, `golems.cl2`: 20 frames, 1 s, a single list that vanilla only ever played facing south.
- **The Fire Golem:**
  - `GetMinionAnim` returns nothing for `Special`, so it rises in the golem sheet.
  - `MinionBodyLook` tints it `hue::FireOrange` during `SpecialStand`.
  - `OnMinionRisen` (called from `MonsterSpecialStand`) lights the Apocalypse flare as the rise ends, then it binds the Elemental. The flare no longer plays at summon.
- **Re-forming on a new floor** has no rise.
- **Still stand:** `GolemStandFrame` = 4 of the 16-frame walk (both feet planted; picked from the enlarged strip). `M_StartStand` sets it, and `ProcessMonsters` skips `processAnimation` for a standing golem with `GolemHoldsStillFrame`. That covers every golem except the Fire Golem, whose flames keep running, and companions.

## Bow skills
- **The check:** the ten Round-3 arrow skills were already shots. The eight RfA-12 bow skills rode `MissileID::Warcry` as casts with the magic animation.
- **The change:** `CheckPlrSpell` now arms `ArmBowSkillSpell` (rogue_arrows) and sends `CMD_RATTACKID`/`XY`. `DoRangeAttack` calls `CastSpell` at the first release frame with `executedSpell` set, so price, cooldown and cues are the cast's. The bow sound plays.
- **Latches:** `ArmArrowSkill(anything)` clears the new latch.
- **Can't cast:** a skill it can't pay for, or one still cooling under a held button, drops the latch and looses a plain arrow.
- **New function:** `IsRfa12BowSkill` is exported.

## Strafe as a spin
- **Where:** `oracool/whirlwind` grew a second spin kind (`IsStrafing`, `IsSpinning`, `StartStrafe`). `IsWhirlwinding` stays Whirlwind-only, because its hooks are melee shares.
- **How it plays:**
  - Held on whichever button it is readied on (`_pLRSpell` or `_pRSpell`).
  - She glides toward the cursor at run speed; `IsSpinning` feeds both run-speed checks.
  - She is drawn with the bow attack sheet's release frame, its facing turning each tick.
- **Shots:** `LooseStrafeShot` every `StrafeShotTicks` = 5. It aims at an enemy in view (8 tiles, clear line) `StrafeAimPercent` = 60 +5/rank, max 95; otherwise it fires 5 tiles in a random direction.
- **Mana and stopping:** Strafe's mana cost is paid at the start and each second. The spin stops when she can't pay, on button release, on a bow swap, in a window, or on death.
- **From a hotkey** it is still the old volley.
- **Text:** the description and the facts lines are rewritten.

## Arrow sizes
- **New tool:** `tools/ScaleMissileSheet.ps1` scales each cell about its centre.
- `magic_arrow.png` at 50%, `guided_arrow.png` at 62%. Both are now 38 px long, like `frost_arrow.png`.

## Portal shadows
- **The shadow:** the dungeon blue portal is the vanilla CL2. In play it draws a solid black ground shadow at the lower left. The exported strip the other sheets come from had lost it (the index read as clear).
- **The export:** `DISABLED_Dev370Looks` writes `portal_cl2_frames.png` (the CL2 as drawn). It is copied to `Resources/01. Blizzard Assets/Animated Items 2/`.
- **`BuildRiftPortals.ps1`:**
  - `ReadGroundShadow` takes the shadow as the CL2's pure-black pixels outside the strip's row span; the CL2 sits 1 px left.
  - `ApplyGroundShadow` lays it after the centre fill.
  - The rim's near-black edge is now kept exactly. The gold's old half-grey treatment is gone.
- **The in-town portal** (`portal_town.png`) gets its shadow back. It was stripped on 2026-09-20 at the user's word; the new note asks for every portal.
- **The red portal:** new `rportal.png` (`CopyShadowOntoRed`). It is the red strip with the blue's shadow pixels and a black centre, and it replaces the CL2 through misdat's PNG-wins rule.
- **The Guardian recolour:** `GuardianPortalRgbTable` passes the shadow colours and pure black through, so the shadow stays black.

## Rain of Arrows brief
- `Resources/02. Oracooll Assets/ChatGPT RfA/RfA-61 - Rain of Arrows - one falling arrow.md`: one 24x72 still, tip down at (12,69), with a dithered motion streak. It delivers to `delivered-packs/batch-90-falling-arrow`.
- Nothing is wired yet. The multiplying code gets built when the sheet arrives.

## Tests
- `OracoolDev370.AGolemRisesWhenSummonedAndThenStandsStill`, `TheEightBowSkillsAreShotOnALatch`, `StrafeShootsEveryFiveTicksMostlyAtEnemies`.
- The Fire Golem test (v1.12.364) now expects the rise first, then the flare.
- The preview `OracoolPreview.DISABLED_Dev370Looks` (env `ORCL_DEV370_OUT`) draws the clay and molten rise strips with the stand frame, the five portals, the three arrows, and the CL2 portal export.

## To check in play
- Golem rise facing (the single south-facing list plays whichever way it faces), and the molten-to-flame hand-off.
- Strafe: turn speed, shot rate and mana per second; the spin on the left button.
- Bow skills: Rain of Arrows / Phantom Volley timing at the release frame.
- Portal shadows on real floors, the in-town portal inside the Rift Monument's arch, and the red portal's colours now that it draws true colour.
- Not done: the other new arrow sheets (barbed 71px, piercing 74, crippling 66, phantom 71, shock 58, barrage 44) are also longer than 38px. Only Magic and Guided were asked for.

## v1.12.371: every new arrow at 38 px (commit 8bf0bb91)
The user: "align their size to other arrows size." `tools/ScaleMissileSheet.ps1` on the other six arrow sheets:
- barbed 54%, piercing 51%, crippling 58%, phantom 54%, shock 66%, barrage 86%.
- All nine arrow sheets now measure 38-39 px. 937/937; ini unchanged.

## v1.12.372: Rain of Arrows' falling arrow (commit eef19276)
- **The delivery:** RfA-61 arrived in `delivered-packs/batch-90-falling-arrow`. Checked: 24x72, binary alpha, 281 colours, tip at (12,69).
- **Registered as** `missiles/falling_arrow.png`, `MissileGraphicID::FallingArrow` (animWidth 24, animWidth2 -20).
- **`AddFallingArrows`** (missiles.cpp) places 16 art-effect copies on random tiles within the skill's reach. Each one:
  - sets off at its own moment over 16 ticks, out of sight (offset -4000) until then;
  - falls 192 px in 6 ticks onto its tile with a ±10/±5 px scatter;
  - stands 2 ticks, then goes.
- **Rain of Arrows** uses it; the old volley sheet is the fallback without the art. The damage is unchanged and still lands at the cast, not with the arrows.
- **Test:** `OracoolDev372.RainOfArrowsDropsTheOneArrowMultiplied`. 938/938; ini unchanged.
- **Not done:** the arrows have no floor shadow, though the brief told ChatGPT the game would add one.
- **To check in play:** the fall speed and spread, and walls drawn over a falling arrow (it is drawn in its landing tile's pass).
