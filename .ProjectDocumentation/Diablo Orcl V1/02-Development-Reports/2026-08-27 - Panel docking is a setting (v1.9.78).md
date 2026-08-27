# Panel docking is a setting (v1.9.78)

**Date:** 2026-08-27
**Version:** 1.9.77 → 1.9.78
**Tests:** 564, of which 562 pass — the two standing baseline failures, unchanged.

---

> "maybe we need to align the limestone windows to the middle. i am now playing on surface laptop
> with its 3:2 aspect ratio and the bottom dock doesnt sit nice with me."

## Why this became a setting rather than a decision

The panels are **720 tall**. At 16:9 that is the whole screen, so bottom and middle are the same
placement and the question never arose — which is why bottom-docking looked right when it shipped
yesterday and wrong today. Nothing changed but the screen.

There is a real argument on each side, and neither wins outright:

**Bottom** is not arbitrary. The inventory grid is laid out to end exactly where the mana orb begins,
and the orbs are bottom-anchored — so bottom-docking is the **only** anchor that preserves that
alignment on any screen. Centring breaks it.

**Middle** looks better on a tall screen, which is the whole of its case, and that is not a small
thing. A rule that is geometrically defensible and reads as "stranded" is still wrong.

That is a taste call about a screen this code cannot see, so it is in the INI:

```ini
Panel Docking = Middle    ; or Bottom
```

**Default is Middle**, because that is the preference actually expressed by someone looking at it.
Bottom is one word away if the orb alignment turns out to matter more in play.

It covers the five side panels — inventory, Abilities, character sheet, stash and shop. The floating
windows keep the placements settled in v1.9.76 and are not affected.

## What this does not do

It does not rescale the panels. On a 3:2 screen there is genuinely more vertical room than a 720-tall
panel uses, and centring only distributes the slack more evenly — it does not fill it. Making the
panels grow with the screen is a much larger change (every grid, tab strip and button row inside them
is positioned against fixed constants) and is not something to start without asking.

---

## Still queued — four items

The Rare tab; the Set shop; the hammer-cursor Repair rework; and Refresh on Basic/Rare/Supplies.
