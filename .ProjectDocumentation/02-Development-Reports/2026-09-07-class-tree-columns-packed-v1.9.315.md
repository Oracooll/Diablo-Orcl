# Every class tree packed from the left (v1.9.315)

**Date:** 2026-09-07
**Request:** "abilities seem scattered a bit. audit all abilities trees and arrange them a bit. arrange rule - fill column 1 first, then column two, then column 3 if a third skill at given clevel row exists."

## The audit

A script parsed all 272 rows of the class-tree table (both row shapes) and, per (class, page, tier), compared the columns in use with 0..n-1. 37 tiers were not packed from the left; 40 rows moved, relative order kept:

| Class | Rows | What moved |
|---|---|---|
| Paladin | 12 | Blessed Hammer 2->1; Conversion 1->0; Fist of the Heavens 2->0; Holy Fire/Thorns 1,2->0,1; Holy Shock/Sanctuary 1,2->0,1; Conviction 2->1; Resist Lightning/Cleansing 1,2->0,1; Redemption/Salvation 1,2->0,1 |
| Sorceress | 4 | Shiver Armor 1->0; Cold Mastery 2->1; Lightning Mastery 2->0; Fire Mastery 2->1 |
| Bard | 3 | the three tier-5 capstones 1->0 |
| Monk | 21 | every ladder rung 1->0 (the page had been laid out in the middle column on purpose; the user's rule applies to every tree, and the header note says so now) |

Barbarian and Rogue were already packed. Nothing else about a row changed - page, tier, kind, spell, cap - and the position within a class block (the save slot and icon frame) is untouched, since only the column field moved.

## Verification

The script re-run reports nothing to shift and no two rows share a (tier, column); the 23 class-tree tests (which assert exactly that) pass; suite 683/683.

The script, `tree_columns3.pl`, is a one-off in the session scratchpad; the rule it enforces is stated in the header beside the Monk note.
