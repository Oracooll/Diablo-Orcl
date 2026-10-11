# v1.13.013-015 - the fifth graphics batch: feel (2026-10-11)

User: "do all 10 suggestions, an audit loop and let me test afterwards." All ten in oracool/scene_feel.cpp/.h, each its own option (Settings > Oracool Edition, all on).

| Option | What it does | Where |
| --- | --- | --- |
| Hit Flash | a struck monster flashes white for 0.12 s (on the body as drawn; not while veiled) | NoteMonsterHit from M_StartHit; DrawMonsterHitFlash in DrawMonsterHelper |
| Impact Lights | a burst of the element's colour on the floor and in the air where a projectile stops, or is removed beside a monster, another player or a wall | TickImpacts; DrawSceneFeelLights |
| Area Atmosphere | Cathedral/Catacombs dust motes in the light, Caves drips and rings, Hell ash and embers, Crypt wisps, Nest spores | DrawSceneFeelLights |
| Lava Light | lava tiles within 2 tiles warm and brighten what stands there, flickering | DrawSceneFeelBodies (before the light passes) |
| Smooth Camera | after a jump of 3+ tiles while not walking (Teleport, Leap, phase) the picture eases from where it was (<= 80 px, 0.24 s, via the shake shift); within 9 tiles of a living rift guardian the world leans in 5% | TickCamera, CameraGlideOffset, ApplySceneFeelAfterWorld; cursor.cpp aims through WorldPointerFor |
| Wet Floors | figures faintly mirrored on any open floor in the Caves and the Nest | scene_fx.cpp Reflect (Ground.damp) |
| Drop Effects | shadow under a tumbling item, sparkles behind a rare one, a dust puff on landing | TickDrops; bodies + lights |
| Death Effects | blood (bone chips for undead) on a blow of a fifth of max life or more and on death; dust when the corpse lands; nothing for a dissolving guardian | NoteMonsterHit, TickDeaths |
| Window Light | soft beams slanting down from walls facing an explored room in the Cathedral, motes in them; the ten nearest. Stylised: the tilesets have no window pieces | DrawSceneFeelLights |
| Level Transition | the game's fade closes/opens as an iris about the screen's middle (not the loading screen's own fades) | engine/dx.cpp ApplyPresentTransforms, IrisFadeRow |

Audit loop:
- Round 1 (correctness + performance reviewers), fixed in v1.13.014: clicks aimed through the glide/shake/lean-in (WorldPointerFor); glide only on a real jump, zoom-scaled, cumulative, capped; no iris on the loading screen (IsShowingProgress); no bursts where a missile only expires; hit flash on the drawn body; no blood or dust for a dissolving guardian; std::less on pointers. Performance (Debug): lean-in ~15-20 ms -> per-column tables and packed lerps; iris ~10-15 ms -> solid runs + mirrored halves; lava light (flicker per tile, table falloff, inline tile lookup); window beams (10 nearest, inline writes); impact bursts (inline writes).
- Round 2: no P1/P2 - loop closed. Its P3s taken in v1.13.015: the lava falloff table as its formula says (round 1 had made it dimmer), strikes judged on the missile's own and next tile only (and not our own hero), beam ties broken by place.

Left (P3): beams pop when the nearest ten change; the hit flash uses the current frame under frame blending; the lean-in and iris centre on the screen, not the hero, with a side panel open; window beams push a splat a pixel on the GPU path.

Tests: test/oracool_graphics_batch5_test.cpp (4). ctest 1055/1055.
