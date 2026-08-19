# Seventy-Five Stops Being the End of the Road

**Version:** 1.8.35
**Date:** 2026-08-19
**Tests:** 464 total, 462 passing. The two standing baseline failures only
(`Drlg_l1.CreateL5Dungeon_diablo_3_844660068`, `Timedemo.WarriorLevel1to2`). Five new tests.

## What was there

Vanilla's entire player-resistance system is three lines at the end of `CalcPlrItemVals`:

```cpp
player._pMagResist = clamp(mr, 0, MaxResistance);   // MaxResistance == 75
```

A hard cap, and no reference to the difficulty. Every point past 75 is thrown away, and Hell asks no
more of a character than Normal does. A Barbarian gains her level in all three schools, so she
arrives at the cap without wearing a single resistance item - after which resistance gear is worth
exactly nothing to her forever.

Unlike the last four Pipeline entries checked before starting, this one was accurately sized.

## The curve

Three steps, and the ORDER is the design:

1. **Penetration.** The difficulty's penalty comes off the raw total, before any cap. This is what
   makes gear matter: on Hell you need +60 to stand where a Normal character stands for free.
   Applying it after the cap would make it a flat tax no amount of gear could answer - the opposite
   of the intent.
2. **Floor at zero.**
3. **The soft cap.** Up to 75, a point is a point. Past it, three raw points buy one, to a hard
   ceiling of 90.

Penalties are **Normal 0, Nightmare -30, Hell -60, Torment -90**. Evenly spaced rather than copied
from D2's 0/-40/-100, for two reasons: this fork has four difficulties where D2 has three, and a
regular ladder is easier to reason about when the telemetry CSV is eventually read back and these get
tuned against real play. Treat all four as first drafts.

The two rules compose. Reaching the ceiling costs **120 raw points on Normal and 210 on Torment**.
Reaching the soft cap on Hell costs 135.

## The one place I departed from D2

D2 lets resistance go **negative**, so an unprepared character on Hell takes amplified elemental
damage. That works mechanically here with no further change - `resper` is applied as
`damage - damage * resper / 100`, so a negative value amplifies by itself.

It is not done. It is a far larger balance swing than the one being asked for: every character who
has not built for resistance becomes glass on Nightmare, including on the way there. The stated goal
was "resistance gear matters at endgame", and the penetration alone delivers that. Removing the floor
is one line in `ApplyResistanceCurve` if it is ever wanted, and that is written down beside it.

## One call site, three readers

The curve lives behind a call in `CalcPlrItemVals` and nowhere else. That is deliberate:
`missiles.cpp` (two paths) and `objects.cpp` (fire traps) read `_pFireResist` and friends directly
and must keep seeing a plain percentage. Putting the curve at the point of use would mean three
implementations of it, and the fire trap would have been the one that got forgotten.

## The save format is untouched, and that is not luck

The three fields are `int8_t` and hold only the FINAL value, which the curve bounds to [0, 90]. The
raw total has always been a local `int` inside `CalcPlrItemVals`. Old characters load and are
recomputed, exactly as they are on every equipment change.

## Two pinned tests moved, and both were right to fail

`Writehero.pfile_write_hero` and `OracoolStatSheet.CalcPlrItemValsAggregationPinned` both failed on
the first run, asserting 75. Neither is a bug: both characters had gear well past the old hard cap,
and the soft cap now lets the excess through. `_pMagResist` 75 -> 89, `_pLghtResist` 75 -> 90.

Both are on Normal, whose penalty is zero, so the whole delta is the soft cap and nothing else. The
assertions are updated with that stated in place, and the flip is named here and in the commit rather
than quietly absorbed - the same treatment the Torment immunity flip got this morning.

The useful half of the writehero assertion is the one that did NOT move: fire resist stays exactly 16.
A total below the soft cap must still be precisely what it always was, and that is now pinned.

## Five new tests, pinning shape rather than constants

The numbers are expected to be tuned, so a test that restated them would just have to be edited
alongside them. These pin properties:

- **Bounded.** Exhaustive over raw -200..400 on all four difficulties: never below 0, never above 90.
- **Monotonic.** Over the same range: putting on a resistance item can never LOWER the character
  sheet. This is the one a rewrite with rounding could plausibly break.
- **Diminishing.** Thirty points below the soft cap buy strictly more than thirty above it, and
  strictly more than zero - so the test fails both if the soft cap disappears and if it silently
  becomes a hard cap again.
- **Strictly harder.** Each difficulty penetrates more than the one before, and Normal costs nothing.
- **Reachable.** The ceiling can be reached on every difficulty. A ceiling nobody can touch is not a
  soft cap, it is a lie told on the character sheet.

## The character sheet and the wiki

Gold on the sheet means "there is nothing left to buy", so it now marks 90 rather than 75. Left at
75, the sheet would tell a player to stop shopping with 15 points still on the table.

The mechanics page gains a Resistance section, and every number in it - both caps, the divisor, all
four penalties, and the two worked examples - is **derived** through `BuildWiki.ps1` from
`player_resistance.h` rather than typed. That is the standing complaint about this wiki, so a new page
section was not going to be the place to add eight more hand-typed values.

## Files

- `Source/oracool/player_resistance.h` / `.cpp` - new; the curve and its reasoning.
- `Source/items.cpp` - the three clamps become three calls.
- `Source/panels/charpanel.cpp` - gold at the hard cap.
- `test/oracool_audit_test.cpp` - five new tests, one pinned assertion moved.
- `test/writehero_test.cpp` - two pinned values moved.
- `tools/BuildWiki.ps1`, `wiki/mechanics.html` - the derived Resistance section.
- `Diablo Orcl V1/07-Backlog/Pipeline.md` - entry moved to Shipped. 37 rows.

## To feel for

A character on Normal notices nothing unless they were already past 75, where the sheet will now read
higher. The change is meant to be felt on Hell and Torment: resistances that used to sit at a
comfortable 75 will read 15 and 0 until real gear goes on. If that reads as too punishing in play,
the four penalties are a single line in `player_resistance.h` and the wiki follows them automatically.
