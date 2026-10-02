# Diablo Orcl v1.9.92 - Follow-up Deep Audit

Date: 2026-08-27  
Audited workspace: `C:\Users\hroga\OneDrive\2. Personal Files\Software\Diablo\Diablo Orcl V1`  
Primary audited build: `build\x64-Debug`  
Comparison build: `build\x64-Release`  
Final audited commit: `ced2886e736bfa262c36a3de8852a0cfbfb28c72`  
Commit subject: `Audit v1.9.88 findings 3-9 (v1.9.92)`  
Source/generated version: `1.9.92`  
Previous report: `chatgpt audits\CLAUDE_DEEP_AUDIT_v1.9.88.md`

## Purpose and scope

This is a coding-agent handoff for the audit requested after the v1.9.92 follow-up work. It rechecks every numbered finding from the v1.9.88 audit, rebuilds and stress-tests the current Debug tree, follows the new shop mechanics through the SDL event loop, and audits the custom MPQ/release pipeline.

No production source was edited during this audit. The Debug executable and generated configuration were rebuilt to prove the source state; the stale Debug `oracool.mpq` was deliberately not repacked so the freshness defect and its evidence were preserved. The only new source-tree artifact from this pass is this report.

Severity used below:

- **P1**: release-blocking or capable of corrupting/losing meaningful player state.
- **P2**: important correctness, reproducibility, or test-integrity problem.
- **P3**: lower-risk hardening, documentation, or concurrency problem.

## Executive summary

The C++ build itself is current and healthy enough to run: `ORACOOL_VERSION`, the generated Debug `config.h`, the rebuilt Debug executable, and the Release executable all identify v1.9.92. Serial and parallel CTest runs both completed 565 of 567 tests successfully. The same two longstanding baseline tests failed in both modes; no new ordinary CTest regression appeared.

The audit nevertheless found ten actionable issues:

1. **P1 gameplay:** closing a shop and clicking an item before the next logic tick can route a stale paid Repair/Recharge cursor into the destructive vanilla skill.
2. **P1 release integrity:** the custom MPQ packer can publish a partial archive on a mid-pack failure and cannot report final publish failure through its exit code.
3. **P2 build freshness:** the current Debug executable is v1.9.92 but its `oracool.mpq` predates a current custom asset, so Debug playtesting does not represent HEAD.
4. **P2 economy/data loss:** the direct held-item and right-click sale paths can remove an item even when all sale proceeds cannot be stored.
5. **P2 shop correctness:** invisible one-page overflow is no longer directly purchasable, but it can appear after another item is bought without a vendor refresh.
6. **P2 release reproducibility:** `devilutionx.mpq` recursively packs a reused build-assets directory that CMake never prunes, allowing deleted assets to return as ghosts.
7. **P2 QA:** the class-tree helpers do not reset the whole player; the audit binary fails when its tests are shuffled as one process.
8. **P3 versioning:** generated wiki data still declares v1.9.91 while the source and executables are v1.9.92.
9. **P3 build concurrency:** release staging and both MPQ response files use shared fixed temporary paths.
10. **P3 validation:** packaging accepts an executable if the expected version appears anywhere in its ASCII strings, rather than checking an authoritative version field.

The first audit’s fixes are substantial: the full-stock out-of-bounds access is fixed; HUD inset reload, UTF-8 vertical labels, refresh-prompt teardown, parallel save-test isolation, and store-test state isolation are fixed. The service cursor, one-page stock, and sale-settlement findings are only partially closed and have narrower residual cases documented below.

Recommended implementation order:

1. Close the service-cursor event race and make every sale gesture use one transactional settlement function.
2. Make MPQ creation explicitly commit/abort, preserve the previous good archive until validation, and verify archive contents in packaging.
3. Make the one-page shelf a stable authoritative stock set.
4. Make asset staging clean/manifest-driven and wire custom assets into the build graph.
5. Repair the class-tree fixture and add whole-binary shuffled stress to CI.
6. Regenerate/version-check the wiki, use unique temporary paths, and strengthen executable identity.

---

## Disposition of the v1.9.88 numbered findings

