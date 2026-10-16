# Orcl Act: the buried city's stones (v1.13.061, 2026-10-16)

ChatGPT's RfA-70 (batch-99, approved as delivered) and RfA-71 (batch-100, sent back as RfA-71b).

## RfA-70: approved and in the game
Eight ruin pieces, each one painting. All eight are in their content-height ranges, with binary alpha and the 4 px margin, in the approved props' sandstone and palette.

| Piece | Where it stands |
|---|---|
| broken column | the Glass Flats' ruins; the Tomb's court (two, flanking its arch); out in the dunes, with fallen drums beside it |
| fallen column | the Glass Flats' ruins; the Tomb's court; beside the lone columns |
| toppled head | the Glass Flats' ruins; the Tomb's court |
| obelisk | the Tomb's court; one alone in the Tomb's dunes |
| eclipse stele | the Glass Flats' ruins; the Tomb's court; one alone in the Tomb's dunes |
| ruined arch | the head of the Tomb's court |
| headless statue | the Tomb's court (two) |
| brazier | the Tomb's court: two, facing each other across its middle, off any road crossing it |

- **Code:** `PlanRuins` now takes the floor. The Glass Flats keep the bone pieces and add four stones; the Tomb's ruins become a court of eleven pieces. The lone pieces use `FindDesertSpot` on floors 2 and 3.
- **Footprints:** the arch, the fallen column and the head are wide, so `BlockFootprint` blocks their neighbours.
- **The braziers' fire is game-drawn:** RfA-70 paints them cold. `DrawWoodsGlows` draws a flickering bowl of fire with three rising tongues, fainter by day. `WoodsNightLamps` adds a warm light by night (reach 5).
- **Finding a place for the braziers:** they take the nearest free opposite pair of tiles up to 5 out from the court's middle, and fall back to the two nearest free tiles. On some seeds the salt road runs straight through the court, which shut out the first fixed spots.
- **Scale:** every piece is drawn at 29%, the desert props' default, and can be tuned on the Level Designer's Desert tab.
- **Tests:** the new hook `DesertRuinsForTest` lets `DesertFloorsAreWalkableAndHoldTheirSights` check, on 5 seeds, that floors 3 and 4 have reachable ruins and that only the Tomb has its two braziers. `DISABLED_DesertFloors` now also renders a `ruins` view.

## RfA-71: sent back (RfA-71b)
The paintings read well: the sinkhole with its stake and rope, and the stairway between its sun-disk posts. But each sits in a bright orange halo that shows as a lit ring on the sand:

| Entrance | Its rim | `sand_a`'s mean |
|---|---|---|
| sinkhole | (159,123,85) | (104,92,74) |
| stairway | a warm band | (104,92,74) |

RfA-71b asks for the sand round each entrance to be repainted to `sand_a` (each channel within 8 of its mean), and allows a larger sinkhole mouth. Until it arrives, the game keeps drawing its own hole.
