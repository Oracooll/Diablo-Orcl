# ChatGPT Deep Audit - local v1.9.97

Date: 2026-08-27

Audited workspace: `C:\Users\hroga\OneDrive\2. Personal Files\Software\Diablo\Diablo Orcl V1`

Primary build: `build\x64-Debug`

Audited commit: `820ff72f76945476d90c9a2f4e5213806293dc38`

Commit subject: `The ghost outline was the mouse wheel (v1.9.97)`

Comparison baseline: `ced2886e736bfa262c36a3de8852a0cfbfb28c72` / the v1.9.92 ChatGPT audit

Branch: `oracool-v1-main`, six commits ahead of `origin/oracool-v1-main` and zero behind at audit time

Source and Debug version: `1.9.97`

Release-tree generated version: `1.9.92` (stale comparison tree; not the audited runtime)

## Purpose and scope

This is a fresh audit of the six local commits after the v1.9.92 audit:

1. v1.9.93 - stale paid-service cursor and transactional MPQ publication;
2. v1.9.94 - asset archives in the build graph and shop sale/shelf follow-ups;
3. v1.9.95 - fixture cleanup, temporary-path work, and version identity;
4. v1.9.95 wiki regeneration;
5. v1.9.96 - durability wear consolidation and negative-durability healing;
6. v1.9.97 - deferred monster outline moved before zoom scaling.

The pass included source tracing, adversarial state simulation, an ordinary Debug build, serial and parallel CTest, shuffled/repeated store and Oracool test binaries, release-package preflight, archive/hash comparisons, and the wiki's own integrity verifier.

No source fixes were made. This report is the only file added by the audit.

## Executive summary

No new P1 issue was established. The v1.9.93 paid-service cursor fix and the core MPQ partial-publication fix both hold under the inspected paths.

Seven actionable findings remain:

| # | Priority | Area | Summary |
|---|---|---|---|
| 1 | P2 | Premium shop | One-page stock is not authoritative after a purchase, and Refresh Until can report an item that is not visible |
| 2 | P2 | Supplies shop | The combined Pepin + Adria shelf retains hidden Adria stock that can surface after a purchase |
| 3 | P2 | Selling/gold | A held stack is preflighted at one item's value but paid at the whole stack's value; the unplaced remainder is ignored |
| 4 | P2/build | MPQ concurrency | The packer calls its fixed `.building` path unique, but simultaneous packers still share and delete it |
| 5 | P2/release | Archive integrity | Release packaging validates MPQ timestamps, not contents; the packer's validation checks names but never reads bytes |
| 6 | P3/release | Zip publication | The final zip is deleted before the replacement is moved, so the claimed atomic publication guarantee is false |
| 7 | P2/docs/QA | Wiki | The committed bundle fails its own fingerprint verifier, while the pages are independently two versions behind the code |

The ordinary Debug build is current and internally coherent. CTest remains 570/572 because of the same two standing failures. The two focused stress binaries completed 19,800 shuffled test executions without a failure.

The current Release tree is not release-ready. Its generated configuration still says v1.9.92, its executable has no ProductVersion resource, and `BuildReleasePackage.ps1` correctly refuses to package it.

## Disposition of the v1.9.92 findings

