# 2026-10-01 - The lightning family (v1.12.310)

**Date:** 2026-10-01. Debug only. 902 tests pass; the Debug `diablo.ini` was unchanged by ctest. oracool.mpq repacked (407 files). Commit e3c44d17.

One build for a batch of the user's assets and requests, gathered over the evening without building in between.

## The strike system
- **Sheets:** `tools/BuildBallLightning.py` takes the user's 24 white-on-black strike pieces (8 short, 8 chain, 4 long, 4 impact) and colours them on an ice-blue ramp. The faint glow is dithered.
- **Turning:** the engine cannot rotate a sprite, so each piece is pre-turned to 32 angles. Each sheet has a row per variant and a frame per angle, read clockwise from east.
- **`AddLightningStrike` (missiles.cpp):**
  - A short reach gets one short spark.
  - A longer one gets chain or long bodies laid along the exact screen line, ending in an impact fork whose tip lands on the target. Where pieces overlap, the overlap falls on their axis joins.
  - Without the fork, the bodies run the whole way: an arc.
  - Each piece is a held frame on the art carrier, standing on the tile under its centre. Its last tick is at half alpha.
- **Ball:** the user's 72-frame ball, one frame a tick.

## Skills
- **Ball Lightning:**
  - The ball rolls toward the aim for 3 s (`AddArtBolt`, tagged with its field's stamp in var5) and stops at walls.
  - Every 5-9 ticks it strikes a random enemy within 3 tiles for Charged Bolt's damage at that rank. With no enemy near, it throws a spark into the air.
- **Lightning Rod:**
  - The user's totem. It strikes an enemy within 2 tiles once a second.
  - When it swallows a lightning missile, its burst is seven crown strikes every way plus one per enemy hit. The rod then fades in 10 ticks.
  - The RfA-27 burst sheet is only a fallback.
- **Faraday Ring:**
  - The user's halo. The stone pixels are locked to one median colour, because their per-frame palettes made the ring wobble.
  - It strikes an enemy within 2 tiles twice a second, and strikes each missile it destroys, from the rim toward it.
- **Storm Crucible:** the conductors wear the Lightning Rod sheet. The arc is a fork-less strike crown to crown, redrawn every 3 ticks and doubled on a damage run.
- **Lightning Clone** (renamed from Ride the Lightning):
  - The Sorcerer teleports, with a fork-less arc from where he stood to where he lands.
  - A clone stays at the origin and runs to him at his walk, a tile every 8 ticks, any of 8 ways. It strikes an enemy within 3 tiles every `max(1, 31 - rank)` ticks. On reaching him it bursts into sparks.
  - The renderer draws the hero's live walk sprites in `Tint::Clone`: pure white with lightning-blue bands, shadows kept. A list held on the missile would dangle when his sheet reloads.
  - The Impact cue is elecimp1, at most every 4 ticks. The damage per strike stays 3-12, +2-4 per level.
- **Conduit:** the hero is drawn in `Tint::Electric`, bluish-white with bands, instead of the loop sheet.
- **Lighting:** the lightning sheets, strikes, the ball, the clone, and the Static Charge and Conduit buffs light the floor (`ArtEffectLightRadius`, `PlayerState::buffLight`). Lights are freed by `EndArtEffects` and by the ball's end.
- **Mana Shield** (renamed from Energy Shield; the enum and sound slots keep the old code name): its orb glints blue (`Tint::Glint`, 0x5A96FF).
- **Absolute Zero:** halved at the user's pick. The sheet is 256x128 with anchor {0,48}, and the reach is 2 tiles. The impact sound is Blood Star impact.

## Round 65 audit fixes
- SORT keeps readied scroll bindings.
- Melee side blows feed the RfA-12 on-hit passives.
- The side blows carry the front blow's skill share, pinned at the front blow.
- Vaulting Strike's paid blow is covered by the prepaid check, as Leap Attack's is.
