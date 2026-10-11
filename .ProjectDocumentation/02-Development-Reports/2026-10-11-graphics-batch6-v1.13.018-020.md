# v1.13.018-020 - the sixth graphics batch: glow (2026-10-11)

User: "do all 20 suggestions, an audit loop and let me test afterwards." All twenty in oracool/scene_glow.cpp/.h, each its own option (Settings > Oracool Edition, all on).

| Option | What it does |
| --- | --- |
| Level-Up Burst | a column of gold light, a ring across the floor and rising motes on a level-up |
| Casting Glow | the floor round a caster lights in the spell's colour during the cast |
| Potion Effects | an eighth of life or mana gained at once swirls red/blue round the hero |
| Pickup Glints | gold and magic+ floor items glint now and then; a pickup throws sparkles |
| Chest and Door Effects | a chest opens in gold sparks and light; a door raises dust |
| Elite Auras | champions (lesserAffix) and uniques stand in two turning rings, coloured by affix |
| Hero Hurt Flash | a blow of 1/33 life or more flashes the hero red and pulses the screen's edges |
| Shrine Glow | unused shrines, goat shrine, cauldron and the four fountains glow, motes rising |
| Lit Homes | after dark the homes of Griswold, Pepin, Ogden, Adria and Gillian show a window and add a lamp to the town's light |
| Lens Glints | up to 8 flames and fire spells throw a horizontal streak |
| Spell Charge | motes spiral into the hands until the release frame, then a flash |
| Status Lights | burning monsters light the floor with flickering fire, frozen ones with cold (statusVisuals already drew on the bodies) |
| Blood Trails | a hard blow (1/6 life) splashes a floor mark away from the hero; under a third of life a walking monster drips (ground_fx marks) |
| Lingering Marks | Fire Wall, Flame Wave and Inferno scorch each tile once; cold missiles that strike leave frost (ground_fx marks) |
| Boss Presence | rift guardians, the Skeleton King, Butcher, Diablo, Lazarus and Na-Krul darken the floor round them and the screen's edges within 10 tiles |
| Orb Slosh | the HUD orbs slosh after a quick change and a glint crosses the surface (hud_art DrawLivingLiquid) |
| Exit Glow | stairs and entrances breathe a light: cold going down, warm going up |
| Corpse Settle | a body cools toward grey from 5 s to 65 s after it falls |
| Town Forge | Griswold's forge glows and throws sparks every 1.4-2.6 s |
| Armour Sparks | a melee blow on skeletons, knights, golems, gargoyles throws white-gold sparks |

Not built as suggested: idea 19's stars - Tristram is seen from above and has no sky.

Audit loop:
- Round 1 (correctness + performance), fixed in v1.13.019: monsters placed where DrawMonsterHelper draws them (southward steps), nothing for hidden monsters, gear changing the pools no longer reads as a hurt or a gulp, bodies already on a level seeded old, the forge clock reset with the scene clock, hurt flash in the Hive and Crypt branch, glints only for flames and fire, homes only for the townsfolk who keep one (no duplicates), lamps only after dark, no scorch under Ring of Fire's caster. Performance (Debug): the edge band (~6 ms) as spans with a cached table, the boss darkness (~1.5 ms) as angle/distance tables, pools' splats through a raw buffer, the level-up column inline, elite rings by tables, the light list every 0.2 s.
- Round 2: no P1/P2 - loop closed. Its P3s taken in v1.13.020: splats appended from a scratch buffer in one copy (Pool and the level-up column), each ring's dashes locked to it, bodies laid out of sight dated twice a second.

Tests: test/oracool_graphics_batch6_test.cpp (5). ctest 1061/1061.