| Prior finding | v1.9.92 disposition | Current conclusion |
|---|---|---|
| 1. Full vendor array out-of-bounds | **Fixed** | `RemoveFromVendorStock(Item *, capacity, idx)` performs a capacity-bounded move and clears the tail. The full-array regression passes. |
| 2. Repair/Recharge cursor lifetime | **Partially fixed** | Walkaway and the next logic tick disarm correctly, and paid predicates require a live shop. A same-event-batch close/click race still falls into vanilla Repair/Recharge. See Finding 1. |
| 3. Invisible one-page stock purchase | **Partially fixed** | Navigation and activation now use placed entries only. Raw overflow remains and can become visible after a purchase without refresh. See Finding 5. |
| 4. HUD `cellInsets` hot-reload cache | **Fixed** | Reset clears cached insets and size mismatch forces reconstruction. |
| 5. UTF-8 vertical labels | **Fixed for code points** | Rendering decodes UTF-8 rather than slicing bytes. Full grapheme-cluster shaping remains outside the current implementation but no original byte-splitting bug remains. |
| 6. Refresh Until walkaway teardown | **Fixed** | Walkaway calls `CloseRefreshUntilPrompt()`, which performs text-input teardown rather than clearing only the visible flag. |
| 7. Sale proceeds near stash cap | **Partially fixed** | Stash headroom and backpack room are combined for text-list sales. New direct sale gestures bypass the same preflight. See Finding 4. |
| 8. Parallel save-file collision | **Fixed in observed runs** | Serial and `-j 8` runs now have identical outcomes; no parallel-only save failure occurred. |
| 9. Store test state leakage | **Fixed in the store suite** | 750 shuffled/repeated store tests passed. A separate class-tree fixture leak was found. See Finding 7. |

---

## Finding 1 - P1: stale paid service cursor can execute the destructive vanilla skill

### Evidence

The v1.9.92 fix correctly distinguishes raw state from authority:

- `Source/stores.cpp:4619-4622` says a paid service cursor is live only while the matching flag is armed **and** a shop grid screen is open.
- `Source/stores.cpp:4624-4629` intentionally exposes the raw armed flag for cleanup.
- `Source/stores.cpp:4642-4657` clears both the flag and `pcurs`.
- `Source/stores.cpp:3552-3566` reconciles stale state once per game-logic tick.
- The walkaway path disarms synchronously at `Source/stores.cpp:3589-3603`.

Several other closures do not disarm synchronously:

- The grid X sets only `stextflag = TalkID::None` at `Source/oracool/shop_grid.cpp:804-808`.
- `StoreESC` changes to another store screen or `None` at `Source/stores.cpp:5011-5098` without first clearing the service cursor.
- Renderer overlap closure sets only `stextflag = TalkID::None` at `Source/stores.cpp:4939-4948`.

That would be harmless if cleanup always ran before another input event, but it does not:

- `Source/diablo.cpp:1422-1442` drains the entire queued SDL message batch before deciding whether to run game logic.
- `UpdateStoreState()` is reached later from `Source/diablo.cpp:2042-2046`.
- When `runGameLoop` is false, `Source/diablo.cpp:1454-1461` may process input/render and continue without that cleanup.

The target click then reaches `TryIconCurs`:

- For `CURSOR_REPAIR`, `Source/diablo.cpp:3222-3238` uses paid service only if `IsShopRepairCursorArmed()` is live.
- Once the shop screen has closed, that predicate is false even though the raw flag and repair cursor remain.
- Execution then falls directly into `DoRepair`/`RepairItem` at `Source/diablo.cpp:3239-3248`.
- Recharge has the same shape at `Source/diablo.cpp:3251-3277`.

The live predicate successfully prevents charging outside the shop, but it changes the meaning of the still-visible cursor from “paid full repair/recharge” to “use the class skill.” Vanilla Repair/Recharge can permanently reduce maximum durability or maximum charges. This is exactly the destructive fallback the cleanup code comments intend to prevent.

### Simulated user sequence

1. Open Griswold’s shop and arm paid Repair, or open Adria’s shop and arm paid Recharge.
2. Click the shop X or press Escape.
3. Before a game-logic tick runs, click an eligible inventory/tab/stash item. Two already-queued SDL events are sufficient; no thread race is required.
4. The close event makes the paid predicate false but leaves `ShopArmedServiceCursor` and `pcurs` stale.
5. The item event reaches the vanilla fallback and modifies the item with the class skill.

The same exposure exists when a shop is closed during rendering because another left panel/runeword book overlaps it: the following outer-loop input batch can run before game logic.

### Impact

- Permanent maximum-durability or maximum-charge loss on the clicked item.
- The action differs from the service the player selected.
- No paid-service price or shop UI explains what happened.
- A rapid click, double-click, controller event burst, or queued input can trigger it.

### Required measure

Use both immediate and defense-in-depth protection:

1. In `TryIconCurs`, before either vanilla fallback, check the raw `IsAnyShopServiceCursorArmed()` state. If raw shop state exists but the paid predicate is no longer live, call `DisarmShopServiceCursor()` and consume the click. Never reinterpret that stale cursor as a class skill.
2. Centralize store-screen transitions/closure in a helper that tears down service cursor, refresh prompt, hover/selection state, and other shop-owned state synchronously.
3. Keep `UpdateStoreState` as a safety net, not the first point at which correctness is restored.
4. Audit all direct assignments to `stextflag` and all calls that replace a store screen.

### Required regression tests

- Arm paid Repair, set the screen to `None`, invoke the next item click without calling `UpdateStoreState`, and assert:
  - maximum durability and current durability are unchanged;
  - gold is unchanged;
  - raw service state is clear;
  - cursor is `CURSOR_HAND`.
