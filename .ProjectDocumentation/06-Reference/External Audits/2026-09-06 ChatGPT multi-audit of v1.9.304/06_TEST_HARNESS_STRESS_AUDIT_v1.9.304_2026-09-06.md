# Test-harness order, repeatability, and state-isolation audit

Snapshot: Diablo Orcl v1.9.304, source `c659976d69906489c34c28c636b477f14f424bb6`

## Outcome

The ordinary CTest layout launches many discovered tests as separate processes, so it hides global-state leakage between tests in the same executable. Whole-binary shuffle/repeat runs reproduced failures in four binaries. These are harness defects unless stated otherwise; they should not be reported as player crashes. They matter because they make future randomized/stress CI unreliable and can conceal real order-sensitive engine bugs.

## QA-01 - `oracool_audit_test` has archive/audio/cursor lifetime leakage

Command:

```text
oracool_audit_test.exe --gtest_shuffle --gtest_random_seed=22062 --gtest_brief=1
```

Result: 240 tests, 223 passed, 17 failed.

Observed failures included:

- Nine class-tree/aura cases terminating with Windows SEH `0xc0000094` (integer divide by zero).
- Rendering/asset/passive cases terminating with `0xc0000005` (access violation), including oil tint, belt art/glyphs, hero aura chunks, passive rendering, and rune tint.

The order-sensitive seam is visible in the source:

- `OracoolColdPack.EveryDeliveredSheetLoadsAtItsSpecifiedShape` calls `LoadCoreArchives()` at `test/oracool_audit_test.cpp:8989-8994` and has no matching teardown.
- Other tests call `LoadCoreArchives` and `FreeCursor` at scattered locations (`9873-10039` and beyond) without one common RAII owner.
- Subsequent class-aura tests can now find sound files while the headless audio device/mixer state was never initialized for that lifetime, producing the divide-by-zero path.
- Later render tests can run after cursor resources were freed and not reconstructed.

Repair: introduce suite/fixture RAII for archive mounting, audio initialization, cursor loading, globals, and teardown. Tests that only inspect metadata should stub sound playback rather than depending on whichever earlier test mounted data.

## QA-02 - `inv_test` reuses the Player and multiplayer global

Command:

```text
inv_test.exe --gtest_shuffle --gtest_repeat=5 --gtest_brief=1
```

Four repetitions failed `InvTest.GoldAutoPlace_MultiplayerStillUsesInventory`; one passed. Expected piles were 5000 and 900, while contaminated runs observed 5900 and 0.

`InvTest::SetUp` at `test/inv_test.cpp:20-31` only does `Players.resize(1)` and clears extra-tab arrays. If the vector already has one Player, resize does not reconstruct it. Ordinary inventory, belt, stash, held item, network provider, options, and globals such as `gbIsMultiplayer` can survive another test.

Repair: assign a fresh `Player {}` or reconstruct the global vector for every test; reset Stash and all modified globals/options in SetUp/TearDown. Prefer scoped guards for `gbIsMultiplayer` and the network provider.

## QA-03 - `player_test` leaks animation and skill state

Command:

```text
player_test.exe --gtest_shuffle --gtest_repeat=5 --gtest_brief=1
```

The first repetition passed all 17 tests. Later repetitions failed `Player.PM_DoGotHit`: expected recovery frames 3-8, observed 1. One repetition also failed `Player.FuriousCharge_Disabled_UntilSkillsSystemExists` because enabled/dash state leaked.

`PM_DoGotHit` at `test/player_test.cpp:92-99` resizes/reuses the global Player without a fixture reset. Furious Charge tests at `278+` mutate process-global flags/state across free `TEST` bodies. Reconstruct the player and reset every class-mechanic singleton before and after each case. A reusable `GameGlobalsGuard` would be less error-prone than per-test handwritten cleanup.

## QA-04 - `writehero_test` deletes its active save directory between cases

Command:

```text
writehero_test.exe --gtest_shuffle --gtest_repeat=5 --gtest_brief=1
```

Current relocated-build result: four repetitions had 9/10 pass and failed `Writehero.ReplaceFileAtomicallyOverwritesAndReportsBack`; one repetition passed 10/10. The failure logged missing paths beneath `C:\Diablo Orcl\x64-Debug\test-saves-<pid>` and `MoveFileExW` error 3.

Root cause:

- `UseIsolatedPrefPath` at `test/isolated_pref_path.hpp:56-62` creates `test-saves-<process-id>` and stores it globally as PrefPath.
- `DropIsolatedPrefPath` at `74-83` removes that directory after a passing test.
- `IsolatedPrefPathGuard` at `92-97` invokes those two functions but does not restore the previous path.
- Every test in the same executable has the same process ID. After one guard deletes the directory, later unguarded or differently ordered operations still point at the deleted directory. A following guard can recreate it, which explains the shuffle dependence.

Repair options, best first:

1. Give every test a unique directory using process ID plus sanitized current test suite/name (and repeat index if needed), store the previous PrefPath, and restore it in the destructor.
2. Or keep one per-process directory for the entire executable and delete it only at global environment teardown.
3. At minimum recreate the currently selected directory before every write, but this leaves global-path ownership ambiguous.

Add a direct test that runs two guards sequentially, performs writes after each, and verifies the original PrefPath is restored.

## CI changes

Keep the fast discovered-per-test CTest lane, and add:

```text
each_test_binary --gtest_shuffle --gtest_repeat=10 --gtest_brief=1
```

Use a recorded seed on failure. Run at least the major global-heavy binaries (`oracool_audit_test`, `inv_test`, `player_test`, `writehero_test`, `loadsave_test`) as whole processes. Add a serial repeat lane under sanitizers, because parallel process isolation and random order test different risks.

Also make fixtures assert their preconditions (empty player containers, expected archive/audio state, default mechanic state). A contaminated test should fail at setup with a precise message instead of producing a misleading gameplay assertion later.

## Clean stress evidence

Earlier/current focused runs found stable behavior in item, missile, quest, and store tests, and all 73 targeted new-mechanics tests in the current binary passed when invoked in controlled groups. The failures above are deterministic enough to fix but do not invalidate those functional results.
