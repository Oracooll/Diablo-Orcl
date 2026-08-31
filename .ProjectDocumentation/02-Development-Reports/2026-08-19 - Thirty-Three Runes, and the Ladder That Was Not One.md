---
date: 2026-08-19
version: 1.8.9
area: Sockets v2 - point 6, the full Diablo II rune set
---

# Thirty-Three Runes, and the Ladder That Was Not One

The wiki said five runes. Diablo II has 33, the art for all of them has been sitting in the MPQ
drop zone since v1.7.8, and the user's directive was blunt: "They should be 33, not 5."

## One generator, one order

`tools/GenRunes.ps1` emits seven things from a single table: the item ids, the AllItemsList rows,
the ICURS ids, the CEL frame widths and heights, the cut specs, and the effect rows. The same
discipline as GenItemSets and GenUniqueItems, for the same reason - a CEL stores no names and no
sizes, so a frame's POSITION in the file is the only thing tying it to an id.

The sheet's grid was **measured** rather than eyeballed: a pass over `item-runes-v1.png` counting
non-green runs found 11 columns and 3 rows with their exact extents. The five specs written by hand
in v1.7.8 were off by up to 6px against that.

## The affixes

D2's own numbers wherever the channel exists. The good news, found by reading `ItemSpecialEffect`
rather than assuming: this engine has far more of D2's vocabulary than the launch five used.
Knockback (Nef), the attack-speed and hit-recovery ladders (Shael), FastBlock (Eld, Shael), Thorns
(Amn), StealLife5 and StealMana5 (Amn, Vex) and TripleDemonDamage (Pul) are all real flags, so
those seven runes needed no substitute at all.

Nine do, and each names its substitution on its own row: poison and cold have no channel (Tal,
Thul -> magic resist), and crushing blow, open wounds, deadly strike, freeze, blind, "monster
flees" and "prevent monster heal" have none either (-> flat or percentage damage).

**Two runes act on the host item rather than on the totals**, which is why they are called out:

- **Hel** reduces the host's own strength/magic/dexterity requirements by 20%, capped at 60% so six
  of them cannot delete a requirement. Read in `Player::CanUseItem` and in the panel's requirement
  line through `EffectiveRequirement` - never written into the item, because a requirement written
  back would compound on every character-sheet recalculation.
- **Zod** is written in, and deliberately: "indestructible" is a durability VALUE this engine
  already has, and ten separate decrement sites test for it. `_iMaxDur` is left intact so
  extraction can hand back a destructible item.

## The ladder that was not one

The first test run failed, and it failed on something real. `GenRunes.ps1` emitted rows only for
the 28 NEW runes, leaving the five from v1.7.8 on their old numbers - El 3, Tir 7, Ral 11, Ort 15,
Sol 19, spaced as though five were the whole set. Once the other 28 landed between them the ladder
stopped climbing: **Ral gated at 11 while Tal, three places earlier in D2's order, gated at 20.**

Fixed at the source rather than patched: the generator now emits all 33 rows, the five shipped ones
included, into their own include at their original enum positions. Their indices do not move -
those are positional save format - but their depth and price do:

| Rune | qlvl before | qlvl now | value now |
|---|---|---|---|
| El | 3 | 3 | 1,200 |
| Tir | 7 | 9 | 1,532 |
| Ral | 11 | 23 | 2,823 |
| Ort | 15 | 26 | 3,190 |
| Sol | 19 | 34 | 4,603 |

The ladder now maps D2's own span (El at level 11, Zod at 69) onto our 1-96 area ladder, so El is a
first-floor find and Zod is a deep Torment one. Value climbs 13% a rung to a 30,000 cap, which is
what makes Ascend Runes a real ladder rather than a formality.

## A test that earned its place

`AllThirtyThreeRunesExist` walks the 33 in D2's order and asserts three things: every one is seen
as a rune (the two-island `IsOracoolRuneIdx` - a single range would have silently classified 28 of
them as ordinary misc items that never drop and never socket), the qlvl ladder never goes
backwards, and every rune grants SOMETHING in some host.

That last check flagged Tir, and this one was the test's fault rather than the data's: Tir's
mana-per-kill is an event resolved in `RuneManaPerKill`, not a totals field - the same shape as the
Skull's life-per-kill. Exempted alongside Hel and Zod, with the reason written down.

`HelReducesRequirementsAndZodPreventsBreaking` pins the two host-acting runes, including that
`_iMaxDur` survives Zod.

## Verified

**450 tests, the usual two** (`Drlg_l1.CreateL5Dungeon_diablo_3_844660068`,
`Timedemo.WarriorLevel1to2`). 448 CEL frames, all 28 new icons cut clean at 28x28 through the green
key - no emerald-style chroma trap this time, the tablets are grey on green.

## Next

Point 7 - the runeword table - is the remaining unblocked item. Points 5 and 8 still wait on
Levski's Roar.
