# Skill rank audit: which skills gain nothing from a second point (2026-09-07)

**Request:** "audit which skills make no sense in investing more than 1 skill point in them. we need to come up with something to make it worth it."

**Method:** every consumer of `ClassTreeInvestment` / `GetSpellLevel` across Source/oracool and missiles.cpp, per row of the 272-row tree. Two facts shape the whole answer: aura radius is `4 + (points-1)/2`, capped at 8 (a step every two ranks, flat from rank 9), and no Oracool-added skill's mana cost falls with rank (`sManaAdj = 0` on every new spelldat row; Paladin skills use a flat table cost). So "cheaper at higher rank" is never a hidden benefit.

## 1. Rank 2+ identical to rank 1 (13 rows, all cap > 1)

| Class | Skill | Rank-1 effect | Where the rank is not read | Proposal |
|---|---|---|---|---|
| Pal | Smite | always hits, 2 s stun | `ShieldBashStunTicks = 40`, paladin_melee.cpp:122/266 | stun `40 + 4/rank` ticks, and +5% weapon damage per rank |
| Pal | Charge | 2 s dash, 3 s cooldown | furious_charge.cpp:20-21, no rank anywhere | cooldown `3000 - 100/rank` ms (floor 1 s); dash damage +10%/rank |
| Pal | Blessed Hammer | 60% weapon damage per tile | `BlessedHammerPercent = 60`, paladin_ranged.cpp:125; missiles.cpp ignores `_mispllvl` | `60 + 5/rank` %, and `BlessedHammerTicks` +4/rank (a longer spiral = more tiles) |
| Pal | Fist of the Heavens | 150% blast, 60% nova bolts | constants paladin_ranged.cpp:58-60 | `150 + 10/rank` % centre, `60 + 4/rank` % bolts |
| Pal | Blessed Shield | 125% throw, 1-tile splash | `BlessedShieldPercent = 125`, :63 | `125 + 8/rank` %; splash radius 2 at rank 10 |
| Pal | Hammer of Faith | 50% splash to adjacent | `HammerOfFaithSplashPercent = 50`, paladin_melee.cpp:112 | `50 + 3/rank` % (cap 100 at rank 17) |
| Pal | Thorns (aura) | the engine's Thorns flag | class_tree.cpp:867-869, `p` unused | reflect `p` extra points per hit taken via a thorns-damage total, on top of the flag |
| Bar | Increased Speed (passive) | run toggle | class_tree.cpp:1637-1639, `> 0` | route through the movement-speed channel: +5% per rank (rank 30 = 250%, capped by the run at 160 so effectively 12 useful ranks; or cap the row at 12) |
| Rog | Guided Arrow | cannot miss | missiles.cpp:291-294; no damage bonus, no end-of-flight case | give it the `ElementalBonus` line as physical: +2 damage per rank; at rank 10 pierces once |
| Bard | Song of Swiftness (aura) | FastAttack flag | class_tree.cpp:916-919, `> 0` run | +3% movement speed per rank through the slow/speed channel, and Faster attack at rank 10 |
| Monk | Sweeping Reed | hits the tiles beside the target at 100% | melee_skills.cpp:73-79 zero profile | `100 + 10/rank` % to the side targets (cap 5 → 150%) |
| Monk | Breaking Current | 1 s stun, 33% profile | `StunMonster(*front, 20)`, melee_skills.cpp:329-333 | `20 + 5/rank` ticks like Stun's own `30 + 4(rank-1)`; bonusPerRank 5 |
| Monk | Flowing Step (passive) | run toggle | class_tree.cpp:1650-1652 | +5% movement per rank (cap 5 → 125%), and the described evade chance 3%/rank |

## 2. Weak or partial scaling (24 rows)

**Step-gated, with dead ranks**

