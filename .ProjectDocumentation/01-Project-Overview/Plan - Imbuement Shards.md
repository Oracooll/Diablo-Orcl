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
