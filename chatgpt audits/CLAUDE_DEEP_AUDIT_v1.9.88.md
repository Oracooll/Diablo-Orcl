# Diablo Orcl v1.9.88 — Deep Debug-Build Audit

Date: 2026-08-27  
Audited workspace: `C:\Users\hroga\OneDrive\2. Personal Files\Software\Diablo\Diablo Orcl V1`  
Audited build: `build\x64-Debug`  
Final audited commit: `f256bc55f86b83ee1c6217e104390df5ba277c2a`  
Commit subject: `The complete 273 skill icons (v1.9.88)`  
Generated build version: `ORACOOL_VERSION "1.9.88"`

## Purpose of this document

This is a handoff report for another coding agent. It contains the complete set of observations from a read-only audit of the current v1.9.88 Debug build, including source traces, simulated user sequences, exact test results, build/release weaknesses, and suggested corrective measures.

No production source was modified during the audit. Findings should be re-verified immediately before implementation in case the branch has advanced.

## Executive summary

The v1.9.88 executable, generated configuration, source tree, and `oracool.mpq` were mutually current at the end of the audit. Ninja reported `no work to do`, the branch matched its upstream, and the new 273-icon atlases had the correct structural frame counts.

The build is nevertheless not release-ready. The audit found three high-priority player-facing defects:

1. A purchase from a completely full Smith, Witch, or Healer stock can read beyond the corresponding fixed-size array.
2. Paid Repair/Recharge cursors are not cancelled consistently when a shop closes, allowing paid service outside the shop or accidental fallback to the destructive vanilla skill.
3. The new single-page shop hides overflow visually but continues to expose it to keyboard/controller navigation and purchase logic.

It also found a v1.9.88-specific hot-reload out-of-bounds path, broken UTF-8 vertical tab labels, incomplete prompt teardown, a sale-settlement loss condition, nondeterministic parallel tests, an order-dependent store test, and several build/release hygiene gaps.

Recommended release order:

1. Fix the three vendor-stock out-of-bounds loops and add full-array tests.
2. Centralize shop teardown and make placed stock the authority for navigation and transactions.
3. Fix the hot-reload cache, UTF-8 label layout, prompt teardown, and sale preflight.
4. Isolate all save-writing tests and reset store globals between tests.
5. Wire `oracool.mpq` into the build graph, stop globbing arbitrary DLLs, add a real sanitizer configuration, and correct artifact version identity.

---

## Finding 1 — P1: full vendor stock causes an out-of-bounds access

### Evidence

The three stock-removal loops terminate only when the next item is empty:

- Smith: `Source/stores.cpp:2178-2191`, especially line 2187
- Witch: `Source/stores.cpp:2746-2757`, especially line 2753
- Healer: `Source/stores.cpp:2958-2987`, especially line 2983

Representative code shape:

```cpp
for (; !smithitem[idx + 1].isEmpty(); idx++) {
    smithitem[idx] = std::move(smithitem[idx + 1]);
}
smithitem[idx].clear();
```

The special case handles only a purchase whose initial index is the final array entry. It does not protect a purchase from an earlier index when the array contains no empty sentinel.

The current generators deliberately permit full arrays:

- `SMITH_ITEMS` is 45 (`Source/stores.h:34`). A maximum base roll is 23 items, followed by 7 salvage charms and 15 Oracool entries: `23 + 7 + 15 = 45`. The maximum base roll occurs on one of seven count outcomes when all reserved helpers fill.
- `WITCH_ITEMS` is 90 (`Source/stores.h:33`). A maximum base roll is 48, followed by 7 salvage, 12 fixed Oracool entries, 10 books, 8 staves, and 5 rare staves: `48 + 7 + 12 + 10 + 8 + 5 = 90`. The maximum base roll is one of thirteen count outcomes when the helper pools are eligible.
- The Healer array has 20 entries. Its generated base count is capped at 15 and then 5 Oracool charms are stocked. The cap is reached by 3/8 Diablo count rolls and 5/10 Hellfire count rolls.

`SortVendor` already contains explicit capacity bounds, which is additional evidence that a completely full vendor array is an expected state rather than an impossible invariant.

### Simulated trigger

