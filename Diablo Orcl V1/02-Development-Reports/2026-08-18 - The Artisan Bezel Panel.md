# The Artisan Bezel Panel

**Version:** 1.7.76 → 1.7.77
**Date:** 2026-08-18
**Request:** "continue with D", then — on review — *"you did not use the correct panels. use this
collection - oracool-stone-panel-artisan-bezel-family-v1.0.0"*
**Shipped as:** one shared untitled limestone panel, all six windows keeping their drawn titles.

All six 340×720 side panels — inventory, stash, quests, waypoints, character and abilities — now
draw the **artisan-bezel family's ashen limestone**: a vine-carved outer frame with the Judgment
frieze across the bottom 95 pixels, and a plain stone field between.

## What shipped, and what did not

The first pass (1.7.76) used the **large carved-title** family instead: six backgrounds, one per
window, each with the window's name incised into a 44px top rail. That was the wrong collection, and
1.7.77 replaces it with the one asked for.

The correction is not only a file swap, because the two families make opposite demands of the code:

| | Carved-title family (1.7.76) | Artisan bezel (1.7.77, shipped) |
|---|---|---|
| Backgrounds | six, one per window | **one, shared by all six** |
| Titles | incised in stone; code stops drawing them | **drawn, exactly as before** |
| Abilities window | needed its own rail-less variant | no special case — it is the same panel |

So the title suppression is gone. Every `DrawOutlinedString` is unconditional again, the `SidePanel`
enum and its per-window asset table are gone, and `DrawSidePanelArt` is back to taking nothing but a
position. The net diff against 1.7.75 is now what it should have been in the first place: **one new
PNG, one changed asset path**, and no behavioural change in any window.

That is also why the shared background was possible again. A carved title makes "shared" impossible
by construction — the moment the name is part of the picture, every window needs its own picture. An
untitled panel does not, and the code is simpler for it.

## The Abilities window stops being a special case

Worth recording, because it was the sharpest argument in the first pass and it evaporates here.

Under the carved-title family the Abilities window could not use a titled panel: its band names the
**current sheet** — Skills, Spells, Class Skills, Auras — and carries the page arrows, so a name
incised into stone would be right on one sheet out of four. It needed a rail-less variant of its
own.

With an untitled family for every window, that problem does not exist. The Abilities window draws its
sheet name over the same stone as everything else.

## Contract

The pack's integration guide is short and this build already satisfied it:

- `panel: 0,0,340,720`, `bezel: 0,625,340,95`.
- The middle HUD may overlap the frieze; the carving carries no semantic content, so partial
  occlusion is safe.
- Controls at y ≥ 625 still need relocating or disabling while covered — none of the six put
  controls there. The inventory grid plus its new bezel ends at exactly 624, the stash's at 615.
- Drawn 1:1, no filtering.

The upper 625 pixels are pixel-identical to the previous palette-safe background family, so this is
a strictly richer bottom edge over the same stone the panels already wore.

## Verification

Debug build clean at 1.7.77; suite **445 of 447**, the two failures being the standing baseline pair.
`oracool.mpq` carries `ui\panel_bg.png`; the six `panel_bg_*` variants from the first pass are gone
from both the archive and the tree.

**To see it in game:** open each of the six windows. All should share the same limestone frame with
the winged frieze along the bottom, and all should name themselves in drawn gold text as they always
did — including the Abilities window's sheet name changing as you page between sheets.

## Note for unit E

The frieze occupies y 625..719, and the pack says the middle HUD may overlap it. That overlap is
about to be re-cut: unit E replaces the bottom HUD plate, and the two now share an edge. Worth
checking them together rather than separately.
