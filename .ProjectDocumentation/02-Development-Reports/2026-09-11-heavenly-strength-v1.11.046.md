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

## Every two-hander, and the shield icon (v1.11.047-048)

The user reported: "Heavenly Strength works for Great Sword, but doesnt work for Great Axe, Bows, Staff. Check your code."

That was the first cut's design. It took only the Barbarian's two, swords and maces, because theirs are the body sprites with a shield variant. The user wants every two-hander.

**Every two-hander.** `HeavenlyStrengthGrips` now takes every two-handed **weapon** (`ICLASS_WEAPON`, `ILOC_TWOHAND`): great axe, pike (an Axe), bow and staff join the sword and mace. The skill reads "Bear a two-handed weapon in one hand and a shield in the other."

The other hand still takes only a shield. For every class but the Bard, the paste rules put a second weapon in place of the first (`pasteIntoSelectedHand` in inv.cpp), so no dual-wield opens.

**The icon.** The user then asked: "when these cases occur show a shield icon over the hero. There is one such icon in original assets. Reflect icon i think it was."

An axe, bow, staff or pike held with a shield draws the hero with the weapon and no shield, because no such body sprite exists. For exactly those cases, `DrawPlayerIcons` now paints the **Reflect** icon over the hero. That is the Hellfire spell's own `MissileGraphicID::Reflect` shield, drawn at the spell's own offset.
- The condition is `oracool::HeavenlyGripHidesTheShield(player)`: the grip active, a non-sword non-mace two-hander in one hand, a shield in the other.
- It is drawn once even when the Reflect spell is also up.
- A sword or mace shows its own shield and gets no icon.

**Tests.** `HeavenlyStrengthLetsATwoHanderShareTheHandsWithAShield` now expects one hand for the maul, great axe, bow and staff. It also checks that an axe or a staff with a shield hides the shield (icon on), and a sword with a shield does not.

Debug and Release built, ctest **709/709**, RTM refreshed with exe 1.11.048. **Not seen in play.**

**To check:**
- A great axe, bow or staff plus a shield: both equip, and the Reflect shield hangs over the hero.
- A great sword or maul plus a shield: the body shows the shield, and no icon.
