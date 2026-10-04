# 2026-10-04 - Rogue Skill Cards notes (v1.12.375)

**Date:** 2026-10-04. Debug only. All 940 tests pass. The Debug `diablo.ini` was unchanged by ctest (md5 b3392ce4, since the user's play). Commit 4673db13. No asset changed, so no MPQ repack.

The source is the Rogue Skill Cards page, db `picks`: 54 docs, the first Rogue notes ever applied. The snapshot is in scratchpad `cards/db375/rogue` and is copied to `cards/db/rogue` as the applied state.

## Arrows (rogue_arrows `RogueArrowLookOf`, rfa12 `FlyArrow`)
- **Round-3 arrows:** every one is the vanilla arrow. `AddRogueArrow` calls `SetMissAnim(Arrow)` and `Loose` tints it and gives it a light, which `ProcessRogueArrow` carries along.
  - Magic: pink, light 2.
  - Fire, Exploding, Immolation, Guided: burning (`Tint::Glint`, FireOrange), light 2.
  - Cold, Ice, Freezing: ice glint, light 2.
  - Multiple Shot and Strafe: plain.
- **RfA-12 bow skills** (art bolts):
  - Barbed Shaft: infrared, light 1.
  - Piercing Shot: infrared, light 2, and every monster it passes blinks infrared (`ArtBoltBlinks` → `Marks.blinkTicks` → DrawMonster).
  - Crippling Shot: burning.
  - Shock Arrow: bright, light 3, an ArcSpark every 4 ticks (`ArtBoltSparks`).
  - Barrage: pink, the first arrow at the cast and four more 2 ticks apart through a field (`BarrageIntervalTicks`).
- **Freezing Arrow** ends in Frost Nova's sheet at 100%, HueCycle ice; the freezing_burst sheet is the fallback.

## Casts again
`IsRfa12BowSkill` now excludes three skills, so they take the cast path:
- **Rain of Arrows:** the fire cast animation (`GetPlayerGraphicForSpell`), 12 arrows (`FallingArrowCount`), an `IS_STING1` on each landing.
- **Hunter's Mark:** the magic cast. The marked monster is drawn as through infravision (`MonsterHunterMarked`, two hooks in scrollrt).
- **Phantom Volley:** the magic cast. A falling arrow in PhantomPurple onto each enemy struck (`AddFallingArrowsOn`), its ring purple (`FallingArrowRingHue`).

## Javelin & Spear page
- **Throws** (Poison, Plague, Anchor Javelin 6 tiles; Harpoon and Valkyrie's Spear 8):
  - The click arms `ArmJavelinThrow` and sends CMD_SATTACKXY. `ThrowArmedJavelin` casts the skill at the swing's hit frame through CastSpell, playing the Cast cue at release.
  - Out of reach, she walks and the held button asks again; with shift, nothing.
  - Projectiles: brown (Harpoon), bright (Anchor), acid (Poison, Plague), all light 1. Valkyrie's Spear is 150% burning, landing in the Apocalypse boom.
- **Staff look:** every javelin strike (Jab, Power Strike, Impale, Charged Strike, Fend, Lightning Strike, Sweep, Reaping Point, Turning Pike) and every throw swings in the Rogue's staff attack sheet with the staff's frames.
  - player.cpp `BeginStaffLook`/`EndStaffLook` cache the staff sheet per armour and class and swap it into `AnimationData[Attack]` for the swing.
  - The look ends in ProcessPlayers and before a shot, cast or block.
- **Jab:** a swing chain of three real thrusts. Each windup is cut to a third, and each thrust plays its swing cue.
- **Fend:** `DrawFendRing`, Might's ring in FendOrange under her, opening to 128x64 over the swing.
- **Turning Pike:** pivots after any strike at a target, not only a landed one.

## Magic
- **Shadow Step:** to the cursor (clamped, short of walls).
  - A white glinting ghost of her stand fades over 5 ticks where she left (`ShadowStepGhostMark`).
  - She is unseen for those 5 ticks, then fades in glinting white for 5 (`ShadowStepFadeTicks`, DrawPlayerTinted).
- **Vault:** cast instantly from the click, with no animation, only while standing. Three vaults inside 3 s, then a 15 s cooldown (`VaultCharges`). The landing cue is gone.
- **Decoy:** drawn ghost white-grey with Glint (`IsDecoyBody`).
- **Lightning Fury:** already plays the lightning cast; unchanged.
- **Valkyrie:** the note "alternative to The Ancients" describes it as it is; unchanged.

## Sounds
- 16 picks through `cards/mkcardpicks.js` and the generator, including Harpoon and javelin release = fballbow, Vault = teleport (no impact), Valkyrie's Spear impact = nestxpld, Lightning Strike swing2/lning1, Phantom Volley cast6/sting1, Rain of Arrows cast2.
- New Cast slots in `tools/skill_sound_slots.csv`: ReapingPoint, TurningPike.

## The audit (subagent, read-only, 6 findings, all fixed before the commit)
1. The staff cache took the worn sheet: `LoadPlrGFX` keeps a loaded slot. The worn sheet is now set aside first.
2. Fire and frost arrows kept their own sprites: the type was set without `SetMissAnim`.
3. The spark test on var5 fired for Ball Lightning's stamp. Flights are now marked with `ArtBoltExtrasMark`.
4. Rogue arrow lights jumped to radius 5. `ChangeLightXY` now keeps their own radius.
5. A held out-of-reach throw walked and never threw.
6. The staff look leaked into a same-tick shot.

**Open, multiplayer only (V1 is single-player):** a remote Rogue's Shadow Step fade does not count down on this client, and Vault sends no net command.

## Cards
- The Rogue page is at Version 9, rebuilt with `cards/add_rogue375.js` (31 SKILL_FX entries).
- 16 strips from `OracoolPreview.DISABLED_Rogue375CardStrips` (env `ORCL_ROGUE375_OUT`): eight arrow looks, Multiple Shot's six-arrow fan, Phantom Volley, the Decoy, the Shadow Step fade, the Frost Nova, the Exploding burst, the staff attack, the Fend ring.
- Skills with an animation went from 27 to 38.

## To check in play
- The staff swing's look on each armour.
- Jab's three thrusts.
- Throw ranges and the walk-in.
- Vault's charges.
- The Shadow Step fade.
- Phantom Volley's purple rain.
- Barrage's cadence.
- The arrow colours on real floors.
