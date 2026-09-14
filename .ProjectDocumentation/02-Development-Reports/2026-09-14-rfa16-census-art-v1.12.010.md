# RfA-16 enters the game: the census skills get their own art

2026-09-14 — v1.12.010

## Why

> "check for new delivery from chatgpt."

ChatGPT delivered RfA-16 at 16:16. It came in three packs under `Resources/01-in-use-assets/delivered-packs/`,
with 16 runtime assets in all. Every skill built from the census notes had been running on placeholder visuals:

- the throw flew as an arrow;
- the grenade flew as a Fireball;
- Meteor, the thunder bolts and the acid clouds showed as the warcry floor shockwave;
- the two Sorcerer auras had no ring.

## What went in

### Batch 34: two aura rings

`ui/aura_static_field.png` and `ui/aura_thunder_storm.png` were copied into the archive tree. `AuraFiles` already
named both, so no code changed. There are 59 rings now.

### Batch 35: eight sheets

All eight are registered as `PngOnly` graphics (`misdat.h/.cpp`). Without a sheet in the archive the slot stays
empty, and every caller keeps the old placeholder.

| Sheet | Cell | Used by | How |
|---|---|---|---|
| `thrown_sword`, `thrown_axe` | 48x48, 8 frames | Weapon Throw | `ThrowArmedWeapon` dresses the arrow it fires in the spin (`ThrownWeaponGraphic`: the axe sheet if an axe is in hand, otherwise the sword sheet). The flight and damage are unchanged. |
| `grenade` | 32x32, 8 frames | Grenadier | Grenadier's Fireball wears the bomb in flight. `ProcessFireball` still swaps to its own explosion at the end. |
| `acid_javelin` | 64x64, 16 facings | Poison and Plague Javelin | New `MissileID::AcidJavelin`, drawn only. It flies from the Rogue to the tile the skill already struck, and stops there. |
| `acid_cloud` | 128x96, 12-frame loop | Poison Javelin's pool (3 s), Plague Javelin (5 s) | New `MissileID::AcidCloud`, lasting the field's life. Plague's once-a-second shockwave now only shows when the sheet is missing. |
| `meteor` | 96x160, 10 frames | Meteor | New `MissileID::MeteorFall`, cast on the target. It steps every second tick, so 20 ticks: exactly the second before `TickField` lands the impact. |
| `meteor_impact` | 160x128, 14 frames | Meteor | New `MissileID::MeteorImpact`, spawned when the impact lands. Frames 1-10 play once, then 11-14 loop at a slower step for the rest of the field (about 3 s). It is lit. |
| `thunder_bolt` | 64x192, 8 frames | Thunder Storm | New `MissileID::ThunderBolt` on the struck enemy's tile, in place of the shockwave. It is lit. |

- **Missile code.** The four stationary effects share `AddCensusEffect` and `ProcessCensusEffect` in
  `missiles.cpp`. The javelin has its own add and process pair.
- **Placement.** Floor anchors come from the delivery's notes and follow `AddWarcryRing`'s rule (the tile centre
  is 16 px above a cell's bottom edge):
  - meteor and bolt touchdown: offset -13;
  - impact floor centre (80,90): +22;
  - cloud baseline 91: -3.
- **Spawning.** `rfa12_actives.cpp` gets a `Show()` helper. It spawns an effect only when its art is loaded and
  returns false otherwise, so the caller falls back to `Ring()`.

### Batch 36: six glyphs

- **Intake.** The pack shipped with no `manifest.json`, so one was written from the notes (class, page, name,
  file) and the pack was added to `BuildGlyphStrips.ps1`.
- **Script report.** The six old keys (Double Throw, Lord Commander, Long Arm of the Law, Increased Stamina,
  Evocation, Beacon of Ytar) are now listed as expected unused.
- **Frame check.** A frame-by-frame compare against the committed strips found exactly six changed frames:
  Paladin 40 and 42, Barbarian 4 and 16, Sorcerer 32, Monk 31.

## Not verified here

This build was not run in the game. The delivery's own notes flag the same checks.

- **Heights.** The thrown weapons and the grenade are 48 and 32 px cells hanging from the tile like the Blessed
  Shield spin, so they may fly lower than the arrow they replace.
- **Anchors.** Meteor, bolt, impact and cloud anchors are computed from the notes, not seen. They are the likeliest
  numbers to move (`CensusEffectOffset` in `missiles.cpp`).
- **Javelin.** The damage lands at the cast, so the javelin arrives a few ticks after the target reacts.
- **Loops.** Check the cloud and burn loops for seams, and the rings on bright and dark floors.

## Tests

- `OracoolCensusNotes.TheCensusArtIsWiredToItsSkills`: each new missile names its own sheet, and with nothing
  in hand the throw wears the sword sheet.
- `MissileArtLoaded` could not be asked from a test: it reads `MissileSpriteData`, which the test DLL does not
  export (LNK2001, the same trap as `SpellsData`). The first run failed to link on it, and the assertion was dropped.
- `MissilesData`'s positional `static_assert` now ends at `MissileID::ThunderBolt`.
