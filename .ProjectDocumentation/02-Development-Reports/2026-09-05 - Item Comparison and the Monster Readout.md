# Item Comparison and the Monster Readout (v1.9.258)

**Date:** 2026-09-05 · **Requests:** "add comparison tool tip (showing stats of equipped item) when hovering over items in stash/inv grid for easy comparison to equipped item of same item slot. equipped item tooltip to show title EQUIPPED ITEM somewhere (bottom or top of description in GREEN font)." / "monsters Class/Hit Points/DMG/XP to be displayed under their healthbar. Font White/RED/White/Gold."

## The comparison panel (cursor_tooltip.cpp)

Hovering an item in the backpack grid (any tab) or the stash now draws, beside its panel, one panel per worn counterpart: right of it, or left when the right has no room. Each is headed **EQUIPPED ITEM** in green and otherwise printed by the same item printer the hover uses (name in its tier colour, then `PrintItemDetails` or `PrintItemDur` for an unidentified piece), captured out of the panel-string store and the store restored - one printer, so the two panels cannot disagree.

Counterparts by slot: helm, chest, amulet and the six Oracool slots map one to one; a ring shows both fingers; a one-hand weapon shows the weapon hand and a shield the off hand (or the two-hander that fills both); a two-hander shows both hands, since it displaces both. Only occupied slots produce a panel. Worn items and belt potions get no comparison.

The tooltip drawer was split into `MeasureBlock` and `DrawBlock` so the extra panels reuse the plate, border, per-line colours and two-run lines unchanged; the dirty rect now covers every panel drawn.

## The monster readout (qol/monhealthbar.cpp)

Under the health bar, left side, four 12px lines with the name's black offset:

| Line | Colour |
|---|---|
| Class: Undead / Demon / Animal | white |
| Hit Points: current / max (whole points) | red |
| Damage: min - max (the melee range) | white |
| XP: what this difficulty pays for the kill | gold |

The mlvl stays right-aligned on the first line.

## Verification

Debug build clean; 624/625 with the standing `Drlg_l1` failure. In the game: hover a helm in the backpack while wearing one; hover a ring while wearing two; hover a shield with a two-hander equipped. Target a monster for the four lines.

## v1.9.264: shop wares compare too

"add comparison tooltip for shop items too." The shop grid now records the item under the cursor (`HoveredShopItem`, cleared with its hover flag each pass), and the tooltip's container lookup asks it first. Buy tabs compare the ware with what is worn; on Repair and Recharge the ware is the player's own piece, so the counterpart that IS the hovered item is skipped and a worn helm is never set beside itself. The Sell tab's backpack items compare as the backpack does.