- Repeat for Recharge and maximum charges.
- Parameterize X, Escape, overlap closure, vendor-screen transition, walkaway, portal/level transition, and initialization.
- Prove legitimate vanilla Repair/Recharge still runs when no raw shop-service flag exists.
- Add an event-order integration test with “close event then target event” in one fetched batch.

---

## Finding 2 - P1: the MPQ packer can publish partial output and hides finalization failure

### Evidence

`tools/oracool_mpq_pack.cpp` uses `MpqWriter` as though scope exit were a transaction boundary:

- It deletes the final output first at `tools/oracool_mpq_pack.cpp:71-75`.
- It then writes entries in a loop.
- Missing files, short reads, invalid paths, or `WriteFile` failures return `1` immediately at `tools/oracool_mpq_pack.cpp:85-116`.
- On the nominal path it prints success and returns `0` at `tools/oracool_mpq_pack.cpp:123-124` without explicitly finishing or publishing.

`MpqWriter` has different semantics:

- `MpqWriter::~MpqWriter` automatically calls `Publish()` when `Finish()` was not called explicitly (`Source/mpq/mpq_writer.cpp:419-442`).
- `Publish()` finalizes and atomically replaces its target (`Source/mpq/mpq_writer.cpp:379-407`).
- Therefore an early `return 1` after one or more successful writes still lets the destructor finalize and publish those already-written entries as a valid but partial archive.
- On the success path, `return 0` is decided before the destructor calls `Publish()`. A final table-write or atomic-replace failure cannot change the process exit code. The wrapper can announce success even though publication failed.

There is a third edge: `RemoveFile(outPath)` is not checked. If deletion fails and the target still exists, `MpqWriter` stages a copy and edits it. Entries that should have been removed can survive, defeating the “always start from scratch” comment.

The release script does not detect archive completeness:

- `tools/BuildReleasePackage.ps1:114-156` checks required file existence and timestamps.
- `tools/BuildReleasePackage.ps1:199-215` counts top-level staged files, not entries inside either MPQ.
- The header claims `assets` are “required and counted” (`tools/BuildReleasePackage.ps1:15-20`), but no MPQ is opened, listed, hashed, or compared with a manifest.

### Failure scenario

1. A response file lists hundreds of custom assets.
2. The packer writes the first N entries.
3. Entry N+1 is missing, locked, unreadable, or fails a short read.
4. `main` returns `1`, but stack unwinding runs the writer destructor.
5. The destructor publishes the first N entries at the final `oracool.mpq` path. The prior known-good archive was already deleted.
6. The wrapper correctly reports that this invocation failed, but a later packaging invocation sees an existing, fresh archive and can accept it.

This conclusion is a direct code-path proof. Destructive failure injection against the user’s real archive was intentionally not performed.

### Impact

- A release can contain an incomplete but structurally valid custom asset archive.
- A missing UI/sound/object asset may fail only at the mechanic that requests it.
- The previous good archive is destroyed before the replacement proves it is complete.
- A “successful” pack can still fail during destructor finalization with exit code zero.

### Required measure

1. Build into a unique temporary destination and leave the current final archive untouched.
2. On every entry failure, explicitly abort/discard the writer’s shadow; do not rely on destructor publication.
3. On success, explicitly call and check `Finish()`/`Publish()`. A publication failure must produce a nonzero process exit.
4. Validate the completed temporary archive against the exact response-file manifest:
   - every expected normalized path exists;
   - no unexpected path exists;
   - entry count matches;
   - preferably compare sizes or hashes.
5. Only after validation atomically replace the known-good final archive.
6. Make `BuildReleasePackage.ps1` independently open/list both `oracool.mpq` and `devilutionx.mpq` and compare them with generated manifests. Timestamp is an optimization, not integrity proof.

### Required regression tests

- Inject a missing file after at least one successful entry and assert the old final archive is byte-identical.
- Inject a read failure, `WriteFile` failure, final table-write failure, and atomic-replace failure; each must return nonzero.
- Verify no partial final archive and no stale temp file remains after each failure.
- Remove an entry from the manifest and prove the next successful archive does not retain it.
- Corrupt or truncate a fresh archive while preserving its timestamp and prove packaging rejects it.

---

## Finding 3 - P2/build: the current Debug runtime has a stale custom asset archive

### Evidence

Current artifact timestamps and versions:

| Artifact | Size | UTC modification time |
|---|---:|---|
| `build\x64-Debug\DiabloOrcl.exe` | 13,221,376 bytes | 2026-08-27 15:24:01 |
| `build\x64-Debug\oracool.mpq` | 35,729,453 bytes | 2026-08-27 05:24:47 |
| `Packaging\resources\oracool_assets\ui\barb_tree_icons.png` | 142,358 bytes | 2026-08-27 06:11:54 |
| `build\x64-Release\DiabloOrcl.exe` | 4,427,776 bytes | 2026-08-27 12:47:59 |
| `build\x64-Release\oracool.mpq` | 35,729,453 bytes | 2026-08-27 12:48:18 |

