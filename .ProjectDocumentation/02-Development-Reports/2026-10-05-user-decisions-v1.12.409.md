# 2026-10-05 - The user's five decisions (v1.12.408-409)

**Date:** 2026-10-05. Debug only. All 954 tests pass (two new). The Debug `diablo.ini` was unchanged by ctest (md5 b3392ce4).

**Two builds:** v1.12.408 built the five decisions; v1.12.409 added their tests.

The audit (rounds 95-121) left five design questions. The user answered them in the question interface:

1. **Javelin range: throw from 8.** Poison, Plague and Anchor Javelin are now thrown from 8 tiles, as their text says (`JavelinThrowRange`). `OracoolRogue375` expects 8.

2. **Skill attacks trigger the on-hit item powers.**
   - **Before the damage** (`SkillBlowItemDamage`): every weapon blow takes triple damage to demons. A melee blow also takes Devastation, Jester's and the Doppelganger roll.
   - **After the damage** (`SkillBlowItemAfterHit`): life steal, mana steal and the random steal, then the item knockback once a swing (not on side blows or spin bursts).
   - **Who calls them:** the RfA-12 weapon blows (`StrikeBlow`, the pooled path, melee and bow) and the class melee skills' fresh blows (`FullBlowAt`).
   - **Already had them:** Whirlwind's blows go through `PlrHitMonst`.

3. **Torment damage cap: an overflow percent.**
   - **The field:** a new `Monster::damageOverflow` (saved in two spare bytes of the monster record; an old save reads 100). It holds the damage that cannot fit the 255 of the bytes. The bytes keep the spread, scaled down together.
   - **The helpers:** `SetMonsterDamage`, `ScaleMonsterDamage` and `MonsterEffectiveDamage`.
   - **Where it is set:** the Torment ladder (ordinary and unique), Hell's unique clamp, the champion/Dread boss profile, the escort lift, the Hollow, Feral and Brutal variants, and rift tier scaling.
   - **Where it is applied:** `RiftBossDamagePercent` now returns the rift run's overflow times the monster's own, the one percent every blow and the health bar already apply. A rift roster boss's own overflow folds into his run percent at rise.

4. **Arena death: re-enter free.** The arena last opened is remembered until its boss falls (`OpenArena`). A map of that kind read again re-enters it as it was left: saved, the boss as wounded as he was, the same seed, the portal kept. The map is not spent (inventory, belt and stash paths). Forgotten on a new game (`ResetNamedEncountersForNewGame`, from `FreeGame`).

5. **Paralysis rolls once per spell.** A spell's repeated damage ticks on one monster are a single hit for one second (`SpellHitGapTicks`, `MonsterMarks::spellHitTicks`). That covers Paralysis's 15% roll and the shared per-hit rules (Momentum, Crescendo). Arrows and skill blows are each their own hit, as before.

**Tests:**
- `OracoolUser1005.MonsterDamagePastItsBytesIsKeptAsOverflow`
- `OracoolUser1005.AnOpenArenaIsReenteredFreeUntilANewGame`

## For the play check
- **Skill attacks:** a Barbarian with a life-steal weapon using Cleave or Seismic Slam should heal. A Pul rune or Civerb's should triple skill damage on demons.
- **Torment Hell champions and Dread bosses** should hit harder than their trash. The health bar's damage line shows it.
- **Arena re-entry:** die in an arena, then read another map of the same kind. You should land back in the same room, and the map should stay in your pack.
- **Lightning with Paralysis** should stun about one time in seven per bolt.
