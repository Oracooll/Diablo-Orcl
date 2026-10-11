# v1.13.009-010 - the fourth graphics batch: town life, weather's traces, trails, shockwaves, the guardian's fall (2026-10-11)

User: "Build rtm and then proceed with all suggested." The RTM folder was refreshed first with a Release build of v1.13.008 (no source change, no bump). Then the 11 suggestions, all in the new oracool/scene_life.cpp/.h, each behind its own option (Settings > Oracool Edition, all on).

| Option | What it does | Drawn |
| --- | --- | --- |
| Cloud Shadows | soft cloud shadows drift over Tristram by day, fainter when it is overcast | DrawSceneLifeBodies (before the light passes) |
| Town Life | crows peck on the ground by day and take off within 3 tiles of anyone (they land out of sight and come back after 25-65 s); leaves blow through by day; fireflies at night | bodies (crows, leaves), lights (fireflies) |
| Puddles | the ground's wetness rises in rain (40 s to full) and dries over 150 s; puddles on about one open tile in six, sized by it, rippled while it rains. Debug: `weather wet` / `weather dry` | ApplySceneLifeGround (floor pass) |
| Footprints | toe and heel prints every 14 px of the hero's walk in snow or on wet ground, fading over 30 s | floor pass |
| Torch Smoke | puffs rising from the top of torches, candles and burning crosses (the flame tip found from the sprite's topmost pixel) | bodies |
| Swing Trails | a melee swing's arc in front of the hero over the frames up to the blow; fire or lightning damage colours it | lights |
| Missile Trails | a streak back along the velocity, in the missile's element colour (arrows faint grey; walls, portals, a charging monster none) | lights |
| Shockwaves | explosions, novas and impacts (by missile type, or a missile switching to an explosion sheet) bend the picture in a ring | ApplySceneLifeAfterWorld (post-zoom, before the shake) |
| Guardian Death Moment | a rift guardian's fall: white-gold flash, the world drained of colour for 1.8 s, a shake kick (scene_fx KickScreenShake), a big shockwave, embers, "VANQUISHED" and his name | after-world + card beside the entrance card |
| Drop Flares | a unique, set, primal, buffed-unique or runeword item flares like a star where it lands (seen tumbling, then landed) | lights |
| Living Portals | town, red and rift portals twist about their middle, glow on the floor and draw motes in | SwirlPortal (DrawMissile) + lights |

Audit (one reviewer): no P1. Fixed in v1.13.010: the death moment skipped in a later rift (the fallen guardian's id was never forgotten); the swing arc one frame late (_pAFNum is 1-based); crows landing in view at wide resolutions (now checked against the screen); a new missile at a freed one's address missing its wave (start tile compared too); the wave cap 12 -> 8 (cost at 2x zoom). Left: the hero's own dropped unique flares too; missile trails show over walls; the swirl turns whatever is inside the portal's oval; wetness carries into a new game.

Tests: test/oracool_graphics_batch4_test.cpp (5). ctest 1050/1050 on v1.13.010, diablo.ini unchanged.
