# The second audit: catching the first audit's own mistakes, and icons for every socketable

**Version:** 1.11.065
**Date:** 2026-09-12
**Branch:** renderer-32bit

## What was asked

> "reaudit the contents of the wiki for incorrect information.
> also - add pictures to all items mentioned in sockets and gems page."

## Why a second pass found so much

Three auditors re-checked all 27 hand-written pages, and were told explicitly to verify the
**previous pass's corrections** as well as the original prose. That was the right instruction: the
first audit's fixes contained four errors of their own.

| Page | What the first pass did | What was actually true |
|---|---|---|
| ui.html | Rewrote the plate-colour table | Left a **duplicate "Grey plate" row**, and the Red row described the HUD meaning, not the Abilities window's ("not yours yet, not learned, or not built") |
| ui.html | - | Still listed the **yellow/red assignment rings**, removed 2026-09-02; the F-key corner badges replaced them |
| ui.html | - | "Four sheets" - there are **five** (Spells, three trees, Passive Skills), contradicting the same page's own line 75 |
| controls.html | Rewrote the skill-well row | Left the **old contradictory row** beneath it |
| affixes.html | Uniques 143 -> 250 | **360**: 110 vanilla plus 250 of this fork's |
| mechanics.html | Added "bows need both hands" | The clause "whatever its vanilla data says" is wrong - every bow is *authored* two-handed; the fork rule is that Heavenly Strength will not one-hand one |

A fifth disputed point is worth recording because the auditors disagreed: one said the D2 runeword
count is **59**, the other **61**. Rather than pick, the array in `tools/GenRunewords.ps1` was bounded
by line number (57-119) and its entries counted directly: **59**, and the dedupe filter drops none.
The page already said 59 and was left alone. Two agents disagreeing is a prompt to measure, not to
average.

## Two more broken generators

**Item sets counted 94 instead of 15.** `BuildWiki.ps1` derived a set's display name from each
*piece's* own `SET_` id, so every one of the 94 pieces became its own set: the stat read 94 and the
card grid drew 94 one-piece cards. The real table is `ItemSets[]` in `item_sets_data.inc` - fifteen
rows carrying a display name and a piece index range. Pieces are now assigned by that range, exactly
as `item_sets.cpp`'s `FindItemSetOwning` does. **94 pieces, 15 sets, 0 unassigned.** This fixed
`sets.html` and the overview's stat at once. The generator also now maps the two authored slots with
no equipment slot of their own - relic to bracers, cloak to legs - so the Slot column stops
contradicting the page's prose.

**Every item's icon index was capped at the vanilla sheets.** The cursor enum `#include`s ten `.inc`
files, and every Oracool icon is declared in one of them; parsing `itemdat.h` alone left 98 items
with no index and 173 pointing past the end of what had been exported.

## Pictures for every socketable

`tools/oracool_art_export.cpp` cut only the two vanilla cursor sheets. The engine has a third -
`data\inv\oracool_items.cel`, loaded as `pCursCels3` - and `GetInvItemSprite` already indexes into
it, so the exporter needed four lines to cut it: **626 more icons, 1.01 MB**. The encyclopedia art
step now walks all three sheets end to end with accumulating offsets, so a `curs` index addresses the
whole run: **866 icons, 1.34 MB**, and all 452 base items resolve one (max index 857, none out of
range).

`sockets.html` gained pictures throughout: an icon column on the gem, rune, jewel and charm tables,
and a labelled gallery for the two families the page describes only in prose - the eight Mystic Orbs
and the seven salvage materials. The join is on the item's enum constant against `WIKI.items`, so
there is no second table to keep in step.

## The rest of the corrections

Endgame/plate/key corrections aside: ethereal items **can** be repaired (the crafting recipe *Mend
the Ethereal*, not a smith); `NEVER` in the items table means quest items and starting gear only -
the gem, rune and set families are `REGULAR` and held back by the drop hook instead; the spells
table's "unused" role covers the class-tree spell slots, not just the null entry; four kinds of charm
share the cap, nineteen in all, not three; the Barbarian gains 2 life per point of Vitality, not 2.5,
which the page's own table already showed; set drop rates also scale by difficulty and x6 for an
endgame boss; Wirt's stock is rolled at character level, not at depth, so he is not on the vendor
table; the unique-monster roll is ~8% in single player and 16% in multiplayer; only *elemental*
masteries grant damage and resistance; the Mana column is before class discounts; the quest-marker
list was missing the wounded townsman and Celia; vendor level plus two is a single-player rule;
`history.html` still called 1.8 "current" and pointed at the wrong vault path; `pipeline.html` still
named Levski's Roar as its biggest blocker, which shipped in the 1.8 line.

## Verification

All 25 page scripts pass `node --check`. Data checks: 94 set pieces in 15 sets, 0 unassigned; 452
items with an icon index, 0 unresolved, 0 out of range; six representative socketables
(gem/rune/jewel/orb/charm/salvage) each resolve to a file that exists. **The rendered page was again
not verified by eye** - the browser cannot open `file://` and is not signed in for the private
artifact.

## Still open

- **`uniques.html` shows 250 of 360.** `BuildWiki.ps1:394` parses only `unique_items_data.inc`; the
  110 vanilla `UniqueItems[]` rows in `itemdat.cpp` are never read.
- **`colours.html` is missing `ColorMagicDamage`.** Regenerating with `BuildFontColourLegend.pl` does
  not help - the registry itself reports 23 colours, so the generator's detection misses it.
- **Levski's bulk salvage path** still places one unit at a time and destroys the surplus
  (`salvage.cpp:211-221`); the page understates this as "reported rather than swallowed".
- The items compendium filters on damage or armour, so the "rings and amulets take a tier, only
  value moves" claim cannot be checked on the page itself.
