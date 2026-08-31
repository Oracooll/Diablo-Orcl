---
date: 2026-08-16
version: 1.7.5
area: Self-audit over the autonomous run (v1.6.25-1.7.4)
---

# The Audit of the Long Night

Ordered by the user ("continue with everything and audit everything when you are done"), run over
all sixteen versions shipped in the autonomous session, with the lifetime checklist applied to
every piece of new state and every new caller's assumptions re-checked.

## Three findings, all fixed and pinned (v1.7.5)

1. **The runeword rename was invisible.** `Item::getName()` returns the BASE name for
   NORMAL-quality items - and normal is the one quality runewords form on, so a completed "Steel"
   still displayed as "Short Sword" everywhere (only the description's gold "Runeword: Steel"
   line betrayed it). The word's name now wins in that branch. Found by asking the checklist's
   question: who CONSUMES _iIName, and does that consumer's gate know about the new writer?

2. **Stale time-to-kill clocks.** Monster slots recycle across levels; a monster that was hit but
   never killed left its telemetry clock running, and the NEXT monster in that slot logged an
   hours-long TTK on its kill. Anything over ten minutes now reads as a stale clock and reports 0.

3. **Out-of-bounds read at the level cap.** `GetLevelExperienceSpan` indexes `ExpLvlsTbl[_pLevel]`;
   the XP counter always guarded max level before calling, but the gain blinker - the function's
   new caller from this run - did not, reading one past the table at the cap. The bounds moved
   INTO the function (single authority), so no future caller can repeat it. This is the "a fix
   adding a caller must re-check callee assumptions" rule catching my own work.

## Verified clean (the checks that found nothing)

- **Seed replay discipline**: every new roll (sockets, ethereal, MF/GF, gems/charms/runes drops)
  lives in the unseeded drop tail; the pack golden tests stayed green through all sixteen
  versions, and MF explicitly reads player state only outside recreation.
- **Chunk tail**: torn-tail rejection, unknown-tag skip, legacy-file no-op all pinned; the
  preview path (hero list UI) deliberately skips chunks - display-only.
- **Version lockstep**: OracoolItemFormatVersion 3->4 and StashVersion 4->5 moved together twice.
- **Ethereal + the broken-item flow**: an ethereal item at 0 durability goes permanently broken
  (the SP keep-broken feature) rather than destroyed - unrepairable forever, which reads as the
  price paid. Deliberate; noted rather than changed.
- **Pink plates, RMB attack, Shield Bash guards**: re-read against their consumers; no gaps found.

## Known simplifications (documented, not defects)

- Charm "reading order" is InvList insertion order, not visual grid order - the active three can
  differ from the top-left three after rearranging. A grid-order sort is a small later polish.
- Gem insertion is backpack-hosted only (socketing worn gear means carrying it first) - stated in
  the code as a design choice.
- MF covers monster drops (SpawnItem), not chests/barrels - scope, not oversight.
- Runeword item names display white, not gold (the description carries the gold line) - polish
  when the name-colour plumbing next opens.

## Remaining in Phase 1 (next session, from memory)

Gambling at Wirt and the crafting window - the two store-UI units - then Phase 2.

**State: 384/386** - the usual two. Sixteen feature versions plus this audit, every one committed,
tested and pushed separately.
