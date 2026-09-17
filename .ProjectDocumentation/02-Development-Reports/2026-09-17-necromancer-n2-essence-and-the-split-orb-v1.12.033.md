# The Necromancer, phase N2: Essence and the split orb (v1.12.033)

**Date:** 2026-09-17 - Debug only - 805 of 805 tests. N1 was confirmed in play at v1.12.032 ("it is all good").

## What was built

- **`oracool/essence.{h,cpp}`**, modelled on `oracool/rage` but a SECOND pool beside mana, not a replacement
  (D3). 100 points; empty to full within 20 seconds (D8) - 399 ticks, measured by test. Kept in 1/64 points like
  life and mana, because a quarter of a point a tick cannot be said in whole points. Refills fighting or not;
  the dead refill nothing; no potion touches it; never saved. A hero ENTERS THE GAME with an empty pool
  (reset where Rage is reset: a new game, not a new level), so the refill can be watched in the first
  twenty seconds in town.
- **One pay path.** `CanPaySkill`, `SettleSkill` and `SkillResourceLine` (oracool/rage) learned the third
  currency: a spell with an `EssenceCost` is paid in Essence and nothing else, and its tooltip says "Essence
  Cost: N". `EssenceCost` returns 0 for every spell today - the priced rows arrive with their spell ids in
  N5-N7 (proposed: Corpse Explosion 10, a curse 25, Revive 35) - so nobody's costs changed, which a test pins.
- **The split orb** (`hud_art` `DrawSplitOrb`): mana liquid on the left half, Essence on the right, each to its
  own level, meeting on the sphere's centre column; the cradle goes over both. The Essence liquid is the mana
  liquid tinted dark green at load, at 70% brightness so the mana half stays the livelier. Falls back to the
  single orb if the liquid layers are missing.
- **The numbers** on the orb are two lines for him: mana above the centre in the usual colours, Essence below
  in green (`DrawFlaskValuesInColor`).

## Not in this build

- The character sheet has no Essence row: its layout is one static table measured once for every class, and a
  row only one class has needs that changed first. Deferred to the closing phase unless asked for sooner.
- Nothing spends Essence, by design of this phase.

## For the user's look in play

Start a Necromancer: the right orb is split blue/green; the green half climbs from empty to full in twenty
seconds; spending mana lowers only the blue half; the two value lines do not collide (with "show mana values" on).
This is drawn over the world - a screenshot is the only verification.
