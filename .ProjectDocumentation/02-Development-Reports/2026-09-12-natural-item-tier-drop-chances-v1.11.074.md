# Natural drop chances for the item tiers

**Version:** 1.11.074
**Date:** 2026-09-12
**Branch:** renderer-32bit

## What was asked

> "make sure when new ini file is created monster density is set too 300% and drop chances of all
> item tiers is set to whatever feels natural to you."

and then, decisively:

> "change my current ini file in RTM to natural drop chances of item tiers, because i think i made
> them too harsh. i barely see rares."

## Monster density was already right

`monsterDensityPercent` already defaulted to **300**, and the live RTM ini already said
`Monster Density=300`. Nothing to change; reported rather than "fixed".

## What the old numbers were, and why the test fought back

The first pass raised Rare 2 -> 10 and the suite failed on
`OracoolAudit.ShippedDefaultsMatchTheReferenceIni` - which turned out to be the right answer to the
wrong question. That test pins these defaults on purpose, and its comment says why:

> "These five move as one balance decision: three times the monsters and champion packs, with special
> items an order of magnitude rarer. If a future change wants to raise a drop rate, it should have to
> look at the density it is paired with."

So the stingy rates were **the user's own tuning**, handed over on 2026-08-27, deliberately paired
with the 300% density. The test existed to stop exactly the change being made without weighing that
pairing. Worth recording that the guard worked.

The second message resolved the tension: the pairing was reconsidered *and* the user reported from
play that it was too harsh. So the change is deliberate, not accidental.

## The new ladder

Rolled in the order Unique -> Primal -> Buffed Unique -> Rare, each only if the one before failed.

| Option | Was | Now | Effective after the conditionals |
|---|---|---|---|
| Primal Item Drop Chance | 1 | **1** | ~1.0% |
| Buffed Unique Item Drop Chance | 1 | **3** | ~3.0% |
| Rare Item Drop Chance | 2 | **6** | ~5.8% |
| Unique Drop Chance Percent | 50 | **100** | vanilla's own window |

Each tier is about twice as rare as the one below it, which is the shape a quality ladder wants.

**Why 6 and not 10.** At 2, the roll landed on about one in fifty magic-eligible drops - and since
only some drops are magic-eligible at all, a rare was nearer one in a hundred items. That is the
"barely see rares" being reported, and three times the monsters does not fix a rate that low; it just
produces more of the same white items. But the pairing is still real: at 300% density a rate of 10
would put rares near a third of a vanilla level's loot. 6 is about one in seventeen eligible drops -
regularly seen, still worth stopping for.

**Unique Drop Chance Percent** went to 100 separately. It was 50, and its own INI comment calls it
"the knob that NERFS" - a fresh install was shipping uniques at half of vanilla's window for no
stated reason. It was not part of the 2026-08-27 reference-ini diff, so nothing the user chose is
being overturned there.

Primal stays at 1: every affix on a Primal is forced to its maximum, so it should remain the rarest
thing on the ladder.

## The live ini, not just the defaults

Defaults only reach a NEW ini, so the running configuration was edited too:
`DiabloOrcl RTM\diablo.ini` had Rare=2, Buffed Unique=1, Unique Percent=50. All three now match the
new defaults; Primal=1 and Monster Density=300 were already correct and were left alone.

The file was **backed up first** as `diablo.ini.backup-2026-09-12` - it is the user's own tuned
config, a hundred-odd settings of which three were touched, and the edit was done key by key with the
CRLF endings preserved rather than by rewriting the file.

## Verification

Debug and Release build clean; **718/718** tests pass, including the reference-ini guard with its new
expectations. RTM updated with the Release exe and the edited ini.

One caveat the user should know: if the game was running while this was written, its own save-on-exit
may overwrite the ini with what it had in memory. Worth confirming the three values in-game.