1. Generate a stock with every entry non-empty.
2. Buy the first or any middle non-replenishing item.
3. Compaction shifts entries until `idx` reaches the last valid entry.
4. The next loop condition reads `stock[capacity]`.

The first invalid operation is an out-of-bounds read. If adjacent memory happens to look non-empty, the loop may import unrelated data into the final vendor slot and continue beyond it, turning the issue into corruption or a crash.

### Required measure

Use a capacity-bounded move for every fixed array and clear the final slot. For example:

```cpp
std::move(std::begin(stock) + idx + 1, std::end(stock), std::begin(stock) + idx);
stock[capacity - 1].clear();
```

Also validate `idx` before taking money or placing the purchased item. Do not rely on an empty sentinel when full arrays are valid.

### Required tests

For Smith, Witch, and Healer independently:

- Fill every array entry.
- Purchase the first removable entry.
- Purchase a middle entry.
- Purchase the final entry.
- Verify exactly one item is removed, relative order is preserved, the final entry is empty, and no adjacent global state changes.
- Run these tests under ASan or another bounds checker.

---

## Finding 2 — P1: shop Repair/Recharge cursor lifetime is broken

### Evidence

The service cursor is represented by two pieces of state:

- `ShopArmedServiceCursor`, which distinguishes paid shop service from the vanilla skill.
- `pcurs`, which remains `CURSOR_REPAIR` or `CURSOR_RECHARGE` until explicitly changed.

Arming occurs at `Source/stores.cpp:4531-4545`.

`DisarmShopServiceCursor` at `Source/stores.cpp:4557-4562` clears only `ShopArmedServiceCursor`. It does not restore `CURSOR_HAND`.

Important exit paths are inconsistent:

- Walkaway at `Source/stores.cpp:3508-3539` calls `DisarmShopServiceCursor`, then closes the store, but leaves the special cursor active.
- The shop X button at `Source/oracool/shop_grid.cpp:773-777` sets `stextflag = TalkID::None` without clearing either state.
- `StoreESC` does not centrally cancel the service cursor.
- The shop-overlap/automatic close path around `Source/stores.cpp:4841-4853` closes the store without a complete cursor teardown.
- `InitStores` calls `DisarmShopServiceCursor`, but the function still does not reset `pcurs`.

Inventory targeting at `Source/diablo.cpp:3218-3273` first checks the paid flag. If it is absent, the same cursor falls through to the vanilla `DoRepair`/`DoRecharge` path. Vanilla Repair reduces `_iMaxDur`; vanilla Recharge reduces `_iMaxCharges` (`Source/items.cpp:7947-7990`).

### Simulated user sequences

#### Sequence A: paid service outside a shop

1. Open a Smith or Witch grid.
2. Arm Repair or Recharge.
3. Click the shop X button, press an exit route that does not disarm, or allow an automatic overlap close.
4. Click an inventory item.

The paid-service flag may still be armed, so the shop transaction can execute and charge gold even though no shop is open.

#### Sequence B: destructive vanilla fallback after walkaway

1. Begin or retain a movement path while the shop panel is open.
2. Arm Repair or Recharge.
3. Move more than five tiles from the vendor.
4. Walkaway clears only the paid discriminator and closes the shop.
5. Click a damaged item or discharged staff.

The still-active special cursor enters the vanilla skill branch. Maximum durability or charges may be permanently reduced, contrary to the paid shop operation the player selected.

### Required measure

Create one function such as `CancelShopServiceCursor()` that:

1. Clears `ShopArmedServiceCursor`.
2. If `pcurs` is `CURSOR_REPAIR` or `CURSOR_RECHARGE`, calls `NewCursor(CURSOR_HAND)`.
3. Is called by every shop exit and transition: X, ESC, walkaway, overlap closure, initialization, vendor/dialog transitions, and exceptional exits.

Defensively make `IsShopRepairCursorArmed()` and `IsShopRechargeCursorArmed()` require that the appropriate shop/vendor screen is still active. The flag alone should not grant authority to charge a transaction.

### Required tests

- Arm Repair -> X -> click item: no repair and no charge.
- Arm Recharge -> ESC -> click staff: no recharge and no charge.
- Arm Repair -> walk away -> click item: vanilla Repair must not run.
- Arm Recharge -> automatic close -> click staff: vanilla Recharge must not run.
- Successful in-shop Repair/Recharge still performs one transaction, charges once, and returns to the hand cursor.