| Previous finding | Current disposition |
|---|---|
| 1. Stale paid-service cursor can become the destructive vanilla skill | Fixed. The click path now consumes stale shop state before vanilla Repair/Recharge fallbacks. Store stress passes. |
| 2. MPQ packer can publish partial output and hide finalization failure | Core failure fixed. It builds a shadow, explicitly finishes/publishes, reopens it, and only then replaces the final archive. Concurrency and content-validation gaps remain in Findings 4 and 5. |
| 3. Debug custom MPQ is stale | Fixed. Both custom and engine archives are build dependencies and are current in Debug. |
| 4. Direct sale gestures can remove an item before all proceeds fit | Partially fixed. The fit gate is present, but it is fed the wrong amount for held stacks and all callers ignore the settlement remainder. See Finding 3. |
| 5. Hidden one-page overflow can surface without a refresh | Partially fixed. Fresh Basic/Magic/Premium/curated stock is trimmed, but Premium refills the trim holes and Supplies is exempt. See Findings 1 and 2. |
| 6. Deleted engine assets can return from a reused tree | Fixed in the ordinary CMake archive path: the explicit engine manifest now drives the MPQ. |
| 7. Class-tree test helpers leak Player state | Fixed. The test fixture resets the Player and the affected binary passed 18,100 shuffled executions. |
| 8. Generated wiki is behind | It was regenerated at v1.9.95, then immediately fell behind v1.9.96/v1.9.97. The bundle also fails its independent fingerprint check. See Finding 7. |
| 9. Fixed temporary paths make concurrent jobs interfere | Packaging staging now uses a GUID. The packer's output shadow remains fixed and the batch response file is only probabilistically unique. See Finding 4. |
| 10. Executable version check is non-authoritative | Fixed for rebuilt binaries through VERSIONINFO. Debug reports ProductVersion 1.9.97. The stale Release executable predates the resource and is correctly rejected. |

---

## Finding 1 - P2: Premium stock stops being a one-page shelf after purchase, and Refresh Until searches invisible stock

### Evidence

`Source/oracool/shop_grid.cpp:712-733` materializes a one-page shelf by running `PlaceStock()` and clearing every raw item that was not placed.

`Source/stores.cpp:4059-4087` applies that trim to `SmithPremiumBuy` when town stock is created. However, trimming the `premiumitems` array does not reconcile `numpremium`.

`Source/stores.cpp:2292-2318` handles a Premium purchase by:

1. clearing the purchased array slot;
2. decrementing `numpremium`;
3. calling `SpawnPremium()`.

`Source/items.cpp:6794-6841` does not refill one sold slot. When `numpremium < 30`, it walks the entire vanilla Premium head and fills every empty slot. It then stocks the Oracool tail and recounts the whole array. Every hole that the one-page trim deliberately created is therefore eligible to return after the first purchase.

The comment at `Source/stores.cpp:2470-2472` says the post-purchase call is merely "restocking a sold slot". The implementation does more than that.

The Refresh Until path has a second form of the same bug:

- `Source/stores.cpp:2403-2430` clears all 30 entries, regenerates them, and scans every raw `premiumitems` entry for the requested name;
- it returns success as soon as any raw entry matches;
- `Source/stores.cpp:3664-3674` closes the prompt and reopens Premium without calling `TrimShopStockToOnePage()`;
- normal manual Refresh does call the trim at `Source/stores.cpp:2465-2475`.

Consequently, Refresh Until can say "Found X" when X is one of the entries `PlaceStock()` cannot place. The result message is true about the backing array but false about what the player can see or buy. Timeout and safety-limit outcomes also leave the final untrimmed generation in place.

### Player-visible scenario

1. Generate Premium stock whose 30 raw items do not all fit the 10x16-cell shop grid.
2. The town setup clears the overflow, but `numpremium` still describes the pre-trim state.
3. Buy one visible item.
4. `numpremium--` makes `SpawnPremium()` enter its refill branch and repopulate all empty vanilla slots, including trim holes.
5. The grid is repacked from a larger backing array, so new stock appears without the player pressing Refresh.

For Refresh Until, choose a target that appears only after the grid is already full. The search reports it found, but the target has no `PlacedSlot`.

### Recommended measure

Make the visible Premium shelf the authoritative stock state, rather than treating placement as a rendering filter:

- reconcile `numpremium` after trimming;
- have a purchase refill only the purchased shelf position, or defer all regeneration to the explicit Refresh action;
- trim every full regeneration, including every Refresh Until exit path;
- declare a target found only after the generation has been materialized and the target has a `PlacedSlot`;
- remove or correct the misleading "one sold slot" comment.

### Required regression tests

- Generate an overfull Premium array, trim it, buy one item, and assert that no previously trimmed entry returns.
- Assert that a purchase creates at most the intended replacement count.
- Put the Refresh Until target only in an unplaced raw entry and assert that the search continues.
- Exercise found, timeout, and 100,000-attempt exits and assert that every resulting stock entry is placed.
- Assert that `numpremium` always equals the number of non-empty `premiumitems` entries after trim, refresh, and purchase.

