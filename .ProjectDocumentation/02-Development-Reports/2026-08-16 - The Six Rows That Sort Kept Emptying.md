---
date: 2026-08-16
version: 1.7.38
tags: [stash, bugfix, regression-test]
---

# The Six Rows That Sort Kept Emptying

> "sort function of stash isnt utilizing last 6 rows."

The report named Sort. Sort was innocent — it was just the only witness.

## What was actually wrong

`AutoPlaceItemInStash` searches for a free spot by walking a rectangle of candidate
top-left corners, shrunk by the item's own footprint so nothing can hang off the edge:

```cpp
for (auto stashPosition : PointsInRectangle(Rectangle { { 0, 0 },
        Size { 10 - (itemSize.width - 1), 10 - (itemSize.height - 1) } })) {
```

Both bounds were the literal `10`, from the vanilla 10×10 stash page.

The page is not 10×10 any more. It became 10×**16** when the stash moved into the shared
340×720 theme — `StashGridColumns` / `StashGridRows` in `qol/stash.h`, which the grid array,
the on-screen layout and the save-file sizing all derive from. This scan was the one place
that kept its own copy of the number, and only the width half of it happened to still be right.

So rows 10–15 — the last six — were unreachable by **every** automatic placement:

- Gillian's "deposit to stash"
- shift-click an item into the stash
- the re-pack `SortStash` performs

Once the scan ran out of candidates it moved to the next page, so a stash that looked full
at 100 items would silently start page 2 with six empty rows still sitting on page 1.

## Why Sort took the blame

Manual drag-and-drop uses a different path (`CheckStashPaste`), which was already clamped
against `StashGridSize` and could reach the bottom rows fine. So items *could* live down
there — the user had put them there by hand.

Sort is the only operation that clears the page and re-places everything through the broken
scan. It didn't just fail to fill the last six rows; it actively **emptied** them and pushed
the overflow to page 2. That's what made it look like a Sort bug.

## The fix

One line, derived from the constants instead of restating them:

```cpp
const Size scanArea { StashGridSize.width  - (itemSize.width  - 1),
                      StashGridSize.height - (itemSize.height - 1) };
```

`StashGridSize` was already sitting in the same file, built from the same `StashGridColumns` /
`StashGridRows` pair as everything else. Next time the page is resized, this follows.

## Pinned

Two tests in `oracool_audit_test.cpp`, both verified to go red against the old literal before
being accepted:

**`StashAutoPlaceReachesEveryRowOfThePage`** — places exactly one page's worth of one-cell
items (160), asserts all 160 stay on page 0 and that every single cell is occupied. Then runs
`SortStash` over them and asserts the bottom row survives the re-pack.

**`StashAutoPlaceSeatsTallItemsAgainstTheBottomEdge`** — blocks every row above the last band
a 2×3 item could occupy, then asserts the item seats with its bottom row on row 15.

The second test is the one worth keeping honest about. Under the bug it did **not** fail to
place the item — it exhausted the truncated scan, wrapped to page 1, and landed at that page's
top-left, reporting a perfectly successful placement. Checking only "did it place?" would have
passed. The test therefore pins the page count as well as the row, because on this code path a
wrap is indistinguishable from a success unless you look for it.

That failure mode is also the real-world shape of the bug: nothing errors, nothing is lost,
the stash just quietly spreads across more pages than it needs.

## Shipped

`ORACOOL_VERSION` 1.7.38. 421 tests, the two long-standing failures unchanged
(`Drlg_l1.CreateL5Dungeon_diablo_3_844660068`, `Timedemo.WarriorLevel1to2`).

## Worth noting

The `10` survived the resize because it was a bare literal in a loop bound, in a file that
otherwise derives everything from `StashGridColumns` / `StashGridRows`. The resize touched
`stash.h`, the drawing code, and `loadsave.cpp` — all the places that *mention* the grid size
by name. A literal mentions nothing, so nothing led back to it.
