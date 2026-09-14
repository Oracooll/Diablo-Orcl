# Valkyrie takes skill points and wears her glyph

2026-09-14 — v1.12.011

## Why

The user sent a screenshot of the Rogue's Passive & Magic page, with the Valkyrie cell hovered:

> "2 problems with valkyrie skill - the icon and the fact that it requires book! fix these."

The cell showed the red vanilla Golem spell icon. The tooltip said "Not learned — Raised by books, not by skill
points".

## Cause

Both problems had one cause: the Valkyrie row rode `SpellID::Golem`, the Sorcerer's book spell.

- **The book.** `IsClassTreeRowRetiredAsSpell` is `SpellHasBook(ClassTreeSpellId(row))`, so the row counted as a
  book row. It refused points, and the Rogue could only learn it from a Book of Golem.
- **The icon.** The Abilities page draws a legacy spell's own icon in place of the class strip frame
  (`spell_book.cpp`, `IsLegacySpell`). The Valkyrie glyph from batch 13 was in the Rogue strip all along; it was
  just never asked for.

## Fix

- **A new id.** `SpellID::Valkyrie` is id 245, which makes `MAX_SPELLS` 246. It is added to all four tables keyed
  by the enum:
  - `SpellsData`: -1 book and staff levels, so it never drops; it rides `MissileID::Warcry` into
    `CastRfa12Active`, like the other cast actives;
  - `SpellITbl`;
  - `SpellBand`;
  - the `LAST` marker.
- **The row.** It now names that id. It takes points like any tree active and draws the glyph.
- **The cast.** It is the engine's Golem at the skill's rank, **kept until she falls**. It does not use `Summon`,
  whose spirit lasts thirty seconds. An earlier summon's clock is cleared, so it cannot end her.
- **The description.** It now says she fights until she falls and replaces any other summon.

## Save format

The skill-investment chunk grows by one byte. It is count-prefixed, so a 245-entry hero loads into the first 245
entries. The writehero hash is re-baselined with a changelog line.

## Not verified here

- This build was not run in the game.
- A Rogue who had learned Golem from a book keeps that spell in her spellbook. It is simply no longer tied to the
  Valkyrie row.

## Tests

- `OracoolCensusNotes.ValkyrieTakesSkillPointsNotBooks`:
  - the row's id is `Valkyrie`;
  - it is not a book row, and `SpellHasBook` says no;
  - a level-36 Rogue with a point can invest in it.