---

## Finding 2 - P2: Supplies keeps a hidden reserve that surfaces after buying an Adria item

### Evidence

The Supplies tab is a combined view built by `SmithConsumablesStock()` at `Source/stores.cpp:422-432`:

1. four infinite Pepin potion entries;
2. every non-empty item in Adria's `witchitem` array.

Town setup trims `WitchBuy` by itself at `Source/stores.cpp:4083`, then deliberately omits `SmithConsumables` at `Source/stores.cpp:4079-4081`.

`TrimShopStockToOnePage()` also refuses `SmithConsumables` outright at `Source/oracool/shop_grid.cpp:714-718`.

That protects Pepin's replenishing potion objects from being cleared, but it does not make the combined shelf stable. Adria's stock is first sized to fill a page alone. Prepending four Pepin entries reduces the remaining space, so later Adria entries can be unplaced in Supplies while still remaining alive in `witchitem`.

When a non-replenishing Adria entry is bought, `UpdateSmithConsumablesStockAfterPurchase()` at `Source/stores.cpp:3054-3058` removes it from Adria's backing stock. The next render recomputes placement and can bring a previously hidden Adria entry onto the Supplies shelf. That is precisely the hidden-reserve behavior the one-page change was intended to remove.

### Recommended measure

Materialize Supplies as a combined shelf:

- run placement over the protected Pepin prefix plus the Adria entries;
- never clear or remove a Pepin entry during trimming;
- clear only unplaced Adria backing entries;
- perform this once per vendor-stock epoch and after an explicit refresh, not every frame;
- retain the existing removal behavior for a visible non-replenishing Adria purchase.

### Required regression tests

- Fill Adria's own page, prepend all four Pepin items, materialize Supplies, and assert every surviving Adria item has a `PlacedSlot`.
- Buy the first, middle, and last visible non-replenishing Adria entry and assert that no previously hidden item appears.
- Buy each infinite Pepin potion repeatedly and assert that it remains stocked and that the Adria visible set does not change.
- Test mixed item sizes so the assertion covers packing gaps rather than only item count.

### Additional hardening

`GetShopStock(TalkID::SmithConsumables)` at `Source/stores.cpp:4379-4383` pushes every Pepin entry without testing `isEmpty()`, although `InitializeSmithPepinPotions()` says an unavailable potion is cleared and "skipped by the draw." The current four potion types resolve, so this was not reproduced in the normal configuration, but the claimed empty-entry invariant is false. Filter empty protected entries before `PlaceStock()` and drawing.

---

## Finding 3 - P2: held stack sales preflight one unit and pay the entire stack

### Evidence

`GetItemSellValue()` at `Source/items.cpp:3343-3349` calculates a stack sale as:

`max(unit value / 4, 1) * stackCount`

`StoreGoldFit()` at `Source/stores.cpp:2531-2555` does not use that amount. It always starts with:

`int cost = item._iIvalue;`

The held-item path at `Source/stores.cpp:4491-4541` correctly computes `price = GetItemSellValue(sold)`, but then calls `StoreGoldFit(sold, false)`. It later calls `CreditSaleProceeds(price)` and ignores the returned unplaced amount.

For a stack of 99 ordinary consumables with unit value `V`, the gate checks approximately `V`, while settlement attempts to place approximately `24.75 * V`. This is not theoretical inventory state:

- stackable consumables can live on the belt;
- the shop deliberately allows the inventory beside it and permits an item to be picked onto the cursor;
- a held belt stack frees no backpack cell, so `freesItemCells=false` is correct;
- Adria accepts ordinary misc consumables.

At `Stash.gold == INT_MAX`, with backpack gold headroom greater than `V` but less than the exact stack proceeds, the fit gate approves the sale. `CreditSaleProceeds()` places what fits, logs the remainder as lost, and returns it. The caller records the buyback entry and clears the held item anyway.

