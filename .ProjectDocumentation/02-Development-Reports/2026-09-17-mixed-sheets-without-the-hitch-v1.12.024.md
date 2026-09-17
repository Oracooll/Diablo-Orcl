# Mixed sheets without the hitch

2026-09-17 — v1.12.024

## Why

> "i tried it. it looks good. what i notice is a noticable lag when first frame of a mixed sprite needs
> to occur like when i place a large shield or when i initiate an attack."

v1.12.023 built a mixed sheet on the main thread at the moment its animation was first wanted. Measured
with the export tool (Debug build, the one the user plays), one sheet at a time:

| sheet | synchronous mix |
|---|---|
| hit | 247 ms |
| walk | 404 ms |
| stand | 527 ms |
| attack | 673 ms |
| lightning cast | 1150 ms |
| magic cast | 2005 ms |

A quarter of a second to two seconds with the game stopped, once per animation. That was the lag.

## What it is now

All three of the remedies offered, in `oracool/sprite_mix`:

**1. Remembered.** A finished sheet - mixed AND already at the size of the class - is kept in memory for
the session and written to `<prefs>/sprite_cache/<key>.osm` for good. The key is everything the pixels
depend on: sprite class, armour tier, weapon class, animation, look, dye, scale (`wld-as-k3-d0-s100`).
A declined mix ("these sheets are no twins") is remembered too - it is the expensive answer to reach.
`CacheVersion` in the file header invalidates old files when the mixer changes; a truncated file reads
as "unknown" and is rebuilt over.

**2. Built ahead, off the main thread.** `LoadPlrGFX` never mixes. Cache hit: the sheet, at once. Miss:
the plain sheet is worn, the request is queued, and the animation remembers the key it waits for
(`PlayerAnimationData::pendingSheetKey`). `PumpPlayerSheetMixer`, once a game tick:

- reads ONE archive sheet for the job in hand. The archive reads stay on the main thread on purpose -
  nothing says the MPQ reader is safe to share - and one read a tick is what a plain animation change
  already costs;
- hands a fully read job to a worker thread, which does all the pixel work and writes the cache file;
- puts a finished sheet on every hero waiting for it. `AnimInfo` and `previewCelSprite` are views into the
  sheet being replaced, so whichever points inside it is re-bound before the old sheet is freed.

`PrewarmPlayerLook` queues EVERY animation of a look the moment the gear changes, so the attack sheet is
being built before the first swing. The worker is stopped and JOINED in `diablo_quit`, after the exit
watchdog is armed - this project has a standing problem with processes that outlive their window, and
a thread running while the statics it uses are torn down is how one more would be made.

**3. Less work.** `Dilate` and `Erode` now work inside the bounding box of what they are given instead
of the whole frame (a shield or a blade is a twentieth of it), and the twin check is memoised per pair
of files. It was going to be a hard-coded table; memoising is the same saving without transcribing
numbers measured by a different script, and the disk cache means it runs once ever anyway.

## Measured

`oracool_sprite_export --mix` now also drives the background path end to end:

```
background path, key wld-as-k3-d0-s100: cache before = unknown
  finished=1 after 498 ms, 54 pumps, worst single pump 19 ms (that is all the main thread ever pays)
  cache after = READY, taking it cost 0 ms, identical to the synchronous mix: YES
```

and on the second run `cache before = READY (from disk)`. So: the worst the main thread ever pays is one
archive read (19 ms here, Debug), the sheet is on the hero about half a second after the gear change
without a frame being dropped for it, and every later time is free. The result is byte-identical to the
synchronous mix.

## What to expect in game

The first time ever a look is worn: the plain sheet for about half a second per animation, then the
swap - a buckler that becomes a heater shield a moment after equipping. From then on, including after
restarting the game, the mixed sheet is there from the first frame.

Not run in the game. 795 tests pass; the mixer itself needs the archives and is verified through the
export tool rather than ctest.
