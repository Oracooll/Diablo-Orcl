# Shops close on walkaway, Adria fills her grid (v1.9.87)

Date: 2026-08-27
Version: 1.9.87
Tests: 563/565 serially (the two standing baseline failures)

---

## Vendors close when you walk away

A shop is a PANEL in this fork rather than a modal screen — the inventory sits open beside it so
items can be dragged across to sell, repair and recharge. The cost of that, unnoticed until now, is
that the player can walk off with the shop still up: a third of the screen covered, swallowing every
click in its rect, following them around town.

Checked once a game tick in town, after the player has moved so the distance is this tick's.

**The towner is derived from the SCREEN, not from `talker`.** That global is only written by the
gossip paths, so for most shop screens it is stale and names whoever was spoken to last — exactly the
wrong thing to measure a distance against.

**Sub-screens follow their parent one step.** Confirm, No money, No room and Gossip belong to
whatever raised them, which `stextshold` still remembers; asking about the sub-screen alone would
answer "no towner" and leave a confirmation dialog floating after its shop had closed. One step, not
a walk — following the chain recursively would hang the game outright if two screens ever pointed at
each other.

**Five tiles, against the two `TalkToTowner` needs to open a shop.** The gap is deliberate: a
threshold equal to the opening one would slam the shop shut on a single step taken by accident.

It closes rather than backing out to the vendor's dialog — the player has left the counter, and a
dialog they did not ask for is no better than the shop they did not ask to keep. Not via `StoreESC`,
which walks a screen back to its parent and re-opens it; the one piece of state that would have
cleaned up is the service cursor, so that is disarmed explicitly. A hammer left armed by a shop the
player has walked away from would repair the next thing they clicked and charge them for it.

## Adria fills the grid

Her shelf was forty-five items against a 160-cell grid, and her wares are small — a potion or a rune
is one cell, a book four, a staff six — so most of it sat empty. Ninety now, **over-supplying the
grid on purpose**: `PlaceStock` lays out what fits and the remainder was never on the shelf, so the
shelf ends where the page does.

The reserved blocks became COUNTS rather than fractions of the array. They were fractions while the
array *was* the shelf; it is not any more, so a fraction would grow every block the next time the
array does — which is not what "twelve gems" means.

**Books and staves are guaranteed rather than hoped for.** Both were already reachable through her
ordinary roll, which is exactly the problem: a bad draw left her with neither. Ten books, eight
staves, five rare staves.

One helper serves all three blocks, because they differ only in what they ask for: books take no
affixes (a book *is* its spell), plain staves take the ordinary roll, rare staves force the tier with
`onlygood` set — which is what makes a forced tier actually land rather than silently not happen a
large share of the time. Three near-copies would be three places for the retry bound to drift.

Two guards inside it worth naming. The per-slot attempt count is bounded, because a forced tier can
miss and an unbounded "keep trying" spins forever on a depth with no valid candidate. And the TYPE of
what the pool returns is checked: `GetItemIndexForDroppableItem` hands back whatever its static
scratch array held when it finds nothing, and checking the type turns that into "this depth has none
of these" rather than a silently wrong item on the shelf.

## Two bounds moved into the functions that need them

Raising `WITCH_ITEMS` made me look at what else assumes a stock size. `storehold` is 48 entries and
the literal 48 appeared in five places — two array bounds and three caller-side guards. It is
`StoreHoldCapacity` now, and `AddStoreHoldRecharge` and `AddStoreHoldRepair` check it themselves.

`AddStoreHoldRecharge`'s first caller — the equipped weapon, added before the bounded loop starts —
never checked at all. It is safe, but safe because `storenumh` is zero at that point, which is a fact
about the caller rather than a property of the function.

## To look at in game

- Open any vendor, walk five tiles away: the shop closes.
- Adria's Buy tab should be visibly fuller, with a block of books and a block of staves, some of
  them rare-tier.
