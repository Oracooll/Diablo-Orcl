# Negative resistance, as in D2

2026-09-13 — v1.11.127

## Why

> "just like in D2 allow resists to go below 0 if hero lacks resist affixes."

## Before

`ApplyResistanceCurve` took the difficulty's penalty (0 / 30 / 60 / 90) off the raw total, then **floored the
result at zero**. A hero with no resistance gear on Torment read 0% and took full damage, the same as on Normal.
Once the gear ran out, the penalty stopped mattering.

## Now

| Difficulty | No resist gear | Damage taken |
|---|---|---|
| Normal | 0% | 100% |
| Nightmare | −30% | 130% |
| Hell | −60% | 160% |
| Torment | −90% | 190% |

- The floor is **−100**, D2's own minimum: double damage at worst.
- The soft cap (75) and hard cap (90) are unchanged.
- Negative resistance shows **red** on the character sheet. That branch already existed and was unreachable
  until now.

## Where negative values had to be let through

The formula `damage − damage × resist / 100` amplifies a negative value by itself. But all three places that
apply it were guarded with `resist > 0`, so a negative value would have been ignored:

- **`missiles.cpp`, the player-hits-player path, and missiles and traps hitting the player** (fire, lightning,
  magic, acid, and cold through magic). The `resper > 0` branch is a soft landing: the ArghClang grunt, **no hit
  recovery**, and on the player-vs-player path no block roll. Widening that branch to `!= 0` would also have
  spared a negative-resist hero the stagger. So a negative resistance amplifies the damage **first**, and the hit
  then takes the ordinary branch: it can be blocked, and it staggers. The first draft did widen the branch; it
  was caught on review before the build.
- **`objects.cpp`, the burning cross.** It has no branch, so it tests `!= 0`.

The block check at `missiles.cpp` (`resper <= 0 || Hellfire`) treats negative the same as zero, which is right:
an unresisted hit can still be blocked.

## Save format

Untouched. The fields are `int8_t` and hold only the final value, now in [−100, 90], which fits the type.

## Tests

- `ResistanceStaysBetweenTheFloorAndTheHardCap` replaces the zero-floor test, checking every raw value from −200
  to 400 on all four difficulties.
- `AHeroWithoutResistanceGearGoesNegativeOnHarderDifficulties` checks:
  - with no gear, resistance equals minus the difficulty's penalty;
  - partial gear on Torment still leaves the hero negative;
  - −90 turns 100 damage into 190;
  - the floor holds at −100.

## For the user to look at

- Take a hero without resistance gear into Nightmare or deeper. The character sheet's resist rows read red and
  negative.
- Fire, lightning and magic hits land visibly harder.
- Resistance gear lifts the numbers back toward zero and above.
