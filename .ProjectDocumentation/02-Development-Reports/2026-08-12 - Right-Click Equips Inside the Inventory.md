---
title: 2026-08-12 - Right-Click Equips Inside the Inventory
date: 2026-08-12
tags: [dev-report]
summary: Inside the inventory window, right-click now equips backpack equipment and un-equips worn items, and never falls through to casting the readied spell. Consumables keep their vanilla right-click-to-use.
---

# Right-Click Equips Inside the Inventory

User request: "make right clicking on items in inventory screen equip/unequip them. currently right click on the inv window casts the RMB skill which is meaningless in this case."

## What changed

One branch in `RightMouseDown`, placed after the existing `UseInvItem`/`UseStashItem` attempts and before the spell-cast fallthrough:

- Cursor inside the inventory window (`invflag` and the panel rect) and the click was not consumed above: route to `CheckInvCut` with `automaticMove = true`, then **return unconditionally**.

That return is half the request on its own: a right-click anywhere in the window - an empty grid cell, the sygil, panel chrome - now does nothing, where before it cast the readied spell at whatever stood behind the panel.

## Why there was almost nothing to build

`CheckInvCut(automaticMove=true)` is the exact machinery shift-click has used all along: backpack equipment auto-equips into its slot, a worn item auto-stashes into the backpack, and everything it cannot place it leaves untouched. The right-click branch is a second entrance to a tested path, not new behaviour - which also means the six new slots work under it automatically, since the un-equip blocks for them landed with the boots-crash fix.

Order in the chain is what preserves vanilla habits:

| Right-click on | Handled by | Result |
|---|---|---|
| Potion, scroll, book in backpack | `UseInvItem` (unchanged, runs first) | drink / read |
| Equipment in backpack or an extra tab | new branch | equips |
| Worn item on the paperdoll | new branch (`UseInvItem` refuses body slots) | un-equips into backpack |
| Empty cell / panel chrome | new branch's return | nothing |
| Anything outside the window | untouched fallthrough | readied spell, as before |

Shift+right-click stack-splitting runs even earlier in the chain and is unaffected. With an item held on the cursor, right-click inside the window does nothing, as before.

## One linkage detail

`CheckInvCut` lived inside `inv.cpp`'s anonymous namespace - only `CheckInvItem` ever called it. Declaring it in `inv.h` without more would have compiled and then failed to link, since the definition had internal linkage. It is hoisted out of the anonymous namespace (the same pattern `stores.cpp` established for its test hooks); the internal helpers it calls remain visible to it, since anonymous-namespace visibility spans the translation unit.

## Verification

Debug build clean at `ORACOOL_VERSION` **1.1.18**, version string confirmed in the exe. Tests **349/351**, the same two pre-existing failures.

Play-test: right-click a potion (must still drink), a backpack sword (must equip), worn boots (must un-equip - the new slots' first exercise of this path), an empty cell (must do nothing), and open ground outside the window (must still cast).

## Related

- [[2026-08-12 - Worn Items Destroyed by the Diablo Save Remap]]
- [[2026-08-12 - Boots Crash - the Hover Chain Nobody Taught]]
