# 2026-09-27 - The four decisions, and audit pass 5 (v1.12.198)

**Date:** 2026-09-27. Debug only. The user said: "Four things I left for you to decide - fix all four. dont worry about save breaks. and run a few more audits", then "run a few more audits".

That meant two rounds of four read-only audits (listed below), with the PC kept awake this time. I verified each finding in the code before fixing it. Findings left for a decision are at the end.

- **Round 1:** crafting and item conservation; skills and combat; loot rules; levels, rifts and waypoints.
- **Round 2:** save files; windows and input; monsters; stores and gold.

## The four decisions (from audit pass 4)

- **The workshop's counters live on the item.**
  - Item format 15 adds three bytes: `_iOracoolRerolls`, `_iOracoolRemovals` and `_iOracoolLockedAffix`. They were a per-game table keyed by seed, so going back to the menu reset the doubling price and freed the lock.
  - Files in formats 13 and 14 still load. The load-side size checks ask `ItemSaveSizeFor(the file's format)`.
  - A lock that points past the item's affixes (say, after a Cube recipe) locks nothing.
  - Test: `LoadSaveOracoolItemExtensionsTest.RoundTripsFullyPopulatedTieredItem` now pins the three fields.
- **A Guardian Keystone is spent at the first step through the portal, not when it turns.**
  - The rift remembers the keystone's seed. `SpendPendingKeystone` finds it on any backpack page or in the stash and takes it at entry.
  - Quitting before going in costs nothing. With the keystone gone (sold, dropped) there is no step through until it is back.
  - Test: `InvTest.GuardianKeystone_IsSpentAtTheFirstStepNotWhenItTurns`.
- **The Armor class odds bar is the chance to be HIT**, not only to be reached.
  - It now accounts for Dodge, Mantra of Evasion and the shield's block, with the hero taken as standing.
  - New pure chance getters sit beside the rolls: `PassiveMeleeSlipChance` and `Rfa12ActiveMeleeEvadeChance`.
  - Test: `Player.ChanceToBeHit_CountsTheShieldsBlock`.
- **The tooltip card sorts lines by the TRANSLATED prefixes.** Each prefix is items.cpp's own format string passed through `_()`, up to its first `{`.

## Fixed from the audits

### CRASH / memory

- **Stash:** `RemoveStashItem` cleared the removed item's cells on the page on screen only.
  - Ogden's boards and the keystone take items from any page. The other page kept cells naming an index past the list (an out-of-range read when drawn), or, after the swap, a different item.
  - Test: `InvTest.RemoveStashItem_ClearsTheItemOnEveryPage`.
- **Shop service cursors:** the hammer and recharge cursors read every hovered index as a backpack index.
  - On page 1 the arrays happen to sit side by side, so it worked by luck. On pages 2-10 it charged for and "repaired" a hidden slot, and on page 10 it wrote past the pages.
  - `ServiceCursorItem` now resolves worn slots, the open page and the belt separately.

### HIGH

- **Rift portals:**
  - Opening a new rift left the old rift's town portals open. They led into a level rebuilt from the NEW seed with the OLD monsters. `OpenCommon` now closes them.
  - The rift's exit portals were laid before `InitMissiles` emptied the list, so the way out was a bare trigger nobody could see. They are laid after it now.
  - The way home could be laid under the hero who struck the last blow, and standing on it sent him home, the loot left behind. It now avoids heroes, monsters and the arrival exit.
