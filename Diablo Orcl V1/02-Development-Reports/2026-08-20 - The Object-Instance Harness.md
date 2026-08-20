---
date: 2026-08-20
version: 1.8.65
tags: [testing, objects, town, tooling]
---

# The Object-Instance Harness

Fourteen tests that build town's object pool and assert against what comes out of it. The suite
goes from 470 to 484.

## What it is for

Nothing in the suite asserted anything about a placed object **after** `AddObject`. Placement,
selectability and operate routing were all invisible, and the same class of bug has now shipped
four times in a week:

| | |
|---|---|
| 1.8.17 | The monument drew perfectly and ignored every click - `OBJ_STAND`'s row carries `selFlag 0` |
| 1.8.19 | It had a cursor and an outline but no hover name and no working operate |
| 1.8.62 | `orclstash.cel` was rebuilt, installed and packed, and nothing loaded it |
| 1.8.64 | The monument was walked *onto* rather than approached - `_oSolidFlag` had been cleared |

Every one passed a clean build and a clean suite. The harness would have caught three of the four.

## The one it would not have caught, stated up front

**Sprites.** `HeadlessMode` short-circuits the entire graphics path: `SetupObject` skips its
`ObjFileList` lookup, `EnsureObjectGraphicsLoaded` returns immediately, and so do
`ApplyStashChestGraphics` and `ApplyLevskiRoarGraphics`. `_oAnimData` is always empty in a test, so
any assertion on it would pass for the wrong reason — which is worse than no assertion.

That is exactly the gap 1.8.62 fell through, and it is written at the top of the test file rather
than quietly left out. Whether an object wears the right art remains a question only the screen
answers.

## Two design decisions worth recording

**Positions are pins, not lookups.** The tile coordinates are literals rather than reads of the
constants the code uses. Reading the constant would make the test agree with any move, silently. A
literal fails when furniture moves — which forces a conscious update, and the monument has already
moved once (1.8.18). That is precisely the moment a harness should speak up.

**The routing test asserts the predicate, not the flag.** `msg.cpp:1532` passes
`!_oSolidFlag && !_oDoorFlag` as `MakePlrPath`'s `endsAtTarget`. The test spells that expression out
and pins its *value* for our furniture, because the contract is "the path stops beside the
monument", not "a particular bool is true".

## Proving it fails

Both historical bugs were **reintroduced deliberately** — the `_oSelFlag = 3` override deleted and
`_oSolidFlag = false` put back — and the harness went red on three tests, then green again when
`objects.cpp` was restored (`git diff --stat` clean). A test that has never failed for the right
reason is not yet a test.

## What the harness found about itself

Every placement test passed in isolation and two failed in sequence — the shape of a bug that hides
in shared state.

**`ClrAllObjects` does not clear `dObject`.** It resets `Objects`, `ActiveObjectCount`,
`ActiveObjects` and `AvailableObjects` and leaves the tile map alone. In a running game that is
correct: level generation zeroes the dungeon arrays before objects are placed. A harness has no
level generation, so the tile map kept every index from the previous test —
`AddLevskiRoarObject` then saw its requested tile as occupied, walked its fallback list, and after a
few rounds found every candidate occupied and placed nothing at all. The tests were asserting
against whatever stale object the map still pointed at.

Not a defect in the game; a real constraint on anything that drives object placement outside level
generation, so the fixture clears `dObject` first and says why.

## Also in this commit

A **`static_assert`** in `objdat.cpp` that `ObjMasterLoadList` and `object_graphic_id` are the same
length — 68 each today. `object_graphic_id` indexes that list, so an enumerator without a row reads
off the end and a row without an enumerator is unreachable. Neither shows up as a build or test
failure. This was noted in yesterday's audit as verified-by-hand; now it is verified by the
compiler, at the definition, where the two sit next to each other.

Three globals gained `DVL_API_FOR_TEST`: `setlevel`, `ActiveObjects`, `ActiveObjectCount`.
`WINDOWS_EXPORT_ALL_SYMBOLS` covers functions but not data, which is what that macro exists for —
`Objects[]` already carried it.

## Verification

484 tests, the two standing baseline failures only
(`Drlg_l1.CreateL5Dungeon_diablo_3_844660068`, `Timedemo.WarriorLevel1to2`).

Wiki rebuilt so `data.js` matches `ORACOOL_VERSION` — the check added in the previous unit would
otherwise have flagged it, which is the first time it has been used in anger.
