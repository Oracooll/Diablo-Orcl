# The Necromancer, phase N10: art intake, batches 37 and 38 (v1.12.040)

**Date:** 2026-09-18 - Debug only - **819 of 819 tests** - `oracool.mpq` repacked for the Debug tree.

ChatGPT delivered RfA-17's batches 37 and 38 to `Resources/02-concept-assets/delivered-packs/`; batch 40 (the 24
item bases) is in progress there (`chatgpt-rfa40/STATUS.md`), batch 39 (glyphs) is on hold by the request's own
word. Both delivered packs moved to `01-in-use-assets/delivered-packs/`, the runtime files to
`Packaging/resources/oracool_assets/`, two lines in `ASSET-LEDGER.md`.

## Batch 37 - the face

- `ui_art/hero6.png` (180x76): the hero-select portrait. No code - `LoadHeros` already looked for `hero6.png`.
- `ui/silhouette_necromancer.png` (248x356): `SilhouetteArt[6]`, `SilhouetteForClass` sends him there.

## Batch 38 - twelve sheets and the sigils

All twelve registered as `PngOnly` graphics (`MissileGraphicID::BoneTooth` .. `CurseCast`; `bone_spirit.png` is
shipped and registered as `BoneSpiritNecro` but not yet drawn - the book spell's own missile still flies for Bone
Spirit). Ten new `MissileID`s, drawn only, on the RfA-16 machinery:

| Sheet | Missile | How it is used |
|---|---|---|
| `bone_tooth` (32, 16 facings) | `BoneToothBolt` - the javelin's add/process | Teeth: a tooth to each front-arc tile and one down the line; Bone Splinters: three down the line |
| `bone_spear` (96, 16 facings) | `BoneSpearBolt` | Bone Spear: one to the line's end |
| `poison_bolt` (32, 16 facings) | `PoisonBoltFlight` | Blight: to the pool; Poison Nova: sixteen outward, one a facing |
| `bone_spikes` (96, 10) | `BoneSpikesEffect` - census effect | at the cursor |
| `bone_wall` (64x96, 10) | `BoneWallEffect` | one per segment for the wall's 8 s; frames 1-6 rise, 7-10 loop (`BoneWallStandFrame`) |
| `bone_storm` (160x128, 12) | `BoneStormEffect` | 8 s, and it FOLLOWS the caster - `ProcessCensusEffect` moves it to the owner's tile each tick |
| `bone_armor_shell` (96x128, 12) | none - a player-icon overlay | `DrawPlayerIcons` beside the ice shell, `Rfa12BoneShellFrame` (12 frames at 10 a second) while the pool holds |
| `corpse_explosion` (160x128, 12) | `CorpseBurst` | Corpse Explosion, Poison Explosion (with the acid cloud), Death Mark |
| `raise_dead` (96x128, 12) | `RaiseDeadEffect` | Raise Skeleton / Skeletal Mage, Revive, at the corpse |
| `curse_cast` (192x128, 12) | `CurseCastEffect` | every curse cast, at the cursor |
| `bone_hit` (64, 8) | `BoneHitBurst` | registered; not yet spawned on hits (next) |
| `ui/curse_markers.png` (14 x 24) | strip in `hud_art` | `DrawCurseMarkerIcon`; the lettered chip stays as the fallback |

Anchors from the delivery's notes, as `CensusEffectOffset` cases: (cell height - anchor y) - 16, the ring's rule.
Every placeholder (warcry ring, letters) stays behind an `if (!Show(...))`, so a build without a sheet looks as before.

## Not seen

None of it on screen. A screenshot is the only verification - in particular the wall segments' spacing (the notes
warn the visible segment is narrower than the 64px cell), the storm and curse ring heights, and the sigils' place
over heads of different sizes.