| Class | Skill | Formula | Dead ranks | Proposal |
|---|---|---|---|---|
| Pal | Zeal | +1 strike at 1/3/5 (max 4); to-hit +1%/rank | 2, 4, 6-30 give +1% only | +2% weapon damage per rank alongside the to-hit |
| Monk | Seven Reeds | strikes `3 + (r-1)/3`, cap 5 | 2, 3, 5 | strike per rank (3..7 over the 5 ranks) |
| Monk | Hundred Fists | strikes `4 + (r-1)/2` | 2, 4 | +5% damage on the even ranks |
| Rog | Multiple Shot | arrows `2 + level/2`, cap 6 | every odd rank; flat from 8 | +2 damage per rank via ElementalBonus (physical) |
| Rog | Strafe | `3 + level/2`, cap 8 | odd ranks; flat from 10 | same |
| Bar | Leap | range `4 + (rank-1)/3`, cap 8 | two of three; flat from 13 | landing shockwave: 20% + 5%/rank of weapon damage to adjacent |
| Rog | Dodge / Avoid / Evade | `10 + 4(p-1)`, cap 40 | flat from 8 | raise the cap to 60, or cap the rows at 8 |
| Rog | Pierce | `15 + 5(p-1)`, cap 60 | flat from 10 | cap the row at 10 |
| Pal | Vigor | +15%/rank, run at 160% | 5-30 inert | cap the row at 5 (it already reads "at rank 5 you run everywhere") |
| Pal | Conviction | all resistances stripped at any rank; immunities break at 5; radius | only 5 and the radius steps | resistance strip 50% + 10%/rank (full at 5), immunity break at 5 as now |
| Bard | Weaken / Discord | `20 + 2(p-1)`, cap 50 | flat from 16 | cap rows at 16 or lift cap to 78 (2/rank) |
| Bard | Dirge of Dread | `15 + 2(p-1)`, cap 40; repel 4 fixed | flat from 13 | repel `4 + rank/5` |

**Radius-only auras and flat-effect actives**

| Class | Skill | What stays flat | Proposal |
|---|---|---|---|
| Pal | Holy Freeze | chill 3 ticks at every rank, warcries.cpp:577 | chill `3 + rank/2` ticks, and use the slow channel: monsters in the ring at -10% -1%/rank |
| Pal | Sanctuary | repel distance 4, aura_field.cpp:20 | repel `4 + rank/4`; undead in the ring take `rank` holy damage per second |
| Bar | Taunt | only earshot grows | taunted monsters' damage -2%/rank |
| Bard | Daze | `goalVar1 = 3` | daze `3 + rank/3` turns |
| Bar | Battle Command | always +1 spell level; only duration grows | +1 level per 10 ranks (2 at 10, 3 at 20, 4 at 30) |
| Monk | Tranquility | chill 3, radius 2, heal maxHP/50; only duration grows | radius `2 + rank/2`, heal `/50 + rank/5` |
| Rog | Slow Missiles | deflect `50 + 5(r-1)` cap 80 | cap the row at 7 |

## 3. Scaling properly (about 89 rows)
Paladin 17, Barbarian 22, Sorcerer 14 (the cold set and masteries), Rogue 19, Bard 8, Monk 9. No action.

## 4. maxRank 1 by design (113 rows)
The 108 Passive-Skills-page rows and five Monk capstones. Correct as they are.

## 5. Inert, unimplemented (88 rows)
Listed in the inert-skill plan; out of scope here.

## 6. Rows that take no points at all (18)
Retired as book spells (`sBookLvl >= 0`): the Sorcerer's 13 castables, Valkyrie (Golem), Charm (Berserk), Sonic Barrier and Spirit Ward (both Mana Shield), Inner Sight (Search). Rank is moot; books raise them. class_tree.h's note now lists the two Mana Shield rows.

## The recommendation

Three moves, in order of value for effort:

1. **A per-rank percentage on every flat Paladin and Monk skill** (section 1's nine attack rows). One helper, `RankPercent(base, perRank, rank)`, applied where each constant is read; the Abilities window's "Next level" line then has something to say for all of them.
2. **Cap the rows that go flat** (Vigor 5, Pierce 10, Dodge/Avoid/Evade 8, Slow Missiles 7, Weaken/Discord 16) with `maxRank`, so a point can never be wasted there. The table supports it already; the Monk uses it.
3. **Route the three run toggles through the movement-speed channel** (Increased Speed, Song of Swiftness, Flowing Step), so they scale in percent the way Vigor now does and stack with the item affix.

Not recommended: making auras' radius scale faster. Radius is what makes an aura feel like an aura, and 8 tiles is already the screen.
