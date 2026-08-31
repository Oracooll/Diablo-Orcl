# Jewels, the Third Socket Family

**Version:** v1.9.8 → v1.9.9
**Date:** 2026-08-21
**Tests:** 492/494 (the two standing baseline failures: `Drlg_l1.CreateL5Dungeon_diablo_3_844660068`, `Timedemo.WarriorLevel1to2`)

## What shipped

Fifteen jewels — five families across three grades — join runes and gems as the third thing that
goes into a socket.

| Family | What it gives | Weapon / Shield / Armour |
|---|---|---|
| Fervor | to-hit | to-hit / AC / to-hit |
| Focus | mana | mana in all three |
| Aegis | armour class | damage / AC / AC |
| Ruin | damage | damage / AC / strength |
| Warding | magic resistance | to-hit / resist / resist |

Grades scale one set of numbers: **Flawed 60 %, Plain 100 %, Radiant 160 %**, at qlvl 6 / 14 / 24
and 800 / 2000 / 4800 gold. They drop at **1 %** off the same hook the other socketables use, gated
by `BandedQlvl` against the area level, so Radiants do not appear in the Church.

They stack, they auto-pick-up with gems, and SORT lands them in a block of their own on the material
page: three rows directly above the gems, one row per grade, one column per family.

## The decision that shaped this

The Pipeline row asked for *"a socketable with rolled affixes rather than a fixed effect"* — a jewel
whose stats are rolled per instance, the D2 behaviour. **That is not what shipped, deliberately.**

`Item::_iSocketed` stores a socket's contents as a bare `uint16_t` base index. There is nowhere in
that to put a roll. Per-instance affixes would have meant a new per-socket record in the item
extension, which is save format — `OracoolItemFormatVersion` would go to 9 and every existing hero
would need a migration path, for a feature whose value is variety rather than depth.

Fifteen fixed indices buy most of that variety for **no save change at all**. `Writehero.pfile_write_hero`
passing untouched is the proof. If rolled jewels are wanted later they can be added as a fourth
family beside these rather than instead of them, and the save cost gets paid then, once, knowingly.

## Where a new socketable falls out of the game

Three of the four wiring sites below were found by re-reading the audit suite rather than by
anything failing. All three would have built green:

1. **`GetItemIndexForDroppableItem`** (`Source/items.cpp:1632`) — the seeded pool is save format;
   `UnPackItem` replays a dungeon item's seed through that exact walk to recover its index. A family
   that joins the pool re-routes every seeded recreation. Jewels are excluded and drop through their
   own hook, like every Oracool family before them.
2. **`Source/qol/stash.cpp`** — without a routing line, SORT files jewels among the swords on page 0.
3. **`Source/qol/autopickup.cpp`** — jewels ride the existing gem toggle rather than getting a fourth
   option nobody would find.
4. **`Source/oracool/salvage.cpp`** — jewels were already refused *incidentally*, by being
   `ILOC_UNEQUIPABLE`. Incidental is not the same as pinned: the refusal is now explicit and tested,
   because a salvage-all that eats a hoard of Radiants is the one bug this system must not have.

## The new test

`OracoolAudit.JewelsAreAWholeSocketFamily` holds four things: fifteen ids, a cursor each that
nobody else shares and that lands inside the icon strip, an effect in at least one of the three
hosts, and a refusal from salvage. It also asserts the five family predicates are **disjoint** —
the pool exclusion is one OR-chain of hand-written id ranges, and every family so far has been a
range appended after the last one.

The gems' own rule is kept: unlike a rune, a jewel is allowed to do nothing in *some* hosts. Doing
nothing in all three is what the test forbids.

## Assets

`tools/GenJewels.ps1` draws the fifteen as faceted lozenges — visually distinct from the salvage
orbs and the rune tablets — and emits all seven generated files from one walk, so ids, table rows,
cursor ids, frame sizes and icon specs cannot drift from each other. The icon strip went **462 → 477
frames**; `tools/build_oracool_mpq.cmd` has been run, so `oracool.mpq` is current.

## What to look at in game

- Kill things on a mid-depth floor until a jewel drops; check the name, the icon and the value.
- Socket one into a weapon, a shield and a piece of armour — the socket line should read differently
  in each, and none of the three should be an empty list after the colon.
- Press SORT in the stash: the jewels should form a 5 × 3 block sitting directly on top of the gem
  columns, worst grade at the top.
- Try a salvage-all with jewels in the backpack. Nothing should be consumed.
