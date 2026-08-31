---
date: 2026-08-16
version: 1.7.14
area: MPQ drop-zone intake, unit 1 - the gem quality ladder
---

# Seven Gems, Five Qualities

First unit of the MPQ drop-zone plan. `Gems.png` is Diablo II's complete gem set as pixel art —
seven types across five qualities — and it replaces both the five stopgap icons cut from a phone
screenshot in v1.7.8 and the flat one-quality gem economy behind them.

## What shipped

**35 gems.** Amethyst, Diamond, Emerald, Ruby, Sapphire, Topaz and Skull, each in chipped, flawed,
normal, flawless and perfect. The five that existed are the **normal** quality of their type and
keep their item indices — the ladder was built around them rather than over them, so nothing
already balanced moved and no item sitting in a save changed meaning. The thirty new ones are
appended, same positional-save-format rule every block here follows.

**One effect row per type, scaled by quality.** Thirty-five gems run off seven rows of numbers and
a percentage: 40 / 70 / 100 / 145 / 200. Normal is 100 because normal *is* the tuned row. Scaling
never rounds a real effect away to nothing — a chipped gem with a small number still gives 1.

The two new types got effects on channels that exist: Amethyst is attack rating in a weapon and
dexterity in armour (both D2's), Diamond is all-three-resists in armour and shields. D2's Diamond
is damage-to-undead in a weapon, which has no channel here, so that half is flat damage.

**D2's actual gem recipe.** The crafting window's first recipe was "three gems of one kind → a
random rune", which is not a Diablo II recipe at all. It is now **Refine Gems**: three identical
gems (same type *and* quality) make one of the next quality, perfect excluded because nothing
refines it. Runes keep their own ladder. This is the recipe that makes a chipped drop worth
picking up.

**Drops walk the ladder.** A gem drop picks a type flat across the seven, then a quality from the
ones the floor's depth has opened, weighted 40/30/18/9/3. Deep floors can still drop chipped
stones; perfect ones stay rare even where they are allowed.

## The emerald, again

The same trap as v1.7.8, and this time measured rather than guessed: the sheet's backdrop reaches
a green-excess of 240 and the emerald's own body reaches **234**. No threshold exists that
separates them, so raising the key would have been guesswork with no correct answer. The emerald
column is instead pre-processed to a black backdrop by exact-colour match against the flat
backdrop and cut with the dark-mode extractor. All five emerald qualities came out clean.

## Verified

**408 tests, the usual two pre-existing failures.** New pins: every (type, quality) pair has its
own index and decomposes back to the same pair, no two share one; normal quality still applies
exactly the tuned numbers; chipped is weaker and perfect stronger; scaling never rounds an effect
to zero; the two new types act on the channels their descriptions claim; refining walks four steps
to perfect and stops there; and a rune is not walked by the gem recipe.

## Next in the plan

Unit 2 — generalize `oracool/paladin_tree` into a shared class-tree module before the Barbarian,
Sorceress and Rogue trees land, so there is one page implementation rather than four.