The root version and both generated configurations say v1.9.92. The Debug executable was rebuilt successfully, but its custom archive is older than at least one source asset. The Release archive is newer, so this is specifically a stale Debug-runtime problem rather than evidence that the current Release archive is stale.

The root cause remains structural: `oracool_mpq_pack` is not in the ordinary game build graph. C++ can rebuild cleanly while custom assets remain whatever a prior manual command produced.

### Impact

- Debug playtesting can report false visual/audio regressions or miss real ones.
- Code and assets in the same runtime directory represent different revisions.
- A test result described as “v1.9.92 Debug” is not fully v1.9.92 until the archive is repacked.

### Immediate recovery

From a configured developer shell:

`tools\build_oracool_mpq.cmd build\x64-Debug`

Then verify that the archive is newer than every manifest input and that its entry manifest matches the source tree.

### Structural measure

- Generate an explicit custom-asset manifest in CMake.
- Make an MPQ custom command depend on every input plus the packer.
- Make the runnable game/package target depend on the archive.
- Add a pre-launch/pre-package freshness and content check.
- Keep source, staging, and finished archives in separate directories so a reused runtime folder cannot masquerade as a reproducible build.

---

## Finding 4 - P2: direct sale gestures can remove an item before all proceeds fit

### Evidence

The v1.9.92 settlement helper is improved:

- `CreditSaleProceeds` fills stash headroom first, then calls `AddGoldToInventory` (`Source/stores.cpp:1562-1603`).
- `StoreGoldFit` counts the sold item’s freed cells, existing backpack gold room, and single-player stash headroom (`Source/stores.cpp:2509-2533`).
- Text-list sale routes call that fit gate before removal.

The newer direct gestures do not share that transaction:

- `ShopSellHeldItem` explicitly skips the room check at `Source/stores.cpp:4481-4483`, records/credits the sale, and clears `HoldItem` unconditionally at `Source/stores.cpp:4484-4490`.
- `ShopSellInventoryItem` removes the active inventory item at `Source/stores.cpp:4553`, then records and credits it at lines 4554-4555, with no `StoreGoldFit` call.
- `CreditSaleProceeds` has no way to fail the sale or restore the item. If `AddGoldToInventory` returns a remainder, it recalculates carried gold and only logs “gold lost” at `Source/stores.cpp:1595-1602`.

The comment saying single-player stash has “no grid to fill” is true but incomplete: `Stash.gold` is capped at `INT_MAX`. Once that pool has no headroom, the remaining proceeds must fit in finite inventory gold stacks/cells.

### Simulated trigger

1. Set `Stash.gold` to `INT_MAX` or close enough that the sale exceeds remaining headroom.
2. Fill the backpack so its existing stacks and free cells cannot hold the remainder.
3. Sell a sufficiently valuable item by:
   - dropping/holding it over the shop’s direct sale path, or
   - right-clicking it in the inventory grid.
4. The item is cleared/removed and added to buyback.
5. Only the gold that physically fits is retained; the remainder is logged as lost.

This is an edge case, but it is a real transaction-integrity failure. Buyback does not make the original transaction safe: recovering the item requires paying for it again and may be impossible in the same saturated state.

### Required measure

Create one transactional sale function used by text-list, held-item, and right-click gestures:

1. Resolve and copy the pristine item.
2. Compute price.
3. Compute exact destination capacity while accounting for cells that the sale will free.
4. If all proceeds cannot be placed, leave item, gold, buyback, cursor, and autosave state unchanged and show `NoRoom`/a specific message.
5. Otherwise settle gold, verify zero remainder, then remove/clear the item and record buyback.

As an additional invariant, make `CreditSaleProceeds` return the unplaced amount or a success result. A function that can lose money must not have a `void` interface that callers cannot check.

### Required regression tests

- Held-item sale with stash at cap and no backpack gold capacity: sale refused and all state byte-equivalent.
- Right-click sale under the same state.
- Partial stash headroom plus enough backpack room: exact full price credited.
- Partial stash headroom plus insufficient backpack room: no partial transaction.
- Text-list, held, and right-click paths produce identical post-state for the same item and capacity.
- Buyback and autosave change only after a successful settlement.

---

## Finding 5 - P2: hidden one-page overflow can appear without a vendor refresh

### Evidence

The direct invisible-purchase defect is fixed:

- `PlaceStock` computes one row-major first-fit page (`Source/oracool/shop_grid.cpp:152-205`).
- Drawing snaps stale selection to a placed item (`Source/oracool/shop_grid.cpp:750-767`).
- Keyboard/controller movement walks only `PlacedSlot` entries (`Source/oracool/shop_grid.cpp:913-947`).
- Activation refuses a selection that is not on the shelf (`Source/oracool/shop_grid.cpp:1002-1017`).

However, `PlaceStock` is only a view over raw stock:

