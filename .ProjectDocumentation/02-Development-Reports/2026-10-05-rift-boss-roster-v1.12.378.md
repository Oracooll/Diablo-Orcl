# 2026-10-05 - The Rift Boss Roster (v1.12.378)

**Date:** 2026-10-04/05. Debug only. All 945 tests pass (5 new). The Debug `diablo.ini` was unchanged by ctest (md5 b3392ce4). Commit 68d7a500. No asset changed.

**Source:** the Rift Boss Roster page (https://claude.ai/artifact/19y6u5hv47hXB6XdcLaRZv), db `verdicts`. All 32 bosses were approved with no comments. The four answers:
- **Q1:** "Let them stay. Have them have different versions with altered stats, tints and size."
- **Q2:** "They may go bigger. Up to 250%."
- **Q3:** "Sure." (a boss may appear off his home floors)
- **Q4:** "No." (no new art or sounds)

## The table (new `oracool/rift_bosses.cpp/.h`)
- **42 rows:**
  - The four faces of old, unchanged: Leoric, the Butcher, Diablo, Na-Krul.
  - Six versions of them (Q1):
    - The Mad King and the Abattoir King (Nephalem).
    - Diablo, the Prime Evil (250%) and the Shadow of Terror (Guardian).
    - Na-Krul Unchained (230%) and Na-Krul, the Void Prisoner (Guardian).
  - The page's 20 Nephalem and 12 Guardian bosses. Guardian bosses are grown to 180-250% for Q2.
- **Each row:**
  - Identity: type, face (the unique row that dresses it), kind.
  - Look: size %, tint (Hue, Glint or Electric) and rgb.
  - Strength: life, damage and armour %, walk and attack pace, resists added, steady.
  - Behaviour: signature, escort and count, summon type.
  - Placement: home tilesets and tier band.
- **Picking (`PickRiftBoss`, called from `OpenCommon`):**
  - The boss is chosen first: any row of the rift's kind whose band (tier N = rung 2N-1 and up) holds the rift's band rung, never one of the last three met.
  - The floor comes second: one of the boss's homes, or any tileset 15% of the time (Q3).
  - This replaces the old coin flips. The four faces' homes repeat the old rule: Diablo on Hell, Na-Krul on the Crypt, the other two anywhere.
- **Strength:**
  - Multipliers apply after the tier scaling.
  - A new boss of an ordinary type first has its base life and damage raised to the rift's floor band (`BandPercent`, 100-400%). A Cathedral zombie in a Hell-band rift is a Hell monster before its x8.
  - The faces of old get no band and no multiplier.

## Hooks
- **monster.cpp:**
  - **GetLevelMTypes:** loads the row's type, escort and summon. A Skeleton King face with no summon still loads a random skeleton type.
  - **SpawnRiftGuardian:** spawns the row's type with its face's unique row.
  - **NewMonsterAnim:** adds the pace's frame skips.
  - **ProcessMonsters:** `RiftBossSkipsThisTick` makes a slow walker lose 1 tick in 4 and a resting stone every tick.
  - **Hits and damage:** `MonsterAttackPlayer`, `ApplyMonsterDamage` and both ranged-attack paths call the signature hooks.
  - **MonsterDeath:** split copies give no XP and no loot.
  - **New exports:** `SpawnRiftBossMinion`, `MoveMonsterInstantly`, `DespawnMonster`, `LevelMonsterTypeIndexOf`.
- **rift.cpp:**
  - The state holds the row index.
  - `OnRiftBossRisen` runs once his slot is named: strength, resists (never all three immunities at once), knockback flag, escort, and his stand rebound at his size.
  - `ProcessRift` runs the boss's timers and the split copies' expiry.
  - `RiftBossName` replaces the four fixed names in the log and the menus.
- **Look and treatment:**
  - **Size:** `monster_scale` draws at the row's size.
  - **Colour:** scrollrt applies the row's glaze, lit as he stands.
  - **Unique treatment:** `IsKnockbackImmune` covers steady bosses. `FightsAsUnique`, the health bar's gold name and resist icons, and `GetMonsterDisplayName` all treat the guardian as a unique.
- **Signatures:** 31 behaviours (`BossSignature`), all on existing pieces:
  - Missiles the monsters already fire: acid, magma, fireball, firebolt, lightning, blood star, flares, Inferno.
  - Player effects already in the game: the player chill, venom, StartPlrHit staggers.
  - Teleports, raises and escorts through the new exports.
  - Missile hits carry only the riding effects, so a signature that lays a missile cannot trigger itself again.
- **Debug:** `riftboss` lists the 42 bosses. `riftboss 24` or `riftboss maw` opens a rift whose bar starts full (`FillRiftBarOnArrival`), so that boss rises on arrival.

## Changes from the page
- **The Endless Maw:** now on the Bone Gasher (MT_YSCAV) with Shadow Beasts. The Devourer and Cave Slugs are worms, and no worm sheet is in the archives; the new sprite test caught it. Malignus's cut-content sheet is present.
- **Stormscale:** lightning strikes where his bite lands. Drakes have no breath to change.
- **Wording:** the Thunder Throne's chain is a doubled bolt (single player has no second target). Sareth's cleave is double damage plus a stagger. Grimwing's rest is a grey glaze with standing still. Kharzug's push is the vanilla one tile.

## The audit (subagent, 8 findings, all fixed before the commit)
1. **Fans and doubled bolts never fired.** These AIs shoot through `MonsterRangedSpecialAttack`. Both paths are hooked now, and the damage is passed in.
2. **List edited while walked.** `TickRiftConjured` erased from the list it was looping over. Expired copies are now gathered first.
3. **Leftover copy ids outside the rift.** A copy's id could outlive the rift and rob a dungeon monster of its loot. `IsRiftConjured` now holds only in the rift.
4. **Expired copies counted as hero kills.** They are now despawned quietly (`DespawnMonster`).
5. **The boss shrank and lost his tint as he died.** The kill marked the rift done before the death animation bound. `IsFallenRiftGuardian` now keeps his look.
6. **Gorefeast's puddle sheet lost after a level change.** It now loads with every build of the floor.
7. **The Defiler Empress played the Defiler quest speech.** She is exempt now.
8. **A failed `riftboss` could force a row into the wrong kind of rift.** A forced row now waits for its own kind.

## Tests (oracool_audit_test, `OracoolRiftBosses.*`)
1. The table holds the four faces unchanged, the six versions, the 22/16 split, sizes, tints, homes and summons.
2. Every rung meets somebody of each kind, and the bands hold.
3. A forced row is drawn at home, and kinds are kept. No face is met twice within three rifts.
4. The guardian wears his row's size, colour, pace and name. Any other slot does not. A steady boss resists knockback.
5. Every sprite the roster names is in the archives.

## Not done
- A dead boss's corpse lies at normal size.

## To check in play
- Sizes up to 250% in corridors.
- The tints on real floors.
- Each signature: `riftboss` with a number.
- Escorts.
- The off-home 15%.
