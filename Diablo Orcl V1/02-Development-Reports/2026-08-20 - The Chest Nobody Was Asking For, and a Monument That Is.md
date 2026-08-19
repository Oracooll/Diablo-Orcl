---
date: 2026-08-20
version: 1.8.62
tags: [assets, objects, town, cel, postmortem]
---

# The Chest Nobody Was Asking For, and a Monument That Is

Two town objects. One was reported broken by the user — "new chest is not in" — and the
diagnosis is the more useful half of this report. The other is Levski's Roar finally getting
the art it has been standing in for since it was placed.

## The chest: the art was fine, nothing read it

v1.8.61 built `objects\orclstash.cel` from the delivered sheet, installed it into both asset
channels, repacked `oracool.mpq`, and reported a clean build and 470 tests. Every one of those
statements was true. The chest still looked exactly as it had the day before.

`AddStashChestObject` ends like this, and had since 2026-08-18:

```cpp
// The Grand Reliquary is NOT applied (user: "bring back previous Stash chest. This one is
// not goodlooking"). ...putting it back is uncommenting two lines rather than redoing the work.
// ApplyStashChestGraphics(*chest);
```

The stash chest is an ordinary `OBJ_CHEST3` whose sprite is swapped onto the *instance*, because
retargeting the type's `ofindex` would repaint every large loot chest in the dungeon. When the
user rejected the reliquary art two days ago, the mechanism was parked along with it — correctly,
and with a comment saying exactly how to bring it back.

So the file `orclstash.cel` was a file nothing loaded. Rebuilding it changed a resource no code
path touched.

**The general shape of this is worth naming**, because it is the fourth instance in a week: the
MPQ that was never repacked, the wiki bundle that went stale, the runeword book drawn from a
gated block, and now this. Each passed a green build and a green test run while being visibly
wrong, and each had the same missing step — *nothing verified that the thing produced was the
thing consumed*. A CEL has no test. The only check is the screen.

Both calls are live again: the one in `AddStashChestObject`, and its twin in `SyncObjectAnim`.
They must move together. `_oAnimData` is a pointer, skipped by `LoadObject` and rebuilt from the
type's `ofindex`, so with only the first the chest would wear its art until the first return to
town and vanilla `chest3.cel` forever after.

## Levski's Roar gets its own sprite

The monument was placed on 2026-08-19 wearing `objects\rockstan.cel` — the Anvil of Fury's rock
stand, the nearest anvil-shaped placeable object in the table, and labelled a placeholder in three
separate comments. It now has `objects\orclroar.cel`: 192×204, one frame, built by the new
`tools/MonumentCel.cs` from the user's painting.

Three decisions in that tool are worth recording.

### One frame

`OBJ_STAND` is static — `animLen` 0, no `Animated` flag. The chest needed six frames because it
opens. Six copies of a monument would be four times the file for nothing.

### The anchor is the plaza's near corner, not the centre of the content box

`DrawObject` places a sprite bottom-anchored and horizontally centred:
`screenPosition.x = targetBufferPosition.x - CalculateWidth2(sprite.width())`, i.e.
`(width - 64) / 2`. Whatever lands at the frame's horizontal centre is what stands on the tile.

The painting has a long hard cast shadow running up and to the left, reaching well past the plaza.
Framing on the content box would have centred the frame on the shadow's midpoint and pushed the
monument most of a tile to the right of where it was placed — a sprite that looks correct in
isolation and is wrong in the world.

So the anchor is measured rather than assumed. The lowest opaque row is the near corner of the
base diamond, which is where the floor visually is; its midpoint is the diamond's own axis, and
the frame is padded symmetrically about that column. Measured: content box 896 wide, anchor at
x=741, so 75px of dead space added on the right to balance the shadow on the left.

The *widest* row would have been the wrong measurement — it picks up the flower beds, which are
not symmetric about the plinth. Same distinction the reliquary's shadow planner had to make, for
the same reason.

### The shadow stays as drawn

CEL transparency is binary; a soft shadow cannot survive the format, which is why every vanilla
object's shadow is painted into its art. This one already is. Removing it would mean keying out a
large black region by luma, which would also eat the monument's own darkest stone. Kept, on the
same principle as the chest.

### Scale

192px — three town tiles. The plaza then reads as a plaza rather than a piece of furniture, and
the statue stands about twice a player's height, which is what makes it a monument.

### Wiring

New `OFILE_ORCLROAR` after `OFILE_ORCLSTASH`, `"orclroar"` in the master load list,
`OracoolLevskiRoarAnimWidth = 192` in `objdat.h`. The monument stays an `OBJ_STAND` and the sprite
is swapped onto the instance — same trick as the chest, and for the same reason: the Caves' actual
rock stands, including the one the Anvil of Fury quest replaces, must stay rock stands.

The `SyncObjectAnim` twin matches on **type** rather than on a position constant. Unlike the chest,
the monument has a fallback placement chain if `{55,66}` is occupied; a coordinate test would
silently stop matching on the day a fallback fired. Town has exactly one `OBJ_STAND`, and the
`currlevel == 0` test already excludes the Caves.

## Verification

Build clean. 470 tests, the two standing baseline failures only
(`Drlg_l1.CreateL5Dungeon_diablo_3_844660068`, `Timedemo.WarriorLevel1to2`).

Both CELs were decoded back to PNG and checked by eye before shipping — six 76×66 chest frames
in closed/ajar/open order twice over, and one 192×204 monument. `oracool.mpq` repacked: 395 files.

Neither of those checks would have caught the chest bug, and that is the point. **The screen is
the only verification**: the chest needs a look in town, and so does the monument's footprint
against the tiles around `{55,66}`.

## Files

- `Source/objects.cpp` — both `ApplyStashChestGraphics` calls restored; new
  `ApplyLevskiRoarGraphics` plus its `SyncObjectAnim` twin
- `Source/objdat.h` / `Source/objdat.cpp` — `OFILE_ORCLROAR`, `OracoolLevskiRoarAnimWidth`
- `Source/oracool/levski_roar.h` — the placeholder-art note replaced with what is actually there
- `tools/MonumentCel.cs`, `tools/build_levski_roar_cel.cmd` — new
- `Packaging/resources/oracool_assets/objects/orclroar.cel` — new, 192×204, 1 frame
