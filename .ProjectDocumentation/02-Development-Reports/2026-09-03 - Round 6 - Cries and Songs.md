# Round 6 — cries and songs (v1.9.188)

Round 6 of [[Plan - Developing Every Inert Skill]]. Twenty-one rows go live: the Barbarian's
Warcries page (Howl, Taunt, Shout, Battle Cry, Battle Orders, War Cry, Battle Command), the Bard's
Lullaby, Sound Shock, Shout, Daze and the three song-auras Dirge of Dread, Discord and Weaken, the
Monk's Temple Bell, Purifying Breath and Tranquility, the Rogue's Inner Sight and Slow Missiles,
the Paladin's Vengeance and Holy Freeze.

## One mechanism, two vocabularies

`oracool/warcries.{h,cpp}`. A cry is **cast like a spell**: it has a SpellID, a mana price, the
cast animation and sound. Its missile — `MissileID::Warcry`, one for all seventeen — calls
`CastWarcry` with the spell the player is executing and deletes itself. A cry with nothing to do
(nobody in earshot, a buff still nearly full) fizzles and costs nothing.

What a cry does is one of three things:

| Shape | Rows | How |
|---|---|---|
| **timed buff** on the caster | Shout (+50% armour +10/rank, 40 s), Battle Orders (+20 life and mana +10/rank, 40 s), Battle Command (+1 to every skill, 30 s), Purifying Breath (+20 all resists +5/rank, 30 s), Vengeance (fire and lightning on the weapon, 30 s), Slow Missiles (half the arrows turn aside, 20 s), Tranquility (chills neighbours, 2% life a second, 12 s) | per-player table with a tick count; sheet buffs feed `ItemBonusTotals` through the aura provider and force a recompute when they start and end |
| **timed debuff** on what heard it | Battle Cry (−25% damage and armour, 24 s), Inner Sight (−30% armour, 20 s) | per-monster table, asked at the point of use: `EffectiveMonsterArmor` in every player to-hit roll, damage and aim in `MonsterAttackPlayer` |
| **immediate reaction** | Howl, Daze (retreat — Sanctuary's own channel), Taunt (wake and target the caster), War Cry, Lullaby, Bard's Shout, Sound Shock, Temple Bell (stagger through `StunMonster`, some with magic damage) | done at the cast |

The **song-auras** and Holy Freeze are the same vocabulary held rather than shouted: `Kind::Aura`
rows lit through the existing toggle, asked through a new `AuraPointsOn(monster, skill)` beside
Conviction's query. Discord strips armour, Weaken blunts aim and chills each tick, Dirge of Dread
weakens and repels, Holy Freeze chills each tick.

Earshot is the aura radius for the rank (4 tiles +1 per 2 ranks, cap 8). Uniques, champions and
Diablo hold their ground against every repel and stagger.

## Held back

Find Potion, Find Item, Grim Ward and Ode to Glory want corpses (Round 9); Decoy wants an entity;
Cleansing and Redemption have no durations or corpses to work on. Their rows still say so.

## Numbers

- MAX_SPELLS 95 → 112; seventeen more bytes in the investment chunk; the writehero hash is
  re-baselined with its reason.
- New test: `OracoolWarcries.BuffsFeedTheSheetAndDebuffsNeedAnEar`.

## To look at in play

Ready Shout and cast it: the armour on the hero sheet jumps by half and falls back forty seconds
later. Battle Cry in a crowd, then watch the floating damage numbers you take shrink. Howl beside
a pack: everything but the champion runs. Lullaby: the room stands asleep until you swing.
