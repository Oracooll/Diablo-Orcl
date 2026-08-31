# One shelf mechanism, three shelves (v1.9.81)

Date: 2026-08-27
Version: 1.9.81
Tests: 562/564 (the two standing baseline failures)

The last three items on the shop list, plus the aura size the previous shrink got wrong.

---

## The auras: the number asked for is now the number drawn

> "shrink auras. they are not 2 tiles in diameter."

They were not. They were about five and a half, and the previous pass is why — it got the units
wrong twice over.

**First error: radius read as diameter.** "2-3 tile radius" was implemented as
`AuraVisualRadiusTiles` returning 2 or 3, which is a 4-6 tile ring.

**Second error: a projection factor that had outlived its job.** The file carried a sqrt(2) constant
because a world-space *circle* of radius R projects to an ellipse whose extreme point lies on the
diagonal. That was the correct conversion while the drawn ring stood in for the aura's actual reach.
It stopped being the reach in the previous pass — the ring became a mark on the character rather
than a map of the field — and from that moment the size is not derived from anything, it is simply
*stated*. A leftover projection factor on a stated number only makes it wrong by 41%.

So the function is `AuraVisualDiameterHalfTiles` now, returning 4 (two tiles) or 6 (three, once well
invested), and `BlitAura` scales to exactly that with no factor in between. A two-tile ring is 128×64
pixels, which is the 512px source at a quarter.

---

## The three curated shelves

The remaining items were **Rare tab**, **Set tab with an INI toggle**, and **Refresh on Basic, Rare
and Supplies**. They were one piece of work, and the reason to do them together was structural: the
unique shelf's behaviour was two file-scope variables and about twenty `case TalkID::SmithUniqueBuy:`
labels. **Copying that twice would have meant sixty labels and three places for the same stale-row
bug to be fixed in two of them.**

So the shelves are indexed instead:

```cpp
enum class CuratedShelf : uint8_t { Unique, Rare, Set, Count };
struct CuratedShelfState { Item items[CuratedShelfCapacity]; bool initialized; };
```

Everything that *differs* between shelves is a function of the index — the INI switch
(`HasCuratedShelf`), the tab name, the heading, and the generator. Everything that does not differ is
written once: the page-sized array, the buy-and-do-not-refill rule, the stale-row guards that three
separate audits put there, the scroll arithmetic. **Adding a fourth shelf is a row in each of three
functions.**

`HasSmithUniqueShop()` survives as a one-line call to the general form, so nothing that already asked
it had to change.

### What each shelf actually stocks

**Rare** rolls a base from `RndSmithItem` — Griswold's own pool at his own depth — then forces
`OracoolItemTier::Rare` through `SetupAllItems`. Two things follow from using his pool: the tab
offers the same *kinds* of gear the Basic tab does, so it reads as "the same shop, rolled hard"
rather than a separate item universe; and the pick goes through the replay-safe pool walk.

Two details that are load-bearing rather than stylistic:

- **`onlygood` is true.** Without it `GetItemBLevel` has a random component that can decide the item
  rolls no affixes at all, so a forced tier would silently not happen a large share of the time.
  This is the same reasoning `RetierOracoolItem` already records.
- **The loop is bounded by attempts, not successes.** A base that cannot carry tiered affixes is a
  legitimate miss, and `while (generated < 40)` over a pool that happens to be all misses would not
  terminate. The unique and set shelves cannot hang the same way because both draw from a *shrinking*
  candidate list; the rare shelf draws with replacement, so it needs its own bound.

**Set** walks the fifteen sets for pieces the character's level has earned, skipping the two slots
this fork has not built (relic, cloak) and skipping anything already on the shelf — without that last
filter a fifteen-set shelf is four copies of the same gauntlets often enough to notice. Each piece
gets `FinalizeSetPiece`, the same finish every dropped set piece gets, so a bought piece and a found
one are the same kind of object. **No ethereal roll**: that is drop-only, and a vendor handing over an
ethereal piece the player did not ask for is exactly the case `FinalizeSetPiece`'s own comment
declines.

**The createInfo stamp is a dungeon stamp, not a town one**, on both. This is the trap that has now
bitten this project twice — `CF_TOWN` is the one route in `RecreateItem` that re-derives an item's
index by replaying its seed through the droppable pool, so a town-stamped Oracool item comes back
after a reload as something else. That is what ate the Charms of Salvaging.

### Refresh

Free, on **Basic**, **Rare** and **Supplies**, behind one new INI switch (`Shop Stock Refresh`,
default on). Premium keeps its own older switch.

**Unique and Set deliberately have no Refresh, and that is a rule rather than an oversight.** Both are
drawn *without replacement* from a finite pool, so the shelf already holds every item the pool can
offer at that level. A Refresh on either would reshuffle the same contents and read as broken.

One thing worth knowing before pressing it: **Supplies is Pepin's four fixed potions plus Adria's
stock**, and only the second half is generated. So a Refresh there rerolls Adria — including what is
on her own Buy tab, because it is the same array.

`VendorStockLevel()` came out of `SetupTownStores` for this. A reroll has to regenerate a shelf at the
depth it was first built at, and the alternative — recomputing that walk at each call site — is how
two answers to one question come about.

### A bug caught while writing it

The back-out handler switched on `stextflag`, and `StartStore(TalkID::Smith)` *assigns* `stextflag`.
Reading it after the call asks which menu row to select for the screen we just left *for*, not the
one we came *from*. Captured into a local before the call.

---

## Verification

562/564. The eight save-format tests contend over shared files under `ctest -j`, so they report
spurious failures at concurrency; rerun serially they pass. The two real failures are the standing
baseline pair (`Drlg_l1.CreateL5Dungeon_diablo_3_844660068`, `Timedemo.WarriorLevel1to2`).

## To look at in game

The two new tabs are **off by default**, like the unique shelf. In `diablo.ini`:

```
Griswold Sell Rare Items=1
Griswold Sell Set Items=1
```

Then: seven tabs on Griswold's strip (Basic, Magic, Rare, Set, Unique, Supplies, Sold) — the strip
reserves nine slots, so they fit. Check the Set tab has no duplicates, and that Refresh appears on
Basic, Rare and Supplies but *not* on Unique or Set.

And the auras: two tiles across, three once you have five points in one.
