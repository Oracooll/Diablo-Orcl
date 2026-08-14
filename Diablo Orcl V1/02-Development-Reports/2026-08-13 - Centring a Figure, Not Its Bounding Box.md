---
date: 2026-08-13
version: 1.1.85
area: UI / Inventory art, class selection
---

# Centring a Figure, Not Its Bounding Box

## Per-class silhouettes

> Place corresponding silhouettes to Sorcerer and Rogue (in my silhouettes file it is archer).

`tools/CutClassSilhouette.ps1` now loops over a list of cuts instead of carrying one hardcoded
`$COL`/`$ROW`/`$NAME`. Four figures ship: **paladin, archer, sorcerer, barbarian**.

The Barbarian was not asked for. It was cut because enabling the Barbarian by default (below) while
leaving it the only playable class without a silhouette is a gap created by combining the two
requests, and the sheet already had the figure.

`SilhouetteForClass` (hud_art.cpp) is the single place the mapping lives, and the naming trap is
worth recording: the reference sheet's names are the **artist's**, not the game's. Its "Archer" is
`HeroClass::Rogue`, and its "Paladin" is `HeroClass::Warrior`. Bard shares the Archer's, matching
the sprite set it already borrows (playerdat.cpp gives both `"rogue"`). Monk has no figure on the
sheet and simply draws none, which is what every class did before this.

## The alignment bug, and why only two classes had it

> Rogue, Bard, Barbarian silhouettes to be aligned properly.

`DrawClassSilhouette` centres a silhouette by its **canvas width**. That is only the same thing as
centring the figure when the figure is symmetric within its crop - and the cutter tight-crops to the
figure's **bounding box**. The Archer holds a bow out to one side and the Barbarian an axe, so the
box was centred while the body was not.

Measured on the first cut, as the offset of the opaque mass from the canvas centre:

| | offset |
|---|---:|
| paladin | +2.2px |
| sorcerer | −2.6px |
| **archer** | **−34.4px** |
| **barbarian** | **+11.2px** |

Which is exactly why two of the four looked right and two did not - the report named the Rogue and
the Barbarian, and the numbers agree.

### Fixed in the cutter, not the renderer

The cutter now pads each canvas asymmetrically until the figure's centre of mass sits at the canvas
centre. The game needs no per-class nudge table, and any future silhouette is corrected by the same
arithmetic rather than by eye.

**Centre of mass, not of the bounding box.** Mass is what makes a thin protruding bow or haft count
for little while the body dominates - which is the judgement a person is actually making when they
say a figure "looks centred". Re-measured after the change: every class within **0.6px**.

## Barbarian enabled by default

> Enable Barbarian by default. DevilutionX option.

`testBarbarian`'s default flipped from `false` to `true` (options.cpp). It is a Hellfire class that
vanilla hides behind this switch; the option itself stays, so it can be turned off again.

## Files

- `tools/CutClassSilhouette.ps1` - per-class loop, mass centring.
- `Source/oracool/hud_art.cpp/.h` - `SilhouetteArt[]`, `SilhouetteForClass`, per-class draw.
- `Source/options.cpp` - the default.

## Verification

Debug config builds clean at `1.1.85`; full suite 351/353, the two known pre-existing failures.

The four cuts were checked by eye at matched height before wiring (shield/cross, bow, robes, axe -
all the right cards), and the centring was verified numerically before and after.

Not seen in game. Worth confirming: each class shows its own figure, and the Rogue's and
Barbarian's now sit centred behind the equipment slots rather than pushed to one side.
