---
date: 2026-08-15
version: 1.6.20
area: Item colours / Zeal
---

# Yellow Rares, Green Sets, and a Zeal You Can See

Three user reports off one play session. Two fixed here; the third needs one more observation.

## 1. Rares went green - and the fix delivers the Belzebub colour language

The green ramp's donor (indices 144-151) turned out to have a THIRD consumer the audit missed:
`fonts\yellow.trn`, the file behind `UiFlags::ColorYellow` - which is exactly the colour rare-tier
item names use. Every rare's name rendered green ("that is absurd").

The fix is two halves that together give the user's ask - "yellow rare AND Set Green items as in
belzebub":

- **Heal**: every font TRN is remapped at load - any glyph entry landing in the donor run moves to
  the PAL16_YELLOW ramp at matching brightness. Rares are yellow again, and so is anything else
  that ever pointed a font at that run.
- **Harvest**: one font colour is deliberately NOT healed. `ColorOracoolGreen` (UiFlags bit 34, the
  widening's third user) loads the same `yellow.trn` and keeps its original indices - which now ARE
  the green ramp. Green text existed in that file all along; it just used to be yellow.

**Set items wear it.** `IsOracoolItemIdx` - the eight material tiers across thirteen slots, leather
through spectral - is the set range, and `getTextColor` gives it green ABOVE the tier colours,
exactly as D2/Belzebub rank it: a rare-tier Steel Helm is a Steel set piece first.

## 2. Zeal swings you can watch

> "i dont see the hero making rapid atacks with 8-10 frames each."

Right - the extra strikes were invisible damage ticks. The numbers happened; the swings did not.

Zeal is now a **chain of real attacks**: when a Zeal-armed swing's animation ends, the next one
starts immediately toward the next target with all but ~4 windup frames skipped, landing through
the same DoAttack hit-frame path as any other blow - real animation, real to-hit roll, real damage,
the player visibly turning to face each victim. Each landed swing pays its own mana; the chain ends
when the strikes are spent, the mana runs dry, or nothing is left in reach. The invisible-tick
machinery (`ProcessZealBurst`, the pending-burst timers) is deleted, not bypassed.

This also quietly fixed a double-dip: the old first "strike" applied burst damage on top of the
swing that triggered it. Every hit is now exactly one swing's damage.

## 3. The double outline - one question outstanding

The draw path audit found the outline machinery sound: one draw per monster per frame (the walking
two-tile case early-returns the mirror tile), the wall-outline queue clears every frame, and the
second-pass copy lands on identical pixels. Nothing in the code produces two OFFSET outlines - so
the report needs one more datum before the right fix is knowable. Asked the user: does it happen
only on moving monsters?

## State

**368/370** - the usual two.
