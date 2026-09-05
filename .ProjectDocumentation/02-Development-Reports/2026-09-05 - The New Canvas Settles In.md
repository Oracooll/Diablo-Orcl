# The New Canvas Settles In (v1.9.273)

**Date:** 2026-09-05 · **Request:** six items after the 340x720 canvas landed.

| Item | Change |
|---|---|
| Skill picker ground | Two half-transparent passes, the item tooltip's and the books' backing, instead of the opaque stone fill. The ornate border stays. |
| Stash SORT and GOLD | 12px face (were 24). The page label keeps 24. |
| Stash nav buttons | 28x18 faces plus the legacy bevel (were 40x26), "<<" "<" ">" ">>" in the 12px face. The static check that the two pairs never meet in the middle still holds. |
| Vendor text shadows | The "Your gold" line and the control labels are shadowed; they sit on the canvas, not on a plate. |
| Griswold's controls | Their row runs 28..312: the canvas's inner bezel edges are x=22 and x=317 (measured), and the row keeps 6px off each. The tab strip keeps its own 16px inset. A hovered control gets a second translucent pass on its plate; the rect never moves. |
| Titles | `PanelTitleTop` 18 to 28: the canvas's top bezel ends at y=24, and the band starts 3px below it. One number moves every side panel's title and, since the Abilities arrows centre on the same band, the arrows with it. |

On "align horizontally the title with nav arrows (in abilities)": both already centre on the title band's mid-line (the title with VerticalCenter, the arrows from `GetArrowRect`), so they moved together and I changed nothing there. If they still read offset on screen, it is the 30px face's own optical centre; a screenshot will say by how much.

## Verification

Debug build clean; 625/626 with the standing `Drlg_l1` failure. In the game: the picker over either well (world showing through), the stash's control row, Griswold's Buy tab (controls within the bezels, hover darkens), any side panel's title against the bezel.

## v1.9.274: the hover is a shadow, not a ring

"instead of gold boxes, when i hover over items in abilities windows draw 2 times bigger shadow under them. if it is 3 px, make it 6px and darker. draw it under the icons, not under the texts." `DrawHoverShadow` is the slot's resting shadow at six pixels down-left and two passes; the three Abilities hover sites (tree cells, passive slots, Spells-sheet rows) call it instead of the gold ring, with the same bezel inset the resting shadow uses. On the Spells sheet it is the row's ICON rect, not the row's width, so the name and detail lines stay clear. Drawn before the icons, as the resting shadow is, so it lies under the slot. `DrawHoverOutlineHeavy` stays for the waypoint list.
