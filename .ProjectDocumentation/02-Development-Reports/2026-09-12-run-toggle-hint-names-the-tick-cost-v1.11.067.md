# The Walk/Run hint says what a step costs

**Version:** 1.11.067
**Date:** 2026-09-12
**Branch:** renderer-32bit

## What was asked

> "the pop up text on the walk/run toggle - add elabotation text saying how many frames or ticks is
> one 'move'"

## The numbers are derived, not quoted

The hint now reads, for example:

```
Running
One step: 6 ticks, 8 frames
Movement speed 100%
Click to switch. Also the R key.
```

The tick count is computed the way `StartWalkAnimation` computes it, rather than by printing the
plain-walk constants:

- the walk animation is **8 frames**, and its length is `8 - skippedFrames`
- a plain walk floors the skip at **-2**, so 10 ticks
- the run floors it at **2**, so 6 ticks
- `StrideTicksFor(MovementSpeedPercent(player))` is what Movement Speed does to the stride, clamped
  to 4..12 ticks

That matters because a fixed "10 ticks" would be wrong for most characters: items, Vigor and slows
all move it. The table the formula produces:

| Movement speed | stride | walking | running |
|---|---|---|---|
| 50% (slowed) | 12 | 10 | 6 |
| 100% | 10 | 10 | 6 |
| 120% | 8 | 8 | 6 |
| 145% | 6 | 6 | 6 |
| 250% | 4 | 4 | 4 |

Two things that fall out of it and are now visible to the player rather than buried: a slowed walk
still animates at the base 10 ticks (the walk's own floor), and **from about 145% Movement Speed
onward walking is already as fast as running**, so the toggle stops meaning anything for a
well-buffed character.

## The carry is local

`StrideTicksFor` takes its carry by reference and CONSUMES it - that is how a fractional tick is
spread across successive strides so that "+5% a level" is exact over a run of steps rather than
rounded away each time. The hint passes a **local** carry initialised to zero and throws it away, so
hovering the button cannot spend the player's accumulated fraction. Reading the state with the real
carry would have made looking at the button alter the next step.

## Verification

Debug and Release build clean; **718/718** tests pass. RTM updated. The arithmetic was checked
against the documented engine values independently of the build - 100% gives 10 walking and 6
running, 250% gives 4, which are exactly the figures `WalkFrameSkipFor`'s own comment names.
