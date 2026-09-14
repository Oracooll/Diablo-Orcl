# Five passives the engine could not carry, replaced

2026-09-14 — v1.12.007

## Why

The user left the same note on five rows in the Orcl Skill Census:

> "Invent a new passive skill which works with current engine capabilities to replace this one."

## The replacements

Each row keeps its table position, so its icon index, investment and slot bytes are unchanged. Only the name,
description, built flag and enum identifier changed.

| Hero | Was | Why it could not work | Now | Rule | Hook |
|---|---|---|---|---|---|
| Paladin | Lord Commander | no mount, bombardment or phalanx | **Crusader's Stride** | +15% movement speed while an aura burns | `PassiveMoveSpeedBonus` |
| Paladin | Long Arm of the Law | auras have no duration | **Sanctified** | +20% damage against undead and demons | `PassiveDamageDealtPercent` |
| Barbarian | Increased Stamina | no stamina | **Toughness** | +5 Vitality, +2 per level (a mastery, by points) | `ApplyPassive` |
| Sorcerer | Evocation | no cooldowns | **Mana Attunement** | +15% spell damage while mana is above half | `PassiveDamageDealtPercent` (missiles) |
| Monk | Beacon of Ytar | no cooldowns | **Serene Mind** | standing still restores 2% of mana every second | `ProcessPassivesTick` |

The enum values were renamed with them (`CrusadersStride`, `Sanctified`, `Toughness`, `ManaAttunement`,
`SereneMind`), along with the Toughness learn-sound entry. The sound file keeps its old name.

The old glyphs still show the retired ideas. A glyph pass for these five belongs with the next art request.

## Census after this build

- 324 built
- 26 reworded
- 10 needing engine work
- 2 retired
- 72 hidden (Bard)

## Tests

- **Passive Skills census:** 97 built page rows. Toughness is on a masteries page, so it is not counted.
- **Still-inert list:** no longer names these five.

Debug: 777/777 tests pass.
