---
date: 2026-08-16
version: 1.7.19
area: The Monk skill tree, the seventh tier, and per-skill rank caps
---

# The Sixth Tree Needed a Seventh Row

`monk-skill-tree-package.zip` is the sixth and last class tree, and the best-prepared drop the MPQ
folder has produced: a design document with per-rank numbers, an icon brief, and **21 pre-cut
256×256 chroma-green PNGs**, one per skill. Every other tree began with measuring a composite sheet
by hand. This one began with a file list.

That left the interesting work where it belongs — on the two things the Monk's design asked for that
the grid could not do.

## A seventh tier

Every tree so far has been six tiers, gated at character levels 1, 6, 12, 18, 24 and 30. The Monk's
branches are seven skills deep and unlock at **1, 6, 12, 18, 24, 30 and 36**.

The first six thresholds are identical, which is the whole reason this was cheap: extending
`TierLevels` moved no existing skill. The number itself is now named once, as
`ClassTreeTierCount = 7`, and the three places that had a literal `6` — the Abilities window's grid
height, the test's occupancy array, the test's bound assertion — read the constant instead. A
seventh tier that only half the code knew about would have drawn a row nobody could click.

## Per-skill rank caps, and why zero means twenty

Every tree skill until now accepted the same 20 points. The Monk's doc gives **five ranks to skills
1-6 and one rank to each branch capstone** — a capstone is a single decision, not a ladder.

The obvious change is a `maxRank` field on the row struct. The trap is that all 161 rows are
**positional aggregate initialisation**: appending a field leaves the 140 pre-Monk rows silently at
`0`. Had `0` been read literally, every skill in five classes would have quietly stopped taking
points, and nothing would have crashed to say so.

So `0` *means* "the usual cap", read through `ClassTreeMaxRank()` and never off the field directly.
That reading is now the subject of its own test, because it is the kind of convention that survives
exactly as long as someone remembers it:

```
EXPECT_EQ(ClassTreeMaxRank(Might), MaxTreeInvestment);  // declares nothing
EXPECT_EQ(ClassTreeMaxRank(IronRobe), 5);
EXPECT_EQ(ClassTreeMaxRank(Enlightenment), 1);          // a capstone
```

## A ladder, not a grid

The other five trees fill a three-column grid. The Monk's three branches are each a straight
sequence — one skill per tier, seventh at level 36 — so all 21 rows sit in the middle column and the
page reads top to bottom.

The doc says "each skill requires the previous skill in its branch". The tier gate already does
that: skill *n* is unreachable until the level that opens tier *n*, and the levels only rise. No
prerequisite graph was added, which keeps the header's standing claim ("level tiers, not a
prerequisite graph") true for all six classes.

## What actually works

Eight of the 21 rows do something. The rest are listed, described and inert, in the usual way.

| Skill | How it lands |
|---|---|
| Master of the Long Staff | +10% damage and a sharper aim, only with a staff held |
| Flowing Step | Run instead of walk, the same frame skip Vigor and Increased Speed use |
| Iron Robe | Unarmoured: armour class by level plus damage reduction. Light armour: half. Mail and plate: nothing |
| Perfect Vessel | A true tenth of maximum life, plus faster hit recovery |
| Inner Sight | The Monk's own Search, deepened — each point holds the reveal longer |
| Healing Mantra | Held like an aura; mends life per tick, sharing Prayer's channel |
| Spirit Ward | Mana Shield, drinking deeper with every point |
| Enlightenment | A tenth more mana and ten points of every resistance |

`bonusDamage` really is a percentage of weapon damage, so Master of the Long Staff's +10% is the
doc's number exactly. `getHit` is a **flat** subtraction from damage taken, not a percentage, so
Iron Robe's "3% per rank, capped 20%" is spent as points off each blow instead — stated in the row's
own description rather than passed off as the original. Perfect Vessel and Enlightenment read
`_pMaxHPBase` / `_pMaxManaBase`, which are the bases the item totals are added *to*, so "a tenth" is
a true tenth and not a flat number dressed up as one.

The thirteen inert rows are waiting on machinery that does not exist yet: block-triggered counters
(Reed in the Wind, Counterstroke), evade rolls, cooldowns, knockback, ground effects (Tranquility),
death marks (Radiant Palm) and the monster-facing pass (Breaking Current, Temple Bell). Every one of
those already had at least one other class waiting on it.

## A bug the Monk exposed

Spirit Ward rides Mana Shield, and Mana Shield turned out to be **the one ladder that opted out of
this fork's own investment seam**. `GetManaShieldDamageReduction` and `ApplyPlrDamage` both read
`_pSplLvl` directly rather than `GetSpellLevel`, so neither invested skill points nor `+spell levels`
from items ever reached the shield. A Monk with five points in Spirit Ward would have got a shield
that absorbed exactly as much as a Monk with one.

Both now read the effective level. That also quietly fixes the Sorcerer: her Mana Shield has always
ignored the `+spell level` gear she was wearing.

## Verified

**410 tests, the same two pre-existing failures** (`Drlg_l1.CreateL5Dungeon_diablo_3_844660068` and
`Timedemo.WarriorLevel1to2`) and no others — the Mana Shield change disturbed no demo. The grid
invariant test now walks all six classes, and the golden hero hash is **unchanged**: the Monk stores
into the same 30-entry class-tree array the other five use, so the save format did not move.

With the Monk in, all six classes have a tree, and the class-tree system is closed.