---

## Finding 3 — P1: hidden single-page shop stock remains selectable and purchasable

### Evidence

The shop grid is 10 columns by 16 rows (`Source/oracool/shop_grid.cpp:30-37`).

`PlaceStock` at `Source/oracool/shop_grid.cpp:170-203` returns only entries that geometrically fit. Entries that do not fit remain in the underlying `stock` vector. The loop deliberately continues after a failure so that later small items can fill gaps.

`PageCount` at lines 212-215 always returns 1.

However:

- `MoveShopGridSelection` at lines 882-901 computes modulo `stock.size()`, not the number or indices of placed entries.
- Its loop that updates `ShopGridPage` does nothing when the selected stock index was not placed.
- `ActivateShopGridSelection` at lines 957-963 purchases `stock[ShopGridSel]` without verifying that the entry is present in `PlaceStock(stock)`.

Mouse hit-testing uses placed entries, so the problem is primarily keyboard/controller reachable.

### Simulated trigger

1. Open a generously generated Witch/Smith shelf containing more area than the 10x16 grid can display.
2. Use arrows or controller navigation enough times to advance beyond the visible placed indices.
3. The selection outline disappears because no `PlacedSlot` corresponds to `ShopGridSel`.
4. Press Enter/activate.
5. The invisible raw stock entry is purchased.

Buying an earlier visible item also compacts a raw vendor array and repacks the page. A previously hidden item can then appear, making a supposedly fixed one-page shelf replenish from invisible stock.

### Required measure

Make one placement result authoritative for:

- drawing;
- hover hit-testing;
- keyboard/controller navigation;
- selection validation;
- activation/purchase;
- deciding which stock is actually for sale.

Either trim the generated vendor stock to the placed shelf at generation time or maintain a stable list of placed stock indices and navigate only that list. Activation must refuse an index that is not currently placed.

### Required tests

- Generate a mixed-size 90-entry Witch stock.
- Assert every selectable index is visible and every visible item is selectable.
- Iterate every keyboard/controller selection and assert a corresponding `PlacedSlot` exists.
- Assert Enter purchases the item whose outline is visible.
- Assert buying a visible item does not reveal hidden inventory unless explicit restocking is the intended design.

---

## Finding 4 — P2: v1.9.88 hot reload retains an obsolete frame-inset cache

### Evidence

`ArtAsset` contains `std::vector<uint8_t> cellInsets` in `Source/oracool/hud_art.cpp:54`.

`StripCellInset` at lines 1442-1478 computes the cache only when `cellInsets.empty()`. It then returns `cellInsets[index]` without checking that the cached vector matches the newly loaded frame count.

`ResetHudArtCaches` at lines 1279-1317 clears RGBA data, dimensions, load state, and generated surfaces, but never clears `cellInsets`.

This is directly relevant to v1.9.88 because the replaced atlases expanded:

- Barbarian: 30 -> 49 frames
- Rogue: 30 -> 49
- Sorceress: 30 -> 48
- Bard: 21 -> 39
- Monk: 21 -> 39
- Paladin: already 49

The `reloadassets` debug command calls `ResetHudArtCaches` at `Source/debug.cpp:867-872`.

### Impact

If a running Debug process previously cached an old shorter strip and then reloads the expanded asset, `StripCellInset` can validate an index against the new frame count and then index the old shorter vector. That is undefined behavior. Even when the frame count stays the same, changed transparent margins retain stale crop values and render incorrectly.

### Required measure

Add `asset.cellInsets.clear()` to the reset lambda. Consider storing the dimensions or frame count used to build the cache and rebuilding whenever they change.

### Required test

Load a short synthetic strip, force inset computation, replace/reload it with a longer strip, then request the final frame. Assert the cache resizes and the expected source rectangle is used.

---

## Finding 5 — P2: vertical shop labels split UTF-8 into invalid bytes

### Evidence

`DrawVerticalLabel` at `Source/oracool/shop_grid.cpp:457-488`:

- iterates `char` bytes;
- performs ASCII-only uppercasing;
- uses `glyphs.size()` as the visible character count;
- passes each single byte as a one-byte `string_view` to `DrawString`.