The right-click backpack path at `Source/stores.cpp:4583-4619` has the same amount mismatch, although the newly freed 1x1 cell makes loss unlikely under current stack/value caps. The helper contract is still wrong and should not depend on that incidental capacity.

All three `CreditSaleProceeds()` callers currently discard its return (`Source/stores.cpp:2599`, `4535`, and `4615`). Its new non-void interface therefore does not yet enforce settlement.

### Recommended measure

Change the fit API to accept the exact transaction amount:

`StoreGoldFit(int exactPrice, const Item *itemWhoseCellsWillBeFreed)`

Then:

- compute `GetItemSellValue()` once;
- use that same value for preflight, buyback price, and settlement;
- do not record the sale or remove/clear the item until settlement is guaranteed;
- treat a non-zero `CreditSaleProceeds()` result as a transaction failure, not merely a log message;
- add a debug assertion that a successful preflight always yields zero remainder.

### Required regression tests

- Held belt stack, stash at `INT_MAX`, insufficient backpack headroom: sale must be refused and the stack must remain held.
- Same state with exact headroom: sale succeeds and every coin is represented.
- Single-item, unidentified magical, identified magical, and 99-unit ordinary stack values all use the exact same price in preflight and settlement.
- Inject a settlement remainder after a successful-looking preflight and assert rollback/no item removal.

---

## Finding 4 - P2/build: simultaneous packers still share the same destructive shadow path

### Evidence

`tools/oracool_mpq_pack.cpp:73-88` says the archive is built into "a unique temporary."

The actual path at line 89 is fixed:

`const std::string tempPath = outPath + ".building";`

Line 90 deletes that path before creating the writer. Failure handling at lines 99-103 also deletes it.

Two packers targeting the same output therefore share:

- `<output>.building`;
- the `MpqWriter` shadow derived from that path;
- cleanup that cannot distinguish which invocation owns the file.

One process can delete the other process's active temporary, fail validation because it sees the other archive, or publish a generation built from the wrong response list. Ordinary Ninja serializes one producer inside one build invocation, but a manual repack, a second shell, or an overlapping CI/package job can run beside it.

The batch wrapper only partially fixes the response-list twin. `tools/build_oracool_mpq.cmd:50` uses:

`%TEMP%\oracool_mpq_files_%RANDOM%_%TIME:~9,2%.txt`

That is probabilistic rather than exclusive (`RANDOM` has only 32,768 values and the clock suffix is hundredths). More importantly, line 61 exits immediately on packer failure, so line 62 never deletes the response file.

### Recommended measure

- Create the archive temporary with exclusive-create semantics in the final archive's directory.
- Include a real unique run ID (GUID or cryptographic random value), not a fixed suffix.
- Keep the temporary on the same volume so the final replace remains atomic.
- Make cleanup ownership explicit and RAII-based.
- Generate the response file through an API that creates a unique file atomically.
- In the batch wrapper, capture the packer exit code, delete the response file in both success and failure cases, then return the captured code.

### Required regression tests

- Launch two packers simultaneously against the same output with different source manifests; one complete, self-consistent archive may win, but neither process may delete/corrupt the other's work and a partial mixture must never publish.
- Force failure during source read, `Finish()`, `Publish()`, validation, and final replace; the prior final archive must remain byte-identical.
- Assert that no response file or build shadow remains after every controlled failure.

---

## Finding 5 - P2/release: archive validation still proves names and timestamps, not contents

### Evidence

The v1.9.93 packer is much safer than the old implementation: it explicitly finishes, publishes, reopens the temporary archive, and validates before replacement.

However, its validation at `tools/oracool_mpq_pack.cpp:151-168` calls only `MpqArchive::HasFile()` for every expected name. It never calls the already available `ReadFile()` API, never compares entry size/content/hash with the source, and never checks for unexpected entries.

`tools/BuildReleasePackage.ps1` does even less for pre-existing build artifacts:

- it checks that `oracool.mpq` and `devilutionx.mpq` exist;
- it compares their modification times with source/deployed assets;
- it never opens either archive or passes the generated manifests to a verifier.

