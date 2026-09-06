# Item kinds by legend colour; runes drop and wait for a step (v1.10.009)

**Date:** 2026-09-07
**Request:** "BR-3 - health potions / BB-3 - mana potions / YL-3 - rejuv potions / GD-6 - books / OR-7 - runes. Also check why i never see El-Zod runes drop. They appear straight into my backpack. Make them drop and only auto-pickup after i move a tile. / GR-7 - Ethereal Items. Supercedes Socketed modifier. Also Ethereal stat row to use same font."

## Five new colours

BR-3, BB-3, GD-6, OR-7 and GR-7 join the game the legend's way: five .trn files, five `text_color` entries (the tables are 30 now), five `UiFlags` bits (43-47), five lines in the flag lookup, and the outline mask. YL-3 already existed for Rare.

## The kinds

All at `Item::getTextColor`, so every name site follows: floor labels, tooltips, the held-item line, Levski's grid, the stash, shops. Quality still wins, so the kind colours apply to plain-quality items: health potions BR-3, mana potions BB-3, rejuvenation YL-3, books GD-6, runes OR-7 (by item index, as runes have no misc id), ethereal GR-7. Ethereal is read first, so it supersedes the socketed gray on the floor (itemlabels.cpp yields to it), and the "Ethereal (cannot be repaired)" row is GR-7.

## Why runes never touched the floor

The auto-pickup runs at the end of every completed step and sweeps a radius of up to ten tiles. A rune that landed while the player fought in place, or mid-step, was gone at the first footfall - there was no frame in which to see it. `RespawnItem` now stamps every landing item with the tile the player is on, or is walking to (`position.future`, so finishing the step in progress does not count as the move). A rune is picked up only once the player stands on a different tile. Runes only: gold, potions, gems and jewels keep their instant pickup. The stamp is not saved; a level's items reload unstamped and the first step collects them as before.

## Tests

The kinds by misc id, the rune by index, ethereal over socketed, quality over kind, a plain item staying white. Suite 689/689; oracool.mpq repacked (419 files). The legend's Part 1 lists the five as in use.