- It skips items that do not fit but never removes them from the Smith/Witch/Healer arrays.
- Its comments define overflow as “never on the shelf” (`Source/oracool/shop_grid.cpp:153-170`).
- A purchase removes and compacts the raw array through `RemoveFromVendorStock` (`Source/stores.cpp:3413-3419`; Smith/Witch/Healer callers at lines 2230, 2800, and 3023).
- The next draw recomputes `PlaceStock` from that compacted raw array.

Once a visible item is bought, its cells become available. An item that was previously skipped can now fit and appear. No vendor refresh, restock price, time passage, or explicit page change occurred.

### Impact

- The UI says stock is one page, but the underlying vendor inventory can contain a hidden reserve.
- Buying one item can reveal an unadvertised item immediately.
- Shelf contents are not stable for the lifetime of the vendor refresh.
- Layout and purchase order affect which generated items ever become visible.

### Required measure

Choose one authority and persist it:

- **Preferred:** when stock is generated/refreshed, run placement once and physically retain only the placed entries in the vendor array. Hidden overflow is discarded before the player can interact.
- Or store a stable visible-index/stock list for the refresh epoch and remove from that list without admitting previously rejected entries.

Do not recompute “what the vendor carries” from an unchanged oversized backing array after every transaction.

### Required regression tests

- Generate a mixed-size stock whose raw entries exceed one page.
- Capture the visible stock IDs.
- Buy each possible visible item in separate cases.
- Assert the remaining visible IDs equal the original visible set minus the purchased ID; no previously hidden ID appears.
- Repeat for Basic, Premium/Unique/Rare/Set/Supplies tabs and Smith/Witch/Healer where applicable.
- Refresh explicitly and prove a new set is then allowed.

---

## Finding 6 - P2/build: deleted engine assets can return from a reused build tree

### Evidence

The engine-asset pipeline has two conflicting authorities:

- `CMake/Assets.cmake:261-272` copies every current `devilutionx_assets` manifest entry into `build\...\assets`.
- It does not remove files that were copied by an older configuration but later deleted from the manifest/source.
- `tools/build_devilutionx_mpq.cmd:57-70` recursively enumerates **every** file currently present under that reused build-assets directory and sends it to the packer.

Therefore an asset removed from `devilutionx_assets` remains on disk and is reintroduced into the next archive. The script comments correctly say the build tree is the shipping authority, but a directory that is only incrementally populated is not a clean authority.

This problem is currently latent, not an assertion that the present archive has extra files. This audit compared current source/build engine-asset relative paths:

- expected manifest/deployed files: 188;
- build-assets files: 188;
- set difference: empty.

The current Debug `devilutionx.mpq` is also newer than the newest deployed engine asset. The pipeline is clean **today**, but it is not hermetic.

### Impact

- Deleted or intentionally unshipped assets can silently remain in releases.
- Clean and incremental builds can produce different archives from the same commit.
- Manual cleanup becomes a hidden release prerequisite.

### Required measure

- Recreate a dedicated staging directory from the exact CMake manifest for every pack.
- Or feed the exact generated manifest directly to the packer instead of recursively walking `build\...\assets`.
- Reject unexpected files in staging.
- Build the archive in CI from a fresh tree and compare its manifest/hash with an incremental-tree build.

### Required regression test

Place a sentinel file in a temporary reused staging tree that is not in the manifest. The generated archive must exclude it or the build must fail loudly. Then remove a real manifest entry between two test builds and prove it disappears from the second archive.

---

## Finding 7 - P2/QA: class-tree test helpers leak prior Player state

### Evidence

`FreshHero` and `FreshPaladin` at `test/oracool_audit_test.cpp:1887-1913` resize `Players` and set only:

- class;
- level;
- unspent points;
- active-aura byte;
- skill/class-tree investment arrays.

They do not reset the complete `Player` object, `_pmode`, current/max health, inventory, passive slots, or other state touched by earlier tests.

This matters because `GetActiveClassAura` deliberately suppresses an aura for:

- `_pmode == PM_DEATH`; or
- positive max HP with nonpositive current HP

at `Source/oracool/class_tree.cpp:1333-1358`.

Observed stress result:

`oracool_audit_test.exe --gtest_shuffle --gtest_random_seed=92531 --gtest_repeat=20 --gtest_break_on_failure`

failed in iteration 1:

- test: `OracoolClassTree.AuraNeedsAPointBeforeItCanBurn`;
- assertion: `test/oracool_audit_test.cpp:2576`;
- expected active aura `Might`;
- actual `None`.

The entire binary passes 178/178 in registration order. Running only `OracoolClassTree.*` with the same seed also passes because the selected-suite shuffle order differs. This is strong evidence that a preceding test outside the suite leaves player state that `FreshPaladin` does not clear.

CTest normally masks this because GoogleTest discovery registers each `TEST` as a separate process/filter. The whole executable is a second, necessary isolation check.

