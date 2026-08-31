# Two geometry errors in the aura rings (v1.9.44)

The rings shipped in v1.9.43 and were seen running for the first time. They drew, they sat under the
world, they blended, they pulsed. They were also in the wrong place and the wrong size, and both
errors were mine, in arithmetic, in the one file nothing tests.

## What the screenshots showed

Sixteen auras, each a ring sitting roughly a tile south of the character, at what looked like about
two-thirds the size the aura's stated radius implied. Nothing about it crashed, logged, or failed a
build. It looked *nearly* right, which is the failure mode this project keeps producing.

## Error one: the sign on the vertical offset

`DrawAuraGround` repeats `DrawFloor`'s walk to learn where the player's tile lands on screen, then
placed the ring at

```cpp
targetBufferPosition + Displacement { TILE_WIDTH / 2, TILE_HEIGHT / 2 }
```

`targetBufferPosition` is the tile diamond's **bottom**-left. `RenderTile` draws *upward* from it —
`world_draw_black_tile` passes `TriangleHeight` as the height, and `TriangleHeight` is
`LowerHeight + TriangleUpperHeight` = 16 + 15 = 31, rendered bottom-to-top from `sy`. So the tile
occupies the 31 rows *above* that point and its centre is half a tile height **above**, not below.

The `+` should have been `-`. A 32-pixel error, exactly one tile height, which in this projection
reads as a full tile straight down.

## Error two: the projection factor

Worse, because it was a wrong idea rather than a wrong sign. The blit scaled the art by
`radiusTiles / 8`, on the belief that the 512×256 frame *was* the eight-tile radius.

One tile step on screen is `(TILE_WIDTH/2, TILE_HEIGHT/2)` = (32, 16), not `(TILE_WIDTH, 0)`. A
world-space circle of radius R tiles therefore projects to an ellipse with semi-axes `R·32·√2` and
`R·16·√2` — the extreme point lies on the diagonal, and that is where the √2 comes from. So 512 px
of half-width is `256 / (32·√2)` = **5.66** tiles, not 8.

Every ring was drawn at about 71% of the radius it claimed. Now:

```cpp
const int dstW = 2 * radiusTiles * (TILE_WIDTH / 2) * Sqrt2Num / Sqrt2Den;
const int dstH = 2 * radiusTiles * (TILE_HEIGHT / 2) * Sqrt2Num / Sqrt2Den;
```

with √2 as 181/128. At the eight-tile cap that is 724×362, so the art is now *enlarged* by up to √2
rather than only ever shrunk. The header comment promising the opposite is gone. The stretch is
acceptable: the source is all soft gradient with no edge to alias, and the dither is indexed by
**destination** coordinates, so it stays fine-grained however far the source is pulled.

This also makes a comment in `aura_field.cpp` come true for the first time — "eight is about the
point where the field covers everything already on screen." At R=8 the ring is now 1024 px across
the diagonal on a 960-wide screen. It was never going to look like that at the old scale, and that
inconsistency was sitting in the source the whole time, readable, unread.

## Measuring the art rather than assuming it

The scale fix depends on the ring reaching the edges of its frame. That was checked rather than
assumed: scanning `aura_might.png` for peak alpha along the centre row and column puts the bright
band at x = 7 and 500, y = 8 and 243 — semi-axes of about 248×120 in a 512×256 frame, ratio 2.1:1.
The ring does fill its frame, so frame half-width is ring radius, and the arithmetic above holds.
Peak alpha is 166 of 255, exactly the brief's 65% cap.

## What is still not right, and is not a bug

The drawn ring is a circle. The aura **field is a square**: `WithinAura` uses
`WalkingDistance`, which is Chebyshev, and Sanctuary sweeps a `Rectangle { tile, radius }`. In screen
space that square is a diamond with half-width `64R` and half-height `32R`, and the ellipse now drawn
is inscribed in it — touching at the four diagonal extremes and cutting inside along the cardinals.

So a monster standing at the corner of the field is affected while sitting outside the ring. The ring
under-claims, never over-claims, which is the right direction for something a player positions by.
Making it exact would mean a diamond decal, which means re-authoring twenty PNGs. Recorded, not done.

## Verification

Release compiles and links clean at 1.9.44. Debug could not be relinked at the time of the fix — the
game was running and held the exe — so Debug and the suite follow separately.

Neither error is reachable by a test: both are single constants inside a screen-space blit whose
only output is pixels over the world. The standing note that a screenshot is the only verification
here held up exactly, twice in one file.
