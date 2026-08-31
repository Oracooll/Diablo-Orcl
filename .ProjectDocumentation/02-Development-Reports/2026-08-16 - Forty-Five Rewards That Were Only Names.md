# Forty-Five Rewards That Were Only Names

**Version:** 1.7.53
**Date:** 2026-08-16
**Files:** `Source/oracool/item_set_bonus_overrides.txt` (new), `item_set_stats.{h,cpp}`, `item_sets.{h,cpp}`, `Source/items.{h,cpp}`, `tools/GenItemSets.ps1`, `test/oracool_audit_test.cpp`

---

## What started it

> i was indeed wondering about these set affixes. they sound strange.

They sounded strange because they were. Counting them:

```
set                                  rungs  live inert
Vestments of the Ashen Saint             5     4     1
Graveglass Regalia                       4     3     1
Stormcrow Harness                        5     3     2
Steps of the Empty Hand                  6     3     3
Panoply of the Iron Root                 6     2     4
Raiment of the Starless Hour             4     2     2
Wyrmhide Arsenal                         6     2     4
Leoric's Fallen Court                    7     2     5
Works of the Clockwork Penitent          6     1     5
Dawnwarden's Reliquary                   5     1     4
Black Orchard Vesture                    5     1     4
The Crimson Compact                      4     1     3
Choir of Silence                         4     1     3
The Rat King's Tithe                     3     1     2
The Lost Cartographer                    3     1     2
TOTAL                                   73    28    45
```

Forty-five rungs compiled to an empty power list. Not approximately nothing — nothing. A player assembling four pieces of the Ashen Saint earned "Cinderbrand", watched it turn green, and received no stat of any kind.

> i say we substitute nonsense affixes with something more real and substantial. ok?

---

## Why they were empty

The delivered design is ambitious in a direction this engine does not go. Its ladders are built on state machines: a Wyrmhide temper that cycles ember/storm/rime, a Crimson Compact petition counter, a Black Orchard blight stack, an Empty Hand stance-and-flow system, a Lost Cartographer route tracker, and thirty-one distinct `proc:` effects whose *name was the entire specification*.

The generator refused all of it, which was right — inventing a number for `proc:cinderbrand` would have put a lie in the data. But refusing left the rung as a name with nothing behind it, and the tooltip then coloured that name green when earned, which reads as a reward.

Three more rungs were worse than empty, because they compiled to *something*:

| Rung | What survived |
|---|---|
| **(3) Economy of Night** | `-1 light radius`, and nothing else — a third piece was a pure punishment |
| **(7) The Mountain Remembers** | `+15 life`, as the capstone of a seven-piece set |
| **(2) Deep Foundation** | `+12 armour` that was worth **+1** — see below |

---

## Two engine bugs found on the way

### `enhanced_armor` was armour piercing, and under Hellfire it was a bit shift

The keyword table mapped `enhanced_armor` to `IPL_TARGAC` on the strength of the name. `IPL_TARGAC` writes `_iPLEnAc`, which is **armour piercing**, and `Player::CalculateArmorPierce` uses it under Hellfire as a shift:

```cpp
int pIEnAc = _pIEnAc - 1;
if (pIEnAc > 0) tmac >>= pIEnAc;
```

Sixteen of the ninety-four items declare `enhanced_armor` at 10–20. At 15 that is `tmac >>= 14` — monster armour erased outright. Sixteen items were granting a total armour-ignore while their tooltip promised defence.

The irony is that the old test pinned the mistake, with a comment warning that getting the pair the wrong way round meant "nothing would look broken." That was exactly the problem. It rides `IPL_ACP` now.

### Armour on a bonus rung was worth exactly 1 point

There is no flat-armour power in this engine at all. `IPL_ACP` is a *percentage of the item's own armour*, and `ItemBonusTotals::AddItem` computes it as `item._iAC * item._iPLAC / 100`. A set bonus reaches the player through a **scratch item** with no armour of its own, so that is `0 * 12 / 100 = 0` — and the sign fallback just below turns 0 into 1.

So `armor_class_flat:+12` on a rung granted one point of armour, and every armour rung I was about to author would have granted one point too. `ApplySetBonusesToTotals` now reads a rung's `IPL_ACP` as flat armour and adds it directly, which is the only reading available and the one the data means.

---

## The re-authoring

`Source/oracool/item_set_bonus_overrides.txt` — 54 rungs, written in the **same keyword vocabulary as the delivered JSON** and resolved through the same table. That is the whole design: an override naming an inert keyword is a hard error in the generator, so this file *cannot* reintroduce the problem it exists to fix.

```
SET_ID | pieces | keyword:value, keyword:value, ...

SET_ASHEN_SAINT | 4 | fire_damage:[4,12], chance_to_hit:+10
```

Each replacement is fitted to what the rung is **called**, because the names were the only part of the design that survived contact with the engine:

