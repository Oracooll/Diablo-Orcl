# Affix correctness audit

**Date:** 2026-08-20 (against v1.8.96)
**Scope:** every delivered affix value on uniques and item sets, checked against its source package
and against what the engine actually does with it.

Follow-up to the "how many items lack affixes" count, which found none. That answered whether
affixes are *present*. This answers whether they are *right* - a different question, and one this
project has lost to three times ("never trust a package's IPL column").

## Result: no mismatches, in 852 checks

| Check | Count | Wrong |
|---|---:|---:|
| Unique numeric affix values vs package CSV | 480 | **0** |
| Unique special forms (damage ranges, speed tiers, signed light radius) | 65 | **0** |
| Set item stats vs the fifteen `set-data.json` files | 292 | **0** |
| Set stats of Power fidelity missed by the first pass | 15 | **0** |
| **Total** | **852** | **0** |

Coverage: 143 of 143 delivered uniques matched to the package by name; 94 of 94 set items matched by
id across all fifteen sets.

## Three layers, not one

A value can be right and still not work, so each layer was checked separately.

**Layer 1 - is the value transcribed correctly?** Package CSV / JSON against the generated `.inc`.
852 comparisons, all exact.

**Layer 2 - does the chosen `IPL_` mean what the token means?** Every one of the 20 codes the
unique mapping claims as *exact* was read against `SaveItemPower` rather than trusted:

- `IPL_LIFE` / `IPL_MANA` do `r << 6` internally, so the declared value is whole points. Correct.
- `IPL_GETHIT` **subtracts** (`_iPLGetHit -= r`), so the sets' `damage_taken_flat:-1` emitting
  `IPL_GETHIT, 1` is right, not a sign error.
- `IPL_LIGHT` reads `power.param1` rather than the rolled `r` - fine for uniques, where min == max.
- `IPL_SETAC` really does `item._iAC = r`, overwriting the base's armour. That confirms the
  `flat_armor -> IPL_ACP` substitution was necessary, not a shortcut.

**Layer 3 - is anything silently dropped?** The 21 set-stat tokens the value pass could not map all
turned out to be present in `item_set_stats.cpp` with an explicit fidelity - 8 live, 13 Inert with a
stated reason. Nothing falls through unrecorded. Items whose stats are *entirely* inert emit no
powers, as they should: 0 leaks.

## Two collision classes checked, both clean

`IPL_FIREDAM` and `IPL_LIGHTDAM` are **mutually exclusive** in `SaveItemPower` - each clears the
other's flag and zeroes its damage range. An item carrying both would silently lose one.

- Items carrying both: **0**
- Items carrying a duplicate of any *overwrite-style* power (`IPL_SETAC`, `IPL_SETDAM`, `IPL_SETDUR`,
  `IPL_FIREDAM`, `IPL_LIGHTDAM`): **0**

## The one thing worth a decision

**42 uniques carry `IPL_ACP` twice.** Not a defect - it is the documented `flat_armor -> IPL_ACP`
approximation landing on an item that *also* has a real `enhanced_armor_percent`. The two stack
additively, which is harmless arithmetically, but it has two consequences worth knowing:

1. Those items' "flat" armour is inflated into a percentage, so it scales with the base rather than
   adding a constant - a Gothic Plate benefits far more than a Cap does.
2. The item popup shows **two identical-looking `+N% armor` lines**, which reads as a rendering
   fault rather than as two affixes.

Merging the pair at generation time would fix both, and costs one line in the generator. Not done
here: it changes 42 items' stats, which is a balance decision rather than a correctness fix.

## Method note

The audit ran off the shipped packages, extracted fresh:
`Oracool.MPQ/02-source-art/unique-items/unique-item-expansion-250.zip` and the fifteen
`item-sets/set-*.zip`. No intermediate file was trusted.