A structurally openable archive with valid name-table entries and corrupt sector data can pass the packer check. A wrong, truncated, or manually replaced build-tree archive can pass packaging merely by having a recent timestamp.

The current trees also demonstrate the opposite problem with timestamps. Debug and Release `oracool.mpq` are byte-identical:

`SHA256 36BBDABDA6C4F41216716CA03787B496140812BD52285C5CD20A3C029501073D`

The Release archive timestamp predates the newest source-asset timestamp, so the packaging check would reject it after the executable is rebuilt despite its content being identical to the current Debug archive. Modification time is therefore neither sufficient to prove correctness nor necessary for correctness.

### Recommended measure

Add a reusable verification mode to `oracool_mpq_pack` and call it from release packaging for both archives:

- open the archive;
- read every manifest entry fully;
- compare length and a strong content hash with the source file;
- fail on read/decompression/CRC errors;
- where the MPQ API permits, verify the exact entry set or embed and verify a manifest entry;
- emit a deterministic manifest digest that packaging records and checks.

Use timestamps only as an incremental-build optimization. Do not use them as release evidence.

### Required regression tests

- Flip one byte in entry data while preserving the file table and timestamp; verification must fail.
- Replace an archive with a different valid MPQ and touch it newer than all sources; packaging must fail.
- Touch a source file without changing bytes; content verification must pass.
- Remove an entry, add an unexpected entry, and truncate the archive; each must fail with a specific diagnostic.

---

## Finding 6 - P3/release: final zip publication is not atomic despite the comment

### Evidence

`tools/BuildReleasePackage.ps1:238-240` says the output is "either the previous zip or a complete new one."

The implementation at lines 243-245 is:

1. create a complete uniquely named partial zip;
2. delete the existing final zip;
3. move the partial zip to the final name.

There is a real interval between steps 2 and 3 where no final zip exists. A crash, permission error, antivirus lock, OneDrive race, or second packager in that interval loses the prior known-good final. `Move-Item -Force` after an explicit delete is not an atomic replacement primitive.

### Recommended measure

Publish through an actual same-volume replace operation. Keep the unique partial in `OutDir`, flush/close it, verify the zip can be opened and contains the exact staged manifest, then atomically replace the final path. Retain the old zip if replacement fails.

### Required regression tests

- Inject failure immediately before replacement and during replacement; the previous final must remain unchanged.
- Run two packaging jobs for one version simultaneously; the final must always be one complete verified zip.
- Open the final zip after publication and compare its exact file list, sizes, and hashes with the staged manifest.

---

## Finding 7 - P2/docs/QA: the committed wiki bundle fails verification and the pages lag v1.9.97

### Evidence

The repository is clean for tracked wiki files, and no commit after `50219f3` changed `wiki/`. Nevertheless:

`tools\BundleWiki.ps1 -Verify`

fails with:

- current 101-source-file fingerprint: `f7ffddd9e7564a9710e0c45c0a45d321d3a3893b6993c51277e8e2e12a93271f`;
- bundle-stamped fingerprint: `6a59b9b13a2e10e0fb1d5904b50a1e90217041a88bb2d0ef3b7749adcbc5eaae`.

This mismatch exists in the committed state; it is not caused by an uncommitted local wiki edit.

There is also an independent code-version mismatch:

- `ORACOOL_VERSION`: `1.9.97`;
- first line of `wiki/data.js`: `"version":"1.9.95"`.

The pages therefore omit or misdescribe at least the v1.9.96 durability behavior and the v1.9.97 zoom/outline fix. Because `BundleWiki.ps1 -Verify` exits immediately on the fingerprint mismatch, it never reaches its later "pages behind code" warning. One failed invariant masks the second.

### Recommended measure

- Determine why the v1.9.95 regeneration committed a stamp that does not match the committed source set.
- Regenerate `data.js` from v1.9.97 and rebuild the bundle.
- Run `BundleWiki.ps1 -Verify` from a clean checkout and require zero exit status before publishing.
- Change verification to evaluate and report both bundle-vs-pages and pages-vs-code before returning failure.
- Add the verifier to CI or the release checklist so a version bump cannot ship with old generated docs.
- Record the published artifact's digest and compare it with the verified local bundle, closing the final local-vs-published gap.

