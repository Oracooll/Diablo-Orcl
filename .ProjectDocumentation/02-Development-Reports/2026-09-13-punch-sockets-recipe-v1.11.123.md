# Punch Sockets: a recipe that sockets any wearable item

2026-09-13 — v1.11.123

## Why

> "add a recipe in the game and in the recipe books in levski and in the burger menu - Punching Sockets in Items
> - 1Pgem per socket. Number of socket = number of 28x28px grid the item asset is made of (1-6). Pgems are
> consumed in the process. All wearable items are eligible for socketing, no matter the type or tier."

Sockets only ever arrived as a drop roll, on plain gear and jewellery. A found rare, unique, set piece or primal
could never take a stone.

## The recipe — number 17, crafted at Levski's Roar

**1 wearable item + 1 perfect gem per socket → sockets up to its size.**

| Rule | How |
|---|---|
| Eligible | any **unsocketed** item with a place on the paper doll — weapons, armour, helms, shields, rings, amulets and the six worn slots — **of any type or tier** |
| Already socketed | **refused**, even with room for more (user, the same day: *"the item being socketed must not have sockets"*) |
| Sockets | the item's whole footprint in 28×28 cells (1–6), `MaxSocketsForItem` — the same rule drops already follow |
| Cost | one perfect gem per socket, any gem type, mixed stacks allowed; consumed exactly, surplus left in the grid |

The unsocketed rule also covers a **completed runeword**, which always has sockets and whose word would be unmade by
a changed socket count.

## The two books

Nothing to wire. Levski's recipe book and the burger menu's Crafting window both walk `CraftingRecipeCount` and read
the recipe's name and inputs line from the one table, so raising the count to 18 and adding row 17 lists it in both:

- **Punch Sockets** — *1 wearable item + 1 perfect gem per socket -> sockets to its size, 1 per 28x28 cell (1-6)*

## How

`oracool/crafting.cpp`:
- `SocketsToPunch(item)` — wearable test, runeword refusal, `MaxSocketsForItem − _iSocketCount`.
- `FindGridPunchTarget`, `FindGridPerfectGems` (stack-unit counting across any gem types).
- `GridMaterialsFor` case 17: the item, then exactly enough perfect gems.
- `TransmuteLevskiGridWith`: its own branch — the count is per item, not a fixed reagent — sets the socket count,
  marks the new sockets empty, normalises, and consumes one gem per socket through `ConsumeGridReagents`.

It changes the item in place and produces nothing, so no room check applies.

## Tests

`OracoolAudit.PunchSocketsTakesOnePerfectGemPerSocketUpToTheItemsSize` — one gem short is refused; mixed perfect gems
punch the footprint's worth of empty sockets and consume exactly that many; a full item is not offered again; a
primal is eligible; a pile of perfect gems with nothing to socket is not a recipe. The existing recipe-table tests
(every recipe named, distinct, not runnable on an empty grid) cover row 17 too.

## For the user to look at

At Levski's Roar: put a rare helm and four perfect gems in the grid, select Punch Sockets, transmute — the helm has
four empty sockets and the gems are gone. The recipe is listed in Levski's book and in the burger menu's Crafting book.
