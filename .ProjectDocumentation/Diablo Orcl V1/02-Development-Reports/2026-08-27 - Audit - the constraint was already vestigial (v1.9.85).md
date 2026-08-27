# Audit: the constraint was already vestigial (v1.9.85)

Date: 2026-08-27
Version: 1.9.85
Tests: 563/565 serially (the two standing baseline failures)

Re-audit under a new standing rule (user, 2026-08-27): *"i dont care about preserving sdave. i care
about robust coding. reaudit your code and make it fine and robust even at the expense of save
breaking."*

---

## The finding, and a correction to my own claim

I went looking for the biggest thing the save-compatibility constraint had been distorting. **"The
droppable-item pool IS the save format"** has shaped this fork's item code for months: it is why
Oracool items are excluded from the shared pool, why there are four parallel "hook beside the pool"
stocking helpers, why `ReplayScope`/`PoolQlvl` answer differently during a replay, and why every new
vendor function I have written carries a paragraph about never using a town stamp.

I traced it and found the load path is this:

```
UnPackPlayer(pkplr, player);     // replays every item from its seed
LoadHeroItems(player, saveNum);  // reads a COMPLETE stored record over the result
```

I then wrote a test planting the historically destructive combination — an Oracool index wearing a
`CF_SMITH` stamp, exactly the Charm-of-Salvaging case — and asserted the charm survives a hero round
trip. **It passed. I then reintroduced the guard I had just removed, and it still passed.**

So my test was vacuous, and my initial diagnosis was wrong. The reason is worth stating precisely:
the guard was `unpackedItem._iSeed != heroItem._iSeed`, and **every `Recreate*` path copies the seed
across verbatim**, so it could never fire for a non-empty item. The stored record was already
winning, for every item, always.

**The correction:** in V1 single-player the seed replay is a *fallback* for when there is no stored
record, not a gatekeeper for one. The pool walk still governs the multiplayer wire format and
`pack_test` — which is why the exclusions stay — but it decides nothing that survives a load. The
constraint I have been coding around for weeks was already vestigial.

I have corrected the two comment blocks that assert the falsehood, including the canonical one the
other helpers point at. A wrong comment in a load-bearing place is worse than no comment: it is why I
wrote three more copies of the same warning.

### What actually changed in `LoadMatchingItems`

Single-player now assigns the stored record unconditionally. Of the three guards removed, one was
dead (the seed check, above), and one could genuinely discard a good record: `unpackedItem.isEmpty()`
fires when the replay clears the slot, and the complete record — sitting on disk — was thrown away
with it. Ears still take the packed copy in both modes, because an ear carries its owner's name in
fields the item record has no room for.

**I have not demonstrated a live bug this fixes.** It removes dead code and an architectural
inversion; I am not going to dress that up as a crash fixed.

---

## Defects found in my own recent code

**Seven unchecked `*optional` dereferences.** Every call site sits inside a
`case TalkID::Smith{Unique,Rare,Set}Buy:` label, so none can fail today — but they were safe by
*coincidence of those labels*, and a fourth shelf added to one switch and not to `CuratedShelfFor`
would turn all seven into undefined behaviour at once. Now a wrong shelf plus a debug assert.

**`ShelfItems` was unbounded.** The index comes from a TalkID mapping, and a mapping can be wrong;
one shelf past the array is a silent walk over whatever statics follow it — the same shape as the
premium-stock overrun this file already had once.

**The Set shelf identified pieces by their translated display name.** It compared `_iIName` against
`_(def.name)` to decide "already on this shelf" — depending on the display string, on translation,
and on `MakeSetItem` continuing to write exactly that string. It now compares the address of the row
in `ItemSetItems`. Comparing what a thing *is* beats comparing what it is called.

**`OracoolGearBasesFor` filled a caller-supplied raw pointer** with no way to say how much room it
needed; both callers sized their buffer at `IDI_LAST + 1` "to be safe". Returns a sized container now.

**`numpremium` could overstate the shelf.** `numpremium = maxItems` was always a claim rather than a
fact, and with a second stocking pass that can place fewer than asked it would say thirty over a
shelf of twenty-four. Nothing iterates it — which is exactly why it was worth closing before
something starts to.

**`CreateRareVendorItem` had `SetRndSeed(AdvanceRndSeed())`** before its base pick: reseeding the
stream from itself, determinism theatre. The item's own generation is seeded separately.

**The tab column lost its capacity assert** when the tabs moved out of the panel. The three-row strip
had one; the failure it guards is silent (an eighth tab would just draw over the world). Restored as
a tripwire.

---

## Verification

563/565 run serially. The two failures are the standing baseline pair
(`Drlg_l1.CreateL5Dungeon_diablo_3_844660068`, `Timedemo.WarriorLevel1to2`). Under `ctest -j` the
save-format tests contend over shared files and report spurious failures.

The new test is kept and **labelled as characterisation, not regression** — it pins the contract that
the stored record wins, and its comment records that it passes with or without the guard, and why.

## What I did not do

The four parallel stocking helpers, the pool exclusions and the `ReplayScope`/`PoolQlvl` split are
all still there. Now that the constraint is known to be narrower than advertised, they could be
collapsed — Oracool items could simply join the droppable pool. **That is a loot-balance change, not
a robustness one:** it would alter drop rates everywhere and double up with the dedicated drop hooks.
Worth doing deliberately, not as a side effect of an audit.