### Required regression tests

- Clean-checkout verification on Windows and the CI host must produce the same fingerprint.
- A changed page must report bundle drift.
- Only bumping `ORACOOL_VERSION` must report pages-vs-code drift even when bundle-vs-pages also fails.
- A successful build must leave `data.js`, the bundle stamp, and the published-artifact digest mutually consistent.

---

## Exact build and test results

### Debug build

The build was run from a Visual Studio developer environment:

`cmake --build build\x64-Debug --parallel`

Result:

`[0/2] Re-checking globbed directories...`

`ninja: no work to do.`

Verified outputs:

- `build\x64-Debug\Source\config.h` reports Oracool v1.9.97;
- `DiabloOrcl.exe` is 13,226,496 bytes and ProductVersion is `1.9.97`;
- `oracool.mpq` is 35,729,453 bytes and current;
- `devilutionx.mpq` is 4,123,041 bytes and current;
- both archives are normal build dependencies now.

### Serial CTest

Result:

- total: 572;
- passed: 570;
- failed: 2;
- elapsed: 21.82 seconds.

Failures:

1. `Drlg_l1.CreateL5Dungeon_diablo_3_844660068`
   - actual tile 22 versus expected 4 at 1x0;
   - ViewPosition `(67,72)` versus `(67,52)`;
   - ViewPosition `(73,83)` versus `(85,45)`.
2. `Timedemo.WarriorLevel1to2`
   - fatal load diagnostic: `Unable to load character`.

Both are standing baseline failures, but they still remove two important integration signals. The dungeon fixture needs either an explained/reviewed rebaseline or a generator fix. The timedemo fixture needs migration to the current hero format or a compatibility loader so the gameplay replay test runs again.

### Parallel CTest

`ctest ... --output-on-failure -j 8`

Result:

- 570/572 passed;
- the same two tests failed;
- elapsed: 6.89 seconds;
- no parallel-only save, preference-path, or fixture race appeared.

### Oracool shuffled stress

`oracool_audit_test.exe --gtest_shuffle --gtest_random_seed=92531 --gtest_repeat=100 --gtest_break_on_failure --gtest_brief=1`

Result:

- 181 tests per iteration;
- 100 iterations;
- 18,100/18,100 passed.

The repeated missing-audio and intentional corrupt-inventory diagnostics are expected harness output; no assertion failed.

### Store shuffled stress

`stores_test.exe --gtest_shuffle --gtest_random_seed=37473 --gtest_repeat=100 --gtest_break_on_failure --gtest_brief=1`

Result:

- 17 tests per iteration;
- 100 iterations;
- 1,700/1,700 passed.

This specifically supports the paid-service cursor fix and the repaired fixture isolation. It does not cover the Premium/Supplies materialization or exact held-stack sale cases in Findings 1-3.

## Release readiness checks

The current Release tree is stale by construction, not a valid v1.9.97 candidate:

- `build\x64-Release\Source\config.h` says v1.9.92;
- `build\x64-Release\DiabloOrcl.exe` has no ProductVersion resource;
- its executable timestamp is older than the v1.9.93-v1.9.97 changes.

Running `tools\BuildReleasePackage.ps1 -BuildDir build\x64-Release ...` correctly stopped before staging with:

`DiabloOrcl.exe carries no version resource. It predates the VERSIONINFO stamp.`

That failure is a successful guard, not a defect in the new version check. Rebuild Release only after the code findings are resolved, then rerun the complete archive/content checks proposed above.

## Standing QA and hardening observations

### Sanitizer coverage remains absent

`CMake/platforms/windows.cmake:1-2` still forces `ASAN OFF` and `UBSAN OFF`. The successful Debug tests therefore provide no AddressSanitizer or UndefinedBehaviorSanitizer coverage. Add at least one supported clang-cl/MSVC ASan CI build, especially for the large inventory/shop/serialization surface.

