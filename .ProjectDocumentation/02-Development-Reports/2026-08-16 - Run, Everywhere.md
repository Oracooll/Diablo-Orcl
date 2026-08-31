---
date: 2026-08-16
version: 1.7.11
area: Megaplan Phase 2.5 - the run toggle
---

# Run, Everywhere

The run toggle, closing the smallest Phase 2 item: press **R** (settable in keymapping, like
every bind) to switch between walking and running, anywhere - not just town.

No new speed mechanic was invented, which was the megaplan's own instruction: running is the
double-speed frame skip Run In Town has always applied in StartWalkAnimation, the one Furious
Charge's dash already borrowed. The toggle is its third consumer - a one-line addition to that
condition. Session state, default off, single-player only, and the event log says "Running" /
"Walking" on each flip so the current mode is always checkable.

Stamina was consciously skipped: the megaplan lists it as optional, and D1's danger economy is
about what walks toward you, not a second drain bar.

**State: 398 tests, the usual two.**

## Phase 2 remaining

The aura gameplay pass - the one large item left, with its implementation plan already in the
vault ("Paladin Auras - Gameplay Implementation Plan"). Next session's opener.
