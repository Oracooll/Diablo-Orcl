---
date: 2026-08-15
version: 1.5.73
area: Paladin skills / ranged
---

# The Three That Throw Something

Steps 5–7, and the end of the plan. All seven Paladin skills now do what their description says.

## Where they hang

The melee three are asked *"this swing landed, do you add anything?"* from inside `DoAttack`. These
three are asked *"the button was pressed, do something"* from `CheckPlrSpell`, before any animation
exists — a different question at a different moment, hence `oracool/paladin_ranged.{h,cpp}` rather
than more of `paladin_melee`.

`CastRangedPaladinSkill` returns a bool, and a false falls through to the swing. That is not
defensive padding: it is how "requires a shield" and "cannot afford it" stay consistent with the rule
every other skill follows — the ability never does nothing.

## Weapon damage, for things that are not swings

The user chose weapon-based scaling, and the melee three got it for free because the hook already
receives the damage the swing dealt. These three have no swing, so `RollWeaponDamage` mirrors the
first lines of the engine's own melee roll: weapon min..max, the item's percentage bonus, its flat
bonus, the character's damage modifier.

It deliberately stops before the two things that follow in `PlrHitMonst` — the Warrior/Barbarian
double-damage roll and the weapon-type modifiers — because those describe *a swing connecting*, and
none of these is a swing.

| Skill | Damage | Shape |
|---|---|---|
| Fist of the Heavens | 150% centre, 75% around | target tile + its eight neighbours |
| Blessed Shield | 125% primary, 100% carried | the target, then up to 4 within 3 tiles of **it** |
| Blessed Hammer | 60% per tile crossed | a spiral out from the caster |

Blessed Hammer's is lowest because it crosses many tiles in one cast and every crossing is a hit.
Blessed Shield picks its carry targets by distance from the **first enemy**, not from the player,
which is what makes it read as a shield bouncing through a knot of monsters rather than a second area
blast centred on you.

## Reuse that is a pattern, not a workaround

`MissileID::ApocalypseBoom` is the engine's own one-tile blast: it plants itself where told, runs its
animation, and damages what is standing there exactly once. Apocalypse builds its *entire* effect by
scattering these over an area — so Fist of the Heavens dropping nine of them, and Blessed Shield
dropping one per enemy, is using the engine the way it already uses itself. No new missile type was
needed for either, and the user's art drops onto them later without touching the mechanics.

## The spiral, which did need new movement

Blessed Hammer is the only missile in the file that does not travel on a velocity vector, and the
plan flagged it last for exactly that reason.

The seam turned out to be `UpdateMissilePos`: it derives a missile's tile and screen offset from
`position.traveled`, a fixed-point pixel displacement from `position.start`. Nothing requires that
displacement to have come from a velocity. So `ProcessBlessedHammer` writes it directly each tick
from an angle and a radius, both growing with the tick count, and calls `UpdateMissilePos` — a spiral
expressed without touching the movement code every other missile depends on.

Three details are load-bearing:

- **The y term is halved.** The dungeon is drawn isometrically, so a circle traced on the ground is
  an ellipse of half the height on screen. An unhalved circle would look like a hammer orbiting a
  vertical hoop.
- **Damage fires only when the tile changes.** At 20 ticks a second, damaging whatever it overlaps
  every tick is not a hammer, it is a blender.
- **The bounds check is not optional.** This is the same hazard `SpawnLightning` was carrying earlier
  today: a missile off the map indexes `dPiece` out of bounds. A spiral is *likelier* to do it, since
  it walks outward with nothing to stop at.

The numbers: 60 ticks at 2.2px each reaches 132 screen pixels, and one tile step is 32px across — so
it winds out to roughly four tiles over three seconds, with a little over three full turns. Well
inside the 10-tile range cap, deliberately: the spiral has to be watchable, and a hammer that reached
the cap would be a blur.

## State

353/355, the standing baseline. `IsPaladinSkillImplemented` now returns true for everything — kept
rather than deleted, because it is the one place that states which skills have mechanics, and the
next skill added will start out without any.

Not tested in-game — the user runs the game. Worth watching for: the spiral's shape and speed (the
three tuning numbers are one edit each), whether Fist of the Heavens' nine blasts read as one impact
or as nine, and that Blessed Shield refuses without a shield rather than swinging silently.
