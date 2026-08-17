# The Grand Reliquary

**Version:** 1.7.71
**Date:** 2026-08-18
**Request:** "start on Unit A" — the first unit of the 2026-08-18 MPQ drop-zone sweep.

The town Stash Chest has been borrowing vanilla `chest3.cel` since the day it was placed. It now
has its own art: a six-frame, 160×160 Grand Reliquary — blackened iron, aged-gold ribs, oxblood
lancet windows, four Cathedral finials — drawn as a fixed orthographic-isometric world object that
sits on Diablo's 64×32 floor diamonds rather than reading as an icon pasted into the scene.

## The package did the hard part

Unusually for delivered art, `orclstash.cel` needed no cutting, no keying and no quantizing. It
arrived as a finished CEL: six frames, bottom-up RLE, binary run-skip transparency, every opaque
pixel already restricted to palette indices **128..255** — the stable global half, so the sprite
does not shift when town's lower palette cycles. The pack's own build decodes what it wrote and
compares all 153,600 pixels back against the quantizer input.

It also read our code before it was written. The frame layout repeats its closed/opening/open trio
across frames 1-3 and 4-6, explicitly so that the fork's existing "**closed is frame 4, open is
frame 6**" logic keeps meaning the same thing. Nothing in `AddStashChestObject`, `OperateStashChest`
or `CloseStashChestObject` needed touching.

## Why the sprite is swapped on the instance, not the type

Two constraints pull in opposite directions here, and the resolution is the whole of this change.

`OBJ_CHEST3` is **shared** with every large loot chest in the dungeon, so retargeting the type's
`ofindex` would turn all of them into reliquaries. The obvious alternative — a dedicated object type
— was already tried once and reverted: a custom `OBJ_STASHCHEST` drew in the wrong order, and the
A/B test that settled the question chose plain `OBJ_CHEST3`. Draw order is a **type-level** property
(`_oPreFlag` / `IsFloorPassObject`, read by `GetObjectDrawPass` in scrollrt.cpp), so reintroducing a
custom type would reintroduce exactly the bug that was paid for once already.

So the type stays `OBJ_CHEST3` and only this one instance wears the new sprite —
`oracool::ApplyStashChestGraphics(Object&)`, which loads `OFILE_ORCLSTASH` and points
`_oAnimData` at it.

### The trap that comes with that, and where it is defused

An instance-level sprite override **does not survive a save on its own.** `_oAnimData` is a pointer:
`LoadObject` skips it, and `SyncObjectAnim` rebuilds it by asking `AllObjects[_otype].ofindex` —
which says `OFILE_CHEST3`. Applied in one place only, the chest would be a reliquary until the first
return to town and an ordinary chest forever afterwards, with nothing reporting anything wrong.

`SyncObjectAnim` therefore carries a matching call, guarded by the same position test the two
existing `OperateObject` / `SyncOpObject` sites already use to pick this chest out of the crowd:

```cpp
if (currlevel == 0 && !setlevel && object._otype == OBJ_CHEST3 && object.position == StashChestPosition)
    oracool::ApplyStashChestGraphics(object);
```

This is the audit lifetime checklist working as intended — *where does it live, is it saved, does it
hold still.* It lives on the instance, it is not saved, and it does not hold still.

## The width rule

CEL stores no width; `LoadCel` must be told. Leaving the vanilla chest's 96 would split this
sprite's RLE scanlines mid-row and render it as garbage, so `OracoolStashChestAnimWidth = 160` sits
in objdat.h beside the enum entry rather than as a bare literal at the call site.

`_oAnimWidth` is set to match for honesty's sake, though nothing draws with it — `DrawObject`
measures the sprite itself (`CalculateWidth2(sprite.width())`), and the field's only real consumer is
the save file, which writes it and its derived `_oAnimWidth2` for vanilla compatibility.

One inherited subtlety worth stating: `AddStashChestObject` still loads `OFILE_CHEST3` before
`AddObject`, even though the chest ends up wearing something else. `SetupObject` looks the **type's**
graphic up in `ObjFileList` and hard-fails with "Unable to find object_graphic_id" if it is absent.
The swap happens after.

