# Griswold's Salvage window wears the user's painted UI (v1.12.096)

**Date:** 2026-09-21 · **Version:** v1.12.096 · **Tests:** 832/832

User: "assemble Griswold's Salvage UI. You will find Assembled version to guide you, the background and the icons.
Icons as usual - a notch brighter on hover and sink on click. The Salvage title you put yourself. Font - same as
Levski's Cube title."

## The assets (Resources\Griswold Salvage UI)

| Delivered | Becomes | How |
|---|---|---|
| Griswold Salvage UI 2.png (320x352, the forge with a "Salvage Results" plate and a dark results box) | `ui\salvage_canvas.png` | as is |
| Griswold Salvage ICONS.png (2172x724, seven framed tiles of ~271x277 at y 218..494) | `ui\salvage_white/magic/rare/unique/set/primal/ethereal.png` | each tile cut by its frame rect and resampled to 56x56 (bicubic) |
| Griswold Salvage UI Assembled.png | the placement guide, filed as `salvage_assembled_sample.png` | diffed against the bare background: row one at y 78, x 32 / 98 / 163 / 230; row two at y 137, x 49 / 132 / 214 |

Filed under `Resources\01-in-use-assets\ui\griswold-salvage-ui`. The sheet's frame colours follow the seven tier
colours (white, blue, yellow, gold, green, beige, grey); the sample's second row runs Set, Primal, Ethereal, and the
code keeps that order while indexing the icons in `SalvageTier` order.

## The window (`oracool/levski_roar.cpp`)

- `SalvageSkin()`: Griswold's host wears the painted UI whenever `ui\salvage_canvas.png` loads; the Roar painting
  is the fallback. `CurrentFrameSize`, `ButtonRect` (the close X at (296,5), the seven icons) and `CloseButtonRect`
  answer for it, so the hover hints and the click loop work unchanged.
- `DrawSalvageWindow`: the forge, the title "Salvage" in the game's 30 px gold with a shadow (the Cube's title font)
  in a band at y 30, the seven icons - brighter 15% under the cursor, sunk 2 px down-left while pressed
  (`PressedSalvageIcon`, released from LeftMouseUp with the Cube's buttons), titlemov on entry and press, the quarter
  shade on an icon with nothing of its tier in the pack - and the results box: the last seven salvage lines under
  the painted "Salvage Results" plate, oldest at the top.
- The click loop records every result ("Salvaged 3 Rare into Rare Fibres", "Nothing to salvage: Set") in
  `SalvageResults` beside the event log; on this window nothing else is clickable, so a held item can never land in
  the Roar's invisible grid.

## Where Griswold's recipes went

The painted window has no grid, so his book had nowhere to run. `HostOfRecipe` sends the gear recipes (5, 6, 10-16)
and the material ladder (26, 27) to the **Cube's** book; nothing was deleted. The Salvage tab is salvage only. If
you want a grid back on Griswold's window, say so and it can be drawn under the results box.
