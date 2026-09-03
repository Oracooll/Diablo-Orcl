# Round 8 — the Paladin's last three (v1.9.190)

Round 8 of [[Plan - Developing Every Inert Skill]]. The plan listed seven; Vengeance and Holy
Freeze went live in Round 6, and Cleansing and Redemption stay held. Three rows ship.

| Row | Shape | Effect |
|---|---|---|
| Sacrifice | melee latch (Round 4) | +150% (+20/rank); a twelfth of the damage dealt is taken from the Paladin's own life, never the last point |
| Holy Bolt | the tree's own spell | `SpellID::HolyBoltSkill` riding `MissileID::HolyBolt` at the rank — its own id, so it cannot collide with the book spell again |
| Conversion | targeted cry (Round 6) | one enemy near the cursor takes the engine's Berserk flags for 20 s (+2/rank) and gives them back when the clock runs out |

## Why these shapes

Holy Bolt was withdrawn on 2026-08-18 because the row rode `SpellID::HolyBolt`, the book spell,
and the tree's "retired as spell" rule refused it points. A SpellID of its own is the whole fix.

Conversion was withdrawn because it borrowed Berserk's behaviour, which is permanent. The cry
sets the same flags with the same exemptions (uniques, champions, Diablo, the magic-immune, a
monster mid-fade or mid-charge) and adds the one thing that was missing: a clock, kept beside the
monster debuffs in `oracool/warcries.cpp` and cleared where they are. The monster's own strength
is left as it is; Berserk's damage boost is not copied.

`CastWarcry` gained an aimed overload; the shared missile now passes its destination through.

## Held back

Cleansing: nothing on a player here has a duration to shorten. Redemption: corpses (Round 9).

## Numbers

- MAX_SPELLS 120 → 123; three more bytes in the investment chunk; the writehero hash is
  re-baselined with its reason.

## To look at in play

Sacrifice on a full-life Paladin: a big number on the monster and a small red one on you.
Conversion on a pack member: it turns and fights its fellows, then turns back twenty seconds later.
