# v1.12.433 - GPU renderer plan, Stage 0: measuring the world draw (2026-10-11)

User: "go with your recommendations, start stage 0" on the Orcl GPU Renderer Plan doc (all six decisions as recommended: Direct3D 11; graphics card by default with automatic CPU fallback; frame cap matching the display with vsync; Stage 1 first, then play; native ARM64 looked at after Stage 1; "same within a shade").

Built: oracool/frame_timing gains a pass timer. DrawGame wraps each pass in a ScopedFramePass (floor, light, marks, rings, sprites, relief, colour, night, beams, particles, haze, bloom, mood, outlines, zoom), timed with the performance counter and summed per world frame. With the FPS counter on:
- three lines under the frame-time line show each pass's average ms per world frame over the last second, and the world total;
- every ten seconds a row is appended to frame_passes.csv in the pref folder (beside diablo.ini): time, level type and number, set level, zoom, screen size, world frames, average and worst frame, frames over 25 ms, world ms, each pass's ms, and the 18 effect options as digits (order in the header's comment).
Off (FPS counter off): nothing is shown or logged; the timer still runs (about 30 counter reads a frame).

Next: the user plays one session on the Snapdragon laptop with Show FPS on (town by day and night, a Cathedral, the Caves' lava, Hell, zoomed in once); I read frame_passes.csv and report what Stage 1 would win.
Test: OracoolFramePasses.EveryPassHasItsOwnName. ctest 1016/1016.