- **Cinderbrand** — a burning brand — is fire damage and accuracy.
- **Take Root** is vitality and life. **Bark of Ages** is armour and resistance. **Bastion Within** is damage reduction and fast block.
- **Teeth of the Hour**, in the clockwork set, is lightning damage and thorns.
- **Returning Malice** declared `spell_reflect`, which has no hook — it is thorns, the engine's own version of harm returned to whoever dealt it.
- **Reaping Mercy**, a harvest-heal proc, is life steal.
- **Every Threshold Has a Name** was about doors and traps, and `IPL_ABSHALFTRAP` was sitting unused in the enum — the closest any delivered keyword came to landing on its feet.
- **The Murder Descends**, the capstone of a *bow* set, now fires **multiple arrows**.

`Economy of Night` keeps its `-1` light radius, because seeing less is that set's whole conceit. It is now paid for.

### Nine keywords added

Channels the engine already had and the table had simply never named: `all_attributes`, `armor_vs_demons`, `armor_vs_undead`, `fire_arrows`, `lightning_arrows`, `multiple_arrows`, `half_trap_damage`, `life_steal`, `mana_steal`. Adding a keyword is the intended way to widen the palette; inventing a number for an inert one is not.

### Two engine rules the authoring had to respect

- **Steal does not stack.** `player.cpp` assigns `skdam = 5 * dam / 100` *over the top* of the 3% case rather than adding, and the flags only exist at exactly 3 and 5 — any other value silently does nothing. So a ladder carries at most one steal rung, or a deliberate 3→5 upgrade where the higher supersedes the lower. The generator now refuses any other value.
- **Flags carry no magnitude.** Thorns, knockback, half-trap, multiple arrows and triple-damage-vs-demons are presence-only; granting one twice in a ladder grants it once.

---

## The guards

The point of this pass is not that 45 rungs are fixed. It is that the failure cannot recur silently:

| Guard | Catches |
|---|---|
| Generator: rung with zero live powers | a named reward that grants nothing — **build failure** |
| Generator: override naming an inert keyword | writing the bug back by hand |
| Generator: override matching no rung | a typo'd set id, which would otherwise fail *silently* — the rung stays empty and the file looks like it covers it |
| Generator: `IPL_INDESTRUCTIBLE`/`NOMINSTR`/`SETDAM`/… on a rung | powers that are no-ops on a scratch item |
| Generator: steal value not 3 or 5 | a stat that reads real and does nothing |
| `EveryBonusRungGrantsSomething` | the same invariant, checked against the shipped table |
| `EverySetBonusStatHasText` | a stat that grants something but *renders* as blank |
| `ArmorOnABonusRungIsWorthItsDeclaredValue` | the +1 collapse returning |

The old `BonusLadderPicksTheHighestRungReached` asserted that Cinderbrand was inert, with a note saying "if that is deliberate, update this test." It now asserts the opposite.

---

## The tooltip says what a rung does

The other half of "they sound strange": the panel showed a rung's *name* and never its effect. Even the 28 live ones. "Warmth of the Reliquary" never mentioned +15% fire resist and +5 vitality.

New `PrintSetBonusPower(const ItemPower &)` — neither existing printer fits, because `PrintItemPower` reads an item's accumulated fields (a rung has no item) and `PrintOracoolAffixPower` takes a single magnitude (so it cannot say "5-10 fire damage").

Its stats are joined onto **one line** under the rung name rather than a line each, deliberately: Leoric's thirteen-piece ladder — 13 item rows plus 7 rungs — would otherwise be taller than the screen. Hence the terser phrasing, `+15% fire res` rather than `Resist Fire: +15%`.

```
  (2) Warmth of the Reliquary                     green
      +15% fire resist, +5 vit                    green
  (4) Cinderbrand                                 red
      4-12 fire damage, +10% to hit               red
```

---

## Verification

- Debug build clean at 1.7.53.
- Generator: **520 live stat lines** (was 390), **39 inert** (was 150), and all 73 rungs non-empty.
- Full suite **441 of 443** — three new tests added, and the two failures are the standing baseline pair (`Drlg_l1.CreateL5Dungeon_diablo_3_844660068`, `Timedemo.WarriorLevel1to2`), unchanged in identity.

---

## Still open

- The **Rat King's Tithe is about gold**, and `gold_from_monsters` has no `IPL_` to ride — `Player::_pGoldFind` exists and charms feed it, but nothing writes it from an item. Wiring one is small and would restore that set's whole identity.
- Five of Leoric's thirteen items have no droppable base, so its 10/12/13 rungs are unreachable in play. Authored anyway, so the ladder is whole when the items arrive.
- 71 delivered keywords remain inert. They are listed in `item_set_stats.cpp` with a note saying what building each would take; the state machines behind them are a content project, not a gap.

---

*Supersedes the closing note in [[2026-08-16 - The Whole Ladder, Not Just the Rung You Are On]], which described Cinderbrand as inert. It was, at 1.7.52. It is not now.*
