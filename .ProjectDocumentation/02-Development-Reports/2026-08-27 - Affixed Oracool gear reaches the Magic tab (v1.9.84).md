# Affixed Oracool gear reaches the Magic tab (v1.9.84)

Date: 2026-08-27
Version: 1.9.84
Tests: 562/564 (the two standing baseline failures)

Closes the gap opened in v1.9.83: making the Basic tab's Oracool gear plain left affixed Oracool
bases available only as monster drops, because the premium shelf rolls its bases through
`RndPremiumItem` — which walks the droppable pool Oracool items are deliberately excluded from.

Six of Griswold's thirty Magic slots are now affixed Oracool gear.

---

## Why a second pass, not a branch inside `SpawnOnePremium`

The obvious implementation is a roll inside `SpawnOnePremium`: sometimes pick an Oracool base
instead of a vanilla one, and let the existing `GetItemAttrs` → `ApplyVendorTier` → `GetItemBonus`
sequence do the rest. **That would have broken the save format**, and not in a way any test here
would catch.

`RecreatePremiumItem` replays `SpawnOnePremium`'s sequence verbatim:

```cpp
SetRndSeed(iseed);
_item_indexes itype = RndPremiumItem(player, plvl / 4, plvl);
```

A single extra `GenerateRnd` inserted between those two lines shifts the item's own random stream, so
every recreated premium item would differ from the one that was generated. The decision could have
been hashed off the seed instead — the pattern `ApplyVendorTier` uses — but a second pass over the
array touches none of that stream at all, and matches the three Oracool stocking helpers that already
exist.

## The array is split, not interleaved

The tail of `premiumitems` is the Oracool block; the vanilla roll and its rotation both work over the
head.

That is deliberate. The level-up rotation shifts a **contiguous run** left and refills its end — an
Oracool item caught inside that run would be aged out by a rotation with no way to replace it, and
the Oracool proportion would bleed away over a few levels. Splitting keeps the rotation exactly what
it was.

The Oracool block is cleared and rebuilt when the shelf's **depth** moves, so it keeps pace with the
character the same way the rotation does. Buying from it still refills the slot immediately, like
every other premium slot.

## Two flags that carry weight

- **`onlygood` is true.** Without it `GetItemBLevel` has a random component that can decide an item
  rolls no affixes at all — so a shelf that is supposed to be magical would come out part plain. Same
  reasoning `RetierOracoolItem` records.
- **`allowTieredRoll` is false.** These come out as ordinary magic items. The Magic tab promises
  affixed gear; Rare, Set and Unique each have a tab of their own to promise from, and a Primal
  turning up on the Magic shelf would blur all four.

The createInfo stamp is a bare level, never `CF_SMITHPREMIUM` — a town stamp is the route that
re-derives an item's index by replaying its seed, and it is what would bring the item back as
something else after a reload.

The depth gate is shared with the plain shelf now (`OracoolGearBasesFor`), so the two cannot come to
disagree about which bases a depth offers.

---

## Verification

562/564 — the two standing baseline failures and nothing else.

## To look at in game

Griswold's Magic tab: roughly a fifth of it should be Oracool bases (Shoulders through Spectral)
wearing blue affixed names. Compare against the Basic tab, where the same bases should now be plain
white.
