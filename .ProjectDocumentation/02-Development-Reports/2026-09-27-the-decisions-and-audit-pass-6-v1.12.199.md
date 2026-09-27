# 2026-09-27 - The remaining decisions, and audit pass 6 (v1.12.199)

**Date:** 2026-09-27. Debug only. The user said: "fix the decisions for me too", then "then run a few more audits".

The ten design calls left open by pass 5 were decided and coded. Four more read-only audits then ran, and I verified each finding before fixing it:
- quests, shrines and objects;
- spells, the belt and hotkeys;
- options, assets and rendering;
- the test suite itself.

## The decisions

- **Wirt's gamble no longer makes gold.** Worn gear is priced at the slot's base × level × the value percent its base tier is expected to carry at the roll's item level (`ExpectedTierValuePercent`). That is 100% below item level 13 and about 2145% once Torment bases are in play. Rings and amulets carry no base tier and keep their price.
- **Craft recipes stay inside the Rare's limits.** The Rare keeps two of its rolls, so the two craft powers make four. Each power is held to the largest roll a row of its kind allows at or below the item's level (`LargestAffixRollAtOrBelow`), and it is left off where no such row exists yet.
- **Sell All sells the backpack's first page and the belt only.** Pages 2-10, where crafting stock is kept, are left alone. A single item from a page still sells from the list, and the tooltip says so.
- **Revisited floors show the right champion bodies.** After `LoadLevel`, `RestoreUniqueCorpsesAfterLoad` rebuilds each loaded unique's corpse entry. An entry no loaded unique claims belonged to a champion killed before, and which monster that was is not saved, so it is not drawn rather than drawn wrong.
- **Failed summons cost nothing.**
  - A Golem cast checks that the body can exist before dismissing the old golem, and falls back to the hero's own tile.
  - Revive checks that a minion record is free before taking the corpse.
- **The gamepad can work the fork's windows.** A on the Rift Monument's menu, the workshop, the Cube, the runeword book, the skill picker, the event log, the waypoints, the crafting menu or Advanced Stats is a left click at the cursor (`ClickUiAtCursor`). The right stick already moves the cursor.
- **Cube tabs:** the Cube's side tabs count as its window (`IsPointOverLevski`), so right clicks and hovers no longer reach the world behind them.
- **Press and release:**
  - The waypoint menu's Act buttons and rows act on the release inside what was pressed.
  - The runeword book's filters do the same.
- **The Rift Monument's menu takes the keyboard.** Up and Down choose a button, Enter presses it, and 1-3 press a button directly.
- **Not changed:** Alt+F4 with every backpack page AND all 100 stash pages full still loses a staged Cube or bench item. That needs 16,700 cells full at once. The practical cases (a full pack, or a full pack and closing the window) were fixed in v1.12.198.

## Fixed from audit pass 6

### CRASH

- **`DrawVerticalLine`'s bottom clip kept the overhang instead of the part that fits.** A line crossing the screen's last row wrote that many rows past the back buffer: the full map's player arrow, or a panel frame straddling the edge. Test: the renderer primitives test.
- **A machine with no audio device crashed on a completed set.** `PlaySetCompleteSound` skipped the `gbSndInited` check that every other player has.
- **An undecodable sound file (a truncated cue, an MP3 renamed .wav) dereferenced a null stream.** `SetChunk` and `SetChunkStream` now check for it.

### HIGH

- **Every New Game cut base stats back to the class row's vanilla maximum,** for example Vitality 100 for a Paladin, while every other path allows 255. Points spent past it were lost, and a Barbarian lost every Magic shrine. Test: `Player.UnPackPlayer_KeepsBaseStatsPastTheClassRowsMaximum`.
- **Poisoned Water never completed while the hero's minions were out.** It counted slots in use, not hostiles left.
- **Adria's respec left refunded skills on both buttons and the F-keys, still casting.** It now calls `RefreshInnateSpells`, and a left F-key readies only what the hero owns, by its kind.
- **An F-key bound over a Scroll cell became a Spell binding.** On the left button it cast the spell for mana, never reading the scroll.

### MEDIUM

- **Buttons:** the left button kept a spent scroll or staff binding, and silently refused every left click on an enemy. `EnsureValidReadiedSpell` now checks both pairs.
- **Eldritch Shrine:** it turned a potion stack into one potion. The count is kept now.
- **Waypoints:** a waypoint skipped the quest gates the stairs keep. Level 16 needs Lazarus dead in this game, and the Nest and Crypt need their entrances open (`IsWarpOpen`, which the Unlock All Town Entrances option still opens).
- **Options:** INI integers outside an option's range are clamped. A zoom of 0 divided by zero, and a Torment multiplier of 0 gave monsters no life.

### LOW

- `SpawnRewardItem` now moves off an occupied tile, as `SpawnQuestItem` does since v1.12.198.
- `DrawHalfTransparentRectTo` clips both edges.
- The variant option's comment now matches its flag.

## Not changed (reported)

- **Spells:**
  - `LoadHotkeys`' fallback restores old bindings unchecked; slots 8-11 are fed only there.
  - `UseBeltItem` scans hidden belt slots.
  - A stolen potion can be downgraded twice.
- **Test-suite audit:**
  - `NoMonsterIsTheGuardianOutsideTheRift` and `AFadeIsRecordedNotPaintedIntoTheColourTable` cannot fail as written.
  - Nothing loads a format-13 or format-14 item file.
  - `rfa12_actives.cpp` (3,783 lines) has no behavioural test.
  - The workshop's price and lock rules are untested.
  - The pack golden rows compare no cold resistance and no affixes past the second.
  - Cold resistance is not tested end to end.
  - The only drop-rate test is disabled.
- **Minor:**
  - Per-frame allocations in the tooltip card and in Advanced Stats.
  - Shrines and armour stands inside rifts scale by the set-level id.

## Tests

- v1.12.199 Debug builds clean, and the full suite passes: 867 of 867, including the shuffled runs.
- New tests:
  - `Player.UnPackPlayer_KeepsBaseStatsPastTheClassRowsMaximum`
  - the vertical-line clip case in the renderer test
  - the gamble price-by-tier checks in `OracoolAudit.WirtHasAShopAndAGambleTabAndTheGambleScalesWithLevel`
- One build stopped on two slips of mine: a `std::array` treated as a pointer, and a test variable named `top` that clashed with a global.