### Impact

- Tests can pass or fail based on registration/filter order.
- New preceding tests can create “random” class-tree failures.
- A test intended to prove aura semantics may actually prove residue from another mechanic.
- CTest’s process isolation hides fixture defects that matter when helpers are reused inside one process or repeated runs.

This is a test-integrity finding, not proof of a gameplay aura bug.

### Required measure

- Replace both helpers with one true fixture that resets/constructs a complete `Player` and every relevant global.
- Initialize a valid living baseline explicitly: non-death mode, positive current/base/max health, clean held/inventory/passive state, clean aura audio/effect state.
- Prefer per-test setup/teardown over ad hoc global helpers.
- Document any fields intentionally preserved.

### Required regression/CI behavior

Run both:

1. ordinary discovered CTest in serial and parallel; and
2. each major GoogleTest executable as a whole process with `--gtest_shuffle --gtest_repeat` under several fixed recorded seeds.

Seed 92531 must pass after the fixture repair. Also add an explicit predecessor test that leaves a dead player, then creates a fresh Paladin and asserts it is alive and can activate Might.

---

## Finding 8 - P3: generated wiki version is one release behind

### Evidence

- `ORACOOL_VERSION`: `1.9.92`.
- Debug generated `Source/config.h`: `ORACOOL_VERSION "1.9.92"`.
- Release generated `Source/config.h`: `ORACOOL_VERSION "1.9.92"`.
- `wiki/data.js:1` begins with `"version":"1.9.91"` and `"generated":"2026-08-27 10:22"`.

The v1.9.92 version bump and wiki generation happened in an order that allowed generated documentation to retain the previous version. Existing fingerprint checks can prove bundle/data consistency without proving equality to the repository release version.

### Impact

- In-game/offline documentation can identify the wrong release.
- Release artifacts have inconsistent provenance.
- A valid content fingerprint may create false confidence because it answers a different question.

### Required measure

- Bump `ORACOOL_VERSION` before generating all versioned artifacts.
- Regenerate `wiki/data.js` and `wiki/oracool-wiki-bundle.html` for v1.9.92.
- In CI and release packaging, parse the generated JSON object and assert its version exactly equals `ORACOOL_VERSION`.
- Include generated-artifact freshness in the release checklist.

---

## Finding 9 - P3/build: fixed temporary paths make concurrent builds interfere

### Evidence

- `tools/BuildReleasePackage.ps1:160-163` uses fixed `%TEMP%\oracool-package-$version` and recursively deletes it before staging.
- `tools/build_devilutionx_mpq.cmd:59-70` uses fixed `%TEMP%\devilutionx_mpq_files.txt`.
- `tools/build_oracool_mpq.cmd:47-58` uses fixed `%TEMP%\oracool_mpq_files.txt`.
- Both command files exit immediately on packer failure, before deleting the response file.
- The release script removes/replaces the versioned output zip directly (`tools/BuildReleasePackage.ps1:219-225`).

Two jobs on the same machine, or a user invocation overlapping CI/another shell, can delete or rewrite each other’s lists/staging directories. Different build directories still share the same response-list filename.

### Impact

- A packer can consume a list being truncated/replaced by another invocation.
- One packaging job can recursively delete another job’s staging tree.
- Interrupted runs leave shared stale state.
- Same-version zip creation is not atomic.

### Required measure

- Use GUID/PID-scoped temporary directories and response files.
- Place cleanup in `finally`/a guaranteed command-file cleanup path.
- Write the zip to a unique temporary name, validate it, then atomically replace the requested output.
- Include build configuration and a unique run ID in logs.

### Required regression test

Launch two packaging/packing jobs for the same version against separate temporary build trees. Both must either succeed independently with the correct manifest or fail for their own injected reason; neither may alter the other’s inputs, output, or cleanup path.

---

## Finding 10 - P3/release validation: executable version check is non-authoritative

### Evidence

`tools/BuildReleasePackage.ps1:135-141` scans all ASCII strings in `DiabloOrcl.exe` for anything matching `\d+\.\d+\.\d+` and accepts the binary if the requested release version appears anywhere.

This proves only that those bytes occur somewhere in the executable. A stale binary could contain the expected version in a changelog string, resource path, logging message, or dead data while its actual product/version behavior remains old.

The project’s command-line identity is not yet a clean substitute:

- `Source/diablo.cpp:1588` prints `PROJECT_VERSION` for `--version`, which is the DevilutionX engine version rather than `ORACOOL_VERSION`.
- `Packaging/windows/devilutionx.rc` still has no authoritative Oracool `VERSIONINFO` resource.

### Impact

- Packaging can accept a stale or inconsistently stamped executable.
- Windows file properties and command-line output do not give automation one exact source of truth.

### Required measure

