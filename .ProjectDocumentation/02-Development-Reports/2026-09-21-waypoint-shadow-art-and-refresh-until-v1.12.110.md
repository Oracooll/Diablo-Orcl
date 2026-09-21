---
version: v1.12.110
date: 2026-09-21
area: Assets / waypoint object, Griswold's shop
tests: 832/832
---

# The waypoint gets its shadowed repaint, and Refresh until gets a button of its own

## The asks

> take orclwayp.png from [...] Item Sprites\ and replace the current waypoint assets we are using. The new ones
> have a bit of a shadow to them.

> Refresh until is a bit of a cheat, so if someone activates it put a button somewhere bellow the grid near the
> gold counter.

## The waypoint: the pipeline had to change before the art could go in

The supplied file is **288 x 106** - two 144 x 106 frames. Probing the existing pipeline against the OLD painting
printed `frame 144x106`, so the new file is a **1:1 repaint at the CEL's exact frame size**, not a painting to be
fitted. `tools/WaypointCel.cs` did two things to its source that were both wrong for this:

### 1. It bbox-scales to 144 wide

The tool finds the tight content box across both halves and scales it to fill the frame. The new art's content box
is *tighter* than its frame - that margin is where the shadow lives. Fitting it would have enlarged the platform
and cropped away the very thing the repaint adds, then resampled every pixel of a sprite already at final size.

`WaypointCel.cs` now takes an **EXACT path** when the source is exactly two FrameWidth-wide frames: the halves are
cloned 1:1, no bbox and no scale. The fitting path is untouched, so the original 1536x1024 painting still builds.

### 2. It drops pixels darker than luma 26 - which WAS the shadow

`LumaCut = 26` exists so a soft painted aura can fade out by *area*, the only kind of fade binary CEL transparency
has. Measured against the new art, that cut discarded **1766 of 6721 pixels in the dormant frame - 26% of the
sprite**: the platform's own dark stone and the shadow beneath it.

The first CEL built came out as a skeleton of blue glow lines with no platform under it. The quantised previews
were what caught it - the numbers alone looked plausible.

So the luma cut is now a parameter: `LumaCut` on the fitting path, `ExactLumaCut = 0` on the exact path. Finished
pixel art has no aura to fade, so the cut has nothing to do there except delete the subject.

Result: opaque pixel counts now match the source exactly (6721 dormant, 6499 active, against the source's own
alpha>=96 counts of 6721 and 6499). CEL grew 13316 -> 14428 bytes, which is the shadow.

### Where the art lives

`Resources/01-in-use-assets/world/waypoint-2-states-shadowed-288x106.png`, and `build_waypoint_cel.cmd` points at
it. The original painting stays beside it.

## Refresh until: a seventh button, set apart

Six frames in the row over the painting are Griswold's ordinary services. This one rerolls the shelf until
something wanted appears, is off by default, and the user calls it a cheat - so it is drawn **apart from them**,
at (120, 627): below the grid, right of the gold count, clear of it even at eight digits.

**When the option is off it is not drawn at all**, rather than drawn greyed. The six are fixtures and grey out
when a tab cannot do them; an opt-in the player has not enabled should not be a disabled button to wonder about.
That is the one thing `ShopServiceSlotVisible` exists to say, and it is asked in all four places - draw, click,
hover and release - so an invisible slot cannot be clicked through the painting.

Availability comes from `ShopTabHasRefreshUntil`, derived from `GetShopActions`' own rows, so it answers the
option AND the tab gating in one and cannot drift from the list it reads.

It wears Refresh's glyph - none was commissioned for it - with its own hover text. Its position and that text are
what tell the two apart.

## Build

Debug, clean. 832/832. `objects\orclwayp.cel` repacked at 14428 bytes.

## Not verified

The waypoint has not been seen in the world. The quantised previews are checked and correct, but the sprite is
drawn on town's and sixteen dungeons' floors, and the shadow is the part most likely to read differently against a
lit dungeon floor than against the preview's flat ground.
