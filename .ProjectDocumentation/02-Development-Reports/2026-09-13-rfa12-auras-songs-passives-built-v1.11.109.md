# The 48 RfA-12 skills that need no spell slot are built: sixteen auras, eleven songs, twenty-one passives

2026-09-13 — v1.11.109

## Why

v1.11.108 put all 162 RfA-12 skills in the trees, inert. 114 of them are actives or warcries and need a
spell id each, which `SpellID` (int8_t, 126 of 128 used) cannot give yet. The other 48 need none: the
Paladin's sixteen new auras, the Bard's eleven new Melody songs, and twenty-one passives across the
Barbarian, Rogue and Monk. This unit builds those 48 - main effect and level-up stat both - so they take
points and do what their rows say.

## How it is split

The same split the tree has always used:

- **A number on the sheet** lives in `class_tree.cpp`'s `ApplyAura` / `ApplyPassive`, where the tooltip
  reads it back through `DescribeBonusTotals`: Valor, Steadfast, Resist Magic, Warding Light, Aura of
  Protection, Sanctity, Ballad of Resilience, Hunter's Chant, Serenade of Steel, Song of Plenty,
  Swiftness, Sharpen, Brace (armour), River Stance, Iron Fist, Mountain Stance, Inner Fire. Endurance
  (a share of base life) and Symphony of War (half of every other learned Melody song) need the player,
  so they sit beside the aura call in `ApplyClassTreeToTotals`.
- **A rule** lives in the new `oracool/rfa12_effects.{h,cpp}`, one function per question and each asked
  from exactly one engine site - the header's table lists them. Its numbers are one function each, shared
  by the rule and by `Rfa12AuraFactsAt`, so a tooltip cannot quote a number the rule does not use.

| Hook | Engine site | Skills |
|---|---|---|
| `Rfa12DamageDealtPercent` | `PlrHitMonst`, `MonsterMHit` | Bane of Evil, Dominion, Retaliation, Dead Ground, Deadeye |
| `Rfa12MonsterDamagePercent` | `MonsterAttackPlayer` | Dominion |
| `Rfa12MonsterArmorCutPercent` | `EffectiveMonsterArmor` | Condemnation |
| `Rfa12DamageTakenPercent` | `ApplyPlrDamage` | Battle Hardened |
| `Rfa12BlockBonus` / `Rfa12GrantsBlock` | the two block rolls, `CalcPlrInv` | Staff Parry, Brace |
| `PlayerIgnoresKnockback` | `MonsterAttackPlayer` | Immovable, Heavy Foot |
| `PlayerHoldsAgainstHit` | `StartPlrHit` | Anthem of Valor, Grip of Iron |
| `MonsterRegenBlocked` | `ProcessMonsters` | Lasting Wounds, Deep Wounds |
| `MonsterMayNotice` | `ProcessMonsters` (both activation gates) | Nocturne, Soft Tread |
| `MonsterScented` | `DrawMonsterHelper` | Scent of Blood |
| `Rfa12ReachTarget` | `DoAttack` | Long Reach |
| `TitheTakesCorpse` | the death animation's last frame | Tithe of Ash |
| `OnRfa12Hit` / `Struck` / `PlayerDamaged` / `MonsterKilled` | the existing passive hook sites | the procs and clocks |
| `ProcessRfa12Tick` | `ProcessClassTreeTick` | Radiance, Siren's Call, Doom Procession, the three regenerations, bleeding, marks |

State is per player and per monster slot, file-local, and cleared where the chill table is
(`ClearRfa12State`) and wherever a monster slot is created or deleted (`ClearRfa12StateForMonster`,
beside the warcries' own) - the "statics outlive the game" rule.

## Interpretations worth knowing

- **Symphony of War** was proposed as "every Melody song +50% while this plays", but only one song plays
  at a time. It is built as the medley instead: while it plays, every OTHER Melody song the Bard has
  learned lends its sheet number at half its ranks (rounded up). The row says so.
- **Brace** makes a spear or pike a blocking weapon (the flag a shield sets), plus the block chance.
- **Nocturne / Soft Tread** narrow when a monster in sight first NOTICES the player; a monster already
  hunting is unaffected, and a monster that is hit notices regardless.
- **Dead Ground, Deadeye, Sovereign Measure** count every player missile as "ranged", spells included.
- **Doom Procession** keeps at most eight burning tiles; each strikes once and goes out.

## Tests

Seven new `OracoolRfa12` tests: the block's shape (162 rows on class pages, exactly 48 built, no built
active, description and flag agree), Resist Magic's number and life stat only while lit, Swiftness's
speed and dexterity with its points, Immovable only while it burns, Mercy's threshold and wait,
Endurance's share of base life, and Symphony of War's half ranks with an unlit song's stat staying off.

The first Mercy run failed on the test, not the rule: it expected a heal of 20% of 100 life, but lighting
Mercy recomputes the sheet and its own +10 life level-up stat makes the maximum 110. The assertion now
reads the maximum as it stands.
