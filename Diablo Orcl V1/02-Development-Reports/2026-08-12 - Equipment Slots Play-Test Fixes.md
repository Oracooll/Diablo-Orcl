---
title: 2026-08-12 - Equipment Slots Play-Test Fixes
date: 2026-08-12
tags: [dev-report]
summary: First play-test of the six new slots found three real bugs - a stack-smashing crash on equip, the new types unable to carry affixes, and invalid-packet spam from the set commands - plus icons so holey they read as transparent.
---

# Equipment Slots Play-Test Fixes

The user ran the give*set commands and tried the new slots. Four findings, three of them bugs in yesterday's work.

## 1. The crash: a 7-entry table indexed to 12

Equipping anything into a new slot crashed the game; so did equipping a plain magic amulet.

`DrawInv` had a hand-written `slotSize[]` table with exactly seven entries, indexed by a loop that now runs to twelve. Reading `slotSize[7..12]` walks off the end of a stack array into whatever sits beyond it, and those garbage `Size` values feed straight into `InvDrawSlotBack`'s fill - a wild write. That is also why the *amulet* crashed: equipment changes auto-save, so the character reloaded with a new-slot item still equipped, and every subsequent inventory draw - including the one right after equipping the amulet - hit the same out-of-bounds read. A smashed stack takes the next innocent thing down with it.

The table is now derived from `inventory_layout.h`'s slot rects, the same source `slotPos` already uses - total by construction, so a fourteenth slot cannot reintroduce this. The audit lesson repeats: the scaffolding pass hunted `switch`es with `default:` catch-alls but missed a bare array with a hand-counted length. Both are the same defect wearing different clothes.

## 2. No magic/rare/tiered versions: the affix-eligibility map

`giverset` produced Rare versions of the seven vanilla slots and *plain* versions of the six new types.

`GetAffixItemTypeForItem` is the map every affix roll consults, and the six new types fell through to `AffixItemType::None` - "cannot carry affixes at all", the same bucket as gold and potions. The tier generators then correctly fell back to a plain roll. They now return `Armor`, exactly like the helm they sit alongside; no affix table changes were needed.

## 3. The invalid-packet spam: item level vs. validation

Every spawn produced "Player sent an invalid packet". The set spawner stamped items with the character's raw `_pLevel`; the spawn message loops back through the network layer even in single player, and `IsDungeonItemValid` rejects any item level above 30 that matches no monster's level. A high-level character fails that on every item. The spawner now clamps to 30, the highest level valid everywhere. (The other give commands never hit this because they roll a random monster level and filter with `WouldSurviveNetworkValidation`.)

## 4. The transparent icons: brightness cannot separate dark from dark

The icons were riddled with holes because the cutter dropped every pixel below a brightness of 55 - and dark leather *is* below 55.

Brightness cannot separate a dark item from a dark backdrop. Connectivity can: the backdrop is one contiguous dark region touching the crop's edges, while an item's dark interior is enclosed by its brighter silhouette. The cutter now removes the backdrop with a flood fill inward from the edges, at full source resolution (so scaling blends real alpha into soft edges instead of blending backdrop into the silhouette), then cuts on alpha.

Two measured constants, not guesses: the fill threshold is 30 (the sheets' canvas is luma 3-17, item interiors start ~70; a first attempt at 48 leaked through shadowed silhouette edges into the items), and opaque islands under 60 source pixels are removed - the canvas texture has bright specks the fill correctly walls around, which otherwise survive as floating dots.

Opaque pixel counts roughly doubled per icon (gloves 758 to 1016, legs 536 to 1427, belt 258 to 677).

## Verification

Debug build clean at `ORACOOL_VERSION` **1.1.14**, tests **347/349** (same two pre-existing failures), `oracool.mpq` repacked with the re-cut sheet.

Needs the same play-test again: give*set at each tier (the set should now come out Magic/Rare/Unique/Primal across all thirteen), equip and unequip every new slot, and a look at the icons in the panel.

## Related

- [[2026-08-12 - Six New Equipment Slots]]
