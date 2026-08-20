---
date: 2026-08-20
version: 1.8.66
tags: [ui, hud, runewords]
---

# The Map Icon Opens the Runeword Book

User request: *"replace the automap button in the burger menu with RWBook. keep the icon, just
replace the function and the pop-up tooltip."*

## The swap

One row of `MenuEntries` in `hud_menu.cpp`:

```cpp
{ "Runeword Book", DoRunewordBookEntry, IsRunewordBookOpen },
```

Label, action and lit-state predicate, in place. **The icon is untouched** — `MenuEntries` is locked
to `menu_icons.png`'s row order, so the map graphic stays exactly where it is. That constraint is
usually the thing blocking menu work (it is why directive point 8 is still half done: *removing* an
entry shifts every icon after it and needs the sheet recut). Replacing one in place is the operation
the constraint permits, which is why this was cheap.

The label is the tooltip — `HudMenuEntryLabel` returns `MenuEntries[index].label` — so both moved
together by construction. "Runeword Book" rather than "RWBook": the shorthand was in the request,
but the tooltip has room and the other entries are all spelled out.

**The automap loses nothing.** TAB still opens it, and the burger entry was never the only way in.
`DoAutomapEntry` and `IsAutomapOpen` are deleted — one-liners, trivially restored.

## The part that was not in the request

The burger row and the book overlap.

The row sits just above the HUD plate (~y 595–628 at 960×720); the book is 944×616 anchored to the
mini-map's top edge, reaching to ~y 624. Worse than a cosmetic clash: `DrawHudMenu` runs *after*
`DrawRunewordBook` in scrollrt, and `diablo.cpp` routes `IsPointOverHudMenu` **before**
`HandleRunewordBookClick`. So an open row would draw on top of the book and eat every click in that
strip.

Fixed by closing the burger menu in `OpenRunewordBook`, next to the `CloseAllWindows` call that is
already there for the same reason. **In the book, not in the menu entry**, so it holds for the `W`
key too — opening the book with the row already up is just as reachable.

Two existing comments argue for leaving the row open: `CloseAllWindows` deliberately spares it, and
the row's click handler keeps it up so several panels can be toggled in one go. Neither argument
survives a window that is 944px wide on a 960px screen. `CloseAllWindows` itself was left alone —
making space bar close the burger row is a global behaviour change nobody asked for.

## Verification

484 tests, the two standing baseline failures only. No test covers this — the harness from 1.8.65
asserts on placed world objects, not on HUD widgets.

**This one needs eyes.** Open the burger menu and click the map icon: the book should open and the
row should vanish. Hover the icon first to check the tooltip reads *Runeword Book*. Then press `W`
with the row already open and confirm the same thing happens.

Wiki updated and the artifact republished — `ui.html`, `controls.html` (TAB is now the only way to
the automap) and `history.html`.
