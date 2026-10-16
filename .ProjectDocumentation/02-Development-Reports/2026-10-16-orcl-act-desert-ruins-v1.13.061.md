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

## v1.13.062: RfA-71b approved, the painted ways down in the game
RfA-71b came back with the rim sand at (102,89,71), against sand_a's (104,92,74). The halo is gone, and it is approved as delivered (batch-100b).

**What the game draws:**
- The Salt Road's sinkhole and the Glass Flats' stairway replace the game-drawn funnel.
- `DesertGroundFeature` lays each into the cached ground with its middle on the entrance tile's centre, blended by its soft alpha.
- The hover, the entry and the walkable entrance are unchanged.
- The game-drawn funnel stays as the fallback if a painting fails to load.

**Loading (`LoadDecal`):**
- **Size:** both entrances load at 50% of their painted size. That puts the 150 px painted hero at the game's height and the sinkhole's mouth at the old pit's width.
- **Colour:** the first in-game render showed each as a paler rectangle, because the game's sand is shaded differently from raw sand_a. So each picture is tinted until the sand round its entrance matches the floor's own sand.
- **Fade:** each is also faded out in an oval, from 0.56 to 0.86 of its half-size.

**The trickling grains:** they follow the painted sinkhole's narrower funnel (0.62 of the old radius). None are drawn on the stairway.

## v1.13.064: RfA-70b, more of the buried city
Eight more pieces, approved as delivered (batch-101). All are within their content heights, with binary alpha.
- **The Tomb's court:**
  - the lion guardian lying beside the arch;
  - two burial urns, the sundial and a stub of wall;
  - a broken back wall along its far edge.
- **The Glass Flats' ruins:** urns, the sundial, a wall stub and two broken-wall pieces join the four RfA-70 stones.
- **Every ruins floor:** a sarcophagus lid lies in the sand, as decor (`DecorLid`, ScatterSlots 19).
- **Out in the dunes:** an aqueduct with a wall stub beside it on floors 3 and 4; a lone wall run on the Glass Flats; the guardian's twin with urns on the Tomb's floor.
- **By every road:** two travellers' cairns per floor, between the milestones.
- **Footprints:**
  - The guardian, the aqueduct and the wall stub block theirs.
  - The broken wall runs along +x (top-left to bottom-right on screen) and spans two tiles, so the tile after each piece is a Blocker. A run laid two tiles apart leaves no gap.
