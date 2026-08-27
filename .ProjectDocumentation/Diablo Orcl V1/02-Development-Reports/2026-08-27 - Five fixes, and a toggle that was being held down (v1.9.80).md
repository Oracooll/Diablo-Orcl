# Five fixes, and a toggle that was being held down (v1.9.80)

Date: 2026-08-27
Version: 1.9.80
Tests: 562/564 (the two standing baseline failures)

Five items from the running list. Four are small; one turned out to be a genuine mechanism
misfiring, and it is the interesting one.

---

## 1. Recharge is a cursor now, like Repair

Repair got the vanilla-skill treatment in v1.9.79 — click the button, the hammer appears, click the
item. Recharge now works the same way, and for the same reason: the shop borrows `CURSOR_RECHARGE`
from the Recharge *skill*, so the cursor graphic, the click-an-item targeting and `TryIconCurs`'
routing across backpack, extra tabs and stash all come with it. Nothing is written twice.

The state that tells the skill's version from the shop's used to be a lone `bool
ShopRepairCursorArmed`. With two services it became an enum:

```cpp
enum class ShopServiceCursor : uint8_t { None, Repair, Recharge };
```

One value rather than two flags, because they are mutually exclusive by construction — there is one
cursor — and two independent bools would let a stale Recharge outlive a Repair click and charge for
the next thing the player touched. `DisarmShopServiceCursor()` clears the pair; it runs in
`InitStores` beside the buyback shelf and again immediately after each click.

`ShopRechargeItemAt` mirrors `ShopRepairItemAt` exactly, including the trick that makes both safe:
`RechargePriceFor` returns 0 for anything with nothing to recharge — not a staff, no charge slots,
already full — so the price doubles as the "is this a valid target" test. Clicking the wrong thing
costs nothing and does nothing.

Dropping an item on either button still works, unchanged. The cursor is what the button does when
your hand is empty.

## 2. The tooltips now describe the mechanic that exists

They said "Repair - drop an item here" and "Recharge - drop a staff here". That was the whole
mechanic when it was written and is now half of it, and a hint that describes one of two gestures
teaches the player the other does not exist. Both are four lines now — name, the cursor gesture, the
drop gesture, what it costs and what it restores. Repair All says what it actually does (dearest
first, until the gold runs out) and shows its price, or "Nothing needs repairing" when there is
none.

## 3. The magic shop's Oracool goods were being crowded out

Reported as "i still dont see oracool items in magic shop. i am lvl4 which shouldnt be a reason not
to." The level was not the reason. The arithmetic was.

The stocking helpers fill **empty slots**. They run after the vanilla roll. Adria's array is 45
slots; the vanilla roll took up to 38 of them and the Charms of Salvaging took 7 more — so on a high
roll there were **zero** slots left and the entire Oracool line silently vanished from the shelf.
Not rarely: on every high roll. Griswold had exactly the same hole (45 − 38 − 7 = 0), and Pepin a
milder version of it.

The fix is to make the reservation real rather than hopeful — three named constants, subtracted from
the vanilla item count *before* it rolls:

```cpp
constexpr int SmithOracoolCount  = SMITH_ITEMS / 3;
constexpr int WitchOracoolCount  = WITCH_ITEMS / 4;
constexpr int HealerOracoolCount = 5;
```

The shelf's composition is now decided up front instead of by a dice roll.

Worth stating plainly: at low character level the *candidate pool* is genuinely small, because these
items go through the same depth ladder (`BandedQlvl`) as everything else. At the level a fresh
character shops at, that is roughly the Topaz, the Skull and the El Rune. They will now be there —
but they will not be a wide selection until the character goes deeper, and that is the ladder
working, not a second bug.

## 4. The recipe book is 620 tall

Capped at 620px and centred in the band from the top of the screen to a 100px reserve above the
bottom. On the 720-tall screens this project targets those are the same number — 720 − 100 *is* 620
— so the book fills the band exactly and starts at y=0. They are written as two separate rules
anyway, because they are two rules: the reserve is about the HUD, the 620 is a size, and a screen
that is not 720 tall has to honour both instead of whichever one happened to be hardcoded.

The scrolling and the clipping were already there and are untouched.

## 5. The flashing at Levski's — the real find

> "fix some flashing which occurs when i click on levski. if i am next to him, i need to click a few
> times until the window remains open, instead of blinking and closing."

This was not the window. It was `RepeatMouseAction` in `track.cpp`, which re-sends `CMD_OPOBJXY`
**every frame the mouse button is held**:

```cpp
case MouseActionType::OperateObject:
    if (ObjectUnderCursor != nullptr && !ObjectUnderCursor->isDoor())
        NetSendCmdLoc(MyPlayerId, true, CMD_OPOBJXY, cursPosition);
```

That is safe for every object vanilla puts under it, because operating one **consumes** it — a chest
opens, a shrine spends itself, `_oSelFlag` goes to 0 and the repeats after the first are no-ops.
Doors were excluded because a door *toggles*.

Levski's Roar toggles too, and nobody excluded it. Holding the button for a fifth of a second
flipped the window open and shut several times over, so whether it ended up open came down to the
**parity of how long the click lasted**. Hence "click a few times until it stays".

And this is exactly why it only bit when standing next to him: from further away the walk eats the
hold, and the button is released long before the operate ever fires. The bug needed the player to be
adjacent to be visible at all — which is precisely how the report described it.

The exclusion is now `!isDoor() && !IsLevskiRoarObject()`, with the rule written down: **a repeat is
for operations that consume their object, never for ones that toggle.** `IsLevskiRoarObject` is the
same `currlevel == 0 && !setlevel && _otype == OBJ_STAND` test the graphics swap and the operate path
already make, given a name so this did not become a fourth copy of it.

---

## Verification

562/564 under `ctest -j 8` — with the caveat that the eight save-format tests contend over shared
files at that concurrency and report six spurious failures; rerun serially they all pass. The two
real failures are the standing baseline pair
(`Drlg_l1.CreateL5Dungeon_diablo_3_844660068`, `Timedemo.WarriorLevel1to2`).

## To look at in game

- Adria's Buy tab: gems, runes and jewels should be on the shelf on every visit now, not on lucky
  ones. Same for Griswold's rolled Oracool gear and Pepin's charms.
- Adria's Recharge button with an empty hand: the recharge cursor, then click a staff anywhere —
  backpack, an extra tab, the stash.
- Hover both service buttons and read what they now say.
- **Click Levski once, standing right next to him, and hold the button a moment.** That is the case
  that used to blink.
- The recipe book: 620 tall, top of the screen, scrolls with the wheel.
