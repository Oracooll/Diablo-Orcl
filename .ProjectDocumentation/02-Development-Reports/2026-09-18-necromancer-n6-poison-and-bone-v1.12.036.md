# The Necromancer, phase N6: Poison & Bone (v1.12.036)

**Date:** 2026-09-18 - Debug only - **814 of 814 tests**. N4 and N5 are still awaiting the user's look in play; the
ledger had no verdicts on this page, so the rows were built as written.

## The page, all eighteen rows live

Sixteen new `SpellID`s (`MAX_SPELLS` 275), their cases in `rfa12_actives.cpp` beside the helpers they use (Strike,
Ring, Show, NewField / TickField, StartBuff, MonstersWithin / MonstersOnLine / FrontArc). The two passives read at
the moment they matter:

- **Marrow** - every bone skill +8% a point (`BoneStrike`, magic damage).
- **Virulence** - poisons +25% longer and +10% deeper a point (`Poison`).

**The poison** is a new damage-over-time channel in the RfA-12 monster marks - `poisonTicks` / `poisonDamage`,
the same shape as the burn, struck as `DamageType::Acid` once a second so acid immunity and resistance apply. A
stronger poison replaces a weaker; a weaker one only extends the clock.

The actives:

- **Teeth** - the three front-arc tiles, and 2+rank monsters along the line. **Bone Splinters** - the first three on a
  4-tile line. **Bone Spear** - everything on a 9-tile line. **Bone Spikes** - within 1 of the cursor, staggered a second.
- **Bone Armor** - a `bonePool` of (20+10r) points on a 60 s buff, taken out of every blow before the Chord's pool in
  `Rfa12ActiveAbsorbDamage`. No shell drawn yet (see below).
- **Poison Dagger** - 20 s buff: every landed weapon blow poisons (in `OnRfa12ActiveHit`, melee only).
- **Corpse Explosion** (Essence 10) - the corpse within 3 of the cursor, taken; everything within 2 of it takes
  40+5r % of the dead one's life, physical (4-400). **Poison Explosion** (Essence 10) - the same, as 6 s of poison.
- **Blight** - a 4 s pool; **Bone Wall** - five tiles across the cast for 8 s, whatever stands in one is cut and thrown
  back twice a second; **Bone Prison** - the target is held (staggered) and cut once a second for 3 s;
  **Bone Storm** - 8 s, follows the hero, cuts within 2 twice a second.
- **Decompose** - 5 s of heavy poison on the target. **Poison Nova** - within 5. **Death Nova** - magic within 4,
  then poison.
- **Bone Spirit** - the BOOK spell's own missile, cast at this rank with the player as source; it hunts.

**Essence prices are real now**: `EssenceCost` returns 10 for the corpse skills and 35 for Revive - so N5's Revive,
which read "Paid in Essence" but was charged mana at v1.12.035, is charged Essence from this build.

## Honest placeholders

The plan's "bone and poison missiles, PngOnly with placeholder fallbacks" is the missiles batch of RfA-17, not
delivered. Until it is: Teeth / Splinters / Spear are instant strikes on their lines (no bolt flies), the bone fields
show the warcry ring, Blight and Poison Explosion show the delivered acid cloud (RfA-16), Bone Armor draws no shell.
The row descriptions say what the skills DO; the wording of Bone Wall, Bone Prison, Decompose, Bone Storm and
Poison Dagger was tightened to match (and written back to the ledger).

## Format

`MAX_SPELLS` 259 -> 275: the writehero hash moved (tag 18 carries the extra sixteen entries), noted in the test.
`SpellBand`, `SpellITbl`, `SpellsData` grew their rows.

## For the user's look in play

Teeth on a pack in front; Bone Spear down a corridor; Bone Armor then take a blow (life should not move until the
pool is spent); Poison Dagger with a weapon, watch the acid ticks; Corpse Explosion on a body with monsters round it,
and the Essence half drop by 10; Bone Wall across a doorway; Bone Prison on a runner; Bone Storm while walking;
Bone Spirit; the novas. A screenshot is the only verification.
