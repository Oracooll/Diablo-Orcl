---
title: 2026-08-11 - Waypoint Sprite and the First CEL Encoder
date: 2026-08-11
tags: [dev-report]
summary: The waypoint stops borrowing the Lazarus magic-circle graphic and gets its own two-state platform, which meant writing the project's first CEL encoder and discovering that a world sprite may only use the upper half of the palette.
---

# Waypoint Sprite and the First CEL Encoder

## Why this one was different

Every asset shipped so far - orbs, plate, menu icons, inventory panel, level-up icon - is a PNG loaded through our own `hud_art` machinery, which can read whatever format we care to give it. The waypoint is a **world object**, so the engine loads it through its own `LoadCel` / `CelToClx` path. Replacing it means producing a real `.CEL`, and nothing in this project had ever written one - `tools/oracool_cel_to_png.ps1` only reads them.

So `tools/WaypointCel.cs` is the first encoder. Two format facts shaped it.

### CEL scanlines are stored bottom-up

Like a BMP. This project already learned that the painful way when the first item contact sheets came out upside down, so the encoder writes rows in reverse and the round-trip check below deliberately compares `preview[y]` against `decoded[H-1-y]`.

### Only palette indices 128-255 are safe

A CEL is 8-bit indexed into the **level's** palette, and the low half is redefined per level type and colour-cycled. The waypoint is the one object in the game that appears in town *and* on all 16 dungeon levels, so anything it drew from the low half would come out a different colour on every level.

Measured rather than assumed - `town.pal` against `l1_1.pal`, `l2_1.pal`, `l3_1.pal`, `l4_1.pal`:

| | low 128 entries | high 128 entries |
|---|---|---|
| l1 vs town | 127 differ | **0 differ** |
| l2 vs town | 127 differ | **0 differ** |
| l3 vs town | 127 differ | **0 differ** |
| l4 vs town | 127 differ | **0 differ** |

The upper half turns out to suit this particular art unusually well: 128-135 is a pure blue ramp for the glow and the flames, 176-191 a blue-grey ramp, and 240-254 a clean neutral grey ramp for the stone.

## A quantiser weighting that threw the colour away

The first render came out warm gold-brown instead of the art's cool grey stone. Cause: the nearest-colour search weighted the channels 3/6/1, discounting blue almost entirely - and blue is precisely the channel that separates the palette's gold ramp (192-207) from its grey ramp (240-254). Weighting it down discards exactly the information the choice turns on. Switching to the usual 2/4/3 put the stone back to the colour it was painted.

## Binary transparency and the aura

CEL transparency is a run-skip: a pixel is either inside an opaque run or absent. There is no partial alpha, so the source's broad soft aura cannot fade the way it does in the PNG. Cutting on alpha alone at a low threshold would have ringed the sprite with a hard-edged rectangle of almost-black.

The fix is to cut on alpha (>= 96) **and** on brightness (max channel >= 26). A pixel that is faint and nearly black contributes nothing against a dark dungeon floor while still costing a hard silhouette edge; dropping those lets the aura fall off by *area* rather than by opacity, which is the only kind of fade the format has.

The bounding box, by contrast, is measured with a much looser cut (alpha >= 40) and shared between the two states, so the platform does not shift position when the game swaps frames.

## Geometry

Frames are **144x106**, two of them: frame 1 dormant, frame 2 lit. The engine draws an object with the frame's bottom-left at the tile's bottom vertex, horizontally centred by `CalculateWidth2` (`(width - 64) / 2`), so 144 centres cleanly and makes the platform read as roughly a 2x2-tile landmark anchored at its bottom tile - which is how every oversized object in the game already behaves. The old placeholder was 96 wide with only 79x38 of content in it.

## Its own file, not a replacement for mcirl.cel

The tempting move was to shadow `objects\mcirl.cel` in `oracool.mpq`, since the waypoint was already borrowing it and it already has more than two frames. That would have been wrong: `OFILE_MCIRL` is also `OBJ_MCIRCLE1` and `OBJ_MCIRCLE2`, the two magic circles in the Archbishop Lazarus quest, and they would have silently become waypoint platforms.

So `OFILE_ORCLWAYP` is a new entry (`objects\orclwayp.cel`) and `OBJ_WAYPOINT` points at it. One consequence worth recording: the new id sits *after* `OFILE_L5BOOKS`, which is the upper bound of `LoadLevelObjects`' loop. That is deliberate - the waypoint is registered explicitly by `EnsureWaypointGraphicsLoaded` on every level rather than by matching a level type, precisely because it is the object that appears everywhere. The two `filesWidths[65]` arrays became `filesWidths[NumObjectGraphicFiles]`, a new constant derived from the enum, since 65 was exactly the old entry count and adding one would have overrun them.

## Verification

- **Round trip**: the encoded CEL decoded back through `oracool_cel_to_png.ps1` - a reader written independently, from DevilutionX's own parser - and compared against the quantiser's own preview. **30,528 pixels across both frames, 0 mismatches.** That the comparison only matches with the vertical flip applied is itself the confirmation that the bottom-up ordering is right.
- **Sub-header trap**: a frame whose first little-endian uint16 is exactly 10 is read as carrying an optional 10-byte sub-header and skipped. An opaque run of 10 would need a following index byte of 0 to collide, which cannot happen while every index is >= 128. The encoder asserts rather than assumes.
- Debug build clean at `ORACOOL_VERSION` **1.1.3**, version string confirmed in `DiabloOrcl.exe`. Tests **347/349**, the same two pre-existing failures as the previous report (`Drlg_l1` stairs, `Timedemo`'s pre-save-break demo character) and no new ones.
- `oracool.mpq` repacked: 12 files, 554,710 bytes, now carrying `objects\orclwayp.cel`.

**Not yet play-tested.** Needs a look at the waypoint in town and on a dungeon level, dormant and lit, to confirm the size reads right against the tiles and that the colours hold across level types - which is the whole point of the palette restriction above.

## Related

- [[2026-08-10 - Waypoint System Complete]]
- [[2026-08-11 - Level Up Icon Under the Clock]]