Like `OFILE_ORCLWAYP`, the new entry sits after `OFILE_L5BOOKS` — outside `LoadLevelObjects`' scan
range — so it is only ever loaded by the explicit `EnsureObjectGraphicsLoaded` call, never
speculatively on levels that have no stash.

## Half size and mirrored (1.7.72)

> *"this is too big. scale down to half the size and use the mirror asset. try placing it in the
> same tile as the previous chest."*

At 160 wide the reliquary spanned two and a half floor tiles and read as a building. It is now
**76×70 and mirrored**, which is the same footprint class as the vanilla chest it replaced.

The re-cut deliberately does **not** resample the delivered CEL. Those are palette indices, and
resampling them would blend index numbers as if they were colours — index 200 halfway to index 140
is not a colour halfway between them. So the halving goes back to the pack's RGBA masters and
redoes the last two steps of its own pipeline, in the order that pipeline used them: **downsample
first, quantise second.** New tool `tools\ReliquaryCel.cs` + `tools\build_reliquary_cel.cmd`, a
sibling of the waypoint's encoder, with three differences:

- **One shared content box across all three states**, not a box per state. The pack's geometry
  contract is one crop, one scale, one bottom-centred placement, so the chest does not shift when it
  opens; boxing each state separately would break exactly that. Measured 150×140 of the 160² canvas,
  halved to 76×70 (rounded up to an even width, which centres cleanly under the engine's
  `CalculateWidth2`).
- **Mirroring is done on the RGBA master**, not by taking the pack's mirrored CEL. The two are
  equivalent — the pack's mirror is an exact scanline reversal — but flipping before the downsample
  keeps full colour depth through the resample.
- **No luma cut.** The waypoint encoder drops near-black pixels because its soft outer glow has to
  fade out by area under binary transparency. This chest is blackened iron with a hard silhouette;
  the same cut would punch holes in its own shadowed faces.

`OracoolStashChestAnimWidth` moved 160 → **76** in the same commit. That pairing is the one thing
that must never drift: CEL stores no width, so a stale constant would split every RLE scanline
mid-row and render the sprite as garbage. The build script prints the width it produced, for exactly
this reason.

**The tile never moved.** `StashChestPosition { 55, 67 }` is the same constant the borrowed
`chest3.cel` sat on and has not been touched — the chest appearing to sit somewhere else was the
oversized sprite's visual mass, not its anchor. Worth confirming by eye now that the footprint is
sane.

## Verification

Debug build clean at 1.7.72; suite **445 of 447**, the two failures being the standing baseline pair
(`Drlg_l1.CreateL5Dungeon_diablo_3_844660068`, `Timedemo.WarriorLevel1to2`). `oracool.mpq` repacked
to 387 files, with `objects\orclstash.cel` (21,614 bytes, down from 83,328) alongside
`objects\orclwayp.cel`.

**To see it in game:** the chest in town, at its usual spot. It should be a large gothic reliquary
rather than a wooden chest, should open when clicked and close when the Stash panel closes, and —
the part worth checking deliberately — should **still be a reliquary after leaving town and coming
back**, which is the whole point of the second call site. Also worth a glance: whether it faces the
right way, and whether the selection outline sits well against a sprite that is deliberately wider
than the tile it occupies.

## Filed

| Landed | As |
|---|---|
| the re-cut 76×70 mirrored CEL | `Packaging/resources/oracool_assets/objects/orclstash.cel` (shipped; built, not copied) |
| the three RGBA state masters | `02-source-art/world/grand-reliquary-chest-v1.1.0-{closed,opening,open}-rgba-160x160.png` — **the build script's inputs**, so renaming them breaks it |
| both delivered CELs, both state strips | `02-source-art/world/grand-reliquary-chest-v1.1.0-*` |
| `Chest Icon.png` (the approved 1254² master) | `02-source-art/world/grand-reliquary-chest-approved-master-1254x1254.png` |

Six units of the sweep remain; see *MPQ Drop Zone - Intake Plan 2026-08-18*.
