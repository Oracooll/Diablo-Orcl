# The shield in town

2026-09-17 — v1.12.026

## Why

> "can we borrow the shield from town sprites?"

Until now the mixer declined both town sheets, so a hero carrying a Tower Shield wore the heater in
the dungeon and the buckler in Tristram.

## Why they were declined, and why that was wrong

The twin test compared palette INDICES: of the pixels two sheets both draw, how many are identical.
Town scored 44-55% and was treated as "another pose". Looking at the sheets side by side showed the
opposite - same walk, same stance, shield hanging at the side - so the test was the thing at fault.
Two renders of the same body are not index-for-index the same: they carry different shading noise,
one step along the SAME ramp, all over the body.

Scored by ramp (material) instead, every same-body pair lands at 68-92% and every different pose at
42-58%:

| pair (sword vs sword+shield) | by index | by ramp |
|---|---|---|
| light, town stand | 55% | 68% |
| heavy, town walk | 44% | 75% |
| light, stand | 67% | 76% |
| light sword vs staff, attack (a different pose) | 20% | 42% |

So, in `oracool/sprite_mix`:

- `AreTwins` counts pixels of the same RAMP; threshold 63.
- `SolidDifference` is SEEDED BY RAMP, GROWN BY INDEX. A new object is a different material, so a
  difference starts only where the ramp changes or there was nothing. A heater's white rim over grey
  plate is the same ramp as what it covers, so index differences within two pixels of a seed are taken
  with it.

## What the looser test let in, and the guards that answer it

The first contact sheet after the change showed two regressions, both as a heavy body standing inside
a light one: the fire cast (flames differ frame for frame between the two renders, and flames are their
own ramp) and the sword-only attack (the heavy vote is noisy enough to call half a torso "sword").

A piece is now only believed if it is plausibly that piece:

- a shield may be at most 78% of the figure it is on (62% declined the honest town walk, where a heater
  seen side-on really is most of the silhouette; fire sits far above either);
- a blade may be at most 320 pixels (real ones measure 60-190). A sheet whose blade fails is composed
  again with the sword left to its own tier, so the shield is not lost with it.

## Where each animation stands now

| animation | shield | sword |
|---|---|---|
| town stand | **mixed (new)** | mixed |
| town walk | **mixed (new)** | own tier's |
| stand, walk, lightning, magic | mixed | mixed |
| attack, hit | mixed | own tier's |
| fire, block, death | own tier's | own tier's |

`PrewarmPlayerLook` no longer skips town and asks for the town sheet names there. `CacheVersion` is 3,
so sheets cached by v1.12.024-025 are rebuilt once.

## Verified how

`oracool_sprite_export --mix`, contact sheet `engine_mix_contact2.png`: the heater hangs at his side in
town in all eight facings, no heavy-body artefacts anywhere, background path still byte-identical to
the synchronous mix. 795 tests pass. Not run in the game.
