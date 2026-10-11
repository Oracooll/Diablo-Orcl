# v1.12.430-432 - the second graphics batch and its audit loop (2026-10-11)

User: "let's do 1,2,3,5,7,8 for now from your last round of suggestions" ("4,6,9 we leave for later" - impact feel, the AI-upscaled HD pack and the GPU renderer), then "when done run an audit loop until you fix all major issues then i will test."

## What was built (v1.12.430), each behind its own option, all on by default
| Option | What it does | Where |
| --- | --- | --- |
| Day and Night | Tristram on a 20-minute clock (stops while paused or in the single-player menu): rose dawn 04:30-07:30, untouched day, amber dusk from 16:30, dark-blue night 20:30-04:30. After dark: the heroes' own light (9.5 tiles), a lamp by each townsperson standing in town, town/red portals, loot. A tint field over the finished town through the colour pass's loop (TintRect). Debug: `timeofday [hour]` | oracool/colour_light.cpp ApplyTownDayNight |
| Floor Marks | blood pools where monsters fall (bone dust for undead; never for the hero's minions, golems, companions), scorch / frost / acid / blood from explosions by colour; generated blots with a seeded ragged rim, blended from the lit floor's own colour, floor tiles only, 200 max, fade over a minute or two, cleared on a level change; own clock and xorshift | oracool/ground_fx.cpp; hooks in MonsterDeath and AddMissileExplosion |
| Loot Beams | rare (smaller), unique, set, primal, runeword drops on the ground: a beam (180 px, its colour, white core, shimmer), a floor glow, and a coloured light in the colour field (rebuilt whenever the shown loot changes) | oracool/ground_fx.cpp, colour_light.cpp |
| Lit Surface Relief | what DrawTileContent draws (walls, objects, figures) takes its brightness as height; a per-tile light direction (every light's pull by falloff, none within 3 tiles of a light, a lean upward) brightens what faces a light and shades what faces away, up to 40/256. Floor and hover outlines untouched. Dungeon only | oracool/colour_light.cpp Begin/ApplySurfaceRelief |
| Zoom Sharpening | contrast-adaptive sharpening after AMD FidelityFX CAS (green-channel amplitude, sharpness 0.6) over the filtered zoom | oracool/post_fx.cpp SharpenAdaptive |
| Smooth Animation | heroes and monsters cross-faded between frames by AnimationInfo::getFrameBlendFraction: ground kept, frame drawn, ground restored, next frame drawn, both mixed. Not for stalled animations (state unchanged for 1.5 ticks), chilled heroes/monsters, rift bosses, petrified/frozen, Whirlwind/companion spin, the last frame | oracool/frame_blend.cpp; scrollrt.cpp DrawMonsterHelper / DrawPlayer |

DrawGame order now: floor -> smooth floor light -> floor marks + loot glows -> floor objects -> rings -> [relief keep] DrawTileContent [relief] -> colour -> town day/night -> loot beams -> particles -> haze -> bloom -> mood/vignette -> outlines -> zoom (+ sharpening).

## Audit loop
Round 1 (three reviewers: animation; marks/loot; relief/day-night/CAS), fixed in v1.12.431:
- P2 smooth animation was not a cross-fade - the old frame stayed whole under the new (doubled bodies). Now the ground is kept and both frames drawn onto it.
- P2 chilled monsters/heroes and slow rift bosses skip game ticks: the blend faded and snapped back at 10 Hz. No blend for them.
- P2 the loot's coloured floor light stayed after a pickup (the field rebuilds only on light changes): a per-frame hash of shown loot sets UpdateLighting.
- P2 relief lit the hero from below by his own light, and saturated on almost every edge: pull fades within 3 tiles of a light, gentler slope, cap 40.
- P3s: due tick showed the old frame, beam NaN top row, rim clipped square, minions/Decoy bled on dismissal, outlines embossed, town portals unlit in town, phantom lamps from stale Towners slots, clock ran under the menu, per-frame allocation, debug comment misplaced.
Round 2: no P1/P2 in any of the three areas. v1.12.432 takes two round-2 P3s: the outline mask uses the real outline colours (165, 194, 233, 181, 197, 245) and the relief slope is halved rather than quartered (it was faint).

## Left (P3, documented)
- Loot beams show over walls in front of them (a beacon, D3-like).
- Rares are common late; their beam is 0.6 height and strength.
- Marks and glows draw over floor-pass objects (waypoint, Stonegate arch) when the deferred floor pass is off (town, or smooth lighting and wall shadows both off).
- A death mark sits on the monster's tile, not its mid-walk position.
- A one-frame blend-and-snap as a chill ends; the hover outline stays the frame's own.
- Frame cost unmeasured: the relief copies the frame and makes a full pass (the heaviest of the batch). Measure with the frame-time line under Show FPS.

Tests: test/oracool_graphics_batch2_test.cpp (22 tests). ctest 1015/1015 on v1.12.431.
