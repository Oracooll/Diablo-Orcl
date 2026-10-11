# v1.13.005-008 - the third graphics batch, the rift guardian's entrance, the low health warning (2026-10-11)

User: "i like all of your suggestions. you can start implementing all of them. i also want screen shake when rift boss appears. rift boss to appear a bit further away from hero with some sort of large-scale light beam. the appearance effect to be nice one - fade-in or erecting from the ground or something." Then, mid-batch: "i want reddish pulsating screen border bleeding inward making it clear hero is approaching 0 health."

## What was built, each behind its own option (Settings > Oracool Edition)
| Option | Default | What it does | Where |
| --- | --- | --- | --- |
| Figure Shadows | on | heroes, monsters and townsfolk cast soft silhouettes away from the three strongest lights near them (torches, spells, lamps; the sun in Tristram by day, fixed at 10:00 with Day and Night off), longer and fainter the further the light, stopped by walls, fading as lights swap; beside the painted shadows, never instead of them | oracool/scene_fx.cpp, scrollrt.cpp DrawFigureGroundPass |
| Exploration Memory | off | explored floor and walls you cannot light now stay faintly visible (light 12); monsters, items and corpses there stay hidden | scene_fx ExplorationMemoryLight, colour_light.cpp plane, scrollrt DrawFloor/DrawDungeon |
| Weather | on | rain or snow in Tristram now and then: wet-dark or white ground, splashes on walkable floor, streaks dimmed by the hour, distant lightning on rainy nights. Debug: `weather rain/snow/clear` | scene_fx, debug.cpp |
| Ground Fog | on | drifting mist on the floor of the Caves and the Crypt, seen only where there is light | scene_fx ApplyGroundFog |
| Water Reflections | on | figures by water mirrored in it, faint and rippling, on water tiles only (the town river too) | scene_fx Reflect |
| Living Town | on | Tristram's river shimmers (post_fx water light, the river found by colour) and its trees sway (only the pixels the trees drew, while nothing covers them) | post_fx.cpp, scene_fx Begin/EndFoliageMicro, ApplyFoliageSway |
| Afterimages | on | Charge and Whirlwind leave ghosts as he moves; Leap and Teleport a trail along the jump | scene_fx DrawPlayerAfterimages |
| Dissolve Effects | on | a rift guardian burns away into embers as he dies and leaves no corpse; what he conjures fades in | scene_fx veil, monster.cpp MonsterDeath |
| Status Visuals | on | frost sparkles on the frozen, flames on the burning, green drips on the poisoned, motes on the chilled | scene_fx DrawMonsterStatus, rfa12 MonsterBurning/Poisoned |
| Living Orbs | on | the orbs' liquid sways and bubbles; the life orb glows red under a third, on the warning's heartbeat | hud_art.cpp DrawLivingLiquid |
| Pixel Art Smoothing | on | with the Sharp Zoom Filter on the card: xBR's edge rule at any scale over the sharp bilinear | gpu_post.cpp PSZoom mode 4 |
| CRT Look | off | scanlines, a phosphor mask, darker corners over the whole frame | scene_fx ApplyCrtLook |
| Debanding | on | the smooth light's multiply rounds through an 8x8 ordered pattern anchored to the world | colour_light.cpp ApplySmoothFloorLight |
| Rift Guardian Entrance | on | he rises 7+ tiles away: a column of gold light and a ring across the floor, the picture shaking, black bars, "GUARDIAN OF THE RIFT" and his name; he rises out of the floor as he fades in (0.35-1.85 s) and stands still till then | scene_fx, rift.cpp, monster.cpp |
| Low Health Warning | on | under a third of life a dark red glow bleeds in from the screen's edges with an uneven creeping edge, pulsing like a heartbeat, deeper and faster toward death | scene_fx ApplyAfterWorld |

Not built: light through windows - the levels have no window or grate tiles tagged, so it needs those found first.

Game changes, deliberate: the guardian's spawn ring starts at 7 tiles (then 3-6, then any open tile), he is held still for his rise (in ticks at the game's speed; the hold ends in Stand, not "strike now"), and a dissolved guardian leaves no corpse. Everything else reads the game only. The effects run on their own clock (paused with the game).

DrawGame order now: floor -> smooth floor light (dithered) -> weather ground -> marks -> floor objects -> rings -> fog -> figure shadows/reflections -> [keep] DrawTileContent (veil, status, afterimages, tree capture) -> tree sway -> GPU or CPU passes (+ guardian beam beside the loot beams) -> [DrawView] shake, falling weather, lightning, low health -> HUD -> entrance card -> cursor -> CRT.

## Audit loop
- Round 1 (two reviewers), fixed in v1.13.006: a Debug crash (an empty veil keep indexed at the screen edge); fog, shadows and reflections drawn after the relief keep (the relief took them for walls) - moved to a pre-pass and the floor pass, shadows into one screen layer; the hero's own light flickering a shadow mid-step; the rising guardian's full outline; off-by-one rows; tree sway double shifts; CRT on partial redraws; xBR shimmer at fractional zoom; the hold in fixed ticks; spawn path budget doubled; memory showing items and corpses; per-figure light and liquid work; inline channel arithmetic (no calls in Debug pixel loops); debanding moved into the light pass; shake as a post-shift.
- Round 2, fixed in v1.13.007: tree sway sorting and moving whole boxes (now by recorded tree pixels); shadows through walls (wall room); sun on the clock with Day/Night off; a dark ring at the light's edge with memory on; fog noise cost; arches drawn black over remembered walls; CRT/entrance bars on 640-wide panels; a free first strike after the hold; the clock's float precision; the beam left behind by a blink; orbs on real time.
- Round 3, fixed in v1.13.008: no P1 or P2 in correctness; performance found the tree capture's cost and missed same-colour pixels (now a top-byte mark, exact), seams in the sway (phase continuous in world x, tables, no calls), and shadows popping on light swaps (strength over the fourth light); P3s taken: hold flags cleared on level load, dash ghosts after the clock wrap, the wall cut faded, lamp shadows cross-faded with the sun.
- Round 4: no P1 or P2 - the loop is closed. Its P3s are below: the tree capture's cost in Debug where trees are dense (a fully opaque half could be recorded without the mark), lamp shadows starting at a sliver above zero at dusk (the fade and TownIsDark use different measures), four equal lamps cancelling each other's shadows, a held guardian killed mid-rise leaving a hold flag for the slot until the next level.

Tests: test/oracool_graphics_batch3_test.cpp (7 tests). ctest 1044/1044 on v1.13.008.

## Left (P3, documented)
- Floating numbers and health bars do not shake with the picture; the entrance's bottom bar covers the belt for 3.8 s.
- Afterimages and reflections draw in palette colours (no unique TRN or hero dye).
- A guardian struck during his rise leaves the hold early (he may act while still sunk).
- A conjured monster fades in again when it re-enters view after half a second.
- Figures seen only through infravision or Scent of Blood cast no shadow.
- Non-tree town pieces with upper parts (if any) would sway: check with a screenshot.
- The tree capture costs several ms in Debug in tree-dense views: watch the Sprites line under Show FPS.
