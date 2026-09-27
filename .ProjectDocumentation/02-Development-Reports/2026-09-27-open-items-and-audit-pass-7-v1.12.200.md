# 2026-09-27 - The open items, and audit pass 7 (v1.12.200)

**Date:** 2026-09-27. Debug only. The user said: "fix the open items too, then run a few more audits".

The items left open by pass 6 were fixed. Four more read-only audits then ran:
- the HUD and on-screen widgets;
- level generation and spawns;
- whether tooltips match what the game applies;
- performance hot paths.

I verified each finding before fixing it.

## The open items

- **Spells and belt:**
  - `LoadHotkeys`' fallback restores only a binding the hero can use (`HeroHasBinding`), and the readied pair too. The left F-key path uses the same check.
  - The pad's potion button skips the hidden belt slots.
  - A potion stolen and downgraded is placed after the steal loop, so the loop cannot reach it twice.
- **Tests that could not fail:**
  - `NoMonsterIsTheGuardianOutsideTheRift` now names a real guardian (`SetRiftGuardianForTest`) and checks both inside and outside the rift.
  - `SetFadeLevel` records the level before its headless early return, and the fade test checks it and that the colour table is not painted.
- **New tests:**
  - `LoadSaveItemFormats.Formats13And14StillReadAndTheFormatComesBack`: records cut back to formats 14 and 13 in memory. `SaveHelper` can capture to a buffer and `LoadHelper` can read one.
  - `OracoolRfa12Actives.EveryActiveCastsTicksAndClearsOnAnEmptyFloor`: every RfA-12 active cast, ticked to its end and cleared.
  - `OracoolWorkshop.PricesDoubleFromTheItemsOwnCountersAndTheLockHolds`: the lock rule is now `WorkshopLockAllowsReroll`, which the bench uses.
  - `Player.ColdResistanceFromAnItemReachesTheHeroLikeFire`.
  - `OracoolAudit.TheDropTableKeepsItsShape`: the drop table enabled, with invariants.
- **The pack golden rows** now carry cold resistance and the whole affix count. 100 rows were extended with values captured from the current generation, and all 61 pack tests pass, so every other field is unchanged.

## Fixed from audit pass 7

### HIGH

- **Slows reached the sheet but never the feet.**
  - The walk took max(-2, skip) and the run ignored slows, so a chill's "Move speed 50%" changed nothing.
  - The walk now takes the skip as it is, and the run keeps its +4 frames over the slowed walk. The slowest stride is 20 ticks (50%) instead of 12.
- **Fire plus lightning from two sources (a Flame and a Spark shard) became one Hellfire spectral bolt,** rolled from the fire range, on a landed hit only. In single player both blows fire, as the sheet lists them.
- **Melee elemental splits (Searing, Voltaic, Glacial) and venom clamped the hero's resistance to 0-75.** They now use it as it stands, negatives amplifying, as missiles do.
- **The waypoint sigil's move cleared its old tile blindly,** which could erase a level-16 lever or a Na-Krul book. It clears the tile only if the tile still names the sigil.
- **A full synchronous save ran on every kill and every pickup.** Those two triggers now save at most once every 10 seconds. A level change, a purchase or leaving the game still saves at once.
- **The rim-and-glow backing was computed per pixel per frame.** Its colours are cached per footprint (position, size, hue, style), and the output is the same pixel for pixel.

### MEDIUM

- **Abilities window:** active skill ranks include the items' +spell levels, shown as "(+N from items)".
- **Refinement shards:** they no longer double tier stats (spell levels, the armour-pierce tier, light radius). Those round to the nearest.
- **Sheet:**
  - The sheet's block chance counts the skills' block (Hold Your Ground, Reed in the Wind, Staff Parry, Brace).
  - Armour pierce reads as the percent ignored (25, 50, 75...), not the raw tier.
  - Class melee skills (Bash, Double Swing, Frenzy, Berserk, thrusts) show the weapon's range × their bonus instead of a dash.
- **Event log:**
  - Repeated identical lines fold into one line with a count, so autosaves no longer flood it.
  - Its lines use the loop's own line height.
- **"Game Saved"** draws under the clock instead of on it.
- **Levels:**
  - Rift floors get their champions and boss; the counts had refused every set level.
  - The hero lands beside the sigil when a monster stands on it.
  - Area level 64 stays within the useful-item level field.
  - Monsters taken back by `PlaceGroup` free their lights.

### LOW

- **Item tooltip text:**
  - Resistances print their real value (no "+75% MAX").
  - Magic find says what it does ("% chance a plain weapon or armor found is Rare").
  - The Sparkling Shrine logs the experience actually gained.
- **Screen and maps:**
  - The orb clip line follows the docked panels on taller screens.
  - Search's item markers land on the mini-map.
  - The mini-map's pan resets on every level.

## Not changed (reported)

- **The event log runs under the mana orb and the durability icons.** The user asked on 2026-09-24 for the log to reach the bottom of the screen, so shortening it is their call.
- **Monster lights on a revisited level depend on a count that is rolled again.** A throwaway `InitMonsters` runs before `LoadLevel`, so lights and saved `lightId`s disagree. It needs the light pool rebuilt from the loaded monsters.
- **Performance, MEDIUM and LOW:**
  - The tooltip card is rebuilt every frame.
  - Asset lookups run per grid cell.
  - The lazy monster rescaling causes hitches.
  - Telemetry opens and closes its file per event.
  - Floating numbers are costly when many are alive.
- **Display and generation, LOW:**
  - The full automap leaves bands at 4:3 (upstream).
  - Bow attack frames are shown as melee frames.
  - Champion lights can fill most of the 32 light slots at Torment.

## Tests

- v1.12.200 Debug builds clean, and the full suite passes: 872 of 872, including the shuffled runs.
- Two tests pinned the old rules and were updated:
  - The hotkey fallback test now gives the hero the spell, and checks that an unknown one is not restored.
  - The movement test expects the deeper stride.
- Two build stops were my own slips: the ambiguous `Item` in tests again, and an unexported `setlvlnum`.
