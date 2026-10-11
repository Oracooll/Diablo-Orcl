# v1.12.434-439 - the GPU renderer, Stages 1 to 3, and their audit loop (2026-10-11)

User: "start stage 1", then "proceed with the other stages. we will audit them all in one audit loop at the end", "skip the arm64 build stage", "keep going until the audit loop is clean".

## The design (a change from the plan doc)
Not the HUD-as-a-layer + own swap chain of the plan: the CPU draws the world as before; everything after the sprites makes ONE trip to a private Direct3D 11 device (oracool/gpu_post), and the finished, zoomed picture is read back into the back buffer before the HUD. So the HUD, its see-through panels, screenshots, the window, the mouse and SDL's present are untouched. Cost: an upload and a read-back a frame. CPU path kept in full (option off, card failure, 8-bit tests).

## Stage 1 (v1.12.434)
Bloom, area mood, vignette, zoom (Scale2x at 2x, sharp bilinear, nearest), CAS sharpening as integer HLSL on the CPU's own tables (post_fx.h shared tables) - pixel-exact (WARP comparison tests). Option Graphics Card Renderer (on), `gpu` pass in the frame timer, `gpuinfo` debug command. Round 1 of its audit: no trip when the card has nothing to do; d3dcompiler_47.dll looked up at run time; toggling the option retries a failed card.

## Stage 2 (v1.12.437)
The CPU passes split into plan + apply (PlanLightColour / PlanTownDayNight / ApplyTintPlan, PlanSurfaceRelief / ApplyReliefPlan, PlanHeatHazeAndWater / ApplyHazePlan), so both renderers use the same numbers. On the card, in the CPU's order: relief (against the world kept on the card before the sprites), light tint or day/night, beams and particles as splats (oracool/splats.h, premultiplied ONE/INV_SRC_ALPHA points), water light, lava haze, then Stage 1, with the deferred monster outlines on their own layer laid on after the mood (WallOutlineLayer) - which also ends Stage 1's outline-order difference. Exact to the pixel but for covering (ash) splats (within a step).

## Stage 3 (v1.12.437), each its own option (on), card only
| Option | What |
| --- | --- |
| Per-Pixel Lights | each light's round pool in its colour; shadows traced once a tile on the CPU (3 rays) into a visibility texture, sampled bilinearly - soft edges; at most 12 lights, those reaching the view; dungeon, and town after dark |
| Lit Sprites | normals from a 2-px Sobel on what DrawTileContent drew, lit by each light from 40 px above its tile; replaces Lit Surface Relief on the card; town after dark |
| Wide Soft Bloom | half-size bright pass, 5-level down/up chain, a fifth of it added |
| Light Shafts | up to 4 flames/spells near the view's middle: the bright pass smeared along the ray, nearest the light strongest |
| Heat Distortion | the picture bent (bilinear) over lava (the liquid map), fire spells and flames |
| Filmic Colour Grade | per area gain/lift/gamma, saturation, S-curve, black point, after the mood |

Stage 4 (native ARM64): dropped at the user's word (Windows on ARM's translation runs the game well).

## Audit loop
Round 1 (two reviewers, all of v1.12.434-437): one P2 - per-pixel lights traced 3 rays x 8 steps per pixel per light (tens of ms on an iGPU) -> per-tile visibility on the CPU, 12-light cap, culled to the view. P3s fixed: wide bloom summed 5 levels (white fires), Stage 3 ground half a pixel off, the relief field and liquid map sent for nothing or twice, daylight Lit Sprites trip in town, per-frame allocations, lava strength texture remade each frame, ~20 shaders compiled in the first level frame (now at start-up in dx_init), CSV header written into an old file, renderer help text. v1.12.438.
Round 2: no P1/P2 on either side. v1.12.439 takes two P3s: the shadow tracing clipped to the view (gpu_post.cpp optimised in Debug like colour_light.cpp), the CSV options column named `options25` so an older file is kept aside.

## Left (P3)
Stage 3 numbers are tuned by eye on synthetic frames only - a screenshot pass in game is the real check. The read-back stalls the CPU until the card finishes (no overlap). First start compiles the shaders (a moment at start-up). A covering splat may round a step from the CPU.

Tests: oracool_gpu_post_test (5), oracool_gpu_stage2_test (5), oracool_gpu_stage3_test (6), all on WARP. ctest 1035/1035.