- **Workshop pack loans:** Gillian's loans charged the pack the whole loan whatever the recipe used, and a result landing in the loan's cell was lost. The loan is now settled by what is left in its cell.
- **Imbue:** it consumed a whole stack of shards for one.
- **Allies:**
  - Monster-vs-monster: a monster killed by a minion's struck-back damage or by Iron Maiden was stood up again at 0 life, unkillable.
  - Class melee area skills (Whirlwind, Fend, Wheel of Heaven, Sweeping Reed, Radiant Palm, Hammer of Faith's splash, Zeal) hit the hero's companions and minions, and credited the kills.
- **Items:**
  - Guaranteed boss drops (keystone, sealed map, encounter reward) overwrote an item already on the boss's tile. `SpawnQuestItem` now moves them to the nearest free tile.
  - A staff's spell prefix ignored the item-level ceiling.
- **Shops:** repair prices overflowed an int on million-gold Torment items. They are computed in 64 bits and capped. Test: `StoresTest.RepairPriceFor_DoesNotOverflowOnAMillionGoldItem`.
- **Windows:** a left click on the event log's body walked the hero, swung, cast, or dropped the held item.

### MEDIUM

- **Refinement and item prices:**
  - Refinement shards now scale cold resistance.
  - Items reworked at the bench sold for their base value. They are re-priced from their affixes now.
- **Levski's Cube:** it hands its grid back to the stash when the pack is full.
- **Alt+F4:** closing the window now returns the Cube's and workshop's staged items too. `SaveOnExit` closes both.
- **Wirt's gamble:** with nowhere to put the result, it took the gold and the item was gone. The result now goes to the stash, or really at the hero's feet.
- **Level changes:**
  - Confuse and Conversion became permanent after a level change. Both are released before a level is saved.
  - The level load wiped the hero's own passive clocks: Cheat Death's minute, Final Service, Rathma's Shield, the Rampage stacks. It now clears only the per-monster marks (`ClearPassiveMarks`).
- **Missiles:** the hero's own missiles cut down his minions.
- **Save files:** a stash, hero-items or extra-pages file that was THERE but could not be read was treated as absent, then overwritten by the next save.
  - The stash and extra pages are now left untouched and not saved over for that game.
  - The hero load stops rather than rebuild the gear from seeds.
- **Monsters:**
  - A champion's resistance merge could make it immune to all three schools (Webwidow's clones on Normal). The immunity that came only from the base type goes back to a resistance. Test: `OracoolAudit.AChampionIsNeverImmuneToAllThreeSchoolsByTheMerge`.
  - A revisited floor kept discarded monsters' colours in their slots. `RestoreVariantTint` rebuilds an ordinary monster's tint on load.
- **Windows:**
  - Right clicks passed through the Rift Monument's menu.
  - A refused close logged a red line every tick while the hero walked away. It now logs once every 5 seconds.
  - The runeword book and the crafting menu opened under a modal shop dialog.
  - Stat buttons still acted after the sheet was closed mid-press.
  - The workshop and the Cube could both be open after a refused close.
  - Escape now closes a lone event log.

### LOW

- **Items:**
  - A stat and its curse can no longer share an item. Test: `PrimalItemTest.NoItemCarriesAnAffixAndItsCurse`.
  - A Primal's rerolled affix is a perfect roll.
  - Removing a shard no longer repairs the item.
- **Workshop:** its floor fallback drops on a free tile.
- **Skills:** Serenity already thawed the chill (v1.12.197). Hunter's Claim, echoes and threads no longer outlive their monster.
- **Monsters:**
  - Champions past the 31-slot corpse cap leave their type's body.
  - Minions take no variant.
  - Hell damage on a unique clamps at 255.
  - The Monster Variant Chance option can no longer be changed mid-game.
  - The health bar is computed in 64 bits.
- **Save files:**
  - A crafted tab file with an out-of-range count clears its grid.
  - Hero names are copied bounded.
- **Tests:** `PackTest`'s golden row HF 57 was regenerated, because the curse-twin rule changed what that seed rolls.

## Not changed: decisions for the user

- **Wirt's gamble is a gold faucet.** From about hero level 42 the result often sells for more than the gamble costs, because top tiers value items ×30. There are three ways to fix it:
  - price the gamble from the result;
  - discount a gambled item's sale value;
  - drop the tier roll for gambles.
- **Craft recipes add two fixed powers on top of a Rare's roll.** That can go past the Rare affix limit and the item-level ceiling. It is D2-style crafting; say if it should obey the limits.
- **"Sell All" also sells everything on pages 2-10**, including crafting stock. Either the tooltip or the rule should change.
- **Revisited floors can still show champions' corpses with the wrong body.** The corpse table is not saved with the level.
- **Failed summons:** a Golem cast at rock dismisses the old golem, and a failed Revive still takes the corpse.
- **Gamepad:** it cannot operate the fork's windows (Stonegate menu, workshop, Cube).
- **Cube tabs:** the Cube's side tabs are not treated as window area.
- **Press/release:** a few windows act on the press rather than the release (waypoint menu, runeword filters).
- **Keyboard:** the Stonegate menu has no keyboard selection.
- **Alt+F4 with a full pack AND a full stash:** a staged Cube or bench item is still lost.

## Tests

- v1.12.198 Debug builds clean, and the full suite passes: 866 of 866, including the shuffled runs.
- Two earlier builds stopped on my own slips: a missing `qol/stash.h` include in levski_roar.cpp, and an unused helper that did not compile.
- Not covered by a new test:
  - the rift portal fixes (they need a level load; a screenshot is the check);
  - the unreadable-file refusals;
  - the window fixes;
  - the controller changes.
