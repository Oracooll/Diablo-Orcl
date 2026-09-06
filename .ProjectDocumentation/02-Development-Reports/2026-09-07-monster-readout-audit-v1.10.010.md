# Monster readout audit: damage as swung, XP as paid (v1.10.010)

**Date:** 2026-09-07
**Request:** "make sure monster stats are correct in a sense they show actual stats of actual monster kind and tier, not some basic stats that are wrong in uniques, minions and lesser uniques cases."

## What was already right

Hit Points and Damage on the health bar read the monster's OWN fields, and every tier writes those at spawn: PrepareUniqueMonst from the unique table, the minion scaling, the lesser-unique stat line, the Hollow/Feral variants, and the Nightmare/Hell/Torment multipliers. Class is the kind's class, which is what a borrowed sprite is.

## What was wrong

- **Damage** ignored a champion's Might. MonsterAttack adds it at swing time through `PackAdjustedDamage`, so a pack member with "(Might)" after its name showed the number it did NOT hit for. The readout now runs both ends of the range through the same call.
- **XP** showed the raw table value after the difficulty formula, but the kill runs the level-difference clamp on the monster's LEVEL against the player's, and a unique's level is double its table level. So a unique's line was wrong by construction, and every line was wrong whenever the player's level differed from the monster's. The clamp (and the multiplayer cap) is now one function, `KillExperienceFor` in player.cpp, used by the grant, the health bar and the XP counter, so all three quote the same number.

## Tests

The clamp at equal level, ten up, five down, fifteen down. Suite 690/690.
