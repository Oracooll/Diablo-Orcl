---
date: 2026-08-16
version: 1.7.9
area: Megaplan Phase 2.1 - skill points on level-up, feeding the existing ladders
---

# A Point Per Level

Phase 2 opens with its foundation: skill points. The persistence has been riding along since
1.6.27 (the SkillPoints hero chunk round-trips through every save); this unit is the gameplay
that fills it.

## The rules

- **One point per character level** - D2's rate - granted in NextPlrLevel, with a log line.
- **Characters who levelled before this collect what they are owed** on load: a retroactive
  top-up to (level - 1) points, running in the same slot as the belt migration, no-op once paid.
- **Points deepen skills, they never teach them**: a skill is investable only if actually known -
  book level above zero, or an innate ability. Item-granted spells are deliberately excluded, so
  unequipping a staff cannot strand points in a skill the character no longer has.
- **Cap 20 per skill** (D2's), spent one click at a time.

## One seam feeds every ladder

`Player::GetSpellLevel` now adds `_pSkillInvestment` - and that accessor is what every damage
formula, duration, and missile count already reads. Firebolt with book level 1 and two invested
points casts at level 3. Nothing else needed touching.

**Zeal is the exception that proves the design**: its strike ladder was pure character level
(6->2, 8->3, ... 12->5), which is also why telemetry had it flagged as too strong for free. Now
the level-6 gate buys the 2-strike burst and only INVESTED points buy more - one strike per two
points, still capped at five. A Paladin who spreads their points elsewhere keeps the base burst.
The two old ladder tests were re-pinned to the new rule.

## The spend UI

On the Abilities sheets (Spells, Class Skills, and the Skills sheet's Paladin rows): a gold "+"
button appears at a row's right edge whenever the local player holds an unspent point that row
can take, and vanishes when the pool is empty - the sheet stays quiet otherwise. Beside it, a
permanent "+N" shows what the skill has already been fed. The title band shows "Points: N" while
any are unspent. Zeal's row takes investment even though it cannot be readied (it has a SpellID;
only the ready path withholds it).

## Next

Phase 2 continues: respec at Adria (RespecCost/RefundAllSkillPoints are already written and
tested, waiting for her dialog), then the run toggle, then the aura gameplay pass.

**State: 398 tests, the usual two pre-existing failures.** New pins: retro grant pays once,
unlearned spells refuse points, invest/refund round-trip through GetSpellLevel, and the
point-driven Zeal ladder in both test files.
