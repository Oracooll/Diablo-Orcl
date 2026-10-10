# Visual techniques audit, round 3 - clean (2026-10-10, over v1.12.424)

One read-only reviewer re-checked the round-2 changes and swept the nine techniques again. Nothing above P3; no code change, no build.

- The shadow rule `dLight + 1 >= OpenLevel` was checked numerically against a Python model of DoLighting (LightFalloffs, LightConeInterpolations, RotateRadius): for the hero's radius-10 light dLight and OpenLevel agree to within one level on every tile at offset 0, and on 99.9% of tiles over all 225 walking offsets - the rule never switches the hero's shadows off. Small moving lights (radius <= 3) differ more at sub-tile offsets, so their own blocked part can leak past a wall for a tile or two (P3, accepted).
- Flicker's per-tile multiplier, VisibleWorldRegion vs ZoomScale (both zoom paths read nothing outside it), indexed-surface gates, overflow and bounds: correct.
- Accepted P3: a new game whose first level has the same key as the last (town to town) can show up to ~12 s of the old game's motes/embers; for one frame on entering a level the old flame/shadow tables may apply.

The audit loop on the nine techniques is closed: rounds 1-2 fixed (v1.12.423-424), round 3 clean. Next: the user's play pass.
