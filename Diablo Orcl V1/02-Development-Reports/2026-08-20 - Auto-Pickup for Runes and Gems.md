---
date: 2026-08-20
version: 1.8.69
tags: [options, items, sockets]
---

# Auto-Pickup for Runes and Gems

Two INI booleans, both **on by default**, following `Auto Pickup Scrolls` exactly - the same four
sites, the same shape, so the block reads as one family rather than as three one-offs.

```
Auto Pickup Runes = 1
Auto Pickup Gems  = 1
```

## Why they are their own switches

The generic Misc branch in `DoPickup` keys on `_iMiscId`, and **both families are `IMISC_NONE`** -
they are identified by `IDidx`, the same test the item description and the socket code use. So they
could not have joined the existing switch even if that had been the tidier answer.

Routed through `IsOracoolRuneIdx` / `IsOracoolGemIdx` rather than an index range, so a rune added
later is collected without this line being touched.

Separate switches rather than one "Auto Pickup Socketables", because someone who turns scrolls off
to stop the clutter will still want these - a rune is never clutter, and walking back over one is
the commonest way to lose it.

## Backpack only

`AutoPlaceItemInInventory` alone, no `AutoPlaceItemInBelt` - unlike scrolls and potions, which take
both. A rune in a belt slot would be a hotkey that does nothing.

Both return false when the backpack is full, so a full pack leaves the stone on the floor rather
than silently eating it.

## Verification

486 tests, the two standing baseline failures only. Nothing here is testable headlessly -
`DoPickup` reads `MyPlayer` and the live options - so it wants a walk over a dropped rune with a
full pack and with an empty one.

The INI is written on first run after this build; existing `diablo.ini` files gain both keys with
their defaults on next save.
