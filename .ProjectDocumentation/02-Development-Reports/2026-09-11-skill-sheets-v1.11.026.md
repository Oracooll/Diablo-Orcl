# Skill sheets from the GPT briefs - v1.11.025 to v1.11.026

**Date:** 2026-09-11, overnight (the user asleep: "dont wait for my approvals. i want all implemented by morning time")
**Branch:** renderer-32bit, local commits only

## What came in

GPT drew the four batches in `Resources\ORCL-skill-asset-briefs.md` between 00:50 and 01:05. Every sheet passed its checks: exact canvas, binary alpha, the preview read by eye, and the facing order for the two directional sheets.

| Batch | Sheet | Shape | Where it shows |
|---|---|---|---|
| 1 Paladin | fist_of_heavens_bolt | 10 x 64x128 | Fist of the Heavens comes down as a bolt of light (was: the mace item's drop tumble) |
| 1 Paladin | holy_spark | 8 x 64 | the ring of sparks Fist throws out on landing (was: ChargedBolt) |
| 1 Paladin | blessed_shield_spin | 16 x 48 | Blessed Shield in flight (was: the shield item's tumble, painted divine) |
| 2 Rogue | magic_arrow | 16 facings x 4 | Magic Arrow (was: the plain arrow) |
| 2 Rogue | guided_arrow | 16 facings x 4 | Guided Arrow (was: the plain arrow) |
| 3 Barbarian | warcry_ring | 12 x 160 | a floor shockwave under the crier, for every cry (was: nothing) |
| 4 Hits | hit_fire, hit_lightning | 6 x 64 | every fire and lightning blow's weapon explosion (was: vanilla's magma burst and charged bolt) |
| 4 Hits | hit_cold | 6 x 64 | packed, nothing uses it yet: weapons have no cold channel |

## How it is wired

- **Pre-registered before the art existed.** The nine sheets are `MissileGraphicID`s whose `MissileSpriteData` rows carry the new `MissileGraphicsFlags::PngOnly`. A PngOnly slot with no PNG stays empty and never looks for a CL2. `MissileArtLoaded(id)` decides at each use site whether the skill shows the new art or keeps what it borrowed. So a `-fix` package, or a missing sheet in a zip built without it, degrades to the old look rather than failing.
- `MissileFileData::name` grew from 20 to 24 bytes, because `fist_of_heavens_bolt` is twenty characters on its own.
- **Holy spark** is swapped per missile in `FistOfTheHeavensImpact`, not on the `MiniNovaBall` row, because a lesser unique's nova uses the same missile and keeps its look.
- **Arrows:** `AddRogueArrow` sets `_miAnimType` before its Arrow/Elemental branch, so the new sheets take the elemental-arrow path (SetMissDir, four frames per facing).
- **Warcry ring:** a new `MissileID::WarcryRing` is appended after `Warcry`, and the table-size `static_assert` now pins it. `AddWarcry` drops one under the crier after a successful cast. The ring draws on the floor (`_miPreFlag`), 64 px down from its tile, because a sprite hangs from its tile by its bottom edge and this ellipse is centred in a 160-pixel cell. A still missile renders at exactly `position.offset` (`UpdateMissileRendererData`). Without the art, the missile removes itself.
- **Hit flashes:** `AddWeaponExplosion` keeps the vanilla graphic's range, so a blow gets exactly as many to-hit rolls as before, then dresses the missile in the flash. Vanilla's explosion ends the tick it lands (`CheckMissileCol` zeroes the range), which is why its fire burst vanishes on a hit. With the flash art, a landed blow plays the remaining frames instead (var3 = no second roll), and the flash holds its near-empty last frame rather than bursting again (var4, `_miAnimAdd = 0`).

## Bug found on the way, fixed

The fire and lightning damage from **Enchant, Vengeance, Fire Mastery and Lightning Mastery never reached a blow** unless the weapon itself carried the fire or lightning item flag. They add to `totals.fireMin/Max` and `lightningMin/Max`, and so to `_pIFMinDam` etc. The character sheet shows it. But `DoAttack` only spawned the `WeaponExplosion` that deals that damage when `ItemSpecialEffect::FireDamage` / `LightningDamage` was set, and nothing but item generation sets those flags. `DoAttack` now also spawns it when `_pIFMaxDam` / `_pILMaxDam` > 0. The Hellfire both-flags path (the spectral arrow) is untouched, since it still keys on the item flags.

## Verification

- Debug build, ctest **697/697** at v1.11.025 and again at v1.11.026. Release built. Both archives repacked (405 files public). RTM refreshed with exe 1.11.026, oracool.mpq, oracool_private.mpq and README.
- Not verified in play. This is all drawing over the world, and a screenshot is the only real check. Things to look at:
  1. Fist of the Heavens: the bolt should land where the blast is. Check its ground point against the target tile.
  2. The spark ring and Blessed Shield: size, and whether the pale yellow survives the palette.
  3. Magic and Guided Arrow: turn and fire in all eight directions, and check the arrowhead leads.
  4. Any Barbarian cry: the ring should spread around the feet, not the chest. If it sits high or low, the 64 in `AddWarcryRing` is the one number to move.
  5. Hit an enemy with Vengeance or Enchant on a plain weapon: fire and lightning damage numbers should now appear, with the new flashes.

## Morning follow-up: Blessed Hammer's hit area (v1.11.027)

The user asked whether the hammer's touch area really travels with its animation. It does: `UpdateMissilePos` derives the tile and the draw offset from the same pixel position, and a still missile renders at exactly that offset. So the checked tile is always the tile the sprite is drawn on. Damage lands as intended: each tile entered rolls the Paladin's magic to-hit (no distance penalty), deals 60% of a weapon roll, and the hammer carries on after a hit.

It had one real gap, though. It checked only the tile it landed on each tick. On the outer turns of the spiral it covers up to 46 px a tick, and a step to a diagonal neighbour passed over the side tile between the two without checking it. A simulation of the same formula showed **4 of the 29 tiles a cast crosses were never checked**, so a monster standing on one was visibly hammered and took nothing. It now checks 8 times a tick (0 missed, against a 1024-step path), hits a tile once as it enters, and ignores an A-B-A flick at a tile corner. The spiral lives in one function, `BlessedHammerOffsetAt`, shared by the sprite and the hit. `Missiles.BlessedHammerChecksEveryTileItCrosses` pins it. Tests 698/698.

Still true by design: a monster that walks INTO the hammer's current tile is not hit until the hammer enters a new tile, and the hammer passes through walls.

## Open

- The ring shows for every cry, songs included (Lullaby, Sound Shock...). Limiting it to shouts is one `if` in `AddWarcry`.
- hit_cold waits for a cold weapon channel.
- Skill Arrow still flies as the plain arrow. It wasn't in the briefs, and a plain arrow is what it is.