The renderer at `Source/engine/render/text_render.cpp:527-529` stops its loop when `DecodeFirstUtf8CodePoint` returns `Utf8DecodeError`.

Existing affected translations include:

- Bulgarian `Magic` -> `Магия`
- Russian `Magic` -> `Магия`
- Simplified Chinese `Magic` -> `魔法`

Every byte of those non-ASCII code points is invalid when passed independently.

### Impact

Vertical tab labels can be blank, truncated, or garbled. Pitch is also calculated from byte count, so multibyte labels are unnecessarily compressed even if decoding happened to recover.

### Required measure

Decode UTF-8 into code points or grapheme clusters, lay out one complete encoded slice per vertical position, and base pitch on decoded unit count. Avoid bytewise uppercasing; use translated uppercase labels or a Unicode-aware transformation if uppercase is required.

### Required tests

- ASCII label.
- Cyrillic label.
- CJK label.
- A multicode-point grapheme if the font/input pipeline supports one.
- Assert no invalid UTF-8 slice is passed to `DrawString` and every decoded unit fits the tab rectangle.

---

## Finding 6 — P2: walkaway bypasses Refresh Until prompt teardown

### Evidence

Walkaway at `Source/stores.cpp:3537-3539` performs:

```cpp
DisarmShopServiceCursor();
IsRefreshUntilPromptOpen = false;
stextflag = TalkID::None;
```

The proper `CloseRefreshUntilPrompt` at lines 3569-3576 performs three operations:

1. `SDL_StopTextInput()`
2. clears `IsRefreshUntilPromptOpen`
3. resets `RefreshUntilPromptInputState`

Because walkaway clears the flag directly, a later call to `CloseRefreshUntilPrompt` returns immediately and cannot repair the stale SDL/input state.

### Impact

The prompt disappears visually while SDL text input/IME mode and the optional prompt state may remain active. Later keyboard behavior can therefore be inconsistent until another code path resets text input.

### Required measure

Call `CloseRefreshUntilPrompt()` before closing the store. More generally, route every prompt exit through that one teardown function.

### Required test

Open the prompt while the hero has a continuing movement path, exceed the five-tile threshold, and assert:

- prompt flag is false;
- SDL text input is inactive;
- optional input state is reset;
- the shop is closed.

---

## Finding 7 — P2: sale proceeds can be discarded near the stash cap

### Evidence

`CreditSaleProceeds` at `Source/stores.cpp:1562-1588` deposits the entire sale into `Stash.gold` only if the entire amount fits under `INT_MAX`. Otherwise it routes the entire amount through `AddGoldToInventory`, recomputes carried gold, and logs any unplaced remainder as gold lost.

This ignores partial stash headroom. For example, if the stash has room for 100 gold and the sale is worth 1,000, the code attempts to place all 1,000 in the backpack rather than 100 in the stash and 900 in the backpack.

The direct sale paths at `Source/stores.cpp:4390-4495` do not consistently preflight complete settlement. The right-click path removes the item before calling `CreditSaleProceeds`. The held-item path assumes a stash has no capacity issue, which is false at the integer cap.

An ordinary inventory sale often frees enough cells to hold its value, so this condition is most visible with very high-value items, stacked values, a held item not originating from the backpack, or a saturated backpack. The code's explicit `gold lost` branch confirms that incomplete settlement is an accepted current outcome.

### Required measure

Before mutating the sold item:

1. Calculate stash headroom.
2. Calculate actual backpack gold capacity.
3. Refuse the transaction unless their sum covers the entire proceeds.
4. Deposit up to stash headroom and place only the remainder in the backpack.
5. Record buyback and remove/clear the item only after full settlement succeeds.

### Required tests

- `Stash.gold == INT_MAX`, full backpack, sale must be refused and item preserved.
- Small stash headroom plus enough backpack room, proceeds must split exactly.
- Small stash headroom plus insufficient backpack room, sale must be refused.
- Exercise list sale, right-click sale, held-item sale, and Sell All.

---

## Finding 8 — P2/QA: CTest cases race on shared save files

### Evidence

`test/CMakeLists.txt:52-56` registers every GoogleTest case independently with `gtest_discover_tests`, allowing CTest to run cases in parallel.

Save-related fixtures set their preference path to the common build base and use fixed names such as:

- `single_0.sv`
- `multi_0.sv`
- `stash.sv`

For example, `test/loadsave_test.cpp:24-33` uses `paths::BasePath()` and removes `single_0.sv`.

The exact v1.9.88 parallel run produced additional failures that do not occur serially:

- `LoadSaveOracoolItemExtensionsTest.StoredRecordWinsOverAnUnreplayableItem`: checksum mismatch and empty loaded inventory.
- `Writehero.HeroSurvivesAWriteAndReadsBack`: staging/write failures involving `multi_0.sv`, followed by SEH exception `0xc0000005`.
- `Writehero.SaveHeroAndStashWritesBothForANewCharacterWithADirtyStash`: failed writing `single_0.sv`.

The failing collision set changed across separate parallel runs, confirming nondeterminism rather than a stable product assertion.

After the audit runs, the build root contained test-created `multi_0.sv` and `stash.sv`, further demonstrating that the test output directory and runtime artifact directory are mixed.

### Required measure

Give every test process a unique temporary preference/config directory. The preferred solution is process/test-specific filesystem isolation, not serializing the entire suite.

As a temporary containment measure, mark all save-mutating tests with a shared CTest `RESOURCE_LOCK` or `RUN_SERIAL`. This prevents collisions but does not fix build-root pollution.

### Required tests/CI behavior

- `ctest -j 8` should pass with the same result as `ctest -j 1`.
- Two save tests run concurrently should never see one another's archive.
- Test completion should leave no save/config/log files in the binary directory.

---

## Finding 9 — P2/QA: store tests leak global player state across test cases

### Evidence

The exact command:

```text
stores_test.exe --gtest_shuffle --gtest_random_seed=37473 --gtest_repeat=3
```

produced:

- Iteration 1: `Stores.Sold_BuyBackChargesTheSalePriceNotTheItemValue` failed (`storenumh` was 2, expected 1).
- Iteration 2: the same test failed.
- Iteration 3: it passed.

`Stores.StorytellerIdentify_ListsAndIdentifiesItemStoredInExtraTab` at `test/stores_test.cpp:145-178` leaves an item in an extra inventory tab. `Stores.Sold_BuyBackChargesTheSalePriceNotTheItemValue` at lines 235-255 clears the base inventory/belt state but not all extra tabs, so its expected store count depends on prior execution order.

### Required measure

Use a `Stores` test fixture with `SetUp`/`TearDown` that resets:

- `Players`, `MyPlayer`, and held item;
- base inventory list/grid/count;
- all extra inventory tab lists, grids, and counts;
- belt and equipment where relevant;
- stash state;
- buyback/store arrays and counters;
- `stext*` globals, selected page/index, and service cursors.

Run shuffled/repeated tests in CI to keep isolation honest.

---

## Standing serial test failures

The exact v1.9.88 serial suite result was 563/565 passed in approximately 23.7 seconds.

### `Drlg_l1.CreateL5Dungeon_diablo_3_844660068`

Observed mismatch:

- tile `(1,0)` expected 4, actual 22;
- `ViewPosition` actual `(67,72)`, expected `(67,52)`;
- second generation actual `(73,83)`, expected `(85,45)`.

The fixture/test is unchanged from the upstream baseline, while current option defaults include `randomizeQuests = false` (`Source/options.cpp:1346`) and `TestInitGame` does not pin that option (`test/drlg_test.hpp:52-61`). Level 3's Skeleton King quest affects generation.

Measure: explicitly set every dungeon-affecting option in the fixture. Then either retain the old expected generation under the intended old option or regenerate the fixture for the new all-quests/default behavior. Do not let user configuration/default drift define a deterministic test.

### `Timedemo.WarriorLevel1to2`

Observed result: `Unable to load character`.

`ReadHero` at `Source/pfile.cpp:133-151` requires the decoded `hero` record to be at least the current `sizeof(PlayerPack)`. The timedemo fixture predates growth of `PlayerPack`, so `pfile_read_player_from_save` fails at `Source/pfile.cpp:983-984`.

Measure: decide and document the actual supported save-migration boundary. Update the timedemo fixture to a supported current save, and add separate explicit compatibility tests for every older format the release promises to load. Do not make a gameplay timedemo double as an undocumented binary-format migration test.