- Make `DiabloOrcl.exe --version` emit an easily parsed Oracool version plus the engine base.
- Add Windows `VERSIONINFO` fields sourced from `ORACOOL_VERSION`.
- Have packaging execute/capture the version helper or read the exact version resource; require equality, not substring presence.
- Add a negative test where the expected version exists in an unrelated string but the authoritative version differs.

---

## Exact build and test results

### Build

The first direct Ninja invocation from the ordinary shell failed to find standard MSVC headers because it was not running in a Visual Studio developer environment. This was an environment setup failure, not a source regression.

After entering `VsDevCmd.bat`, the Debug build completed successfully. A subsequent incremental build reported:

`ninja: no work to do.`

Verified Debug outputs:

- `build\x64-Debug\Source\config.h` says v1.9.92.
- `build\x64-Debug\DiabloOrcl.exe` was rebuilt at 13,221,376 bytes.
- `build\x64-Debug\oracool.mpq` remains stale as documented in Finding 3.

### Serial CTest

Command shape:

`ctest --test-dir build\x64-Debug --output-on-failure -j 1`

Result:

- total: 567;
- passed: 565;
- failed: 2;
- elapsed: 25.84 seconds.

Failures:

1. `Drlg_l1.CreateL5Dungeon_diablo_3_844660068`
   - golden tile mismatch, including actual tile 22 versus expected 4 at 1x0;
   - ViewPosition mismatches including `(67,72)` vs `(67,52)` and `(73,83)` vs `(85,45)`.
2. `Timedemo.WarriorLevel1to2`
   - `Unable to load character`.

Both are standing baseline failures from earlier audits.

### Parallel CTest

Command shape:

`ctest --test-dir build\x64-Debug --output-on-failure -j 8`

Result:

- total: 567;
- passed: 565;
- failed: the exact same two tests;
- elapsed: 6.54 seconds.

No parallel-only save/pref-path failure occurred. This validates the observed fix for prior Finding 8.

### Store order stress

Command:

`stores_test.exe --gtest_shuffle --gtest_random_seed=37473 --gtest_repeat=50`

Result:

- 15 tests per iteration;
- 50 iterations;
- 750/750 passed.

This validates the observed store fixture cleanup for prior Finding 9.

### Oracool audit executable

Normal whole-binary registration order:

- 178/178 passed.

Shuffled/repeated whole binary:

`oracool_audit_test.exe --gtest_shuffle --gtest_random_seed=92531 --gtest_repeat=20 --gtest_break_on_failure`

Result:

- failed in iteration 1 at `OracoolClassTree.AuraNeedsAPointBeforeItCanBurn`;
- `GetActiveClassAura(player)` returned `None` rather than `Might`.

This is Finding 7.

---

## Standing lower-priority observations from the previous report

These areas were not the main target of the v1.9.92 fix commits and remain relevant:

### Sanitizer configuration still misrepresents coverage

`CMake/platforms/windows.cmake:1-2` still forces `ASAN OFF` and `UBSAN OFF`. A cache or option presentation that suggests debug sanitizer coverage is therefore misleading. Add a real clang-cl/MSVC ASan CI configuration or report the features as unsupported/disabled. Do not rely on this Debug build to detect out-of-bounds behavior.

### Runtime/version identity remains inconsistent

The main menu uses `ORACOOL_VERSION`, while `--version` still emits the engine’s `PROJECT_VERSION`. Windows `VERSIONINFO` is absent. Finding 10 should solve this rather than adding more binary string scanning.

### Debug output remains non-hermetic

The Debug root mixes runtime files, test binaries/libraries, logs, saves, configuration, and manually produced MPQs. The new release script’s explicit allow-list substantially reduces the chance that these are shipped, which is good, but it does not make the shared Debug tree reproducible. Keep package staging separate and build releases from a clean tree.

### CMake’s DLL install glob remains broad

`CMakeLists.txt:559-567` still globs DLLs from a directory. `tools/BuildReleasePackage.ps1` now copies an explicit runtime allow-list, so the custom Windows zip avoids that glob. Any CMake install/CPack path that remains supported should use dependency discovery or an explicit allow-list too.

### Debug Control Flow Guard was not re-certified

The previous Debug audit could prove ASLR, high-entropy VA, DEP/NX, and stack cookies, but not effective CFG coverage. This pass did not repeat PE load-configuration analysis. Validate the clean Release artifact separately before documenting CFG as enabled.

---

## Investigated paths that are not current bugs

These checks are recorded to prevent duplicate or misleading reports:

