# v1.11.053 - Blessed Shield bounces; each strike flashes

2026-09-11. The user: "i dont think blessed shield should have splash dmg. it should bound off of first target in direction to nearest monster and then bouce off to third monster. dmg should reduce with each target - 100% on first, 75% on second, 50% on third. On each hit we should receive a confirmation feedback in the form of small short animation. Effect - HolyBoltExplosion (holyexpl) - this one looks adequate, maybe scaled down a bit."

## The flight

`ProcessBlessedShieldThrow` was rewritten.

**The splash is gone.** Before, the shield dropped nine `ApocalypseBoom` blasts over a 3x3 area where it landed.

**The bounce.**
- The shield strikes its first target, then turns toward the nearest monster it has not struck yet (`NextBlessedShieldTarget`).
- A monster counts only if it is within **6 tiles** (`BlessedShieldBounceTiles`), in sight, hittable, and not the player's golem. This is `FindClosest`'s rule, minus the monsters already struck.
- After the third strike the throw ends. It also ends when no monster is left to turn to, at a wall, or when its range runs out on a throw that strikes nothing.

**Damage.** Each strike carries **100%, 75% and 50%** of the throw (`BlessedShieldStrikePercent` and `BlessedShieldHitDamage` in missiles.h), never less than 1. The throw itself is still 125% of weapon damage, magic.

**Its own collision.** The shield uses its own collision check instead of `MoveMissileAndCheckMissileCol`, because it must fly *through* monsters it has already struck:
- the first target stands right behind the shield when it turns;
- the second target may stand on the line to the third.

Tracking lives in the missile's spare fields: `var1` counts the strikes, and `var2`/`var3` hold the first two targets' ids + 1.

**Two details.**
- Each strike stops the shield just short of its target's tile, as the throw always did. It turns from there.
- The turn does not call `SetMissDir`. That would re-dress the missile from its table graphic and undo the item-tumble fallback, and the shield spins, so its facing never showed.

## The flash

`MissileID::BlessedShieldImpact` is new, appended after `WarcryRing`. `MissilesData`'s `static_assert` now counts it. It is drawn and lit only and deals no damage.
- **Art:** Holy Bolt's own burst, `holyexpl`, at **60%** of its size. `oracool::ScaleClxList` scales it once, and the result is cached. The copy owns its pixels, so it survives the missile graphics being freed and reloaded on a level change.
- **Position:** a sprite hangs from its tile by its bottom edge, so the smaller burst is lifted by half the height it lost. Its centre stays at chest height, where the full-size burst sits.
- **Timing and light:** eight frames at one a tick, lit and timed by the generic `ProcessMissileExplosion`, the same routine Holy Bolt's explosion light follows.
- **Sound:** every strike also plays the shield's impact sound. A wall still plays it too, without a flash.

## Text

- Skill description: "Hurls your shield at a monster. It bounces to the nearest monster, then to a third, striking each for less. A shield is mandatory."
- Tooltip: "Magic damage: 125%, then 75% and 50% of that as it bounces".

## Build notes

- `MissilesData` is now `DVL_API_FOR_TEST`, for the same reason `UniqueItems` is: the test reads it through the inline `GetMissileData`.
- The first version of the test compared the row's `mAddProc` with `&AddBlessedShieldImpact`. Across the DLL boundary the test only sees an import stub, so the two addresses never match. The test now checks the row's data instead: its graphic and its damage type.

## Tests

`OracoolAudit.BlessedShieldStrikesThreeTimesForLessEachTime` checks:
- three targets;
- 100, 75 and 50 from 100, 0 for a fourth, and at least 1 from a weak throw;
- the flash's row is Holy Bolt's burst, magic.

The flight itself needs a level full of monsters and is not unit-tested.

## Verification

Debug and Release built, ctest **711/711**, RTM refreshed with exe 1.11.053. **Not seen in play.**

**To check:**
1. Throw at a pack. The shield should strike one monster, turn to the nearest, then to a third, with a small holy flash on each.
2. Damage should fall off visibly on the second and third hits.
3. A lone monster is struck once and the throw ends.
4. The flash's size and height on the monster. 60% was my reading of "a bit". Tell me a percent if it's too small or too big.
