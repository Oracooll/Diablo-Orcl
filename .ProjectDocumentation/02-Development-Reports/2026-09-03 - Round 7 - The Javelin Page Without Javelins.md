# Round 7 — the javelin page without javelins (v1.9.189)

Round 7 of [[Plan - Developing Every Inert Skill]]. Eight of the Rogue's ten Javelin & Spear rows
go live. The engine has no javelin, no spear and no poison; the round is built on what it has.

## The thrusts are swings with a rule on them

Jab, Power Strike, Impale, Charged Strike, Fend and Lightning Strike joined Round 4's melee latch
in `oracool/melee_skills.cpp` — six more `ClassMeleeSkill` entries, each a profile plus at most one
reaction, the same shape as the Barbarian's and Monk's seventeen:

| Row | Effect |
|---|---|
| Jab | three blows, the 2nd and 3rd at 50% (+5/rank) |
| Power Strike | +30% (+5/rank), plus 1–4 lightning per rank on the blow |
| Impale | +100% (+20/rank) |
| Charged Strike | +20% (+5/rank); throws off 2 Charged Bolts (+1 per 2 ranks) toward the target, at the rank |
| Fend | the spin, at 80% (+5/rank) — Whirlwind's case with a wider share |
| Lightning Strike | +20% (+5/rank); Chain Lightning launched from the Rogue through the target, at the rank |

The two **thrown** rows — Lightning Bolt and Lightning Fury — are spells riding the engine's own
Lightning and Nova at the rank. Their sentences say the bolt carries itself, because no javelin
exists to carry it.

## Held back

Poison Javelin and Plague Javelin (no poison in the engine), the Barbarian's Double Throw (no
thrown weapons). The rows say why.

## Numbers

- MAX_SPELLS 112 → 120; eight more bytes in the investment chunk; the writehero hash is
  re-baselined with its reason.
- The melee-skill test's list grew by six; the two thrown rows are pinned as priced, described
  and not latched.

## To look at in play

Ready Jab: three numbers float off one swing. Charged Strike: bolts scatter from the target.
Lightning Strike beside a pack: the chain runs. Lightning Fury: the Nova ring, from the Rogue.
