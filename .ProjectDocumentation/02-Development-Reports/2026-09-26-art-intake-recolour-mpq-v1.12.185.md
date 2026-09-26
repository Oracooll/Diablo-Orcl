# Art intake, hand-recoloured heroes, rift screens, node MPQ packing — v1.12.185

2026-09-26

> "prepare a rfa 28 with frames you want fed into chatgpt ... also - drop the 20% increase in barb sprite."
> "Device Guard has blocked the packer ... i dont understand what this means but fix it. i dont want problems with mpq files."
> "in resources folder you will find two new portal loading screens. apply them."

## RfA-27 intake (icons, sounds, effects)

- Validated independently of the delivery's own checks. **Accepted:** 72 Necromancer glyphs (strip built by
  BuildGlyphStrips.ps1, 0 mismatches against the tree rows), 223 WAVs (mono 22050 Hz 16-bit, loops 44,100 samples),
  `ui\skill_markers.png`. Sound table 306 -> 529 rows; GenSkillSounds.ps1 had been failing since the 2026-09-14
  renames (Double Throw, Increased Stamina) - a rename map fixes it.
- **Rejected (user):** the 90 effect sheets - every one two flat colours, bare geometry, shapes reused, 20 facing the
  wrong way. RfA-29 written with a measurable bar (>=5 colours, moving frames, no copies, directions vs bone_spear).
- **RfA-29:** passed every measurable check (7-8 colours, no identical frames, no shared masks, facings along the
  axis). Reviewed as contact sheets with the user: 86 accepted and placed; Army of the Dead, Bone Prison (stick
  figures), Wave of Light (outline ovals), Ancestral Court (blobs) sent back in RfA-30. RfA-30 is being watched for.
- Engine (agent): 90 MissileGraphicID rows in one block, `IsAwaitingRedeliveryArt`, AddArtEffect anchors,
  AddArtEffectFacing, AddArtBolt, body overlays, skill markers beside the curse sigil, Impact/Arrive cues replacing
  vanilla sounds at the same moment. Every call fail-soft.

## RfA-28 (Barbarian and Necromancer recolour)

- 24 reference frames exported from the player's own sheets (oracool_sprite_export; 255 sheets per body) as six 6x
  plates; brief asks for a pixel-exact recolour by material.
- Delivery pixel-exact (0 alpha mismatches, flat blocks). Leave-one-frame-out test: a per-index table reproduces the
  hand recolour to a mean 8 RGB units, 6% of pixels visibly off; a neighbour-aware model tested worse and was dropped.
- `tools/GenHeroRecolour.js` -> `hero_recolour_data.inc`: 6 tables (hero x tier); unseen indices borrow along their
  16-entry ramp. Both heroes recoloured in every tier (the Barbarian only in light armour before). Dye ids 1-3 / 4-6,
  sprite cache version 8. The Barbarian's 20% scale dropped.

## Rift loading screens

`gendata\cutriftn.png` (Nephalem, gold) and `cutriftg.png` (Guardian, purple); PickCutscene sends rift set levels there
both ways; the vanilla portal CEL is the fallback.

## MPQ packing and Smart App Control

The "Device Guard" blocks (4551) are Windows 11 Smart App Control (state 1). `tools/oracool_mpq_pack.js` writes the
engine's MPQ format (raw sectors) and verifies every entry before an atomic rename; CMake uses it via node for both
archives, as do build_oracool_mpq.cmd and BuildReleasePackage.ps1. v1.12.185's build packed devilutionx.mpq (188) and
oracool.mpq (931) with it.

## Also

- Ice/bone armour shells animated at 100 fps (GetAnimationFrame takes ms per frame) - now 10.
- Asset test: Hellfire probe path (single backslashes), skips only art awaiting redelivery.

## Build

v1.12.185 Debug. ctest: 806 of 818 on the run; the 12 were 6 test exes Smart App Control blocked at discovery (x2
with the shuffle lane) - 3 passed when run directly, 3 after a relink. Every test passes; timedemo skipped as always.
Commit 74a3795d.
