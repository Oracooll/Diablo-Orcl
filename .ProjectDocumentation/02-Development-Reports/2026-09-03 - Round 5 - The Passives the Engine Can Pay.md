# Round 5 — the passives the engine can pay (v1.9.187)

Round 5 of [[Plan - Developing Every Inert Skill]]. Forty-five passive rows go live across all six
classes. The plan called this the cheapest round per row and warned that it is the least visible;
the numbers below are the ones a player can check against the sheet or the floating damage numbers.

## Two homes

**Sheet rows** — a number the stat sheet already carries — went into `ApplyPassive` in
class_tree.cpp, beside the Barbarian's masteries. The tooltip's "Now:" line prints them from the
same call the game applies, so the text cannot drift from the effect.

| Row | Effect |
|---|---|
| Divine Fortress | +25% armour with a shield |
| Tough as Nails | +25% armour |
| Perfectionist | +10% armour, +10 all resistances |
| Harmony (Monk) | +15 all resistances |
| Superstition | +10 all resistances |
| Glass Cannon | +15% damage, −10% armour |
| Holy Cause | +10% damage |
| Animosity, Astral Presence, Exalted Soul | +20 mana |
| Fanaticism (Crusader) | Fast Attack |
| Fervor | Quick Attack with a one-handed weapon |

**Rule rows** — a chance or a condition read at the moment of a blow — went into a new module,
`oracool/passives.{h,cpp}`: seven hooks, each asked from exactly one engine site.

| Hook | Site | Rows |
|---|---|---|
| damage taken % | `ApplyPlrDamage` | Blur −17, Sixth Sense −25 non-physical, Vigilant −20 non-physical, Sword and Board −30 with shield, Relentless −25 below ⅓ life, Unwavering Will −20 when still; floor −75 |
| damage dealt % | `PlrHitMonst`, `MonsterMHit` | Ruthless +40 vs <⅓, Ambush +40 vs ≥¾, Brawler +20 with 3+ adjacent, Determination +5/adjacent to 20, Steady Aim +20 with nothing within 3, Audacity +15 within 2, Power Hungry +20 at ≥5, Cold Blooded +10 / Cull the Weak +20 vs chilled, Relentless Assault +30 vs frozen or stunned, Single Out +25 vs a straggler, Rampage +5/stack, Unwavering Will +10, Cadence +50 every third swing |
| slips | `MonsterAttackPlayer`, `PlayerMHit` | Dodge (standing), Evade (moving), Avoid (arrows): 10% +4/rank, cap 40 |
| pierce | `CheckMissileCol` | Pierce: 15% +5/rank, cap 60 — the arrow's range is not zeroed |
| cheat death | `ApplyPlrDamage` | Indestructible, Nerves of Steel, Awareness, Near Death Experience: life to ⅓ (NDE mana too), once a minute |
| returns | `DoAttack`, `MonsterMHit`, every mana-spending site | Leech 3% of damage as life; Bloodthirst / Transcendence half of mana spent as life; Requiem 2% life per kill within 4 |
| tick | `ProcessClassTreeTick` | stillness clock (1.5 s), Rampage stacks (5 × 5 s), the save's cooldown, Brooding (1%/s when still) |

Fleet Footed is one more reason `IsClassTreeRunActive` answers yes for a Monk.

The mana-spent hook is wired into all five places this fork subtracts mana for a skill:
`ConsumeSpell`, and the `Pay` of the Paladin, Rogue and melee modules.

## Held back

Thirty-six Passive Skills page rows still read "Not yet built": anything that needs fury, wrath,
hatred, spirit, cooldowns, songs, traps, grenades, rockets, mounts, laws, a gem count, a block
report, or an attack-speed number. Plus the four the engine cannot carry at all — Throwing Mastery,
Spear Mastery, Increased Stamina, the Bard's two page masteries — whose rows say why.

## Numbers

- No save-format change; the writehero hash is untouched.
- New test: `OracoolPassives.SheetRowsMoveTheSheetAndRuleRowsAnswerTheirHooks`.

## To look at in play

Slot Blur on a Sorceress and watch the floating damage numbers shrink by a sixth. Slot Nerves of
Steel on a Barbarian and let something kill him: he stands at a third. Put three points in Dodge
and stand in a crowd. Slot Rampage and chain kills: each swing's numbers climb.
