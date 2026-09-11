# v1.11.058 - the Paladin: every level buys something

2026-09-12. The user, after the skill ledger: "i approve all sugestions on the paladin skills", then three of his own:
- "make vigor +5% faster walk per level for every level. forget the +8hp per level";
- "Holy Fire to be an aura that causes fire in a radius. Radius and DMG grow with every level. Radius stops at certain level - Range 10 tiles. Dmg grows every level. One hit per 3 seconds to all surrounding monsters";
- "Same applies for Holy Freeze and Holy Shock but with their dmg types."

## The three holy auras are pulses now

`aura_field.cpp`'s `ProcessHolyPulse`. Every **60 ticks (3 s)** the lit aura strikes **everything within reach**:

| Aura | Damage at level 1 | Per level | Also |
|---|---|---|---|
| Holy Fire | 4-8 fire | +3 min / +6 max | - |
| Holy Freeze | 3-6 cold | +2 min / +5 max | each hit chills until the next pulse |
| Holy Shock | 1-14 lightning | +1 min / +8 max | lightning's wide spread |

- **Reach:** 4 tiles at level 1, one more a level, **stopping at 10** (level 7) - `HolyPulseRadius`.
- Immunity is respected and a resistant monster takes a quarter, the spell rule.
- The first pulse lands the tick the aura is lit; each one drops the floor shockwave (`WarcryRing`) where the Paladin stands.
- **What they were:** Holy Fire and Holy Shock added elemental damage to every blow, and Holy Freeze chilled without hurting. Those three lines are gone from `ApplyAura`, so the character sheet no longer quotes weapon damage the aura does not add.

## Vigor

`VigorMoveSpeedPerRank` is **5**, and **every level counts**. The stride used to step by threshold (+10, 125, 140, 160 percent) and stop at the run, so ranks past 4 bought nothing.

`StrideTicksFor` now turns the percentage into ticks a stride - 1000 / percent - and **carries the fraction** into the next stride, so the average pace is exactly the percentage. Strides run from 12 ticks (a deep slow) to 4 (250%, the fastest, which Vigor reaches at level 30). The run sources take the faster of the run and the percentage.

The +8 life the ledger proposed for Vigor is dropped, as asked.

## Every approved gain

| Skill | Was | Now |
|---|---|---|
| Smite | stun only, no damage | +15% weapon damage a level; the stun stays 2 s. Mana is charged whenever the bash lands, since the blow is now an effect too |
| Charge | plain swing | +20% damage a level on the arriving blow. Only a blow that ends a real dash carries it (`SetChargeBlowArmed`), so Charge on cooldown is a plain swing |
| Hammer of Faith | 50% splash | 50%, +2 points a level (108% at 30) |
| Blessed Shield | 125% | 125%, +8 points a level (357% at 30). The bounces keep 75% and 50% |
| Blessed Hammer | 60% a hit | 60%, +6 points a level (234% at 30) |
| Fist of the Heavens | 150% centre, 60% ring | +10 and +4 points a level; the ring keeps its 150 : 60 share |
| Thorns | item flag: a flat 1-3 | returns 25% of each melee blow taken, +10% a level. The items' 1-3 still applies |
| Cleansing | inert | slows and chills on you wear off 20% sooner, +5% a level, to 90% |
| Sanctuary | repelled undead | ...and burns them for 4-8 magic a second, +2-4 a level |
| Conviction | resistances only | ...and enemy armour -3% a level, to -60% |
| Holy Freeze / Fire / Shock | see above | pulses |

Smite's and Charge's percentages join the swing's own damage beside the Barbarian's and Monk's (`PaladinMeleeDamagePercent`, read in `PlrHitMonst`), so every blow of a swing carries them.

## Tooltips

An aura's lines are still produced by RUNNING its effect, so they cannot drift. What the totals cannot carry - a pulse, a return, a shortening - is now `AuraFieldFactsAt`. The "Radius" line shows only on auras that reach the monsters (`AuraReachesMonsters`); it used to sit on Might and the resists, where it meant nothing.

## Tests

`OracoolClassTree.PaladinSkillsGrowWithEveryLevel` pins: the stride carry (150% averages 6.667 ticks, 250% is the fastest), the pulse reach (4 at level 1, 10 from level 7), that every pulse's damage grows, Thorns and Cleansing at their levels, Conviction's cut, Sanctuary's burn, and all six skill percentages at level 1 and 30. Cleansing shortens a real slow, and Thorns returns nothing once put out.

The old stride-ladder tests now read exact strides with the carry cleared first; the "auras that touch nothing on the sheet" list gains the three pulses and Thorns.

## Verification

Debug and Release built, ctest **715/715**. **Not seen in play** - the pulses, the new stride and the hit flashes want a screenshot.

**To check:**
- Light Holy Fire in a crowd: everything within reach takes fire every 3 seconds, and the ring shows where it lands. Then Holy Freeze (chills) and Holy Shock.
- Vigor: the Move speed line climbs 5% a level, and the stride keeps quickening past the old run cap.
- Smite and Charge hit visibly harder at higher levels; a Charge on cooldown does not.
