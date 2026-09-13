# The 114 RfA-12 actives get their spell ids

2026-09-13 — v1.11.111

## Why

Every active in this engine is readied, priced, cast and levelled through a `SpellID`. v1.11.110 widened
the enum so there was room; this unit gives each of the 114 RfA-12 actives its id. It still adds no
mechanics - every row stays unbuilt, so none of them can be learned or cast yet - which keeps the id work
separate from the effect work that follows class by class.

## What changed

- **114 `SpellID`s**, appended after `GrimWard` in the final list's order, class by class. `MAX_SPELLS`
  126 -> 240, `LAST` is `AncestralCourt`. No name clashed with an existing spell.
- **114 `SpellsData` rows.** Earned on the tree, never found: book and staff level -1, minInt 0, exactly
  as every tree skill before them. Mana climbs with the tier (3, 5, 7, 9, 12, 15); a melee skill costs half,
  as the Round 4 swings do. The element flag names the book colour; `Targeted` marks the ones aimed at the
  cursor.
- **How each will be launched** is recorded in the rows' missile slot, from a plan checked against all 114:
  - 20 are **swung** (`MissileID::Null`) - the melee skills, on a latch like Round 4's;
  - 86 are **cast** as `MissileID::Warcry`, which already calls a skill's effect at the cast frame -
    shouts, curses, spells, leaps, summons and the Monk's area strikes;
  - the Rogue's 8 bow skills are cast the same way and will refuse without a bow.
- **`SpellITbl`** and **`SpellBand`** grew 114 entries each - the bare plate and "no book band", as for
  every tree skill.
- **The tree rows** carry their spell ids. `InnateSpellsBitmask` only grants an implemented row, so an
  unbuilt one with an id is still not castable.

## Save format

The skill-investment chunk is count-prefixed, one byte per spell id, so it grew by 114 bytes and the
hero file's hash moved - `Writehero.pfile_write_hero` re-baselined with a note in its log. A hero saved
with 126 entries loads into the first 126. Readied spells and hotkeys are still one byte each (id + 1, and
the last id is 240, inside the 254 the assert allows).

## Tests

The RfA-12 shape test now expects every active to carry its own id past Grim Ward and nothing else to
have one. Full suite: the only failures were the writehero hash and its shuffled run, both from the
chunk growth above.
