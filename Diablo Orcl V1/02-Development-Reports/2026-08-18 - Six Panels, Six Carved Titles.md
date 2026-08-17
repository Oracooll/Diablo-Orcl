# Six Panels, Six Carved Titles

**Version:** 1.7.76
**Date:** 2026-08-18
**Request:** "continue with D" — unit D of the 2026-08-18 MPQ drop-zone sweep.

The five side panels that shared one background now each have their own, with the window's name
**incised into the stone** rather than drawn over it: a 44px carved top rail, vine-carved edges, and
the Judgment frieze across the bottom 95 pixels.

## The titles stopped being text

This is the whole change, and everything else follows from it. The delivered family carves the name
into the relief, and its README is explicit: *"do not draw a separate procedural title plate or title
over rows 0..43. The carved title is already part of the relief."*

So five panels — Inventory, Stash, Quests, Waypoints, Hero Stats — lost their `DrawOutlinedString`
call. Not deleted: each is now inside `if (!carvedTitle)`, so a build whose art failed to load still
names its own windows. The shared `ui\stash_background.png` that all five drew is retired, because
"shared" stops being possible the moment the title is part of the picture.

`SidePanelArts[]` is indexed **by** the `SidePanel` enum, with a static_assert that the array and the
enum have the same length — six backgrounds, six windows, and no way for the two to drift into each
other's art.

## The Abilities window is the exception, on purpose

It gets `SidePanel::Plain`, the rail-less variant of the same stone, and keeps drawing its own title.

Its title band is not decoration. It names the **current sheet** — Skills, Spells, Class Skills,
Auras — and carries the page arrows at either end. A name incised into stone would be correct on one
sheet and lying on the other three. This is the one place where "the art carries the title" is the
wrong trade, so it is the one place that keeps the old behaviour.

## What the contract cost, which was nothing

The pack requires interactive content to start at **y ≥ 44** and the bottom **y 625..719** to be left
to the frieze. Both were already true here, which is why no layout moved:

- The inventory's equipment block starts at y 51, the stash's content at y 101.
- The inventory grid plus its new bezel ends at exactly 624; the stash's at 615.

The number is named as `oracool::SidePanelTitleRailHeight` rather than left in the art's README, so a
future layout creeping upward is arguing with a constant instead of quietly writing over incised
stone.

## One thing to look at

The inventory's **class silhouette** hangs from y 16 — inside the carved rail. It is drawn after the
background, so the figure's head crosses the carved INVENTORY.

That is deliberately left alone rather than fixed blind. The silhouette is *blended*, not blitted: it
darkens what is behind it instead of replacing it, so the letters should still read through as
shadowed relief rather than being covered. The pack's y≥44 rule is about *interactive* content and
this is background art, so it is within contract either way. But it is the one place in this unit
where the result could be handsome or could be a mess, and it needs an eye rather than an argument.
Moving it down is not free — the figure already clears the tab row by only 2 pixels.

The other wording change worth knowing: the character sheet's stone reads **HERO STATS** where the
code's string said CHARACTER. The delivered family named the six panels itself, and its wording is
what is on screen now.

## Verification

Debug build clean at 1.7.76; suite **445 of 447**, the two failures being the standing baseline pair.
`oracool.mpq` carries all six `panel_bg_*.png`.

**To see it in game:** open each of the six windows in turn — inventory, stash, quests, waypoints,
character, abilities. Five should wear their name in carved stone with no drawn text over it; the
Abilities window should have a blank rail with its sheet name and arrows drawn in, changing as you
page between sheets. Then the silhouette question above.
