# The ladder is 64 rungs, and Hell/Hell is the threshold

**Version:** 1.11.060
**Date:** 2026-09-12
**Branch:** renderer-32bit

## What was asked

> "let's rework area levels - hive = caves, crypt = hell. adjust area lvl accordingly throughout the
> four dificulties."
> "now we should have a total of 4x16=64 area levels."
> "hell/crypt//torment being the hardest"
> "hell/hell should be the threshhold for reaching god tier items. everything should be droppable by
> then"

## The ladder

The Nest (floors 17-20) and the Crypt (21-24) used to extend the ladder to rungs 17-24, which made
them the deepest places in the game. They were never designed to be: they are Hellfire's parallel
path, their monsters are statted for a side-step, and that was settled on 2026-09-12 when the
monster stats for those floors were deliberately left alone ("just an alternative area for the sake
of diversity").

So they now *share* rungs instead of adding them. `LadderFloorOf` side-steps floors 17-24 back by
two areas:

```cpp
constexpr int FirstSideStepFloor = 17;
int LadderFloorOf(int floor)
{
	return floor >= FirstSideStepFloor ? floor - 2 * FloorsPerArea : floor;
}
```

| Area | Floors | Rung |
|---|---|---|
| Cathedral | 1-4 | 1-4 |
| Catacombs | 5-8 | 5-8 |
| Caves | 9-12 | 9-12 |
| Hell | 13-16 | 13-16 |
| Nest | 17-20 | 9-12 |
| Crypt | 21-24 | 13-16 |

Sixteen rungs a difficulty, four difficulties: `RungsPerDifficulty = 16`, `MaxAreaLevel = 64`.
Torment/Crypt is the hardest place in the game at alvl 64.

## Everything droppable by Hell/Hell

Hell/Hell is alvl 48. Four content families were aimed at the old 96-rung ladder and had to be
re-aimed, or their deep end would have sat in Torment - past the threshold:

| Family | Was | Now |
|---|---|---|
| Base items (`BandedQlvl`) | authored 1-51 onto 1-60 | onto 1-48, fallback 48 |
| Base tiers (`RungsPerBaseTier`) | one per 24 rungs | one per 12 rungs, so bands 1-12 / 13-24 / 25-36 / 37-64 |
| Spell books (`SpellBookItemLevel`) | 1, 6, 18, 30, 42, 52 | 1, 5, 13, 22, 33, 45 |
| Runes (`GenRunes.ps1` + both `.inc`) | El 3 ... Zod 94 | El 3 ... Zod 48 |

The rune rescale is the one worth calling out. Every rune qlvl was mapped by
`new = round((old - 3) * 45 / 91) + 3` - 28 generated rows and 5 shipped rows. On the old ladder the
top half of the rune ladder collapsed onto a single depth through `BandedQlvl`'s fallback, so the
high runes were not merely late, they were indistinguishable from one another.

`VendorItemLevel` and the `endgame_boss.cpp` threshold comment follow the new
`RungsPerDifficulty` rather than a literal.

## Tests

Two new audit tests, both passing:

- `TheAreaLadderIsSixteenRungsADifficultyAndEndsAtHellHell` - the rung value of every area on every
  difficulty, Nest on 9-12 and Crypt on 13-16, the names unchanged, `BandedQlvl` topping out at 48,
  and the tier boundaries at 1 / 13 / 25 / 37 / 48.
- `EverythingIsDroppableByHellHell` - `AreaLevel(16, DIFF_HELL) == 48`, the deepest spell book at or
  below it, the rune ladder non-decreasing and ending at or below it (and above 24, so the rescale
  did not flatten it), and every base's banded qlvl at or below it.

Full suite: **718/718**. Debug and Release both build clean; the RTM exe is updated.

## The wiki

`BuildWiki.ps1` now derives `maxAreaLevel` from `FloorsPerArea * 16` instead of a hardcoded 96, and
`data.js` regenerates at 64. Eight pages carried the old ladder in hand-written prose or in their own
JS mirrors of the game's tables, all corrected: `areas.html` (lede, formula, a new floor-to-rung
table, the threshold note, the rewritten "two things it fixed"), `mechanics.html`, `tiers.html`,
`affixes.html`, `index.html`, `sockets.html`, `items.html` (its `bandedQlvl` mirror), and `wiki.js`
itself, whose `tierOfLevel` divided by 24 and `areaOfLevel` took `% 24` over six areas - both wrong
for every page that called them.

## Notes for next time

Two process traps hit during this work, both worth remembering:

1. **`grep -c $'\r$'` lies in this environment.** It reported "all lines CRLF, zero bare LF" for
   every file, including files that are pure LF. The line-ending audit it was used for proved
   nothing. Byte-count instead - `[IO.File]::ReadAllBytes` counting 0x0D/0x0A - which showed this
   repo is legitimately mixed: the wiki, `area_level.cpp`, `item_tiers.cpp`, `spell_ranks.cpp` and
   `BuildWiki.ps1` are LF, while `runes_data.inc` and `oracool_audit_test.cpp` are CRLF.
   `.gitattributes` sets `* -text`, so git stores bytes verbatim and will not normalise a mistake
   away.
2. **Perl heredoc anchors always end in a newline.** An anchor whose last line is a partial line
   (`document.querySelector('#scales tbody')`) can never match. And an `edit()` helper must build on
   the previous edit's text, not re-read the file, or only the last edit to a file survives.
