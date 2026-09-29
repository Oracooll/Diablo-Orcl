# 2026-09-29 - Whole-code audit, round 2 (v1.12.227)

**Date:** 2026-09-29. Debug only. The user: "dont stop auditing until you hit the 5 hour tokens limit or until there are
no bugs left in the code", and "you can audit the entire game code from v1.0.0 until now".

Round 2 ran six read-only tracks:
1. items and loot;
2. crafting and the artisans;
3. rifts and travel;
4. the Sorcerer, Rogue, Monk and Necromancer;
5. saves and containers;
6. a review of round 1's v1.12.226 fixes.

Every finding was verified against the code first. The coverage plan is in the session scratchpad
(`audit_coverage.md`).

## Fixed: items could be lost or duplicated

- **Inventory SORT deleted items.** A repack that first-fits in value order can pack worse than the player's own layout,
  and anything left over was dropped. It is now all or nothing, as the stash's SORT: the containers are snapshotted and
  restored untouched, with a message, if anything would not fit.
- **Shift-click equip destroyed the displaced item.** This happened when there was room neither in the pack nor on the
  floor. The swap is now undone: the item stays worn, and the one being equipped goes back to its cell.
- **Workshop stash spend duplicated items.** Spending part of a stash stack at the workshop (Ogden's steps, the Mystic's
  loan) did not mark the stash dirty, so the stack came back on reload next to what it had paid for. It is marked now.
- **Stash edits in place were not saved.** Identify, Repair, Recharge (the skills and the paid services) and Oil on a
  stash item did not mark the stash dirty. They do now.
- **A torn stash file was emptied, not refused.** The first gold picked up then marked it dirty, and the next save wrote
  an empty stash over every hero's. Both "size invalid" branches now refuse the file, as the unreadable case does.
  Stash gold is clamped on load.
- **A torn extra-pages file was saved back half-read.** It is now refused on every early return. A refused file loads no
  page contents: the pages lock for the game (`IsInventoryTabLocked`), so nothing put there goes unsaved, and nothing
  half-read can be moved out and duplicated.

## Fixed: crafting and the Mystic

- **Free the Sockets blanked any host's name.** It now clears the name only when the stones made a runeword.
- **Mystic rework:**
  - It rerolled the base armour; the base's rolls now come from the item's own seed, with the game's RNG put back.
  - It undid Zod's indestructible stamp, and so did a shard removal.
  - It halved Tempering's durability on an ethereal item; the ethereal state is now restored before the shards.
- **The load-time affix repair is retired.** It fixed a v0.3.42 display bug (2026-08-07) that no loadable item can carry,
  and it rewrote legitimate crafted values: a Blood craft merged into a Rare's life affix became 10 life. Crafted life
  and mana steal keep their own row, because their percentages are flags that do not add.
- **Recast and Consecrate** keep Kanai's Work of Cathan.
- **Vendor uniques no longer use up the unique:** the magic shelf restores the unique flags after its roll, as
  `CreateUniqueVendorItem` does, so a unique stocked there can still drop later.
- **A Normal boss's quest unique** gets an item level. It had none, so Awaken refused it.
- **The Cube's rerolls** keep the item's wear and broken state; they were a free repair.
- **Levski's Cube acts on the release:** the X, Recipes, Transmute, the salvage plates and the hammer. A bulk salvage
  plate used to destroy every matching item on the press.
- **Levski's Cube and the workshop close on a level change.** The Cube followed the hero into the dungeon and held
  autosave off.
- **The workshop's gold checks** compare in 64 bits.

## Fixed: rifts

- **A town portal taken on the closing tick** loaded a dead rift. The close now waits for the hero to arrive.
- **Opening a rift destroyed an entered one.** It now refuses, with a message, while a Guardian Rift is live or a cleared
  Nephalem Rift is in its sixty seconds.
- **The fallen guardian's slot stayed "the guardian".** Doppelganger clones in it dropped the guardian's pile again and
  again. `IsRiftGuardian` is now false once the rift is done.
- **Crypt story books in rifts** could move Na-Krul's quest; they are no longer placed there.
- **Placing the hero:** the first placement loop skips exits too. A revisit could put him on the arrival exit, which sent
  him straight home.

## Fixed: the other classes

- **The Sorcerer's cold armours:** they never wore off after a stair. Their missile now travels with the hero, like
  Infravision.
- **Attract and Death Mark** failed on a walking monster; they now match either end of its step.
- **Gamepad attacks:** the primary attack disarms the skill latches, as the mouse does. It used to re-fire, and re-pay,
  the last bow or melee skill.
- **Cold Arrow's chill** grows with rank, as its tooltip says.
- **Teeth:**
  - it no longer hits the monster ahead twice;
  - its line count is capped at the line's 6 tiles.
- **Power Strike's lightning** obeys immunity and resistance.
- **Sweeping Reed** strikes the two tiles beside the target, as its text says.

## Fixed: round 1's own fixes (the v1.12.226 review)

- **Talic:** he could freeze past his leash, spinning without striking. He now steps home if he can, and spins on if not.
- **Weapons Master:** its Rage no longer comes from Whirlwind blows, which paid for the spin and more.
- **Whirlwind:**
  - The start lands the first blow. With just the start's 5 Rage, the press was paid for with nothing landed.
  - Only the readied Whirlwind starts; Quick Cast could start it and pay for it.
- **Vengeance:** the v1.12.226 "cold immunity" check is removed. The game has no cold immunity, so it could never fire;
  that report is corrected.
- **Aura Stop cues** are silent at death.
- **Loading a save:** the missile facing fallback now covers this path too.

## Not changed (open)

- **Melee latches:** a skill armed mid-swing still rides the swing under way (from round 1). It needs capture at
  StartAttack across three modules.
- **Benches on exit:** items on a bench are lost on exit when both the pack and the stash are full (documented backstop).
- **The Mystic's Reroll:** it is paid before its offers exist; a thin pool offers copies of the current affix.
- **Salvage then sell** beats selling some cheap bases (a small gold faucet).
- **Unique level versus item level** on chest drops: a ruling for the user.
- **Belt:** shift-click still moves scrolls, oils and runes into the belt. The potions-only rule was written for
  automatic placement; the user's call.
- **Paladins saved before 2026-09-06:** their tree data is shifted by the Holy Bolt row's removal (v1.9.301), with no
  migration. It only matters if such heroes still exist.
- **The RfA-12 bow skills** ignore walls and range (visual arrows).
- **Madawc:** arrow to-hit and no hit sound (from round 1).
- **Minor:** the Rift Monument's `Gate()` has no town guard (cosmetic).

## Tests

- **Replaced:** the retired repair's five tests became `Item.RepairOracoolAffixesIfCorrupted_LeavesACraftMergedValueAlone`.
- **New:** `OracoolAudit.MysticReworkKeepsTheBaseArmourAndZod`.
- **Updated:** `OracoolLevskiRoar.FreeingSocketsReturnsTheStonesAndTheItem` (a non-word host keeps its name).

Debug build and ctest: 882/882. Nothing seen in play.
