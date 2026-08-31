---
date: 2026-08-19
version: 1.8.11
area: Sockets v2 - point 7, the runeword pool
---

# Three Hundred and Seventy Runewords

The last unblocked point of the nine-point socket directive: "a table of a few hundred runewords
covering all ten non jewelry item types."

## The table is two halves

**61 authored Diablo II words, with D2's own recipes.** Steel is Tir El. Enigma is Jah Ith Ber.
Ancient's Pledge is Ral Ort Tal. This half exists because the first cut got it wrong in an
instructive way: the generator queued D2's NAMES onto generated sequences, so Steel came out as
El Ral. That is worse than inventing names - a player who knows D2 reads "Steel" and expects Tir
El, and gets something else. Three tests failed on it, and they were right to.

Five of D2's words are dropped rather than altered - Sanctuary, Last Wish, Infinity, Phoenix and
Bone all repeat a rune. Legal in D2, but a repeated rune is the exact signature of the stride bug
in the generated half (below), and one rule for the whole table is worth more than five homages.

**309 generated words** fill out every slot with invented names, skipping any sequence an authored
word already claimed. Both halves go through one row builder, so the hand-written part and the
machine-written part cannot end up shaped differently - which is the usual failure of a table with
both.

## The rules the scheme encodes

- **Word length equals the host's socket count exactly** (D2's rule). With Sockets v2's footprint
  caps that means a six-rune word can only form in a 2x3 host, so the long words are the rare ones
  by construction rather than by a rarity roll.
- **The host is the SLOT**, not gems.h's three-way category. `RunewordHost` has ten values; a belt
  word and a helm word are different things even though both are "armour" to a gem. Jewelry is
  absent deliberately - one socket means a one-rune "word", which is just a socketed rune.
- **The grant scales with the runes' total ladder depth**, then takes a shape per slot: weapons get
  damage and to-hit, shields and body armour get armour and resists, the accessory slots get life,
  mana and resists.

`RunewordDefinition::runes` widened from 3 to 6 to match the new socket cap.

## The bug the scheme had

Every six-rune word was **X Y Z X Y Z**. The rune stride was `5 + length`, which at length 6 is 11
- and 11 divides 33, so the walk had order 3 and looped back onto the same three ladder positions.
Strides are now co-prime with 33 (7, 8, 10, 13, 14). Found by reading the generated output before
writing the test, which is the cheaper order.

## The teaching lines needed a cap

Every rune's description lists the words it belongs to - the one thing this fork improved on D2,
where the recipes lived on a wiki. With three words that was a line; with 370, a popular rune
belongs to dozens and the panel would run off the screen. It now shows six and counts the rest:
"...and 27 more runewords".

## Tests

`ThePoolIsWellFormed` is aimed at how a GENERATED table fails - not typos, but entries that are
unreachable or identical. It pins: no duplicate names; no duplicate sequence within a host (the
second would be unreachable forever); every listed rune is a rune; no rune repeats inside a word;
the padding past runeCount is zeroed so a longer word cannot shadow a shorter one's prefix; and
every word grants something.

`EveryNonJewellerySlotHasWords` pins that all ten slots have words and that rings, amulets and misc
resolve to no host at all.

Two older tests had to be corrected rather than satisfied, both pinning the fork's invented
recipes: Ancient's Pledge was Ral Ort **El** here and is Ral Ort **Tal** in D2. The teaching
assertions also changed shape - asserting a specific word appears in a capped list is brittle by
construction, so the test now checks the list is non-empty, that Steel leads El's, and that the
"and N more" line appears.

## Verified

**452 tests, the usual two** (`Drlg_l1.CreateL5Dungeon_diablo_3_844660068`,
`Timedemo.WarriorLevel1to2`). The wiki's runeword section is a searchable, filterable table of all
370, parsed from the generated include - checked in the browser: 370 rows, and the four tables a
too-broad sed had silently emptied are back.

## What is left of the directive

Points 5 and 8 - extraction, and moving crafting into Levski's Roar - remain blocked on Levski's
Roar itself, which does not exist and whose recipe table was to be approved first. Point 10 of the
user's list is still blank.
