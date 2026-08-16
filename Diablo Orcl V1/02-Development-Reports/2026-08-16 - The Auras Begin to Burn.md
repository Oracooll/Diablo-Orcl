---
date: 2026-08-16
version: 1.7.12
area: Megaplan Phase 2 Stage 1 - aura activation and the fourteen accumulator auras
---

# The Auras Begin to Burn

The 24 auras have been named, described, tiered and iconned since v1.1.80, and doing nothing ever
since. Stage 1 of their implementation plan makes them real.

## What lands

**Activation.** Clicking an unlocked row on the Auras sheet lights it; clicking the burning one
puts it out; lighting another replaces it. That is the plan's one rule - exactly one aura, no
duration, no timers - and it means the sheet needed no new control, just a click handler where
there used to be a deliberate `return`. The burning row wears the gold ring permanently.

**Effects, through the Phase 0 seam.** The provider table in `stat_sheet.cpp` gains a fifth row
("aura"), and that is the entire wiring. Fourteen auras map onto accumulators the game already
has: Might and Righteousness onto bonus damage, Defense and Defiance onto armor, Resistance and
Aura Mastery onto the three resists, Holy Fire and Holy Shock onto the elemental damage pairs,
Life Aura onto hit points, Focus and Blessing onto to-hit, Endurance onto vitality and the
damage-taken channel, Vigilance onto light radius, Fanaticism onto to-hit plus damage plus the
FastAttack flag. Everything downstream already reads those totals, so nothing else changed. They
scale on character level until Stage 2 gives auras levels of their own.

The other ten stay **completely silent** - a test asserts their totals are byte-identical to no
aura at all. Vigor, Regeneration, Holy Freeze, Conviction, Sanctuary, Retribution, Purge, Shield
Aura, Cleanse and Swiftness need pulses, monster-facing queries or walk speed, and an aura that
half-works is worse than one that visibly waits its turn.

**Persistence, without a save break.** The active aura is one byte in a new chunk (tag 4) on the
hero file's OEXT tail - exactly what that tail was built for in Phase 0.1. Old builds skip the
tag; pre-1.7.12 heroes load with no aura. The golden hero hash moved for the sixth time, this
time deliberately, and the reason is now change #6 in that test's own list. The chunk reader
takes only the first byte, leaving room for Stage 2's 24 aura levels to append without a new tag.

The provider's condition re-checks the unlock, so an aura that outlives the rules it was lit
under (a level rollback, a hand-edited save) stops contributing rather than quietly persisting.

## Also: the real rune art

The user supplied `runes.png` in the MPQ folder - the lossless original of the sheet I had
rebuilt from the chat paste. Identical geometry, so the cut coordinates held; the icons were
re-cut from it and the archive repacked. The names stay legible at 28px.

**State: 401 tests, the usual two.** New pins: toggle refuses the wrong class and locked tiers,
activation replaces and the second click clears, each accumulator gets the right contribution,
later-stage auras leak nothing, and the active aura round-trips through the chunk tail.

## Next

Stage 2 (aura levels via Holy Tomes/shrines) or Phase 3 (the bestiary multiplier). Stage 1 is
independently shippable by design - the remaining stages are additive.
