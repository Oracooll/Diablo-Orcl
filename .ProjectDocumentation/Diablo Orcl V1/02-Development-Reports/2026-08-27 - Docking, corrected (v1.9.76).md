# Docking, corrected (v1.9.76)

**Date:** 2026-08-27
**Version:** 1.9.75 → 1.9.76
**Tests:** 564, of which 562 pass — the two standing baseline failures, unchanged.

---

## I applied the rule to the wrong windows

> "the limestone windows i wanted docked at the botom were inventory, hero stats, etc... I dont see
> those in your list."

Correct, and the mistake is worth naming precisely rather than just fixing. "Limestone window" is the
project's word for the **side panels** — the 340-wide slabs that carry the inventory, the character
sheet, the Abilities pages, the stash and the shop. I read it as "every window with an ornate border"
and moved four floating windows that nobody had asked about, while leaving the five that were meant.

The clue was there and I walked past it: v1.9.75's own table listed five windows and **not one of
them was a panel the player opens with a keybind**. A rule about "all limestone windows" that touches
nothing the player thinks of as a window should have read as a misunderstanding.

## What moved now

The panels, all five, sharing one anchor because they share slots — the stash, character sheet, quest
log and shop take the left slot, the inventory and Abilities the right, and two panels occupying the
same slot must occupy the *same space*:

| panel | was | now |
|---|---|---|
| Inventory | top-right | bottom-right |
| Abilities / spell book | top-right | bottom-right |
| Character sheet | top-left | bottom-left |
| Stash | top-left | bottom-left |
| Shop | top-left | bottom-left |

**A no-op at 1280×720**, where these panels are exactly as tall as the screen, and the entire point
on anything taller — the panel then sits with the HUD it belongs to instead of floating at the top
with a gap beneath it.

## What moved back

**Runeword book** — returned to the mini-map's top edge. It was never in scope: *"runeword book was
fine as it was. i didnt ask for it to be moved."*

**Levski's Roar** — centred vertically, not bottom-docked and not at the third-of-the-way-down it had
before.

**Levski's recipe book** — centred in the band from the top of the screen down to a 100px margin
above the bottom, rather than copying the Roar window's top edge. Centring in the *usable* band
rather than the whole screen is the point: the bottom hundred pixels are the HUD's.

**Skill picker** — untouched, and it was untouched in v1.9.75 too. `git diff` on that file for the
docking commit is empty; it was already anchored to the button it belongs to, which is why the report
listed it as "already right, by accident". Worth stating plainly since the note read as though I had
moved it.

**Waypoint menu** — stays bottom-docked. Confirmed as correct.

---

## Still queued — five items

The Rare tab; the Set shop; the hammer-cursor Repair rework; Refresh on Basic/Rare/Supplies; and
Adria and Pepin stocking Oracool goods (Griswold's Basic shelf is still the only one wired up).