### The hidden-outline move is source-correct but lacks a visual regression

`Source/engine/render/scrollrt.cpp:1335-1356` now drains deferred outlines after scene composition and before `ZoomScale()`. The order is correct for the reported ghost at zoom >1.0 and also keeps outlines below automap/mini-map overlays. No automated pixel test exercises zoom factors 1.0, 1.1, 1.5, and 2.0 with wall occlusion. Add a deterministic render capture or golden-image test so a future queue move cannot restore the double outline.

### The suspected single-player broken-item load resurrection is not a bug

Do not change `ClampDurability()` based only on the packed byte path. `Source/loadsave.cpp:1188-1207` directly assigns the authoritative full `heroItem` record in single-player, preserving `_iDurability == 0` and `_iOracoolBroken == true`. The byte-clamped reconstruction is not the final single-player item. A save/load regression test for broken worn, backpack, stash, and extra-tab items is still worthwhile, but this audit did not establish state resurrection in the supported mode.

### Debug and Release artifacts must not be mixed in evidence

Debug is current v1.9.97; Release is stale v1.9.92. Any screenshot, timedemo, package, or manual play result should record the executable ProductVersion and archive manifest digest so artifacts from the two trees cannot be accidentally combined.

## Recommended patch order for Claude

### Patch 1 - make shop stock authoritative

Fix Premium trim/recount/refill and Refresh Until first, then materialize the protected Pepin + Adria combined shelf. Add the invariant tests before changing generator counts.

### Patch 2 - make sale settlement exact and transactional

Pass the already computed exact sale price into the fit gate, require a zero remainder, and preserve the item/buyback state on any settlement failure.

### Patch 3 - finish MPQ transaction and verification work

Give every packer invocation an exclusively owned temporary, clean response lists on all exits, and add full entry-content verification callable by packaging.

### Patch 4 - make zip replacement genuinely atomic

Verify the completed partial zip, then use an actual same-volume replace primitive without deleting the old final first.

### Patch 5 - repair generated documentation integrity

Regenerate for v1.9.97, make the verifier report all invariants, and gate clean-checkout/published-artifact hashes.

### Patch 6 - restore a fully meaningful test gate

Resolve or explicitly rebaseline the L5 golden, migrate the timedemo hero fixture, add the new shop/sale/packer tests, add a render golden, and introduce sanitizer coverage.

## Final acceptance checklist

- [ ] Premium purchase never resurrects a trim-cleared entry.
- [ ] Refresh Until can only report a target with a visible `PlacedSlot`.
- [ ] Supplies has no hidden Adria reserve after the protected Pepin prefix is placed.
- [ ] A held 99-unit stack at stash cap is either paid completely or not sold.
- [ ] Every `CreditSaleProceeds()` success path proves a zero remainder.
- [ ] Two same-output packers cannot share/delete a temporary or publish mixed contents.
- [ ] Packaging reads and hashes every expected entry in both MPQs.
- [ ] Touching an unchanged source file does not invalidate a content-identical archive.
- [ ] Final zip replacement preserves the prior zip on every injected failure.
- [ ] `BundleWiki.ps1 -Verify` passes from a clean checkout and reports v1.9.97.
- [ ] The published wiki artifact hash matches the verified local bundle.
- [ ] CTest is 572/572, including the dungeon golden and timedemo.
- [ ] New shop, sale, archive, and render regressions pass in shuffled/repeated runs.
- [ ] A supported sanitizer job passes.
- [ ] A clean Release rebuild reports ProductVersion 1.9.97 and passes the hardened package gate.

## Bottom line

The six follow-up commits materially improved the build and fixed the highest-risk prior cursor/MPQ failures. Debug v1.9.97 builds coherently and the focused stress suites are stable. The remaining gameplay risk is concentrated in backing-state versus visible-state mismatches in Premium/Supplies and an exact-price mismatch for held stack sales. The remaining release risk is that timestamps and name-table lookups are still being treated as proof of archive contents, while the checked-in wiki already demonstrates that generation/integrity gates are not yet closing the loop.
