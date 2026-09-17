# A shield and a sword from another armour tier

2026-09-17 — v1.12.023

## Why

> "i like the Weapon from another tier. I find it acceptable. I think i need to see it in action to make
> a final judgement. We need to introduce Shield from another tier and Weapon from another tier and we
> need assign Shield and Sword to particular item types so i can test vanilla sprite vs sprite with
> shield vs sprite with sword vs sprite with shield and sword."

## The test matrix

Wear LIGHT or MEDIUM armour on a Paladin (Warrior) or a Barbarian - both wear the Warrior's sheets -
and hold:

| in hand | look |
|---|---|
| Short Sword / Falchion / Scimitar / Claymore / Blade / Sabre, with Buckler / Small / Large Shield | vanilla |
| the same small sword + **Kite, Tower or Gothic Shield** | heavy tier's heater shield |
| **Long, Broad, Bastard, Two-Handed or Great Sword** + a small shield (or none) | heavy tier's longsword |
| a big sword + a big shield | both |

Decided by the BASE item's own cursor, so a unique with its own picture changes nothing. An item the
hero cannot use shows nothing, as it already shows no weapon class. Heavy armour is already the heavy
look and is left alone.

## How (oracool/sprite_mix)

No art is shipped. `MixPlayerSheet` assembles the sheet at LOAD time from the archive's own CL2s, in
palette-index space; `LoadPlrGFX` asks it before falling back to the plain CL2, and `CalcPlrItemVals`
reloads the sheets when `GearLookCode` changes as well as when `_pgfxnum` does (`Player::_pGearLook`).

- **Shield** = "sword and shield" minus "sword" in the heavy tier, found by the shield it replaces;
  what stood in front of the old shield stays in front of the new one; seen from behind, the heavy
  sword crossing the shield's back is wiped.
- **Body with empty hands** = a vote across sword, mace, axe, staff (and mace-with-shield, whose
  mace and shield each stand alone in the vote and lose it).
- **Sword** = what the heavy sword sheet shows that the heavy vote does not - the component that
  reaches furthest outside the silhouette, which is what tells a blade from a scabbard.
- **Dyed classes**: pieces brought in are moved to free indices and given their true colour, so the
  heater's white lion stays white on a blue-dyed Barbarian and the heavy blade stays steel.

## What the first contact sheet caught

The spell-cast sheets with and without a shield are NOT the same render, and subtracting them brought
the whole heavy body across. `AreTwins` now measures every pair (twins share 65-85% of their pixels,
strangers 30-50%) and the mixer declines an animation whose sheets are not twins. Scores by animation
are in the session's `twinscore.js` output; the upshot:

| animation | shield | sword |
|---|---|---|
| stand, walk, lightning, magic | mixed | mixed |
| attack, hit | mixed | own tier's (too few twin voters) |
| fire, block, death, both TOWN sheets | own tier's | own tier's |

So in town he is vanilla, and in the dungeon the look can change between animations - a heater shield
standing, a buckler while casting Fire Bolt or blocking. That is a limit of what the archive holds,
not a bug to chase: those sheets were rendered differently and have no twin to subtract.

## Known blemishes, all visible in `engine_mix_contact.png`

- Facing south-west (row 5) standing: a ghost of the heavy blade beside the light one.
- The heavy sword sits near a light hand rather than in it; each tier holds it at its own angle.
- A pixel or two of gap between a slim body and a shield lifted from a bulky one.

## Verified how

`tools/oracool_sprite_export.cpp --mix <dir>` runs the engine's own mixer over every animation for the
four cases and writes PNGs, which is how the contact sheet was made - the mixer needs the archives and
cannot run under ctest. Tests (795 pass, 1 new) cover the item rules. Load cost was not measured: a
mixed sheet reads up to a dozen CL2s, once per gear change. Not run in the game.
