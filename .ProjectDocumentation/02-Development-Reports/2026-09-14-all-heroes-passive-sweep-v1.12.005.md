# The all-heroes sweep: every passive the engine can carry, built

2026-09-14 — v1.12.005

## Why

> "sweep through all skills/passives/auras of all heores and bultd whichever can be built and make a all-heroes
> artifact with all of their skill so i can check which ones are done and which ones we need to adjust to game
> engine."

## Before and after

| Hero | Rows | Inert before | Inert after |
|---|---|---|---|
| Paladin | 72 | 11 | 2 |
| Barbarian | 73 | 4 | 4 |
| Sorcerer | 72 | 14 | 4 |
| Rogue | 73 | 14 | 6 |
| Bard | 72 | 22 | 15 |
| Monk | 72 | 12 | 1 |
| **Total** | **434** | **77** | **32** |

45 rows were built: 43 on the Passive Skills pages, plus the Monk's Reed in the Wind and Counterstroke on Way of the Body. The census artifact counts 29 of the built rows as reworded to fit the engine; each carries its note there.

## New hooks (passives.h)

| Hook | Asked from | Carries |
|---|---|---|
| `OnPassiveMissileHit` | missiles.cpp, after a player missile's damage | Paralysis, Temporal Flux, Thrill of the Hunt, Archery (composite/battle), element marks; spells also reach the shared hit rules |
| `OnPassiveBlock` | `StartPlrBlock` | Insurmountable, Renewal, Counterstroke |
| `OnPassiveDamaged` | `ApplyPlrDamage`, after life is taken | Galvanizing Ward's clock, Illusionist |
| `PassiveBlockBonus` | monster melee and missile block rolls | Hold Your Ground, Reed in the Wind |
| `PassiveThornsPercent` | `ThornsReturnPercent` | Iron Maiden |
| `PassiveMoveSpeedBonus` | `MovementSpeedBonusPercent` | Illusionist, Tactical Advantage, Hot Pursuit (speed bursts) |
| `PassiveMonsterDamagePercent` | monster melee damage | Numbing Traps, Dissonance |
| `PassiveManaCostPercent` | `GetManaAmountAtLevel` | Chant of Resonance |
| `PassiveSkillDamagePercent` | Blessed Hammer, Blessed Shield, Smite | Blunt, Towering Shield |

The existing hooks took the rest:

- **`PassiveDamageDealtPercent`:** Conflagration, Elemental Exposure, Arcane Dynamo, Mythic Rhythm, Sharpshooter, Perfect Pitch, Crescendo, Magnum Opus, Chorus, Unity, Counterstroke, Seize the Initiative, Combination Strike, Momentum.
- **`PassiveDamageTakenPercent`:** Galvanizing Ward, Dominance, Magnum Opus.
- **`OnPassiveManaSpent`:** Wrathful, Prodigy, Arcane Dynamo.
- **`OnPassiveMonsterKilled`:** Dominance, Blood Vengeance.
- **`PassiveCheatsDeath`:** Unstable Anomaly.
- **Evade checks:** The Guardian's Path, Tactical Advantage.
- **`PassiveShrugsOffStagger`:** Stagecraft.
- **`ApplyPassive` (sheet):** Righteousness, Blood Vengeance, The Guardian's Path (staff), Finery, Archery, Rhythm, Alacrity.

Per-monster element marks live in `passives.cpp`. A monster outside the `Monsters` table, such as a test's local one, has no marks and is ignored.

## A bug found on the way

Astral Presence and Exalted Soul added `totals.mana += 20`. `totals.mana` is in 1/64 units, the same units items use, so "twenty more mana" was a third of a point. They now add `20 << 6`, as do Righteousness and Blood Vengeance.

## Still needing engine work (32)

The reason for each is in its description and in the census artifact.

- **No cooldown system:** Evocation, Beacon of Ytar.
- **No mount, bombardment or phalanx:** Lord Commander. **No aura durations:** Long Arm of the Law.
- **No traps or sentries:** Custom Engineering. **No grenades:** Grenadier.
- **Songs:** they are single auras with no duration, cost or range, and do not strike.
  - Sustain, Encore, Countermelody, Improvisation, Refrain, Timbre, Virtuoso, Overture and Reverberation, plus the Bard's D2 inert rows.
  - D2 inert rows: Epic Solo, Perfect Harmony, Legendary Ballad, Resonance, Echoing Song, Ode to Glory.
- **Actives with no SpellID or missile:**
  - Sorcerer: Static Field, Thunder Storm, Meteor.
  - Rogue: Decoy, Poison Javelin, Plague Javelin.
  - Barbarian: Double Throw.
- **No thrown weapons:** Throwing Mastery. **No stamina:** Increased Stamina.
- **Retired from the page:** Boon of Bul-Kathos, Ballistics.

## Tests

- **New `oracool_passive_sweep_test`:**
  - every Passive Skills row is built unless it is on the still-inert list, and "Not yet built" appears exactly when a row is inert;
  - Hold Your Ground, Iron Maiden, Blunt;
  - Chant of Resonance mantras only;
  - Illusionist's speed burst.
- **`oracool_audit_test` census counts:** inert floor >50 → >25; Passive Skills page rows built 50 → 93.
