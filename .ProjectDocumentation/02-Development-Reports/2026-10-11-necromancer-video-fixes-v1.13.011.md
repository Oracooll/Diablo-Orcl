# v1.13.011 - fixes from the Necromancer play video (2026-10-11)

Source: the user's 32.6 min recording on the RTM v1.13.008 build (Captures/... 17-15-51.mp4), reviewed at one frame every 5 s plus full-resolution close-ups. User: "too light tiles in town when snowing, blinking ghosts of my necro and check the light funnel from fireball".

- Light Shafts (gpu_post.cpp): only kind 1 lights (the level's fixed flames) cast shafts. A moving spell light smeared the bright pass into a hard band from the hero through the fireball and on, over walls (11:09).
- Afterimages (scene_fx.cpp DrawPlayerAfterimages): the Leap/Teleport trail keys on position.tile (where he stands) and never fires while walking; the draw tile could step two at once on a plain walk past a tree, and a phantom blinked beside him (17:13).
- Weather (scene_fx ApplyWeatherGround, scene_life DrawSceneLifeBodies): the snow-white / rain-dark tone moved from the floor pass to the whole picture before the light passes; on the floor alone it skipped every tree's and rock's ground piece (drawn as walls) - a checkerboard (17:10). Splashes stay on the floor. Test updated (batch3 RainDarkensTheTownAndSnowWhitensIt).

Not fixed, open: Caves lava flat yellow then blue at 15:05-15:45 - a helper traced the palette path and found it sound; the colours are exactly PAL8_YELLOW/PAL8_BLUE, the automap's ramps, so probably the map overlay (user to confirm). Red full-screen frames at ~17:16.8 cover the HUD too - not a world effect; awaiting the user. Three small palette bugs found on the way (stale cycling colours in sprite caches, Hell light tables without LightTableVersion++, entry 31 never cycled) - offered, not done.

ctest 1050/1050, diablo.ini unchanged. Commit c8a18088 (local).
