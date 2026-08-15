---
date: 2026-08-15
version: 1.6.13
area: Tests / audit regression suite
---

# Thirteen Tests That Name Their Bugs

User request: *"write the regression tests."* The audit's fixes are now pinned - a new
`test/oracool_audit_test.cpp` with thirteen tests, each encoding a bug that actually shipped. The
test names say what BROKE, so a future red run reads as "that bug is back", not as an abstract
invariant failing.

## What is pinned

| Test | The bug it guards against |
|---|---|
| `LesserUniqueNameIgnoresAiSeed` | v1.6.4: "his name was constantly changing" - the name derived from per-tick state |
| `LesserUniqueNameIsDeterministicAndSeedDependent` | same fix, the other direction |
| `DisplayNameIsAffixPlusGeneratedName` | v1.6.5: kill log and death screen naming the borrowed champion |
| `TintLeavesScriptedUniquesAlone` | v1.6.5: the regression that repainted Gharbad two shades darker on every load |
| `TintStaysInsideTheRamp` | the tint's whole contract, layout-independent: global half only, own ramp, ≤2 shades, and at least one seed actually tints |
| `NameSeedRollStaysInBiasCorrectedBand` | the 0x7FFF bias-band constraint the seed range was designed around |
| `SilentQuestBossesAreNotLesserUniqueCandidates` | v1.6.5: Skeleton King, Butcher, Hork Demon and Na-Krul spawnable as lesser uniques - plus the talker half and one ordinary champion that must STAY borrowable |
| `ShieldCountsInEitherHand` | v1.6.3: a shield in the left hand was not a shield |
| `InnateMaskFollowsTheShield` | v1.6.3: the stale _pAblSpells mask, plus a class-leak check |
| `ZealStrikeLadder` | the user's specified ladder: 0 below 6, 2 at 6, +1 per 2 levels, cap 5 |
| `TotalPlayerGoldSaturatesInsteadOfWrapping` | v1.6.11: the rich-player int overflow that approved every purchase - including the negative-pool state the wraparound produced |
| `GradualHealDoesNotOutliveItsHero` | v1.6.8: the potion that healed the next character - with a control asserting the drip still works at all |
| `PremiumBuyStaleRowChargesNothing` | v1.6.11: the sparse-scan overrun - a stale row must charge nothing, consume nothing, clear nothing |

## Two small seams, following the file's own precedent

`stores.cpp` already had `SimulateStorytellerIdentifyForTest` and
`SimulateSmithConsumablesPurchaseForTest` for exactly this problem - the store internals live in a
2,700-line anonymous namespace. Two additions in the same style:

- `SimulateSmithPremiumBuyForTest(selectedIndex, item)` - sets the held selection and runs the
  premium-buy completion as ConfirmEnter would.
- `IsQuestUniqueForTest(mName)` - classification by name, looked up inside lesser_uniques.cpp so the
  test does not need `UniqueMonstersData` DLL-exported (data symbols, unlike functions, are not
  covered by WINDOWS_EXPORT_ALL_SYMBOLS; `numpremium`/`premiumitems` got the existing
  `DVL_API_FOR_TEST` treatment instead, matching `storenumh` beside them).

## What is deliberately NOT here

The input-layer fixes (HUD chrome routing, shift-cast, interaction-wins) and the Zeal burst timing
live in the click dispatch and the game tick - pinning them means simulating input, which this suite
does not do. The stash/tab torn-file guards need a crafted archive. These remain play-test items:
the shift-cast paths on both buttons, Zeal's rhythm mid-fight, and a save/load over a lesser unique.

## The baseline moves

**367/369.** The suite grew from 356 to 369; the two failures are the same two that predate all of
this work (`Drlg_l1.CreateL5Dungeon_diablo_3_844660068`, `Timedemo.WarriorLevel1to2` - the latter now
test #361 in the renumbered run). Every future session inherits the audit's findings as executable
checks rather than as prose.