This failure is older than v1.9.87/v1.9.88 and should not be described as a new icon/shop regression.

---

## Exact test results

### Serial

Command:

```text
ctest --test-dir build/x64-Debug --output-on-failure -j 1
```

Result:

- 563/565 passed.
- Failures: dungeon fixture and timedemo fixture described above.

### Parallel

Command:

```text
ctest --test-dir build/x64-Debug --output-on-failure -j 8
```

Result:

- 560/565 passed in approximately 7.0 seconds.
- The two standing failures remained.
- Three additional shared-save failures appeared.
- Other parallel runs produced a different set of save-extension failures, confirming the race.

### Shuffled stores

Command:

```text
stores_test.exe --gtest_shuffle --gtest_random_seed=37473 --gtest_repeat=3
```

Result:

- First two iterations: 14/15 passed.
- Third iteration: 15/15 passed.
- Failure depends on leaked extra-tab inventory state.

### Coverage gaps discovered by search

No targeted tests were found for the new/high-risk paths named below:

- `CloseStoreIfPlayerWalkedAway`
- `PlaceStock`
- `MoveShopGridSelection`
- `DrawVerticalLabel`
- `ArmShopRepairCursor`
- `ArmShopRechargeCursor`
- full Smith/Witch/Healer stock removal
- HUD art cache reload after changing atlas frame count

These gaps explain why the serial suite can remain almost green while the new UI/mechanic paths contain reproducible state-machine and bounds defects.

---

## v1.9.88 icon-mechanic audit

### What was verified

The six current PNG IHDR dimensions correspond to the intended 56x56 horizontal frame counts:

| Class | Dimensions | Frames |
|---|---:|---:|
| Paladin | 2744x56 | 49 |
| Barbarian | 2744x56 | 49 |
| Sorceress | 2688x56 | 48 |
| Rogue | 2744x56 | 49 |
| Bard | 2184x56 | 39 |
| Monk | 2184x56 | 39 |

Total: 273 frames.

The counts agree with the class block counts documented in `Source/oracool/hud_art.cpp` and sum to `ClassTreeSkillCount`.

The current `oracool.mpq` was 35,729,453 bytes and timestamped `2026-08-27T05:24:47Z`, after the six atlas source files at approximately `05:23:49Z`. The v1.9.88 development report also records a successful 416-file repack. Therefore the current archive was fresh at audit time.

The `ClassTreeStrips` array is used by load, quantize, and reset passes, so all six classes are included in those pipelines.

### What was not visually proven

The audit did not conduct a full interactive playthrough of all 273 frames. Structural dimensions and source ordering were checked, and the project report states that the 273 manifest names were diffed against the skill table with zero differences, but a human visual pass is still warranted.

Required manual smoke test:

- Open all four pages for every class.
- Inspect the deepest rows that were previously beyond the old strips.
- Check locked, unlocked/unspent, invested, selected, HUD-well, and speedbook render states.
- Use `reloadassets` after replacing an atlas to verify the cache fix.

---

## Build provenance and freshness

The workspace advanced during the audit from v1.9.86 to v1.9.87 and then v1.9.88 because another process was committing/rebuilding. All final provenance, test, and freshness checks were repeated against v1.9.88.

Final state:

- HEAD: `f256bc55f86b83ee1c6217e104390df5ba277c2a`
- Upstream: same commit
- Generated `Source/config.h`: Oracool 1.9.88
- `DiabloOrcl.exe`: timestamp `2026-08-27T05:24:38Z`
- `libdevilutionx_so.dll`: timestamp `2026-08-27T05:24:39Z`
- `oracool.mpq`: timestamp `2026-08-27T05:24:47Z`
- Direct Ninja dry run: `ninja: no work to do`
- Tracked files: clean at the final check
- Existing untracked `.claude/` directory: not touched by the audit

Build configuration:

- Generator: Ninja
- Compiler: MSVC 14.52.36520
- Build type: Debug
- Dependency mode: vcpkg
- `BUILD_TESTING=ON`
- `DISCORD_INTEGRATION=OFF`
- `BUILD_ASSETS_MPQ=OFF`
- `CPACK=FOR_RELEASE`

---

## Build/release observations

### Sanitizer configuration is misleading and ineffective

`CMakeCache.txt` reports:

