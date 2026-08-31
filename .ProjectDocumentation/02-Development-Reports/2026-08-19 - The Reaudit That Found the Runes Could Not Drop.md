---
date: 2026-08-19
version: 1.8.10
area: Reaudit of 1.8.8-1.8.9, and the wiki brought back into agreement
---

# The Reaudit That Found the Runes Could Not Drop

A reread of the last two builds against the wiki, per the user's instruction. Two real bugs, both
from the same cause, and both invisible to the compiler: **the enum order is not the rune order**.

## Bug 1 - twenty-eight of the thirty-three runes could never drop

`TrySpawnOracoolGem` collected rune candidates by walking the index range
`IDI_ORACOOL_RUNE_EL .. IDI_ORACOOL_RUNE_SOL`. That span is the five runes from v1.7.8; the other
28 are appended after the entire gem ladder, so the drop pool silently stayed at five while the
item table, the icons, the effects and the wiki all said 33.

Its candidate array was also `_item_indexes candidates[12]`, sized when twelve was more than the
whole family. Walking 33 into it would have overflowed the stack buffer.

Both fixed: the walk goes through the ladder (`RuneLadderSize` / `RuneAtLadderPosition`) and the
array is sized off `MaxRuneLadder`.

## Bug 2 - Ascend Runes produced the wrong rune, and could produce a charm

The recipe's output was `IDidx + 1`. In enum order that gives **Eld -> Nef**, skipping Tir; and
**Sol + 1 is Charm of Luck**, because the charms sit between the old runes and the gem ladder.

There is now a generated `RuneOrder[33]` in Diablo II's sequence, with `NextRune()` and
`IsTopRune()` over it. Anything meaning "the next rune up" goes through the ladder - the enum order
cannot be made into the rune order, because item indices are positional save format and the five
shipped runes cannot move. The recipe's exclusion moved from Sol to **Zod**, which is the real top.

## Two tests were pinning the old, wrong behaviour

Worth recording, because a green suite would otherwise have looked like proof:

- `AscendRunesConsumesPairAndProducesNextRung` expected two El to make a **Tir**. D2's ladder puts
  **Eld** above El - the old expectation was the `index + 1` bug written down as a requirement, and
  it only looked right while Eld did not exist.
- `MixedGemsDoNotSatisfyThreeOfAKind` asserted a Sol pair cannot ascend. Sol was only the top while
  five of thirty-three existed. It now pins **Zod** as the top, and additionally that a Sol pair
  DOES ascend - the positive case the old test could not have.

## The wiki disagreed in four places

- **Base item count was wrong in both directions.** Moving the five shipped rune rows into a
  generated include took them out of the wiki's parser, and the 28 new ones were never in it: the
  count went 358 -> 353 when it should have gone to **386**. The generator now concatenates both
  rune includes before parsing.
- The runes page said "The five runes" and listed five. It now carries all 33 in ladder order with
  their drop depth and value, renders the new channels (the four attributes, magic and gold find,
  and the special-effect flags by name rather than as raw flags), and calls out Hel and Zod.
- The crafting table said "Sol is the top of the shipped ladder". It is Zod.
- The overview card said "five runes and three runewords".

## Verified

**450 tests, the usual two** (`Drlg_l1.CreateL5Dungeon_diablo_3_844660068`,
`Timedemo.WarriorLevel1to2`).
