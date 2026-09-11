# Set pieces keep their weapon families (v1.11.034)

**Date:** 2026-09-11
**Branch:** renderer-32bit, local commits only

The user asked: "While waiting, do the set items that have no bases".

## Finding: the set items with no bases were already fixed

That gap closed on **2026-08-21**. The continuation plan (2026-08-16) and the project memory still described it, and both had gone stale:

- **Amulets and rings** (19 pieces) resolve through `ItemMiscIdIdx`, the first droppable amulet or ring row. This added no item ID, so no save-format change.
- **The relic and the cloak** were re-slotted, by the user's decision that day, onto bracers and legs. Those are the only two locations Leoric's Fallen Court leaves free, so they are the only ones that raise its ceiling.
- `OracoolItemSets.EverySetPieceHasASpawnableBase` pins 94 spawnable and 0 unspawnable, and it has passed in every run since.
- Named set pieces also **drop**, through their own hook outside the droppable pool so they are save-safe, and a vendor shelf stocks them.

## The gap that was still open: every main-hand piece was a sword

`BaseItemForSetSlot` keyed on the slot word alone, so all eight `main_hand` pieces sat on the Short Sword:
- "Thunder's Black Pinion" is designed as a long war bow, and it fought as a one-handed blade.
- Four maces and scepters (Censer of Saint Vhal, Scepter of the Hollow Litany, Morrowbell of the Last Watch, Edict of the Broken Scepter) fought as swords too.

This was flagged by the external audit of 2026-08-17.

**`BaseItemForSetPiece(def)`** now reads the design's base word:

| Designed as | Base now |
|---|---|
| bow | `IDI_ROGUE`, the Short Bow (two-handed) |
| mace, scepter, censer, bell | the new `IDI_ORACOOL_SETBASE_MACE` |
| falchions, the sickle | the Short Sword, unchanged |

Both carriers are **`IDROP_NEVER` with no unique type**, exactly like the Short Sword. They are never in a drop or shop pool and never roll a unique. Only creation asks the function (the named-set drop, the vendor shelf, `giveitemset` and `givesset`). A saved piece keeps the base it was made on, so existing items do not change. The pack and hero goldens pass unchanged.

**The bow is two-handed, and that costs nothing.** Its set, Stormcrow Harness (six pieces), has no off-hand piece. `NoSetPromisesARungItCannotPay` used to assert that no set base was two-handed. It now models the hands properly: up to two one-hand pieces OR one two-hander, whichever wears more of the set. Every rung is still reachable.

New test: `OracoolItemSets.MainHandSetPiecesKeepTheirWeaponFamily`. It pins the eight pieces to bow, mace or sword, and both carriers to never-drop with no unique type.

## Also

My edit helper put CRLF endings on a single-line replacement in item_sets.h, which is an all-LF file. The endings were restored, and every touched file was checked against HEAD.

## Verification

Debug and Release built. ctest **700/700** with no expected value changed. RTM refreshed with exe 1.11.034. Not seen in play. The check is to drop or buy Thunder's Black Pinion (Stormcrow Harness) and fire it as a bow, and to see a set mace swing as a mace.

## Follow-up: the Rat King's gold find (v1.11.035)

The user asked: "While waiting, do the Rat King's gold find". Like the missing bases, this had already been done on 2026-08-21, and the continuation plan had gone stale. I traced the chain end to end:

1. **The power:** each of the four pieces carries `IPL_GOLDFIND`, 15-20% each. `SaveItemPower` writes it to `Item::_iPLGoldFind`, and the save format stores that field (loadsave version 8).
2. **The rungs:** all three rungs carry it too, at 40, 50 and 60%. `ApplySetBonusesToTotals` applies every reached rung on a scratch item and adds that item like a worn one, so the rungs stack. A full set gives 215%.
3. **The total:** `ItemBonusTotals::AddItem` sums it into `goldFind`, which becomes `Player::_pGoldFind`. The charms of Greed feed the same total.
4. **The payout:** `ApplyMagicAndGoldFindToDrop` scales every fresh gold pile by (100 + gold find)%. Monster drops (`SpawnItem`) and object drops (`SetupBaseItem`) both reach it through `FinalizeFreshDrop`. A saved pile reloads with its stored amount, so nothing is replayed.
5. **Uniques:** those with `gold_find_percent` ride the same power, through the unique affix table.

The only gap was a test. The payout was pinned (`GoldFindScalesDroppedGold`), but the set's half was not. `OracoolFindStats.RatKingsTitheGoldFindReachesThePlayer` now equips two real pieces made by `MakeSetItem` and expects 70, which is 15 + 15 plus the two-piece rung's 40. ctest 701/701.
