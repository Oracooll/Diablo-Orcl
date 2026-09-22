# Four things a cleared rift got wrong — v1.12.169

2026-09-22

> when nephalem rift is completed:
> 1. text with countdown timer doesnt fit the second digit of the time remaining.
> 2. there is no hover text when hover over the portal that is opened - place a hover text Back to
>    town. Rift Closes.
> 3. Make the select area of this portal as big as the select (click) area of vanilla portal.
> 4. when i take the exit portal i spawn next to farhnam. spawn me next to the nephalem/guardian
>    rift monument.

Two of the four turned out to be the same omission.

## 1. The clipped countdown

The label was drawn in a box the width of the BAR, which is the mini-map's width, and centred in
it. `"Nephalem Rift: cleared - closes in 30s"` is wider than that, so `AlignCenter` put its start
left of the box and the clip took the tail — the second digit of the countdown, then the `s`.

The box now measures the text (`GetLineWidth`) and grows from the bar's centre to fit, clamped back
on screen if that would run it off an edge.

**Measured rather than padded by a guessed margin.** The label's length changes with the rift's
name, its percentage, its guardian's name and its clock; a margin that fits the longest of those
today is a margin that clips the next one.

## 2 and 3. One omission, two symptoms

`CheckRiftPortal` began with `if (leveltype != DTYPE_TOWN) return;`. It was written for the portal
that opens in the Stonegate, in town — so the portal a **cleared rift** opens had no cursor
handling at all.

- No hover text, because nothing set one.
- A click area a fraction of the sprite, because what little there was came from the return
  **trigger**, which is one tile.

Both fixed by giving the in-rift portal the same treatment: `EntranceBoundaryContains`, which is
vanilla's own test and the one `CheckTown` uses for the town portal three functions above it — the
portal's tile and the tiles its 96x128 sprite rises over. **Asking the same question is what makes
the two areas the same size rather than merely similar.**

The hover reads "Back to town" / "Rift Closes."

`cursPosition` is not moved onto the portal's tile the way the town-side branch moves it to the
gate's entry tile: here the trigger IS the portal's tile, and moving the cursor as well would walk
the hero to a tile he is already standing on when he clicks the near edge.

## 4. Farnham

`GetMapReturnPosition()` switches on `setlvlnum` and its `default:` returns
`GetTowner(TOWN_DRUNK)->position + SouthEast` — Farnham. A rift is a set level whose number is none
of the four quest cases, so it fell straight through to him.

Nothing was wrong with that default until a set level existed that the player walks into from
somewhere else entirely. It now returns the Stonegate's entry tile when the player is in a rift.

**Asked of the rift, not added as another `case`.** The rift's level number is chosen per kind
(`RiftLevelFor`), so a case list here would have to be kept in step with a table over in
`oracool/rift.cpp`.

## Files

- `Source/oracool/rift.cpp` — the label's box
- `Source/cursor.cpp` — the in-rift portal's hover and click area
- `Source/quests.cpp` — the return position

Built clean, 832/832. No asset changes.

## What to look at in play

1. Clear a rift and read the countdown — "closes in 30s" through "closes in 9s", all digits.
2. Hover the portal that opens: "Back to town" / "Rift Closes."
3. Click its top edge, its bottom edge and its sides — the whole sprite should take the click, the
   way a town portal does.
4. Take it. You should arrive at the Rift Monument, not beside Farnham.
