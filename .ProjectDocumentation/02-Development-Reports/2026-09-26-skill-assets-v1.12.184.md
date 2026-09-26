# Every skill's sounds and art, checked — v1.12.184

2026-09-26

> When done i want you double check if all skills have their sounds and visual assets in place as they should

## Audit (read-only agents)

- Every shipped file is sound: 306 skill WAVs (mono 22050 Hz 16-bit, loops exactly 2.0 s); six class icon
  strips with one 56x56 glyph per row (Necromancer strip absent — batch 39 on hold); 59 aura rings; 45
  missile PNGs matching their sprite rows; curse_markers.png 14 cells.
- Packed but never used: the Barbarian's ten war-cry shouts (RfA-23, "drop-in, no code" — the cast-cue hook
  had been removed at v1.9.195), cast cues of 16 more skills, Static Field's cue (auras asked only for
  Start), Decoy/Valkyrie arrive cues, Warmth/Enchant start cues, Blizzard's impact cue; hit_cold.png,
  ice_armor_break.png, ice_ground.png.
- Bugs: raise_dead drew half its animation; Rogue Fire Arrow's light stayed at the launch tile and it had
  no impact burst; Skeletal Mages always shot a vanilla arrow; AddAcidJavelin checked the wrong art for
  three necro bolts; a player's minion missiles could hit players.
- Never made: Necromancer icons; dedicated sounds for the RfA-12 and Necromancer skills; 16 silent RfA-12
  Paladin auras; ~90 actives with no visual of their own.

## Decisions (user)

- Delivered cast cues REPLACE the generic IS_CAST2 (war cries + 16 more) — one sound per moment.
- Write an RfA brief for what was never made.

## What changed

- `player.cpp` StartSpell: IS_CAST2 → the tree row's Cast cue when it has one (local player; cold spells
  excluded, their cue already replaces the launch sound).
- `skill_sounds`: `SkillSoundPath` / `HasSkillSound`; StartClassAuraLoop falls back to Cast.
- `companion.cpp`: arrivals play Arrive cues in place of LS_RESUR. `class_tree.cpp`: passives' Learn
  falls back to Start.
- Visuals agent: HitCold flash (`AddColdHitFlash`, WeaponExplosion cold kind) on cold hits without own
  impact; `AddArtEffect` for art with no MissileID (ice_armor_break on expiry); `MissileGraphicID::IceGround`
  under Brittle Ground; census range = frames × ticks per frame; Fire Arrow light follows + MagmaBall
  burst; minion ranged attacks fire `orders.missile` (mages' elements); Bone Armor / Poison Dagger / Bone
  Storm timers; Blizzard shard impact cue. Mine: Holy Freeze flash; minion missiles skip players.
- `spelldat.h` SpellsData and `misdat.h` MissileSpriteData exported for tests.

## Tests

- New `test/oracool_skill_assets_test.cpp`: per visible row, icon frame, every sound cue, aura ring and
  spell missile sheets asked of the mounted archives; plus every registered missile sheet. 432 rows,
  268 cues, 39 rings, 0 missing (Necromancer icons recorded as awaiting art). Table:
  `2026-09-26-skill-assets-report-v1.12.184.md`.
- Strip test now requires frames >= rows for every class.
- Skill rules simulation still 432 rows / 9,230 ranks / 0 discrepancies.
- ctest: **840/840**. Device Guard blocked the MPQ packer (code 4551); no packed asset changed.

## Tools and art pipeline

- `BuildGlyphStrips.ps1`: Necromancer support (necro_tree_icons.png), current Resources path, no empty
  strip written. `GenSkillSounds.ps1`: current zip path, Necromancer class. ~50 other tools still point at
  dead Resources paths (listed in the agent report; untouched).
- **RfA-27** — `Resources\02. Oracooll Assets\ChatGPT RfA\RfA-27 - Skill Icons, Sounds and Effects.md`:
  batch 39 Necromancer glyphs (released from RfA-17's hold), batch 50 (48 aura WAVs), batch 51 (175 cast/
  impact/arrive cues for 133 skills), batches 52-58 effect sheets for 90 actives grouped by kind. Bard deferred.

Commit 1e870470.
