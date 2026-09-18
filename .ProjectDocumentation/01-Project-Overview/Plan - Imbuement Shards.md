# Plan - Imbuement Shards

**Date:** 2026-09-19 - the code at v1.12.043 - the live plan with the decisions is the artifact **Imbuement Shards**
(https://claude.ai/artifact/WntDqFo7C888ykoK9S7gS5); this note is its paper copy and does not carry the answers.

## The idea (the user's, 2026-09-19)

Replace the Mystic Orbs - Median XL's mechanic, "the other one's idea" - with the mod's own **Imbuement Shards**:
many kinds (one per hero stat; one that improves every affix slightly; one for durability; one for lowering the
requirements), many per item (twenty proposed), and RECORDED on the item rather than baked into its stat fields.

## Where we start (checked in the source)

- 8 orbs (`oracool/mystic_orbs.*`, `GenMysticOrbs.ps1`): +3 Str/Dex/Mag/Vit, +5 all res, +2 damage, +5% MF, +8% GF; fixed, permanent.
- Cap 6 per item, one byte `_iOracoolOrbCount` (format 9); applied by dropping the orb on the item (`inv.cpp`, before the socket insert) through `ApplyItemPower`, so the stat lands in the item's own fields.
- Because only the COUNT is kept, crafting refuses orbed items (`crafting.cpp`, audit 2026-08-26). Drops through the treasure classes' `orbWeight`. Sound OrbAbsorb, milestone FillOrbCap, tooltip "Mystic Orbs: 3 / 6", real icons since batch 23.
- Items have NO level requirement (only Str/Mag/Dex, lowered by Hel through `EffectiveRequirement`); set pieces alone carry a level.

## What a shard is

1. Fixed value, never rolled.  2. Many per item under ONE cap (20 proposed) - the cap is what makes a shard a decision.
3. **Recorded, not baked**: a ledger of kinds on the item; stats computed from it through the stat-sheet seam
(`oracool/stat_sheet` BonusProvider). Rebuilds and rerolls keep them, tooltips list them, the crafting refusal goes,
and the Roadmap card "Keep Mystic Orbs through a rebuild" closes by construction.

## Roster as proposed (D1, D6 decide)

| Shard | Each | Lands in |
|---|---|---|
| Strength / Magic / Dexterity / Vitality | +1 | the Imbuements provider |
| Refinement (new) | every affix +3% of its rolled value (limit 10 proposed) | affix-only totals of the item, scaled at sheet time |
| Tempering (new) | +10 max durability (limit 10; indestructible declines) | derived max durability recomputed from the ledger |
| Ease (new) | -3 to each Str/Mag/Dex requirement, floor 0 | `EffectiveRequirement` |
| Warding / Fury / Fortune / Avarice (orb carry-overs, if kept) | +2 all res / +1 damage / +2% MF / +3% GF | provider |

The eight orb item indices are positional save format: they are RE-LABELLED as the first eight shards; new kinds are
appended after the Necromancer bases.

## The ten decisions (answered on the page; recommendation in brackets)

D1 roster [seven + the four carry-overs]; D2 cap per item [20]; D3 limits per kind [Refinement and Tempering stop at 10,
Ease at zero requirements, stats free]; D4 the requirement shard [lower Str/Mag/Dex; adding a level requirement is a
system of its own]; D5 Refinement semantics [+3% per shard on the affix totals]; D6 numbers [the table]; D7 old orbs at
the bump [stacks convert 1:1, orbed items rebuilt clean]; D8 how applied [drop on the item, as orbs]; D9 removal
[permanent]; D10 sources [drops only through the socketable share, deeper kinds deeper].

## Build order

S1 decisions closed - S2 data and generator (`GenImbuementShards.ps1`, RfA-18 drafted) - S3 the ledger (format 11,
loaders, hash, refusal removed) - S4 applying (TryImbue, caps, sound, log, milestone, tooltip) - S5 effects (provider,
Refinement, Ease, Tempering) - S6 drops and sources - S7 art intake and MPQ repack - S8 tests, telemetry, records,
Roadmap (retire the two orb cards) - S9 play pass and numbers.

## Assets (RfA-18)

7 or 11 shard icons 28x28; one ground tumble sheet (the orb tumble redrawn); one imbue sound; optionally the milestone
signet redrawn. Everything builds on stand-ins (orb icons recoloured by the generator) until the batch lands.

## Tests

Ledger round-trips (hero, stash, tabs) at format 11 with the hash moved; the cap and per-kind limits; Ease never below
zero; Tempering declines indestructible; twenty Strength shards total exactly +20 worn and 0 in the pack; Refinement
scales affixes only; rebuild and reroll keep the ledger; audit rows; every kind droppable on the 64-rung ladder; none
in a shop pool; old orb stacks load as shards.

## The answers (2026-09-19, read from the page)

| # | Answer |
|---|---|
| D1 | **"All you can think of"** - the roster became 24 kinds (below) |
| D2 | **Twenty** per item |
| D3 | Refinement and Tempering stop at ten, Ease at zero requirements, stat shards free; extended to the wider roster: Stone (damage taken) ten, Radiance (light) five, Arcana (spell levels) three |
| D4 | Lower the Strength, Magic and Dexterity requirements (no level requirement exists) |
| D5 | +3% of each affix's value per shard, on the affix totals at sheet time |
| D6 | The proposed numbers |
| D7 | Orb stacks convert one for one; orbed items are rebuilt from seed and lose the baked bonus |
| D8 | Drop the shard onto the item, as the orbs |
| D9 | **Permanent, PLUS a Levski's Roar recipe that strips every shard from an item and returns none** |
| D10 | Drops only, through the socketable share, deeper kinds deeper |

## The roster as decided (24)

Hero stats: Strength, Magic, Dexterity, Vitality (+1). Life and mana: Blood (+5 life), Spirit (+5 mana). Striking:
Fury (+1 damage), Keenness (+2% damage), Precision (+2% to hit), Flame (+1-2 fire damage), Spark (+1-3 lightning).
Defence: Bulwark (+2 armour), Stone (-1 damage taken, limit 10), Warding (+2 all res), Ember/Storm/Veil (+3 fire/
lightning/magic res). Finding and seeing: Fortune (+2% MF), Avarice (+3% GF), Radiance (+1 light, limit 5), Arcana (+1
spell levels, limit 3). Item-acting: Refinement (+3% affixes, limit 10), Tempering (+10 max durability, limit 10),
Ease (-3 per requirement, floor 0). Left out on purpose: speed ladders and on-hit flags (switches, not numbers) and
a one-shard indestructible. Orb indices re-labelled: Might=Strength, Grace=Dexterity, Insight=Magic, Vigour=Vitality,
Warding, Fury, Fortune, Avarice; sixteen new kinds appended after the Necromancer bases. Drop bands: shallow from
rung 1 (Bulwark, Ember, Storm, Veil, Radiance, Tempering, Ease), middle from the Caves' rung (the stats, Blood,
Spirit, Fury, Keenness, Precision, Flame, Spark, Warding, Fortune, Avarice), deep from the second difficulty (Stone,
Arcana, Refinement). RfA-18: 24 icons, one tumble, one sound.