- `ASAN=FOR_DEBUG`
- `UBSAN=FOR_DEBUG`

But `CMake/platforms/windows.cmake:1-2` unconditionally executes:

```cmake
set(ASAN OFF)
set(UBSAN OFF)
```

The generated Ninja compile lines contain normal MSVC Debug checks such as `/RTC1`, but no ASan/UBSan instrumentation. The full-stock out-of-bounds defect therefore receives no runtime sanitizer detection.

Measure: make the cache accurately report unsupported/disabled status, and add a real clang-cl ASan build in CI if MSVC configuration cannot provide the desired sanitizer coverage.

### PE hardening

The Debug executable is x64 and has:

- ASLR/dynamic base;
- high-entropy virtual address support;
- DEP/NX compatibility;
- compiler stack cookies.

Control Flow Guard metadata was not convincing in this Debug artifact: the load configuration indicated instrumentation-related metadata, but the Guard CF table/count was empty and the corresponding effective DLL characteristic was absent. Treat this as a lower-priority Debug observation and validate the actual Release artifact separately before claiming CFG coverage.

### Executable identity is incomplete

Windows version metadata fields were empty:

- `FileVersion`
- `ProductVersion`
- `FileDescription`
- `ProductName`
- `OriginalFilename`

`Packaging/windows/devilutionx.rc` contains the manifest/icon resources but no `VERSIONINFO` resource.

The source implementation of `--version` at `Source/diablo.cpp:1585-1589` prints `PROJECT_NAME` plus `PROJECT_VERSION`, which identifies the DevilutionX engine base (`1.5.5Debug`) rather than Oracool v1.9.88. The main menu and `gszProductName` correctly use `ORACOOL_VERSION`, so the artifact has inconsistent identities.

The command exited successfully during the harness check but did not produce visible parent-console output in that environment. That absence is inconclusive because the Windows console writer attempts to attach to the parent console. The source-level version mismatch is conclusive.

Measure:

- Make `--version` print both Oracool release and engine base.
- Add Windows `VERSIONINFO` populated from `ORACOOL_VERSION` and appropriate product strings.
- Add a small version smoke test that captures output in a known console/pipe configuration.

The Debug executable is unsigned. That is expected for a local Debug build and is not itself a defect; signing should be evaluated on the Release artifact.

### Runtime directory contains stale/test-only DLLs

DLLs in the Debug root included:

- `bz2d.dll`
- `discord_game_sdk.dll` even though Discord integration is off
- `fmtd.dll`
- `gmock.dll`
- `gtest.dll`
- `libdevilutionx_so.dll`
- `libpng16d.dll`
- `libsodium.dll`
- `SDL2_imaged.dll`
- `SDL2d.dll`
- `zlibd1.dll`

`gtest`, `gmock`, and `libdevilutionx_so` are test-related. The Discord DLL is stale relative to the current configuration.

`CMakeLists.txt:565-567` uses `file(GLOB ... "${SDL2_WIN32_DLLS_DIR}/*.dll")`, and lines 627-630 install every globbed DLL. A reused directory configured for packaging can therefore silently include disabled or test-only libraries.

Measure: use an explicit allow-list of runtime dependencies or CMake runtime dependency collection from the actual executable. Package from a clean Release tree, never the shared Debug/test directory.

### `oracool.mpq` is not part of the default build graph

`oracool_mpq_pack` is `EXCLUDE_FROM_ALL` at `CMakeLists.txt:377-382`. The documented workflow requires manually running `tools\build_oracool_mpq.cmd`.

Install/package logic at `CMakeLists.txt:614-624` verifies only that `oracool.mpq` exists. It does not verify that the archive is newer than or content-equivalent to `Packaging/resources/oracool_assets`.

The current v1.9.88 archive was fresh, so this is a future/reproducibility risk rather than a claim that the audited archive is stale.

Measure:

- Add a generated file list and custom command whose dependencies are all packed assets.
- Make the game/package target depend on the MPQ output.
- Retain the install-time existence guard, but add a freshness/content manifest check.
- Build packages in a clean directory to prove the archive can be produced without manual state.

### Build directory is non-hermetic

The binary directory mixes:

- executable and runtime DLLs;
- test executables and test DLLs;
- PDB/ILK/import libraries;
- `diablo.ini`;
- save files and stash files;
- logs;
- manually generated `oracool.mpq`.

