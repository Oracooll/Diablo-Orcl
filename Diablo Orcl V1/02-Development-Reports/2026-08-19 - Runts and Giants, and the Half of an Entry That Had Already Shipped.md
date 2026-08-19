# Runts and Giants, and the Half of an Entry That Had Already Shipped

**Version:** 1.8.36
**Date:** 2026-08-19
**Tests:** 468 total, 466 passing. The two standing baseline failures only
(`Drlg_l1.CreateL5Dungeon_diablo_3_844660068`, `Timedemo.WarriorLevel1to2`). Four new tests.

## Checked first, and half of it was done

The entry read: *"Giant and runt monsters through the sprite scaler that already ships, plus a
Colossal lesser-unique affix."*

The Colossal half shipped in Phase 3.2 and is thorough - `oracool/monster_scale` with a cache keyed
on (type, percent), the affix wired into `lesser_uniques`, and even the corpse case handled in
`scrollrt.cpp` so a Colossal champion does not shrink at the moment it dies.

What had not shipped was the other half: ordinary monsters, wearing no affix, born at an odd size.
The `MonsterSize` enum had exactly two values, `Normal` and `Colossal`, though its own comment
already anticipated three - *"three of them are already at the limit of what reads as different"*.

So the remaining work was real, and smaller than the entry implied.

## Derived, not stored

A monster's size has to survive a save and reload unchanged. A runt that grew back on load would
read as a rendering bug, not a feature. Two ways to get that: store it, or compute it from things
already stored.

Storing it means a new field on `Monster`, which monsters carry into the level save files - a save
break, on an entry marked Save: No. Computing it costs nothing, because the level seed and the
monster's own index are both already saved and both already stable:

```cpp
return OrdinaryMonsterSize(glSeedTbl[currlevel], monster.levelType, monster.getId());
```

**Not the cosmetic RNG stream**, despite this being purely cosmetic. `CosmeticRnd` is seeded from
the wall clock and never saved - which is exactly what makes it safe for visual bursts and exactly
what makes it wrong here. The mix used instead is a plain integer hash that touches no stream at
all, so it also cannot shift a deterministic replay.

## Two rolls, and the reason is memory before flavour

The **type** rolls first for whether it has an odd size on this floor at all; only then does each
**individual** roll for whether it is one. Half of types get no variant; a quarter of a variant
type's members are odd-sized, so roughly one monster in eight.

That shape is a memory decision. The scale cache owns six animations per (type, size), so a single
roll per monster would let one floor's skeletons demand a runt sheet *and* a giant sheet on top of
the normal one. Deciding per type caps it at one extra sheet per type - the same order of cost the
Colossal affix already pays, rather than double it.

It also reads better. "The skeletons down here run small" is a place with a character. A random
scatter of sizes across every type at once is noise.

## The sizes

| Size | Percent | Who |
|---|---|---|
| Runt | 75 | Ordinary monsters |
| Normal | 100 | Everything else |
| Giant | 120 | Ordinary monsters |
| Colossal | 140 | The affix, and only the affix |

Giant deliberately stops short of Colossal. A champion has to be the biggest thing in the room, and
two large sizes that read alike would make the affix mean less rather than the giant mean more.
Runt stops at 75 for the opposite reason: below about 70% the sprite sits inside the floor diamond
and starts looking like a dropped item.

Uniques and affixed champions are excluded entirely. Both have a silhouette somebody chose, and
rolling a size on top of that would overwrite a decision with a coin flip.

## Cosmetic only, and that is a choice

Size carries no stat change. A runt is not weaker and a giant is not tougher.

That is what keeps this save-safe: the moment size affects a monster's numbers it is gameplay, it
has to be authored at spawn and stored, and the entry stops being Save: No. If sized monsters should
hit differently, that is a separate piece of work with a save-format cost, and it should be decided
on purpose rather than arrived at.

One honest consequence: hit-testing uses real sprite dimensions, so a runt is a slightly smaller
click target and a giant a slightly larger one. The Colossal affix has had this property since
Phase 3.2 and it has not been a problem.

## Four tests, pinning the derivation's properties

- **Stable.** The same (seed, type, id) gives the same size across calls. This is the save/reload
  property, and with nothing stored it is entirely a question of the function being pure.
- **Never Colossal.** A birth roll must not produce the champion silhouette - a rank-and-file
  monster wearing a champion's read with none of a champion's danger is worse than no variety.
- **At most one odd size per type per floor.** The memory bound, stated as the property that
  produces it rather than as a number.
- **A minority, and both occur.** Across 60,000 samples both sizes appear and together stay under a
  third. A floor where half the monsters are odd-sized has no odd-sized monsters, only noisy ones.

## Files

- `Source/oracool/monster_scale.h` / `.cpp` - two enum values, their percents, and the derivation.
- `test/oracool_audit_test.cpp` - four new tests.
- `Diablo Orcl V1/07-Backlog/Pipeline.md` - entry moved to Shipped; health globes marked deferred at
  the user's request rather than deleted. 36 rows.

## To look for

Walk a floor and watch one monster type. Roughly one in eight ordinary monsters should be visibly
smaller or larger than its fellows, and on any given floor a type is one or the other, never both.
Reload the level and the same individuals must be the same sizes - that is the property most worth
checking by eye, because it is the one no test can prove from inside a single session.
