# v1.11.055 - a repaired item is whole again; the shop's gold on bare canvas; button labels raised

2026-09-12. The user sent a screenshot of Griswold's shop and listed three problems:

1. "a shield is 16/16 durability but has X on it and doesnt appear as shield when i equip it. It was DUR 0 and Griswold repaired it but something doesnt work with this repair."
2. "remove the baground behind gold counter in store."
3. "center button labels better by raising them a few px."

## 1 - the repaired shield stayed broken

**How an item breaks.** In single player, equipment is not destroyed at 0 durability. `BreakOrRemoveEquipment` empties it (`_iDurability = 0`) and raises `_iOracoolBroken`. `CalcSelfItems` starts every worn item at `_iStatFlag = !_iOracoolBroken`, so a broken item is switched off:
- it gets the X overlay;
- it gives no stats;
- it is not drawn on the body, so the hero showed no shield.

**Why the repair didn't fix it.** Several paths restore durability, and only some cleared the flag.

| Path | Cleared the flag? |
|---|---|
| Smith repair of a worn item (`SmithRepairItemAt`, body branch) | yes |
| New shop's repair (`ShopRepairItemAt`) | yes |
| Repairing the held item | yes |
| Crafting | yes |
| Smith repair of a **backpack** item (`SmithRepairItemAt`, `InvList` branch), which Repair All also uses | **no** |
| The Repair skill (`RepairItem`) | **no** |

The shield broke while worn, was taken off, and was mended in the backpack by Repair All. It came back 16/16 with the flag still up.

**Fix.**
- Both missing paths now clear the flag.
- The rule is made self-healing, so no restore path can leave the problem behind again. **Broken means emptied.**
  - `CalcSelfItems` treats a worn item with durability above 0 as whole. This heals the user's shield on the next recalculation, and also covers shrines and any path added later.
  - Loading a save keeps the flag only if durability is 0. Durability is read earlier in the same function, and the edit script checked that order.

## 2 - gold counter

`DrawShopControls` drew "Your gold" on the vanilla button face pressed in (or the limestone plate `ShopGoldPlateArt`). It is now drawn straight on the panel, shadowed. That was its first look, when the shadow was asked for (2026-09-05).

## 3 - button labels

The row buttons' labels (Repair, Repair All, Recharge, Refresh) are raised **2 px** (`LabelLift`). The font's letters sit low in their line, so `VerticalCenter` alone put them below the plate's middle. The pressed face still sinks its word 1 px. The side tabs are unchanged, because their labels run vertically.

## Tests

- **New:** `OracoolStatSheet.ARepairedItemIsNoLongerBroken`. A worn shield broken at 0 is switched off. Give it back its durability only, as the backpack repair did, and it is whole and counted again.
- **Corrected:** `CalcPlrInv.BrokenItemContributesNoStatBonus` raised the flag on a 40/40 weapon, a state the game never produces. It now breaks the weapon the way `BreakOrRemoveEquipment` does (durability 0 plus the flag). It still proves a broken item gives nothing.

## Verification

Debug and Release built, ctest **713/713**. **Not seen in play.**

**The RTM exe was NOT refreshed:** the RTM `DiabloOrcl.exe` was in use, almost certainly the user's running game, and was left alone. Copy it once the game is closed:

    Copy-Item "build\x64-Release\DiabloOrcl.exe" "..\DiabloOrcl RTM" -Force

**To check:**
- Equip the Door of the Penitent: no X, and the hero holds the shield.
- The gold line in any shop has no plate behind it.
- The button words sit centred.
