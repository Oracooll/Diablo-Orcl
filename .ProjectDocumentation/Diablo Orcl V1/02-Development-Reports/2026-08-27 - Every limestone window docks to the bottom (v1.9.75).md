# Every limestone window docks to the bottom (v1.9.75)

**Date:** 2026-08-27
**Version:** 1.9.74 → 1.9.75
**Tests:** 564, of which 562 pass — the two standing baseline failures, unchanged.

---

> "New game-wide rule - All limestone windows to be docked flush with the bottom of the screen
> instead of the top."

## They had no shared rule at all

That is the finding, and it is more of a change than the request sounds like. Each window had picked
its own anchor:

| window | was anchored to |
|---|---|
| Levski's Roar | one third of the way down the screen |
| Levski's recipe book | whatever the Roar window's top happened to be |
| Runeword book | the mini-map's top edge — a top-**right** anchor |
| Waypoint menu | `{ 0, 0 }` — the screen's top-left corner |
| Skill picker | the HUD plate, growing upward — **already** bottom-docked |

Five windows, four different ideas about where a window belongs. The skill picker had it right by
accident: it was anchored to the button it belongs to, which happens to sit at the bottom.

## One rule, asked for rather than repeated

`oracool::BottomDockedTop(height)` lives beside `PanelTitleTop` in `ornate_border.h`, which is
already the file every limestone window includes for its title band. The windows ask it; none of them
decides for itself any more.

Bottom is the right edge to share because **it is the one the HUD already owns** — the side panels
end on it, the belt and the orbs sit against it. A floating window that lines up with them reads as
part of the same furniture rather than hovering over the world at a height of its own.

Clamped at zero, and in that direction deliberately: a window taller than the screen is cut off at
the **bottom**, not pushed off the top, because the top is where the title band and the close button
live. Losing the way out of a window is worse than losing its last row.

The recipe book now docks in its own right rather than copying the Roar window's top edge. The two
are different heights, so sharing a top left their bottoms ragged — which did not matter when nothing
else had a shared edge, and does now.

---

## Still queued — four items

The Rare tab; the Set shop; the hammer-cursor Repair rework; and Refresh on Basic/Rare/Supplies.

Also outstanding from the last instalment: Adria and Pepin do not stock Oracool goods yet — Griswold's
Basic shelf is the only one wired up.
