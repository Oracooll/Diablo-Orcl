# The Valkyrie: darker gold, a black shadow, and walking the right way

2026-09-14 — v1.12.014

## Why

> "can you make valkyrie darker and with black shadow? also her movement in town is funny. it is like she walks in
> the wrong direction then teleports close to me."

## The walk

- **Wrong way.** v1.12.013 offset her mid-step with `Displacement(dir).worldToScreen()`. That transform's vertical
  axis is the camera's, the opposite sign from the one the renderer draws walkers with. Stepping South it moved her
  **up**. She slid a whole step the wrong way, then snapped onto the tile she had actually reached: the "teleport".
  She now uses the renderer's own `MovingOffset` table from `GetOffsetForWalking`, copied as `WalkStep`, with a note
  saying why not `worldToScreen`.
- **Falling behind.** A Rogue running in town outpaced her 8-tick steps. Past 14 tiles the catch-up rule put her
  back beside the Rogue: a second, real teleport. When she is more than 4 tiles behind, a step now takes 4 ticks.

## The look

- **Darker.** Her gold translation is the yellow ramp pushed three shades dark instead of three shades light.
- **Black shadow.** Colours darker than luminance 40 (the sheets' outline and the shadow under her feet) keep their
  own palette entry, so they stay black instead of turning dark gold. The Decoy's blue ghost is unchanged.

## Not verified here

This build was not run in the game.
