# 2026-09-28 - The Visual FX Schedule, and the Guardian portal's violet (v1.12.212)

**Date:** 2026-09-28. Debug only.

The user asked for a new page, "Diablo Orcl Visual FX Schedule", rebuilt from the ground up:
- every animated visual effect in the game, the way it currently renders;
- the old review page's card style;
- no accept/remove buttons, but a comment box on each card;
- the ChatGPT Animation Review page left as it is;
- vanilla effects in a tab only if the 255-file publish limit allowed it.

The limit is real (255 files per artifact version), so vanilla effects are left out and the page links to the Vanilla Animation Library instead.

## Rendered by the game

A new preview test, `OracoolPreview.DISABLED_ExportVisualFx` (`test/oracool_audit_test.cpp`), writes one strip of frames per effect to `ORCL_FX_OUT`, plus `fx.txt` (name, frames, cell size, ms a frame).

**What it renders:**
- **Every true-colour PNG effect sheet** (every `MissileSpriteData` row with `colours`), drawn through its own colour table at full light. 16-facing sheets show the east row; the two-row portals show their open row, then their loop row twice.
- **The effects code builds from other art**, through the same functions the game calls:
  - Seismic Slam: Fire Wall ×3, `Tint::Hue` gold.
  - Flame Ring: a ring of Fire Wall flames.
  - Ember Mine: Apocalypse.
  - Death Nova: Flash, both halves, yellow-green.
  - Earthquake: molten.
  - Frost Nova: `ScaleClxList` at 250%.
  - The war-cry ring in four hues (`RingHueForSkill`, `HueCycle`).
  - Ice armour and Astral Projection: hero tints, on a Sorcerer and a Monk.
  - Dark Mending: the Mend glow on a Skeleton.
  - Serenity and Retribution: the `DrawCycledStill` rings, halves behind and in front of the body.
  - Army of the Dead: a green Skeleton run and a green bone burst.
  - The effects whose colour drifts with the clock are captured in real time, with an `SDL_Delay` per frame.
- **Every aura ring**, through the real `BlitAura`.
- **Every fork item tumble**, as `InitItemGFX` loads and scales it.

The strips became GIFs through the review page's `MakeAnimGifs.ps1`.

**The page:** 193 effects on 194 published files:
- 131 skill effects;
- 3 portals;
- 39 aura rings;
- 20 item tumbles.

Left out:
- the Bard's rings, its tumble and its sheets, since the Bard is hidden;
- Brittle Ground's two stills, which are not an animation.

A "Packed but not drawn" list names the 11 sheets nothing draws now.

**Comments:** they save to the page's database, collection `comments`, one document per effect.

## The Guardian portal regression (v1.12.211)

Since v1.12.211, PNG missile sheets keep their own colours, and their pixels index those colours instead of the palette. `DrawMissilePrivate` still drew `RiftPortalPurple` through `GuardianPortalRgbTable()`, a palette-index table, so the portal came out in scrambled colours. The exporter found it: its violet had to be applied by hand.

**The fix:** `GuardianPortalRgbTable(const SpriteColours *)` (`oracool/rift.cpp`).
- **With the sheet's colours:** each colour goes to the same eight-step violet ramp by its strongest channel.
- **Why not a hue tint:** a first try kept each colour's luminance, and blue carries so little that the portal came out nearly black.
- **Without them:** the old palette table, unchanged.
- **Callers:** both scrollrt and the exporter call it.

**Checked for the same fault elsewhere:**
- the shield's divine TRN: an item sprite, in the palette;
- the stone-curse corpse: a CL2.

Neither is affected.

## Test

- **Build:** Debug built clean. The first link failed because the forward declaration said `class SpriteColours` where the type is a `struct`, and MSVC names the two differently.
- **ctest:** 878 of 878 passed.
- **MPQ:** no repack needed; no assets changed.
