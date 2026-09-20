# The Salvage window's message: one per press, framed sprite, the tier's colour (v1.12.097)

**Date:** 2026-09-21 · **Version:** v1.12.097 · **Tests:** 832/832

User (after a preview of two other forms): "i want 1 message per salvage button press: X (type) Items destroyed /
(Icon frame 60x60 with 56x56 high res version of salvaged material sprite) X (material name) Salvaged. Font in colour
of item type. Message stays until another salvage icon is pressed ... horizontally and vertically aligned in middle of
Salvage Results area." Then: "icon frame to have 1px outline border on the outside of these 60x60px. frame color -
according to salvaged item."

## What is drawn (`DrawSalvageWindow`, levski_roar.cpp)

Under the painted "Salvage Results" plate, in the box (30,214,260,106), centred both ways as one block:

1. `4 Rare Items destroyed` - the 12 px font in the tier's colour (`SalvageTierAdjectives`: White, Magic, Rare,
   Unique, Primal, Set, Ethereal; `SalvageTierColors`: the seven item-type colours of Item::getTextColor).
2. A 60x60 near-black plate with a 1 px outline OUTSIDE it in the tier's RGB (`SalvageTierRgb`), the material's
   56x56 sprite two pixels inside the plate, and `7 Rare Fibres Salvaged` beside it, vertically centred on the plate,
   in the same colour.

The message replaces the previous one at the next press and stays otherwise (cleared for a new game). A press that
finds nothing shows the same two lines with zeros, so every press answers in the same place.

## The count

`SalvageAllInBackpack` gains `int *materialsMade`: how many materials were actually placed in the pack (the yield,
less anything the overflow line reports lost). The event log line names it too: "Salvaged 4 Rare into 7 Rare Fibres".

## The sprites

There is no painted 56 px material art: the seven materials exist at 28x28 (batch 19). `ui\salvage_mat_<tier>.png`
are those doubled crisply (nearest neighbour) to 56x56 as a stand-in; a request for painted 56 px versions is the
next art item if the doubled pixels read too coarse.
