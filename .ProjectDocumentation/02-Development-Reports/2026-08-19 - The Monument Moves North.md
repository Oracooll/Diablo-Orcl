---
date: 2026-08-19
version: 1.8.18
area: Levski's Roar - placement and the rest of the click path
---

# The Monument Moves North

Two small changes on the user's instruction, and one assumption turned into a guarantee.

## Placement

{55,66}, one tile north of the stash chest at {55,67}, replacing the {56,67} the first cut used.

The fallback list stays - town furniture has moved before and a silent overlap would put two
operable objects on one tile - but it now **logs when a fallback is taken**. Previously a fallback
was silent, which means the answer to "is it where I asked?" was "probably", and the user had to
find out with `tiledata objectindex` rather than being told.

## The rest of the click path

1.8.17 fixed `_oSelFlag`, which was the reason the monument ignored clicks. Tracing the rest of
that path for this change turned up a second assumption worth pinning: `ACTION_OPERATE` in
player.cpp checks `_oBreak == 1` FIRST, and a breakable object gets **swung at** rather than
operated.

A stand is not breakable, so this was never going to fire - but "never going to fire" is what was
also true of `_oSelFlag` until it wasn't. `_oBreak = 0` is now set explicitly beside the selFlag,
with the reason recorded, because the failure it prevents is a click that silently becomes an
attack.

## Note on the fix that has not been play-tested

The selFlag fix shipped one build ago and the monument has still not been confirmed clickable in
game - the suite cannot cover it, as 1.8.17 recorded. The user was testing at {56,67} before that
build existed, which would have looked identical to the bug. This build is the one to test.

## Verified

**454 tests, the usual two**. Wiki updated in three places to name the tile.
