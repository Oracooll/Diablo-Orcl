# Ethereal items wear a purple backing and outline

2026-09-13 — v1.11.120

## Why

> "make a purple-ish backing with purple outline for ethereal items."

An ethereal item is the one piece of gear a player has to treat differently — it cannot be repaired — yet in the
grids it looked exactly like any other item of its quality. Its name already turns grey (GR-7) when it is a plain
item, but a magic or rare ethereal piece kept its tier's backing and gave no sign at all.

## What changed

`InvDrawSlotBack` (`Source/inv.cpp`), the one function every item grid draws its slot backing through — the
backpack, the equipped slots, the stash and the shop grid — has a new branch:

| | Value on the 32-bit screen | Indexed fallback |
|---|---|---|
| Fill | deep violet `0x3A2A52` | `PAL16_BLUE + 12` |
| Outline, 2 px inside the slot | bright purple `0xA070E0` | `PAL16_BLUE + 2` |

Drawn as values through `FillRectRgb`, the way the set green is: the shared palette has no purple ramp, so no index
could hold the colour. The steel-blue ramp is only the 8-bit fallback.

### Precedence

1. **Sockets / runewords** — unchanged, still first ("sockets outrank quality"). An ethereal base carrying a
   runeword is first a runeword; the runeword's green outline says more about it.
2. **Ethereal** — new, here.
3. **The quality tiers** — rare, buffed unique, primal, set, magic, unique.

So an ethereal rare shows purple rather than yellow: ethereal is the fact that changes what the player does with
it, and its name still carries the rare colour.

The inspect-another-player view keeps its own orange, as every other branch does.

## Tests

`EtherealItemsWearAPurpleBackingAndOutline` draws the backing onto an 8-bit surface and checks the fill index, the
outline index, that a socketed ethereal item keeps the socket backing, and that a plain non-ethereal item still
draws no backing at all.

## For the user to look at

The backpack, stash and a shop: an ethereal item sits on a deep violet slot with a purple border.