- **Full vendor stock removal:** the old fixed-array out-of-bounds loop is gone; the capacity-aware helper and regression are present.
- **Invisible direct purchase:** mouse, keyboard, controller navigation, and activation now require a `PlacedSlot`. The residual issue is stock stability after purchase, not direct access to an invisible index.
- **Walkaway cleanup:** walkaway synchronously disarms service cursors and closes Refresh Until correctly. The residual cursor issue is the pre-tick X/Escape/overlap sequence.
- **Engine asset archive now:** source/deployed asset path sets are both 188 and identical; `devilutionx.mpq` is fresh. Finding 6 is about future incremental deletions.
- **Release custom archive now:** the Release `oracool.mpq` is current relative to the inspected custom assets. Finding 3 is scoped to Debug.
- **Commercial Blizzard archives in the build tree:** the Release build directory contains local game-data MPQs used for testing, but `BuildReleasePackage.ps1` stages an explicit top-level manifest and scans the result for seven forbidden archive names. Their presence in the build tree alone is not evidence that the custom package ships them.
- **Store shuffled stress:** all 750 cases passed; do not continue reporting the old store test leak without a new failing seed.
- **Typed vendor fallback:** no concrete invalid item index was established in the previously suspected typed-stock fallback. A failing seed/invariant is still required before changing it.

---

## Proposed patch sequence for Claude

### Patch 1 - prevent player-state loss

- Add stale raw shop-service cancellation/consumption in `TryIconCurs`.
- Centralize synchronous store teardown.
- Unify all sale gestures behind a preflighted transaction.
- Add queued-event service tests and stash-cap direct-sale tests.

Acceptance:

- No shop-owned cursor can reach vanilla Repair/Recharge after any close.
- No sale removes an item unless 100% of proceeds are represented in stash/backpack gold.

### Patch 2 - make shelf stock authoritative

- Materialize only placed stock at vendor refresh, or persist a stable visible-stock epoch.
- Remove purchased entries without admitting old overflow.
- Add mixed-size full-shelf tests.

Acceptance:

- Before an explicit refresh, visible stock after a purchase equals original visible stock minus purchased entries.

### Patch 3 - make MPQs transactional and verifiable

- Give the packer explicit success/abort control.
- Build and validate a unique temporary archive.
- Atomically replace only after exact manifest verification.
- Make packaging independently validate both MPQs.
- Add failure-injection tests.

Acceptance:

- Every injected failure leaves the last known-good archive byte-identical.
- A partial, stale-entry, extra-entry, corrupt, or timestamp-forged archive is rejected.

### Patch 4 - make asset builds hermetic

- Generate exact manifests from CMake.
- Recreate clean staging directories.
- Wire `oracool.mpq` into the default runnable/package dependency graph.
- Compare clean and incremental archive manifests/hashes in CI.

Acceptance:

- The same commit produces the same archive contents from clean and reused build directories.

### Patch 5 - restore test trust

- Replace `FreshHero`/`FreshPaladin` with a full fixture reset.
- Run whole GoogleTest executables shuffled/repeated as well as discovered CTest cases.
- Keep serial and parallel CTest lanes.

Acceptance:

- Seed 92531 passes for at least 20 repetitions.
- A deliberately dead predecessor cannot contaminate a fresh aura test.

### Patch 6 - release/version hardening

- Regenerate the v1.9.92 wiki after the version bump.
- Assert wiki version equals `ORACOOL_VERSION`.
- Add authoritative CLI/Windows version metadata.
- Replace fixed temp paths and make zip publication atomic.

Acceptance:

- Source, generated config, executable identity, wiki, archive manifests, README, and zip name all report the same version.
- Two concurrent same-version packaging runs cannot interfere.

---

## Final acceptance checklist

- [ ] X then immediate target click cannot invoke vanilla Repair/Recharge.
- [ ] Escape then immediate target click cannot invoke vanilla Repair/Recharge.
- [ ] Renderer/left-panel closure cannot leave a shop-owned cursor live.
- [ ] Held, right-click, and text-list sales share one all-or-nothing transaction.
- [ ] Stash-at-`INT_MAX` sale tests lose neither item nor gold.
- [ ] Buying from a full one-page shelf does not reveal old hidden overflow.
- [ ] MPQ pack failure never replaces the last good archive.
- [ ] Packer exit code reflects final `Finish`/`Publish` outcome.
- [ ] Packaging validates exact MPQ manifests, not only existence/timestamps.
- [ ] Deleted engine assets disappear from incremental archives.
- [ ] Debug `oracool.mpq` is rebuilt and content-current.
- [ ] Whole `oracool_audit_test` passes shuffled seed 92531 for 20 repetitions.
- [ ] Serial and parallel CTest outcomes remain identical.
- [ ] Wiki data and bundle identify v1.9.92.
- [ ] `DiabloOrcl.exe --version` and Windows file metadata identify the same Oracool version.
- [ ] Packaging uses unique temporary paths and atomic final output.
- [ ] The two known baseline tests are either deliberately updated/fixed or remain explicitly quarantined with owners and rationale.

## Bottom line

v1.9.92 closes most of the concrete v1.9.88 defects, and its ordinary tests are stable in parallel. It is not yet safe to treat the audit as fully closed: the service-cursor fix has a pre-tick event-order hole, direct sale gestures bypass the corrected settlement gate, and the MPQ pipeline can convert a failed pack into a fresh partial release artifact. Those three should be addressed before relying on rapid shop interactions or the current release automation.
