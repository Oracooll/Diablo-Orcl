# The Valkyrie guards close and shoots the nearest threat

2026-09-14 — v1.12.016

## Why

> "we need to redesign her behaviour in dungeons. i want valkyrie to stick close to me - 2-3 tiles range and shoot
> whoever is closest."

In v1.12.015 she still used the Golem's brain: she chased her chosen enemy and shot once it was within 8 tiles, so she
wandered off after it.

## Her own brain

`GolumAi` hands a Valkyrie to `ValkyrieAi` (`monster.cpp`) at once. The Golem keeps its own melee brain unchanged.

Each time she is free (not walking, shooting or spawning):

1. **Regroup.** More than **10** tiles from her Rogue (a teleport, a portal, a long run), she is placed on a free tile
   within 3 tiles of her, standing.
2. **Leash.** More than **3** tiles away, she walks back first: one step toward the Rogue, straight or up to two turns
   aside. Staying close comes before shooting.
3. **Shoot.** Within the leash she shoots the enemy **closest to her Rogue** among those she can see (clear missile
   line) within 8 tiles of herself. Nearest to the hero rather than nearest to her, because she is the hero's guard.
   The arrow is still the owner's own, at the hero's damage (`ValkyrieShoot`, unchanged).
4. **Settle.** With nothing to shoot, she drifts back until she is within **2** tiles.

Candidates are live, hittable, visible monsters that are not player minions.

## Not verified here

This build was not run in the game.
- **Speed.** The Golem's walk may be slower than a running Rogue. She would then lag and fall back on the regroup.
- **Stepping.** One step can be refused by a wall or a crowd, and she tries again on the next free tick.
