# The belt row, the hero's slots, and a gradient that was not there — v1.12.165

2026-09-22

Five changes asked for in one message, and then a sixth that was a correction to my own work.

## The hero's slots lose the texture

> remove the grid texture from backing of items when they are placed in item slots on the hero.

`gridLines` was already exactly that question: the grid loops leave it at its default and the body
slots pass `false`, because a worn item has no cell boundaries to rule. A helm's slot is a painted
shape on the character's doll, not a cell, and a 28px grid slot tiled four times across it says the
wrong thing about what it is.

So the body slots keep `DrawSlotStoneUnderlay` — vanilla's stone — under their tint, for good.

## The belt

> move belt consumables icons 4px to the right.
> apply grid texture also on non consumables slots - portal, burger menu, running toggle.
> tint portal icon blue. tint burger menu icon oragne. tint running icon red for walk, green for run.

**The nudge** was retired to zero when the fifth HUD's belt cells became the painting's own holes.
The user is looking at the current painting and the potions read left in them, so it is `{4, 0}`
again — measured by eye against the art, as the `+3` that preceded it was.

**The three button slots** — menu, portal, run toggle — get the same slot art the item cells wear,
inside the plate, through one small `DrawBeltSlotFace`.

**The tints needed a new draw.** The existing belt-glyph recolour (`DrawTabGlyph`, 2026-09-06)
moves the glyph's white to another **palette index**, which is all an index can do — and it is why
there could be no green: the palette has not one entry of it. `DrawBeltGlyphTinted` writes colour
**values** on a 32-bit target and falls back to the nearest index only on an indexed one.

The glyph's shadow pixels are untouched, so the figure keeps its relief; only the white band
(the grey ramp's top three entries after quantising) takes the colour.

| Button | Colour |
|---|---|
| Town portal | `0x5090E0` |
| Burger menu | `0xF09030` |
| Run | `0x64A064` — the mod's own green, `ColorOracoolGreen`'s band |
| Walk | `0xC04030` — the workshop's own red |

The green and red are existing values on purpose: a run toggle should not introduce a fourth green.

## Ogden's badges move right

> move the badges of items in ogden shops from bottom left to bottom right.

The box is sized to the number it holds, so the **right** edge is the fixed one now and the box
grows leftward as a count reaches three figures.

## The gradient was real and invisible

> i forgot to mention i didnt notice gradient backing. are you sure gradient backing works?

It worked. It was imperceptible, and measuring said why in one line: **the slot art's median
luminance is 5**, and 499 of its 784 pixels are under 20. It is nearly black.

`TintRectRgb` computes `level = (floor + lum * (255 - floor) / 255) * brightness / 100`. With the
span at 15, a median pixel ran:

| span | median pixel (L=5) | L=60 | L=150 |
|---|---|---|---|
| ±15 (shipped) | 27 → 36 — **9 of 255** | 71 → 95 | 144 → 192 |
| ±35 (now) | 21 → 42 — a doubling | 55 → 111 | 112 → 224 |

Nine levels out of 255 is under three per cent, spread down 28 rows and mostly hidden behind the
item's own sprite. The span was chosen against vanilla's stone — far brighter — and never
re-checked against the art that replaced it. That is the whole of the mistake: not the arithmetic,
the constant.

At 35 the dark majority moves by a factor of two, which is what it needs before a ramp reads as a
ramp, and the bright end stays well inside the clamp.

**Where it shows**: in the margin around the sprite and through the sprite's transparent gaps, not
as a wash across the cell. On a 1x1 potion that is a thin border; on a 2x3 sword the ramp runs the
full 84 rows with much more backing exposed.

If it still reads flat the next lever is not the span but the **floor**, currently 10, which is
what carries colour on the black majority.

## Files

- `Source/inv.cpp` — the body-slot gate, the belt nudge, the gradient span
- `Source/oracool/hud_art.cpp` — `DrawBeltSlotFace`, `DrawBeltGlyphTinted`, the four colours
- `Source/oracool/workshop.cpp` — the badge box

Built clean, 832/832. No asset changes.

## What to look at in play

1. A worn helm or sword on the doll: stone under the tint, no grid slots.
2. The belt: potions 4px right, and the three buttons wearing the slot face.
3. The run toggle, walked and run: red and green.
4. Ogden's Gems tab: counts in the bottom right.
5. A 2x3 item on the stash grid - the backing's bottom should read brighter than its top.
