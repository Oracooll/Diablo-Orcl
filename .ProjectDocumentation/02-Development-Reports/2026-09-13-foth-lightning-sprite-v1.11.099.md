# Fist of the Heavens' ring goes back to the lightning sprite

2026-09-13 — v1.11.099

## What happened

I first read "FotH to disperse Charged Bolts when it lands instead of this current asset" as a
mechanical change and rebuilt the landing as wandering `MissileID::ChargedBolt` missiles. The user
corrected it:

> "actually i want foth to roll back to how it worked, i just want to change the asset that
> disperses with the lightning asset used in charged bolt. i misspoke the last time."

So this reverts the mechanic and changes only the sprite.

## The change is a deletion

The ring is back to what it was: 36 `MissileID::MiniNovaBall` missiles on a quarter arc mirrored
into four, radius 4, exactly as `ProcessNovaCommon` lays Nova out.

The asset needed no swap at all. **`MiniNovaBall` already draws `MissileGraphicID::ChargedBolt`** —
the `miniltng` sheet the Charged Bolt spell uses (`misdat.cpp:163`). Between 2026-09-11 and now the
ring wore `MissileGraphicID::HolySpark` whenever that art happened to be loaded, and that re-skin is
what made the landing look like a burst of sparks rather than lightning. Removing it is the whole
change.

## The sheets, for the record

The user asked which sprite suits the skill:

| | file | width | y-offset | frames |
|---|---|---|---|---|
| Charged Bolt | `miniltng` | 64 | 0 | 8 |
| Chain Lightning / Lightning | `lghning` | 96 | 16 | 8 |

`miniltng` is the right one here, for a mechanical reason rather than a taste one: the ring is 36
separate projectiles flying to their own tiles, and `lghning` is drawn to be tiled end-to-end into a
continuous arc. Thirty-six of those flying apart would read as disconnected beam fragments, and at
96px with a 16px lift they would pile up badly near the centre.

Chain Lightning was also considered and rejected: `ProcessLightningControl` **ignores the damage it
is handed** and recomputes from `Players[...]._pLevel` every tick, so a Chain Lightning FotH would
have silently scaled off character level instead of the weapon - breaking the rule the whole
Paladin ranged family is built on.

## The stats sheet stays fixed

`PaladinCastDamageType(FistOfTheHeavens)` now asks `MissileID::MiniNovaBall`, which is Lightning -
so the hero stats screen still draws FotH's damage in yellow rather than the physical white it used
before v1.11.097. That half of the earlier work was a separate request and is unaffected.

## Verification

- Debug: **728 tests, 0 failed.**
- Release built and linked; `DiabloOrcl RTM\DiabloOrcl.exe` refreshed.
- No asset changed, so no MPQ repack. `holy_spark` is now referenced by nothing; its `misdat.cpp`
  row is left in place so the art stays available.

## What to look at in play

Cast Fist of the Heavens. The ring should be lightning again rather than pale sparks, and the
stats-screen damage line should still be yellow.
