# Inventory, belt, tabs, and stash atomicity audit

Snapshot: Diablo Orcl v1.9.304, source `c659976d69906489c34c28c636b477f14f424bb6`  
Audit status: one confirmed P1 duplication family

## INV-01 - partial stack merges are committed before the operation can succeed

Priority: P1  
Confidence: confirmed by direct control-flow and state-conservation simulation  
Player reachability: normal single-player pickup, transfer, stash withdrawal, held-item recovery, and purchasing flows

### Root cause

`Source/inv.cpp:2077-2103` (`MergeStackableItemIntoBelt`) writes into every matching belt stack immediately when `persistItem` is true.

`Source/inv.cpp:2105-2140` (`AutoPlaceItemInBelt`) performs that merge at line 2115 before it knows whether an empty belt slot exists. If only part fits and every real belt slot is occupied, it returns `false` at line 2140 after already increasing one or more stacks.

`Source/inv.cpp:2189-2231` (`MergeStackableItemIntoInventory`) does the same across the base backpack and all nine extra tabs.

`Source/inv.cpp:2233-2292` (`AutoPlaceItemInInventory`) then has two separate defects:

1. It can return `false` after partial stacks were already mutated.
2. At line 2290 it passes the original full `item` to `AutoPlaceItemInExtraTabs`, rather than the reduced local `remainder`.

`Source/qol/stash.cpp:1217-1298` repeats the transaction problem. It mutates partial stash stacks at lines 1235-1252 and only afterwards scans for room. A full stash causes a `false` return after the partial mutations.

### Deterministic simulations

#### Inventory-to-extra-tab duplication

Initial state:

- A matching backpack stack contains 95 units; maximum is 99.
- Incoming stack contains 20.
- The base backpack has no empty cell.
- An extra tab has room for one new stack.

Execution:

1. `MergeStackableItemIntoInventory` commits 4 units into the 95-stack, producing 99.
2. Local `remainder` becomes 16.
3. Base placement fails.
4. Line 2290 sends the original 20-unit `item` to the extra tab.

Final total is 99 + 20 = 119, while the correct conserved total is 95 + 20 = 115. Four units are duplicated.

#### Failed inventory placement still duplicates

Use the same 95 + 20 state, but fill every destination cell. The merge still commits four units. The function returns `false`; its caller retains the original 20-unit source. Total ownership again rises by four.

#### Belt fallback duplicates

Put a 95-unit matching stack in one real belt slot and occupy every other real belt slot. Give `AutoPlaceItemInBelt` 20 units. It commits four, cannot place the 16 remainder, and returns `false`. Callers such as `AutoGetItem` then submit the original 20 to inventory. The four merged belt units are extra.

#### Stash transfer duplicates

Fill every stash grid cell, leaving only four units of headroom in one matching 95-stack. Ctrl-click a 20-stack into the stash. The stash becomes 99, placement returns `false`, and `TransferItemToStash` leaves the source 20-stack in place. Four units were created.

### Affected call chains

- Ground pickup: `Source/inv.cpp:3057`, especially belt then inventory at `3102-3107`.
- Backpack/equipment/belt to stash: `Source/inv.cpp:2576-2600`.
- Extra inventory tab to stash: `Source/inv.cpp:2643-2675`.
- Stash withdrawal: `Source/qol/stash.cpp:620-640`.
- Closing stash with a held item: `Source/inv.cpp:3849-3870`.
- Returning a held item before auto-save: `Source/oracool/auto_save.cpp:102-132`.
- Store placement and its probe/commit sequence: `Source/stores.cpp:676-686` and purchase callers.

The common caller contract is reasonable: remove or discard the source only when auto-placement returns true. The callee violates that contract by modifying destinations on a false return.

### Why current tests pass

- `test/inv_test.cpp:396-434` covers 40 onto 50 and 95 plus 20, but the normal backpack has room for the remainder.
- `test/inv_test.cpp:436-449` covers a belt merge, not partial merge plus a full belt.
- Extra-tab tests at `test/inv_test.cpp:990-1067` cover simple fallback, not fallback after a committed partial merge.
- `test/oracool_audit_test.cpp:9787-9838` deposits many one-unit potions into an initially empty stash. It never combines partial headroom with a full grid.
- The focused inventory/stash lane passed 20 tests in this audit; none represents the failing capacity boundary.

## Recommended repair

Treat each public auto-place call as a transaction.

Preferred shape:

1. Run a complete capacity simulation with no mutation. Include all merge headroom and the exact slot/grid needed for the remainder.
2. If the whole incoming count cannot be owned by the destination set, return false without changing anything.
3. If it can, commit merges and place exactly the calculated remainder.
4. Make the function return an unambiguous result: either a boolean with all-or-nothing semantics, or an explicit moved count that every caller must consume.

Changing line 2290 from `item` to `remainder` is necessary, but not sufficient. It fixes the successful extra-tab over-credit while leaving false-return mutations in belt, inventory, and stash.

A copy-and-swap transaction is straightforward for stash. For Player containers, a small placement plan (destination index plus count) avoids copying the entire Player and also prevents network/UI notifications until commit.

## Required regression tests

1. Backpack 95 + incoming 20, base full, extra tab empty: result must be 99 + 16, never 99 + 20.
2. Same state with every destination full: return false and compare all container counts and items to an untouched snapshot.
3. Belt 95 + incoming 20, all real belt slots occupied, inventory available: final global delta must be exactly 20.
4. Same belt state with all fallbacks full: false and no belt mutation.
5. Full stash with four units of matching headroom: false and no stash mutation.
6. Full stash except one cell: 95 + 20 becomes 99 + 16 and the source is removed once.
7. Matching headroom spread across backpack and multiple extra tabs: conservation still holds.
8. Repeat each test through the real caller (`AutoGetItem`, ctrl-click transfer, withdrawal, CloseStash, and held-item auto-save recovery), not only the helpers.

For every test, calculate total units across source plus all destinations before and after. This invariant catches loss and duplication even if placement layout changes later.
