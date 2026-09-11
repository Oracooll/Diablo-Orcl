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

## The block sheet that never existed (v1.11.049)

The user hit an error box entering a dungeon: "Failed to open file: plrgfx\warrior\wma\wmabl.cl2".

That name is the Warrior body (`w`), medium armour (`m`), **axe** (`a`), **block** (`bl`).
- The level load asks for every body graphic.
- The block graphic is asked for whenever a shield is worn (`_pBlockFlag`).
- Vanilla never allowed an axe beside a shield, so no axe block sheet was ever drawn.
- v1.11.047 widened Heavenly Strength to every two-hander, which made the pair possible and the load fatal. Bows and staves would have hit the same wall.

**The fix is at the single authority, `LoadPlrGFX`**, as the Shield Bash and death-sheet crashes were fixed.
- For the Block graphic, the archive is asked (`FindAsset`) whether the named sheet exists before it is trusted.
- When it does not exist, `oracool::BlockSheetFallback` sends the hero to the shield-carrying sheet nearest the weapon:
  - the axe, the staff and the mace use mace-and-shield's (`h`);
  - the sword uses sword-and-shield's (`d`);
  - the bow and the empty hand use empty-hand-and-shield's (`u`).
- Asked, not assumed: a class that HAS the sheet keeps it. The Monk blocks with his staff.
- The rename happens before the width and the PNG look-up, which follow the weapon graphic.

For the few frames of a block, the body shows the borrowed weapon. The shield is what the animation is about, and the alternative was no block at all.

**Test.** `OracoolClassTree.HeavenlyStrengthBlocksWithASheetThatExists` checks the rule, then the real archive: `wmabl.cl2` is absent, and all nine fallback sheets a Warrior-bodied hero can be sent to (`w{l,m,h}{u,d,h}bl.cl2`) are present. It skips without diabdat.mpq.

Debug and Release built, ctest **710/710**, RTM refreshed with exe 1.11.049. **Not seen in play.**

**To check:** enter a dungeon with an axe, a bow or a staff plus a shield, then get hit and block.

## v1.11.050 - bows need both hands again; axe and staff wear the mace-and-shield body

The user, 2026-09-11: "lets make bows always require 2 hands. makes no sense to have a bow and shield. but it is possible, and therefore we keep it, to wear heavy axe or big staff with one hand and shield in the other." Then: "hero wears axe + shield we will use mace+shield combo assets. hero wears staff+shield - same combo. No Reflec icon anymore."

**The bow rule.** `HeavenlyStrengthGrips` now turns a bow down. Every other two-hander still goes in one hand while the passive is slotted: the great sword, the maul, the great axe (and the pike, an Axe) and the staff. The skill's text now reads: "Bear a two-handed axe, sword, mace or staff in one hand and a shield in the other."

**A bow and shield saved by 047-049.** The new `oracool::EnforceTwoHandedGrip` handles these. It runs on every level load in `LoadGameLevel`, after the hero has been placed on a tile.
- It looks for a weapon that `GetItemLocation` says needs both hands, with anything in the other hand.
- It moves that other item off the same way the passive's removal does: into the backpack, or onto the ground at the hero's feet if the backpack is full.
- An axe with a shield is still a legal pair and is left alone.

**The body.** In `CalcPlrItemVals`, an axe or a staff held with a shield now selects `PlayerWeaponGraphic::MaceShield`. A sword keeps `SwordShield`. The hero is drawn holding a mace and shield. The attack timing is that sheet's too, so it follows the mace-and-shield frames.

**The icon is gone.** The Reflect icon over the hero was removed, along with the `HeavenlyGripHidesTheShield` check behind it. The Reflect spell's own icon is untouched. The v1.11.049 block fallback (`BlockSheetFallback`) stays, as insurance for any sheet an archive lacks.

**Tests**, in `HeavenlyStrengthLetsATwoHanderShareTheHandsWithAShield`:
- With the passive slotted, a bow is `ILOC_TWOHAND` while a mace, an axe and a staff are `ILOC_ONEHAND`.
- `EnforceTwoHandedGrip` leaves an axe and shield alone.
- It parts a bow and shield, and the shield lands in the backpack.
- An axe or a staff with a shield gives `MaceShield` in `_pgfxnum`'s weapon nibble, and a sword with a shield gives `SwordShield`.

Debug and Release built, ctest **710/710**, RTM refreshed with exe 1.11.050. **Not seen in play.**

**To check:**
- Axe with a shield, and staff with a shield: the hero is drawn with a mace and shield, walking, attacking and blocking, with no icon overhead.
- A bow can no longer go beside a shield.
- A hero saved with a bow and a shield has the shield in the backpack after the next level load.