This makes packaging globs dangerous, creates test collisions, and obscures which files are genuine runtime deliverables.

Measure: separate runtime, test output, test temporary data, tooling output, and packaging staging directories. Large PDB/ILK files are expected in Debug but should never enter Release packages unless intentionally publishing symbols separately.

---

## Additional investigated paths and non-findings

### Typed vendor selection

`StockVendorTypedItems` and `GetItemIndexForDroppableItem` were inspected for a suspected invalid item-index path. The current fallback storage is static/zero-initialized and yields a valid enum value rather than a directly proven out-of-range index. No actionable memory-safety finding was established there. Do not report or change this path without a concrete failing seed or stronger invariant evidence.

There may still be design questions around vendor flags/level selection, but they were not substantiated sufficiently to rank as bugs in this audit.

### Current MPQ freshness

The archive is fresh in the audited build. The finding is that freshness is not guaranteed by the build graph, not that v1.9.88 currently ships stale art.

### New atlas geometry

The six atlas dimensions are divisible by 56 and match the expected class counts. No geometric frame-count defect was found in the new PNG files themselves.

### Debug signature

`NotSigned` is informational for this Debug build, not a release blocker by itself. Check the actual distributed Release binary separately.

---

## Proposed implementation sequence

### Patch 1: memory safety and tests

- Replace all three sentinel compaction loops with bounded moves.
- Add index guards before transaction mutation.
- Add full-stock first/middle/last tests.
- Run them under an actual sanitizer configuration.

### Patch 2: unified shop teardown

- Add `CancelShopServiceCursor()`.
- Add a unified shop/prompt close routine if feasible.
- Route X, ESC, walkaway, overlap close, dialog transitions, and initialization through it.
- Make paid-service predicates require a compatible active shop.
- Add cursor lifecycle regression tests.

### Patch 3: one-page shelf authority

- Represent the visible shelf once.
- Navigate and activate only placed indices.
- Decide explicitly whether overflow is discarded, deferred, or restocked; do not let raw array compaction decide implicitly.
- Add mixed-size, keyboard/controller, and post-purchase tests.

### Patch 4: UI/state correctness

- Clear `cellInsets` on art reload.
- Decode vertical labels as UTF-8 units.
- Route walkaway through `CloseRefreshUntilPrompt`.
- Preflight and atomically settle sale proceeds.

### Patch 5: deterministic test infrastructure

- Unique temp directory per test process.
- Store fixture that resets all tabs/global state.
- Run both serial and parallel CTest in CI.
- Run shuffled/repeated store tests in CI.

### Patch 6: reproducible artifacts

- Make MPQ generation a real dependency.
- Use an explicit runtime DLL set.
- Separate Debug/test/package outputs.
- Add Windows version resources and correct CLI version output.
- Add a clean Release packaging smoke test and validate hardening on that artifact.

---

## Acceptance checklist

A corrective branch should not be considered complete until all of the following are true:

- [ ] Full Smith, Witch, and Healer stocks can be purchased from without any out-of-bounds access.
- [ ] Every shop exit returns Repair/Recharge cursors to the hand and clears paid authority.
- [ ] A shop service can never execute while the correct shop is not active.
- [ ] Every keyboard/controller-selected shop item is visible.
- [ ] Enter can never purchase an unplaced stock entry.
- [ ] Buying an item does not unexpectedly reveal hidden one-page stock.
- [ ] `reloadassets` safely handles a changed frame count and changed transparent margins.
- [ ] Cyrillic and CJK vertical tab labels render as complete characters.
- [ ] Walkaway fully stops SDL text input and clears prompt state.
- [ ] No valid sale can remove an item without paying its complete value.
- [ ] `ctest -j 1` and `ctest -j 8` produce the same passing result.
- [ ] Shuffled/repeated store tests are deterministic.
- [ ] The dungeon test pins every generation-affecting option.
- [ ] The timedemo uses a supported save and migration boundaries have dedicated tests.
- [ ] A clean build automatically produces a current `oracool.mpq`.
- [ ] Packaging contains only intended runtime DLLs.
- [ ] `DiabloOrcl.exe --version`, Explorer metadata, the main menu, and multiplayer identity consistently identify the Oracool release.

