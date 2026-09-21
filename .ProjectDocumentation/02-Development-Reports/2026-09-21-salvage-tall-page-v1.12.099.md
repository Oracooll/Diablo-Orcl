# Griswold's Salvage page at the shop's own size: 340x720 (v1.12.099)

**Date:** 2026-09-21 · **Version:** v1.12.099 · **Tests:** 832/832

User: "i want to assemble a new Salvage page for griswold shop, one which is 340x720 size to fit nicely with the rest
of his UI windows. Take this picture (Resources\Griswold's Salvage UI Full Size.png), add the Salvage icons under the
picture or overlap them a bit with the bottom part of the picture, draw a dark gold frame for Salvage results and
leave enough pixels below to avoid overlapping with health orb."

## The painting

862x1824, whose aspect is 340x720 to within a pixel, resampled 1:1 into `ui\salvage_canvas_tall.png` (filed under
`Resources\01-in-use-assets\ui\griswold-salvage-ui`). Measured on the resampled file: the lit forge gives way to bare
floor at about y 378, and the bottom bezel starts at y 698.

## The page

`SalvageLayout` now names both pages and `SalvagePage()` picks the tall one whenever its painting loads; the 320x352
page of v1.12.096 stays as the fallback. The tall page:

| Piece | Where |
|---|---|
| Window | 340x720, docked bottom-left with the shop panel (`BottomDockedTop`), not centred |
| Title | "Salvage", the game's 30 px gold, at (22,26) in the dark of the smithy's roof |
| Icons, row one | White, Magic, Rare, Unique at y 344 - thirty pixels over the forge's floor line - x 43/109/175/241 |
| Icons, row two | Set, Primal, Ethereal at y 410, x 76/142/208, centred |
| Results | (30,480) 280x132, inside the dark gold frame |
| Clearance | the frame's last pixel is y 615, above the inventory grid's own floor at 618 (`OrbClearanceBottom` 624 less its frame), so the life and mana orbs are never covered |

The frame is four 1 px rings drawn by `DrawDarkGoldFrame`: a near-black shadow outward, a lit gold edge, the dark
gold body, and a keyline inside - carved into the painting rather than laid over it. The icons keep the hover
brighten, the 2 px press sink, titlemov and the idle-tier shade; the message keeps its form from v1.12.097-098 and is
centred in the new frame.
