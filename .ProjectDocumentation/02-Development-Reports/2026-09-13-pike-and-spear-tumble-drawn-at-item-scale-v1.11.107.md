# Pikes and spears lie on the floor at item size again

2026-09-13 — v1.11.107

## What the user saw

A Pike on a cave floor drawn about as tall as the hero: a full-length polearm from the label up past the
player's head. Obsidian Gloves, Crusader Pauldrons and the other drops beside it were normal.

## It was the art, not the code

Every tumble goes through the same path: `GetItemDropAnimIndexFor` picks a sheet, `InitItemGFX` loads
`items\<name>.png` through `LoadPngItemDropSheet` at `ItemAnimWidth` (96), and the draw is identical for
all of them. All twenty fork sheets are the same 1248x160 canvas (13 frames of 96x160), so neither the
mapping nor the frame size was at fault.

Measuring the drawn pixels per frame showed the difference:

| sheet | resting frame content |
|---|---|
| gloveflip | 40x29 |
| quiverflip | 48x38 |
| luteflip | 48x41 |
| **spearflip** (batch 22) | **90x144** |

The batch-22 spear sheet (Spear and Pike bases, `UITYPE_SPEAR` / `UITYPE_PIKE`, and every unique or set
piece built on them) drew its polearm to fill the whole 96x160 frame from frame 8 on. The other five
batch-22 sheets (cloak, relic, lute, quiver, focus) are at item scale and were left alone.

## The fix

Each frame of `spearflip.png` was rescaled so its longest side is at most 48 px, the size the lute and
quiver rest at. The scale is anchored on the content's **horizontal centre and bottom edge**, so the arc
of the throw and the spot it lands on are unchanged; frames 0-1 were already small and are untouched.
Alpha was hardened (below 128 transparent, above opaque) because the loader quantizes to the palette.

Resting frame: 90x144 -> 30x48. Frame count stays 13, so a pike already lying on a saved floor still
lands on a valid frame.

Written to `Packaging/resources/oracool_assets/items/spearflip.png` (packed into oracool.mpq by the
normal build) and the wiki copy `wiki/sprites/items/spearflip.png`. The delivered original in
`Resources/.../batch-22-exotic-tumbles` is kept as delivered.

## Shipped

- Debug and Release built; `oracool.mpq` repacked by both.
- RTM folder: `DiabloOrcl.exe` and `oracool.mpq` replaced. An asset fix needs the MPQ copied, not just
  the exe.

## To look at in game

Drop or find a Spear or Pike: the tumble should arc the same way and come to rest as a short diagonal
polearm, about the length of a quiver on the floor.
