# Durability ran past zero and kept going (v1.9.96)

**Date:** 2026-08-27
**Version:** 1.9.96
**Trigger:** user report - "i have magic oracool items with negative durability. investigate and fix."

## The defect

Vanilla Diablo spends durability in seven separate places - four in `DamageWeapon`, two in
`DamageParryItem`, one in `DamageArmor` - and each one is the same three lines written out again:
decrement, test for zero, break. That duplication was safe in vanilla for a reason that is easy to
miss: **breaking an item REMOVED it from the slot.** The next wear tick found the slot empty, and
there was nothing left to decrement.

This fork changed that. `BreakOrRemoveEquipment` (`Source/inv.cpp:2054`) keeps a broken item
equipped and inert in single-player - it sets `_iDurability = 0`, raises `_iOracoolBroken`, and
leaves it where it is, so the player can see what broke and carry it to the smith. That one change
turned all seven copies into runaways, and four of the seven made it permanent by testing
`== 0` rather than `<= 0`:

```cpp
player.InvBody[INVLOC_HAND_RIGHT]._iDurability--;
if (player.InvBody[INVLOC_HAND_RIGHT]._iDurability == 0) {   // <-- only EXACTLY zero
```

An item sitting at 0 gets decremented to -1, the test does not fire, and every subsequent tick takes
it one step further down. Nothing ever catches it again. `DamageArmor` was the worst of them, because
its wear pool covers all eight armour slots and a broken piece stayed in the pool - so it kept
absorbing ticks that should have landed on gear that still had durability to lose.

The left-hand weapon branch used `<= 0` and so re-broke the item every tick instead, which is less
visible but re-ran the break autosave on every hit.

## The fix

One primitive, `WearDurabilityPoint(Player &, inv_body_loc)`, declared in `Source/player.h` and
defined in `Source/player.cpp`. All seven call sites now go through it. It:

- refuses to spend a point from an empty slot, an indestructible item, or an already-broken one;
- never decrements below zero;
- breaks **at** zero, not at exactly zero;
- clamps `_iDurability` to 0 on the way into the break, so a save carrying the old runaway heals the
  moment the item breaks again.

`DamageArmor`'s wear pool now also excludes broken pieces outright, so they stop soaking up ticks.

It is declared in the header rather than kept file-local specifically so the runaway can be tested
directly - the two public entry points are both file-local and both need a live `MyPlayer`.

## Three narrowing bugs found on the way

The investigation turned up three more places where an `int` durability field was routed through a
byte. None of them could produce a negative on their own, but two could produce a **zero** max
durability, which is where the runaway above then started from.

1. **`IPL_DUR_CURSE`** (`Source/items.cpp`) clamped with `std::max<uint8_t>(item._iMaxDur, 1)`. The
   template argument converts the int field to `uint8_t` *before* comparing, so a curse harsher than
   100% left `_iMaxDur` negative and -60 came back as **196** - a durability curse that handed out
   durability, with the clamp-to-1 it was written for never reached. Now `std::max<int>`.

2. **`IPL_DUR`** added its bonus with no ceiling. `_iMaxDur` is an int here but a byte in the packed
   item record (`pack.cpp`'s `bMDur`), and 255 is the "cannot break" sentinel. Capped at 254.

3. **`ApplyBaseTier`** (`Source/oracool/item_tiers.cpp`) passed `item._iMaxDur` - an int - into
   `ScaleByte`, whose parameter is a `uint8_t`. Anything already above 255 was truncated to its low
   eight bits on the way in, and exactly 256 arrived as a **zero**. Now `ScaleByPercent` with an
   explicit clamp to `[1, 254]`.

## Healing existing characters

Two clamps, because the user is mid-playthrough with items already reading "Dur: -37/60":

- `LoadItemData` (`Source/loadsave.cpp`) raises any negative `_iDurability` to 0 on read. It is
  placed there and not in `LoadAndValidateItemData`, because **the worn slots do not go through
  that** - `LoadMatchingItems` reads them with `LoadItemData` directly, and the worn slots are
  exactly where the runaway lived.
- `PackItem` (`Source/pack.cpp`) now clamps `bDur` at both ends. It is a byte, so a negative
  `_iDurability` would have wrapped to a large positive going in and come back out as a nearly-full
  item.

## Verification

Three new tests in `test/oracool_audit_test.cpp`, both defect tests confirmed non-vacuous by
reintroducing the exact original code and watching them fail:

| Test | Reintroduced defect | Result |
|---|---|---|
| `WornGearStopsAtZeroInsteadOfRunningNegative` | `== 0` + no broken-item guard | **failed** |
| `TieringAnItemWithHighDurabilityDoesNotZeroIt` | `ScaleByte(item._iMaxDur, ...)` | **failed** |
| `IndestructibleGearNeverSpendsAPoint` | - | passes |

Suite: **570/572**, the two standing baseline failures
(`Drlg_l1.CreateL5Dungeon_diablo_3_844660068`, `Timedemo.WarriorLevel1to2`) and nothing else.

## In game

Load the character that has the negative items - the durability numbers should read 0 rather than a
negative on the first load, and the smith will quote a repair price for them again. New breakage
should stop at 0 and stay there.

Not pushed: GitHub Actions minutes are exhausted until roughly 2026-09-01.
