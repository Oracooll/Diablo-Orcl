# Every icon was eleven frames out, and the uniques page was missing Diablo's own

**Version:** 1.11.066
**Date:** 2026-09-12
**Branch:** renderer-32bit

## What was asked

> "fix the uniques generator to include the 110 vanilla ones"
> "pictures in sockets and gems are completely misplaced and wrong. audit them again."

## The uniques generator

`BuildWiki.ps1` read only `unique_items_data.inc`, so the page listed 250 of the game's 360 uniques
and not one of Diablo's own - no Undead Crown, no Butcher's Cleaver, no Optic Amulet. The vanilla
`UniqueItems[]` rows in `itemdat.cpp` share the include's row shape, so one parser now reads both, in
the order the compiler sees them. **360 uniques: 110 vanilla, 250 Oracool**, with a new `origin`
column and filter on the page.

Two details the old parser would have got wrong on the vanilla rows:

- A power can carry two values, one, or none - `{ IPL_RNDSTEALLIFE }` has no numbers and
  `{ IPL_INVCURS, 77 }` one. The old pattern demanded two and silently dropped every such power.
- The row's own regex groups are captured before the per-power matches run, so nothing downstream can
  overwrite them.

## The icons: off by eleven

The user was right, and the cause was mine. Three compounding bugs:

**1. The enum parse dropped an entry per include.** The cursor enum's entries were matched with a
required trailing comma - but the last entry of each of the ten `.inc` files has none. Ten includes,
ten dropped entries, every index after each one walking backwards.

**2. Alias entries were counted as frames.** Three markers of the form
`ICURS_ORACOOL_LAST = ICURS_ORACOOL_UNQBASE_ARCANE_FOCUS` name no frame of their own. The old pattern
read *both* names as entries and advanced the counter for each, shifting everything after them again.
Comments were also stripped first, since an `ICURS_` name mentioned in one would count as an entry.

**3. The export itself was shifted by eleven.** This was the big one. The width tables begin with the
eleven non-item cursors - hand, identify, repair, ... hourglass - before the items start, so a frame
index is **not** an item's `_iCurs`:

```
_iCurs = frameIndex - (CURSOR_FIRSTITEM - 1)        CURSOR_FIRSTITEM = 12
```

The exporter enumerated raw cursor ids from 1, so every icon in the wiki was eleven frames out. That
is a nasty failure mode: nothing looks broken, each item simply wears a neighbour's picture.

## How it was proved, rather than assumed

Counting was not enough - two audit agents had already disagreed about a count this session. The
chain that settled it:

1. **Per-include contiguity.** Each `.inc` declares a consecutive block of enum entries, so their
   computed indices must be N consecutive integers. Eleven of thirteen passed; the two that failed
   were exactly the two holding alias markers, and subtracting those made their spans match (143 and
   107). The enum parse was then self-consistent.
2. **The engine's own arithmetic.** `cursor.cpp:523` asserts
   `ICURS_ORACOOL_FIRST == InvItems1Size + InvItems2Size + 1 - CURSOR_FIRSTITEM`. Sheet 3 starts at
   frame 240, and `240 - 11 = 229 = ICURS_ORACOOL_FIRST`. The corrected top index, `865 - 11 = 854`,
   equals `ICURS_ORACOOL_LAST` exactly. Both ends check out.
3. **Looking at the pictures.** Before: `ICURS_ORACOOL_GEM_SKULL` showed a robe and `ICURS_ORACOOL_
   GEM_TOPAZ` a pair of gauntlets. After: 376 is an unmistakable skull, 374 an amber gem, 377 a rune
   stone, 706 a red orb. A skull is not a thing one mistakes.

The lesson worth keeping: an icon table that is uniformly shifted still renders a picture in every
row, so it passes every structural check. Only the arithmetic against the engine's own constants, and
then actually looking, catches it.

## Result

**855 icons, indices 0-854**, all 452 base items resolving one, none out of range. The sockets page's
gems, runes, jewels, charms, Mystic Orbs and salvage materials now show the right pictures.
