# The Dark Lord is not a kind of monster

**Version:** 1.11.075
**Date:** 2026-09-12
**Branch:** renderer-32bit

## What was asked

> "dont put prefix on the dark lord name."

then

> "also - dont recolor the dark lord. keep it or return it to vanilla."

## One cause, both symptoms

Both came from the same place, which is why one guard fixes them: the monster-variant system.
`GetMonsterDisplayName` prefixes the variant's word ("Ashen Skeleton") and `ApplyMonsterVariant`
swaps the palette, and **both ask `VariantOf`**.

`VariantOf` already excluded the two things that carry an identity of their own:

```cpp
if (monster.isUnique() || monster.lesserAffix != LesserUniqueAffix::None)
    return MonsterVariant::None;
```

Diablo is neither. `MT_DIABLO` is placed by his own quest rather than from `UniqueMonstersData`, so
**`isUnique()` is false for him** - and he was picking up a variant like any Skeleton on the floor:
"Ashen The Dark Lord" in the health bar, his own palette replaced by a variant TRN.

## The predicate

`MonsterAvailability::Never`, not a check for `MT_DIABLO`:

```cpp
if (monster.data().availability == MonsterAvailability::Never)
    return MonsterVariant::None;
```

It states the actual reason. A variant is a **kind** of ordinary monster - "Ashen" describes a sort
of Skeleton, not a person - and a monster the dungeon never places at random is not an ordinary
monster. Writing the rule that way also covers the four types that never spawn at all (Wyrm, Cave
Slug, Devil Wyrm, Devourer), where excluding them changes nothing, and re-covers the quest bosses
`isUnique()` already caught.

Nothing is persisted, so this returns him to vanilla immediately rather than only on new games: the
variant is derived from `rndItemSeed` every time it is asked for.

## The test pins the premise, not the guard

`TheScriptedMonstersAreNeverRandomlyPlacedAndSoNeverVariants` asserts that Diablo's availability IS
`Never` - which is the fact the guard rests on - and that an ordinary Zombie's is not, so the guard
cannot have switched the whole variant system off instead of just sparing the bosses.

A test that re-asserted the guard would only restate the code. This one fails if someone edits
Diablo's availability to something placeable, which is the change that would silently bring "Ashen
The Dark Lord" back - and it would not be noticed until the last fight of a run.

`MonstersData` needed `DVL_API_FOR_TEST` to be visible to the suite, the same as `MissilesData`;
`monstdat.h` now includes `utils/attributes.h` for it.

## Verification

Debug and Release build clean; **719/719** tests pass (718 plus the new one). RTM updated.
