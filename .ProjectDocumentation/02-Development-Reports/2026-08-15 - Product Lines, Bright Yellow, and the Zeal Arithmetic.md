---
date: 2026-08-15
version: 1.6.23
area: Item colours / Zeal frame budget
---

# Product Lines, Bright Yellow, and the Zeal Arithmetic

Three corrections from the user, each sharpening a definition.

## 1. The material lines are NOT sets - green retired from items

> "Set Items, are items that give bonuses when a whole set is gathered. We have not developed yet
> these. We do have sets of items but they are more like a line of products."

Right - the distinction matters, and the green colour was claiming something the game does not do
yet. The material lines (leather through spectral) now carry NO colour of their own: a rare Steel
Helm is yellow, a unique one gold, exactly like every other item. `UiFlags::ColorOracoolGreen` and
its font stay built and idle, reserved for the real set system - items granting bonuses when the
whole set is worn - whenever it is designed.

## 2. Bright yellow is irreplaceable - so the green ramp moved

> "Rare items color to be bright YELLOW. Right now they are indistinguishable from uniques."

The healed yellow (PAL16_YELLOW's pale gold) sat too close to unique whitegold - the user is right
that the distinction died. But TRUE bright yellow lives only in the PAL8_YELLOW mini-ramp, which
the green had taken. The resolution: **the green ramp moved to the PAL8_ORANGE minis** - the
second-least-used run in the original frame audit (~2,400 px, mostly its near-black darkest shade),
with exactly one code consumer (the automap's player marker, re-pointed to PAL16_ORANGE). Bright
yellow returns untouched: rares blaze again, the RMB ring and automap bright lines revert to their
vanilla indices, the class-skill plate trio remap is deleted, and `ColorOracoolGreen` now loads
yellow.trn shifted +8 onto the relocated green.

## 3. Zeal to the user's arithmetic - every swing compressed, first included

The spec, verbatim: a ~20-frame attack gives a 30-frame budget; 2 swings of 15, 3 of 10, 4 of 7.5,
5 of 6. And yes - the engine handles it, with two existing mechanisms:

- **Frame skipping** (the same `skippedAnimationFrames` vanilla uses for Fast/Faster/Fastest Attack
  items): every Zeal-armed swing - the FIRST included, which the previous build got wrong - skips
  down to its per-swing budget, keeping the frames leading into the true hit frame so the blow
  still lands honestly. `ZealPerSwingTicks` computes `_pAFrames * 150% / strikes` from the
  character's real animation, so the user's numbers hold at any weapon speed (7.5 floors to 7).
- **Recovery cutting**: a chained swing hands over the moment its blow has landed rather than
  playing its recovery; only the burst's final swing follows through, so the flurry ends on a
  complete motion.

Net effect at level 12+: five distinct swings, each ~6 frames, pivoting between targets, inside
150% of one ordinary attack - the exact picture the user described.

## State

**368/370** - the usual two.
