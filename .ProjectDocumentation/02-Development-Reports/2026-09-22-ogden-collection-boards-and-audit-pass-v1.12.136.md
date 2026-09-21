# Ogden's collection boards, and a twelve-point audit — v1.12.134–136

**Date:** 2026-09-22
**Version:** v1.12.136 (v1.12.134 failed a test, v1.12.135 failed to compile — both below)
**Branch:** renderer-32bit

## Part one: the boards

> Tab 1 - Gems [...] Make an invisible 30x30, 7x5 grid and fill it with all types of Gems the same
> order as you use for the stash Sort function. Gems to have bottom left a counter up to 99. White
> text on dark transparent background. If counter is 0, use red font. If a type of gems is not yet
> possessed by the user - desaturate its icon. [...] Tab 2 - Runes - Same canvas. Same rules.

**Most of this already existed.** `workshop.cpp` has had Ogden's Gems / Runes / Jewels tabs with
working Upgrade and Downgrade, the ladders and pack accounting since the artisans landed. What the
user asked for is a new *presentation* of a working feature, which is why it cost one build rather
than five.

### The board is the stash's own layout

Measured on his canvas: the frame's rules stand at x 61..63 and 276..278, its bands at y 429..438 and
591..600, so the opening is **x 64..275 by y 439..590** — 212×152. A 7×5 board of 30px cells is
210×150 and centres inside it with a pixel to spare.

Seven by five is not an arbitrary fit. `SortStash` places a gem at `{ type, GemTopRow + quality }` —
column by `GemType`, row by `GemQuality`. Seven types, five qualities, thirty-five cells, thirty-five
gems. Runes take the same board by ladder position and leave the last two cells empty; jewels sit
grade-major five to a row, the block shape the stash gives them.

### Two behaviour changes

**The downgrade was a pump.** It returned TWO of the lower rung and the button said so — "DOWNGRADE
1 → 2" — while ascending costs two runes. Two down then one up was a net gain on the rune ladder. At
the user's word it is 1:1: *"you dont get two of the lower. sorry. price of conversion."*

**The old message panel cannot coexist with the board.** It occupies y 336..612 and the board sits at
439..590 *inside* it. On stock tabs it is not drawn at all; the message moves under the grid with the
rune confirmation, where the user asked for it.

### The confirmation stays left of the orb

Everything under the grid stops before **x 175**, where `GetHealthOrbRect` begins on a 960-wide
screen. Griswold's Refresh-until plate ends at 154 and the stash's gold at 165 for the same reason:
below `OrbClearanceBottom` the window shares the screen with a sphere, and a button drawn under it
cannot be pressed. A `static_assert` holds the line.

## Part two: the audit

> When done - run a dozen of audits and repair

| # | Audit | Result |
|---|---|---|
| 1 | Dead code after the redesign | `DrawStockList` never called — removed |
| 2 | Orphans | `StockFor`, `StockRow`, `StockScroll` — removed |
| 3 | `SelectedRow` on stock tabs | resolved by 1 |
| 4 | Hover text | **missing entirely — added** |
| 5 | Sound table after appending `IS_SHATTER` | no size guard or enum walk depends on it |
| 6 | Reset on new game | correct |
| 7 | Reset on close | **bug — repaired** |
| 8 | Bench click rect | **bug — repaired** |
| 9 | Title over a portrait | **inconsistent — repaired** |
| 10 | Board order vs `SortStash` | verified for all three ladders |
| 11 | Board bounds and id ranges | clean |
| 12 | The failing test | **root-caused — repaired** |

### Finding 7: `CloseWorkshop` kept the question

`ResetWorkshopForNewGame` and `CloseWorkshop` have near-identical bodies. A `s///` without `/g`
patched the first and left the second, so a rune question walked away from was still standing — and
already half-answered — when the window reopened. **A non-global substitution against a body that
has a twin is a silent half-fix.**

### Finding 8: the invisible bench

`DrawBench` is skipped on stock tabs, but its click rect stayed live — and every tab Ogden has is a
stock tab. A held item dropped in that corner went onto a bench nothing had drawn, returning only
when the window closed. Griswold's Salvage page learned the identical lesson on 2026-09-21: **a
control that is not drawn must not be hit-tested.**

### Finding 4: a glyph says nothing

The wide buttons the arrows replaced carried "UPGRADE 3 → 1" on their faces. A 34px plate carries no
words, so the price of a step — and, on the runes, the only warning before a Zod is spent — had left
the screen entirely. Hover text added for both arrows and every board cell.

### Finding 12: `Item::clear()` leaves everything but `_itype`

`OracoolAudit.InnateMaskFollowsTheShield` passed standalone (#259) and failed inside
`oracool_audit_test_shuffled` (#814) — order-dependent.

```cpp
DVL_REINITIALIZES void clear() { this->_itype = ItemType::None; }
```

That is the whole function. A cleared slot keeps every other field, `_iOracoolBroken` included. The
test builds its shield by setting `_itype` **alone** on a reused `Players[0]`, and
`HasShieldEquipped` rejects a broken shield — so the result depended on whether an earlier test in
the shuffle had left a broken item in that hand.

**Production is unaffected**: a real slot is filled by whole-struct assignment, which overwrites the
stale flag. Only a hand-built item on a reused Player can inherit it. The other three `.clear()`
sites in the file are sound — they tear down at the end, or refill by assignment.

Fixed with fresh `devilution::Item {}` values, then **verified by ten shuffled runs, all clean**.
Ten passes is evidence, not proof, for an order-dependent failure; what can be said is that the
mechanism is understood and closed.

## Two failed builds, both mine, both the same shape

- **v1.12.135**: `Item {}` in a translation unit where `Item` is ambiguous. The tests beside it
  already write `devilution::Item`.
- **v1.12.129** earlier: `utils/string_view.hpp` where the path is `utils/stdcompat/string_view.hpp`.

Both were names written from memory instead of copied from the code beside them. Neither is visible
when reading the diff; each costs a full build.

## Files

- `Source/oracool/workshop.cpp` — the board, the arrows, the confirmation, the 1:1 downgrade, the
  sounds, and repairs 1, 2, 4, 7, 8, 9.
- `test/oracool_audit_test.cpp` — repair 12.

No asset changes since v1.12.133's repack (608 files), so no MPQ repack.

## Still open

- The **Jewels tab** exists and is built from the same code; the user's list named only Gems and
  Runes. Kept, since Temper Jewels is one of the four abilities they asked to be told about.
- Ogden's window is still **two windows**: his workshop (Gems / Runes / Jewels / Recipes) hands off
  to the Levski page (Cube / Recipes) through its Recipes tab.
