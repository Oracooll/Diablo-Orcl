# Books raise spells, points raise skills

**Version:** 1.8.78
**Date:** 2026-08-20
**Part C** of [[Plan - The Skill Picker and the Points-Only Abilities Window]].

> "New rule - Spells cant be affected by skill points, only by books. Vanila D1."
> "we remove spells from skill trees of sorcerer completely."

## The line was already in the data

`spelldat`'s **`sBookLvl == -1`** marks exactly those SpellIDs no book can teach - and the Paladin's
seven were authored that way deliberately ("earned at the tree, not bought", `spelldat.cpp:75`). So
the rule needed no new table and no save break, just `SpellHasBook()` over a field that has been
carrying this distinction all along.

- **Book spell** -> level = `_pSplLvl` + item bonuses. Points refused.
- **Bookless skill** -> takes points as before.

## What it cost, class by class

| Class | Rows retired | Left |
|---|---|---|
| **Sorcerer** | **13** - her whole castable set | 17 rows, **1 implemented** |
| Rogue | 1 (Golem) | intact |
| Bard | 1 (Berserk) | intact |
| Monk | 1 (Search) | intact |
| Paladin | 0 | intact |
| Barbarian | 0 | intact |

**The Sorcerer no longer has a working tree.** Her other sixteen rows are the cold page and assorted
actives, all but one carrying `implemented = false` because this engine has no cold damage and no
chill. She needs new non-spell skills authored - masteries, passives - before her page means
anything. That is content design and is NOT part of this work.

## Retired, not deleted

`ClassTreeIconIndex` is simultaneously the icon-strip position **and** the `_pClassTreeInvestment`
index. Deleting rows would drift the art and misalign every existing save. So the rows stay in the
table and `BuildClassTreePage` skips them - the grid is addressed by page/tier/column, so a retired
row leaves its cell empty and shifts nothing.

## The migration

Points could reach a book spell two ways: the Spells sheet spent straight into `_pSkillInvestment`,
and the Sorceress's tree rows stored there by SpellID. Both doors are shut, so anything left is
stranded where the player cannot reach it - for a Sorceress, potentially her entire pool.

`RefundBookSpellInvestment` runs at the end of `ApplyHeroChunks`, after every chunk, and hands it
back. **Idempotent by construction** - it zeroes what it refunds, so the repair is its own version
gate, and there is no flag to get wrong.

## Changed

- `oracool/skill_points.{h,cpp}` - `SpellHasBook`, `RefundBookSpellInvestment`, and the new first
  gate in `IsSkillInvestable`.
- `oracool/class_tree.{h,cpp}` - `IsClassTreeRowRetiredAsSpell`, the `BuildClassTreePage` filter,
  and a matching refusal in `CanInvestClassTreePoint`.
- `oracool/hero_chunks.cpp` - the migration call and its log line.
- `panels/spell_book.cpp` - the Spells sheet's spend corners deleted.

## Verification

**489/491**, the two standing baseline failures. Suite grew by two:

- `BookSpellsRefusePointsAndBooklessSkillsAcceptThem` - the predicate against both sides of the line,
  and a fully learned Firebolt still refusing a point.
- `StrandedBookSpellPointsAreRefundedIdempotently` - the refund takes book-spell points, leaves
  bookless ones alone, and finds nothing on a second run.

Three existing tests were rewritten to pin the new rule rather than the old one: the skill-point
round trip now uses Zeal, the chunk round trip uses Zeal and Charge, and the page test counts
retired rows instead of expecting every row on a page.

`Writehero.pfile_write_hero` unmoved - no save format change.
