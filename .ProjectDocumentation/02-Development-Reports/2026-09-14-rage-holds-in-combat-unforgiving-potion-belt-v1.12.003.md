# Rage holds in combat, every landed blow earns, Unforgiving built, belt takes only potions

2026-09-14 — v1.12.003

## Why

> "dont auto put stuff in belt unless they are potions. other items go in inventory by default.
> rage fill very slow and drains rapidly. we need to reverse that. make sure every hit that lands delivers rage. i am
> not sure it works. double check.
> the draining - it should only start draining after i stoped swinging at monster. ... once i stop swinging at
> monsters start a 5 seconds countdown timer ... if i dont initiate a new combat, i relax and naturally rage begins
> depleting, but slowly - 1 rare per 1 second. also - move unforgiving passive to lvl10 slot and develop it."

## Why Rage filled slowly — what the double-check found

v1.12.002 paid out a generator's Rage through the old mana "pay" test. That test was written to charge a price only
when a skill did something, and it lost Rage in four ways:

1. **Stun** settled only when the stun took. A target that shrugs off stagger gave no Rage for a landed blow.
2. **Clasp of Ruin** settled only if the target was still alive after the blow. A killing blow gave nothing.
3. **Double Swing and Frenzy** have no damage bonus. They settled only through their extra blow, which needs the
   first blow to deal damage. A first blow that dealt none gave nothing.
4. **One payment per swing.** Every generator earned once, however many of its blows struck: both of a Double Swing,
   or three monsters cut by Cleave.

On top of that, the drain was 4 points a second after 3 seconds with no gain. That was faster than one hit a second
at +6 could fill it.

## Rage now

- **Earned per landed blow.** For the Barbarian, a melee skill settles on any blow that struck a monster, and a
  generator grants its Rage for each one:
  - Double Swing, both blows landed: 12.
  - Cleave through three: 18.
  - Whirlwind's blows count too, but Whirlwind is a spender and earns nothing.
  - Backhand, a cast, earns when its cast strikes.
- **Combat holds the pool.** `DoAttack` calls `NoteRageCombat` on every swing at a monster, landed or missed. A
  swing at an empty tile does not count. A generator blow that lands also counts. Spending does not: a shout into
  an empty room lets the clock run.
- **Calm:** 5 seconds after the last combat, then 1 point a second (`RageCalmDelayTicks` 100,
  `RageDecayIntervalTicks` 20). Re-engaging stops the drain at once.
- Mana users are unchanged. They keep the old pay test.

## Unforgiving

- **Built.** Assigned, it stops the drain: once calm, Rage rises 2 a second, up to the pool.
- **Moved to the level-10 cell** (tier 1, column 1), swapping cells with the unbuilt Inspiring Presence, which is now
  at level 30.
- **How the move was made.** A passive's unlock level used to be its count in table order. It is now its grid cell,
  `tier * 3 + column`, which is the position `BuildClassTreePage` draws it at. So a passive moves by changing its
  cell, and the row keeps its table position, which is what its icon, investment index and slot bytes key off.
  - All 108 passive rows were checked first: table order and grid order agreed everywhere.
  - So no other passive's unlock level moved.

## Belt

- **`Item::isPotion()`** is new: life, mana, rejuvenation and arena potions.
- **Belt only for potions.** Floor pickup, store purchase and auto-pickup put an item in the belt only when it is a
  potion. Everything else goes to the inventory and never to the belt.
- **Stash closing with an item held:** potions go to the belt, anything else to the inventory, then the stash. The
  belt is the last resort before the "no room" fatal.
- **Unchanged:** explicit shift-click moves, from the inventory or the stash to the belt.

## Tests

`oracool_rage_test`:

- Rage holds through a minute of swinging, waits 5 seconds, drains 1 a second, and stops again on re-engaging.
- Per-blow earning: Double Swing ×2, Cleave ×3, nothing for 0 blows.
- Spending does not restart the calm clock.
- Unforgiving unlocks at 10, Inspiring Presence at 30, Rampage still at 36.

In `oracool_audit_test`, the passive-page census now expects 43 built Passive Skills rows instead of 42; the new one
is Unforgiving.

Debug: 769 of 769 tests pass. Release is built and copied to RTM.
