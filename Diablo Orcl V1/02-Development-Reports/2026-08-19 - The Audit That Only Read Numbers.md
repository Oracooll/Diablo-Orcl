---
date: 2026-08-19
version: 1.8.13
area: Wiki - the misinformation the previous audit walked past
---

# The Audit That Only Read Numbers

The user screenshotted two boxes from the sockets page and asked how they were still there. Both
were on a page audited one build earlier, which is the finding worth recording: **the 1.8.12 audit
checked numbers against source and swept for stale numerals, and neither method can see a sentence
that is wrong in words.**

## What the previous audit could not have caught

**The sockets page contradicted itself in four lines.** Its lede said "Sockets only ever appear on
basic, TIERLESS equipment" while the bullet list below it said every base tier can host them. The
tier exclusion was removed in 1.8.8. The numeral sweep looked for "one to three" and "tierless" -
and did find the tierless in the mechanics page - but the lede's copy was never read because it
carries no number.

**"The three hosts" omitted jewelry.** Since 1.8.8 rings and amulets take sockets, and
`SocketHostForItemType`'s `default:` puts them in the **Armour** column. The table listed body,
helm and the six accessories and stopped. A player socketing a ring had no way to learn from the
wiki that a Ruby there gives life rather than fire damage. That is an omission, not a stale number,
so nothing in the previous method pointed at it.

**"Insertion is permanent"** was true but doubled down on a rule under active reversal, and left
out the useful half: insertion is **backpack-only**. `inv.cpp` returns on the location mismatch
before ever reaching the socket path, so an equipped item cannot be socketed where it sits.

## Three more found by reading rather than grepping

- **Magic Find "only upgrades basic, tierless drops"** conflated the two axes the tiers page calls
  independent. `ApplyMagicAndGoldFindToDrop` checks `hasOracoolTier()` - the QUALITY tier - so a
  Torment-base white sword is upgradeable exactly like a Normal one.
- **Salvage was described three times as though it exists** ("Feedstock for salvage", "worth
  salvaging", "the feedstock for salvage"). It is not built. A reader would have gone looking for a
  mechanic that is not in the game.
- **The crafting window's row said "Sockets, gems and runes."** It has three recipes - refine gems,
  ascend runes, rework charms - and does not touch sockets at all.

## The method that failed, and its replacement

1.8.12's audit was: verify constants against source, then grep for stale numerals. It found four
things and declared the wiki correct. Every miss above is prose that is *grammatically fine and
factually superseded* - the class of error a numeral grep is structurally blind to.

The replacement, used here: read each page's PROSE against the source, asking three questions of
every sentence - does it state a rule that a later build changed, does it describe a system that
does not exist yet, and does it omit a case the code handles. All six findings answer one of those.

## Verified

Rebuilt and rechecked in the browser: the lede now says basic-quality at any base tier, the host
table names rings and amulets, and the permanence note says backpack-only and flags the coming
change. Tables intact - 7 gems, 33 runes, 370 runewords. No engine code touched, so the test state
stands at 1.8.11's **452, the usual two**.
