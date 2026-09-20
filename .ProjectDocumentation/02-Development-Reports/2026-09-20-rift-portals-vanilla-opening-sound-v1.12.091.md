# Rift portals open with vanilla's portal sound (v1.12.091)

**Date:** 2026-09-20 · **Version:** v1.12.091 · **Tests:** 831/831

User: "when opening rift portals use vanilla poral opening sound."

Vanilla's town portal plays `sentinel.wav` (`LS_SENTINEL`, the TownPortal missile row's cast sound) as it
blossoms. The rift portals' missile rows carry no sound; the gate played the mod's own `UiEventSound::RiftOpen`
cue instead.

- `Source/oracool/stonegate.cpp` `LightGate`: `PlaySfxLoc(LS_SENTINEL, gate.position)` when the gate lights with
  sound (a Nephalem or Guardian rift opened at the monument); the silent relight on returning to town stays
  silent, as before.
- `Source/oracool/rift.cpp` `LayWayHome`: the same sound where the way-home portal rises on the guardian's tile.
- `UiEventSound::RiftOpen` stays defined but nothing plays it now; `RiftClose` is untouched.
