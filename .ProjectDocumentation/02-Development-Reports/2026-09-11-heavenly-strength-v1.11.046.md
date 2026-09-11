# Heavenly Strength (v1.11.045-046)

**Date:** 2026-09-11
**Branch:** renderer-32bit (default), local commit

The user asked: "build Heavenly Strength passive skill". Then, while the build ran: "if i replace Heavenly Strength while carying two hand sword and shield then shield must go to inventory of drop on ground if inventory is full."

## What it does

It is the Paladin's first Passive Skills row (page 3, tier 0, column 0). It was inert until now; the row said "Not yet built". The new text: **"Bear a two-handed sword or mace in one hand and a shield in the other."**

## How

The Barbarian already had this grip in vanilla. `Player::GetItemLocation` reports a two-handed sword or mace as `ILOC_ONEHAND` for a Barbarian, and **every equip rule asks that function rather than the item**: the paste rules in inv.cpp, auto-equip, the cursor tooltip.

So Heavenly Strength is one more clause there, `oracool::HeavenlyStrengthGrips(player, item)`: a two-handed sword or mace, with the passive slotted (`PassiveSlotOf >= 0`, the page's whole meaning of "active"). Nothing else in the equip code had to change.
- It is declared at the top of `player.h`, so the inline `GetItemLocation` can ask it without pulling in the class tree.
- It is defined in `oracool/class_tree.cpp`.

**Why swords and maces only**, as with the Barbarian: they are the two-handers whose body sprites have a shield variant (`SwordShield`, `MaceShield` in `CalcPlrItemVals`).
- An axe, a staff or a pike in one hand would draw the hero with no shield at all.
- A bow needs both hands.

## Taking it out

`SetPassiveSlot` (replacing it) and `ClearPassiveSlot` (emptying it) are the only writers of the passive slots. Both now call `ReleaseHeavenlyGrip` when Heavenly Strength is the row leaving.
- If a two-hander and a shield are both in hand, the shield goes to the **backpack** (`AutoPlaceItemInInventory`).
- If the backpack is full, it goes to the **ground at the hero's feet** (new `DropItemBesidePlayer`, the dying hero's own free-tile search, `DeadItem`), with the message "Your backpack is full - the shield is on the ground at your feet."
- The slot change is never refused. The first build refused it on a full backpack, and the user's instruction replaced that.
- The hero is recalculated with sprites (`CalcPlrInv(player, true)`), because the body changes from sword-and-shield to sword. The Abilities window's own recalculation after a slot change is stats-only.

## Tests

`OracoolClassTree.HeavenlyStrengthLetsATwoHanderShareTheHandsWithAShield`:
- Without the passive, a two-handed sword takes both hands; slotted, it takes one, and so does a maul.
- A bow, an axe and a staff keep both hands.
- Clearing the slot with a sword and a shield in hand leaves the sword and puts the shield in the backpack.

`EveryInertRowContributesNothing` now counts **42** built Passive-page rows, and the class tree's comment says so.

**Writing the test took four rounds, recorded here because two of the lessons are general:**
- **Archives.** The backpack reads an item's size from the cursor sprites, which live in `diabdat.mpq`. With only the core archives mounted it faulted, so the test mounts the game archives and skips without them.
- **Uninitialised fields.** `Player::_pNumInv` and `InvGrid` have no default initialisers, so `player = {}` leaves them holding garbage and the placement wrote `InvList[garbage]`. The test now resets the backpack the way `InvTest`'s SetUp does.
- **The network notice.** `AddItemToInvGrid` sends `MyPlayer`'s backpack changes to the network, and in this test's process that send faulted. Temporary markers traced the fault there, and they were all removed (inv.cpp is byte-identical to HEAD).
  - `InvTest`'s own placements survive the same call. Why the two processes differ is **not settled**.
  - The test's Paladin is therefore a second player, so the notice is not sent. The placement, the grip and the slot change are what the test covers.
  - In play the notice runs on every pickup.
- **Not covered by a test:** the full-backpack branch (the ground drop). It needs a live level.

## Verification

Debug and Release built, ctest **709/709**, RTM refreshed with exe 1.11.046. **Not seen in play.**

**To check with a Paladin:**
- Slot Heavenly Strength, then equip a two-handed sword or maul and a shield together. The body should show sword or mace and shield.
- An axe, a staff or a bow still takes both hands.
- Empty or replace the slot: the shield goes to the backpack, or to the ground with the message when the backpack is full.
