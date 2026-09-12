# Smart Loot: drops lean toward the class that found them

2026-09-13 — v1.11.102

## The request

After a discussion of Diablo III's Loot 1.0 and Loot 2.0:

> "Smart Loot - i like that. we need to develop something similar."

With two constraints from the user that shaped everything:

> "in Orcl mod we don't have stats limits. I am not sure if the barb can have magic or not. I will
> probably replace mana with Rage further down the road. I suggest any item should be able to drop
> with a any hero class, just drop percentage should favour more usable one."

And the D3 tuning: about 80% of drops aimed, 20% left fully random.

The primary/secondary affix split from Loot 2.0 was **declined**: "defies the purpose of unified
affixes library." Not built.

## A correction on the way in

I first proposed rejecting items a class "can never equip", reasoning from the per-class stat caps in
`PlayersData`. That was true of vanilla and **wrong for this fork**: `ModifyPlrStr` clamps to a flat
255 for every class (`player.cpp:3995`), so any class can reach any stat. The user caught it. There is
no filter anywhere in this change - only weighting.

## How it works

`Source/oracool/smart_loot.{h,cpp}`.

**Aim four drops in five.** `SmartLootShouldAimThisDrop` rolls 80%. The other fifth is untouched, so
every item stays reachable by every class.

**Best of three, not reject-until-good.** An aimed drop generates `SmartLootCandidates` (3)
candidates and keeps the one that suits the class best. A rejection loop would need a threshold -
"good enough" - which cannot be tuned honestly: too high and it runs out of attempts and ships the
last candidate anyway, too low and it never fires. Best-of-N needs no threshold, always improves, and
cannot fail. It also cannot become a filter, because a candidate is kept whenever nothing better turns
up.

**The score is the class's own stat profile**, read out of `PlayersData`'s maxStr / maxMag / maxDex:

| Class | Str | Mag | Dex | Reads as |
|---|---|---|---|---|
| Paladin | 250 | 50 | 60 | Strength |
| Rogue | 55 | 70 | 250 | Dexterity |
| Sorcerer | 45 | 250 | 85 | Magic |
| Monk | 150 | 80 | 150 | Str/Dex hybrid |
| Bard | 120 | 120 | 120 | generalist |
| Barbarian | 255 | 0 | 55 | Strength |

These are no longer caps, but they are still the statement of what each class is about, and they
are maintained (Griswold's premium stock reads them). Using them as a *weight* rather than a single
"primary stat per class" is what lets Monk and Bard be honest - a primary-stat table would have to
invent one for each. It also stays correct if the Barbarian's mana becomes Rage: magic gear is not a
Barbarian's axis either way. Vitality is excluded because every class wants it, so it would add the
same amount to every candidate and change no comparison.

What gets weighted:

- **What the item asks for** - its Str/Mag/Dex requirements, weighted double. This needs no
  class-to-item-type table: a staff asks for Magic, a war bow for Dexterity, a great axe for Strength.
- **What the item gives** - its rolled +Str/+Mag/+Dex affixes.

Only equipment is considered. Gold, potions, scrolls and books serve every class alike.

## Where it applies

| Drop source | Aimed? | How |
|---|---|---|
| Monster drops | **Yes** | three finished items scored, requirements *and* affixes |
| Chests, barrels, theme rooms (`CreateRndItem`) | **Yes** | three base indices scored, requirements only |
| Weapon racks etc. (`CreateTypeItem`) | No | the caller chose a type on purpose; left alone |
| Quest and scripted drops | No | untouched |

## Why the seed replay is safe

Every candidate is generated with its **own fresh seed** and is an entirely ordinary item. Smart Loot
changes only *which* of them is kept. The kept item's seed replays to exactly that item, so
`pack_test` - which exercises `RecreateItem`, not the drop path - is unaffected, and the suite
confirms it.

It is a generation change for future drops: the aim roll and the extra candidates draw on the main
stream. That changes what a new game drops, which is the point. Single-player saves store full item
records, so nothing already found changes.

## Test

`OracoolSmartLoot.DropsLeanTowardTheClassWithoutLockingAnythingOut` measures rather than asserts:

- **Profile**: every Magic-only base scores 0 for the Barbarian and above 0 for the Sorcerer; the Bard
  scores both Strength- and Magic-leaning bases (fails if the weights become a single primary stat).
- **Lift**: simulated best-of-3 over the real base-item population must beat a single blind draw by
  at least 20%, for Sorcerer, Barbarian and Rogue.
- **Nothing locked out**: over 300,000 aimed Sorcerer draws, *every* equipment base is still reached.
- **The aim is a coin**: 40,000 rolls land between 75% and 85%.

**Proven by reverting.** Setting `SmartLootCandidates` to 1 (no aiming) fails the lift assertion for
all three classes:

```
best-of-1 barely improved suitability
best-of-1 barely improved suitability
best-of-1 barely improved suitability
```

## A trap hit while writing the test

The first version built its `Player` probes as stack locals and segfaulted in 0.03s. `Player` is far
too large for the stack - the same trap already recorded for the backpack tests. The probes now live
in `Players[1..3]`.

## Verification

- Debug: **731 tests, 0 failed.**
- Release built and linked; `DiabloOrcl RTM\DiabloOrcl.exe` refreshed.
- No asset changed, so no MPQ repack.

## Not yet verified

The test measures the **base-item** scorer in simulation over the real item table. The monster-drop
path scores *finished* items, affixes included, and that path is only covered by the full suite
passing - there is no measurement of the in-game lift yet. The honest check is play: a Sorcerer should
now find noticeably more staves and +Magic gear, and a Barbarian noticeably fewer Magic-requirement
items, while both still see some of everything.

## What to look at in play

1. Play a Sorcerer for a floor or two. Staves, and rings/amulets with +Magic, should turn up more
   often than they used to - but plate, axes and bows should still drop sometimes.
2. Switch to a Barbarian on a similar floor. Strength gear should dominate; Magic-requirement items
   should be rarer, **not absent**.
3. A Bard should feel roughly like before - its profile is flat, so aiming barely changes its drops.
   That is correct, not a bug.
