---
title: 2026-08-12 - Six New Equipment Slots
date: 2026-08-12
tags: [dev-report]
summary: Shoulders, bracers, gloves, belt, legs and boots become real item types with real slots - enums, equip logic, save format, a third icon sheet, one test item each, and five give*set debug commands. Along the way three latent test bugs and one design constraint on the loot tables.
---

# Six New Equipment Slots

The paperdoll has had thirteen slots drawn on it since the inventory rebuild, six of them inert frames that rejected everything. This makes them real.

Landed across five commits, `d1bf4bb` through `1.1.13`.

## The enums, and why everything is appended

`ItemType` +6, `item_equip_type` +6, `inv_body_loc` 7 to 13, plus the matching `inv_xy_slot` and `inv_item` entries.

Every one of them is **appended, never inserted**. `ItemType` gets written into the tiered-item save extension and `_item_indexes` is positional in `AllItemsList`, so renumbering an existing value would silently reinterpret every stored item rather than fail.

The new equip location is **`ILOC_WAIST`, not `ILOC_BELT`**. That name was already taken and means "stackable thing that goes in the potion belt". A second `ILOC_BELT` would have read as that and been wrong every time anyone touched it.

`NUM_INVLOC` going to 13 is **save-breaking** - `PlayerPack` embeds `InvBody` at a fixed size and `pfile.cpp` gates loading on an exact `sizeof()`, so an old save is rejected cleanly rather than misread. Fine under V1's always-new-game rule.

## What came for free, and what did not

Anything looping `0..NUM_INVLOC` picked the new slots up with no change at all: `CalcPlrItemVals` (via `EquippedPlayerItemsRange`, so AC, damage, resists and stats all accumulate), the save round-trip, sync, msg, the durability-damaging shrine. Anything naming a slot explicitly kept working - including the armour graphic, which reads `INVLOC_CHEST` and only `INVLOC_CHEST`. That last one is why the new slots need no new player animations.

What did not come free were the `switch`es with a `default:`. Those compile silently and do the wrong thing:

- `EquipSlotForBodyLocation` returned `Chest` for anything unlisted. Harmless while six slots drew nothing; six slots all hit-testing the chest the moment they went live.
- `Item::isEquipment()` gates `CanEquip`. Leaving the six locations out of it would have made every new item permanently unequippable, with no error anywhere.

New `isOracoolWorn()` is deliberately **not** folded into `isArmor()`. That was the obvious move and it would have sent boots to the chest slot: `isArmor()` also drives the gamepad placement logic in `plrctrls.cpp`, which assumes armour means body armour.

## Icons

`_iCurs` widened from 8 to 16 bits. Sheets 1 and 2 already use 229 of the 256 ids a byte offers, and the six types have roughly a hundred icons between them in the user's art. Runtime only - saves store the `AllItemsList` index and rebuild from seed.

`pCursCels3` is a third icon sheet, `data\inv\oracool_items.cel` in `oracool.mpq`, loaded **unconditionally** (unlike Hellfire's `objcurs2`) because these items exist in both games. Its ids continue past sheet 2's whether or not sheet 2 was loaded, so both games agree on which id means which icon. `CreateHalfSizeItemSprites` had to change to match: it used to size itself by whether Hellfire was running, which worked only because sheet 2's ids are unreachable in a Diablo game. Sheet 3's ids are reachable in both and sit past sheet 2's block, so that block's slots are now counted even when its content is absent.

`ICURS_ORACOOL_FIRST` is hardcoded at 229, matching how every other `ICURS_` value is written, with two `static_assert`s in `cursor.cpp` checking it against the real sheet sizes and `CURSOR_FIRSTITEM` - none of which `itemdat.h` can see.

`tools/ItemIconCel.cs` is the project's second CEL encoder after `WaypointCel.cs`, and the first to need **per-frame sizes**: the belt is 2x1 among five 2x2s, and a CEL stores no widths at all.

Ground drops reuse existing animations (`larmor`) rather than shipping six more CELs, with the user's agreement. That required a new `GetItemDropAnimIndex` - `ItemCAnimTbl` has exactly one entry per vanilla item graphic and no bounds check, so reading it for one of our ids was an out-of-bounds read that happens to work until it doesn't. A `static_assert` now pins the table's length.

## The loot tables are a separate job

The six test items are `IDROP_NEVER` for now, and that is not laziness.

Making them `IDROP_REGULAR` puts them in the shared candidate list every generation path funnels through (`GetItemIndexForDroppableItem`). That lengthens the list the RNG indexes into, so **the same seed produces different items** - and `pack_test`'s several dozen golden item structs all stop matching. Confirmed by doing it: 22 failures across two suites.

Joining the loot tables therefore means re-baselining those goldens, which destroys their value as a regression net for exactly this kind of change. It is a deliberate follow-up, not something to slip in alongside the plumbing.

## Three latent test bugs, all pre-existing

Each was invisible until something moved:

1. **`writehero_test`** cleared only 7 of its pack's `InvBody` entries with a literal. The rest stayed zeroed - and `idx == 0` is `IDI_GOLD`, a real item, not an empty slot - so it unpacked six phantom gold pieces. A stale literal `40` for `InvList` was sitting next to it.
2. **`pack_test`** had the identical trap in its `PlayerPack` aggregate initialiser: seven `InvBody` entries listed, six value-initialised to zeros behind them. Now six explicit `0xFFFF` entries, which is what `PackItem` writes for a blank.
3. **`stores_test`**'s `makeItem` lambda took `int8_t curs`, truncating every id above 127. `ICURS_LEATHER_ARMOR` (135) arrived as -121 and round-tripped back to 135 **only** because `_iCurs` was itself `uint8_t`. At 16 bits it became 65415, `GetInvItemSize` read far out of bounds, and `SortStash` crashed with a divide by zero.

The golden save hash was re-baselined for the intended format change - third time, reason recorded alongside the previous two.

## The give*set commands

`givebset`, `givemset`, `giverset`, `giveuset`, `givepset` drop one item per equipment slot at Basic, Magic, Rare, Buffed Unique and Primal quality respectively.

Base items are found by walking `AllItemsList` for the first entry with the right `iLoc` rather than hardcoding thirteen indices - those indices are positional and shift whenever the table gains a row, which is precisely what this work kept doing. `IDROP_NEVER` entries are skipped so a slot does not land on a quest base (`ILOC_HELM`'s first match is The Undead Crown), except for the six worn types, which are `IDROP_NEVER` on purpose and would otherwise be unreachable.

Thirteen items, not thirteen locations: the rings are the only pair sharing an item location, so `ILOC_RING` appears twice.

The user assumed thirteen items fit within five tiles. `GetSuperItemSpace` widens its search until it finds a free tile, so in the open they do - but a corridor or doorway is another matter, so the command reports what it actually placed rather than assuming.

## Verification

Debug build clean at `ORACOOL_VERSION` **1.1.13**, version string and all five command names confirmed present in the exe. Tests **347/349** at every commit - `Drlg_l1.CreateL5Dungeon_diablo_3_844660068` and `Timedemo.WarriorLevel1to2` only, both pre-existing and unrelated.

**Not play-tested.** Needs: the five commands run and the items picked up; each of the six new slots equipped and unequipped by drag, by shift-click and by auto-equip; the AC total checked against the character sheet; and a look at whether the leather-tier icons read well in the panel - they quantise warm, because the palette's saturated-warm family is its red-brown ramp. Steel-tier source items would land much closer to their originals.

## Related

- [[2026-08-12 - Six Play-Test Fixes]]
- [[2026-08-11 - Waypoint Sprite and the First CEL Encoder]]
