---
date: 2026-08-14
version: 1.5.21
area: In-game menu art, stash grid
---

# Fire in the Wrong Palette, and a Row Under the Orb

> 1. Fix the diablo logo on the in-game esc key menu. The fire is funny looking and the animation is
>    too fast.
> 2. also in the last screenshot you will see last row of stash grid is overlapped by the health orb
>    - we will need to delete this row from existance.

## 1. The fire

The screenshot says more than the complaint does: the **letters render correctly** and only the
flames are wrong. That split is the whole diagnosis, and it matches what the Hellfire audit
established weeks ago - *UI-range art is safe in a level, scene-range art is not.*

`ui_art\smlogo` is front-end art. It came here at 1.5.9 to escape the crash `data\diabsmal` causes
once hellfire.mpq shadows it, and nothing was done about the palette it was authored for:

- Its **letters** use the palette's upper half, which every palette in the game shares by design.
  Drawn in a level palette they are still the right colours.
- Its **flames** are down at indices 10-19, in the scene half - the half recoloured per dungeon type.
  In town those indices are mud and stone. Hence the pink and red blocks over the fire.

Fixed by drawing the sprite through a **TRN** built at runtime: each of smlogo's own colours mapped
to the nearest colour the level palette actually has, matched on RGB rather than on index. Two things
fall out of that:

- The letters barely move, because a colour already present in the target matches itself. The part
  that was right stays right without being special-cased.
- The fire lands in PAL16_YELLOW/ORANGE/RED (192-239), which is where fire belongs.

Matched against `orig_palette` and restricted to 128-255, for the reason `hud_art.cpp` already gives:
that is the stable half, and `orig_palette` is the real level palette rather than the gamma-corrected
copy. Rebuilt when that half actually changes - once per level load, never while the menu is up.

### The speed

Hand-rolled, at a frame every **25ms**: fifteen frames in under four tenths of a second, which is a
flicker rather than a flame. Replaced with `GetAnimationFrame`, the shared clock the **front end's**
logo already runs on at 60ms a frame. So this both slows down and stops being a second opinion about
how fast a Diablo logo burns. `LogoAnim_tick` and `LogoAnim_frame` went with it.

## 2. The stash row

**The screenshot does not show the stash** - it is the pause menu, and it is the only shot from
today. So the row was measured rather than looked at, out of the constants:

| | |
|---|---:|
| `StashGridTop` (24+50+3+24 → +26+4 → +22+8) | 161 |
| `StashCellPx` (INV_SLOT_SIZE_PX + 1) | 29 |
| Row 17's span | **625 – 653** |
| Row 16's span | 596 – 624 |
| Health orb top (`720 - HealthOrbScreenSize.height`) | **624** |

Row 17 sat entirely inside the orb. Row 16 ends exactly where the orb begins. **One row is the right
amount**, which is what the user reported seeing - two independent routes to the same number.

`StashGridRows` 17 → 16. Deleted rather than hidden, on the user's wording: a row you cannot see but
that auto-place can still fill is worse than no row.

### Why the guard did not catch it

There was already a `static_assert` for exactly this:

```cpp
static_assert(StashGridBottom <= 660, "Stash grid now overlaps the central HUD");
```

It passed the whole time, because **660 is the central HUD plate's top and the plate is not what the
stash meets**. The health orb sits to the plate's *left*, is 96px tall, and therefore reaches higher -
and the stash panel is at the screen's top-left, so the orb is its actual neighbour. The assert was
guarding against the wrong object.

Now written against the orb, with the off-by-one stated rather than assumed: `StashGridBottom` is the
row *after* the last, so the grid's final pixel is `StashGridBottom - 1`, and it is allowed to share
the orb's first row - that one line is the grid's bottom rule meeting the very top of the sphere,
where the circle is a few pixels wide.

### Save format

`StashVersion` 2 → 3. The cell count per page changed, so a version 2 stash cannot be read at the new
stride; it is rejected rather than misread, the same path the 10x10 → 10x17 bump took. **Existing
stashes are lost.** That is the known cost of the standing call that saves are not important yet.

Also fixed in passing: `SaveStash` sized its write buffer from a literal `10 * 10` and had been
wrong since the page became 10x17. Harmless - it only reserves - but it is the exact divergence
`StashGridRows` lives in a header to prevent, and there was no reason for that line to be exempt.

## Files

- `Source/gmenu.cpp` - `LogoPalette`, `LogoTrn`, `BuildLogoTrn`, `ClxDrawTRN`, `GetAnimationFrame`.
- `Source/qol/stash.h` - `StashGridRows` 16.
- `Source/qol/stash.cpp` - the asserts, now against the orb.
- `Source/loadsave.cpp` - `StashVersion` 3, and the buffer size from the constants.

## Verification

Debug config builds clean at `1.5.21`; full suite **352/354**, the two known pre-existing failures.
The rebuilt asserts are themselves the check on the stash geometry - they are evaluated at compile
time, so a clean build IS the proof that the grid clears the orb and that no further row would fit.

The logo has to be looked at. Worth confirming: the fire reads as fire in town AND on a dungeon level
(two different palettes, two different TRNs), and the animation now matches the main menu's.
